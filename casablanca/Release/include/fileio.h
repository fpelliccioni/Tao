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
* fileio.h
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
#pragma once

#include "pplx.h"
#include "xxpublic.h"

#ifdef WIN32
#include <cstdint>
#include <safeint.h>
#endif

#include "basic_types.h"

#ifndef _CONCRT_H
#ifndef _LWRCASE_CNCRRNCY
#define _LWRCASE_CNCRRNCY
// Note to reader: we're using lower-case namespace names everywhere, but the 'Concurrency' namespace
// is capitalized for historical reasons. The alias let's us pretend that style issue doesn't exist.
namespace Concurrency { }
namespace concurrency = Concurrency;
#endif
#endif

namespace Concurrency { namespace streams
{

namespace details
{
    /// <summary>
    /// A record containing the essential private data members of a file stream,
    /// in particular the parts that need to be shared between the public header
    /// file and the implementation in the implementation file.
    /// </summary>
    struct _file_info
    {
        _ASYNCRTIMP _file_info(std::ios_base::openmode mode, size_t buffer_size) : 
            m_rdpos(0), 
            m_wrpos(0), 
            m_atend(false), 
            m_buffer_size(buffer_size),
            m_buffer(nullptr),
            m_bufoff(0), 
            m_bufsize(0),
            m_buffill(0),
            m_mode(mode)
        {
        }
            
        // Positional data

        size_t m_rdpos;
        size_t m_wrpos;
        bool   m_atend;

        // Input buffer

        size_t m_buffer_size;  // The intended size of the buffer to read into.
        SafeSize m_bufsize;    // Buffer allocated size, as actually allocated.

        char   *m_buffer;
        size_t m_bufoff;    // File position that the start of the buffer represents.
        size_t m_buffill;   // Amount of file data actually in the buffer

        std::ios_base::openmode m_mode;

        pplx::recursive_lock m_lock;
    };


	/// <summary>
    /// This interface provides the necessary callbacks for completion events.
    /// </summary>
    class _filestream_callback
    {
    public:
        virtual void on_opened(_In_ details::_file_info *) { }
        virtual void on_closed(bool) { }
        virtual void on_error(const std::exception_ptr &) { }
        virtual void on_completed(size_t) { }
    protected:
        virtual ~_filestream_callback() {}
    };

}
}}

extern "C" 
{
/// <summary>
/// Open a file and create a streambuf instance to represent it.
/// </summary>
/// <param name="callback">A pointer to the callback interface to invoke when the file has been opened.</param>
/// <param name="filename">The name of the file to open</param>
/// <param name="mode">A creation mode for the stream buffer</param>
/// <param name="prot">A file protection mode to use for the file stream (not supported on Linux)</param>
/// <returns>True if the opening operation could be initiated, false otherwise.</returns>
/// <remarks>
/// True does not signal that the file will eventually be successfully opened, just that the process was started.
/// </remarks>
#if !defined(__cplusplus_winrt)
_ASYNCRTIMP bool _open_fsb_str(_In_ concurrency::streams::details::_filestream_callback *callback, const utility::char_t *filename, std::ios_base::openmode mode, int prot);
#endif

/// <summary>
/// Create a streambuf instance to represent a WinRT file.
/// </summary>
/// <param name="callback">A pointer to the callback interface to invoke when the file has been opened.</param>
/// <param name="file">The file object</param>
/// <param name="mode">A creation mode for the stream buffer</param>
/// <returns>True if the opening operation could be initiated, false otherwise.</returns>
/// <remarks>
/// True does not signal that the file will eventually be successfully opened, just that the process was started.
/// This is only available for WinRT.
/// </remarks>
#if defined(__cplusplus_winrt)
_ASYNCRTIMP bool _open_fsb_stf_str(_In_ concurrency::streams::details::_filestream_callback *callback, ::Windows::Storage::StorageFile^ file, std::ios_base::openmode mode, int prot);
#endif

/// <summary>
/// Close a file stream buffer.
/// </summary>
/// <param name="info">The file info record of the file</param>
/// <param name="callback">A pointer to the callback interface to invoke when the file has been opened.</param>
/// <returns>True if the closing operation could be initiated, false otherwise.</returns>
/// <remarks>
/// True does not signal that the file will eventually be successfully closed, just that the process was started.
/// </remarks>
_ASYNCRTIMP bool _close_fsb_nolock(_In_ concurrency::streams::details::_file_info **info, _In_ concurrency::streams::details::_filestream_callback *callback);
_ASYNCRTIMP bool _close_fsb(_In_ concurrency::streams::details::_file_info **info, _In_ concurrency::streams::details::_filestream_callback *callback);


/// <summary>
/// Write data from a buffer into the file stream.
/// </summary>
/// <param name="info">The file info record of the file</param>
/// <param name="callback">A pointer to the callback interface to invoke when the write request is completed.</param>
/// <param name="ptr">A pointer to a buffer where the data should be placed</param>
/// <param name="count">The size (in characters) of the buffer</param>
/// <returns>0 if the read request is still outstanding, -1 if the request failed, otherwise the size of the data read into the buffer</returns>
_ASYNCRTIMP size_t _putn_fsb(_In_ concurrency::streams::details::_file_info *info, _In_ concurrency::streams::details::_filestream_callback *callback, const void *ptr, size_t count, size_t char_size);

/// <summary>
/// Write a single byte to the file stream.
/// </summary>
/// <param name="info">The file info record of the file</param>
/// <param name="callback">A pointer to the callback interface to invoke when the write request is completed.</param>
/// <param name="ptr">A pointer to a buffer where the data should be placed</param>
/// <returns>0 if the read request is still outstanding, -1 if the request failed, otherwise the size of the data read into the buffer</returns>
_ASYNCRTIMP size_t _putc_fsb(_In_ concurrency::streams::details::_file_info *info, _In_ concurrency::streams::details::_filestream_callback *callback, int ch, size_t char_size);

/// <summary>
/// Read data from a file stream into a buffer
/// </summary>
/// <param name="info">The file info record of the file</param>
/// <param name="callback">A pointer to the callback interface to invoke when the write request is completed.</param>
/// <param name="ptr">A pointer to a buffer where the data should be placed</param>
/// <param name="count">The size (in characters) of the buffer</param>
/// <returns>0 if the read request is still outstanding, -1 if the request failed, otherwise the size of the data read into the buffer</returns>
_ASYNCRTIMP size_t _getn_fsb(_In_ concurrency::streams::details::_file_info *info, _In_ concurrency::streams::details::_filestream_callback *callback, _Out_writes_ (count) void *ptr, _In_ size_t count, size_t char_size);

/// <summary>
/// Flush all buffered data to the underlying file.
/// </summary>
/// <param name="info">The file info record of the file</param>
/// <param name="callback">A pointer to the callback interface to invoke when the write request is completed.</param>
/// <returns>True if the request was initiated</returns>
_ASYNCRTIMP bool _sync_fsb(_In_ concurrency::streams::details::_file_info *info, _In_ concurrency::streams::details::_filestream_callback *callback);

/// <summary>
/// Adjust the internal buffers and pointers when the application seeks to a new read location in the stream.
/// </summary>
/// <param name="info">The file info record of the file</param>
/// <param name="pos">The new position (offset from the start) in the file stream</param>
/// <returns>True if the request was initiated</returns>
_ASYNCRTIMP size_t _seekrdpos_fsb(_In_ concurrency::streams::details::_file_info *info, size_t pos, size_t char_size);

/// <summary>
/// Adjust the internal buffers and pointers when the application seeks to a new read location in the stream.
/// </summary>
/// <param name="info">The file info record of the file</param>
/// <param name="pos">The new position (offset from the start) in the file stream</param>
/// <returns>True if the request was initiated</returns>
_ASYNCRTIMP size_t _seekrdtoend_fsb(_In_ concurrency::streams::details::_file_info *info, int64_t offset, size_t char_size);

/// <summary>
/// Adjust the internal buffers and pointers when the application seeks to a new write location in the stream.
/// </summary>
/// <param name="info">The file info record of the file</param>
/// <param name="pos">The new position (offset from the start) in the file stream</param>
/// <returns>True if the request was initiated</returns>
_ASYNCRTIMP size_t _seekwrpos_fsb(_In_ concurrency::streams::details::_file_info *info, size_t pos, size_t char_size);
}
