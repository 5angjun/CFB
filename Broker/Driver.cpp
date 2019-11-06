#include "driver.h"


/*++

Routine Description:

Simple function to create and share the event associated with the userland notification 
that new data has been captured by the driver (and can be read).


Arguments:

	hDevice - a handle to the IrpDumper device

	
Return Value:

	Returns a handle to the event shared with the driver on success, INVALID_HANDLE_VALUE otherwise

--*/
static inline HANDLE ShareHandleWithDriver(_In_ HANDLE hDevice)
{
	DWORD dwNbBytesReturned;
	

	HANDLE hDataEvent = ::CreateEvent(
		NULL,
		TRUE,
		FALSE,
		NULL
	);

	if (!hDataEvent)
		return INVALID_HANDLE_VALUE;


	BOOL bRes = ::DeviceIoControl(
		hDevice,
		IOCTL_SetEventPointer,
		(byte*)&hDataEvent,
		sizeof(HANDLE),
		NULL,
		0,
		&dwNbBytesReturned,
		NULL
	);

#ifdef _DEBUG
	xlog(LOG_DEBUG, L"ShareHandleWithDriver() returned: %s\n", bRes ? L"TRUE" : L"FALSE");
#endif

	if (bRes)
		return hDataEvent;

	::CloseHandle(hDataEvent);

	return INVALID_HANDLE_VALUE;
}


/*++

Routine Description:

Fetch one IRP (metadata + data) temporarily from the IrpDumper driver, and stores it in memory.


Arguments:

	hDevice -

	hEvent - 

	Session -



Return Value:

	Returns ERROR_SUCCESS on success, GetLastError() otherwise

--*/
static DWORD FetchNextIrpFromDevice(_In_ HANDLE hDevice, _In_ HANDLE hEvent, _In_ Session& Session)
{
	DWORD dwBufferSize = 0;
	byte* lpBuffer = NULL;
	DWORD lpNumberOfBytesRead;

	BOOL bRes = ::ReadFile(hDevice, lpBuffer, dwBufferSize, &lpNumberOfBytesRead, NULL);

	//
	// Retrieve the expected buffer size
	//
	xlog(LOG_DEBUG, L"ReadFile(hDevice=%p, dwBufferSize=%d, lpNumberOfBytesRead=%d) -> %d\n", hDevice, dwBufferSize, lpNumberOfBytesRead, bRes);
	DWORD dwErrCode = ::GetLastError();

	if (bRes == FALSE)
	{
		PrintErrorWithFunctionName(L"ReadFile(GetBufferSize)");
		return dwErrCode;
	}

	if (lpNumberOfBytesRead == 0)
	{
		return ERROR_NO_DATA;
	}

	//
	// Create a buffer of the correct size, and fetch the raw IRP
	//
	dwBufferSize = lpNumberOfBytesRead;
	lpBuffer = new byte[dwBufferSize];

	bRes = ::ReadFile(hDevice, lpBuffer, dwBufferSize, &lpNumberOfBytesRead, NULL);
	dwErrCode = ::GetLastError();

	if (bRes == FALSE)
	{
		PrintErrorWithFunctionName(L"ReadFile(GetBufferData)");
		return dwErrCode;
	}


	if (dwBufferSize == lpNumberOfBytesRead && dwBufferSize >= sizeof(INTERCEPTED_IRP_HEADER))
	{
		PINTERCEPTED_IRP_HEADER pIrpHeader = (PINTERCEPTED_IRP_HEADER)lpBuffer;
		PINTERCEPTED_IRP_BODY pIrpBody = (PINTERCEPTED_IRP_BODY)(lpBuffer + sizeof(INTERCEPTED_IRP_HEADER));

#ifdef _DEBUG
		xlog(LOG_DEBUG, 
			L"New IRP received:\n"
			L" - timestamp:%llu\n"
			L" - IRQ level:%x\n"
			L" - Major type:%x\n"
			L" - IoctlCode:%x\n"
			L" - PID=%d / TID=%d\n"
			L" - InputBufferLength=%d / OutputBufferLength=%d\n"
			L" - DriverName:%s\n"
			L" - DeviceName:%s\n",
			pIrpHeader->TimeStamp,
			pIrpHeader->Irql,
			pIrpHeader->Type,
			pIrpHeader->IoctlCode,
			pIrpHeader->Pid, 
			pIrpHeader->Tid,
			pIrpHeader->InputBufferLength, 
			pIrpHeader->OutputBufferLength,
			pIrpHeader->DriverName,
			pIrpHeader->DeviceName
		);
#endif // _DEBUG


		//
		// pushing new IRP to the session queue\n");
		//
		Irp irp(pIrpHeader, pIrpBody);

		std::unique_lock<std::mutex> mlock(Session.m_IrpMutex);
		Session.m_IrpQueue.push(irp);
		mlock.unlock();
	}

	//
	// Reset the event
	//
	ResetEvent(hEvent);

	return ERROR_SUCCESS;
}


/*++

Routine Description:

Fetch all the IRP stored in the IrpDumper driver, and stores it in memory.


Arguments:

	hDevice -

	hEvent -

	Session -



Return Value:

	Returns ERROR_SUCCESS on success, GetLastError() otherwise

--*/
DWORD FetchAllIrpFromDevice(_In_ HANDLE hDevice, _In_ HANDLE hEvent, _In_ Session& Session, _Out_ PDWORD lpdwNumberOfIrpDumped)
{
	DWORD dwRes = ERROR_SUCCESS;
	*lpdwNumberOfIrpDumped = 0;

	do
	{
		dwRes = FetchNextIrpFromDevice(hDevice, hEvent, Session);
		
		if (dwRes == ERROR_NO_DATA)
			break;

		(*lpdwNumberOfIrpDumped)++;

		if (dwRes != ERROR_SUCCESS)
			break;
	} 
	while (Session.IsRunning());

	return dwRes;
}


/*++

Routine Description:

This routine collects the IRP data from the driver.

Arguments:

	lpParameter -


Return Value:

	Returns 0 on success

--*/
DWORD IrpCollectorThreadRoutine(_In_ LPVOID lpParameter)
{
	Session& Sess = *(reinterpret_cast<Session*>(lpParameter));


	//
	// Get a handle to the device
	//
	wil::unique_handle hIrpDumperDevice(
		::CreateFile(
			CFB_USER_DEVICE_NAME,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL
		)
	);

	if (hIrpDumperDevice==NULL || hIrpDumperDevice.get()==INVALID_HANDLE_VALUE)
	{
		PrintErrorWithFunctionName(L"CreateFile(hDeviceObject)");
		return ERROR_DEVICE_NOT_AVAILABLE;
	}


	//
	// Create an event for the driver to notify the broker new data has arrived
	//
	wil::unique_handle hIrpDataEvent(
		ShareHandleWithDriver(hIrpDumperDevice.get())
	);

	if (!hIrpDataEvent)
	{
		PrintErrorWithFunctionName(L"ShareHandleWithDriver()");
		return ERROR_INVALID_HANDLE;
	}


	const HANDLE Handles[2] = {
		Sess.m_hTerminationEvent,
		hIrpDataEvent.get()
	};

	xlog(LOG_SUCCESS, L"[IrpCollectorThreadRoutine] listening for new IRPs from driver...\n");

	while (Sess.IsRunning())
	{
		DWORD dwWaitResult = ::WaitForMultipleObjects(_countof(Handles), Handles, FALSE, INFINITE);

		switch (dwWaitResult)
		{
		case WAIT_OBJECT_0:
#ifdef _DEBUG
			xlog(LOG_DEBUG, L"[IrpCollectorThreadRoutine] received termination Event\n");
#endif // _DEBUG

			Sess.Stop();
			break;

		case WAIT_OBJECT_0 + 1:
		{
#ifdef _DEBUG
			xlog(LOG_DEBUG, L"new IRP data Event\n");
#endif // _DEBUG

			DWORD dwNbIrpDumped = 0;
			DWORD dwRes = FetchAllIrpFromDevice(hIrpDumperDevice.get(), hIrpDataEvent.get(), Sess, &dwNbIrpDumped);
#ifdef _DEBUG
			xlog(LOG_DEBUG, L"fetched %d IRP from driver\n", dwNbIrpDumped);
#endif // _DEBUG
			break;
		}

		default:
			PrintErrorWithFunctionName(L"WaitForMultipleObjects()");
			Sess.Stop();
			break;
		}
	}


#ifdef _DEBUG
	xlog(LOG_DEBUG, L"terminating thread TID=%d\n", GetThreadId(GetCurrentThread()));
#endif // _DEBUG

	return ERROR_SUCCESS;
}


/*++

Routine Description:

Takes a request Task, and creates and send a valid DeviceIoControl() to the IrpDumper driver. The function
also builds a response Task from the response of the  DeviceIoControl().


--*/
static Task SendTaskToDriver(_In_ Task task, _In_ HANDLE hDevice)
{

#ifdef _DEBUG
	xlog(LOG_DEBUG, L"Sending to device Task=%d (Type: %s, Length: %d)\n", task.Id(), task.TypeAsString(), task.Length());
#endif // _DEBUG

	byte* lpOutputBuffer = nullptr;
	DWORD dwOutputBufferSize = 0;
	DWORD dwNbBytesReturned = 0;
	DWORD dwErrCode;

	while (TRUE)
	{
		dwErrCode = ERROR_SUCCESS;

		//
		// send the DeviceIoControl
		//
		BOOL bRes = ::DeviceIoControl(
			hDevice,
			task.IoctlCode(),
			task.Data(),
			task.Length(),
			lpOutputBuffer,
			dwOutputBufferSize,
			&dwNbBytesReturned,
			NULL
		);

#ifdef _DEBUG
		xlog(LOG_DEBUG, L"DeviceIoControl(0x%x) returned: %s\n", task.IoctlCode(), bRes ? L"TRUE" : L"FALSE");
#endif // _DEBUG

		//
		// If the ioctl was ok, we exit
		//
		if (bRes)
			break;

		dwErrCode = ::GetLastError();


		//
		// If the buffer was too small, retry with the appropriate size
		//
		if (::GetLastError() == ERROR_INSUFFICIENT_BUFFER)
		{
			dwOutputBufferSize = dwNbBytesReturned;
			if (lpOutputBuffer)
				delete[] lpOutputBuffer;

			lpOutputBuffer = new byte[dwOutputBufferSize];
			continue;
		}

		break;
	}


	//
	// flag task as Completed, the task can finally be deleted
	//
	task.SetState(TaskState::Completed);


	//
	// Prepare the response task
	// A response task always starts with the response code (DWORD), then optionally the data if any
	//
	DWORD dwResponseBufferSize = dwOutputBufferSize + sizeof(DWORD);
	byte* lpResponseBuffer = new byte[dwResponseBufferSize];
	::memcpy(lpResponseBuffer, &dwErrCode, sizeof(DWORD));
	if (lpOutputBuffer)
	{
		::memcpy(lpResponseBuffer + sizeof(DWORD), lpOutputBuffer, dwOutputBufferSize);
		delete[] lpOutputBuffer;
	}


	Task response_task(TaskType::IoctlResponse, lpResponseBuffer, dwResponseBufferSize);
	delete[] lpResponseBuffer;

	return response_task;
}


/*++

Routine Description:

Reads the new tasks received by the FrontEnd thread, and bounces thoses requests to the backend (i.e. the driver) 
via the IOCTL codes (which can be found in "Driver\Header Files\IoctlCodes.h" - imported by common.h).

The driver must acknowledge the request by sending a response (even if the result content is asynchronous).


Arguments:

	lpParameter - 


Return Value:
	
	Returns 0 on success

--*/
DWORD BackendConnectionHandlingThread(_In_ LPVOID lpParameter)
{
	Session& Sess = *(reinterpret_cast<Session*>(lpParameter));

	wil::unique_handle hIrpDumperDevice(
		::CreateFile(
			CFB_USER_DEVICE_NAME,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL
		)
	);

	if (hIrpDumperDevice == NULL || hIrpDumperDevice.get() == INVALID_HANDLE_VALUE)
	{
		PrintErrorWithFunctionName(L"CreateFile(hDeviceObject)");
		return ERROR_DEVICE_NOT_AVAILABLE;
	}

	const HANDLE Handles[2] = { 
		Sess.m_hTerminationEvent, 
		Sess.RequestTasks.m_hPushEvent
	};

	xlog(LOG_SUCCESS, L"[BackendConnectionHandlingThread] listening for new task events...\n");
	
	while ( Sess.IsRunning() )
	{
		//
		// Wait for a push event or a termination notification event
		//
		DWORD dwWaitResult = ::WaitForMultipleObjects(_countof(Handles), Handles, FALSE, INFINITE);

		switch (dwWaitResult)
		{
		case WAIT_OBJECT_0:
		{
#ifdef _DEBUG
			xlog(LOG_DEBUG, L"[BackendConnectionHandlingThread] received termination Event\n");
#endif // _DEBUG

			Sess.Stop();
			continue;
		}

		case WAIT_OBJECT_0 + 1:
		{
			//
			// pop() from request task list. Although the pop is blocking, there should always be
			// some data available (because of the Event)
			//
			auto request_task = Sess.RequestTasks.pop();

			auto response_task = SendTaskToDriver(request_task, hIrpDumperDevice.get());

			//
			// push to response task list
			//
			Sess.ResponseTasks.push(response_task);
			break;
		}

		default:
			PrintErrorWithFunctionName(L"WaitForMultipleObjects()");
			Sess.Stop();
			continue;
		}

	}

#ifdef _DEBUG
	xlog(LOG_DEBUG, L"terminating thread TID=%d\n", GetThreadId(GetCurrentThread()));
#endif // _DEBUG

	return ERROR_SUCCESS;
}



/*++

Routine Description:

Start the thread responsible for the communcation between the broker and the backend (i.e.
the driver).


Arguments:

	lpThread -  a pointer to the handle of the created


Return Value:

	Returns TRUE if the thread was successfully created; FALSE in any other case

--*/
_Success_(return)
BOOL StartBackendManagerThread(_In_ PVOID lpParameter)
{
	HANDLE hThread;
	DWORD dwThreadId;

	//
	// Starts the thread that communicates with the driver
	//
	hThread = ::CreateThread(
		NULL,
		0,
		BackendConnectionHandlingThread,
		lpParameter,
		CREATE_SUSPENDED,
		&dwThreadId
	);

	if (!hThread)
	{
		PrintError(L"CreateThread(Driver)");
		return FALSE;
	}

	
#ifdef _DEBUG
	xlog(LOG_DEBUG, "CreateThread(Driver) created as TID=%d\n", dwThreadId);
#endif // _DEBUG


	Session& Sess = *(reinterpret_cast<Session*>(lpParameter));
	Sess.m_hBackendThread = hThread;


	//
	// Starts the thread that collects the IRP from the driver
	//
	hThread = ::CreateThread(
		NULL,
		0,
		IrpCollectorThreadRoutine,
		lpParameter,
		CREATE_SUSPENDED,
		&dwThreadId
	);

	if (!hThread)
	{
		PrintError(L"CreateThread(Irp)");
		return FALSE;
	}

#ifdef _DEBUG
	xlog(LOG_DEBUG, "CreateThread(Irp) created as TID=%d\n", dwThreadId);
#endif // _DEBUG


	Sess.m_hIrpFetcherThread = hThread;

	return TRUE;
}