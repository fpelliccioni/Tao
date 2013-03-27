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
* proxy_tests.cpp
*
* Tests cases for using proxies with http_clients.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace web::http;
using namespace web::http::client;

using namespace tests::functional::http::utilities;

namespace tests { namespace functional { namespace http { namespace client {

SUITE(proxy_tests)
{

#ifndef __cplusplus_winrt
// IXHR2 does not allow the proxy settings to be changed
TEST_FIXTURE(uri_address, auto_discovery_proxy)
{
    test_http_server::scoped_server scoped(m_uri);
    scoped.server()->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, methods::PUT, U("/"), U("text/plain"), U("this is a test"));
        p_request->reply(200);
    });
    http_client_config config;
	
    config.set_proxy(web_proxy::use_auto_discovery);
    VERIFY_IS_FALSE(config.proxy().is_disabled());
    VERIFY_IS_FALSE(config.proxy().is_specified());
    http_client client(m_uri, config);

    http_asserts::assert_response_equals(client.request(methods::PUT, U("/"), U("this is a test")).get(), status_codes::OK);
}

TEST_FIXTURE(uri_address, disabled_proxy)
{
    test_http_server::scoped_server scoped(m_uri);
    scoped.server()->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, methods::PUT, U("/"), U("text/plain"), U("sample data"));
        p_request->reply(status_codes::OK);
    });
    
    http_client_config config;
    config.set_proxy(web_proxy(web_proxy::disabled));
    VERIFY_IS_TRUE(config.proxy().is_disabled());
    VERIFY_IS_FALSE(config.proxy().is_auto_discovery());
    VERIFY_IS_FALSE(config.proxy().is_specified());
    VERIFY_IS_FALSE(config.proxy().is_default());

    http_client client(m_uri, config);
    http_asserts::assert_response_equals(client.request(methods::PUT, U("/"), U("sample data")).get(), status_codes::OK);
}

#endif // __cplusplus_winrt

#ifdef __cplusplus_winrt
TEST_FIXTURE(uri_address, no_proxy_options_on_winrt)
{
    http_client_config config;	
    config.set_proxy(web_proxy::use_auto_discovery);
    http_client client(m_uri, config);

	VERIFY_THROWS(client.request(methods::GET, U("/")).get(), http_exception);
}
#endif

// Test is disabled since one time the office site was unavaliable.
// We want 100% reliability in our automated test cases.
TEST_FIXTURE(uri_address, proxy_with_credentials,
             "Ignore", "Manual")
{
    uri u(U("http://netproxy.redmond.corp.microsoft.com"));

    web_proxy proxy(u);
    VERIFY_IS_TRUE(proxy.is_specified());
    VERIFY_ARE_EQUAL(u, proxy.address());
    credentials cred(U("artur"), U("fred")); // relax, this is not my real password
    proxy.set_credentials(cred);

    http_client_config config;
    config.set_proxy(proxy);

    // Access to this server will succeed because the first request will not be challenged and hence 
    // my bogus credentials will not be supplied.
    http_client client(U("http://www.microsoft.com"), config);

    http_response response = client.request(methods::GET).get();
    VERIFY_ARE_EQUAL(status_codes::OK, response.status_code());
    response.content_ready().wait();
}

} // SUITE(proxy_tests)

}}}}