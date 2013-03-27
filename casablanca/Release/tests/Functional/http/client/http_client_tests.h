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
* http_client_tests.h
*
* Common declarations and helper functions for http_client test cases.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#include <http_client.h>

#include "unittestpp.h"
#include "http_test_utilities.h"

namespace tests { namespace functional { namespace http { namespace client {

class uri_address
{
public:
    uri_address() : m_uri(U("http://localhost:34568/")) {}
    web::http::uri m_uri;
};

// Helper function to send a simple request to a server to test
// the connection.
void test_connection(tests::functional::http::utilities::test_http_server *p_server, web::http::client::http_client *p_client, const utility::string_t &path);

// Helper function send a simple request to test the connection.
// Take in the path to request and what path should be received in the server.
void test_connection(tests::functional::http::utilities::test_http_server *p_server, web::http::client::http_client *p_client, const utility::string_t &request_path, const utility::string_t &expected_path);

// Helper function to verify http_exception is thrown with correct error code
// TFS 578058 - should have VERIFY_ARE_EQUAL(code, _exc.error_code()); below.
#define VERIFY_THROWS_HTTP_ERROR_CODE(expression, code)                                 \
    UNITTEST_MULTILINE_MACRO_BEGIN                                                      \
        try                                                                             \
        {                                                                               \
            expression;                                                                 \
            VERIFY_IS_TRUE(false, "Expected http_exception not thrown");                \
        }                                                                               \
        catch (const web::http::http_exception & _exc)	                        \
        {                                                                               \
            VERIFY_IS_TRUE(std::string(_exc.what()).size() > 0);                        \
        }                                                                               \
        catch(...)                                                                      \
        {                                                                               \
            VERIFY_IS_TRUE(false, "Exception other than http_exception thrown");        \
        }                                                                               \
    UNITTEST_MULTILINE_MACRO_END

}}}}