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
* http_asserts.h - Utility class to help verify assertions about http requests and responses.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#include "test_http_server.h"
#include "test_http_client.h"

#include "http_test_utilities_public.h"

namespace tests { namespace functional { namespace http { namespace utilities {

/// <summary>
/// Helper function to do percent encoding of just the '#' character, when running under WinRT.
/// The WinRT http client implementation performs percent encoding on the '#'.
/// </summary>
TEST_UTILITY_API utility::string_t percent_encode_pound(utility::string_t str);

/// <summary>
/// Static class containing various http request and response asserts.
/// </summary>
class http_asserts
{
public:

    /// <summary>
    /// Asserts that the specified request is equal to given arguments.
    /// </summary>
    TEST_UTILITY_API static void assert_request_equals(
        web::http::http_request request, 
        const web::http::method &mtd,
        const utility::string_t & relative_uri);

    TEST_UTILITY_API static void assert_request_equals(
        web::http::http_request request, 
        const web::http::method &mtd,
        const utility::string_t &relative_uri,
        const std::map<utility::string_t, utility::string_t> &headers);

    TEST_UTILITY_API static void assert_request_equals(
        web::http::http_request request, 
        const web::http::method &mtd,
        const utility::string_t & relative_uri,
        const utility::string_t & body);

    /// <summary>
    /// Asserts that the specified response is equal to given arguments.
    /// </summary>
    TEST_UTILITY_API static void assert_response_equals(
        web::http::http_response response,
        const web::http::status_code &code);

    TEST_UTILITY_API static void assert_response_equals(
        web::http::http_response response,
        const web::http::status_code &code,
        const utility::string_t &reason);

    TEST_UTILITY_API static void assert_response_equals(
        web::http::http_response response,
        const web::http::status_code &code,
        const std::map<utility::string_t, utility::string_t> &headers);

    /// <summary>
    /// Asserts the given http_headers contains the given values.
    /// </summary>
    TEST_UTILITY_API static void assert_http_headers_equals(
        const web::http::http_headers &actual,
        const web::http::http_headers &expected);

    /// <summary>
    /// Asserts the specified test_request is equal to its arguments.
    /// </summary>
    TEST_UTILITY_API static void assert_test_request_equals(
        const test_request *const p_request,
        const web::http::method &mtd,
        const utility::string_t &path);

    /// <summary>
    /// Asserts the specified test_request is equal to its arguments.
    /// </summary>
    TEST_UTILITY_API static void assert_test_request_equals(
        const test_request *const p_request,
        const web::http::method &mtd,
        const utility::string_t &path,
        const utility::string_t &content_type);

    /// <summary>
    /// Asserts the specified test_request is equal to its arguments.
    /// </summary>
    TEST_UTILITY_API static void assert_test_request_contains_headers(
        const test_request *const p_request,
        const web::http::http_headers &headers);

    /// <summary>
    /// Asserts the specified test_request is equal to its arguments.
    /// </summary>
    TEST_UTILITY_API static void assert_test_request_contains_headers(
        const test_request *const p_request,
        const std::map<utility::string_t, utility::string_t> &headers);

    /// <summary>
    /// Asserts the given HTTP request string is equal to its arguments.
    /// NOTE: this function only makes sure the specified headers exist, not that they are the only ones.
    /// </summary>
    TEST_UTILITY_API static void assert_request_string_equals(
        const utility::string_t &request,
        const web::http::method &mtd,
        const utility::string_t &path,
        const utility::string_t &version,
        const std::map<utility::string_t, utility::string_t> &headers,
        const utility::string_t &body);

    /// <summary>
    /// Asserts the given HTTP response string is equal to its arguments.
    /// NOTE: this function only makes sure the specified headers exist, not that they are the only ones.
    /// </summary>
    TEST_UTILITY_API static void assert_response_string_equals(
        const utility::string_t &response,
        const utility::string_t &version,
        const web::http::status_code &code,
        const utility::string_t &phrase,
        const std::map<utility::string_t, utility::string_t> &headers,
        const utility::string_t &body);

    /// <summary>
    /// Asserts the specified test_request is equal to its arguments.
    /// </summary>
    TEST_UTILITY_API static void assert_test_request_equals(
        const test_request *const p_request,
        const web::http::method &mtd,
        const utility::string_t &path,
        const utility::string_t &content_type,
        const utility::string_t &body);

    /// <summary>
    /// Asserts the specified test_response is equal to its arguments.
    /// </summary>
    TEST_UTILITY_API static void assert_test_response_equals(
        const test_response * const p_response,
        const web::http::status_code &code);

    TEST_UTILITY_API static void assert_test_response_equals(
        const test_response * const p_response,
        const web::http::status_code &code,
        const std::map<utility::string_t, utility::string_t> &headers);

    TEST_UTILITY_API static void assert_test_response_equals(
        const test_response * const p_response,
        const web::http::status_code &code,
        const web::http::http_headers &headers);

    TEST_UTILITY_API static void assert_test_response_equals(
        test_response * p_response,
        const web::http::status_code &code,
        const utility::string_t &content_type);

    TEST_UTILITY_API static void assert_test_response_equals(
        test_response * p_response,
        const web::http::status_code &code,
        const utility::string_t &content_type,
        const utility::string_t data);

private:
    http_asserts() {}
    ~http_asserts() {}
};

}}}}
