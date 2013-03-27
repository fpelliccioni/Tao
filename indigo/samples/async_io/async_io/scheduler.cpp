#include "stdafx.h"
#include "scheduler.h"

eventloop_scheduler::eventloop_scheduler(HWND hWnd) : hWnd(hWnd), id(0)
{
}

void eventloop_scheduler::schedule(const std::function<void()> &work_item)
{
	std::unique_lock<std::mutex> lock(mtx);
	works[++id] = work_item;
	lock.unlock();
	PostMessage(hWnd, WM_USER, (WPARAM)id, (LPARAM)(runnable*)this);
}

void eventloop_scheduler::run(unsigned int id)
{
	std::unique_lock<std::mutex> lock(mtx);
	auto iter = works.find(id);
	std::function<void()> work_item = std::move(iter->second);
	works.erase(iter);
	lock.unlock();
	work_item();
}

io_daemon_scheduler::io_daemon_scheduler()
{
	hThread = CreateThread(NULL, 0, io_daemon_scheduler::routine, this, 0, NULL);
	hMutex = CreateMutex(NULL, FALSE, NULL);
	hSemaphore = CreateSemaphore(NULL, 0, INT_MAX, NULL);
}

void io_daemon_scheduler::schedule(const std::function<void()> &work_item)
{
	WaitForSingleObject(hMutex, INFINITE);
	works.push(work_item);
	ReleaseMutex(hMutex);
	ReleaseSemaphore(hSemaphore, 1, NULL);
}

DWORD WINAPI io_daemon_scheduler::routine(__in LPVOID lpParameter)
{
	auto self = (io_daemon_scheduler*)lpParameter;
	while (true)
	{
		auto result = WaitForSingleObjectEx(self->hSemaphore, INFINITE, TRUE);
		switch (result)
		{
		case WAIT_OBJECT_0:
			{
				WaitForSingleObject(self->hMutex, INFINITE);
				auto work_item = self->works.front();
				self->works.pop();
				ReleaseMutex(self->hMutex);
				work_item();
			}
			break;
		}
	}
}
