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
* producerconsumerstream.h
*
* This file defines a basic memory-based stream buffer, which allows consumer / producer pairs to communicate
* data via a buffer.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#pragma once

#ifndef _CASA_PRODUCER_CONSUMER_STREAMS_H
#define _CASA_PRODUCER_CONSUMER_STREAMS_H

#include <vector>
#include <queue>
#include <algorithm>
#include <iterator>
#include "pplxtasks.h"
#include "astreambuf.h"
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

// Suppress unreferenced formal parameter warning as they are required for documentation
#pragma warning(push)
#pragma warning(disable : 4100)

namespace Concurrency { namespace streams {

    namespace details {

        /// <summary>
        /// The basic_producer_consumer_buffer class serves as a memory-based steam buffer that supports both writing and reading
        /// sequences of characters. It can be used as a consumer/producer buffer.
        /// </summary>
        template<typename _CharType>
        class basic_producer_consumer_buffer : public streams::details::streambuf_state_manager<_CharType>
        {
        public:
            typedef typename ::concurrency::streams::char_traits<_CharType> traits;
            typedef typename basic_streambuf<_CharType>::int_type int_type;
            typedef typename basic_streambuf<_CharType>::pos_type pos_type;
            typedef typename basic_streambuf<_CharType>::off_type off_type;

            /// <summary>
            /// Constructor
            /// </summary>
            basic_producer_consumer_buffer(size_t alloc_size) 
                : streambuf_state_manager<_CharType>(std::ios_base::out | std::ios_base::in),
                m_alloc_size(alloc_size),
                m_allocBlock(nullptr),
                m_total(0), m_total_read(0), m_total_written(0),
                m_synced(0)
            { 
            }

            /// <summary>
            /// Destructor
            /// </summary>
            virtual ~basic_producer_consumer_buffer()
            { 
                // Invoke the synchronous versions since we need to
                // purge the request queue before deleting the buffer
                this->close().wait();

                _PPLX_ASSERT(m_requests.empty());
                m_blocks.clear();
            }

            /// <summary>
            /// can_seek is used to determine whether a stream buffer supports seeking.
            /// </summary>
            virtual bool can_seek() const { return false; }

            /// <summary>
            /// Get the stream buffer size, if one has been set.
            /// </summary>
            /// <param name="direction">The direction of buffering (in or out)</param>
            /// <remarks>An implementation that does not support buffering will always return '0'.</remarks>
            virtual size_t buffer_size(std::ios_base::openmode = std::ios_base::in) const
            {
                return 0;
            }

            /// <summary>
            /// Set the stream buffer implementation to buffer or not buffer.
            /// </summary>
            /// <param name="size">The size to use for internal buffering, 0 to not buffer at all.</param>
            /// <param name="direction">The direction of buffering (in or out)</param>
            /// <remarks>An implementation that does not support buffering will silently ignore calls to this function and it will not have
            ///          any effect on what is returned by subsequent calls to buffer_size().</remarks>
            virtual void set_buffer_size(size_t , std::ios_base::openmode = std::ios_base::in)
            {
                return;
            }

            /// <summary>
            /// For any input stream, in_avail returns the number of characters that are immediately available
            /// to be consumed without blocking. May be used in conjunction with sbumpc() to 
            /// read data without incurring the overhead of using tasks.
            /// </summary>
            virtual size_t in_avail() const { return m_total; }


            // Seeking is not supported
            virtual pos_type seekpos(pos_type, std::ios_base::openmode) { return (pos_type)-1; }

            virtual pos_type seekoff(off_type offset, std::ios_base::seekdir whre, std::ios_base::openmode mode)
			{
				if (offset == 0 && whre == std::ios_base::cur && mode == std::ios_base::in)
					return (pos_type)m_total_read;
				else if (offset == 0 && whre == std::ios_base::cur && mode == std::ios_base::out)
					return (pos_type)m_total_written;
				else
					return (pos_type)-1; 
			}
            
			virtual _CharType* alloc(size_t count)
            {
                if ( !this->can_read() )
                {
                    // No one is reading, so why bother?
                    return nullptr;
                }

                // We always allocate a new block even if the count could be satisfied by
                // the current write block. While this does lead to wasted space it allows for
                // easier book keeping

                _PPLX_ASSERT(!m_allocBlock);
                m_allocBlock = std::make_shared<_block>(count);
                return m_allocBlock->wbegin();
            }

            virtual void commit(size_t count)
            {
                pplx::scoped_critical_section l(m_lock);

                // The count does not reflect the actual size of the block.
                // Since we do not allow any more writes to this block it would suffice. 
                // If we ever change the algorithm to reuse blocks then this needs to be revisited.

                _PPLX_ASSERT((bool)m_allocBlock);
                m_allocBlock->update_write_head(count);
                m_blocks.push_back(m_allocBlock);
                m_allocBlock = nullptr;

                update_write_head(count);
            }

            virtual bool acquire(_Out_writes_ (count) _CharType*& ptr, _In_ size_t& count)
            {
                pplx::scoped_critical_section l(m_lock);

                if (m_blocks.empty())
                {
                    count = 0;
                    ptr = nullptr;

                    // If the in_avail is 0, we want to return 'false' if the stream buffer
                    // is still open, to indicate that a subsequent attempt could
                    // be successful. If we return true, it will indicate that the end
                    // of the stream has been reached.
                    return !this->is_open(); 
                }
                else
                {
                    auto block = m_blocks.front();

                    count = block->rd_chars_left();
                    ptr = block->rbegin();

                    _PPLX_ASSERT(ptr != nullptr);
                    return true;
                }
            }

            virtual void release(_Out_writes_ (count) _CharType *, _In_ size_t count)
            {
                pplx::scoped_critical_section l(m_lock);
                auto block = m_blocks.front();

                _PPLX_ASSERT(block->rd_chars_left() >= count);
                block->m_read += count;

                update_read_head(count);
            }

        protected:        

            virtual pplx::task<bool> _sync()
            {
                pplx::scoped_critical_section l(m_lock);

                m_synced = in_avail();

                fulfill_outstanding();

                return pplx::task_from_result(true);
            }

            virtual pplx::task<int_type> _putc(_CharType ch)
            {
                return pplx::task_from_result((this->write(&ch, 1) == 1) ? static_cast<int_type>(ch) : traits::eof());
            }

            virtual pplx::task<size_t> _putn(const _CharType *ptr, size_t count)
            {
                return pplx::task_from_result<size_t>(this->write(ptr, count));
            }


            virtual pplx::task<size_t> _getn(_Out_writes_ (count) _CharType *ptr, _In_ size_t count)
            {
                pplx::task_completion_event<size_t> tce;
                enqueue_request(_request(count, [this, ptr, count, tce]()
                {
                    // VS 2010 resolves read to a global function.  Explicit
                    // invocation through the "this" pointer fixes the issue.
                    tce.set(this->read(ptr, count));
                }));
                return pplx::create_task(tce);
            }

            virtual size_t _sgetn(_Out_writes_ (count) _CharType *ptr, _In_ size_t count)
            { 
                pplx::scoped_critical_section l(m_lock);
                return can_satisfy(count) ? this->read(ptr, count) : (size_t)traits::requires_async();
            }

            virtual size_t _scopy(_Out_writes_ (count) _CharType *ptr, _In_ size_t count)
            {
                pplx::scoped_critical_section l(m_lock);
                return can_satisfy(count) ? this->read(ptr, count, false) : (size_t)traits::requires_async();
            }

            virtual pplx::task<int_type> _bumpc()
            {
                pplx::task_completion_event<int_type> tce;
                enqueue_request(_request(1, [this, tce]()
                {
                    tce.set(this->read_byte(true));
                }));
                return pplx::create_task(tce);
            }

            virtual int_type _sbumpc()
            {
                pplx::scoped_critical_section l(m_lock);
                return can_satisfy(1) ? this->read_byte(true) : traits::requires_async();
            }

            virtual pplx::task<int_type> _getc()
            {
                pplx::task_completion_event<int_type> tce;
                enqueue_request(_request(1, [this, tce]()
                {
                    tce.set(this->read_byte(false));
                }));
                return pplx::create_task(tce);
            }

            int_type _sgetc()
            {
                pplx::scoped_critical_section l(m_lock);
                return can_satisfy(1) ? this->read_byte(false) : traits::requires_async();
            }

            virtual pplx::task<int_type> _nextc()
            {
                pplx::task_completion_event<int_type> tce;
                enqueue_request(_request(1, [this, tce]()
                {
                    this->read_byte(true);
                    tce.set(this->read_byte(false));
                }));
                return pplx::create_task(tce);
            }

            virtual pplx::task<int_type> _ungetc()
            {            
                return pplx::task_from_result<int_type>(traits::eof()); 
            }

        private:

            /// <summary>
            /// Close the stream buffer for writing
            /// </summary>
            pplx::task<bool> _close_write()
            {
                // First indicate that there could be no more writes.
                // Fulfill outstanding relies on that to flush all the
                // read requests.
                this->m_stream_can_write = false;

                {
                    pplx::scoped_critical_section l(this->m_lock);

                    // This runs on the thread that called close.
                    this->fulfill_outstanding();
                }

                return pplx::task_from_result(true);
            }

            /// <summary>
            /// Updates the write head by an offset specified by count
            /// </summary>
            /// <remarks>This should be called with the lock held</remarks>
            void update_write_head(size_t count)
            {
                m_total += count;
				m_total_written += count;
                fulfill_outstanding();
            }

            /// <summary>
            /// Writes count characters from ptr into the stream buffer
            /// </summary>
            size_t write(const _CharType *ptr, size_t count)
            {
                if (!this->can_write() || (count == 0)) return 0;

                // If no one is going to read, why bother?
                // Just pretend to be writing!
                if (!this->can_read()) return count;

                pplx::scoped_critical_section l(m_lock);

                // Allocate a new block if necessary
                if ( m_blocks.empty() || m_blocks.back()->wr_chars_left() < count )
                {
                    SafeSize alloc = m_alloc_size.Max(count);
                    m_blocks.push_back(std::make_shared<_block>((size_t)alloc));
                }

                // The block at the back is always the write head
                auto last = m_blocks.back();
                auto countWritten = last->write(ptr, count);
                _PPLX_ASSERT(countWritten == count);

                update_write_head(countWritten);
                return countWritten;
            }

            /// <summary>
            /// Fulfill pending requests
            /// </summary>
            /// <remarks>This should be called with the lock held</remarks>
            void fulfill_outstanding()
            {
                while ( !m_requests.empty() )
                {
                    auto req = m_requests.front();

                    // If we cannot satisfy the request then we need
                    // to wait for the producer to write data
                    if (!can_satisfy(req.size())) return;

                    // We have enough data to satisfy this request
                    req.complete();

                    // Remove it from the request queue
                    m_requests.pop();
                }
            }

            /// <summary>
            /// Represents a memory block
            /// </summary>
            class _block
            {
            public:
                _block(size_t size)
                    : m_read(0), m_pos(0), m_size(size), m_data(new _CharType[size]) 
                {
                }

                ~_block()
                {
                    delete [] m_data;
                }

                // Read head
                size_t m_read;

                // Write head
                size_t m_pos;

                // Allocation size (of m_data)
                size_t m_size;

                // The data store
                _CharType * m_data;

                // Pointer to the read head
                _CharType * rbegin()
                {
                    return m_data + m_read;
                }

                // Pointer to the write head
                _CharType * wbegin()
                {
                    return m_data + m_pos;
                }

                // Read upto count characters from the block
                size_t read(_Out_writes_ (count) _CharType * dest, _In_ size_t count, bool advance = true)
                {
                    SafeSize avail(rd_chars_left());
                    auto countRead = static_cast<size_t>(avail.Min(count));

                    _CharType * beg = rbegin();
                    _CharType * end = rbegin() + countRead;

#ifdef _MS_WINDOWS
                    // Avoid warning C4996: Use checked iterators under SECURE_SCL
                    std::copy(beg, end, stdext::checked_array_iterator<_CharType *>(dest, count));
#else
                    std::copy(beg, end, dest);
#endif // _MS_WINDOWS

                    if (advance)
                    {
                        m_read += countRead;
                    }

                    return countRead;
                }

                // Write count characters into the block
                size_t write(const _CharType * src, size_t count)
                {
                    SafeSize avail(wr_chars_left());
                    auto countWritten = static_cast<size_t>(avail.Min(count));

                    const _CharType * srcEnd = src + countWritten;

#ifdef _MS_WINDOWS
                    // Avoid warning C4996: Use checked iterators under SECURE_SCL
                    std::copy(src, srcEnd, stdext::checked_array_iterator<_CharType *>(wbegin(), static_cast<size_t>(avail)));
#else
                    std::copy(src, srcEnd, wbegin());
#endif // _MS_WINDOWS

                    update_write_head(countWritten);
                    return countWritten;
                }

                void update_write_head(size_t count)
                {
                    m_pos += count;
                }

                size_t rd_chars_left() const { return m_pos-m_read; }
                size_t wr_chars_left() const { return m_size-m_pos; }

            private:

                // Copy is not supported
                _block(const _block&);
                _block& operator=(const _block&);
            };

            /// <summary>
            /// Represents a request on the stream buffer - typically reads
            /// </summary>
            class _request
            {
            public:

                typedef std::function<void()> func_type;
                _request(size_t count, const func_type& func)
                    : m_func(func), m_count(count)
                {
                }

                void complete()
                {
                    m_func();
                }

                size_t size() const
                {
                    return m_count;
                }

            private:

                func_type m_func;
                size_t m_count;
            };

            void enqueue_request(_request req)
            {
                pplx::scoped_critical_section l(m_lock);

                if (can_satisfy(req.size()))
                {
                    // We can immediately fulfill the request.
                    req.complete();
                }
                else
                {
                    // We must wait for data to arrive.
                    m_requests.push(req);
                }
            }

            /// <summary>
            /// Determine if the request can be satisfied.
            /// </summary>
            bool can_satisfy(size_t count)
            {
                return (m_synced > 0) || (this->in_avail() >= count) || !this->can_write();
            }

			/// <summary>
            /// Reads a byte from the stream and returns it as int_type.
            /// Note: This routine shall only be called if can_satisfy() returned true.
            /// </summary>
            /// <remarks>This should be called with the lock held</remarks>
            int_type read_byte(bool advance = true)
            {
                _CharType value;
                auto read_size = this->read(&value, 1, advance);
                return read_size == 1 ? static_cast<int_type>(value) : traits::eof();
            }

            /// <summary>
            /// Reads up to count characters into ptr and returns the count of characters copied.
            /// The return value (actual characters copied) could be <= count.
            /// Note: This routine shall only be called if can_satisfy() returned true.
            /// </summary>
            /// <remarks>This should be called with the lock held</remarks>
            size_t read(_Out_writes_ (count) _CharType *ptr, _In_ size_t count, bool advance = true)
            {
                _PPLX_ASSERT(can_satisfy(count));

                size_t read = 0;

                for (auto iter = begin(m_blocks); iter != std::end(m_blocks); ++iter)
                {
                    auto block = *iter;
                    auto read_from_block = block->read(ptr + read, count - read, advance);

                    read += read_from_block;

                    _PPLX_ASSERT(count >= read);
                    if (read == count) break;
                }

                if (advance)
                {
                    update_read_head(read);
                }

                return read;
            }

            /// <summary>
            /// Updates the read head by the specified offset
            /// </summary>
            /// <remarks>This should be called with the lock held</remarks>
            void update_read_head(size_t count)
            {
                m_total -= count;
				m_total_read += count;

                if ( m_synced > 0 )
                    m_synced = (m_synced > count) ? (m_synced-count) : 0;

                // The block at the front is always the read head.
                // Purge empty blocks so that the block at the front reflects the read head
                while (!m_blocks.empty())
                {
                    // If front block is not empty - we are done
                    if (m_blocks.front()->rd_chars_left() > 0) break;

                    // The block has no more data to be read. Relase the block
                    m_blocks.pop_front();
                }
            }

            // The in/out mode for the buffer
            std::ios_base::openmode m_mode;

            // Total available data
            size_t m_total;

			size_t m_total_read;
			size_t m_total_written;

            // Keeps track of the number of chars that have been flushed but still
            // remain to be consumed by a read operation.
			size_t m_synced;

            // Default block size
            SafeSize m_alloc_size;

            // The producer-consumer buffer is intended to be used concurrently by a reader
            // and a writer, who are not coordinating their accesses to the buffer (coordination
            // being what the buffer is for in the first place). Thus, we have to protect
            // against some of the internal data elements against concurrent accesses
            // and the possibility of inconsistent states. A simple non-recursive lock
            // should be sufficient for those purposes.
            pplx::critical_section m_lock;

            // Memory blocks
            std::deque<std::shared_ptr<_block>> m_blocks;

            // Queue of requests
            std::queue<_request> m_requests;

            // Block used for alloc/commit
            std::shared_ptr<_block> m_allocBlock;
        };

    } // namespace details 

    /// <summary>
    /// The producer_consumer_buffer class serves as a memory-based steam buffer that supports both writing and reading
    /// sequences of bytes. It can be used as a consumer/producer buffer.
    /// </summary>
    /// <remarks> 
    /// This is a reference-counted version of basic_producer_consumer_buffer.</remarks>
    template<typename _CharType>
    class producer_consumer_buffer : public streambuf<_CharType>
    {
    public:
        typedef _CharType char_type;

        /// <summary>
        /// Create a producer_consumer_buffer.
        /// </summary>
        /// <param name="alloc_size">The internal default block size.</param>
        producer_consumer_buffer(size_t alloc_size = 512) 
            : streambuf<_CharType>(std::make_shared<details::basic_producer_consumer_buffer<_CharType>>(alloc_size))
        {
        }
    };

}} // namespaces


#pragma warning(pop) // 4100
#endif  /* _CASA_PRODUCER_CONSUMER_STREAMS_H */
