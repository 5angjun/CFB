#include <Windows.h>
#include <stdio.h> 

#include "stdafx.h"

#include "../Common/common.h"
#include "client.h"
#include "utils.h"
#include "device.h"


static BOOLEAN g_DoRun = FALSE;


/**
 *
 */
VOID PrintHelpMenu()
{
	wprintf(
		L"? , help            -- Print this help menu\n"
		L"exit , quit         -- Exit cleanly\n"
		L"hook <DriverName>   -- Add `DriverName' to the list of hooked drivers\n"
		L"unhook <DriverName> -- Remove `DriverName' from the list of hooked drivers\n"
	);
	return;
}


/**
 *
 */
static BOOL CtrlHandler(DWORD fdwCtrlType)
{
	switch (fdwCtrlType)
	{
	case CTRL_C_EVENT:
		xlog(LOG_INFO, L"Received Ctrl-C event, stopping...\n");
		g_DoRun = FALSE;
		return TRUE;

	case CTRL_CLOSE_EVENT:
		xlog(LOG_INFO, L"Received Ctrl-Close, stopping...\n");
		g_DoRun = FALSE;
		return TRUE;

	default:
		return FALSE;
	}
}


/**
*
*/
VOID RunInterpreter()
{
	if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE))
	{
		xlog(LOG_ERROR, L"Failed to install CtrlHandler...\n");
	}

	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);

	if (hStdin == INVALID_HANDLE_VALUE)
	{
		xlog(LOG_CRITICAL, L"GetStdHandle failed: %lu\n", GetLastError());
		return;
	}


	xlog(LOG_INFO, L"Starting main loop...\n");
	g_DoRun = TRUE;

	BYTE lpBufferCommand[256];
	WCHAR lpBufferCommandW[sizeof(lpBufferCommand)];

	while (g_DoRun)
	{
		DWORD dwNumberOfBytesRead;
		LPWSTR* lpCommandEntries;
		DWORD dwNbEntries;

		RtlZeroMemory(lpBufferCommand, sizeof(lpBufferCommand));
		RtlZeroMemory(lpBufferCommandW, sizeof(lpBufferCommandW));

		wprintf(CLI_PROMPT);
		fflush(stdout);

		if (!ReadFile(hStdin, lpBufferCommand, sizeof(lpBufferCommand), &dwNumberOfBytesRead, NULL))
		{
			PrintError(L"ReadFile()");
			g_DoRun = FALSE;
			break;
		}

		size_t szNumConvertedChars = 0;
		mbstowcs_s(&szNumConvertedChars, lpBufferCommandW, sizeof(lpBufferCommandW), lpBufferCommand, _TRUNCATE);

		xlog(LOG_DEBUG, L"Received '%s'\n", lpBufferCommandW);

		StringWStrip((LPWSTR) lpBufferCommandW);

		lpCommandEntries = StringWSplit(lpBufferCommandW, L' ', &dwNbEntries);
		if (!lpCommandEntries)
		{
			xlog(LOG_ERROR, L"Failed to parse the command\n");
			continue;
		}

		if (!wcscmp(lpCommandEntries[0], L"quit") || !wcscmp(lpCommandEntries[0], L"exit"))
		{
			xlog(LOG_INFO, L"Exiting...\n");
			g_DoRun = FALSE;
		}

		else if (!wcscmp(lpCommandEntries[0], L"help") || !wcscmp(lpCommandEntries[0], L"?"))
		{
			PrintHelpMenu();
		}

		else if (!wcscmp(lpCommandEntries[0], L"hook"))
		{
			if (dwNbEntries != 2) {
				xlog(LOG_ERROR, L"hook command expects 1 argument only\nExample: hook tcpip\n");
			} 
			else
			{
				LPWSTR lpDriver = lpCommandEntries[1];
				xlog(LOG_DEBUG, L"Trying to hook '%s'\n", lpDriver);

				if (!HookDriver(lpDriver))
				{
					PrintError(L"HookDriver()");
				}
				else
				{
					xlog(LOG_SUCCESS, L"Driver object '%s' is now hooked\n", lpDriver);
				}
			}
			
		}
		else if (!wcscmp(lpCommandEntries[0], L"unhook"))
		{
			if (dwNbEntries != 2) {
				xlog(LOG_ERROR, L"unhook command expects 1 argument only\nExample: unhook tcpip\n");
			}
			else
			{
				LPWSTR lpDriver = lpCommandEntries[1];
				xlog(LOG_DEBUG, L"Trying to unhook '%s'\n", lpDriver);

				if (!HookDriver(lpDriver))
				{
					PrintError(L"UnhookDriver()");
				}
				else
				{
					xlog(LOG_SUCCESS, L"Driver object '%s' is now unhooked\n", lpDriver);
				}
			}
		}
		else
		{
			xlog(LOG_ERROR, L"Unknown command '%s'\n", lpCommandEntries[0]);
		}

		FreeAllSplittedElements(lpCommandEntries, dwNbEntries);
	}

	return;
}