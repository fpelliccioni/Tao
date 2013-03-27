#include "stdafx.h"
#include "async_fstream_base.h"

std::ios_base::failure async_fstream_base::make_exception(DWORD error)
{
    return std::ios_base::failure("error " + std::to_string(error));
}

void async_fstream_base::throw_error(DWORD error)
{
    throw make_exception(error);
}

void async_fstream_base::throw_last_error()
{
    throw_error(GetLastError());
}
