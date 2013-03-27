#include "stdafx.h"
#include "async_io.h"
#include "async_ifstream.h"

async_ifstream::async_ifstream(const std::wstring &filename)
{
    ZeroMemory(&overlapped, sizeof(overlapped));

    hFile = CreateFile(filename.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        MessageBoxW(NULL, filename.c_str(), L"Cannot open file", MB_ICONERROR);
        throw_last_error();
    }

    pIo = CreateThreadpoolIo(hFile, io_completion_callback, this, NULL);
    if (pIo == NULL)
        throw_last_error();
}

async_ifstream::~async_ifstream()
{
    WaitForThreadpoolIoCallbacks(pIo, TRUE);
    CloseThreadpoolIo(pIo);
    CloseHandle(hFile);
}

size_t async_ifstream::size() const
{
    return GetFileSize(hFile, NULL);
}

std::future<std::vector<char>> async_ifstream::read(size_t size)
{
    if (size == 0)
        throw std::invalid_argument("zero should be greater than zero");
    
    bool _false = false;
    while (!io_pending.compare_exchange_strong(_false, true))
        ;

    StartThreadpoolIo(pIo);
    buffer = make_unique<std::vector<char>>(size);
    pro = make_unique<std::promise<std::vector<char>>>();
    auto future = pro->get_future();
    auto result = ReadFile(hFile, buffer->data(), size, NULL, &overlapped);
    if (result)
    {
        overlapped.Offset += result;
        buffer->resize(result);
        pro->set_value(std::move(*buffer));
        buffer.reset();
        pro.reset();
        io_pending.store(false);
    }
    else
    {
        auto error = GetLastError();
        if (error != ERROR_IO_PENDING)
        {
            CancelThreadpoolIo(pIo);
            throw_error(error);
        }
    }
    return future;
}

VOID CALLBACK async_ifstream::io_completion_callback(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PVOID Overlapped, ULONG IoResult, ULONG_PTR NumberOfBytesTransferred, PTP_IO Io)
{
    auto self = (async_ifstream*)Context;
    if (IoResult == NO_ERROR)
    {
        self->overlapped.Offset += NumberOfBytesTransferred;
        self->buffer->resize(NumberOfBytesTransferred);
        self->pro->set_value(std::move(*self->buffer));
    }
    else
        self->pro->set_exception(std::make_exception_ptr(make_exception(GetLastError())));
    self->buffer.reset();
    self->pro.reset();
    self->io_pending.store(false);
}
