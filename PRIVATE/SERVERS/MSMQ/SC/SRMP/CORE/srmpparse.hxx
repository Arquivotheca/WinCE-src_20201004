//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
BOOL UtlTimeToIso860Time(time_t tm, WCHAR *szBuffer);
BYTE* Base642OctetW(LPCWSTR szBase64, DWORD cbBase64, DWORD *pdwOctLen);
LPWSTR Octet2Base64W(const BYTE* OctetBuffer, DWORD OctetLen, DWORD *Base64Len);

extern const WCHAR cszHttpPrefix[];
extern const WCHAR cszHttpsPrefix[];
extern const WCHAR cszMSMQPrefix[];
extern const DWORD ccHttpPrefix;
extern const DWORD ccHttpsPrefix;
extern const DWORD ccMSMQPrefix;
#endif
