#include "stdafx.h"
#include "async_io.h"
#include "async_ofstream.h"

async_ofstream::async_ofstream(const std::wstring &filename)
{
	ZeroMemory(&overlapped, sizeof(overlapped));

	hFile = CreateFile(filename.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_OVERLAPPED, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		throw_last_error();

	pIo = CreateThreadpoolIo(hFile, io_completion_callback, this, NULL);
	if (pIo == NULL)
		throw_last_error();
}

async_ofstream::~async_ofstream()
{
	WaitForThreadpoolIoCallbacks(pIo, TRUE);
	CloseThreadpoolIo(pIo);
	CloseHandle(hFile);
}

std::future<size_t> async_ofstream::write(std::vector<char> data)
{	
	bool _false = false;
	while (!io_pending.compare_exchange_strong(_false, true))
		;

	StartThreadpoolIo(pIo);
	buffer = make_unique<std::vector<char>>(std::move(data));
	pro = make_unique<std::promise<size_t>>();
	auto future = pro->get_future();
	auto result = WriteFile(hFile, buffer->data(), buffer->size(), NULL, &overlapped);
	if (result)
	{
		overlapped.Offset += result;
		pro->set_value(result);
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

VOID CALLBACK async_ofstream::io_completion_callback(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PVOID Overlapped, ULONG IoResult, ULONG_PTR NumberOfBytesTransferred, PTP_IO Io)
{
	auto self = (async_ofstream*)Context;
	if (IoResult == NO_ERROR)
	{
		self->overlapped.Offset += NumberOfBytesTransferred;
		self->pro->set_value(NumberOfBytesTransferred);
	}
	else
		self->pro->set_exception(std::make_exception_ptr(make_exception(GetLastError())));
	self->buffer.reset();
	self->pro.reset();
	self->io_pending.store(false);
}
