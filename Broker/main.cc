/*++

Module Name:
   main.c

   
Abstract:

This is the main file for the broker.

The broker is a privileged process (must have at least SeDebug and SeLoadDriver) and is responsible 
for communicating with the IrpDumper driver by:
- create the service and load the driver
- instructing the driver to add/remove hooks to specific driver(s)
- fetching IRP data from the hooked drivers
- upon cleanup the resources, unload the driver and delete the service.


--*/


#include "main.h"


static HANDLE g_hTerminationEvent;


/*++

Routine Description:

Attempts to acquire a privilege by its name.


Arguments:

	lpszPrivilegeName - the name (as a wide string) of the privilege


Return Value:

	Returns TRUE if the privilege was successfully acquired

--*/
static BOOL AssignPrivilegeToSelf(_In_ const wchar_t* lpszPrivilegeName)
{
	HANDLE hToken = INVALID_HANDLE_VALUE;
	BOOL bRes = FALSE;
	LUID Luid = { 0, };

	bRes = OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &hToken);
	if (bRes)
	{
		bRes = LookupPrivilegeValue(NULL, lpszPrivilegeName, &Luid);
		if (bRes)
		{
			LUID_AND_ATTRIBUTES PrivAttr = { 0 };
			PrivAttr.Luid = Luid;
			PrivAttr.Attributes = SE_PRIVILEGE_ENABLED;

			TOKEN_PRIVILEGES NewPrivs = { 0, };
			NewPrivs.PrivilegeCount = 1;
			NewPrivs.Privileges[0].Luid = Luid;
			

			bRes = AdjustTokenPrivileges(
				hToken,
				FALSE,
				&NewPrivs,
				sizeof(TOKEN_PRIVILEGES),
				(PTOKEN_PRIVILEGES)NULL,
				(PDWORD)NULL
			) != 0;

			if(bRes)
				bRes = GetLastError() != ERROR_NOT_ALL_ASSIGNED;
		}

		CloseHandle(hToken);
	}

	return bRes;
}


/*++

Routine Description:

Simple helper function to check a privilege by name on the current process.


Arguments:

	lpszPrivilegeName - the name (as a wide string) of the privilege

	lpHasPriv - a pointer to a boolean indicating whether the current process own that 
	privilege


Return Value:

	Returns TRUE if the current has the privilege 

--*/
static BOOL HasPrivilege(_In_ const wchar_t* lpszPrivilegeName, _Out_ PBOOL lpHasPriv)
{
	LUID Luid = { 0, };
	BOOL bRes = FALSE, bHasPriv = FALSE;
	HANDLE hToken = INVALID_HANDLE_VALUE;

	do
	{
		xlog(LOG_DEBUG, L"Checking for '%s'...\n", lpszPrivilegeName);

		bRes = LookupPrivilegeValue(NULL, lpszPrivilegeName, &Luid);
		if (!bRes)
		{
			PrintError(L"LookupPrivilegeValue");
			break;
		}

		LUID_AND_ATTRIBUTES PrivAttr = { 0 };
		PrivAttr.Luid = Luid;
		PrivAttr.Attributes = SE_PRIVILEGE_ENABLED | SE_PRIVILEGE_ENABLED_BY_DEFAULT;

		PRIVILEGE_SET PrivSet = { 0, };
		PrivSet.PrivilegeCount = 1;
		PrivSet.Privilege[0] = PrivAttr;

		bRes = OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &hToken);
		if (!bRes)
		{
			PrintError(L"OpenProcessToken");
			break;
		}

		bRes = PrivilegeCheck(hToken, &PrivSet, &bHasPriv);
		if (!bRes)
		{
			PrintError(L"PrivilegeCheck");
			break;
		}

		*lpHasPriv = bHasPriv;
		bRes = TRUE;
	} 
	while (0);

	if (hToken != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hToken);
	}

	return bRes;
}



/*++

Routine Description:

Ctrl-C handler: when receiving either CTRL_C_EVENT or CTRL_CLOSE_EVENT, set the termination event.


Arguments:

	fdwCtrlType - the event type received by the window


Return Value:

	Returns TRUE if the event was handled. FALSE otherwise.

--*/
static BOOL CtrlHandler(DWORD fdwCtrlType)
{
	switch (fdwCtrlType)
	{
	case CTRL_CLOSE_EVENT:
	case CTRL_C_EVENT:
		xlog(LOG_INFO, L"Received Ctrl-C event, stopping...\n");
		g_bIsRunning = FALSE;
		SetEvent(g_hTerminationEvent);
		return TRUE;

	default:
		return FALSE;
	}
}


/*++

Routine Description:

The entrypoint for the broker.


Arguments:

	argc - 

	argv - 


Return Value:

	Returns 0 on success.

--*/
int wmain(int argc, wchar_t** argv)
{
	int retcode = EXIT_SUCCESS;
	HANDLE hDriver = INVALID_HANDLE_VALUE;
	HANDLE hGui = INVALID_HANDLE_VALUE;
	HANDLE ThreadHandles[2] = { 0 };
	DWORD dwWaitResult = 0;
	g_bIsRunning = FALSE;

	xlog(LOG_INFO, L"Starting %s (part of %s (v%.02f) - by <%s>)\n", argv[0], CFB_PROGRAM_NAME_SHORT, CFB_VERSION, CFB_AUTHOR);

#ifdef _DEBUG
	xlog(LOG_DEBUG, L"DEBUG mode on\n");
#endif

	//
	// Check the privileges: the broker must have SeLoadDriverPrivilege and SeDebugPrivilege
	//
	BOOL bHasDebugPriv = FALSE;
	if (!HasPrivilege(L"SeDebugPrivilege", &bHasDebugPriv) || !bHasDebugPriv)
	{
		xlog(LOG_WARNING, L"Privilege SeDebugPrivilege is missing, trying to acquire...\n");
		if (!AssignPrivilegeToSelf(L"SeDebugPrivilege"))
		{
			xlog(LOG_CRITICAL, L"%s requires SeDebugPrivilege...\n", argv[0]);
			return EXIT_FAILURE;
		}
	}

	BOOL bHasLoadDriverPriv = FALSE;
	if (!HasPrivilege(L"SeLoadDriverPrivilege", &bHasLoadDriverPriv) || !bHasLoadDriverPriv)
	{
		xlog(LOG_WARNING, L"Privilege SeLoadDriverPrivilege is missing, trying to acquire...\n");
		if (!AssignPrivilegeToSelf(L"SeLoadDriverPrivilege"))
		{
			xlog(LOG_CRITICAL, L"%s requires SeLoadDriverPrivilege...\n", argv[0]);
			return EXIT_FAILURE;
		}
	}

	xlog(LOG_SUCCESS, L"Privilege check succeeded...\n");


	//
	// Create the main event to stop the running threads
	//
	g_hTerminationEvent = CreateEvent(
		NULL,
		TRUE,
		FALSE,
		NULL
	);

	if (!g_hTerminationEvent)
	{
		xlog(LOG_CRITICAL, L"Couldn't setup Termination Event()...\n");
		return EXIT_FAILURE;
	}


	//
	// Install the Ctrl-C handler to clean up properly
	//
	if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE))
	{
		xlog(LOG_CRITICAL, L"Could not setup SetConsoleCtrlHandler()...\n");
		return EXIT_FAILURE;
	}
	

	//
	// Extract the driver from the resources
	//
	if (!ExtractDriverFromResource())
	{
		xlog(LOG_CRITICAL, L"Failed to extract driver from resource, aborting...\n");
		return EXIT_FAILURE;
	}

	xlog(LOG_SUCCESS, L"Driver extracted...\n");


	//
	// Create the named pipe the GUI will read data from
	//
	if (!CreateServerPipe())
	{
		retcode = EXIT_FAILURE;
		goto __CreateServerPipeFailed;
	}

	xlog(LOG_SUCCESS, L"Named pipe '%s' created...\n", CFB_PIPE_NAME);

	//
	// Create the service and load the driver
	//
	if (!LoadDriver())
	{
		retcode = EXIT_FAILURE;
		goto __LoadDriverFailed;
	}

	xlog(LOG_SUCCESS, L"Service '%s' loaded and started\n", CFB_SERVICE_NAME);


	g_bIsRunning = TRUE;


	//
	// Start broker <-> driver thread
	//

	if (!StartBackendManagerThread(&g_hTerminationEvent, &hDriver) || hDriver == INVALID_HANDLE_VALUE)
	{
		retcode = EXIT_FAILURE;
		goto __UnsetEnv;
	}

	
	//
	// Start gui <-> broker thread
	//

	if (!StartFrontendManagerThread(&g_hTerminationEvent, &hGui) || hGui == INVALID_HANDLE_VALUE)
	{
		retcode = EXIT_FAILURE;
		goto __UnsetEnv;
	}

	xlog(LOG_SUCCESS, L"Threads started\n");


	//
	// Wait for those 2 threads to finish
	//
	ThreadHandles[0] = hGui;
	ThreadHandles[1] = hDriver;

	dwWaitResult = WaitForMultipleObjects(2, ThreadHandles, TRUE, INFINITE);

	switch (dwWaitResult)
	{
	case WAIT_OBJECT_0:
		xlog(LOG_SUCCESS, L"All threads ended, cleaning up for application exit...\n");
		break;

	default:
		xlog(LOG_ERROR, L"WaitForMultipleObjects failed (%d)\n", GetLastError());
		break;
	}


__UnsetEnv:
	
	if (!UnloadDriver())
	{
		xlog(LOG_CRITICAL, L"A critical error occured while unloading driver\n");
	}
	else
	{
		xlog(LOG_SUCCESS, L"Service '%s' unloaded and deleted\n", CFB_SERVICE_NAME);
	}


__LoadDriverFailed:

	//
	// Flush and stop the pipe, then unload the service, and the driver
	//
	if (!CloseServerPipe())
	{
		xlog(LOG_CRITICAL, L"A critical error occured while closing named pipe\n");
	} 
	else
	{
		xlog(LOG_SUCCESS, L"Closed '%s' unloaded and deleted\n", CFB_PIPE_NAME);
	}

	

__CreateServerPipeFailed:

	if (!DeleteDriverFromDisk())
	{
		xlog(LOG_CRITICAL, L"A critical error occured while deleting driver from disk\n");
	}
	else
	{
		xlog(LOG_SUCCESS, L"Driver deleted\n");
	}


	CloseHandle(g_hTerminationEvent);

	return retcode;
}