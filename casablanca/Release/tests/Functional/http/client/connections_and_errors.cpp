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
* connections_and_errors.cpp
*
* Tests cases for covering issues dealing with http_client lifetime, underlying TCP connections, and general connection errors.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

#ifndef __cplusplus_winrt
#include "http_listener.h"
#endif

using namespace web; 
using namespace utility;
using namespace concurrency;
using namespace web::http;
using namespace web::http::client;

using namespace tests::functional::http::utilities;

namespace tests { namespace functional { namespace http { namespace client {

// Test implementation for pending_requests_after_client.
static void pending_requests_after_client_impl(uri address, bool guarantee_order)
{
    test_http_server::scoped_server scoped(address);
    const method mtd = methods::GET;

    const size_t num_requests = 10;
    std::vector<pplx::task<http_response>> responses;
    {
        http_client_config config;
        config.set_guarantee_order(guarantee_order);
        http_client client(address, config);

        // send requests.
        for(size_t i = 0; i < num_requests; ++i)
        {
            responses.push_back(client.request(mtd));
        }
    }

    // send responses.
    for(size_t i = 0; i < num_requests; ++i)
    {
        scoped.server()->next_request().then([&](test_request *request)
        {
            http_asserts::assert_test_request_equals(request, mtd, U("/"));
            VERIFY_ARE_EQUAL(0u, request->reply(status_codes::OK));
        });
    }

    // verify responses.
    for(size_t i = 0; i < num_requests; ++i)
    {
        http_asserts::assert_response_equals(responses[i].get(), status_codes::OK);
    }
}

SUITE(connections_and_errors)
{

// Tests requests still outstanding after the http_client has been destroyed.
TEST_FIXTURE(uri_address, pending_requests_after_client)
{
    pending_requests_after_client_impl(m_uri, true);
    pending_requests_after_client_impl(m_uri, false);
}

TEST_FIXTURE(uri_address, server_doesnt_exist,
             "Ignore:Linux", "627642")
{
    http_client client(m_uri);

    try
    {
        client.request(methods::GET).wait();
        VERIFY_IS_TRUE(false);
    }
    catch(std::exception &)
    {
        // Expected.
    }
}

TEST_FIXTURE(uri_address, server_close_without_responding,
             "Ignore:Linux", "627612")
{
    test_http_server server(m_uri);
    VERIFY_ARE_EQUAL(0u, server.open());
    http_client client(m_uri);

    // Send request.
    auto request = client.request(methods::PUT);

    // Close server connection.
    server.wait_for_request();
    server.close();

    try
    {
        request.wait();
        VERIFY_IS_TRUE(false);
    }
    catch(const std::exception &)
    {
        // expected
    }

    // Try sending another request.
    try
    {
        client.request(methods::GET).wait();
        VERIFY_IS_TRUE(false);
    }
    catch(const std::exception &)
    {
        // expected
    }
}

// This test hangs or crashes intermittently on Linux
TEST_FIXTURE(uri_address, request_timeout, "Ignore:Linux", "TFS#612139")
{
    test_http_server::scoped_server scoped(m_uri);
    http_client_config config;
    config.set_timeout(utility::seconds(1));

    http_client client(m_uri, config);

    VERIFY_THROWS_HTTP_ERROR_CODE(client.request(methods::GET).get(), std::errc::timed_out);
}

#if !defined(__cplusplus_winrt)
// This test still sometimes segfaults on Linux, but I'm re-enabling it [AL]
TEST_FIXTURE(uri_address, content_ready_timeout)
{
    auto listener = ::http::listener::http_listener::create(m_uri);
    VERIFY_ARE_EQUAL(0, listener.open());

    streams::producer_consumer_buffer<uint8_t> buf;

    listener.support([buf](http_request request)
    {
        request.reply(200, streams::istream(buf), U("text/plain"));
    });

    {
        http_client_config config;
        config.set_timeout(utility::seconds(1));
        http_client client(m_uri, config);
        http_request msg(methods::GET);
        http_response rsp = client.request(msg).get();

        // The response body should timeout and we should recieve an exception
        VERIFY_THROWS_HTTP_ERROR_CODE(rsp.content_ready().wait(), std::errc::timed_out);
    }

    buf.close(std::ios_base::out).wait();
    VERIFY_ARE_EQUAL(0, listener.close());
}

TEST_FIXTURE(uri_address, stream_timeout)
{
    auto listener = ::http::listener::http_listener::create(m_uri);
    VERIFY_ARE_EQUAL(0, listener.open());

    streams::producer_consumer_buffer<uint8_t> buf;

    listener.support([buf](http_request request)
    {
        request.reply(200, streams::istream(buf), U("text/plain"));
    });

    {
        http_client_config config;
        config.set_timeout(utility::seconds(1));
        http_client client(m_uri, config);
        http_request msg(methods::GET);
        http_response rsp = client.request(msg).get();

        // The response body should timeout and we should receive an exception
        VERIFY_THROWS_HTTP_ERROR_CODE(rsp.body().read_to_end(streams::producer_consumer_buffer<uint8_t>()).wait(), std::errc::timed_out);
    }

    buf.close(std::ios_base::out).wait();
    VERIFY_ARE_EQUAL(0, listener.close());
}
#endif

} // SUITE(connections_and_errors)

}}}}
