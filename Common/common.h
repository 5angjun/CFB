#pragma once

#ifndef _DRIVER
#include <Windows.h>
#include <stdio.h>
#endif // !_DRIVER



#define CFB_PROGRAM_NAME			L"Canadian Fuzzy Bear"
#define CFB_PROGRAM_NAME_SHORT		L"CFB"
#define CFB_AUTHOR					L"@_hugsy_"
#define CFB_VERSION					0.01

#define CFB_USER_DEVICE_NAME		L"\\\\.\\IrpDumper"
#define CFB_DEVICE_NAME				L"\\Device\\IrpDumper"
#define CFB_DEVICE_LINK				L"\\DosDevices\\IrpDumper"

#define CFB_DRIVER_NAME				L"IrpDumper.sys"
#define CFB_SERVICE_NAME			L"IrpDumper"
#define CFB_SERVICE_DESCRIPTION		L"CFB IRP Dumper Driver"
#define CFB_PIPE_NAME				L"\\\\.\\pipe\\CFB"
#define CFB_PIPE_MAXCLIENTS			5
#define CFB_PIPE_INBUFLEN			4096  //TODO : adjust
#define CFB_PIPE_OUTBUFLEN			4096  //TODO : adjust

#define HOOKED_DRIVER_MAX_NAME_LEN	0x104


#ifdef _DEBUG
/* Debug */

#define WIDE2(x) L##x
#define WIDECHAR(x) WIDE2(x)

#define WIDE_FUNCTION WIDECHAR(__FUNCTION__) L"()"
#define WIDE_FILE WIDECHAR(__FILE__)

#define GEN_FMT L"in '%s'(%s:%d) [%d] "
#define __xlog(t, ...) _xlog(t, __VA_ARGS__)
#define xlog(t, _f, ...) __xlog(t, GEN_FMT _f, WIDE_FUNCTION, WIDE_FILE, __LINE__, GetThreadId(GetCurrentThread()), __VA_ARGS__)

#else
/* Release */

#define xlog(t, ...) _xlog(t, __VA_ARGS__)

#endif /* _DEBUG_ */


typedef enum
{
	LOG_DEBUG,
	LOG_INFO,
	LOG_SUCCESS,
	LOG_WARNING,
	LOG_ERROR,
	LOG_CRITICAL
} log_level_t;


typedef struct __hooked_driver_info
{
	BOOLEAN Enabled;
	WCHAR Name[HOOKED_DRIVER_MAX_NAME_LEN];
}
HOOKED_DRIVER_INFO, *PHOOKED_DRIVER_INFO;


__declspec(dllexport) void _xlog(log_level_t level, const wchar_t* format, ...);
__declspec(dllexport) void Hexdump(PVOID data, SIZE_T size);