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

//------------------------------------------------------------------------
// 
// Module Name:
//    NetLog.h
// 
// Abstract:
//    Declaration of the NetLog Functions and Class.
// 
// NOTE:
//    The CNetLog interface is deprecated.
// 
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#ifndef __NETLOG_H__
#define __NETLOG_H__

#include <windows.h>
#ifndef UNDER_CE
    #include <stdio.h>
    #include <tchar.h>
    #ifndef DEBUGMSG
    #define DEBUGMSG(_expr,_printf_expr) ((_expr)? _tprintf _printf_expr, 1 : 0)
    #endif
    #ifndef RETAILMSG
    #define RETAILMSG(_expr,_printf_expr) ((_expr)? _tprintf _printf_expr, 1 : 0)
    #endif
#endif
#include <katoex.h>

//------------------------------------------------------------------------
//  Log verbosity flags (see katoex.h for complete list)
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#ifndef LOG_WARNING
#define LOG_WARNING        3
#endif
#ifndef LOG_MAX_VERBOSITY
#define LOG_MAX_VERBOSITY 15
#endif
#ifndef LOG_DEFAULT
#define LOG_DEFAULT       13
#endif

//------------------------------------------------------------------------
//  Object creation flags.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#define NETLOG_DEBUG_OP 0x00000001  // Optional Debug output
#define NETLOG_DEBUG_RQ 0x00000003  // Required Debug output
#define NETLOG_KATO_OP  0x00000004  // Optional Kato output
#define NETLOG_KATO_RQ  0x0000000C  // Required Kato output
#define NETLOG_PPSH_OP  0x00000010  // DEPRECATED - Optional PPSH output
#define NETLOG_PPSH_RQ  0x00000030  // DEPRECATED- -Required PPSH output
#define NETLOG_FILE_OP  0x00000040  // Optional file output
#define NETLOG_FILE_RQ  0x000000C0  // Required file output
#define NETLOG_CON_OP   0x00000100  // Optional console logging
#define NETLOG_CON_RQ   0x00000300  // Required console logging
#define NETLOG_DRV_OP   0x00000400  // DEPRECATED
#define NETLOG_DRV_RQ   0x00000C00  // DEPRECATED
#define NETLOG_DEF      (NETLOG_DEBUG_RQ | NETLOG_CON_OP | NETLOG_KATO_OP)

#ifdef __cplusplus

//------------------------------------------------------------------------
//  The Logging class
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class CNetLog {
public:

    //  Constructors and destructors:
    CNetLog(LPCTSTR lpszName = NULL)                                         { Construct(lpszName, NETLOG_DEF, NULL); }
    CNetLog(LPCTSTR lpszName, LPCTSTR lpszServer = NULL)                     { Construct(lpszName, NETLOG_DEF, lpszServer); }
    CNetLog(DWORD LogStreams, LPCTSTR lpszServer = NULL)                     { Construct(NULL,     LogStreams, lpszServer); }
    CNetLog(LPCTSTR lpszName, DWORD LogStreams, LPCTSTR lpszServer = NULL)   { Construct(lpszName, LogStreams, lpszServer); }

    ~CNetLog(void);

    // Unicode logging:
    BOOL WINAPIV Log (DWORD dwVerbosity, LPCWSTR wszFormat, ...);
    BOOL WINAPI  LogV(DWORD dwVerbosity, LPCWSTR wszFormat, va_list pArgs);

    // ASCII logging:
    BOOL WINAPIV Log (DWORD dwVerbosity, LPCSTR wszFormat, ...);
    BOOL WINAPI  LogV(DWORD dwVerbosity, LPCSTR wszFormat, va_list pArgs);

    // Flush the log stream(s):
    BOOL Flush(void) const;

    // Get or set the current logging verbosity level:
    DWORD GetMaxVerbosity(void) const;
    void  SetMaxVerbosity(DWORD dwVerbosity);

    // Maintained for backwards compatibility:
    DWORD GetLastError(void) const { return ::GetLastError(); }

private:
 
    //  The real constructor:
    BOOL Construct(LPCTSTR wszName, DWORD LogStreams, LPCTSTR wszServer);

    // Logging service:
    HANDLE m_hLogger;
    
};

#endif

//------------------------------------------------------------------------
//  CNetLog C Interface
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
typedef HANDLE HQANET;

#ifdef __cplusplus
extern "C" {
#endif

HQANET NetLogCreateW(LPCWSTR lpszName, DWORD LogStreams, LPCWSTR lpszServer);
BOOL   NetLogDestroy(HQANET hQaNet);

BOOL WINAPIV NetLogW(HQANET hQaNet, DWORD dwVerbosity, LPCWSTR wszFormat, ...);
BOOL WINAPI  NetLogVW(HQANET hQaNet, DWORD dwVerbosity, LPCWSTR wszFormat, va_list pArgs);
BOOL WINAPIV NetLogA(HQANET hQaNet, DWORD dwVerbosity, LPCSTR wszFormat, ...);
BOOL WINAPI  NetLogVA(HQANET hQaNet, DWORD dwVerbosity, LPCSTR wszFormat, va_list pArgs);

BOOL NetLogFlush(HQANET hQaNet, DWORD fFlushType);

DWORD NetLogGetMaxVerbosity(HQANET hQaNet);
BOOL  NetLogSetMaxVerbosity(HQANET hQaNet, DWORD dwVerbosity);

#ifdef __cplusplus
}
#endif

#endif // __NETLOG_H__
