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
* building_request_tests.cpp
*
* Tests cases manually building up HTTP requests.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

#ifdef _MS_WINDOWS
#include <WinError.h>
#endif

using namespace web::http;
using namespace web::http::client;

using namespace tests::functional::http::utilities;

namespace tests { namespace functional { namespace http { namespace client {

SUITE(building_request_tests)
{

TEST_FIXTURE(uri_address, simple_values)
{
    test_http_server::scoped_server scoped(m_uri);
    test_http_server * p_server = scoped.server();
    http_client client(m_uri);

    // Set a method.
    const method method = methods::OPTIONS;
    http_request msg(method);
    VERIFY_ARE_EQUAL(method, msg.method());

    // Set a path once.
    const utility::string_t custom_path1 = U("/hey/custom/path");
    msg.set_request_uri(custom_path1);
    VERIFY_ARE_EQUAL(custom_path1, msg.relative_uri().to_string());
    p_server->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, method, custom_path1);
        p_request->reply(200);
    });
    http_asserts::assert_response_equals(client.request(msg).get(), status_codes::OK);

    // Set the path twice.
    msg = http_request(method);
    msg.set_request_uri(custom_path1);
    VERIFY_ARE_EQUAL(custom_path1, msg.relative_uri().to_string());
    const utility::string_t custom_path2 = U("/yes/you/there");
    msg.set_request_uri(custom_path2);
    VERIFY_ARE_EQUAL(custom_path2, msg.relative_uri().to_string());
    p_server->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, method, custom_path2);
        p_request->reply(200);
    });
    http_asserts::assert_response_equals(client.request(msg).get(), status_codes::OK);
}

TEST_FIXTURE(uri_address, body_types)
{
    test_http_server::scoped_server scoped(m_uri);
    test_http_server * p_server = scoped.server();
    http_client client(m_uri);

    // Body data types.
    const method method(U("CUSTOMmethod"));
    utility::string_t str_body(U("YES_BASIC_STRING BODY"));
    utility::string_t str_move_body(str_body);
    std::vector<unsigned char> vector_body;
    vector_body.resize(str_body.size()*sizeof(utility::char_t));
    memcpy(&vector_body[0], &str_body[0], str_body.size()*sizeof(utility::char_t));
    std::vector<unsigned char> vector_move_body(vector_body);
    utility::string_t custom_content = U("YESNOW!");

    // vector - no content type.
    http_request msg(method);
    msg.set_body(std::move(vector_move_body));
    VERIFY_ARE_EQUAL(U("application/octet-stream"), msg.headers()[U("Content-Type")]);
    p_server->next_request().then([&](test_request *p_request)
    {
        auto received = p_request->m_body;
        auto sent = vector_body;
        VERIFY_IS_TRUE(received == sent);
        p_request->reply(200);
    });
    http_asserts::assert_response_equals(client.request(msg).get(), status_codes::OK);

    // vector - with content type.
    msg = http_request(method);
    vector_move_body = vector_body;
    msg.headers().add(U("Content-Type"), custom_content);
    msg.set_body(std::move(vector_move_body));
    VERIFY_ARE_EQUAL(custom_content, msg.headers()[U("Content-Type")]);
    p_server->next_request().then([&](test_request *p_request)
    {
        auto received = p_request->m_body;
        auto sent = vector_body;
        VERIFY_IS_TRUE(received == sent);
        p_request->reply(200);
    });
    http_asserts::assert_response_equals(client.request(msg).get(), status_codes::OK);
    
    // string - no content type.
    msg = http_request(method);
    msg.set_body(std::move(str_move_body));
    VERIFY_ARE_EQUAL(U("text/plain; charset=utf-8"), msg.headers()[U("Content-Type")]);
    p_server->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, method, U("/"), U("text/plain; charset=utf-8"), str_body);
        p_request->reply(200);
    });
    http_asserts::assert_response_equals(client.request(msg).get(), status_codes::OK);

    // string - with content type.
    msg = http_request(method);
    str_move_body = str_body;
    msg.headers().add(U("Content-Type"), custom_content);
    msg.set_body(std::move(str_move_body));
    VERIFY_ARE_EQUAL(custom_content, msg.headers()[U("Content-Type")]);
    p_server->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, method, U("/"), custom_content, str_body);
        p_request->reply(200);
    });
    http_asserts::assert_response_equals(client.request(msg).get(), status_codes::OK);
}

TEST(set_body_string_with_charset)
{
	http_request request;
	VERIFY_THROWS(request.set_body(U("body_data"), U("text/plain;charset=utf-16")), std::invalid_argument);
}

TEST_FIXTURE(uri_address, empty_bodies)
{
    test_http_server::scoped_server scoped(m_uri);
    test_http_server * p_server = scoped.server();
    http_client client(m_uri);

    // Body data.
    std::string empty_str;
    std::vector<unsigned char> vector_body;
    utility::string_t str_body;
    utility::string_t wstr_body;

    // empty vector.
    const method method(methods::PUT);
    http_request msg(method);
    msg.set_body(std::move(vector_body));
    p_server->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, method, U("/"), U("application/octet-stream"));
        VERIFY_ARE_EQUAL(0u, p_request->m_body.size());
        p_request->reply(200);
    });
    http_asserts::assert_response_equals(client.request(msg).get(), status_codes::OK);

    // empty string.
    msg = http_request(method);
    msg.set_body(std::move(str_body));
    p_server->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, method, U("/"), U("text/plain; charset=utf-8"));
        VERIFY_ARE_EQUAL(0u, p_request->m_body.size());
        p_request->reply(200);
    });
    http_asserts::assert_response_equals(client.request(msg).get(), status_codes::OK);

    // empty wstring.
    msg = http_request(method);
    msg.set_body(std::move(wstr_body));
    p_server->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, method, U("/"), U("text/plain; charset=utf-8"));
        VERIFY_ARE_EQUAL(0u, p_request->m_body.size());
        p_request->reply(200);
    });
    http_asserts::assert_response_equals(client.request(msg).get(), status_codes::OK);
}

TEST_FIXTURE(uri_address, set_body)
{
    test_http_server::scoped_server scoped(m_uri);
    http_client client(m_uri);
    const method mtd = methods::POST;
    utility::string_t data(U("YOU KNOW~!!!!!"));
    utility::string_t content_type = U("text/plain; charset=utf-8");

    // without content type
    http_request msg(mtd);
    msg.set_body(data);
    VERIFY_ARE_EQUAL(content_type, msg.headers()[U("Content-Type")]);
    scoped.server()->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, mtd, U("/"), content_type, data);
        p_request->reply(200);
    });
    http_asserts::assert_response_equals(client.request(msg).get(), status_codes::OK);

    // with content type
    content_type = U("YESYES");
    const utility::string_t expected_content_type = U("YESYES; charset=utf-8");
    msg = http_request(mtd);
    msg.set_body(data, content_type);
    VERIFY_ARE_EQUAL(expected_content_type, msg.headers()[U("Content-Type")]);
    scoped.server()->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, mtd, U("/"), expected_content_type, data);
        p_request->reply(200);
    });
    http_asserts::assert_response_equals(client.request(msg).get(), status_codes::OK);
}

TEST_FIXTURE(uri_address, set_body_with_charset)
{
    test_http_server::scoped_server scoped(m_uri);
    http_client client(m_uri);

    http_request msg(methods::PUT);
    try
    {
        msg.set_body(U("datadatadata"), U("text/plain;charset=us-ascii"));
        VERIFY_IS_TRUE(false);
    }
    catch(std::invalid_argument &)
    {
        // expected
    }
}

} // SUITE(building_request_tests)

}}}}