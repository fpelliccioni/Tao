#pragma once

#include "stdafx.h"

class async_fstream_base
{
protected:
	static std::ios_base::failure make_exception(DWORD error);
	static void throw_error(DWORD error);
	static void throw_last_error();
	
	HANDLE hFile;
	PTP_IO pIo;
	OVERLAPPED overlapped;
	std::atomic<bool> io_pending;
};
