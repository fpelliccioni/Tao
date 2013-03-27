#pragma once

#include "stdafx.h"
#include "async_fstream_base.h"

class async_ifstream : public async_fstream_base
{
public:
	async_ifstream(const std::wstring &filename);
	~async_ifstream();

	size_t size() const;
	std::future<std::vector<char>> read(size_t size);
private:
	static VOID CALLBACK io_completion_callback(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PVOID Overlapped, ULONG IoResult, ULONG_PTR NumberOfBytesTransferred, PTP_IO Io);
	
	std::unique_ptr<std::vector<char>> buffer;
	std::unique_ptr<std::promise<std::vector<char>>> pro;
};
