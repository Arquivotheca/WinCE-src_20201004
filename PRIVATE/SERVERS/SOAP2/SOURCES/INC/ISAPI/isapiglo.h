//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
