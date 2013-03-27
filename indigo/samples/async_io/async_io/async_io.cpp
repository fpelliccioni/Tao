#include "stdafx.h"
#include "scheduler.h"
#include "async_ifstream.h"
#include "async_ofstream.h"

void iterative_async_impl(std::shared_ptr<std::promise<void>> pro, std::function<std::future<bool>()> fun)
{
    fun().then([pro, fun](std::future<bool> f) {
        if (f.get())
            iterative_async_impl(pro, fun);
        else
            pro->set_value();
    });
}

std::future<void> iterative_async(std::function<std::future<bool>()> fun)
{
    auto pro = std::make_shared<std::promise<void>>();
    iterative_async_impl(pro, fun);
    return pro->get_future();
}

std::future<void> copy_file(const std::wstring &src_path, const std::wstring &dst_path)
{
    static const size_t buffer_size = 1028;

    auto src_stream = std::make_shared<async_ifstream>(src_path);
    auto dst_stream = std::make_shared<async_ofstream>(dst_path);
    auto progress = std::make_shared<size_t>();

    return iterative_async([src_stream, dst_stream, progress]() {
        return src_stream->read(buffer_size).then([dst_stream](std::future<std::vector<char>> f) {
            return dst_stream->write(f.get());
        }).unwrap().then([src_stream, progress](std::future<size_t> f) {
            *progress += f.get();
            return *progress < src_stream->size();
        });
    });
}
