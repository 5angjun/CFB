#include "Task.h"

#define ToString(x) case x: return L# x

static DWORD g_id = 0;
static std::mutex g_mutex;


Task::Task(TaskType type, byte* data, uint32_t datalen)
	:
	m_Type(type), 
	m_State(TaskState::Initialized), 
	m_dwDataLength(datalen),
	m_Data(new byte[datalen])
{
	std::lock_guard<std::mutex> guard(g_mutex);
	m_dwId = g_id++;

	::memcpy(m_Data, data, datalen);
}


Task::Task(HANDLE Handle)
	:
	m_State(TaskState::Initialized)
{
	BOOL fSuccess = FALSE;
	DWORD dwNbByteRead;
	DWORD dwTlvHeaderSize = 2 * sizeof(DWORD);


	//
	// Read the header (Type, Length)
	//
	std::vector<DWORD> headers(2);

	fSuccess = ::ReadFile(
		Handle,
		&headers[0],
		dwTlvHeaderSize,
		&dwNbByteRead,
		NULL
	);

	if (!fSuccess)
	{
		switch (::GetLastError())
		{
		case ERROR_BROKEN_PIPE:
			RAISE_EXCEPTION(BrokenPipeException, "ReadFile(1) failed");

		default:
			RAISE_GENERIC_EXCEPTION("ReadFile(1) failed");
		}
	}


	if (dwNbByteRead != dwTlvHeaderSize)
		RAISE_GENERIC_EXCEPTION("ReadFile(1): invalid size read");


	if (headers[0] >= TaskType::TaskTypeMax)
		RAISE_GENERIC_EXCEPTION("ReadFile(1): Message type is invalid");

	TaskType Type = static_cast<TaskType>(headers[0]);


	//
	// then allocate, and read the data
	//
	DWORD dwDataLength = headers[1];
	std::vector<byte> data(dwDataLength);

	if (dwDataLength)
	{
		fSuccess = ::ReadFile(
			Handle,
			&data[0],
			dwDataLength,
			&dwNbByteRead,
			NULL
		);

		if (!fSuccess)
			RAISE_GENERIC_EXCEPTION("ReadFile(2) failed");

		if (dwNbByteRead != dwDataLength)
			RAISE_GENERIC_EXCEPTION("ReadFile(2): invalid size read");
	}

	std::lock_guard<std::mutex> guard(g_mutex);
	m_dwId = g_id++;
	m_Type = Type;
	m_dwDataLength = dwDataLength;
	m_Data = new byte[m_dwDataLength];
	::memcpy(m_Data, m_Data, m_dwDataLength);
	return;
}


Task::~Task()
{
	//
	// the buffer associated with the Task can only be freeed when the 
	// task is completed.
	//
	if (m_State == TaskState::Completed)
	{
		delete[] m_Data;
	}
}


const wchar_t* Task::StateAsString()
{
	switch (m_State)
	{
		ToString(Initialized);
		ToString(Queued);
		ToString(Delivered);
		ToString(Completed);
	}

	throw std::runtime_error("Unknown Task State");
}


const wchar_t* Task::TypeAsString()
{
	switch (m_Type)
	{
		ToString(IoctlResponse);
		ToString(HookDriver);
		ToString(UnhookDriver);
		ToString(GetDriverInfo);
		ToString(GetNumberOfDriver);
		ToString(NotifyEventHandle);
		ToString(EnableMonitoring);
		ToString(DisableMonitoring);
		ToString(GetInterceptedIrps);
	}

#ifdef _DEBUG
	xlog(LOG_DEBUG, "Undeclared TaskType %d\n", m_Type);
#endif // _DEBUG

	return L"(UnknownType)";
}


const TaskType Task::Type()
{
	return m_Type;
}


const DWORD Task::IoctlCode()
{
	std::map<TaskType, DWORD>::iterator it = g_TaskIoctls.find(m_Type);
	if(it == g_TaskIoctls.end())
		return (DWORD)-1;

	return it->second;
}


void Task::SetState(TaskState s)
{
	m_State = s;
}


const uint32_t Task::Length()
{
	return m_dwDataLength;
}


byte* Task::Data()
{
	return m_Data;
}


const DWORD Task::Id()
{
	return m_dwId;
}
