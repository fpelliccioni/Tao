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
* http_server_api.h
*
* HTTP Library: exposes the entry points to the http server transport apis.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#include "http_listener.h"

#include <memory>

namespace web { namespace http
{
namespace listener
{

class http_server;

/// <summary>
/// Singleton class used to register for http requests and send responses.
///
/// The lifetime is tied to http listener registration. When the first listener registers an instance is created
/// and when the last one unregisters the receiver stops and is destroyed. It can be started back up again if
/// listeners are again registered.
/// </summary>
class http_server_api
{
public:

    /// <summary>
    /// Returns whether or not any listeners are registered.
    /// </summary>
    _ASYNCRTIMP static bool has_listener();

    /// <summary>
    /// Registers a HTTP server API.
    /// </summary>
    _ASYNCRTIMP static void register_server_api(std::unique_ptr<http_server> server_api);

    /// <summary>
    /// Clears the http server API.
    /// </summary>
    _ASYNCRTIMP static void unregister_server_api();

    /// <summary>
    /// Registers a listener for HTTP requests and starts receiving.
    /// </summary>
    _ASYNCRTIMP static unsigned long register_listener(_In_ http_listener_interface *pListener);

    /// <summary>
    /// Unregisters the given listener and stops listening for HTTP requests.
    /// </summary>
    _ASYNCRTIMP static unsigned long unregister_listener(_In_ http_listener_interface *pListener);

    /// <summary>
    /// Gets static HTTP server API. Could be null if no registered listeners.
    /// </summary>
    _ASYNCRTIMP static http_server *server_api();

private:

    /// Used to lock access to the server api registration
    static pplx::critical_section s_lock;

    /// Registers a server API set -- this assumes the lock has already been taken
    static void unsafe_register_server_api(std::unique_ptr<http_server> server_api);

    // Static instance of the HTTP server API.
    static std::unique_ptr<http_server> s_server_api;

    /// Number of registered listeners;
    static int s_registrations;

    // Static only class. No creation.
    http_server_api();
};

} // namespace listener
}} // namespace web::http
