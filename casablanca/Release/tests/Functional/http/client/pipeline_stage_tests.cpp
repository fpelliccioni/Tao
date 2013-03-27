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
* pipeline_stage_tests.cpp
*
* Tests cases using pipeline stages on an http_client.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace web::http;
using namespace web::http::client;

using namespace tests::functional::http::utilities;

namespace tests { namespace functional { namespace http { namespace client {

SUITE(pipeline_stage_tests)
{

TEST_FIXTURE(uri_address, http_counting_methods)
{
    test_http_server::scoped_server scoped(m_uri);
    test_http_server * p_server = scoped.server();

    size_t count = 0;

    auto response_counter = 
        [&count](pplx::task<http_response> r_task) -> pplx::task<http_response>
        {
            ++count;
            return r_task; 
        };
    auto request_counter = 
        [&count,response_counter](http_request request, std::shared_ptr<http_pipeline_stage> next_stage) -> pplx::task<http_response>
        {
            ++count;
            return next_stage->propagate(request).then(response_counter);
        };

    http_client client(m_uri);
    client.add_handler(request_counter);

    // Don't include 'CONNECT' it has a special meaning.
    utility::string_t send_methods[] = 
    {
        methods::GET,
        U("GET"),
        methods::DEL,
        methods::HEAD,
#ifdef _MS_WINDOWS // this is never passed to the listener
        methods::OPTIONS,
#endif
        methods::POST,
        methods::PUT,
        methods::PATCH,
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

    VERIFY_ARE_EQUAL(num_methods*2, count);
}

TEST_FIXTURE(uri_address, http_short_circuit)
{
    size_t count = 0;

    auto request_counter = 
        [&count](http_request request, std::shared_ptr<http_pipeline_stage> next_stage) -> pplx::task<http_response>
        {
            ++count;
            request.reply(status_codes::Forbidden);
            return request.get_response();
        };

    http_client client(m_uri);
    client.add_handler(request_counter);

    // Don't include 'CONNECT' it has a special meaning.
    utility::string_t send_methods[] = 
    {
        methods::GET,
        U("GET"),
        methods::DEL,
        methods::HEAD,
        methods::OPTIONS,
        methods::POST,
        methods::PUT,
        methods::PATCH,
        U("CUstomMETHOD")
    };
    const size_t num_methods = sizeof(send_methods) / sizeof(send_methods[0]);

    for(int i = 0; i < num_methods; ++i)
    {
        http_asserts::assert_response_equals(client.request(send_methods[i]).get(), status_codes::Forbidden);
    }

    VERIFY_ARE_EQUAL(num_methods, count);
}

TEST_FIXTURE(uri_address, http_short_circuit_multiple)
{
    size_t count = 0;

    auto reply_stage = 
        [](http_request request, std::shared_ptr<http_pipeline_stage> next_stage) -> pplx::task<http_response>
        {
            request.reply(status_codes::Forbidden);
            return request.get_response();
        };

    auto count_stage = 
        [&count](http_request request, std::shared_ptr<http_pipeline_stage> next_stage) -> pplx::task<http_response>
        {
            count++;
            return next_stage->propagate(request);
        };

    http_client client(m_uri);
    client.add_handler(count_stage);
    client.add_handler(count_stage);
    client.add_handler(reply_stage);

    // Don't include 'CONNECT' it has a special meaning.
    utility::string_t send_methods[] = 
    {
        methods::GET,
        U("GET"),
        methods::DEL,
        methods::HEAD,
        methods::OPTIONS,
        methods::POST,
        methods::PUT,
        methods::PATCH,
        U("CUstomMETHOD")
    };
    const size_t num_methods = sizeof(send_methods) / sizeof(send_methods[0]);

    for(int i = 0; i < num_methods; ++i)
    {
        http_asserts::assert_response_equals(client.request(send_methods[i]).get(), status_codes::Forbidden);
    }

    VERIFY_ARE_EQUAL(num_methods*2, count);
}

TEST_FIXTURE(uri_address, http_short_circuit_no_count)
{
    size_t count = 0;

    auto reply_stage = 
        [](http_request request, std::shared_ptr<http_pipeline_stage> next_stage) -> pplx::task<http_response>
        {
            request.reply(status_codes::Forbidden);
            return request.get_response();
        };

    auto count_stage = 
        [&count](http_request request, std::shared_ptr<http_pipeline_stage> next_stage) -> pplx::task<http_response>
        {
            count++;
            return next_stage->propagate(request);
        };

    // The counting is prevented from happening, because the short-circuit come before the count.
    http_client client(m_uri);
    client.add_handler(reply_stage);
    client.add_handler(count_stage);

    // Don't include 'CONNECT' it has a special meaning.
    utility::string_t send_methods[] = 
    {
        methods::GET,
        U("GET"),
        methods::DEL,
        methods::HEAD,
        methods::OPTIONS,
        methods::POST,
        methods::PUT,
        methods::PATCH,
        U("CUstomMETHOD")
    };
    const size_t num_methods = sizeof(send_methods) / sizeof(send_methods[0]);

    for(int i = 0; i < num_methods; ++i)
    {
        http_asserts::assert_response_equals(client.request(send_methods[i]).get(), status_codes::Forbidden);
    }

    VERIFY_ARE_EQUAL(0u, count);
}

} // SUITE(pipeline_stage_tests)

}}}}