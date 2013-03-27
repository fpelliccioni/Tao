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
* asyncrt_utils.cpp - Utilities
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

#if defined(__cplusplus_winrt)
using namespace Platform;
using namespace Windows::Security::Cryptography;
using namespace Windows::Security::Cryptography::Core;
using namespace Windows::Storage::Streams;
#endif // #if !defined(__cplusplus_winrt)

#ifdef _MS_WINDOWS
#include <WinCrypt.h>
#else
#include <glib.h>
#include <boost/locale.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>
using namespace boost::locale::conv;
#endif

using namespace web; using namespace utility;
using namespace utility::conversions;

namespace utility
{

#pragma region error categories

namespace details
{

const std::error_category & __cdecl platform_category()
{
#ifdef _MS_WINDOWS
    return windows_category();
#else
    return linux_category();
#endif
}

#ifdef _MS_WINDOWS

const std::error_category & __cdecl windows_category()
{
	static details::windows_category_impl instance;
	return instance;
}

std::error_condition windows_category_impl::default_error_condition(int errorCode) const
{
    switch(errorCode)  
    {  
#ifndef __cplusplus_winrt
    case ERROR_WINHTTP_TIMEOUT:
#endif
    case ERROR_TIMEOUT:
        return std::errc::timed_out;
	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
		return std::errc::no_such_file_or_directory;
    default:    
        return std::error_condition(errorCode, *this);  
    }
}

std::string windows_category_impl::message(int errorCode) const
{
    const size_t buffer_size = 4096;
    DWORD dwFlags = FORMAT_MESSAGE_FROM_SYSTEM;
    LPCVOID lpSource = NULL;

#if !defined(__cplusplus_winrt)
    if (errorCode >= 12000)
    {
        dwFlags = FORMAT_MESSAGE_FROM_HMODULE;
        lpSource = GetModuleHandleA("winhttp.dll"); // this handle DOES NOT need to be freed
    }
#endif

    std::wstring buffer;
    buffer.resize(buffer_size);
    unsigned long result;

    result = ::FormatMessageW(
        dwFlags,
        lpSource,
        errorCode,
        0,
        &buffer[0],
        buffer_size,
        NULL);

    if (result == 0)
    {
        std::ostringstream os;
        os << "Unable to get an error message for error code: " << errorCode << ".";
        return os.str();
    }

    return utility::conversions::to_utf8string(buffer);
}

#else

const std::error_category & __cdecl linux_category()
{
    // On Linux we are using boost error codes which have the exact same
    // mapping and are equivalent with std::generic_category error codes.
	return std::generic_category();
}

#endif

}

#pragma endregion

#pragma region conversions

utf16string __cdecl conversions::utf8_to_utf16(const std::string &s)
{
#ifdef _MS_WINDOWS
    // first find the size
    int size = ::MultiByteToWideChar(
        CP_UTF8, // convert to utf-8
        MB_ERR_INVALID_CHARS, // fail if any characters can't be translated
        s.c_str(),
        -1, // signals null terminated input
        nullptr, 0); // must be null for utf8

    if (size == 0)
    {
        throw utility::details::create_system_error(GetLastError());
    }
        
    // this length includes the terminating null
    std::unique_ptr<wchar_t[]> buffer(new wchar_t[size]);

    // now call again to format the string
    int result = ::MultiByteToWideChar(
        CP_UTF8, // convert to utf-8
        MB_ERR_INVALID_CHARS, // fail if any characters can't be translated
        s.c_str(),
        -1, // signals null terminated input
        buffer.get(), size); // must be null for utf8
        
    if (result == size)
    {
        // success!
        return utf16string(buffer.get(), buffer.get() + size - 1);
    }
    else
    {
        throw utility::details::create_system_error(GetLastError());
    }
#else
    return utf_to_utf<utf16char>(s, stop);
#endif
}

std::string __cdecl conversions::utf16_to_utf8(const utf16string &w)
{
#ifdef _MS_WINDOWS
    // first find the size
    int size = ::WideCharToMultiByte(
        CP_UTF8, // convert to utf-8
        WC_ERR_INVALID_CHARS, // fail if any characters can't be translated
        w.c_str(),
        -1, // signals null terminated input
        nullptr, 0, // find the size required
        nullptr, nullptr); // must be null for utf8

    if (size == 0)
    {
        throw utility::details::create_system_error(GetLastError());
    }
        
    // this length includes the terminating null
    std::unique_ptr<char[]> buffer(new char[size]);

    // now call again to format the string
    int result = ::WideCharToMultiByte(
        CP_UTF8, // convert to utf-8
        WC_ERR_INVALID_CHARS, // fail if any characters can't be translated
        w.c_str(),
        -1, // signals null terminated input
        buffer.get(), size,
        nullptr, nullptr); // must be null for utf8
        
    if (result == size)
    {
        // success!
        return std::string(buffer.get(), buffer.get() + size - 1);
    }
    else
    {
        throw utility::details::create_system_error(GetLastError());
    }
#else
    return utf_to_utf<char>(w, stop);
#endif
}

utf16string __cdecl conversions::usascii_to_utf16(const std::string &s)
{
#ifdef _MS_WINDOWS
    int size = ::MultiByteToWideChar(
        20127, // convert from us-ascii
        MB_ERR_INVALID_CHARS, // fail if any characters can't be translated
        s.c_str(),
        -1, // signals null terminated input
        nullptr, 0); // must be null for utf8

    if (size == 0)
    {
        throw utility::details::create_system_error(GetLastError());
    }

    // this length includes the terminating null
    std::unique_ptr<wchar_t[]> buffer(new wchar_t[size]);

    // now call again to format the string
    int result = ::MultiByteToWideChar(
        20127, // convert from us-ascii
        MB_ERR_INVALID_CHARS, // fail if any characters can't be translated
        s.c_str(),
        -1, // signals null terminated input
        buffer.get(), size); // must be null for utf8

    if (result == size)
    {
        // success!
        return utf16string(buffer.get(), buffer.get() + size - 1);
    }
    else
    {
        throw utility::details::create_system_error(GetLastError());
    }
#else
    return utf_to_utf<utf16char>(to_utf<char>(s, "ascii", stop));
#endif
}

utf16string __cdecl conversions::latin1_to_utf16(const std::string &s)
{
#ifdef _MS_WINDOWS
    int size = ::MultiByteToWideChar(
        28591, // convert from Latin1
        MB_ERR_INVALID_CHARS, // fail if any characters can't be translated
        s.c_str(),
        -1, // signals null terminated input
        nullptr, 0); // must be null for utf8

    if (size == 0)
    {
        throw utility::details::create_system_error(GetLastError());
    }

    // this length includes the terminating null
    std::unique_ptr<wchar_t[]> buffer(new wchar_t[size]);

    // now call again to format the string
    int result = ::MultiByteToWideChar(
        28591, // convert from Latin1
        MB_ERR_INVALID_CHARS, // fail if any characters can't be translated
        s.c_str(),
        -1, // signals null terminated input
        buffer.get(), size); // must be null for utf8

    if (result == size)
    {
        // success!
        return std::wstring(buffer.get(), buffer.get() + size - 1);
    }
    else
    {
        throw utility::details::create_system_error(GetLastError());
    }
#else
    return utf_to_utf<utf16char>(to_utf<char>(s, "Latin1", stop));
#endif
}

utf16string __cdecl conversions::default_code_page_to_utf16(const std::string &s)
{
#ifdef _MS_WINDOWS
    // First have to convert to UTF-16.
    int size = ::MultiByteToWideChar(
        CP_ACP, // convert from Windows system default
        MB_ERR_INVALID_CHARS, // fail if any characters can't be translated
        s.c_str(),
        -1, // signals null terminated input
        nullptr, 0); // must be null for utf8

    if (size == 0)
    {
        throw utility::details::create_system_error(GetLastError());
    }

    // this length includes the terminating null
    std::unique_ptr<wchar_t[]> buffer(new wchar_t[size]);

    // now call again to format the string
    int result = ::MultiByteToWideChar(
        CP_ACP, // convert from Windows system default
        MB_ERR_INVALID_CHARS, // fail if any characters can't be translated
        s.c_str(),
        -1, // signals null terminated input
        buffer.get(), size); // must be null for utf8
    if(result == size)
    {
        return std::wstring(buffer.get(), buffer.get() + size - 1);
    }
    else
    {
        throw utility::details::create_system_error(GetLastError());
    }
#else // LINUX
    return utf_to_utf<utf16char>(to_utf<char>(s, std::locale(""), stop));
#endif
}

utility::string_t conversions::to_string_t(const std::string &s)
{
#ifdef _UTF16_STRINGS
    return utf8_to_utf16(s);
#else
    return s;
#endif
}

utility::string_t conversions::to_string_t(const utf16string &s)
{
#ifdef _UTF16_STRINGS
    return s;
#else
    return conversions::utf16_to_utf8(s);
#endif
}

std::vector<unsigned char> __cdecl conversions::from_base64(const utility::string_t& str)
{
#if defined(__cplusplus_winrt)

    Platform::String^ Str = ref new Platform::String(str.c_str());
    IBuffer^ buffer = CryptographicBuffer::DecodeFromBase64String(Str);

    std::vector<unsigned char> decodedData(static_cast<size_t>(buffer->Length), 0);
    Platform::Array<unsigned char, 1>^ arr;

    CryptographicBuffer::CopyToByteArray(buffer, &arr);
    memcpy(&decodedData[0], arr->Data, arr->Length);
    return decodedData;

#elif defined(WIN32)
    DWORD outputSize = 0;
    CryptStringToBinaryW(str.c_str(), (DWORD)str.size(), CRYPT_STRING_BASE64, NULL, &outputSize, NULL, NULL);

    std::vector<unsigned char> decodedData(static_cast<size_t>(outputSize));
    CryptStringToBinaryW(str.c_str(), (DWORD)str.size(), CRYPT_STRING_BASE64, decodedData.data(), &outputSize, NULL, NULL);
    return decodedData;
#else // linux
    gsize len;
    auto data = g_base64_decode(str.data(), &len);
    std::vector<unsigned char> result;
    for (gsize i = 0; i < len; ++i)
        result.push_back(data[i]);
    g_free(data);
    return result;
#endif
}

std::string conversions::to_utf8string(const std::string &value) { return value; }

std::string conversions::to_utf8string(const utf16string &value) { return utf16_to_utf8(value); }

utf16string conversions::to_utf16string(const std::string &value) { return utf8_to_utf16(value); }

utf16string conversions::to_utf16string(const utf16string &value) { return value; }


utility::string_t __cdecl conversions::to_base64(const std::vector<unsigned char>& input)
{
    if (input.size() == 0)
    {
        // return empty string
        return utility::string_t();
    }

#if defined(__cplusplus_winrt)
    Array<unsigned char, 1>^ arr = ref new Array<unsigned char, 1>((unsigned char*)input.data(), (unsigned int)input.size());
    IBuffer^ buffer = CryptographicBuffer::CreateFromByteArray(arr);

    Platform::String^ Str = CryptographicBuffer::EncodeToBase64String(buffer);
    return utility::string_t(Str->Data());
#elif defined(WIN32)
    DWORD inputSize = static_cast<DWORD>(input.size() & 0xFFFFFFFF);
    DWORD outputSize = 0;
    CryptBinaryToStringW((BYTE*) input.data(), inputSize, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &outputSize);

    utility::string_t encodedString(outputSize - 1, '\0');
    CryptBinaryToStringW((BYTE*) input.data(), inputSize, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &encodedString[0], &outputSize);

    return encodedString;
#else // linux
    auto data = g_base64_encode(input.data(), input.size());
    std::string result(data);
    g_free(data);
    return result;
#endif
}

utility::string_t __cdecl conversions::to_base64(uint64_t input)
{
#if defined(__cplusplus_winrt)
    Array<unsigned char, 1>^ arr = ref new Array<unsigned char, 1>((unsigned char*)&input, (unsigned int)sizeof(input));
    IBuffer^ buffer = CryptographicBuffer::CreateFromByteArray(arr);

    Platform::String^ Str = CryptographicBuffer::EncodeToBase64String(buffer);
    return utility::string_t(Str->Data());
#elif defined(WIN32)
    DWORD inputSize = static_cast<DWORD>(sizeof(input));
    DWORD outputSize = 0;
    CryptBinaryToStringW((BYTE*) &input, inputSize, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &outputSize);

    utility::string_t encodedString(outputSize - 1, '\0');
    CryptBinaryToStringW((BYTE*) &input, inputSize, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &encodedString[0], &outputSize);

    return encodedString;
#else // linux
    auto data = g_base64_encode(reinterpret_cast<const guchar*>(&input), sizeof(input));
    std::string result(data);
    g_free(data);
    return result;
#endif
}
#pragma endregion

#pragma region datetime

#ifndef WIN32
datetime datetime::timeval_to_datetime(struct timeval time)
{
    const uint64_t epoch_offset = 11644473600LL; // diff between windows and unix epochs (seconds)
    uint64_t result = epoch_offset + time.tv_sec;
    result *= 10000000LL; // convert to 10e-7
    result += time.tv_usec*10; //add microseconds (in 10e-7)
    return datetime(result);
}
#endif

/// <summary>
/// Returns the current UTC date and time.
/// </summary>
datetime __cdecl datetime::utc_now()
{
#ifdef _MS_WINDOWS
    ULARGE_INTEGER largeInt;
    FILETIME fileTime;
    GetSystemTimeAsFileTime(&fileTime);

    largeInt.LowPart = fileTime.dwLowDateTime;
    largeInt.HighPart = fileTime.dwHighDateTime;

    return datetime(largeInt.QuadPart);    
#else //LINUX
    struct timeval time;
    gettimeofday(&time, nullptr);
    return timeval_to_datetime(time);
#endif
}

/// <summary>
/// Returns a string representation of the datetime. The string is formatted based on RFC 1123 or ISO 8601
/// </summary>
utility::string_t datetime::to_string(date_format format) const
{
#ifdef _MS_WINDOWS
    int status;

    ULARGE_INTEGER largeInt;
    largeInt.QuadPart = m_interval;

    FILETIME ft;
    ft.dwHighDateTime = largeInt.HighPart;
    ft.dwLowDateTime = largeInt.LowPart;

    SYSTEMTIME systemTime;
    if (!FileTimeToSystemTime((const FILETIME *)&ft, &systemTime))
    {
        throw utility::details::create_system_error(GetLastError());
    }

    std::wostringstream outStream;

    if ( format == RFC_1123 )
    {
        wchar_t dateStr[18] = {0};
        status = GetDateFormatEx(LOCALE_NAME_INVARIANT, 0, &systemTime, L"ddd',' dd MMM yyyy", dateStr, sizeof(dateStr) / sizeof(wchar_t), NULL);
        if (status == 0)
        {
            throw utility::details::create_system_error(GetLastError());
        }

        wchar_t timeStr[10] = {0};
        status = GetTimeFormatEx(LOCALE_NAME_INVARIANT, TIME_NOTIMEMARKER | TIME_FORCE24HOURFORMAT, &systemTime, L"HH':'mm':'ss", timeStr, sizeof(timeStr) / sizeof(wchar_t));
        if (status == 0)
        {
            throw utility::details::create_system_error(GetLastError());
        }

        outStream << dateStr << " " << timeStr << " " << "GMT";
    }
    else if ( format == ISO_8601 )
    {
        wchar_t dateStr[18] = {0};
        status = GetDateFormatEx(LOCALE_NAME_INVARIANT, 0, &systemTime, L"yyyy-MM-dd", dateStr, sizeof(dateStr) / sizeof(wchar_t), NULL);
        if (status == 0)
        {
            throw utility::details::create_system_error(GetLastError());
        }

        wchar_t timeStr[10] = {0};
        status = GetTimeFormatEx(LOCALE_NAME_INVARIANT, TIME_NOTIMEMARKER | TIME_FORCE24HOURFORMAT, &systemTime, L"HH':'mm':'ss", timeStr, sizeof(timeStr) / sizeof(wchar_t));
        if (status == 0)
        {
            throw utility::details::create_system_error(GetLastError());
        }

        outStream << dateStr << "T" << timeStr << "Z";
    }

    return outStream.str();
#else //LINUX
    using namespace boost::posix_time;

    uint64_t input = m_interval; 
    input /= 10000000LL; // convert to seconds
    time_t time = (time_t)input - (time_t)11644473600LL;// diff between windows and unix epochs (seconds)

    struct tm datetime;
    gmtime_r(&time, &datetime);

    const int max_dt_length = 29;
    char output[max_dt_length+1]; output[max_dt_length] = '\0';
    strftime(output, max_dt_length+1, 
        format == RFC_1123 ? "%a, %d %b %Y %H:%M:%S GMT" : "%Y-%m-%dT%H:%M:%SZ",
        &datetime);

    return std::string(output);
#endif
}

/// <summary>
/// Returns a string representation of the datetime. The string is formatted based on RFC 1123 or ISO 8601
/// </summary>
datetime __cdecl datetime::from_string(const utility::string_t& dateString, date_format format)
{
#ifdef _MS_WINDOWS
    SYSTEMTIME sysTime = {0};
    
    if ( format == RFC_1123 )
    {
        std::wstring month(3, L'\0');
        std::wstring unused(3, L'\0');

        const wchar_t * formatString = L"%3c, %2d %3c %4d %2d:%2d:%2d %3c";
        auto n = swscanf_s(dateString.c_str(), formatString, 
            unused.data(), unused.size(), 
            &sysTime.wDay, 
            month.data(), month.size(), 
            &sysTime.wYear,  
            &sysTime.wHour, 
            &sysTime.wMinute, 
            &sysTime.wSecond, 
            unused.data(), unused.size());
    
        if (n == 8)
        {
            std::wstring monthnames[12] = {L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun", L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec"};
            auto loc = std::find_if(monthnames, monthnames+12, [&month](const std::wstring& m) { return m == month;});

            if (loc != monthnames+12)
            {
                FILETIME fileTime;
                sysTime.wMonth = (short) ((loc - monthnames) + 1);

                if (SystemTimeToFileTime(&sysTime, &fileTime))
                {
                    ULARGE_INTEGER largeInt;
                    largeInt.LowPart = fileTime.dwLowDateTime;
                    largeInt.HighPart = fileTime.dwHighDateTime;

                    return datetime(largeInt.QuadPart);  
                }
            }
        }
    }
    else if ( format == ISO_8601 )
    {
        const wchar_t * formatString = L"%4d-%2d-%2dT%2d:%2d:%2dZ";
        auto n = swscanf_s(dateString.c_str(), formatString, 
            &sysTime.wYear,  
            &sysTime.wMonth,  
            &sysTime.wDay, 
            &sysTime.wHour, 
            &sysTime.wMinute, 
            &sysTime.wSecond);

        if (n == 6)
        {
            FILETIME fileTime;

            if (SystemTimeToFileTime(&sysTime, &fileTime))
            {
                ULARGE_INTEGER largeInt;
                largeInt.LowPart = fileTime.dwLowDateTime;
                largeInt.HighPart = fileTime.dwHighDateTime;

                return datetime(largeInt.QuadPart);  
            }
        }
    }

    return datetime();
#else
    std::string input(dateString);

    struct tm output = tm();
    strptime(input.data(), format == RFC_1123 
        ? "%a, %d %b %Y %H:%M:%S GMT"
        : "%Y-%m-%dT%H:%M:%SZ", &output);

    time_t time = timegm(&output);

    struct timeval tv = timeval();
    tv.tv_sec = time;
    return timeval_to_datetime(tv);
#endif
}
#pragma endregion

#pragma region "timespan"

/// <summary>
/// Converts a timespan/interval in seconds to xml duration string as specified by
/// http://www.w3.org/TR/xmlschema-2/#duration
/// </summary>
utility::string_t __cdecl timespan::seconds_to_xml_duration(utility::seconds durationSecs)
{
    auto numSecs = durationSecs.count();

    // Find the number of minutes
    auto numMins =  numSecs / 60;
    if (numMins > 0)
    {
        numSecs = numSecs % 60;
    }

    // Hours
    auto numHours = numMins / 60;
    if (numHours > 0)
    {
        numMins = numMins % 60;
    }

    // Days
    auto numDays = numHours / 24;
    if (numDays > 0)
    {
        numHours = numHours % 24;
    }

    // The format is:
    // PdaysDThoursHminutesMsecondsS
    utility::ostringstream_t oss;

    oss << U("P");
    if (numDays > 0)
    {
        oss << numDays << U("D");
    }
    
    oss << U("T");

    if (numHours > 0)
    {
        oss << numHours << U("H");
    }

    if (numMins > 0)
    {
        oss << numMins << U("M");
    }

    if (numSecs > 0)
    {
        oss << numSecs << U("S");
    }

    return oss.str();
}

static bool is_digit(utility::char_t c) { return c >= U('0') && c <= U('9'); }

/// <summary>
/// Converts an xml duration to timespan/interval in seconds
/// http://www.w3.org/TR/xmlschema-2/#duration
/// </summary>
utility::seconds __cdecl timespan::xml_duration_to_seconds(utility::string_t timespanString)
{
    // The format is:
    // PnDTnHnMnS
    // if n == 0 then the field could be omitted
    // The final S could be omitted

    int64_t numSecs = 0;

    utility::istringstream_t is(timespanString);
    auto eof = std::char_traits<utility::char_t>::eof();

    std::basic_istream<utility::char_t>::int_type c;
    c = is.get(); // P

    while (c != eof)
    {
        int val = 0;
        c = is.get();

        while (is_digit((utility::char_t)c))
        {
            val = val * 10 + (c - L'0');
            c = is.get();

            if (c == '.')
            {
                // decimal point is not handled
                do { c = is.get(); } while(is_digit((utility::char_t)c));
            }
        }

        if (c == L'D') numSecs += val * 24 * 3600; // days
        if (c == L'H') numSecs += val * 3600; // Hours
        if (c == L'M') numSecs += val * 60; // Minutes
        if (c == L'S' || c == eof)
        {
            numSecs += val; // seconds
            break;
        }
    }

    return utility::seconds(numSecs);
}

#pragma endregion

}
