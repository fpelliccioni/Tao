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
* http_methods_tests.cpp
*
* Tests cases for HTTP methods.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace web::http;
using namespace web::http::client;

using namespace tests::functional::http::utilities;

namespace tests { namespace functional { namespace http { namespace client {

SUITE(http_methods_tests)
{

// Tests the defined methods and custom methods.
TEST_FIXTURE(uri_address, http_methods)
{
    test_http_server::scoped_server scoped(m_uri);
    test_http_server * p_server = scoped.server();
    http_client client(m_uri);

    // Don't include 'CONNECT' it has a special meaning.
    utility::string_t send_methods[] = 
    {
        methods::GET,
        U("GET"),
        methods::DEL,
        methods::HEAD,
#ifdef _MS_WINDOWS // -  this is never passed to the listener with http_listener
        methods::OPTIONS,
#endif
        methods::POST,
        methods::PUT,
        methods::PATCH,
#ifndef __cplusplus_winrt
# ifdef _MS_WINDOWS // - ditto
        methods::TRCE,
#endif
#endif

        U("CUstomMETHOD")
    };
    utility::string_t recv_methods[] = 
    {
        U("GET"),
        U("GET"),
        U("DELETE"),
        U("HEAD"),
#ifdef _MS_WINDOWS
        U("OPTIONS"),
#endif
        U("POST"),
        U("PUT"),
        U("PATCH"),
#ifndef __cplusplus_winrt
# ifdef _MS_WINDOWS
        U("TRACE"),
#endif
#endif

        U("CUstomMETHOD")
    };
    const size_t num_methods = sizeof(send_methods) / sizeof(send_methods[0]);

    for(int i = 0; i < num_methods; ++i)
    {
        p_server->next_request().then([i, &recv_methods](test_request *p_request)
        {
            http_asserts::assert_test_request_equals(p_request, recv_methods[i], U("/"));
            VERIFY_ARE_EQUAL(0u, p_request->reply(200));
        });
        http_asserts::assert_response_equals(client.request(send_methods[i]).get(), status_codes::OK);
    }
}

#ifdef __cplusplus_winrt
TEST_FIXTURE(uri_address, http_trace_fails_on_winrt)
{
    http_client client(m_uri);
    VERIFY_THROWS(client.request(methods::TRCE).get(), http_exception);
}
#endif

TEST(http_request_empty_method)
{
	VERIFY_THROWS(http_request(U("")), std::invalid_argument);
}

TEST_FIXTURE(uri_address, empty_method)
{
    test_http_server::scoped_server scoped(m_uri);
    http_client client(m_uri);

    try
    {
        client.request(U(""));
        VERIFY_IS_TRUE(false);
    } catch(std::invalid_argument &)
    {
        // Expected.
    }
}

} // SUITE(http_methods_tests)

}}}}