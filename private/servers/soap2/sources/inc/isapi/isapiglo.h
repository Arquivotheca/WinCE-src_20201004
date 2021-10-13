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
//+---------------------------------------------------------------------------------
//
//
// File:
//      isapiglo.h
//
// Contents:
//
//      SOAP isapi globals
//
//----------------------------------------------------------------------------------

#ifndef _ISAPIGLO_H_INCLUDED
#define _ISAPIGLO_H_INCLUDED

#ifdef INIT_ISAPI_GLOBALS
#ifdef _DEBUG
long        g_connections;
#endif

long        g_cObj;
BOOL        g_fThreadPoolInitialized = FALSE;
BOOL        g_fWorkerQueueInitialized = FALSE;
THREADPOOL* g_pThreadPool;
long        g_cExtensions;
long        g_cThreads;
long        g_cObjCachePerThread;
DWORD       g_cbMaxPost;
SYSTEM_INFO g_si;
bool        g_fIsWin9x = false;
#ifndef UNDER_CE
DWORD       g_dwNaglingFlags = 0;
#endif 

CRITICAL_SECTION g_cs;
#else   //ISAPI_INIT_GLOBALS

#ifdef _DEBUG
extern long g_connections;
#endif
extern long         g_cObj;
extern BOOL         g_fThreadPoolInitialized;
extern BOOL         g_fWorkerQueueInitialized;
extern THREADPOOL*  g_pThreadPool;
extern long         g_cExtensions;
extern long         g_cThreads;
extern long         g_cObjCachePerThread;
extern DWORD        g_cbMaxPost;
extern SYSTEM_INFO  g_si;
#ifndef UNDER_CE
extern bool         g_fIsWin9x;
extern DWORD        g_dwNaglingFlags;
#endif 

extern CRITICAL_SECTION g_cs;
#endif  //ISAPI_INIT_GLOBALS

#define MAX_RES_STRING_SIZE		1024
#define MAX_REQUEST_QUEUE_SIZE 3000

DWORD ProcessRequest(void * pContext);

// Returns the String resource ID that most closely fits the given HRESULT
DWORD HrToMsgId(HRESULT hr);

DWORD GetResourceStringHelper
(
	DWORD	dwMessageId,	// requested message identifier
	LPWSTR	lpBuffer,		// pointer to message buffer
	DWORD	nSize,			// maximum size of message buffer
	va_list	*Arguments		// Arguments to be passed to the message
);

DWORD GetResourceString
(
	DWORD	dwMessageId,	// requested message identifier
	LPWSTR	lpBuffer,		// pointer to message buffer
	DWORD	nSize,			// maximum size of message buffer
    ...                     // Arguments list
);


#ifndef UNDER_CE
// FormatMsg -- Version of FormatMessage that works on all platforms
DWORD WINAPI FormatMsg
	(
	DWORD	dwFlags,		// source and processing options
	LPCVOID	lpSource,		// pointer to message source
	DWORD	dwMessageId,	// requested message identifier
	DWORD	dwLanguageId,	// language identifier for requested message
	LPWSTR	lpBuffer,		// pointer to message buffer
	DWORD	nSize,			// maximum size of message buffer
	va_list	*Arguments		// address of array of message inserts
    );
#endif 

void ForwardToBackslash(char *pstr);
void ForwardToBackslashW(WCHAR *pwstr);

void SafeRelease(IUnknown * ptr, size_t sz);
void SafeFreeLibrary(HINSTANCE h);


#endif  //_ISAPIGLO_H_INCLUDED
