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
* json.h
*
* HTTP Library: JSON parser and writer
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#pragma once

#ifndef _CASA_JSON_H
#define _CASA_JSON_H

#include <memory>
#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include "xxpublic.h"
#include "basic_types.h"
#include "asyncrt_utils.h"

namespace web { namespace json
{
	// Various forward declarations.
    namespace details 
    {
        class _Value; 
        class _Number; 
        class _Null; 
        class _Boolean; 
        class _String; 
        class _Object; 
        class _Array;
        template <typename CharType> class JSON_Parser;
    }

#ifdef _MS_WINDOWS
#ifdef _DEBUG
#define ENABLE_JSON_VALUE_VISUALIZER
#endif
#endif

    /// <summary>
    /// A JSON value represented as a C++ class.
    /// </summary>
    class value
    {
    public:
        /// <summary>
        /// This enumeration represents the various kinds of JSON values.
        /// </summary>
        enum value_type { Number, Boolean, String, Object, Array, Null };

        /// <summary>
        /// Typedef for the standard container holding fields. This is used when constructing an
        /// object from existing objects.
        /// </summary>
        typedef std::vector<std::pair<json::value,json::value>> field_map;

        /// <summary>
        /// Typedef for the standard container holding array elements. This is used when constructing an
        /// array from existing objects.
        /// </summary>
        typedef std::vector<std::pair<json::value,json::value>> element_vector;

        /// <summary>
        /// Defined in support for STL algorithms that rely on iterators. This identifies the type
        /// of the non-const iterator.
        /// </summary>
        typedef std::vector<std::pair<json::value,json::value>>::iterator iterator;

        /// <summary>
        /// Defined in support for STL algorithms that rely on iterators. This identifies the type
        /// of the const iterator.
        /// </summary>
        typedef std::vector<std::pair<json::value,json::value>>::const_iterator const_iterator;

        /// <summary>
        /// Defined in support for STL algorithms that rely on iterators. This identifies the type
        /// of the non-const iterator.
        /// </summary>
        typedef std::vector<std::pair<json::value,json::value>>::reverse_iterator reverse_iterator;

        /// <summary>
        /// Defined in support for STL algorithms that rely on iterators. This identifies the type
        /// of the const iterator.
        /// </summary>
        typedef std::vector<std::pair<json::value,json::value>>::const_reverse_iterator const_reverse_iterator;

        /// <summary>
        /// Constructor creating a null value
        /// </summary>
        _ASYNCRTIMP value();

        /// <summary>
        /// Constructor creating a JSON number value
        /// </summary>
        /// <param name="value">The C++ value to create a JSON value from</param>
        _ASYNCRTIMP value(int32_t value);

        /// <summary>
        /// Constructor creating a JSON number value
        /// </summary>
        /// <param name="value">The C++ value to create a JSON value from</param>
        _ASYNCRTIMP value(double value);

        /// <summary>
        /// Constructor creating a JSON Boolean value
        /// </summary>
        /// <param name="value">The C++ value to create a JSON value from</param>
        _ASYNCRTIMP explicit value(bool value);

    public:

        /// <summary>
        /// Constructor creating a JSON string value
        /// </summary>
        /// <param name="value">The C++ value to create a JSON value from, a C++ STL double-byte string</param>
        _ASYNCRTIMP explicit value(utility::string_t);

        /// <summary>
        /// Constructor creating a JSON string value
        /// </summary>
        /// <param name="value">The C++ value to create a JSON value from, a C++ STL double-byte string</param>
        /// <remarks>This constructor exists in order to avoid string literals matching another constructor,
        /// as is very likely. For example, conversion to bool does not require a user-defined conversion,
        /// and will therefore match first, which means that the JSON value turns up as a boolean.</remarks>
        _ASYNCRTIMP explicit value(const utility::char_t *);

        /// <summary>
        /// Copy constructor
        /// </summary>
        _ASYNCRTIMP value(const value &);

        /// <summary>
        /// Move constructor
        /// </summary>
        _ASYNCRTIMP value(value &&);

        /// <summary>
        /// Assignment operator.
        /// </summary>
        _ASYNCRTIMP value &operator=(const value &);

        /// <summary>
        /// Move assignment operator.
        /// </summary>
        _ASYNCRTIMP value &operator=(value &&);

        /// <summary>
        /// Constructor creating a JSON value from an input stream, by parsing its contents.
        /// </summary>
        /// <param name="input">The stream to read the JSON value from</param>
        _ASYNCRTIMP static value parse(utility::istream_t &input);

#ifdef _MS_WINDOWS
        /// <summary>
        /// Constructor creating a JSON value from a byte buffer stream, by parsing its contents.
        /// </summary>
        /// <param name="stream">The stream to read the JSON value from</param>
        _ASYNCRTIMP static value parse(std::istream& stream);
#endif

        // Static factories

        /// <summary>
        /// Create a null value
        /// </summary>
        /// <returns>A JSON null value</returns>
        static _ASYNCRTIMP value __cdecl null();

        /// <summary>
        /// Create a number value
        /// </summary>
        /// <param name="value">The C++ value to create a JSON value from</param>
        /// <returns>A JSON number value</returns>
        static _ASYNCRTIMP value __cdecl number(double value);

        /// <summary>
        /// Create a number value
        /// </summary>
        /// <param name="value">The C++ value to create a JSON value from</param>
        /// <returns>A JSON number value</returns>
        static _ASYNCRTIMP value __cdecl number(int32_t value);

        /// <summary>
        /// Create a Boolean value
        /// </summary>
        /// <param name="value">The C++ value to create a JSON value from</param>
        /// <returns>A JSON Boolean value</returns>
        static _ASYNCRTIMP value __cdecl boolean(bool value);

        /// <summary>
        /// Create a string value
        /// </summary>
        /// <param name="value">The C++ value to create a JSON value from</param>
        /// <returns>A JSON string value</returns>
        static _ASYNCRTIMP value __cdecl string(utility::string_t value);

#ifdef _MS_WINDOWS
private:
        // Only used internally by JSON parser.
        template <typename CharType> friend class json::details::JSON_Parser;
        static _ASYNCRTIMP value __cdecl string(std::string value);

public:
#endif

        /// <summary>
        /// Create an object value
        /// </summary>
        /// <returns>An empty JSON object value</returns>
        static _ASYNCRTIMP json::value __cdecl object();

        /// <summary>
        /// Create an object value from a map of field/values
        /// </summary>
        /// <param name="fields">A map of field names associated with JSON values</param>
        /// <returns>A non-empty JSON object value</returns>
        static _ASYNCRTIMP json::value __cdecl object(const json::value::field_map &fields);

        /// <summary>
        /// Create an empty JSON array
        /// </summary>
        /// <returns>An empty JSON array value</returns>
        static _ASYNCRTIMP json::value __cdecl array();

        /// <summary>
        /// Create a JSON array
        /// </summary>
        /// <param name="size">The initial number of elements of the JSON value</param>
        /// <returns>A JSON array value</returns>
        static _ASYNCRTIMP json::value __cdecl array(size_t size);

        /// <summary>
        /// Create a JSON array
        /// </summary>
        /// <param name="elements">A vector of JSON values, </param>
        /// <returns>A JSON array value</returns>
        static _ASYNCRTIMP json::value __cdecl array(const json::value::element_vector &elements);

        /// <summary>
        /// Access the type of JSON value the current value instance is
        /// </summary>
        /// <returns>The value's type</returns>
        _ASYNCRTIMP json::value::value_type __cdecl type() const;

        /// <summary>
        /// Is the current value a null value?
        /// </summary>
        /// <returns>True if the value is a null value, false otherwise</returns>
        bool is_null() const { return type() == Null; };

        /// <summary>
        /// Is the current value a number value?
        /// </summary>
        /// <returns>True if the value is a number value, false otherwise</returns>
        bool is_number() const { return type() == Number; }

        /// <summary>
        /// Is the current value a Boolean value?
        /// </summary>
        /// <returns>True if the value is a Boolean value, false otherwise</returns>
        bool is_boolean() const { return type() == Boolean; }

        /// <summary>
        /// Is the current value a string value?
        /// </summary>
        /// <returns>True if the value is a string value, false otherwise</returns>
        bool is_string() const { return type() == String; }

        /// <summary>
        /// Is the current value an array?
        /// </summary>
        /// <returns>True if the value is an array, false otherwise</returns>
        bool is_array() const { return type() == Array; }

        /// <summary>
        /// Is the current value an object?
        /// </summary>
        /// <returns>True if the value is an object, false otherwise</returns>
        bool is_object() const { return type() == Object; }

        /// <summary>
        /// How many children does the value have?
        /// </summary>
        /// <returns>The number of children. 0 for all non-composites.</returns>
        size_t size() const;

        /// <summary>
        /// Represent the current JSON value as a double-byte string.
        /// </summary>
        /// <returns>A string representation of the value</returns>
        _ASYNCRTIMP utility::string_t to_string() const;

        /// <summary>
        /// Parse a string and construct a JSON value.
        /// </summary>
        /// <param name="value">The C++ value to create a JSON value from, a C++ STL double-byte string</param>
        _ASYNCRTIMP static value parse(utility::string_t);

#ifdef _MS_WINDOWS
        /// <summary>
        /// Write the current JSON value as a double-byte string to a stream instance.
        /// </summary>
        /// <param name="stream">The stream that the JSON string representation should be written to.</param>
        _ASYNCRTIMP void serialize(std::basic_ostream<utf16char> &stream) const;
#endif

        /// <summary>
        /// Serialize the content of the value into a stream in UTF8 format
        /// </summary>
        /// <param name="stream">The stream that the JSON string representation should be written to.</param>
        _ASYNCRTIMP void serialize(std::basic_ostream<char>& stream) const;

		/// <summary>
        /// Convert the JSON value to a C++ double, if and only if it is a number value.
        /// Throws <see cref="json_exception"/>  if the value is not a number
        /// </summary>
        /// <returns>A double representation of the value</returns>
        _ASYNCRTIMP double as_double() const;

        /// <summary>
        /// Convert the JSON value to a C++ integer, if and only if it is a number value.
        /// Throws <see cref="json_exception"/> if the value is not a number
        /// </summary>
        /// <returns>An 32-bit integer representation of the value</returns>
        _ASYNCRTIMP int32_t as_integer() const;

        /// <summary>
        /// Convert the JSON value to a C++ bool, if and only if it is a Boolean value.
        /// </summary>
        /// <returns>A C++ bool representation of the value</returns>
        _ASYNCRTIMP bool as_bool() const;

        /// <summary>
        /// Convert the JSON value to a C++ STL string, if and only if it is a string value.
        /// </summary>
        /// <returns>A C++ STL string representation of the value</returns>
        _ASYNCRTIMP utility::string_t as_string() const;

        /// <summary>
        /// Compare two JSON values for equality.
        /// </summary>
        /// <returns>True iff the values are equal.</returns>
		_ASYNCRTIMP bool operator==(const value& other) const;
            
        /// <summary>
        /// Compare two JSON values for inequality.
        /// </summary>
        /// <returns>True iff the values are unequal.</returns>
		bool operator!=(const value& other) const
        {
            return !((*this) == other);
        }

        /// <summary>
        /// Access a field of a JSON object.
        /// </summary>
        /// <param name="key">The name of the field</param>
        /// <returns>A reference to the value kept in the field</returns>
        _ASYNCRTIMP value & operator [] (const utility::string_t &key);

        /// <summary>
        /// Access a field of a JSON object.
        /// </summary>
        /// <param name="key">The name of the field</param>
        /// <returns>A reference to the value kept in the field</returns>
        _ASYNCRTIMP const value & operator [] (const utility::string_t &key) const;

#ifdef _MS_WINDOWS
private:
        // Only used internally by JSON parser
        _ASYNCRTIMP value & operator [] (const std::string &key)
        {
            // JSON object stores its field map as a unordered_map of string_t, so this conversion is hard to avoid
            return operator[](utility::conversions::to_string_t(key));
        }
public:
#endif
        
        /// <summary>
        /// Access an element of a JSON array.
        /// </summary>
        /// <param name="key">The name of the field</param>
        /// <returns>A reference to the value kept in the field</returns>
        _ASYNCRTIMP value & operator [] (size_t index);

        /// <summary>
        /// Access an element of a JSON array.
        /// </summary>
        /// <param name="key">The name of the field</param>
        /// <returns>A reference to the value kept in the field</returns>
        _ASYNCRTIMP const value & operator [] (size_t index) const;

        /// <summary>
        /// Get the beginning iterator element for a composite value.
        /// </summary>
		iterator begin();

        /// <summary>
        /// Get the end iterator element for a composite value.
        /// </summary>
		iterator end();

        /// <summary>
        /// Get the beginning reverse iterator element for a composite value.
        /// </summary>
		reverse_iterator rbegin();

        /// <summary>
        /// Get the end reverse iterator element for a composite value.
        /// </summary>
		reverse_iterator rend();

        /// <summary>
        /// Get the beginning const iterator element for a composite value.
        /// </summary>
		const_iterator cbegin() const;

        /// <summary>
        /// Get the end const iterator element for a composite value.
        /// </summary>
		const_iterator cend() const;

        /// <summary>
        /// Get the beginning const reverse iterator element for a composite value.
        /// </summary>
		const_reverse_iterator crbegin() const;

        /// <summary>
        /// Get the end const reverse iterator element for a composite value.
        /// </summary>
		const_reverse_iterator crend() const;

    private:
        friend class web::json::details::_Object;
        friend class web::json::details::_Array;
        template<typename CharType> friend class web::json::details::JSON_Parser;

#ifdef _MS_WINDOWS
		/// <summary>
        /// Write the current JSON value as a double-byte string to a string instance.
        /// </summary>
        /// <param name="string">The string that the JSON representation should be written to.</param>
        _ASYNCRTIMP void format(std::basic_string<utf16char> &string) const;
#endif
		/// <summary>
        /// Serialize the content of the value into a string instance in UTF8 format
        /// </summary>
        /// <param name="string">The string that the JSON representation should be written to</param>
        _ASYNCRTIMP void format(std::basic_string<char>& string) const;

#ifdef ENABLE_JSON_VALUE_VISUALIZER
        // Make this constructor explicit to prevent erroneous invocations in assignments (esp. non-ENABLE_JSON_VALUE_VISUALIZER case)
        explicit value(std::unique_ptr<details::_Value> v, value_type kind) : m_value(std::move(v)), m_kind(kind)
#else
        explicit value(std::unique_ptr<details::_Value> v) : m_value(std::move(v))
#endif
        {}

        std::unique_ptr<details::_Value> m_value;
#ifdef ENABLE_JSON_VALUE_VISUALIZER
        value_type m_kind;
#endif
    };
    
    /// <summary>
    /// A single exception type to represent errors in parsing, converting, and accessing
    /// elements of JSON values.
    /// </summary>
    class json_exception : public std::exception
    { 
    private:
        std::string _message;
    public:
        json_exception() {}
        json_exception(const utility::char_t * const &message) : _message(utility::conversions::to_utf8string(message)) { }

        // Must be narrow string because it derives from std::exception
        const char* what() const _noexcept
        {
            return _message.c_str();
        }
        ~json_exception() _noexcept {}
    };

    namespace details
    {
        class _Value 
        {
        public:
            virtual std::unique_ptr<_Value> _copy_value() = 0;

            virtual const json::value::element_vector &elements() const { throw json_exception(U("not an array")); }
            virtual const json::value::field_map &fields() const { throw json_exception(U("not an object")); }

            virtual value &index(const utility::string_t &) { throw json_exception(U("not an object")); }
            virtual value &index(std::vector<value>::size_type) { throw json_exception(U("not an array")); }

            virtual const value &cnst_index(const utility::string_t &) const { throw json_exception(U("not an object")); }
            virtual const value &cnst_index(std::vector<value>::size_type) const { throw json_exception(U("not an array")); }

            utility::string_t to_string() const
			{ 
				utility::string_t result;  
				format(result); 
				return result; 
			}

            virtual json::value::value_type type() const { return json::value::Null; }

            virtual double as_double() const { throw json_exception(U("not a number")); }
            virtual int32_t as_integer() const { throw json_exception(U("not a number")); }
            virtual bool as_bool() const { throw json_exception(U("not a boolean")); }
            virtual utility::string_t as_string() const { throw json_exception(U("not a string")); }

            virtual size_t size() const { return m_elements.size(); }

            virtual ~_Value() {}

            virtual json::value::element_vector &elements() { return m_elements; }

        protected:
            _Value() {}
            _Value(std::vector<json::value>::size_type size) : m_elements(size) {}
            _Value(const json::value::element_vector &elements) : m_elements(elements) { }

#ifdef _MS_WINDOWS
            virtual void format(std::basic_ostream<wchar_t> &stream) const
			{
                // Default implementation: just get a string and send it to the stream.
				utility::string_t result;  
				format(result); 
				stream << result;
			}
#endif
            virtual void format(std::basic_ostream<char>& stream) const
			{
                // Default implementation: just get a string and send it to the stream.
				std::string result;  
				format(result); 
				stream << result;
			}

            virtual void format(std::basic_string<char>& stream) const
            {
                stream.append("null");
            }
#ifdef _MS_WINDOWS
            virtual void format(std::basic_string<wchar_t>& stream) const
            {
                stream.append(L"null");
            }
#endif
            mutable json::value::element_vector m_elements;
		private:

			friend class web::json::value;
			friend class web::json::details::_Object;
			friend class web::json::details::_Array;
        };

        class _Null : public _Value
        {
        public:

            virtual std::unique_ptr<_Value> _copy_value()
            {
                return utility::details::make_unique<_Null>(*this);
            }

            virtual json::value::value_type type() const { return json::value::Null; }

            _Null() { }

        private:
            template<typename CharType> friend class json::details::JSON_Parser;
        };

        class _Number : public _Value
        {
        public:

            virtual std::unique_ptr<_Value> _copy_value()
            {
                return utility::details::make_unique<_Number>(*this);
            }

            virtual json::value::value_type type() const { return json::value::Number; }

            virtual double  as_double() const { return m_was_int ? static_cast<double>(m_intval) : m_value; }
            virtual int32_t as_integer() const { return m_was_int ? m_intval : static_cast<int32_t>(m_value); }

		protected:
			virtual void format(std::basic_string<char>& stream) const
            {
                format_impl(stream);
            }
#ifdef _MS_WINDOWS
            virtual void format(std::basic_string<wchar_t>& stream) const
            {
                format_impl(stream);
            }
#endif
        private:
            template<typename CharType> friend class json::details::JSON_Parser;
        public:
            _Number(double value)  : m_value(value), m_was_int(false) { }
            _Number(int32_t value) : m_intval(value), m_was_int(true) { }
        private:
            template<typename CharType>
            void format_impl(std::basic_string<CharType>& stream) const
            {
				std::basic_stringstream<CharType> strbuf;
                if ( m_was_int ) {
                    strbuf << m_intval; 
                }else{
                    strbuf << m_value;
                }
				stream.append(strbuf.str());
            }

            union
            {
                double  m_value;
                int32_t m_intval;
            };

            bool    m_was_int;
        };

        class _Boolean : public _Value
        {
        public:

            virtual std::unique_ptr<_Value> _copy_value()
            {
                return utility::details::make_unique<_Boolean>(*this);
            }

            virtual json::value::value_type type() const { return json::value::Boolean; }

            virtual bool as_bool() const { return m_value; }

		protected:
            virtual void format(std::basic_string<char>& stream) const
            {
                stream.append(m_value ? "true" : "false");
            }

#ifdef _MS_WINDOWS
            virtual void format(std::basic_string<wchar_t>& stream) const
            {
                stream.append(m_value ? L"true" : L"false");
            }
#endif
        private:
            template<typename CharType> friend class json::details::JSON_Parser;
        public:
            _Boolean(bool value) : m_value(value) { }
        private:
            bool m_value;
        };

        class _String : public _Value
        {
        public:

            virtual std::unique_ptr<_Value> _copy_value()
            {
                return utility::details::make_unique<_String>(*this);
            }

            virtual json::value::value_type type() const { return json::value::String; }

            virtual utility::string_t as_string() const;

        protected:
            virtual void format(std::basic_string<char>& stream) const
            {
                stream.push_back('"');
                stream.append(as_utf8_string()).push_back('"');
            }
#ifdef _MS_WINDOWS
            virtual void format(std::basic_string<wchar_t>& stream) const
            {
                stream.push_back(L'"');
                stream.append(as_utf16_string()).push_back(L'"');
            }
#endif
        private:
            template<typename CharType> friend class json::details::JSON_Parser;

            void copy_from(const _String& other)
            {
                if(other.is_wide())
                {
                    m_wstring.reset(new utf16string(*other.m_wstring));
                }
                else
                {
                    m_string.reset(new std::string(*other.m_string));
                }
            }

        public:

            _String(const _String& other) : web::json::details::_Value(other)
            {
                copy_from(other);
            }

            _String& operator=(const _String& other)
            {
                if (this != &other) {
                    copy_from(other);
                }
                return *this;
            }

            _String(const utf16string &value) : m_wstring(utility::details::make_unique<utf16string>(value)) { }
            _String(const std::string &value) : m_string(utility::details::make_unique<std::string>(value)) { }

        private:
            std::string as_utf8_string() const;
            utf16string as_utf16_string() const;

            // One of the below pointers is empty to avoid
            // unnecessary conversions from UTF8 to UTF16
            std::unique_ptr<utf16string> m_wstring;
            std::unique_ptr<std::string> m_string;

            bool is_wide() const
            {
                return m_wstring.get() != nullptr;
            }
        };

        class _Object : public _Value
        {
        public:

            virtual std::unique_ptr<_Value> _copy_value()
            {
                return utility::details::make_unique<_Object>(*this);
            }

            virtual json::value::value_type type() const { return json::value::Object; }

            _ASYNCRTIMP virtual json::value &index(const utility::string_t &key);
            _ASYNCRTIMP virtual const json::value &cnst_index(const utility::string_t &key) const;

            bool is_equal(const _Object* other) const
            {
                if ( m_elements.size() != other->m_elements.size())
                    return false;

                auto iterT  = std::begin(m_elements);
                auto iterO  = std::begin(other->m_elements);
                auto iterTe = std::end(m_elements);
                auto iterOe = std::end(other->m_elements);

                for (; iterT != iterTe && iterO != iterOe; ++iterT, ++iterO)
                {
                    if ( *iterT != *iterO )
                        return false;
                }

                return true;
            }

        protected:
            virtual void format(std::basic_string<char>& string) const;
            virtual void format(std::basic_ostream<char>& stream) const;
#ifdef _MS_WINDOWS
            virtual void format(std::basic_string<wchar_t>& string) const;
            virtual void format(std::basic_ostream<wchar_t>& stream) const;
#endif

        private:
            template<typename CharType> friend class json::details::JSON_Parser;

            template<typename _CharType>
            void format_impl(std::basic_string<_CharType>& string) const;
            template<typename _CharType>
            void format_impl(std::basic_ostream<_CharType>& stream) const;

            _ASYNCRTIMP void map_fields();

        public:
            _Object()
            {
            }
            
            _Object(const json::value::field_map& fields) : _Value(fields) { }

            _ASYNCRTIMP _Object(const _Object& other);

            virtual ~_Object() {}

        private:
            // In order to speed up field lookups, we keep a map that is
            // filled on demand at the first lookup.
            std::unordered_map<utility::string_t, size_t> m_fields;
        };

        class _Array : public _Value
        {
        public:

            virtual std::unique_ptr<_Value> _copy_value()
            {
                return utility::details::make_unique<_Array>(*this);
            }

            virtual json::value::value_type type() const { return json::value::Array; }

            virtual json::value &index(std::vector<json::value>::size_type index)
            {
#ifdef _MS_WINDOWS
               msl::utilities::SafeInt<std::vector<json::value>::size_type> nMinSize(index);
               nMinSize += 1;
               msl::utilities::SafeInt<std::vector<json::value>::size_type> nlastSize(m_elements.size());
#else
               size_t nMinSize  = index + 1;
               size_t nlastSize = m_elements.size();
#endif
               if (nlastSize < nMinSize)
               {
                  m_elements.resize(nMinSize);
                  for (size_t i = nlastSize; i < nMinSize; i++)
                  {
                      m_elements[i].first = json::value(static_cast<int32_t>(i));
                  }
               }
               return m_elements[index].second; 
            }

            virtual const json::value &cnst_index(std::vector<json::value>::size_type index) const
            {
#ifdef _MS_WINDOWS
               msl::utilities::SafeInt<std::vector<json::value>::size_type> idx(index);
               msl::utilities::SafeInt<std::vector<json::value>::size_type> size(m_elements.size());
#else
               size_t idx = index;
               size_t size = m_elements.size();
#endif
                if ( idx >= size )
                    throw json_exception(U("index out of bounds"));
                return m_elements[index].second; 
            }

            bool is_equal(const _Array* other) const
            {
                if ( m_elements.size() != other->m_elements.size())
                    return false;

                auto iterT  = std::begin(m_elements);
                auto iterO  = std::begin(other->m_elements);
                auto iterTe = std::end(m_elements);
                auto iterOe = std::end(other->m_elements);

                for (; iterT != iterTe && iterO != iterOe; ++iterT, ++iterO)
                {
                    if ( *iterT != *iterO )
                        return false;
                }

                return true;
            }

		protected:
            virtual void format(std::basic_string<char>& stream) const;
            virtual void format(std::basic_ostream<char>& stream) const;
#ifdef _MS_WINDOWS
            virtual void format(std::basic_string<wchar_t>& stream) const;
            virtual void format(std::basic_ostream<wchar_t>& stream) const;
#endif
        private:
            template<typename CharType> friend class json::details::JSON_Parser;

            template<typename _CharType>
            void format_impl(std::basic_string<_CharType>& string) const;
            template<typename _CharType>
            void format_impl(std::basic_ostream<_CharType>& stream) const;
        public:
            _Array() {}
            _Array(std::vector<json::value>::size_type size) : _Value(size) {}
            _Array(const json::value::element_vector &elements) : _Value(elements) { }
        private:
        };

    } // namespace details

    inline size_t json::value::size() const
    {
        return m_value->size();
    }

    inline json::value::iterator json::value::begin()
    {
        return m_value->elements().begin();
    }

    inline json::value::iterator json::value::end()
    {
        return m_value->elements().end();
    }

    inline json::value::reverse_iterator json::value::rbegin()
    {
        return m_value->elements().rbegin();
    }

    inline json::value::reverse_iterator json::value::rend()
    {
        return m_value->elements().rend();
    }

    inline json::value::const_iterator json::value::cbegin() const
    {
        return m_value->elements().cbegin();
    }

    inline json::value::const_iterator json::value::cend() const
    {
        return m_value->elements().cend();
    }

    inline json::value::const_reverse_iterator json::value::crbegin() const
    {
        return m_value->elements().crbegin();
    }

    inline json::value::const_reverse_iterator json::value::crend() const
    {
        return m_value->elements().crend();
    }

    /// <summary>
    /// A standard std::ostream operator to facilitate writing JSON values to streams.
    /// </summary>
    _ASYNCRTIMP utility::ostream_t& operator << (utility::ostream_t &os, const json::value &val);

    /// <summary>
    /// A standard std::ostream operator to facilitate reading JSON values from streams.
    /// </summary>
    _ASYNCRTIMP utility::istream_t& operator >> (utility::istream_t &is, json::value &val);

}} // namespace web::json

#endif  /* _CASA_JSON_H */
