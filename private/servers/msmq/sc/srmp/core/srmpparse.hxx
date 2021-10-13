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
/*++


Module Name:

    SrmpParse.hxx

Abstract:

    Contains basic parsing related functions.


--*/
#if ! defined (__srmpparse_HXX__)
#define __srmpparse_HXX__	1

#include <windows.h>
#include "qformat.h"

BOOL IsUriHttp(const WCHAR *szQueue);
BOOL IsUriHttps(const WCHAR *szQueue);
BOOL IsUriHttpOrHttps(const WCHAR *szQueue, DWORD dwQueueChars);
BOOL IsUriMSMQ(const WCHAR *szQueue);
BOOL UtlIso8601TimeToSystemTime(const WCHAR *szIso860Time, DWORD cbIso860Time, SYSTEMTIME* pSysTime);
BOOL UtlSystemTimeToCrtTime(const SYSTEMTIME *pSysTime, time_t *pTime);
BOOL UtlIso8601TimeDuration(const WCHAR *szTimeDuration, DWORD cbTimeDuration, time_t *pTime);
BOOL UtlTimeToIso860Time(time_t tm, WCHAR *szBuffer, DWORD ccBuffer);
BYTE* Base642OctetW(LPCWSTR szBase64, DWORD cbBase64, DWORD *pdwOctLen);
LPWSTR Octet2Base64W(const BYTE* OctetBuffer, DWORD OctetLen, DWORD *Base64Len);

extern const WCHAR cszHttpPrefix[];
extern const WCHAR cszHttpsPrefix[];
extern const WCHAR cszMSMQPrefix[];
extern const DWORD ccHttpPrefix;
extern const DWORD ccHttpsPrefix;
extern const DWORD ccMSMQPrefix;
#endif
