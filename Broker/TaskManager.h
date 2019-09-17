#pragma once

#include "common.h"
#include "task.h"

#include <queue>
#include <mutex>


class TaskManager
{
public:
	TaskManager();
	~TaskManager();

	void push(Task t);
	Task pop();
	HANDLE GetPushEventHandle();
	BOOL SetName(const std::wstring name);


private:
	HANDLE m_hPushEvent = INVALID_HANDLE_VALUE;
	std::wstring m_name;
	std::queue<Task> m_queue;
	std::mutex m_mutex;
	std::condition_variable m_cond;
};