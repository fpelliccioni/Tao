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
* request_helper_tests.cpp
*
* Tests cases for the convenience helper functions for making requests on http_client.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"
#include <fstream>

using namespace web; using namespace utility;
using namespace web::http;
using namespace web::http::client;

using namespace tests::functional::http::utilities;

namespace tests { namespace functional { namespace http { namespace client {

SUITE(request_helper_tests)
{

TEST_FIXTURE(uri_address, non_rvalue_bodies)
{
    test_http_server::scoped_server scoped(m_uri);
    test_http_server * p_server = scoped.server();
    http_client client(m_uri);
    
    // Without content type.
    utility::string_t send_body = U("YES NOW SEND THE TROOPS!");
    p_server->next_request().then([&send_body](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, methods::PUT, U("/"), U("text/plain; charset=utf-8"), send_body);
        p_request->reply(200);
    });
    http_asserts::assert_response_equals(client.request(methods::PUT, U(""), send_body).get(), status_codes::OK);
    
    // With content type.
    utility::string_t content_type = U("custom_content");
    test_server_utilities::verify_request(&client, methods::PUT, U("/"), content_type, send_body, p_server, status_codes::OK, U("OK"));

    // Empty body type
    send_body.clear();
    content_type = U("haha_type");
    p_server->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, methods::PUT, U("/"), content_type);
        VERIFY_ARE_EQUAL(0u, p_request->m_body.size());
        VERIFY_ARE_EQUAL(0u, p_request->reply(status_codes::OK, U("OK")));
    });
    http_asserts::assert_response_equals(client.request(methods::PUT, U("/"), send_body, content_type).get(), status_codes::OK, U("OK"));
}

TEST_FIXTURE(uri_address, rvalue_bodies)
{
    test_http_server::scoped_server scoped(m_uri);
    test_http_server * p_server = scoped.server();
    http_client client(m_uri);
    
    // Without content type.
    utility::string_t send_body = U("YES NOW SEND THE TROOPS!");
    utility::string_t move_body = send_body;
    p_server->next_request().then([&send_body](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, methods::PUT, U("/"), U("text/plain; charset=utf-8"), send_body);
        p_request->reply(200);
    });
    http_asserts::assert_response_equals(client.request(methods::PUT, U(""), std::move(move_body)).get(), status_codes::OK);

    // With content type.
    utility::string_t content_type = U("custom_content");
    move_body = send_body;
    p_server->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, methods::PUT, U("/"), content_type, send_body);
        p_request->reply(200);
    });
    http_asserts::assert_response_equals(client.request(methods::PUT, U(""), std::move(move_body), content_type).get(), status_codes::OK);

    // Empty body.
    content_type = U("haha_type");
    send_body.clear();
    move_body = send_body;
    p_server->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, methods::PUT, U("/"), content_type);
        VERIFY_ARE_EQUAL(0u, p_request->m_body.size());
        p_request->reply(200);
    });
    http_asserts::assert_response_equals(client.request(methods::PUT, U(""), std::move(move_body), content_type).get(), status_codes::OK);
}

TEST_FIXTURE(uri_address, json_bodies)
{
    test_http_server::scoped_server scoped(m_uri);
    test_http_server * p_server = scoped.server();
    http_client client(m_uri);
    
    // JSON bool value.
    json::value bool_value = json::value::boolean(true);
    p_server->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, methods::PUT, U("/"), U("application/json"), bool_value.to_string());
        p_request->reply(200);
    });
    http_asserts::assert_response_equals(client.request(methods::PUT, U("/"), bool_value).get(), status_codes::OK);

    // JSON null value.
    json::value null_value = json::value::null();
    p_server->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, methods::PUT, U("/"), U("application/json"), null_value.to_string());
        p_request->reply(200);
    });
    http_asserts::assert_response_equals(client.request(methods::PUT, U(""), null_value).get(), status_codes::OK);
}

TEST_FIXTURE(uri_address, non_rvalue_2k_body)
{
    test_http_server::scoped_server scoped(m_uri);
    test_http_server * p_server = scoped.server();
    http_client client(m_uri);

    std::string body;
    for(int i = 0; i < 2048; ++i)
    {
        body.append(1, (char)('A' + (i % 26)));
    }
    test_server_utilities::verify_request(&client, methods::PUT, U("/"), U("text/plain"), ::utility::conversions::to_string_t(body), p_server, status_codes::OK, U("OK"));
}

} // SUITE(request_helper_tests)

}}}}
