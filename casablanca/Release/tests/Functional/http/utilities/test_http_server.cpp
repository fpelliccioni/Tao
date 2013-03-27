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
* test_http_server.cpp -- Defines a test server to handle requests and sending responses.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#include "stdafx.h"

#ifdef _MS_WINDOWS
#include <http.h>
#include <agents.h>
#endif
#include <algorithm>

#include "uri.h"
#include "test_http_server.h"
#include "http_listener.h"

using namespace web; using namespace utility;
using namespace utility::conversions;

namespace tests { namespace functional { namespace http { namespace utilities {

#ifdef _MS_WINDOWS
// Helper function to parse verb from Windows HTTP Server API.
static utility::string_t parse_verb(const HTTP_REQUEST *p_http_request)
{
    utility::string_t method;
    std::string temp;
    switch(p_http_request->Verb)
        {
        case HttpVerbGET:
            method = U("GET");
            break;
        case HttpVerbPOST:
            method = U("POST");
            break;
        case HttpVerbPUT:
            method = U("PUT");
            break;
        case HttpVerbDELETE:
            method = U("DELETE");
            break;
        case HttpVerbHEAD:
            method = U("HEAD");
            break;
        case HttpVerbOPTIONS:
            method = U("OPTIONS");
            break;
        case HttpVerbTRACE:
            method = U("TRACE");
            break;
        case HttpVerbCONNECT:
            method = U("CONNECT");
            break;
        case HttpVerbUnknown:
            temp = p_http_request->pUnknownVerb;
            method = utility::string_t(temp.begin(), temp.end());
        default:
            break;
        }
    return method;
}

/// <summary>
/// String values for all HTTP Server API known headers.
/// NOTE: the order here is important it is from the _HTTP_HEADER_ID enum.
/// </summary>
static utility::string_t HttpServerAPIKnownHeaders[] =
{
    U("Cache-Control"),
    U("Connection"),
    U("Data"),
    U("Keep-Alive"),
    U("Pragma"),
    U("Trailer"),
    U("Transfer-Encoding"),
    U("Upgrade"),
    U("Via"),
    U("Warning"),
    U("Allow"),
    U("Content-Length"),
    U("Content-Type"),
    U("Content-Encoding"),
    U("Content-Language"),
    U("Content-Location"),
    U("Content-Md5"),
    U("Content-Range"),
    U("Expires"),
    U("Last-Modified"),
    U("Accept"),
    U("Accept-Charset"),
    U("Accept-Encoding"),
    U("Accept-Language"),
    U("Accept-Authorization"),
    U("Cookie"),
    U("Expect"),
    U("From"),
    U("Host"),
    U("If-Match"),
    U("If-Modified-Since"),
    U("If-None-Match"),
    U("If-Range"),
    U("If-Unmodified-Since"),
    U("Max-Forwards"),
    U("Proxy-Authorization"),
    U("Referer"),
    U("Range"),
    U("Te"),
    U("Translate"),
    U("User-Agent"),
    U("Request-Maximum"),
    U("Accept-Ranges"),
    U("Age"),
    U("Etag"),
    U("Location"),
    U("Proxy-Authenticate"),
    U("Retry-After"),
    U("Server"),
    U("Set-Cookie"),
    U("Vary"),
    U("Www-Authenticate"),
    U("Response-Maximum")
};

static utility::string_t char_to_wstring(const char * src)
{
    if(src == nullptr)
    {
        return utility::string_t();
    }
    std::string temp(src);
    return utility::string_t(temp.begin(), temp.end());
}

static std::map<utility::string_t, utility::string_t> parse_http_headers(const HTTP_REQUEST_HEADERS &headers)
{
    std::map<utility::string_t, utility::string_t> headers_map;
    for(USHORT i = 0; i < headers.UnknownHeaderCount; ++i)
    {
        headers_map[char_to_wstring(headers.pUnknownHeaders[i].pName)] = char_to_wstring(headers.pUnknownHeaders[i].pRawValue);
    }
    for(int i = 0; i < HttpHeaderMaximum; ++i)
    {
        if(headers.KnownHeaders[i].RawValueLength != 0)
        {
            headers_map[HttpServerAPIKnownHeaders[i]] = char_to_wstring(headers.KnownHeaders[i].pRawValue);
        }
    }
    return headers_map;
}

class _test_http_server
{
public:
    _test_http_server(const utility::string_t &uri) 
        : m_uri(uri), m_session(0), m_url_group(0), m_request_queue(nullptr)
    {
        HTTPAPI_VERSION httpApiVersion = HTTPAPI_VERSION_2;
        HttpInitialize(httpApiVersion, HTTP_INITIALIZE_SERVER, NULL);
    }
    ~_test_http_server()
    {
        HttpTerminate(HTTP_INITIALIZE_SERVER, NULL);
    }

    unsigned long open()
    {
        // Open server session.
        HTTPAPI_VERSION httpApiVersion = HTTPAPI_VERSION_2;
        ULONG error_code = HttpCreateServerSession(httpApiVersion, &m_session, 0);
        if(error_code)
        {
            return error_code;
        }

        // Create Url group.
        error_code = HttpCreateUrlGroup(m_session, &m_url_group, 0);
        if(error_code)
        {
            return error_code;
        }

        // Create request queue.
        error_code = HttpCreateRequestQueue(httpApiVersion, U("test_http_server"), NULL, NULL, &m_request_queue);
        if(error_code)
        {
            return error_code;
        }

        // Windows HTTP Server API will not accept a uri with an empty path, it must have a '/'.
        auto host_uri = m_uri.to_string();
        if(m_uri.is_path_empty() && host_uri[host_uri.length() - 1] != '/' && m_uri.query().empty() && m_uri.fragment().empty())
        {
            host_uri.append(U("/"));
        }

        // Add Url.
        error_code = HttpAddUrlToUrlGroup(m_url_group, host_uri.c_str(), (HTTP_URL_CONTEXT)this, 0);
        if(error_code)
        {
            return error_code;
        }

        // Associate Url group with request queue.
        HTTP_BINDING_INFO bindingInfo;
        bindingInfo.RequestQueueHandle = m_request_queue;
        bindingInfo.Flags.Present = 1;
        error_code = HttpSetUrlGroupProperty(m_url_group, HttpServerBindingProperty, &bindingInfo, sizeof(HTTP_BINDING_INFO));
        if(error_code)
        {
            return error_code;
        }
        
        // Spawn a task to handle receiving incoming requests.
        m_request_task = pplx::create_task([this]()
        {
            for(;;)
            {
                const ULONG buffer_length = 1024 * 4;
                char buffer[buffer_length];
                ULONG bytes_received = 0;
                HTTP_REQUEST *p_http_request = (HTTP_REQUEST *)buffer;

                // Read in everything except the body.
                ULONG error_code = HttpReceiveHttpRequest(
                    m_request_queue,
                    HTTP_NULL_ID,
                    0,
                    p_http_request,
                    buffer_length,
                    &bytes_received,
                    0);
                if(error_code)
                {
                    break;
                }

                // Now create request structure.
                auto p_test_request = new test_request();
                m_requests_memory.push_back(p_test_request);
                p_test_request->m_request_id = p_http_request->RequestId;
                p_test_request->m_p_server = this;
                p_test_request->m_path = utf8_to_utf16(p_http_request->pRawUrl);
                p_test_request->m_method = parse_verb(p_http_request);
                p_test_request->m_headers = parse_http_headers(p_http_request->Headers);

                // Read in request body.
                ULONG content_length;
                const bool has_content_length = p_test_request->match_header(U("Content-Length"), content_length);
                if(has_content_length && content_length > 0)
                {
                    p_test_request->m_body.resize(content_length);
                    auto result = 
                        HttpReceiveRequestEntityBody(
                            m_request_queue,
                            p_http_request->RequestId,
                            HTTP_RECEIVE_REQUEST_ENTITY_BODY_FLAG_FILL_BUFFER,
                            &p_test_request->m_body[0],
                            content_length,
                            &bytes_received,
                            NULL);
                    VERIFY_ARE_EQUAL(0, result);
                }

                utility::string_t transfer_encoding;
                const bool has_transfer_encoding = p_test_request->match_header(U("Transfer-Encoding"), transfer_encoding);
                if(has_transfer_encoding && transfer_encoding == U("chunked"))
                {
                    content_length = 0;
                    char buffer[4096];
                    auto result = 
                        HttpReceiveRequestEntityBody(
                            m_request_queue,
                            p_http_request->RequestId,
                            HTTP_RECEIVE_REQUEST_ENTITY_BODY_FLAG_FILL_BUFFER,
                            (LPVOID)buffer,
                            4096,
                            &bytes_received,
                            NULL);

                    while ( result == NO_ERROR )
                    {
                        content_length += bytes_received;
                        p_test_request->m_body.resize(content_length);
                        memcpy(&p_test_request->m_body[content_length-bytes_received], buffer, bytes_received);

                        result = 
                            HttpReceiveRequestEntityBody(
                                m_request_queue,
                                p_http_request->RequestId,
                                HTTP_RECEIVE_REQUEST_ENTITY_BODY_FLAG_FILL_BUFFER,
                                (LPVOID)buffer,
                                4096,
                                &bytes_received,
                                NULL);
                    }

                    VERIFY_ARE_EQUAL(ERROR_HANDLE_EOF, result);
                }


                // Place request buffer.
                Concurrency::asend(m_requests, p_test_request);
            }
        });

        return 0;
    }

    unsigned long close()
    {
        // Wait for all outstanding next_requests to be processed. A hang here means the test case
        // isn't making sure all the requests have been satisfied. We will only wait for 1 minute and then
        // allow the test case to continue anyway. This should be enough time for any outstanding requests to come
        // in and avoid causing an AV. 
        //
        // If the requests haven't been fullfilled by now then there is a test bug so we still will mark failure.
        bool allRequestsSatisfied = true;
        std::for_each(m_all_next_request_tasks.begin(), m_all_next_request_tasks.end(), [&](pplx::task<test_request *> request_task)
        {
            if(!request_task.is_done())
            {
                allRequestsSatisfied = false;
            }
        });
        if(!allRequestsSatisfied)
        {
            // Just wait for either all the requests to finish or for the timer task, it doesn't matter which one.
            auto allRequestsTask = pplx::when_all(m_all_next_request_tasks.begin(), m_all_next_request_tasks.end()).then([](const std::vector<test_request *> &){});
            auto timerTask = pplx::create_delayed_task(30000, [](){});
            (timerTask || allRequestsTask).wait();
            VERIFY_IS_TRUE(false, "HTTP test case didn't properly wait for all requests to be satisfied.");
        }

        // Windows HTTP Server API will not accept a uri with an empty path, it must have a '/'.
        utility::string_t host_uri = m_uri.to_string();
        if(m_uri.is_path_empty() && host_uri[host_uri.length() - 1] != '/' && m_uri.query().empty() && m_uri.fragment().empty())
        {
            host_uri.append(U("/"));
        }
        
        // Remove Url.
        ULONG error_code = HttpRemoveUrlFromUrlGroup(m_url_group, host_uri.c_str(), 0);
        if(error_code)
        {
            return error_code;
        }

        // Stop request queue.
        error_code = HttpShutdownRequestQueue(m_request_queue);
        m_request_task.wait();
        if(error_code)
        {
            return error_code;
        }

        // Release memory for each request.
        std::for_each(m_requests_memory.begin(), m_requests_memory.end(), [](test_request *p_request)
        {
            delete p_request;
        });

        // Close all resources.
        HttpCloseRequestQueue(m_request_queue);
        HttpCloseUrlGroup(m_url_group);
        HttpCloseServerSession(m_session);

        return 0;
    }

    test_request * wait_for_request()
    {
        return wait_for_requests(1)[0];
    }

    pplx::task<test_request *> next_request()
    {
        auto next_request_task = pplx::create_task([this]() -> test_request *
        {
            return wait_for_request();
        });
        m_all_next_request_tasks.push_back(next_request_task);
        return next_request_task;
    }

    std::vector<pplx::task<test_request *>> next_requests(const size_t count)
    {
        std::vector<pplx::task_completion_event<test_request *>> events;
        std::vector<pplx::task<test_request *>> requests;
        for(size_t i = 0; i < count; ++i)
        {
            events.push_back(pplx::task_completion_event<test_request *>());
            auto next_request_task = pplx::create_task(events[i]);
            requests.push_back(next_request_task);
            m_all_next_request_tasks.push_back(next_request_task);
        }
        pplx::create_task([this, count, events]()
        {
            for(size_t i = 0; i < count; ++i)
            {
                events[i].set(wait_for_request());
            }
        });
        return requests;
    }

    std::vector<test_request *> wait_for_requests(const size_t count)
    {
        std::vector<test_request *> m_test_requests;
        for(size_t i = 0; i < count; ++i)
        {
            m_test_requests.push_back(Concurrency::receive(m_requests));
        }
        return m_test_requests;
    }

private:
    friend class test_request;

    ::http::uri m_uri;
    pplx::task<void> m_request_task;
    Concurrency::unbounded_buffer<test_request *> m_requests;

    // Used to store all requests to simplify memory management.
    std::vector<test_request *> m_requests_memory;

    // Used to store all tasks created to wait on requests to catch any test cases
    // which fail to make sure any next_request tasks have completed before exiting.
    // Test cases rarely use more than a couple so it is ok to store them all.
    std::vector<pplx::task<test_request *>> m_all_next_request_tasks;

    HTTP_SERVER_SESSION_ID m_session;
    HTTP_URL_GROUP_ID m_url_group;
    HANDLE m_request_queue;
};
#else
class _test_http_server
{
private:
    const std::string m_uri;
    typename web::http::listener::http_listener m_listener;
    pplx::critical_section m_lock;
    std::vector<pplx::task_completion_event<test_request*>> m_requests;
    std::atomic<unsigned long long> m_last_request_id;

    std::unordered_map<unsigned long long, web::http::http_request> m_responding_requests;

    volatile std::atomic<int> m_cancel;

    pplx::critical_section m_listen_lock;
public:
    _test_http_server(const utility::string_t& uri)
        : m_uri(uri) 
        , m_listener(web::http::listener::http_listener::create(uri))
        , m_cancel(0)
    {
        m_listener.support([&](web::http::http_request result) -> void
        {
            pplx::critical_section::scoped_lock listen_lock(m_listen_lock); // try to serialize requests
            auto tr = new test_request();
            tr->m_method = result.method();
            tr->m_path = result.request_uri().resource().to_string();
            if (tr->m_path == "")
                tr->m_path = "/";

            tr->m_p_server = this;
            tr->m_request_id = ++m_last_request_id;
            for (auto it = result.headers().begin(); it != result.headers().end(); ++it)
                tr->m_headers[it->first] = it->second;
        
            tr->m_body = result.extract_vector().get();

            {
                pplx::critical_section::scoped_lock lock(m_lock);
                m_responding_requests[tr->m_request_id] = result;
            }

            while (!m_cancel)
            {
                pplx::critical_section::scoped_lock lock(m_lock);
                if (m_requests.size() > 0)
                {
                    m_requests[0].set(tr);
                    m_requests.erase(m_requests.begin());
                    return;
                }
            }
        });
    }

    ~_test_http_server()
    {
        close();
    }

    unsigned long open() { return m_listener.open(); }
    unsigned long close()
    {
        ++m_cancel;
        return m_listener.close();
    }

    test_request * wait_for_request()
    {
        return next_request().get();
    }

    unsigned long send_reply(
        unsigned long long request_id,
        const unsigned short status_code, 
        const utility::string_t &reason_phrase, 
        const std::map<utility::string_t, utility::string_t> &headers,
        void * data,
        size_t data_length)
    {
        web::http::http_request request;
        {
            pplx::critical_section::scoped_lock lock(m_lock);
            auto it = m_responding_requests.find(request_id);
            if (it == m_responding_requests.end())
                throw std::runtime_error("no such request awaiting response");
            request = it->second;
            m_responding_requests.erase(it);
        }

        web::http::http_response response;
        response.set_status_code(status_code);
        response.set_reason_phrase(reason_phrase);

        for (auto it = headers.begin(); it != headers.end(); ++it)
            response.headers().add(it->first, it->second);

        unsigned char * data_bytes = reinterpret_cast<unsigned char*>(data);
        std::vector<unsigned char> body_data(data_bytes, data_bytes + data_length);
        response.set_body(std::move(body_data));

        request.reply(response);

        return 0;
    }

    pplx::critical_section m_next_request_lock;
    pplx::task<test_request *> next_request()
    {
        pplx::task_completion_event<test_request*> tce;
        pplx::critical_section::scoped_lock lock(m_lock);
        m_requests.push_back(tce);
        return pplx::create_task(tce);
    }

    std::vector<pplx::task<test_request *>> next_requests(const size_t count)
    {
        std::vector<pplx::task<test_request*>> result;
        for (int i = 0; i < count; ++i)
        {
            result.push_back(next_request());
        }
        return result;
    }

    std::vector<test_request *> wait_for_requests(const size_t count)
    {
        std::vector<test_request*> requests;
        for (int i = 0; i < count; ++i)
        {
            requests.push_back(wait_for_request());
        }
        return requests;
    }
};
#endif

unsigned long test_request::reply(const unsigned short status_code)
{
    return reply(status_code, U(""));
}

unsigned long test_request::reply(const unsigned short status_code, const utility::string_t &reason_phrase)
{
    return reply(status_code, reason_phrase, std::map<utility::string_t, utility::string_t>(), "");
}

unsigned long test_request::reply(
    const unsigned short status_code, 
    const utility::string_t &reason_phrase, 
    const std::map<utility::string_t, utility::string_t> &headers)
{
    return reply(status_code, reason_phrase, headers, U(""));
}

unsigned long test_request::reply(
        const unsigned short status_code, 
        const utility::string_t &reason_phrase,
        const std::map<utility::string_t, utility::string_t> &headers,
        const std::string &data)
{
    return reply_impl(status_code, reason_phrase, headers, (void *)&data[0], data.size() * sizeof(char));
}
#ifdef _MS_WINDOWS
unsigned long test_request::reply(
        const unsigned short status_code, 
        const utility::string_t &reason_phrase, 
        const std::map<utility::string_t, utility::string_t> &headers,
        const utility::string_t &data)
{
    return reply_impl(status_code, reason_phrase, headers, (void *)&data[0], data.size() * sizeof(utility::char_t));
}
#endif

#ifdef _MS_WINDOWS
unsigned long test_request::reply_impl(
        const unsigned short status_code, 
        const utility::string_t &reason_phrase, 
        const std::map<utility::string_t, utility::string_t> &headers,
        void * data,
        size_t data_length)
{
    HTTP_RESPONSE response;
    ZeroMemory(&response, sizeof(HTTP_RESPONSE));
    response.StatusCode = status_code;
    std::string reason(reason_phrase.begin(), reason_phrase.end());
    response.pReason = reason.c_str();
    response.ReasonLength = (USHORT)reason.length();

    // Add headers.
    std::vector<std::string> headers_buffer;
    response.Headers.UnknownHeaderCount = (USHORT)headers.size() + 1;
    response.Headers.pUnknownHeaders = new HTTP_UNKNOWN_HEADER[headers.size() + 1];
    headers_buffer.resize(headers.size() * 2 + 2);

    // Add the no cache header.
    headers_buffer[0] = "Cache-Control";
    headers_buffer[1] = "no-cache";
    response.Headers.pUnknownHeaders[0].NameLength = (USHORT)headers_buffer[0].size();
    response.Headers.pUnknownHeaders[0].pName = headers_buffer[0].c_str();
    response.Headers.pUnknownHeaders[0].RawValueLength = (USHORT)headers_buffer[1].size();
    response.Headers.pUnknownHeaders[0].pRawValue = headers_buffer[1].c_str();

    // Add all other headers.
    if(!headers.empty())
    {
        int headerIndex = 1;
        for(auto iter = headers.begin(); iter != headers.end(); ++iter, ++headerIndex)
        {
            headers_buffer[headerIndex * 2] = utf16_to_utf8(iter->first);
            headers_buffer[headerIndex * 2 + 1] = utf16_to_utf8(iter->second);
            response.Headers.pUnknownHeaders[headerIndex].NameLength = (USHORT)headers_buffer[headerIndex * 2].size();
            response.Headers.pUnknownHeaders[headerIndex].pName = headers_buffer[headerIndex * 2].c_str();
            response.Headers.pUnknownHeaders[headerIndex].RawValueLength = (USHORT)headers_buffer[headerIndex * 2 + 1].size();
            response.Headers.pUnknownHeaders[headerIndex].pRawValue = headers_buffer[headerIndex * 2 + 1].c_str();
        }
    }

    // Add body.
    response.EntityChunkCount = 0;
    HTTP_DATA_CHUNK dataChunk;
    if(data_length != 0)
    {
        response.EntityChunkCount = 1;
        dataChunk.DataChunkType = HttpDataChunkFromMemory;
        dataChunk.FromMemory.pBuffer = (void *)data;
        dataChunk.FromMemory.BufferLength = (ULONG)data_length;
        response.pEntityChunks = &dataChunk;
    }

    // Synchronously sending the request.
    unsigned long error_code = HttpSendHttpResponse(
        m_p_server->m_request_queue,
        m_request_id,
        HTTP_SEND_RESPONSE_FLAG_DISCONNECT,
        &response,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL);

    // Free memory needed for headers.
    if(response.Headers.UnknownHeaderCount != 0)
    {
       delete [] response.Headers.pUnknownHeaders;
    }

    return error_code;
}
#else
unsigned long test_request::reply_impl(
        const unsigned short status_code, 
        const utility::string_t &reason_phrase, 
        const std::map<utility::string_t, utility::string_t> &headers,
        void * data,
        size_t data_length)
{
    return m_p_server->send_reply(m_request_id, status_code, reason_phrase, headers, data, data_length);
}
#endif

test_http_server::test_http_server(const web::http::uri &uri) { m_p_impl = new _test_http_server(uri.to_string()); }

test_http_server::~test_http_server() { delete m_p_impl; }

unsigned long test_http_server::open() { return m_p_impl->open(); }

unsigned long test_http_server::close() { return m_p_impl->close(); }

test_request * test_http_server::wait_for_request() { return m_p_impl->wait_for_request(); }

pplx::task<test_request *> test_http_server::next_request() { return m_p_impl->next_request(); }

std::vector<test_request *> test_http_server::wait_for_requests(const size_t count) { return m_p_impl->wait_for_requests(count); }

std::vector<pplx::task<test_request *>> test_http_server::next_requests(const size_t count) { return m_p_impl->next_requests(count); }

}}}}
