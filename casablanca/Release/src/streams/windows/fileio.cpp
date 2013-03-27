/***
* ==++==
*
* Copyright (c) Microsoft Corporation. All rights reserved. 
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* http://www.apache.org/licenses/LICENSE-2.0
* 
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* ==--==
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* fileio.cpp
*
* Asynchronous I/O: stream buffer implementation details
*
* We're going to some lengths to avoid exporting C++ class member functions and implementation details across
* module boundaries, and the factoring requires that we keep the implementation details away from the main header
* files. The supporting functions, which are in this file, use C-like signatures to avoid as many issues as
* possible.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#include "stdafx.h"
#include "ioscheduler.h"
#include "fileio.h"

using namespace web; 
using namespace utility;
using namespace concurrency;
using namespace utility::conversions;

namespace Concurrency { namespace streams { namespace details {

/***
* ==++==
*
* Implementation details of the file stream buffer
*
* =-=-=-
****/


/// <summary>
/// The public parts of the file information record contain only what is implementation-
/// independent. The actual allocated record is larger and has details that the implementation
/// require in order to function.
/// </summary>
struct _file_info_impl : _file_info
{
    _file_info_impl(std::shared_ptr<io_scheduler> sched, HANDLE handle, _In_ void *io_ctxt, std::ios_base::openmode mode, size_t buffer_size) :
        m_scheduler(sched), 
        m_io_context(io_ctxt),
        m_handle(handle),
        m_outstanding_writes(0),
        _file_info(mode, buffer_size)
    {
    }

    /// <summary>
    /// The Win32 file handle of the file
    /// </summary>
    HANDLE        m_handle;

    /// <summary>
    /// A Win32 I/O context, used by the thread pool to scheduler work.
    /// </summary>
    void         *m_io_context;

    volatile long m_outstanding_writes;

    /// <summary>
    /// A pointer to the scheduler instance used.
    /// </summary>
    /// <remarks>Even though we're using a singleton scheduler instance, it may not always be
    /// the case, so it seems a good idea to keep a pointer here.</remarks>
    std::shared_ptr<io_scheduler> m_scheduler;
};

}}}

using namespace streams::details;

/// <summary>
/// Translate from C++ STL file open modes to Win32 flags.
/// </summary>
/// <param name="mode">The C++ file open mode</param>
/// <param name="prot">The C++ file open protection</param>
/// <param name="dwDesiredAccess">A pointer to a DWORD that will hold the desired access flags</param>
/// <param name="dwCreationDisposition">A pointer to a DWORD that will hold the creation disposition</param>
/// <param name="dwShareMode">A pointer to a DWORD that will hold the share mode</param>
void _get_create_flags(std::ios_base::openmode mode, int prot, DWORD &dwDesiredAccess, DWORD &dwCreationDisposition, DWORD &dwShareMode)
{
    dwDesiredAccess = 0x0;
    if ( mode & std::ios_base::in ) dwDesiredAccess |= GENERIC_READ;
    if ( mode & std::ios_base::out ) dwDesiredAccess |= GENERIC_WRITE;

    if ( mode & std::ios_base::in )
    {
        if ( mode & std::ios_base::out )
            dwCreationDisposition = OPEN_ALWAYS;
        else
            dwCreationDisposition = OPEN_EXISTING;
    }
    else if ( mode & std::ios_base::trunc )
    {
        dwCreationDisposition = CREATE_ALWAYS;
    }
    else
    {
        dwCreationDisposition = OPEN_ALWAYS;   
    }

    // C++ specifies what permissions to deny, Windows which permissions to give,
    dwShareMode = 0x3;
    switch (prot)
    {
    case _SH_DENYRW: 
        dwShareMode = 0x0;
        break;
    case _SH_DENYWR: 
        dwShareMode = 0x1;
        break;
    case _SH_DENYRD: 
        dwShareMode = 0x2;
        break;
    }
}

/// <summary>
/// Perform post-CreateFile processing.
/// </summary>
/// <param name="fh">The Win32 file handle</param>
/// <param name="callback">The callback interface pointer</param>
/// <param name="mode">The C++ file open mode</param>
/// <returns>The error code if there was an error in file creation.</returns>
DWORD _finish_create(HANDLE fh, _In_ _filestream_callback *callback, std::ios_base::openmode mode, int prot)
{
    DWORD error = ERROR_SUCCESS;
    void *io_ctxt = nullptr;

    if ( fh != INVALID_HANDLE_VALUE ) 
    {
        std::shared_ptr<io_scheduler> sched = io_scheduler::get_scheduler();

        io_ctxt = sched->Associate(fh);
        SetFileCompletionNotificationModes(fh, FILE_SKIP_COMPLETION_PORT_ON_SUCCESS);

        // Buffer reads internally if and only if we're just reading (not also writing) and
        // if the file is opened exclusively. If either is false, we're better off just
        // letting the OS do its buffering, even if it means that prompt reads won't
        // happen.
        bool buffer = (mode == std::ios_base::in) && (prot == _SH_DENYRW);
        
        auto info = new _file_info_impl(sched, fh, io_ctxt, mode, buffer ? 512 : 0);

        if ( mode & std::ios_base::app || mode & std::ios_base::ate )
        {
            info->m_wrpos = (size_t)-1; // Start at the end of the file.
        }

        callback->on_opened(info);
    }
    else
    {
        callback->on_error(std::make_exception_ptr(utility::details::create_system_error(GetLastError())));
    }

    return error;
}

/// <summary>
/// Open a file and create a streambuf instance to represent it.
/// </summary>
/// <param name="callback">A pointer to the callback interface to invoke when the file has been opened.</param>
/// <param name="filename">The name of the file to open</param>
/// <param name="mode">A creation mode for the stream buffer</param>
/// <param name="prot">A file protection mode to use for the file stream</param>
/// <returns>True if the opening operation could be initiated, false otherwise.</returns>
/// <remarks>
/// True does not signal that the file will eventually be successfully opened, just that the process was started.
/// </remarks>
bool _open_fsb_str(_In_ _filestream_callback *callback, const utility::char_t *filename, std::ios_base::openmode mode, int prot)
{
    _PPLX_ASSERT(callback != nullptr);
    _PPLX_ASSERT(filename != nullptr);

    std::wstring name(filename);

    std::shared_ptr<io_scheduler> sched = io_scheduler::get_scheduler();

    pplx::create_task([=] ()
        {
            DWORD dwDesiredAccess, dwCreationDisposition, dwShareMode;
            _get_create_flags(mode, prot, dwDesiredAccess, dwCreationDisposition, dwShareMode);

            HANDLE fh = ::CreateFileW(name.c_str(), dwDesiredAccess, dwShareMode, nullptr, dwCreationDisposition, FILE_FLAG_OVERLAPPED, 0);

            _finish_create(fh, callback, mode, prot);
        });

    return true;
}

/// <summary>
/// Close a file stream buffer.
/// </summary>
/// <param name="info">The file info record of the file</param>
/// <param name="callback">A pointer to the callback interface to invoke when the file has been opened.</param>
/// <returns>True if the closing operation could be initiated, false otherwise.</returns>
/// <remarks>
/// True does not signal that the file will eventually be successfully closed, just that the process was started.
/// </remarks>
bool _close_fsb_nolock(_In_ _file_info **info, _In_ streams::details::_filestream_callback *callback)
{
    _PPLX_ASSERT(callback != nullptr);
    _PPLX_ASSERT(info != nullptr);
    _PPLX_ASSERT(*info != nullptr);
    
    _file_info_impl *fInfo = (_file_info_impl *)*info;

    if ( fInfo->m_handle == INVALID_HANDLE_VALUE ) return false;

    std::shared_ptr<io_scheduler> sched = io_scheduler::get_scheduler();

    // Since closing a file may involve waiting for outstanding writes which can take some time
    // if the file is on a network share, the close action is done in a separate task, as
    // CloseHandle doesn't have I/O completion events.
    pplx::create_task([=] ()
        {
            bool result = false;
            DWORD error = S_OK;

            {
                pplx::scoped_recursive_lock lck(fInfo->m_lock);

                if ( fInfo->m_handle != INVALID_HANDLE_VALUE )
                {
                    if ( fInfo->m_scheduler != nullptr )
                        fInfo->m_scheduler->Disassociate(fInfo->m_handle, fInfo->m_io_context);

                    result = CloseHandle(fInfo->m_handle) != FALSE;
                    if ( !result )
                        error = GetLastError();
                }

                if ( fInfo->m_buffer != nullptr )
                {
                    delete fInfo->m_buffer;       
                    fInfo->m_buffer;
                }
            }

            delete fInfo;

            if ( result )
                callback->on_closed(result);
            else
                callback->on_error(std::make_exception_ptr(utility::details::create_system_error(GetLastError())));
        });

    *info = nullptr;

    return true;
}

bool _close_fsb(_In_ _file_info **info, _In_ streams::details::_filestream_callback *callback)
{
    _PPLX_ASSERT(callback != nullptr);
    _PPLX_ASSERT(info != nullptr);
    _PPLX_ASSERT(*info != nullptr);
    
    return _close_fsb_nolock(info, callback);
}

/// <summary>
/// Keeps the data associated with a write request, passed from the place where the operation
/// is started to where it is completed.
/// </summary>
template<typename InfoType>
struct _WriteRequest
{
    _WriteRequest(_In_ InfoType *fInfo,
                  _In_ _filestream_callback *callback, 
                  std::shared_ptr<uint8_t> buffer,
                  DWORD nNumberOfBytesToWrite) : 
        fInfo(fInfo),
        lpBuffer(buffer),
        nNumberOfBytesToWrite(nNumberOfBytesToWrite),
        callback(callback) 
    {
    }

    InfoType *fInfo;
    std::shared_ptr<uint8_t> lpBuffer;
    DWORD nNumberOfBytesToWrite;
    streams::details::_filestream_callback *callback;
};

/// <summary>
/// Keeps the data associated with a read request, passed from the place where the operation
/// is started to where it is completed.
/// </summary>
template<typename InfoType>
struct _ReadRequest
{
    _ReadRequest(_In_ InfoType *fInfo,
                 _In_ _filestream_callback *callback, 
                 _Out_writes_ (nNumberOfBytesToWrite) LPVOID lpBuffer,
                 _In_ DWORD nNumberOfBytesToWrite) : 
        fInfo(fInfo),
        lpBuffer(lpBuffer),
        nNumberOfBytesToWrite(nNumberOfBytesToWrite),
        callback(callback) 
    {
    }

    InfoType *fInfo;
    LPVOID lpBuffer;
    DWORD nNumberOfBytesToWrite;
    streams::details::_filestream_callback *callback;
};

/// <summary>
/// The completion routine used when a write request finishes.
/// </summary>
/// <remarks>
/// The signature is the standard IO completion signature, documented on MSDN
/// </remarks>
template<typename InfoType>
VOID CALLBACK _WriteFileCompletionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
{
    EXTENDED_OVERLAPPED* pOverlapped = (EXTENDED_OVERLAPPED*)lpOverlapped;

    auto req = (_WriteRequest<InfoType> *)pOverlapped->data;

    if ( dwErrorCode != ERROR_SUCCESS && dwErrorCode != ERROR_HANDLE_EOF )
        req->callback->on_error(std::make_exception_ptr(utility::details::create_system_error(dwErrorCode)));
    else 
        req->callback->on_completed((size_t)dwNumberOfBytesTransfered);

    delete req;
}

/// <summary>
/// The completion routine used when a read request finishes.
/// </summary>
/// <remarks>
/// The signature is the standard IO completion signature, documented on MSDN
/// </remarks>
template<typename InfoType>
VOID CALLBACK _ReadFileCompletionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
{
    EXTENDED_OVERLAPPED* pOverlapped = (EXTENDED_OVERLAPPED*)lpOverlapped;

    auto req = (_ReadRequest<InfoType> *)pOverlapped->data;

    if ( dwErrorCode != ERROR_SUCCESS && dwErrorCode != ERROR_HANDLE_EOF )
        req->callback->on_error(std::make_exception_ptr(utility::details::create_system_error(dwErrorCode)));
    else 
        req->callback->on_completed((size_t)dwNumberOfBytesTransfered);
    
    delete req;
}

// TFS: # 398556
// The variable pOverlapped is complained about possible leak
// due to an exception (C6211). This happens several times throughout this file.
// Some work needs to be done here to guarantee we are leak free.
#pragma warning(push)
#pragma warning(disable : 6211)

/// <summary>
/// Initiate an asynchronous (overlapped) write to the file stream.
/// </summary>
/// <param name="info">The file info record of the file</param>
/// <param name="callback">A pointer to the callback interface to invoke when the write request is completed.</param>
/// <param name="ptr">A pointer to the data to write</param>
/// <param name="count">The size (in bytes) of the data</param>
/// <returns>0 if the write request is still outstanding, -1 if the request failed, otherwise the size of the data written</returns>
size_t _write_file_async(_In_ streams::details::_file_info_impl *fInfo, _In_ streams::details::_filestream_callback *callback, std::shared_ptr<uint8_t> ptr, size_t count, size_t position)
{
    auto scheduler = io_scheduler::get_scheduler();

    auto pOverlapped = new EXTENDED_OVERLAPPED(_WriteFileCompletionRoutine<streams::details::_file_info_impl>);
    pOverlapped->m_scheduler = scheduler.get();

    if ( position == (size_t)-1 )
    {
        pOverlapped->Offset = 0xFFFFFFFF;
        pOverlapped->OffsetHigh = 0xFFFFFFFF;
    }
    else
    {
        pOverlapped->Offset = (DWORD)position;
        pOverlapped->OffsetHigh = 0x0;
    }

    auto req = new _WriteRequest<streams::details::_file_info_impl>(fInfo, callback, ptr, (DWORD)count);
    pOverlapped->data = req;

    StartThreadpoolIo((PTP_IO)fInfo->m_io_context);

    DWORD written = 0;
    _InterlockedIncrement(&fInfo->m_outstanding_writes);

    BOOL wrResult = WriteFile(fInfo->m_handle, ptr.get(), (DWORD)count, nullptr, pOverlapped);
    DWORD error = GetLastError();

    // WriteFile will return false when a) the operation failed, or b) when the request is still
    // pending. The error code will tell us which is which.
    if ( wrResult == FALSE ) 
    {
        if (error == ERROR_IO_PENDING)
        {
            return 0;
        }

        CancelThreadpoolIo((PTP_IO)fInfo->m_io_context);
        written = (DWORD)-1;

    }
    else
    {
        // If WriteFile returned true, it must be because the operation completed immediately.
        // However, we didn't pass in a variable address for the number of bytes written, so
        // we have to retrieve it using 'GetOverlappedResult.'
        
        CancelThreadpoolIo((PTP_IO)fInfo->m_io_context);

        if (!GetOverlappedResult(fInfo->m_handle, pOverlapped, &written, FALSE) )
        {
            written = (DWORD)-1;
        }
    }

    delete req;
    delete pOverlapped;
    return (size_t)written;
}

/// <summary>
/// Initiate an asynchronous (overlapped) read from the file stream.
/// </summary>
/// <param name="info">The file info record of the file</param>
/// <param name="callback">A pointer to the callback interface to invoke when the write request is completed.</param>
/// <param name="ptr">A pointer to a buffer where the data should be placed</param>
/// <param name="count">The size (in bytes) of the buffer</param>
/// <param name="offset">The offset in the file to read from</param>
/// <returns>0 if the read request is still outstanding, -1 if the request failed, otherwise the size of the data read into the buffer</returns>
size_t _read_file_async(_In_ streams::details::_file_info_impl *fInfo, _In_ streams::details::_filestream_callback *callback, _Out_writes_ (count) void *ptr, _In_ size_t count, size_t offset)
{
    auto scheduler = io_scheduler::get_scheduler();

    auto pOverlapped = new EXTENDED_OVERLAPPED(_ReadFileCompletionRoutine<streams::details::_file_info_impl>);
    pOverlapped->m_scheduler = scheduler.get();
    pOverlapped->Offset = (DWORD)offset;
    pOverlapped->OffsetHigh = 0;

    auto req = new _ReadRequest<streams::details::_file_info_impl>(fInfo, callback, ptr, (DWORD)count);
    pOverlapped->data = req;

    StartThreadpoolIo((PTP_IO)fInfo->m_io_context);

    BOOL wrResult = ReadFile(fInfo->m_handle, ptr, (DWORD)count, nullptr, pOverlapped);
    DWORD error = GetLastError();

    // ReadFile will return false when a) the operation failed, or b) when the request is still
    // pending. The error code will tell us which is which.
    if ( wrResult == FALSE ) 
    {
        if ( error != ERROR_IO_PENDING ) 
        {
            CancelThreadpoolIo((PTP_IO)fInfo->m_io_context);
            delete req;
            delete pOverlapped;

            if ( error == ERROR_HANDLE_EOF )
            {
                callback->on_completed(0);
            }
            return (error == ERROR_HANDLE_EOF) ? 0 : (size_t)-1;
        }
    }
    else
    {
        // If ReadFile returned true, it must be because the operation completed immediately.
        // However, we didn't pass in a variable address for the number of bytes written, so
        // we have to retrieve it using 'GetOverlappedResult.'
        DWORD read = 0;

        CancelThreadpoolIo((PTP_IO)fInfo->m_io_context);

        if ( GetOverlappedResult(fInfo->m_handle, pOverlapped, &read, FALSE) )
        {
            delete req;
            delete pOverlapped;
            return (size_t)read;
        }
        else
        {
            delete req;
            delete pOverlapped;

            if ( error == ERROR_HANDLE_EOF )
            {
                callback->on_completed(0);
            }
            return (error == ERROR_HANDLE_EOF) ? 0 : (size_t)-1;
        }
    }

    return 0;
}

#pragma warning(pop)

template<typename Func>
class _filestream_callback_fill_buffer : public _filestream_callback
{
public:
    _filestream_callback_fill_buffer(_In_ _file_info *info, Func func) : m_func(func), m_info(info) { }

    virtual void on_completed(size_t result)
    {
        m_func(result);
        delete this;
    }

private:
    _file_info *m_info;
    Func        m_func;
};

template<typename Func>
_filestream_callback_fill_buffer<Func> *create_callback(_In_ _file_info *info, Func func)
{
    return new _filestream_callback_fill_buffer<Func>(info, func);
}

size_t _fill_buffer_fsb(_In_ _file_info_impl *fInfo, _In_ _filestream_callback *callback, size_t count, size_t char_size)
{
    msl::utilities::SafeInt<size_t> safeCount = count;

    if ( fInfo->m_buffer == nullptr )
    {
        fInfo->m_bufsize = safeCount.Max(fInfo->m_buffer_size);
        fInfo->m_buffer = new char[fInfo->m_bufsize*char_size];
        fInfo->m_bufoff = fInfo->m_rdpos;

        auto cb = create_callback(fInfo,
            [=] (size_t result) 
            { 
                pplx::scoped_recursive_lock lck(fInfo->m_lock);
                fInfo->m_buffill = result/char_size; 
                callback->on_completed(result); 
            });

        auto read =  _read_file_async(fInfo, cb, (uint8_t *)fInfo->m_buffer, fInfo->m_bufsize*char_size, fInfo->m_rdpos*char_size);

        switch (read)
        {
        case 0:
            // pending
            return read;

        case (-1):
            // error
            delete cb;
            return read;

        default:
            // operation is complete. The pattern of returning synchronously 
            // has the expectation that we duplicate the callback code here...
            // Do the expedient thing for now.
            cb->on_completed(read);

            // return pending
            return 0;
        };
    }

    // First, we need to understand how far into the buffer we have already read
    // and how much remains.

    size_t bufpos = fInfo->m_rdpos - fInfo->m_bufoff;
    size_t bufrem = fInfo->m_buffill - bufpos;

    if ( fInfo->m_rdpos < fInfo->m_bufoff )
    {
        // Reuse the existing buffer.

        fInfo->m_bufoff = fInfo->m_rdpos;

        auto cb = create_callback(fInfo,
            [=] (size_t result) 
            { 
                pplx::scoped_recursive_lock lck(fInfo->m_lock);
                fInfo->m_buffill = result/char_size; 
                callback->on_completed(bufrem*char_size+result); 
            });

        auto read = _read_file_async(fInfo, cb, (uint8_t*)fInfo->m_buffer, fInfo->m_bufsize*char_size, fInfo->m_rdpos*char_size);

        switch (read)
        {
        case 0:
            // pending
            return read;

        case (-1):
            // error
            delete cb;
            return read;

        default:
            // operation is complete. The pattern of returning synchronously 
            // has the expectation that we duplicate the callback code here...
            // Do the expedient thing for now.
            cb->on_completed(read);

            // return pending
            return 0;
        };
    }
    else if ( bufrem < count )
    {
        fInfo->m_bufsize = safeCount.Max(fInfo->m_buffer_size);

        // Then, we allocate a new buffer.

        char *newbuf = new char[fInfo->m_bufsize*char_size];

        // Then, we copy the unread part to the new buffer and delete the old buffer

        if ( bufrem > 0 )
            memcpy(newbuf, fInfo->m_buffer+bufpos*char_size, bufrem*char_size);

        delete fInfo->m_buffer;
        fInfo->m_buffer = newbuf;

        // Then, we read the remainder of the count into the new buffer
        fInfo->m_bufoff = fInfo->m_rdpos;

        auto cb = create_callback(fInfo,
            [=] (size_t result) 
            { 
                pplx::scoped_recursive_lock lck(fInfo->m_lock);
                fInfo->m_buffill = result/char_size; 
                callback->on_completed(bufrem*char_size+result); 
            });

        auto read = _read_file_async(fInfo, cb, (uint8_t*)fInfo->m_buffer+bufrem*char_size, (fInfo->m_bufsize-bufrem)*char_size, (fInfo->m_rdpos+bufrem)*char_size);

        switch (read)
        {
        case 0:
            // pending
            return read;

        case (-1):
            // error
            delete cb;
            return read;

        default:
            // operation is complete. The pattern of returning synchronously 
            // has the expectation that we duplicate the callback code here...
            // Do the expedient thing for now.
            cb->on_completed(read);

            // return pending
            return 0;
        };
    }
    else
    {
        // If we are here, it means that we didn't need to read, we already have enough data in the buffer
        return count*char_size;
    }
}


/// <summary>
/// Read data from a file stream into a buffer
/// </summary>
/// <param name="info">The file info record of the file</param>
/// <param name="callback">A pointer to the callback interface to invoke when the write request is completed.</param>
/// <param name="ptr">A pointer to a buffer where the data should be placed</param>
/// <param name="count">The size (in characters) of the buffer</param>
/// <returns>0 if the read request is still outstanding, -1 if the request failed, otherwise the size of the data read into the buffer</returns>
size_t _getn_fsb(_In_ streams::details::_file_info *info, _In_ streams::details::_filestream_callback *callback, _Out_writes_ (count) void *ptr, _In_ size_t count, size_t char_size)
{
    _PPLX_ASSERT(callback != nullptr);
    _PPLX_ASSERT(info != nullptr);
    
    _file_info_impl *fInfo = (_file_info_impl *)info;

    pplx::scoped_recursive_lock lck(info->m_lock);

    if ( fInfo->m_handle == INVALID_HANDLE_VALUE ) return (size_t)-1;

    if ( fInfo->m_buffer_size > 0 )
    {
        auto cb = create_callback(fInfo,
            [=] (size_t read) 
            { 
                auto sz = count*char_size;
                auto copy = (read < sz) ? read : sz;
                auto bufoff = fInfo->m_rdpos - fInfo->m_bufoff;
                memcpy((void *)ptr, fInfo->m_buffer+bufoff*char_size, copy);
                fInfo->m_atend = copy < sz;
                callback->on_completed(copy);
            });

        int read = (int)_fill_buffer_fsb(fInfo, cb, count, char_size);

        if ( read > 0 )
        {
            auto sz = count*char_size;
            auto copy = ((size_t)read < sz) ? (size_t)read : sz;
            auto bufoff = fInfo->m_rdpos - fInfo->m_bufoff;
            memcpy((void *)ptr, fInfo->m_buffer+bufoff*char_size, copy);
            fInfo->m_atend = copy < sz;
            return copy;
        }

        return (size_t)read;
    }
    else
    {
        return _read_file_async(fInfo, callback, ptr, count*char_size, fInfo->m_rdpos*char_size);
    }
}

/// <summary>
/// Write data from a buffer into the file stream.
/// </summary>
/// <param name="info">The file info record of the file</param>
/// <param name="callback">A pointer to the callback interface to invoke when the write request is completed.</param>
/// <param name="ptr">A pointer to a buffer where the data should be placed</param>
/// <param name="count">The size (in characters) of the buffer</param>
/// <returns>0 if the read request is still outstanding, -1 if the request failed, otherwise the size of the data read into the buffer</returns>
size_t _putn_fsb(_In_ streams::details::_file_info *info, _In_ streams::details::_filestream_callback *callback, const void *ptr, size_t count, size_t char_size)
{
    _PPLX_ASSERT(info != nullptr);
    
    _file_info_impl *fInfo = (_file_info_impl *)info;

    pplx::scoped_recursive_lock lck(fInfo->m_lock);

    if ( fInfo->m_handle == INVALID_HANDLE_VALUE ) return (size_t)-1;

    std::shared_ptr<uint8_t> buf(new uint8_t[msl::utilities::SafeInt<size_t>(count*char_size)]);
    memcpy(buf.get(), ptr, count*char_size);
    
    // To preserve the async write order, we have to move the write head before read.
    auto lastPos = fInfo->m_wrpos;
    if (fInfo->m_wrpos != static_cast<size_t>(-1))
    {
        fInfo->m_wrpos += count;
        lastPos *= char_size;
    }
    return _write_file_async(fInfo, callback, buf, count*char_size, lastPos);
}

/// <summary>
/// Write a single byte to the file stream.
/// </summary>
/// <param name="info">The file info record of the file</param>
/// <param name="callback">A pointer to the callback interface to invoke when the write request is completed.</param>
/// <param name="ptr">A pointer to a buffer where the data should be placed</param>
/// <returns>0 if the read request is still outstanding, -1 if the request failed, otherwise the size of the data read into the buffer</returns>
size_t _putc_fsb(_In_ streams::details::_file_info *info, _In_ streams::details::_filestream_callback *callback, int ch, size_t char_size)
{
    return _putn_fsb(info, callback, &ch, 1, char_size);
}

/// <summary>
/// Flush all buffered data to the underlying file.
/// </summary>
/// <param name="info">The file info record of the file</param>
/// <param name="callback">A pointer to the callback interface to invoke when the write request is completed.</param>
/// <returns>True if the request was initiated</returns>
bool _sync_fsb(_In_ streams::details::_file_info *, _In_ streams::details::_filestream_callback *callback)
{
    _PPLX_ASSERT(callback != nullptr);
    
    // Writes are not cached
    callback->on_completed(0);

    return true;
}

/// <summary>
/// Adjust the internal buffers and pointers when the application seeks to a new read location in the stream.
/// </summary>
/// <param name="info">The file info record of the file</param>
/// <param name="pos">The new position (offset from the start) in the file stream</param>
/// <returns>New file position or -1 if error</returns>
size_t _seekrdpos_fsb(_In_ streams::details::_file_info *info, size_t pos, size_t)
{
    _PPLX_ASSERT(info != nullptr);
    
    _file_info_impl *fInfo = (_file_info_impl *)info;

    pplx::scoped_recursive_lock lck(info->m_lock);

    if ( fInfo->m_handle == INVALID_HANDLE_VALUE ) return (size_t)-1;

    if ( pos < fInfo->m_bufoff || pos > (fInfo->m_bufoff+fInfo->m_buffill) )
    {
        delete fInfo->m_buffer;
        fInfo->m_buffer = nullptr;
        fInfo->m_bufoff = fInfo->m_buffill = fInfo->m_bufsize = 0;
    }

    fInfo->m_rdpos = pos;
    return fInfo->m_rdpos;
}

/// <summary>
/// Adjust the internal buffers and pointers when the application seeks to a new read location in the stream.
/// </summary>
/// <param name="info">The file info record of the file</param>
/// <param name="offset">The new position (offset from the end of the stream) in the file stream</param>
/// <param name="char_size">The size of the character type used for this stream</param>
/// <returns>New file position or -1 if error</returns>
_ASYNCRTIMP size_t _seekrdtoend_fsb(_In_ streams::details::_file_info *info, int64_t offset, size_t char_size)
{
    _PPLX_ASSERT(info != nullptr);
    
    _file_info_impl *fInfo = (_file_info_impl *)info;

    pplx::scoped_recursive_lock lck(info->m_lock);

    if ( fInfo->m_handle == INVALID_HANDLE_VALUE ) return (size_t)-1;

    if ( fInfo->m_buffer != nullptr )
    {
        // Clear the internal buffer.
        delete fInfo->m_buffer;
        fInfo->m_buffer = nullptr;
        fInfo->m_bufoff = fInfo->m_buffill = fInfo->m_bufsize = 0;
    }

    auto newpos = SetFilePointer(fInfo->m_handle, (LONG)(offset*char_size), nullptr, FILE_END);

    if ( newpos == INVALID_SET_FILE_POINTER ) return (size_t)-1;

    fInfo->m_rdpos = (size_t)newpos/char_size;
    return fInfo->m_rdpos/char_size;
}


/// <summary>
/// Adjust the internal buffers and pointers when the application seeks to a new write location in the stream.
/// </summary>
/// <param name="info">The file info record of the file</param>
/// <param name="pos">The new position (offset from the start) in the file stream</param>
/// <returns>New file position or -1 if error</returns>
size_t _seekwrpos_fsb(_In_ streams::details::_file_info *info, size_t pos, size_t)
{
    _PPLX_ASSERT(info != nullptr);
    
    _file_info_impl *fInfo = (_file_info_impl *)info;

    pplx::scoped_recursive_lock lck(info->m_lock);

    if ( fInfo->m_handle == INVALID_HANDLE_VALUE ) return (size_t)-1;

    fInfo->m_wrpos = pos;
    return fInfo->m_wrpos;
}