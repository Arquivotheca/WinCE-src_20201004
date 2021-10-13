//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
// tuxmain.h

// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------
#ifndef _TUXMAIN_H_
#define _TUXMAIN_H_

#ifdef UNDER_CE
#ifdef SUPPORT_IPV6
#include <winsock2.h>
#else
#include <winsock.h>
#endif
#else
#include <winsock2.h>
#endif

#include <WINDOWS.H>
#include <TCHAR.H>
#include <TUX.H>
#include <KATOEX.H>
#include <PERFLOGGERAPI.H>

#define MODULE_NAME     _T("perf_winsock2")

// --------------------------------------------------------------------
// Windows CE specific

#ifdef UNDER_CE
#define Debug NKDbgPrintfW
#ifndef STARTF_USESIZE
#define STARTF_USESIZE     0x00000002
#endif
#ifndef STARTF_USEPOSITION
#define STARTF_USEPOSITION 0x00000004
#endif
#ifndef ZeroMemory
#define ZeroMemory(Destination,Length) memset(Destination, 0, Length)
#endif
#ifndef _vsntprintf
#define _vsntprintf(d,c,f,a) wvsprintf(d,f,a)
#endif
#endif // UNDER_CE

// NT ASSERT macro
#ifdef UNDER_NT
#include <crtdbg.h>
#define ASSERT _ASSERT
#define Debug _tprintf
#endif

// externs
extern CKato            *g_pKato;
extern SPS_SHELL_INFO   *g_pShellInfo;

#endif // _TUXMAIN_H_

