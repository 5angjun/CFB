#ifndef __UTILS_H__
#define __UTILS_H__

#include "Common.h"

#include <stdio.h>
#include <wchar.h>
#include <stdarg.h>

#pragma once

VOID CfbDbgPrint(const WCHAR* lpFormatString, ...);
VOID CfbHexDump(UCHAR *Buffer, ULONG Length);

#endif /* __UTILS_H__ */
