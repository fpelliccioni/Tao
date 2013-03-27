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
* request_stream_tests.cpp
*
* Tests cases covering using streams with HTTP request with http_client.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

#if defined(__cplusplus_winrt)
using namespace Windows::Storage;
#endif

using namespace web; 
using namespace utility;
using namespace concurrency;
using namespace web::http;
using namespace web::http::client;

using namespace tests::functional::http::utilities;

namespace tests { namespace functional { namespace http { namespace client {

utility::string_t get_full_name(const utility::string_t &name)
{
#if defined(__cplusplus_winrt)
    // On WinRT, we must compensate for the fact that we will be accessing files in the
    // Documents folder
    auto file = pplx::create_task(
        KnownFolders::DocumentsLibrary->CreateFileAsync(
            ref new Platform::String(name.c_str()), CreationCollisionOption::ReplaceExisting)).get();
    return file->Path->Data();
#else
    return name;
#endif
}

template<typename _CharType>
pplx::task<streams::streambuf<_CharType>> OPEN_R(const utility::string_t &name)
{
#if !defined(__cplusplus_winrt)
	return streams::file_buffer<_CharType>::open(name, std::ios_base::in);
#else
    try
    {
        auto file = pplx::create_task(
            KnownFolders::DocumentsLibrary->GetFileAsync(ref new Platform::String(name.c_str()))).get();

        return streams::file_buffer<_CharType>::open(file, std::ios_base::in);
    }
    catch(Platform::Exception^ exc) 
    { 
        throw utility::details::create_system_error(exc->HResult);
    }
#endif
}

SUITE(request_stream_tests)
{

// Used to prepare data for stream tests
void fill_file(const utility::string_t &name, size_t repetitions = 1)
{
    std::fstream stream(get_full_name(name), std::ios_base::out | std::ios_base::trunc);

    for (size_t i = 0; i < repetitions; i++)
        stream << "abcdefghijklmnopqrstuvwxyz";
}

void fill_buffer(streams::streambuf<uint8_t> rbuf, size_t repetitions = 1)
{
    const char *text = "abcdefghijklmnopqrstuvwxyz";
    size_t len = strlen(text);
    for (size_t i = 0; i < repetitions; i++)
        rbuf.putn((const uint8_t *)text, len);
}

#if defined(__cplusplus_winrt)
TEST_FIXTURE(uri_address, ixhr2_transfer_encoding)
{
    // Transfer encoding chunked is not supported. Not specifying the
    // content length should cause an exception from the task. Verify
    // that there is no unobserved exception

    http_client client(m_uri);

    auto buf = streams::producer_consumer_buffer<uint8_t>();
    buf.putc(22).wait();
    buf.close(std::ios_base::out).wait();

    http_request reqG(methods::PUT);
    reqG.set_body(buf.create_istream());
    VERIFY_THROWS(client.request(reqG).get(), http_exception);

    VERIFY_THROWS(client.request(methods::POST, U(""), buf.create_istream()).get(), http_exception);
}
#endif

TEST_FIXTURE(uri_address, set_body_stream)
{
    utility::string_t fname = U("request_stream.txt");
    fill_file(fname);

    test_http_server::scoped_server scoped(m_uri);
    test_http_server * p_server = scoped.server();
    http_client client(m_uri);

    http_request msg(methods::POST);
    msg.set_body(OPEN_R<uint8_t>(fname).get());
#if defined(__cplusplus_winrt)
	msg.headers().set_content_length(26);
#endif
    p_server->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, methods::POST, U("/"));
        VERIFY_ARE_EQUAL(26u, p_request->m_body.size());
        std::string str_body(std::begin(p_request->m_body), std::end(p_request->m_body));
        VERIFY_ARE_EQUAL(U("abcdefghijklmnopqrstuvwxyz"), ::utility::conversions::to_string_t(str_body));
        p_request->reply(200);
    });
    http_asserts::assert_response_equals(client.request(msg).get(), status_codes::OK);
}

// Implementation for request with stream test case.
static void stream_request_impl(const uri &address, bool withContentLength)
{
    utility::string_t fname = U("request_stream.txt");
    fill_file(fname);

    test_http_server::scoped_server scoped(address);
    test_http_server * p_server = scoped.server();
    http_client client(address);

    p_server->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, methods::POST, U("/"));
        VERIFY_ARE_EQUAL(26u, p_request->m_body.size());
        std::string str_body(std::begin(p_request->m_body), std::end(p_request->m_body));
        VERIFY_ARE_EQUAL(U("abcdefghijklmnopqrstuvwxyz"), ::utility::conversions::to_string_t(str_body));
        p_request->reply(200);
    });

    if(withContentLength)
    {
        http_asserts::assert_response_equals(client.request(methods::POST, U(""), OPEN_R<uint8_t>(fname).get(), 26, U("text/plain")).get(), status_codes::OK);
    }
    else
    {
        http_asserts::assert_response_equals(client.request(methods::POST, U(""), OPEN_R<uint8_t>(fname).get(), U("text/plain")).get(), status_codes::OK);
    }
}

#if !defined(__cplusplus_winrt)
TEST_FIXTURE(uri_address, without_content_length)
{
    stream_request_impl(m_uri, false);
}
#endif

TEST_FIXTURE(uri_address, with_content_length)
{
    stream_request_impl(m_uri, true);
}

TEST_FIXTURE(uri_address, producer_consumer_buffer_with_content_length)
{
    streams::producer_consumer_buffer<uint8_t> rbuf;
    fill_buffer(rbuf);
    rbuf.close(std::ios_base::out);

    test_http_server::scoped_server scoped(m_uri);
    test_http_server * p_server = scoped.server();
    http_client client(m_uri);

    http_request msg(methods::POST);
    msg.set_body(streams::istream(rbuf));
    msg.headers().set_content_length(26);

    p_server->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, methods::POST, U("/"));
        VERIFY_ARE_EQUAL(26u, p_request->m_body.size());
        std::string str_body(std::begin(p_request->m_body), std::end(p_request->m_body));
        VERIFY_ARE_EQUAL(U("abcdefghijklmnopqrstuvwxyz"), ::utility::conversions::to_string_t(str_body));
        p_request->reply(200);
    });
    http_asserts::assert_response_equals(client.request(msg).get(), status_codes::OK);
}

TEST_FIXTURE(uri_address, stream_partial_from_start)
{
    utility::string_t fname = U("stream_partial_from_start.txt");
    fill_file(fname, 200);

    test_http_server::scoped_server scoped(m_uri);
    test_http_server * p_server = scoped.server();
    http_client client(m_uri);

    http_request msg(methods::POST);
    auto stream = OPEN_R<uint8_t>(fname).get().create_istream();
    msg.set_body(stream);
    msg.headers().set_content_length(4500);

    p_server->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, methods::POST, U("/"));
        VERIFY_ARE_EQUAL(4500u, p_request->m_body.size());
        std::string str_body(std::begin(p_request->m_body), std::end(p_request->m_body));
        p_request->reply(200);
    });
    http_asserts::assert_response_equals(client.request(msg).get(), status_codes::OK);

    // We should only have read the first 4500 bytes.
    auto length = stream.seek(0, std::ios_base::cur);
    VERIFY_ARE_EQUAL((size_t)length, (size_t)4500);

    stream.close().get();
}

TEST_FIXTURE(uri_address, stream_partial_from_middle)
{
    utility::string_t fname = U("stream_partial_from_middle.txt");
    fill_file(fname,100);

    test_http_server::scoped_server scoped(m_uri);
    test_http_server * p_server = scoped.server();
    http_client client(m_uri);

    http_request msg(methods::POST);
    auto stream = OPEN_R<uint8_t>(fname).get().create_istream();
    msg.set_body(stream);
    msg.headers().set_content_length(13);

    stream.seek(13, std::ios_base::cur);

    p_server->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, methods::POST, U("/"));
        VERIFY_ARE_EQUAL(13u, p_request->m_body.size());
        std::string str_body(std::begin(p_request->m_body), std::end(p_request->m_body));
        VERIFY_ARE_EQUAL(str_body, "nopqrstuvwxyz");
        p_request->reply(200);
    });
    http_asserts::assert_response_equals(client.request(msg).get(), status_codes::OK);

    // We should only have read the first 26 bytes.
    auto length = stream.seek(0, std::ios_base::cur);
    VERIFY_ARE_EQUAL((int)length, 26);

    stream.close().get();
}


TEST_FIXTURE(uri_address, stream_close_early,
			 "Ignore", "563156")
{
	http_client client(m_uri);
	test_http_server::scoped_server scoped(m_uri);
	scoped.server()->next_request().then([](test_request *request)
	{
		request->reply(status_codes::OK);
	});

	// Make request.
	streams::producer_consumer_buffer<uint8_t> buf;
	auto responseTask = client.request(methods::PUT, U(""), buf.create_istream(), 10);

	// Write a bit of data then close the stream early.
	unsigned char data[5] = { '1', '2', '3', '4' };
	buf.putn(&data[0], 5).wait();
	buf.close(std::ios::in);
	buf.close(std::ios::out);

	// Verify exception occurred sending request.
	VERIFY_THROWS(responseTask.get(), std::exception);
    http_asserts::assert_response_equals(client.request(methods::GET, U("")).get(), status_codes::OK);
}

TEST_FIXTURE(uri_address, stream_close_early_by_exception,
			 "Ignore", "563156")
{
	http_client client(m_uri);
	test_http_server::scoped_server scoped(m_uri);
	scoped.server()->next_request().then([](test_request *request)
	{
		request->reply(status_codes::OK);
	});

	// Make request.
	auto buf = streams::producer_consumer_buffer<uint8_t>();
	auto responseTask = client.request(methods::PUT, U(""), buf.create_istream(), 10);

	// Write a bit of data then close the stream early.
	unsigned char data[5] = { '1', '2', '3', '4' };
	buf.putn(&data[0], 5).wait();
	buf.close(std::ios::in, std::make_exception_ptr(std::exception("my exception")));
	buf.close(std::ios::out, std::make_exception_ptr(std::exception("my exception")));

	// Verify exception occurred sending request.
	VERIFY_THROWS(responseTask.get(), std::exception);
    http_asserts::assert_response_equals(client.request(methods::GET, U("")).get(), status_codes::OK);
}

TEST_FIXTURE(uri_address, get_with_body_nono)
{
    http_client client(m_uri);

	streams::producer_consumer_buffer<uint8_t> buf;
    buf.putc(22).wait();
    buf.close(std::ios_base::out).wait();

    http_request reqG(methods::GET);
    reqG.set_body(buf.create_istream());
    VERIFY_THROWS(client.request(reqG).get(), http_exception);

    http_request reqH(methods::HEAD);
    reqH.set_body(buf.create_istream());
    VERIFY_THROWS(client.request(reqH).get(), http_exception);
}

} // SUITE(request_stream_tests)

}}}}