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
* client_construction.cpp
*
* Tests cases for covering creating http_clients.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"
#include <fstream>

using namespace web::http;
using namespace web::http::client;

using namespace tests::functional::http::utilities;

namespace tests { namespace functional { namespace http { namespace client {

SUITE(client_construction)
{

// Tests using different types of strings to construct an http_client.
TEST_FIXTURE(uri_address, string_types)
{
    // The goal of this test case is to make sure we can compile,
    // if the URI class doesn't have the proper constructors it won't.
    // So we don't need to actually do a request.
    http_client c1(U("http://localhost:4567/"));
    http_client c3(utility::string_t(U("http://localhost:4567/")));
}

// Tests different variations on specifying the URI in http_client constructor.
TEST_FIXTURE(uri_address, different_uris)
{
    const utility::string_t paths[] = 
    {
        U(""),
        U("/"),
        U("/toplevel/nested"),
        U("/toplevel/nested/")
    };
    const utility::string_t expected_paths[] =
    {
        U("/"),
        U("/"),
        U("/toplevel/nested"),
        U("/toplevel/nested/")
    };
    const size_t num_paths = sizeof(paths) / sizeof(paths[0]);
    for(size_t i = 0; i < num_paths; ++i)
    {
        uri address(U("http://localhost:55678") + paths[i]);
        test_http_server::scoped_server scoped(address);
        http_client client(address);
        test_connection(scoped.server(), &client, expected_paths[i]);
    }
}

// Helper function verifies that when constructing an http_client with given
// URI std::invalid_argument is thrown.
static void verify_client_invalid_argument(const uri &address)
{
    try
    {
        http_client client(address);
        VERIFY_IS_TRUE(false);
    } catch(std::invalid_argument &)
    {
        // expected
    }
}

TEST_FIXTURE(uri_address, client_construction_error_cases)
{
    uri address(U("nothttp://localhost:34567/"));

    // Invalid scheme.
    verify_client_invalid_argument(address);
    
    // empty host.
    address = uri(U("http://:34567/"));
    verify_client_invalid_argument(address);
}

TEST_FIXTURE(uri_address, move_not_init)
{
    test_http_server::scoped_server scoped(m_uri);
    
    // move constructor
    http_client original(m_uri);
    http_client new_client = std::move(original);
    test_connection(scoped.server(), &new_client, U("/"));

    // move assignment
    original = http_client(m_uri);
    test_connection(scoped.server(), &original, U("/"));
}

TEST_FIXTURE(uri_address, move_init)
{
    test_http_server::scoped_server scoped(m_uri);
    
    // move constructor
    http_client original(m_uri);
    test_connection(scoped.server(), &original, U("/"));
    http_client new_client = std::move(original);
    test_connection(scoped.server(), &new_client, U("/"));

    // move assignment
    original = http_client(m_uri);
    test_connection(scoped.server(), &original, U("/"));
}

// Verify that we can read the config from the http_client
TEST_FIXTURE(uri_address, get_client_config)
{
    test_http_server::scoped_server scoped(m_uri);

    http_client_config config;
    utility::seconds timeout(100);
    config.set_timeout(timeout);
    http_client client(m_uri, config);

    const http_client_config& config2 = client.client_config();
    VERIFY_ARE_EQUAL(config2.timeout().count(), timeout.count());
}

} // SUITE(client_construction)

}}}}