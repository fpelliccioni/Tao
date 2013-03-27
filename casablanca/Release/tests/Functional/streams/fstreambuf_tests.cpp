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
* Basic tests for async file stream buffer operations.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#include "stdafx.h"

#if defined(__cplusplus_winrt)
using namespace Windows::Storage;
#endif

#ifdef _MS_WINDOWS
# define DEFAULT_PROT (int)std::ios_base::_Openprot
#else
# define DEFAULT_PROT 0
#endif

namespace tests { namespace functional { namespace streams {

using namespace utility;
using namespace ::pplx;

// Used to prepare data for read tests

utility::string_t get_full_name(const utility::string_t &name);

void fill_file(const utility::string_t &name, size_t repetitions = 1);
#ifdef _MS_WINDOWS
void fill_file_w(const utility::string_t &name, size_t repetitions = 1);
#endif

//
// The following two functions will help mask the differences between non-WinRT environments and
// WinRT: on the latter, a file path is typically not used to open files. Rather, a UI element is used
// to get a 'StorageFile' reference and you go from there. However, to test the library properly,
// we need to get a StorageFile reference somehow, and one way to do that is to create all the files
// used in testing in the Documents folder.
//
#pragma warning(push)
#pragma warning(disable : 4100)  // Because of '_Prot' in WinRT builds.
template<typename _CharType>
pplx::task<concurrency::streams::streambuf<_CharType>> OPEN(const utility::string_t &name, std::ios::ios_base::openmode mode, int _Prot = DEFAULT_PROT)
{
#if !defined(__cplusplus_winrt)
    return concurrency::streams::file_buffer<_CharType>::open(name, mode, _Prot);
#else
    try
    {
        if ( (mode & std::ios::out) )
        {
            auto file = pplx::create_task(
                KnownFolders::DocumentsLibrary->CreateFileAsync(
                    ref new Platform::String(name.c_str()), CreationCollisionOption::ReplaceExisting)).get();

            return concurrency::streams::file_buffer<_CharType>::open(file, mode);
        }
        else
        {
            auto file = pplx::create_task(
                KnownFolders::DocumentsLibrary->GetFileAsync(ref new Platform::String(name.c_str()))).get();

            return concurrency::streams::file_buffer<_CharType>::open(file, mode);
        }
    }
    catch(Platform::Exception^ exc) 
    { 
        throw utility::details::create_system_error(exc->HResult);
    }
#endif
}

template<typename _CharType>
pplx::task<concurrency::streams::streambuf<_CharType>> OPEN_W(const utility::string_t &name, int _Prot = DEFAULT_PROT)
{
    return OPEN<_CharType>(name, std::ios_base::out, _Prot);
}

template<typename _CharType>
pplx::task<concurrency::streams::streambuf<_CharType>> OPEN_R(const utility::string_t &name, int _Prot = DEFAULT_PROT)
{
    return OPEN<_CharType>(name, std::ios_base::in, _Prot);
}

#pragma warning(pop)

SUITE(file_buffer_tests)
{

TEST(OpenCloseTest1)
{
    // Test using single-byte strings
    auto open = OPEN_W<char>(U("OpenCloseTest1.txt"));

    auto stream = open.get();

    VERIFY_IS_TRUE(open.is_done());
    VERIFY_IS_TRUE(stream.is_open());

    auto close = stream.close();
    auto closed = close.get();

    VERIFY_IS_TRUE(close.is_done());
    VERIFY_IS_TRUE(closed);
    VERIFY_IS_FALSE(stream.is_open());
}

TEST(OpenForReadDoesntCreateFile1, "Ignore:Linux", "TFS#616619")
{
    utility::string_t fname = U("OpenForReadDoesntCreateFile1.txt");

    VERIFY_THROWS_SYSTEM_ERROR(OPEN_R<char>(fname).get(), std::errc::no_such_file_or_directory);

    std::ifstream is;
    VERIFY_IS_NULL(is.rdbuf()->open(fname.c_str(), std::ios::in));
}

TEST(OpenForReadDoesntCreateFile2, "Ignore:Linux", "TFS#616619")
{
    utility::string_t fname = U("OpenForReadDoesntCreateFile2.txt");

    VERIFY_THROWS_SYSTEM_ERROR(OPEN<char>(fname, std::ios_base::in | std::ios_base::binary ).get(), std::errc::no_such_file_or_directory);

    std::ifstream is;
    VERIFY_IS_NULL(is.rdbuf()->open(fname.c_str(), std::ios::in | std::ios_base::binary));
}

TEST(WriteSingleCharTest1)
{ 
    auto open = OPEN_W<char>(U("WriteSingleCharTest1.txt"));
    auto stream = open.get();

    VERIFY_IS_TRUE(open.is_done());
    VERIFY_IS_TRUE(stream.is_open());

    bool elements_equal = true;
    for (uint8_t ch = 'a'; ch <= 'z'; ch++)
    {
        elements_equal = elements_equal && (ch == stream.putc(ch).get());
    }

    VERIFY_IS_TRUE(elements_equal);

    auto close = stream.close();
    auto closed = close.get();

    VERIFY_IS_TRUE(close.is_done());
    VERIFY_IS_TRUE(closed);
    VERIFY_IS_FALSE(stream.is_open());
}
#ifdef _MS_WINDOWS
TEST(WriteSingleCharTest1w)
{ 
    auto open = OPEN_W<wchar_t>(U("WriteSingleCharTest1w.txt"));
    auto stream = open.get();

    VERIFY_IS_TRUE(open.is_done());
    VERIFY_IS_TRUE(stream.is_open());

    bool elements_equal = true;
    for (wchar_t ch = L'a'; ch <= L'z'; ch++)
    {
        elements_equal = elements_equal && (ch == stream.putc(ch).get());
    }

    VERIFY_IS_TRUE(elements_equal);

    auto close = stream.close();
    auto closed = close.get();

    VERIFY_IS_TRUE(close.is_done());
    VERIFY_IS_TRUE(closed);
    VERIFY_IS_FALSE(stream.is_open());
}
#endif

TEST(WriteBufferTest1)
{ 
    auto open = OPEN_W<char>(U("WriteBufferTest1.txt"));
    auto stream = open.get();

    VERIFY_IS_TRUE(open.is_done());
    VERIFY_IS_TRUE(stream.is_open());

    std::vector<char> vect;

    for (uint8_t ch = 'a'; ch <= 'z'; ch++)
    {
        vect.push_back(ch);
    }

    VERIFY_ARE_EQUAL(stream.putn(&vect[0], vect.size()).get(), vect.size());

    auto close = stream.close();
    auto closed = close.get();

    VERIFY_IS_TRUE(close.is_done());
    VERIFY_IS_TRUE(closed);
    VERIFY_IS_FALSE(stream.is_open());
}
#ifdef _MS_WINDOWS
TEST(WriteBufferTest1w)
{ 
    auto open = OPEN_W<wchar_t>(U("WriteBufferTest1w.txt"));
    auto stream = open.get();

    VERIFY_IS_TRUE(open.is_done());
    VERIFY_IS_TRUE(stream.is_open());

    std::vector<wchar_t> vect;

    for (wchar_t ch = L'a'; ch <= L'z'; ch++)
    { 
        vect.push_back(ch);
    }

    VERIFY_ARE_EQUAL(stream.putn(&vect[0], vect.size()).get(), vect.size());

    auto close = stream.close();
    auto closed = close.get();

    VERIFY_IS_TRUE(close.is_done());
    VERIFY_IS_TRUE(closed);
    VERIFY_IS_FALSE(stream.is_open());
}
#endif

TEST(WriteBufferAndSyncTest1, "Ignore", "478760")
{ 
    auto open = OPEN_W<char>(U("WriteBufferAndSyncTest1.txt"));
    auto stream = open.get();

    VERIFY_IS_TRUE(open.is_done());
    VERIFY_IS_TRUE(stream.is_open());

    std::vector<char> vect;

    for (uint8_t ch = 'a'; ch <= 'z'; ch++)
    { 
        vect.push_back(ch);
    }

    auto write = stream.putn(&vect[0], vect.size());

    auto sync = stream.sync().get();

    VERIFY_IS_TRUE(sync);

    VERIFY_IS_TRUE(write.is_done());
    VERIFY_ARE_EQUAL(write.get(), vect.size());

    auto close = stream.close();
    auto closed = close.get();

    VERIFY_IS_TRUE(close.is_done());
    VERIFY_IS_TRUE(closed);
    VERIFY_IS_FALSE(stream.is_open());
}

TEST(ReadSingleChar_bumpc1)
{
    utility::string_t fname = U("ReadSingleChar_bumpc1.txt");
    fill_file(fname);

    auto stream = OPEN_R<char>(fname).get();

    VERIFY_IS_TRUE(stream.is_open());

    uint8_t buf[10];
    memset(buf, 0, sizeof(buf));

    for (int i = 0; i < sizeof(buf); i++)
    {
        buf[i] = (uint8_t)stream.bumpc().get();
        VERIFY_ARE_EQUAL(buf[i], 'a'+i);
    }

    stream.close().get();

    VERIFY_IS_FALSE(stream.is_open());
}

#ifdef _MS_WINDOWS
TEST(ReadSingleChar_bumpcw)
{
    utility::string_t fname = U("ReadSingleChar_bumpcw.txt");
    fill_file_w(fname);

    auto stream = OPEN_R<wchar_t>(fname).get();

    VERIFY_IS_TRUE(stream.is_open());

    wchar_t buf[10];
    memset(buf, 0, sizeof(buf));

    for (int i = 0; i < 10; i++)
    {
        buf[i] = stream.bumpc().get();
        VERIFY_ARE_EQUAL(buf[i], L'a'+i);
    }

    stream.close().get();

    VERIFY_IS_FALSE(stream.is_open());
}
#endif

TEST(ReadSingleChar_bumpc2)
{
    // Test that seeking works.
    utility::string_t fname = U("ReadSingleChar_bumpc2.txt");
    fill_file(fname);

    auto stream = OPEN_R<char>(fname).get();

    VERIFY_IS_TRUE(stream.is_open());

    stream.seekpos(3, std::ios_base::in);

    uint8_t buf[10];
    memset(buf, 0, sizeof(buf));

    for (int i = 0; i < sizeof(buf); i++)
    {
        buf[i] = (uint8_t)stream.bumpc().get();
        VERIFY_ARE_EQUAL(buf[i], 'd'+i);
    }

    stream.close().get();

    VERIFY_IS_FALSE(stream.is_open());
}

TEST(filestream_length)
{
    utility::string_t fname = U("ReadSingleChar_bumpc3.txt");
    fill_file(fname);

    auto stream = OPEN_R<char>(fname, _SH_DENYRW).get();
    stream.set_buffer_size(512);

    VERIFY_IS_TRUE(stream.is_open());

    test_stream_length(stream.create_istream(), 26);

    stream.close().get();

    VERIFY_IS_FALSE(stream.is_open());
}

TEST(ReadSingleChar_bumpc3)
{
    // Test that seeking works.
    utility::string_t fname = U("ReadSingleChar_bumpc3.txt");
    fill_file(fname);

    auto stream = OPEN_R<char>(fname, _SH_DENYRW).get();
    stream.set_buffer_size(512);

    VERIFY_IS_TRUE(stream.is_open());

    stream.seekpos(2, std::ios_base::in);

    // Read a character asynchronously to get the buffer primed.
    stream.bumpc().get();

    auto ras = concurrency::streams::char_traits<char>::requires_async();

    for (int i = 3; i < 26; i++)
    {
        auto c = (uint8_t)stream.sbumpc();
        if ( c != ras )
        {
            VERIFY_ARE_EQUAL(c, 'a'+i);
        }
    }

    stream.close().get();

    VERIFY_IS_FALSE(stream.is_open());
}

TEST(ReadSingleChar_nextc)
{
    utility::string_t fname = U("ReadSingleChar_nextc.txt");
    fill_file(fname);

    auto stream = OPEN_R<char>(fname).get();

    VERIFY_IS_TRUE(stream.is_open());

    uint8_t buf[10];
    memset(buf, 0, sizeof(buf));

    for (int i = 0; i < sizeof(buf); i++)
    {
        buf[i] = (uint8_t)stream.nextc().get();
        VERIFY_ARE_EQUAL(buf[i], 'b'+i);
    }

    stream.close().get();

    VERIFY_IS_FALSE(stream.is_open());
}
#ifdef _MS_WINDOWS
TEST(ReadSingleChar_nextcw)
{
    utility::string_t fname = U("ReadSingleChar_nextcw.txt");
    fill_file_w(fname);

    auto stream = OPEN_R<wchar_t>(fname).get();

    VERIFY_IS_TRUE(stream.is_open());

    wchar_t buf[10];
    memset(buf, 0, sizeof(buf));

    for (int i = 0; i < 10; i++)
    {
        buf[i] = stream.nextc().get();
        VERIFY_ARE_EQUAL(buf[i], L'b'+i);
    }

    stream.close().get();

    VERIFY_IS_FALSE(stream.is_open());
}
#endif

TEST(ReadSingleChar_ungetc)
{
    // Test that seeking works.
    utility::string_t fname = U("ReadSingleChar_ungetc.txt");
    fill_file(fname);

    auto stream = OPEN_R<char>(fname).get();

    VERIFY_IS_TRUE(stream.is_open());

    stream.seekpos(13, std::ios_base::in);

    uint8_t buf[10];
    memset(buf, 0, sizeof(buf));

    for (int i = 0; i < sizeof(buf); i++)
    {
        buf[i] = (uint8_t)stream.ungetc().get();
        VERIFY_ARE_EQUAL(buf[i], 'm'-i);
    }

    stream.close().get();

    VERIFY_IS_FALSE(stream.is_open());
}

TEST(ReadSingleChar_getc1)
{
    utility::string_t fname = U("ReadSingleChar_getc1.txt");
    fill_file(fname);

    auto stream = OPEN_R<char>(fname, _SH_DENYRW).get();

    VERIFY_IS_TRUE(stream.is_open());

    uint8_t ch0 = (uint8_t)stream.getc().get();
    uint8_t ch1 = (uint8_t)stream.getc().get();

    VERIFY_ARE_EQUAL(ch0, ch1);

    stream.close().get();

    VERIFY_IS_FALSE(stream.is_open());
}

TEST(ReadSingleChar_getc2)
{
    utility::string_t fname = U("ReadSingleChar_getc2.txt");
    fill_file(fname);

    auto stream = OPEN_R<char>(fname, _SH_DENYRW).get();

    VERIFY_IS_TRUE(stream.is_open());

    stream.seekpos(13, std::ios_base::in);

    uint8_t ch0 = (uint8_t)stream.getc().get();
    uint8_t ch1 = (uint8_t)stream.sgetc();

    VERIFY_ARE_EQUAL(ch0, ch1);

    stream.close().get();

    VERIFY_IS_FALSE(stream.is_open());
}

#ifdef _MS_WINDOWS
TEST(ReadSingleChar_getc1w)
{
    utility::string_t fname = U("ReadSingleChar_getc1w.txt");
    fill_file_w(fname);

    auto stream = OPEN_R<wchar_t>(fname, _SH_DENYRW).get();

    VERIFY_IS_TRUE(stream.is_open());

    wchar_t ch0 = stream.getc().get();
    wchar_t ch1 = stream.getc().get();

    VERIFY_ARE_EQUAL(ch0, ch1);
    VERIFY_ARE_EQUAL(ch0, L'a');

    stream.seekpos(15, std::ios_base::in);

    ch0 = stream.getc().get();
    ch1 = stream.getc().get();

    VERIFY_ARE_EQUAL(ch0, ch1);
    VERIFY_ARE_EQUAL(ch0, L'p');

    stream.close().get();

    VERIFY_IS_FALSE(stream.is_open());
}

TEST(ReadSingleChar_getc2w)
{
    utility::string_t fname = U("ReadSingleChar_getc2w.txt");
    fill_file_w(fname);

    auto stream = OPEN_R<wchar_t>(fname, _SH_DENYRW).get();

    VERIFY_IS_TRUE(stream.is_open());

    stream.seekpos(13, std::ios_base::in);

    wchar_t ch0 = stream.getc().get();
    wchar_t ch1 = stream.getc().get();

    VERIFY_ARE_EQUAL(ch0, ch1);
    VERIFY_ARE_EQUAL(ch0, L'n');

    stream.seekpos(5, std::ios_base::in);

    ch0 = stream.getc().get();
    ch1 = stream.getc().get();

    VERIFY_ARE_EQUAL(ch0, ch1);
    VERIFY_ARE_EQUAL(ch0, L'f');

    stream.close().get();

    VERIFY_IS_FALSE(stream.is_open());
}
#endif

TEST(ReadBuffer1)
{
    // Test that seeking works.
    utility::string_t fname = U("ReadBuffer1.txt");
    fill_file(fname);

    // In order to get the implementation to buffer reads, we have to open the file
    // with protection against sharing.
    auto stream = OPEN_R<char>(fname, _SH_DENYRW).get();
    stream.set_buffer_size(512);

    VERIFY_IS_TRUE(stream.is_open());

    char buf[10];
    memset(buf, 0, sizeof(buf));

    auto read = 
        stream.getn(buf, sizeof(buf)).then(
        [=](pplx::task<size_t> op) -> size_t
        {
            return op.get();
        });

    VERIFY_ARE_EQUAL(sizeof(buf), read.get());

    bool elements_equal = true;

    for (int i = 0; i < sizeof(buf); i++)
    {
        elements_equal = elements_equal && (buf[i] == 'a'+i);
    }

    VERIFY_IS_TRUE(elements_equal);

    stream.seekpos(3, std::ios_base::in);

    memset(buf, 0, sizeof(buf));

    read = 
        stream.getn(buf, sizeof(buf)).then(
        [=](pplx::task<size_t> op) -> size_t
        {
            return op.get();
        });

    VERIFY_ARE_EQUAL(sizeof(buf), read.get());

    elements_equal = true;

    for (int i = 0; i < sizeof(buf); i++)
    {
        elements_equal = elements_equal && (buf[i] == 'd'+i);
    }

    VERIFY_IS_TRUE(elements_equal);

    stream.close().get();

    VERIFY_IS_FALSE(stream.is_open());
}

#ifdef _MS_WINDOWS
TEST(ReadBuffer1w)
{
    // Test that seeking works.
    utility::string_t fname = U("ReadBuffer1w.txt");
    fill_file_w(fname);

    // In order to get the implementation to buffer reads, we have to open the file
    // with protection against sharing.
    auto stream = OPEN_R<wchar_t>(fname, _SH_DENYRW).get();

    VERIFY_IS_TRUE(stream.is_open());

    wchar_t buf[10];
    memset(buf, 0, sizeof(buf));

    auto read = 
        stream.getn(buf, 10).then(
        [=](pplx::task<size_t> op) -> size_t
        {
            return op.get();
        });

    VERIFY_ARE_EQUAL(10u, read.get());

    bool elements_equal = true;

    for (int i = 0; i < 10; i++)
    {
        elements_equal = elements_equal && (buf[i] == L'a'+i);
    }

    VERIFY_IS_TRUE(elements_equal);

    stream.seekpos(3, std::ios_base::in);

    memset(buf, 0, sizeof(buf));

    read = 
        stream.getn(buf, 10).then(
        [=](pplx::task<size_t> op) -> size_t
    {
        return op.get();
    });

    VERIFY_ARE_EQUAL(10u, read.get());

    elements_equal = true;

    for (int i = 0; i < 10; i++)
    {
        elements_equal = elements_equal && (buf[i] == L'd'+i);
    }

    VERIFY_IS_TRUE(elements_equal);

    stream.close().get();

    VERIFY_IS_FALSE(stream.is_open());
}
#endif

TEST(ReadBuffer2)
{
    // Test that seeking works when the file is larger than the internal buffer size.
    utility::string_t fname = U("ReadBuffer2.txt");
    fill_file(fname,30);

    // In order to get the implementation to buffer reads, we have to open the file
    // with protection against sharing.
    auto stream = OPEN_R<char>(fname, _SH_DENYRW).get();

    VERIFY_IS_TRUE(stream.is_open());

    char buf[10];
    memset(buf, 0, sizeof(buf));

    auto read = 
      stream.getn(buf, sizeof(buf)).then(
        [=](pplx::task<size_t> op) -> size_t
        {
            return op.get();
        });

    VERIFY_ARE_EQUAL(sizeof(buf), read.get());

    bool elements_equal = true;

    for (int i = 0; i < sizeof(buf); i++)
    {
        elements_equal = elements_equal && (buf[i] == 'a'+i);
    }

    VERIFY_IS_TRUE(elements_equal);

    // Test that we can seek to a position near the end of the initial buffer,
    // read a chunk spanning the end of the buffer, and get a correct outcome.

    stream.seekpos(505, std::ios_base::in);

    memset(buf, 0, sizeof(buf));

    read = 
      stream.getn(buf, sizeof(buf)).then(
        [=](pplx::task<size_t> op) -> size_t
        {
            return op.get();
        });

    VERIFY_ARE_EQUAL(sizeof(buf), read.get());

    elements_equal = true;

    for (int i = 0; i < sizeof(buf); i++)
    {
        elements_equal = elements_equal && (buf[i] == 'l'+i);
    }

    VERIFY_IS_TRUE(elements_equal);

    stream.close().get();

    VERIFY_IS_FALSE(stream.is_open());
}

TEST(SeekEnd1)
{
    utility::string_t fname = U("SeekEnd1.txt");
    fill_file(fname,30);

    // In order to get the implementation to buffer reads, we have to open the file
    // with protection against sharing.
    auto stream = OPEN_R<char>(fname).get();

    auto pos = stream.seekoff(0, std::ios_base::end, std::ios_base::in);

    VERIFY_ARE_EQUAL(30*26, pos);
}

#ifndef __cplusplus_winrt // Deisabled for bug TFS 600500
TEST(IsEOFTest)
{
    utility::string_t fname = U("IsEOFTest.txt");
    fill_file(fname,30);

    auto stream = OPEN_R<char>(fname).get();
    VERIFY_IS_FALSE(stream.is_eof());
    stream.getc().wait();
    VERIFY_IS_FALSE(stream.is_eof());
    stream.seekoff(0, std::ios_base::end, std::ios_base::in);
    VERIFY_IS_FALSE(stream.is_eof());
    stream.getc().wait();
    VERIFY_IS_TRUE(stream.is_eof());
    stream.seekoff(0, std::ios_base::beg, std::ios_base::in);
    VERIFY_IS_TRUE(stream.is_eof());
    stream.getc().wait();
    VERIFY_IS_FALSE(stream.is_eof());
}
#endif

TEST(CloseWithException)
{
    struct MyException {};
    auto streambuf = OPEN_W<char>(U("CloseExceptionTest.txt")).get();
    streambuf.close(std::ios::out, std::make_exception_ptr(MyException()));
    VERIFY_THROWS(streambuf.putn("this is good", 10).get(), MyException);
    VERIFY_THROWS(streambuf.putc('c').get(), MyException);

    streambuf = OPEN_R<char>(U("CloseExceptionTest.txt")).get();
    streambuf.close(std::ios::in, std::make_exception_ptr(MyException()));
    char buf[100];
    VERIFY_THROWS(streambuf.getn(buf, 100).get(), MyException);
    VERIFY_THROWS(streambuf.getc().get(), MyException);
}

TEST(inout_regression_test)
{
    std::string data = "abcdefghijklmn";
    concurrency::streams::streambuf<char> file_buf = OPEN<char>(U("inout_regression_test.txt"), std::ios_base::in|std::ios_base::out).get();
    file_buf.putn(&data[0], data.size()).get();

    file_buf.bumpc().get(); // reads 'a'
    
    char readdata[256];
    memset(&readdata[0], '\0', 256);

    file_buf.seekoff(0, std::ios::beg, std::ios::in);
    auto data_read = file_buf.getn(&readdata[0], 3).get(); // reads 'bcd'. File contains the org string though!!! 

    memset(&readdata[0], '\0', 256);
    
    file_buf.seekoff(0, std::ios::beg, std::ios::in);
    data_read = file_buf.getn(&readdata[0], 3).get(); // reads 'efg'. File contains org string 'abcdef..'.

    file_buf.close().wait();
}

} // SUITE(file_buffer_tests)

}}}
