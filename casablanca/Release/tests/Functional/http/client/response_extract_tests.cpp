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
* response_extract_tests.cpp
*
* Tests cases covering extract functions on HTTP response.
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
using namespace utility::conversions;
using namespace web::http;
using namespace web::http::client;

using namespace tests::functional::http::utilities;

namespace tests { namespace functional { namespace http { namespace client {

SUITE(response_extract_tests)
{

// Helper function to send a request and response with given values.
template<typename CharType>
static http_response send_request_response(
    test_http_server *p_server, 
    http_client * p_client, 
    const utility::string_t&content_type, 
    const std::basic_string<CharType> &data)
{
    const method method = methods::GET;
    const ::http::status_code code = status_codes::OK;
    std::map<utility::string_t, utility::string_t> headers;
    headers[U("Content-Type")] = content_type;
    p_server->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, method, U("/"));
        VERIFY_ARE_EQUAL(0u, p_request->reply(code, U(""), headers, data));
    });
    http_response rsp = p_client->request(method).get();
    http_asserts::assert_response_equals(rsp, code, headers);
    return rsp;
}

static utf16string switch_endian_ness(const utf16string &src_str)
{
    utf16string dest_str;
    dest_str.resize(src_str.size());
    unsigned char *src = (unsigned char *)&src_str[0];
    unsigned char *dest = (unsigned char *)&dest_str[0];
    for(size_t i = 0; i < dest_str.size() * 2; i+=2)
    {
        dest[i] = src[i + 1];
        dest[i + 1] = src[i];
    }
    return dest_str;
}

TEST_FIXTURE(uri_address, extract_string)
{
    test_http_server::scoped_server scoped(m_uri);
    http_client client(m_uri);

    // default encoding (Latin1)
    std::string data("YOU KNOW ITITITITI");
    http_response rsp = send_request_response(scoped.server(), &client, U("text/plain"), data);
    VERIFY_ARE_EQUAL(to_string_t(data), rsp.extract_string().get());

    // us-ascii
    rsp = send_request_response(scoped.server(), &client, U("text/plain;  charset=  us-AscIi"), data);
    VERIFY_ARE_EQUAL(to_string_t(data), rsp.extract_string().get());

    // Latin1
    rsp = send_request_response(scoped.server(), &client, U("text/plain;charset=iso-8859-1"), data);
    VERIFY_ARE_EQUAL(to_string_t(data), rsp.extract_string().get());
    
    // utf-8
    rsp = send_request_response(scoped.server(), &client, U("text/plain; charset  =  UTF-8"), data);
    VERIFY_ARE_EQUAL(to_string_t(data), rsp.extract_string().get());

#ifdef _MS_WINDOWS
    // utf-16le
    utf16string wdata(utf8_to_utf16("YES NOW, HERHEHE****"));
    data = utf16_to_utf8(wdata);
    rsp = send_request_response(scoped.server(), &client, U("text/plain; charset=utf-16le"), wdata);
    VERIFY_ARE_EQUAL(to_string_t(data), rsp.extract_string().get());

    // utf-16be
    wdata = switch_endian_ness(wdata);
    rsp = send_request_response(scoped.server(), &client, U("text/plain; charset=utf-16be"), wdata);
    VERIFY_ARE_EQUAL(to_string_t(data), rsp.extract_string().get());

    // utf-16 no BOM (utf-16be)
    rsp = send_request_response(scoped.server(), &client, U("text/plain; charset=utf-16"), wdata);
    VERIFY_ARE_EQUAL(to_string_t(data), rsp.extract_string().get());

    // utf-16 big endian BOM.
    wdata.insert(wdata.begin(), ('\0'));
    unsigned char * start = (unsigned char *)&wdata[0];
    start[0] = 0xFE;
    start[1] = 0xFF;
    rsp = send_request_response(scoped.server(), &client, U("text/plain; charset=utf-16"), wdata);
    VERIFY_ARE_EQUAL(to_string_t(data), rsp.extract_string().get());

    // utf-16 little endian BOM.
    wdata = utf8_to_utf16("YOU KNOW THIS **********KICKS");
    data = utf16_to_utf8(wdata);
    wdata.insert(wdata.begin(), '\0');
    start = (unsigned char *)&wdata[0];
    start[0] = 0xFF;
    start[1] = 0xFE;
    rsp = send_request_response(scoped.server(), &client, U("text/plain; charset=utf-16"), wdata);
    VERIFY_ARE_EQUAL(to_string_t(data), rsp.extract_string().get());
#endif
}

#ifdef _MS_WINDOWS
#ifndef __cplusplus_winrt
TEST_FIXTURE(uri_address, extract_string_endian_uneven_bytes)
{
	// It is really hard to write this test case with our test server infrastructure so we will
	// have to use our http_listener.
	listener::http_listener listener = listener::http_listener::create(m_uri);
	VERIFY_ARE_EQUAL(0L, listener.open());
	http_client client(m_uri);

	listener.support([](http_request request)
	{
		http_response response(status_codes::OK);
		response.set_body("uneven1");
		response.headers().set_content_type(U("text/plain; charset=utf-16be"));
		request.reply(response).wait(); 
	});

	http_response response = client.request(methods::GET, U("")).get();
	VERIFY_THROWS(response.extract_string().get(), std::exception);
}
#endif
#endif

TEST_FIXTURE(uri_address, extract_string_incorrect)
{
    test_http_server::scoped_server scoped(m_uri);
    http_client client(m_uri);

    // with non matching content type.
    const std::string data("YOU KNOW ITITITITI");
    http_response rsp = send_request_response(scoped.server(), &client, U("non_text"), data);
    VERIFY_THROWS(rsp.extract_string().get(), http_exception);

    // with unknown charset
    rsp = send_request_response(scoped.server(), &client, U("text/plain; charset=uis-ascii"), data);
    VERIFY_THROWS(rsp.extract_string().get(), http_exception);
}

#ifndef __cplusplus_winrt
TEST_FIXTURE(uri_address, extract_empty_string)
{
    ::http::listener::http_listener listener = listener::http_listener::create(m_uri);
    http_client client(m_uri);
    listener.support([](http_request msg)
    {
        auto ResponseStreamBuf = streams::producer_consumer_buffer<uint8_t>();
        ResponseStreamBuf.close(std::ios_base::out).wait();

        msg.reply(status_codes::OK, ResponseStreamBuf.create_istream(), U("text/plain")).wait();
    });

    listener.open();

    auto response = client.request(methods::GET).get();
    auto data = response.extract_string().get();

    VERIFY_ARE_EQUAL(0, data.size());
}
#endif

TEST_FIXTURE(uri_address, extract_json)
{
    test_http_server::scoped_server scoped(m_uri);
    http_client client(m_uri);

    // default encoding (Latin1)
    json::value data = json::value::string(U("JSON string object"));
    http_response rsp = send_request_response(scoped.server(), &client, U("application/json"), to_utf8string(data.to_string()));
    VERIFY_ARE_EQUAL(data.to_string(), rsp.extract_json().get().to_string());

    // us-ascii
    rsp = send_request_response(scoped.server(), &client, U("application/json;  charset=  us-AscIi"), to_utf8string(data.to_string()));
    VERIFY_ARE_EQUAL(data.to_string(), rsp.extract_json().get().to_string());

    // Latin1
    rsp = send_request_response(scoped.server(), &client, U("application/json;charset=iso-8859-1"), to_utf8string(data.to_string()));
    VERIFY_ARE_EQUAL(data.to_string(), rsp.extract_json().get().to_string());

    // utf-8
    rsp = send_request_response(scoped.server(), &client, U("application/json; charset  =  UTF-8"), to_utf8string((data.to_string())));
    VERIFY_ARE_EQUAL(data.to_string(), rsp.extract_json().get().to_string());

#ifdef _MS_WINDOWS
    // utf-16le
    auto utf16str = data.to_string();
    rsp = send_request_response(scoped.server(), &client, U("application/json; charset=utf-16le"), utf16str);
    VERIFY_ARE_EQUAL(data.to_string(), rsp.extract_json().get().to_string());

    // utf-16be
    utf16string modified_data = data.to_string();
    modified_data = switch_endian_ness(modified_data);
    rsp = send_request_response(scoped.server(), &client, U("application/json; charset=utf-16be"), modified_data);
    VERIFY_ARE_EQUAL(data.to_string(), rsp.extract_json().get().to_string());

    // utf-16 no BOM (utf-16be)
    rsp = send_request_response(scoped.server(), &client, U("application/json; charset=utf-16"), modified_data);
    VERIFY_ARE_EQUAL(data.to_string(), rsp.extract_json().get().to_string());

    // utf-16 big endian BOM.
    modified_data.insert(modified_data.begin(), U('\0'));
    unsigned char * start = (unsigned char *)&modified_data[0];
    start[0] = 0xFE;
    start[1] = 0xFF;
    rsp = send_request_response(scoped.server(), &client, U("application/json; charset=utf-16"), modified_data);
    VERIFY_ARE_EQUAL(data.to_string(), rsp.extract_json().get().to_string());

    // utf-16 little endian BOM.
    modified_data = data.to_string();
    modified_data.insert(modified_data.begin(), U('\0'));
    start = (unsigned char *)&modified_data[0];
    start[0] = 0xFF;
    start[1] = 0xFE;
    rsp = send_request_response(scoped.server(), &client, U("application/json; charset=utf-16"), modified_data);
    VERIFY_ARE_EQUAL(data.to_string(), rsp.extract_json().get().to_string());
#endif
}

TEST_FIXTURE(uri_address, extract_json_incorrect)
{
    test_http_server::scoped_server scoped(m_uri);
    http_client client(m_uri);

    // with non matching content type.
    json::value json_data = json::value::string(U("JSON string object"));
    http_response rsp = send_request_response(scoped.server(), &client, U("bad guy"), json_data.to_string());
    VERIFY_THROWS(rsp.extract_json().get(), http_exception);

    // with unknown charset.
    rsp = send_request_response(scoped.server(), &client, U("application/json; charset=us-askjhcii"), json_data.to_string());
    VERIFY_THROWS(rsp.extract_json().get(), http_exception);
}

TEST_FIXTURE(uri_address, set_stream_try_extract_json)
{
	test_http_server::scoped_server scoped(m_uri);
    http_client client(m_uri);

	http_request request(methods::GET);
	streams::ostream responseStream = streams::bytestream::open_ostream<std::vector<uint8_t>>();
	request.set_response_stream(responseStream);
	scoped.server()->next_request().then([](test_request *req)
	{
		std::map<utility::string_t, utility::string_t> headers;
		headers[header_names::content_type] = U("application/json");
		req->reply(status_codes::OK, U("OK"), headers, U("{true}"));
	});
	
	http_response response = client.request(request).get();
	VERIFY_THROWS(response.extract_json().get(), http_exception);
}

TEST_FIXTURE(uri_address, extract_vector)
{
    test_http_server::scoped_server scoped(m_uri);
    http_client client(m_uri);

    // textual content type - with unknown charset
    std::string data("YOU KNOW ITITITITI");
    std::vector<unsigned char> vector_data;
    std::for_each(data.begin(), data.end(), [&](char ch)
    {
        vector_data.push_back((unsigned char)ch);
    });
    http_response rsp = send_request_response(scoped.server(), &client, U("text/plain; charset=unknown"), data);
    VERIFY_ARE_EQUAL(vector_data, rsp.extract_vector().get());

    // textual type with us-ascii
    rsp = send_request_response(scoped.server(), &client, U("text/plain;  charset=  us-AscIi"), data);
    VERIFY_ARE_EQUAL(vector_data, rsp.extract_vector().get());

    // textual type with Latin1
    rsp = send_request_response(scoped.server(), &client, U("text/plain;  charset=iso-8859-1"), data);
    VERIFY_ARE_EQUAL(vector_data, rsp.extract_vector().get());

    // textual type with utf-8
    rsp = send_request_response(scoped.server(), &client, U("text/plain;  charset=utf-8"), data);
    VERIFY_ARE_EQUAL(vector_data, rsp.extract_vector().get());

    // textual type with utf-16le
    rsp = send_request_response(scoped.server(), &client, U("text/plain;  charset=utf-16LE"), data);
    VERIFY_ARE_EQUAL(vector_data, rsp.extract_vector().get());

    // textual type with utf-16be
    rsp = send_request_response(scoped.server(), &client, U("text/plain;  charset=UTF-16be"), data);
    VERIFY_ARE_EQUAL(vector_data, rsp.extract_vector().get());

    // textual type with utf-16
    rsp = send_request_response(scoped.server(), &client, U("text/plain;  charset=utf-16"), data);
    VERIFY_ARE_EQUAL(vector_data, rsp.extract_vector().get());

    // non textual content type
    rsp = send_request_response(scoped.server(), &client, U("blah;  charset=utf-16"), data);
    VERIFY_ARE_EQUAL(vector_data, rsp.extract_vector().get());
}

TEST_FIXTURE(uri_address, set_stream_try_extract_vector)
{
	test_http_server::scoped_server scoped(m_uri);
    http_client client(m_uri);

	http_request request(methods::GET);
	streams::ostream responseStream = streams::bytestream::open_ostream<std::vector<uint8_t>>();
	request.set_response_stream(responseStream);
	scoped.server()->next_request().then([](test_request *req)
	{
		std::map<utility::string_t, utility::string_t> headers;
		headers[header_names::content_type] = U("text/plain");
		req->reply(status_codes::OK, U("OK"), headers, U("data"));
	});
	
	http_response response = client.request(request).get();
	VERIFY_THROWS(response.extract_vector().get(), http_exception);
}

TEST_FIXTURE(uri_address, head_response)
{
    test_http_server::scoped_server scoped(m_uri);
    http_client client(m_uri);

	const method method = methods::HEAD;
    const ::http::status_code code = status_codes::OK;
    std::map<utility::string_t, utility::string_t> headers;
    headers[U("Content-Type")] = U("text/plain");
    headers[U("Content-Length")] = U("100");
    scoped.server()->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, method, U("/"));
        VERIFY_ARE_EQUAL(0u, p_request->reply(code, U(""), headers));
    });
    http_response rsp = client.request(method).get();
    VERIFY_ARE_EQUAL(0u, rsp.body().streambuf().in_avail());
}

} // SUITE(response_extract_tests)

}}}}