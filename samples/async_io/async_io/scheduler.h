#pragma once

class runnable
{
public:
	virtual void run(unsigned int param) = 0;
};

class eventloop_scheduler : public std::scheduler, public runnable
{
public:
	eventloop_scheduler(HWND hWnd);
	
	void schedule(const std::function<void()> &work_item);

	void run(unsigned int id);

private:
	HWND hWnd;
	std::mutex mtx;
	std::map<unsigned int, std::function<void()>> works;
	unsigned int id;
};

extern eventloop_scheduler *gui_scheduler;

class io_daemon_scheduler : public std::scheduler
{
public:
	io_daemon_scheduler();

	void schedule(const std::function<void()> &work_item);
private:
	static DWORD WINAPI routine(__in LPVOID lpParameter);

	HANDLE hThread;
	HANDLE hMutex;
	HANDLE hSemaphore;
	std::queue<std::function<void()>> works;
};

extern io_daemon_scheduler *io_scheduler;