//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

#ifndef _radiotest_utils_h
#define _radiotest_utils_h
#include <tux.h>
#include <kato.h>

// Common Kato Logging functions
BOOL WINAPI LoadGlobalKato(void);
BOOL WINAPI CheckKatoInit(void);
//VOID WINAPI Log(LPTSTR szFormat, ...);
VOID WINAPI LogConditionalVerbose(BOOL fVerbose, LPTSTR szFormat, ...);
VOID WINAPI LogWarn(LPTSTR szFormat, ...);
VOID WINAPI LogPass(LPTSTR szFormat, ...);
VOID WINAPI LogFail(LPTSTR szFormat, ...);
VOID WINAPI LogComment(LPTSTR szFormat, ...);
VOID WINAPI Log(LPCTSTR tszFormat, ...);

// For running out of a storage card
extern BOOL    g_fRunFromMMC;
extern TCHAR   g_tszExecutablePath[];
extern TCHAR   g_tszStorageCardRoot[];
extern TCHAR   g_tszPersistentStorageRoot[];
extern TCHAR   g_tszLogFilesPath[];

const UINT64 HUNDRED_NS_IN_ONE_HOUR = 36000000000;     // number of 100ns in an hour
const ULONG  HUNDRED_NS_IN_ONE_SEC  = 10000000;        // number of 100ns in a second

// Time related functions
UINT64 FileTimeToUINT64(const FILETIME * lpFileTime);
void UINT64ToFileTime(const UINT64 time64, FILETIME * lpFileTime);
UINT64 GetSystemTimeUINT64(void);
void UINT64ToSystemTime(const UINT64 time64, SYSTEMTIME * sysTime);
UINT64 GetLocalTimeUINT64(void);
DWORD GetTicksElapsed(const DWORD start);

WCHAR ToHexWCHAR(const BYTE bData);

// Device info stuffs
BOOL GetBuildInfo(
                  DWORD *pdwMajorOSVer,
                  DWORD *pdwMinorOSVer,
                  DWORD *pdwOSBuildNum,
                  DWORD *pdwROMBuildNum
                  );
BOOL GetPlatformName(LPTSTR tszPlatname, DWORD dwMaxStrLen);

BOOL TerminateSMSApps();
BOOL DeleteConnections();
BOOL RestoreDTPTConnection();

// Command line helpers
void CommandLineToArgs(TCHAR szCommandLine[], int *argc, LPTSTR argv[]);
#endif
