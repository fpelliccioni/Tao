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
* interopstream.h
*
* Adapter classes for async and STD stream buffers, used to connect std-based and async-based APIs.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#pragma once

#include "pplxtasks.h"
#include "astreambuf.h"
#include "streams.h"

#ifdef _MS_WINDOWS
#include <safeint.h>
#endif

#ifndef _CONCRT_H
#ifndef _LWRCASE_CNCRRNCY
#define _LWRCASE_CNCRRNCY
// Note to reader: we're using lower-case namespace names everywhere, but the 'Concurrency' namespace
// is capitalized for historical reasons. The alias let's us pretend that style issue doesn't exist.
namespace Concurrency { }
namespace concurrency = Concurrency;
#endif
#endif

#pragma warning(push)
// Suppress unreferenced formal parameter warning as they are required for documentation
#pragma warning(disable : 4100)
// Inherited via dominance. Disabling is necessary for iostream, and exactly what STL does.
#pragma warning(disable: 4250)  

namespace Concurrency { namespace streams {

    template<typename CharType> class stdio_ostream;
    template<typename CharType> class stdio_istream;

#pragma region Asynchronous streams on top of synchronous stream buffers.
    namespace details {

    /// <summary>
    /// The basic_stdio_buffer class serves to support interoperability with STL stream buffers.
    /// Sitting atop a std::streambuf, which does all the I/O, instances of this class may read
    /// and write data to standard iostreams. The class itself should not be used in application
    /// code, it is used by the stream definitions farther down in the header file.
    /// </summary>
    template<typename _CharType>
    class basic_stdio_buffer : public streambuf_state_manager<_CharType>
    {
        typedef std::char_traits<_CharType> traits;
        typedef typename traits::int_type int_type;
        typedef typename traits::pos_type pos_type;
        typedef typename traits::off_type off_type;
        /// <summary>
        /// Private constructor
        /// </summary>
        basic_stdio_buffer(_In_ std::basic_streambuf<_CharType>* streambuf, std::ios_base::openmode mode) 
            : m_buffer(streambuf), streambuf_state_manager<_CharType>(mode)
        { 
        }

    public:
        /// <summary>
        /// Destructor
        /// </summary>
        virtual ~basic_stdio_buffer()
        { 
            this->close();
        }

    private:
        //
        // The functions overridden below here are documented elsewhere.
        // See astreambuf.h for further information.
        //     
        virtual bool can_seek() const { return this->is_open(); }

        virtual size_t in_avail() const { return (size_t)m_buffer->in_avail(); }

        virtual size_t buffer_size(std::ios_base::openmode direction = std::ios_base::in) const { return 0; }
        virtual void set_buffer_size(size_t size, std::ios_base::openmode direction = std::ios_base::in) { return; }

        virtual pplx::task<bool> _sync() { return pplx::task_from_result(m_buffer->pubsync() != std::char_traits<_CharType>::eof()); }

        virtual pplx::task<int_type> _putc(_CharType ch) { return pplx::task_from_result(m_buffer->sputc(ch)); }
        virtual pplx::task<size_t> _putn(const _CharType *ptr, size_t size) { return pplx::task_from_result((size_t)m_buffer->sputn(ptr, size)); }

        size_t _sgetn(_Out_writes_ (size) _CharType *ptr, _In_ size_t size) { return m_buffer->sgetn(ptr, size); }
        virtual size_t _scopy(_Out_writes_ (size) _CharType *, _In_ size_t) { return (size_t)-1; }

        virtual pplx::task<size_t> _getn(_Out_writes_ (size) _CharType *ptr, _In_ size_t size) { return pplx::task_from_result((size_t)m_buffer->sgetn(ptr, size)); }

        virtual int_type _sbumpc() { return m_buffer->sbumpc(); }
        virtual int_type _sgetc() { return m_buffer->sgetc(); }

        virtual pplx::task<int_type> _bumpc() { return pplx::task_from_result<int_type>(m_buffer->sbumpc()); }
        virtual pplx::task<int_type> _getc() { return pplx::task_from_result<int_type>(m_buffer->sgetc()); }
        virtual pplx::task<int_type> _nextc() { return pplx::task_from_result<int_type>(m_buffer->snextc()); }
        virtual pplx::task<int_type> _ungetc() { return pplx::task_from_result<int_type>(m_buffer->sungetc()); }

        virtual pos_type seekpos(pos_type pos, std::ios_base::openmode mode) { return m_buffer->pubseekpos(pos, mode); }
        virtual pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode mode) { return m_buffer->pubseekoff(off, dir, mode); }

        virtual _CharType* alloc(size_t count) { return nullptr; }
        virtual void commit(size_t count) {}

        virtual bool acquire(_Out_writes_ (size) _CharType*& ptr, _In_ size_t& count) { return false; }
        virtual void release(_Out_writes_ (size) _CharType *ptr, _In_ size_t count) { }

        template<typename CharType> friend class concurrency::streams::stdio_ostream;
        template<typename CharType> friend class concurrency::streams::stdio_istream;

        std::basic_streambuf<_CharType>* m_buffer;
    };

    } // namespace details

    /// <summary>
    /// stdio_ostream represents an async ostream derived from a standard synchronous stream, as
    /// defined by the "std" namespace. It is constructed from a reference to a standard stream, which
    /// must be valid for the lifetime of the asynchronous stream.
    /// </summary>
    /// <remarks>
    /// Since std streams are not reference-counted, great care must be taken by an application to make
    /// sure that the std stream does not get destroyed until all uses of the asynchronous stream are
    /// done and have been serviced.
    /// </remarks>
    template<typename CharType>
    class stdio_ostream : public basic_ostream<CharType>
    {
    public:
        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="stream">The synchronous stream that this is using for its I/O</param>
        template <typename AlterCharType>
        stdio_ostream(std::basic_ostream<AlterCharType>& stream)
            : basic_ostream<CharType>(streams::streambuf<AlterCharType>(std::shared_ptr<details::basic_stdio_buffer<AlterCharType>>(new details::basic_stdio_buffer<AlterCharType>(stream.rdbuf(), std::ios_base::out))))
        {
        }

        /// <summary>
        /// Copy constructor
        /// </summary>
        /// <param name="other">The source object</param>
        stdio_ostream(const stdio_ostream &other) : basic_ostream<CharType>(other) { }

        /// <summary>
        /// Assignment operator
        /// </summary>
        /// <param name="other">The source object</param>
        stdio_ostream & operator =(const stdio_ostream &other) { basic_ostream<CharType>::operator=(other); return *this; }
    };

    /// <summary>
    /// stdio_istream represents an async istream derived from a standard synchronous stream, as
    /// defined by the "std" namespace. It is constructed from a reference to a standard stream, which
    /// must be valid for the lifetime of the asynchronous stream.
    /// </summary>
    /// <remarks>
    /// Since std streams are not reference-counted, great care must be taken by an application to make
    /// sure that the std stream does not get destroyed until all uses of the asynchronous stream are
    /// done and have been serviced.
    /// </remarks>
    template<typename CharType>
    class stdio_istream : public basic_istream<CharType>
    {
    public:
        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="stream">The synchronous stream that this is using for its I/O</param>
        template <typename AlterCharType>
        stdio_istream(std::basic_istream<AlterCharType>& stream)
            : basic_istream<CharType>(streams::streambuf<AlterCharType>(std::shared_ptr<details::basic_stdio_buffer<AlterCharType>>(new details::basic_stdio_buffer<AlterCharType>(stream.rdbuf(), std::ios_base::in))))
        {
        }

        /// <summary>
        /// Copy constructor
        /// </summary>
        /// <param name="other">The source object</param>
        stdio_istream(const stdio_istream &other) : basic_istream<CharType>(other) { }

        /// <summary>
        /// Assignment operator
        /// </summary>
        /// <param name="other">The source object</param>
        stdio_istream & operator =(const stdio_istream &other) { basic_istream<CharType>::operator=(other); return *this; }
    };

#pragma endregion

#pragma region Synchronous streams on top of asynchronous stream buffers.

    namespace details {

    /// <summary>
    /// IO streams stream buffer implementation used to interface with an async streambuffer underneath.
    /// Used for implementing the standard synchronous streams that provide interop between std:: and concurrency::streams::
    /// </summary>
    template<typename CharType>
    class basic_async_streambuf : public std::basic_streambuf<CharType>
    {
    public:
        typedef std::char_traits<CharType> traits;
        typedef typename traits::int_type int_type;
        typedef typename traits::pos_type pos_type;
        typedef typename traits::off_type off_type;

        basic_async_streambuf(streams::streambuf<CharType> async_buf) : m_buffer(async_buf) 
        {
        }
    protected:

        //
        // The following are the functions in std::basic_streambuf that we need to override.
        //

        /// <summary>
        /// Write one byte to the stream buffer.
        /// </summary>
        int_type overflow(int_type ch)
        {
            return m_buffer.putc(CharType(ch)).get(); 
        }

        /// <summary>
        /// Get one byte from the stream buffer without moving the read position.
        /// </summary>
        int_type underflow() 
        {
            return m_buffer.getc().get(); 
        }

        /// <summary>
        /// Get one byte from the stream buffer and move the read position one character.
        /// </summary>
        int_type uflow() 
        {
            return m_buffer.bumpc().get(); 
        }

        /// <summary>
        /// Get a number of characters from the buffer and place it into the provided memory block.
        /// </summary>
        std::streamsize xsgetn(_Out_writes_ (count) CharType* ptr, _In_ std::streamsize count) 
        {
			size_t cnt = size_t(count);
			size_t read_so_far = 0;

			while (read_so_far < cnt)
			{
				size_t rd = m_buffer.getn(ptr+read_so_far, cnt-read_so_far).get();
				read_so_far += rd;
				if ( rd == 0 )
					break;
			}
            return read_so_far; 
        }

        /// <summary>
        /// Write a given number of characters from the provided block into the stream buffer.
        /// </summary>
        std::streamsize xsputn(const CharType* ptr, std::streamsize count)
        {
            return m_buffer.putn(ptr, (size_t)count).get(); 
        }

        /// <summary>
        /// Synchronize with the underlying medium.
        /// </summary>
        int sync()
        {
            return m_buffer.sync().get(); 
        }

        /// <summary>
        /// Seek to the given offset relative to the beginning, end, or current position.
        /// </summary>
        pos_type seekoff(off_type offset, 
                         std::ios_base::seekdir dir, 
                         std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out)
        {
            return m_buffer.seekoff(offset,dir,mode);
        }

        /// <summary>
        /// Seek to the given offset relative to the beginning of the stream.
        /// </summary>
        pos_type seekpos(pos_type pos, 
                         std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out)
        {
            return m_buffer.seekpos(pos, mode);
        }

    private:
        concurrency::streams::streambuf<CharType> m_buffer;
    };

    } // namespace details

    /// <summary>
    /// A concrete STL ostream which relies on an asynchronous stream for its I/O.
    /// </summary>
    template<typename CharType>
    class async_ostream : public std::basic_ostream<CharType>
    {
    public:
        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="astream">The asynchronous stream whose stream buffer should be used for I/O</param>
        template <typename AlterCharType>
        async_ostream(streams::basic_ostream<AlterCharType> astream) 
            : std::basic_ostream<CharType>(&m_strbuf),
              m_strbuf(astream.streambuf())
        {
        }

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="strbuf">The asynchronous stream buffer to use for I/O</param>
        template <typename AlterCharType>
        async_ostream(streams::streambuf<AlterCharType> strbuf) 
            : std::basic_ostream<CharType>(&m_strbuf),
              m_strbuf(strbuf)
        {
        }

    private:
        details::basic_async_streambuf<CharType> m_strbuf;
    };

    /// <summary>
    /// A concrete STL istream which relies on an asynchronous stream for its I/O.
    /// </summary>
    template<typename CharType>
    class async_istream : public std::basic_istream<CharType>
    {
    public:
        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="astream">The asynchronous stream whose stream buffer should be used for I/O</param>
        template <typename AlterCharType>
        async_istream(streams::basic_istream<AlterCharType> astream) 
            : std::basic_istream<CharType>(&m_strbuf),
              m_strbuf(astream.streambuf())
        {
        }

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="strbuf">The asynchronous stream buffer to use for I/O</param>
        template <typename AlterCharType>
        async_istream(streams::streambuf<AlterCharType> strbuf) 
            : std::basic_istream<CharType>(&m_strbuf),
              m_strbuf(strbuf)
        {
        }

    private:
        details::basic_async_streambuf<CharType> m_strbuf;
    };

    /// <summary>
    /// A concrete STL istream which relies on an asynchronous stream buffer for its I/O.
    /// </summary>
    template<typename CharType>
    class async_iostream : public std::basic_iostream<CharType>
    {
    public:
        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="strbuf">The asynchronous stream buffer to use for I/O</param>
        async_iostream(streams::streambuf<CharType> strbuf) 
            : std::basic_iostream<CharType>(&m_strbuf),
              m_strbuf(strbuf)
        {
        }

    private:
        details::basic_async_streambuf<CharType> m_strbuf;
    };
#pragma endregion

}} // namespaces
#pragma warning(pop) // 4100
