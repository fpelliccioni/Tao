/* Tao C++11 Libraries.
 *
 * Copyright 2013  Fernando Pelliccioni (fpelliccioni@gmail.com)
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://github.com/fpelliccioni/Tao for library home page.
 */

#ifndef BOOST_CPP11_TOOLS_FUNCTION_TRAITS_HPP_INCLUDED
#define BOOST_CPP11_TOOLS_FUNCTION_TRAITS_HPP_INCLUDED

namespace boost {


namespace detail {

template <typename ReturnType, typename... Args>
struct function_traits_impl
{
    enum { arity = sizeof...(Args) };

    typedef ReturnType result_type;

    template <std::size_t i>
    struct arg
    {
        typedef typename std::tuple_element<i, std::tuple<Args...>>::type type;
    };
};

} // namespace detail

template <typename Functor>
struct function_traits
    : public function_traits<decltype(&Functor::operator())>
{};

//For Function pointer
template <typename ReturnType, typename... Args>
struct function_traits<ReturnType(*)(Args...)>
	: detail::function_traits_impl<ReturnType, Args...>
{};

// For const Functors
template <typename ClassType, typename ReturnType, typename... Args>
struct function_traits<ReturnType(ClassType::*)(Args...) const>
	: detail::function_traits_impl<ReturnType, Args...>
{};

// For mutable Functors
template <typename ClassType, typename ReturnType, typename... Args>
struct function_traits<ReturnType(ClassType::*)(Args...)>
	: detail::function_traits_impl<ReturnType, Args...>
{};



} // namespace boost

#endif // BOOST_CPP11_TOOLS_FUNCTION_TRAITS_HPP_INCLUDED