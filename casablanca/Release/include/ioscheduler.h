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
* ioscheduler.h
*
* Internal Header:
*
* Defines a Windows threadpool based I/O Scheduler
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#include "xxpublic.h"

// This header file is only for Windows builds
#ifdef _MS_WINDOWS

#include "windows.h"

namespace Concurrency { namespace streams { namespace details {

/***
* ==++==
*
* Scheduler details.
*
* =-=-=-
****/

class io_scheduler;

/// <summary>
/// Our extended OVERLAPPED record.
/// </summary>
/// <remarks>
/// The standard OVERLAPPED structure doesn't have any fields for application-specific
/// data, so we must extend it.
/// </remarks>
struct EXTENDED_OVERLAPPED : OVERLAPPED
{
    EXTENDED_OVERLAPPED(LPOVERLAPPED_COMPLETION_ROUTINE func) : data(nullptr), func(func)
    {
        memset(this, 0, sizeof(OVERLAPPED));
    }

    void *data;
    LPOVERLAPPED_COMPLETION_ROUTINE func;
    io_scheduler *m_scheduler;
};

/// <summary>
/// Scheduler of I/O completions as well as any asynchronous operations that
/// are created internally as opposed to operations created by the application.
/// </summary>
/// <remarks>This scheduler uses the Vista thread pool</remarks>
class io_scheduler
{
public:

    /// <summary>
    /// Destructor
    /// </summary>
    ~io_scheduler();

    /// <summary>
    /// Associate a handle use for I/O with the scheduler.
    /// </summary>
    void *Associate(HANDLE fHandle)
    {
        return (void *)CreateThreadpoolIo(fHandle, IoCompletionCallback, this, &m_environ);
    }

    /// <summary>
    /// Disassociate a handle from the scheduler.
    /// </summary>
    void Disassociate(HANDLE fHandle, void *ctxt)
    {
        UNREFERENCED_PARAMETER(fHandle);
        CloseThreadpoolIo((PTP_IO)ctxt);
    }

    /// <summary>
    /// Get the I/O completion key to use with the scheduler.
    /// </summary>
    DWORD get_key() const { return (((DWORD)this) & 0xFAFAFA00) + sizeof(EXTENDED_OVERLAPPED); }

    /// <summary>
    /// Get the I/O scheduler instance.
    /// </summary>
    _ASYNCRTIMP static std::shared_ptr<io_scheduler> __cdecl get_scheduler();

private:

    /// <summary>
    /// Constructor
    /// </summary>
    io_scheduler()
    {
        m_cleanupGroup = CreateThreadpoolCleanupGroup();

        if (m_cleanupGroup == nullptr)
        {
            throw std::bad_alloc();
        }

        InitializeThreadpoolEnvironment(&m_environ);
        SetThreadpoolCallbackCleanupGroup(&m_environ, m_cleanupGroup, nullptr);
    }

    /// <summary>
    /// Callback for all I/O completions.
    /// </summary>
    static void CALLBACK IoCompletionCallback(
        PTP_CALLBACK_INSTANCE instance,
        PVOID ctxt,
        PVOID pOverlapped,
        ULONG result,
        ULONG_PTR numberOfBytesTransferred,
        PTP_IO io)
    {
        UNREFERENCED_PARAMETER(io);
        UNREFERENCED_PARAMETER(ctxt);
        UNREFERENCED_PARAMETER(instance);

        if ( pOverlapped != nullptr )
        {
            EXTENDED_OVERLAPPED *pExtOverlapped = (EXTENDED_OVERLAPPED *)pOverlapped;

            pExtOverlapped->func(result, (DWORD)numberOfBytesTransferred, (LPOVERLAPPED)pOverlapped);

            delete pOverlapped;
        }

    }

    TP_CALLBACK_ENVIRON m_environ;
    PTP_CLEANUP_GROUP m_cleanupGroup;
};

}}} // namespaces;

#endif