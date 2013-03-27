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
*
* ==--==
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* http_server.h
*
* HTTP Library: interface to implement HTTP server to service http_listeners.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#include "http_listener.h"

namespace web { namespace http
{
namespace listener
{

/// <summary>
/// Interface http listeners interact with for receiving and responding to http requests.
/// </summary>
class http_server
{
public:

    /// <summary>
    /// Release any held resources.
    /// </summary>
    virtual ~http_server() { };

    /// <summary>
    /// Start listening for incoming requests.
    /// </summary>
    virtual unsigned long start() = 0;

    /// <summary>
    /// Registers an http listener.
    /// </summary>
    virtual unsigned long register_listener(_In_ http_listener_interface *pListener) = 0;

    /// <summary>
    /// Unregisters an http listener.
    /// </summary>
    virtual unsigned long unregister_listener(_In_ http_listener_interface *pListener) = 0;

    /// <summary>
    /// Stop processing and listening for incoming requests.
    /// </summary>
    virtual unsigned long stop() = 0;

    /// <summary>
    /// Asynchronously sends the specified http response.
    /// </summary>
    /// <param name="response">The http_response to send.</param>
    /// <returns>A operation which is completed once the response has been sent.</returns>
    virtual pplx::task<void> respond(http_response response) = 0;
};

} // namespace listener
}} // namespace web::http