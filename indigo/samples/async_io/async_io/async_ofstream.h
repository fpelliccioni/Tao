#pragma once

#include "stdafx.h"
#include "async_fstream_base.h"

class async_ofstream : public async_fstream_base
{
public:
	async_ofstream(const std::wstring &filename);
	~async_ofstream();

	std::future<size_t> write(std::vector<char> data);
private:
	static VOID CALLBACK io_completion_callback(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PVOID Overlapped, ULONG IoResult, ULONG_PTR NumberOfBytesTransferred, PTP_IO Io);
	
	std::unique_ptr<std::vector<char>> buffer;
	std::unique_ptr<std::promise<size_t>> pro;
};
