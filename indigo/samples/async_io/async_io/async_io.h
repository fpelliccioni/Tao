#pragma once

#include "resource.h"

template<typename T>
std::unique_ptr<T> make_unique()
{
	return std::unique_ptr<T>(new T());
}

template<typename T, typename Arg0>
std::unique_ptr<T> make_unique(Arg0 arg0)
{
	return std::unique_ptr<T>(new T(std::forward<Arg0>(arg0)));
}


std::future<void> copy_file(const std::wstring &src_path, const std::wstring &dst_path);