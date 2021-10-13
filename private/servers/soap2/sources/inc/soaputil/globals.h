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
//+----------------------------------------------------------------------------
//
//
// File:
//      Globals.h
//
// Contents:
//
//      Global variables and constants declarations
//
//-----------------------------------------------------------------------------


#ifndef __GLOBALS_H_INCLUDED__
#define __GLOBALS_H_INCLUDED__


extern HINSTANCE                g_hInstance;
extern IGlobalInterfaceTable   *g_pGIT; 


#if !defined(UNDER_CE) || defined(DESKTOP_BUILD)
extern HINSTANCE                g_hInstance_language;
extern bool                     g_fIsWin9x;             // true if running under Win9x
#endif 


extern LONG                     g_cLock;
extern LONG                     g_cObjects;
extern IMalloc                 *g_pIMalloc;
extern CErrorCollection         g_cErrorCollection;

// Returns the String resource ID that most closely fits the given HRESULT
DWORD HrToMsgId(HRESULT hr);

DWORD GetResourceStringHelper
(
    DWORD    dwMessageId,    // requested message identifier
    LPWSTR   lpBuffer,       // pointer to message buffer
    DWORD    nSize,          // maximum size of message buffer
    va_list *Arguments       // Arguments to be passed to the message
);

DWORD GetResourceString
(
    DWORD    dwMessageId,    // requested message identifier
    LPWSTR    lpBuffer,      // pointer to message buffer
    DWORD    nSize,          // maximum size of message buffer
    ...
//    va_list    *Arguments  // Arguments to be passed to the message
);

// Converts from MultiByte to WCHAR
int CwchMultiByteToWideChar
    (
    UINT    codepage,        // 0 for default
    char   *pchIn,           // input string
    int     cchIn,           // size of input string
    WCHAR  *pwchOut,         // Output buffer
    int     cwchOut          // size of output buffer
    );

#if !defined(UNDER_CE) || defined(DESKTOP_BUILD)
// FormatMsg -- Version of FormatMessage that works on all platforms
DWORD WINAPI FormatMsg
    (
    DWORD    dwFlags,        // source and processing options
    LPCVOID  lpSource,       // pointer to message source
    DWORD    dwMessageId,    // requested message identifier
    DWORD    dwLanguageId,   // language identifier for requested message
    LPWSTR   lpBuffer,       // pointer to message buffer
    DWORD    nSize,          // maximum size of message buffer
    va_list *Arguments       // address of array of message inserts
    );
#endif 

void ForwardToBackslash(char *pstr);
void ForwardToBackslashW(WCHAR *pwstr);

#endif //__GLOBALS_H_INCLUDED__

