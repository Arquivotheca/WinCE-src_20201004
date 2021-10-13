//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------
#ifndef __TUXMAIN_H__
#define __TUXMAIN_H__

#include <WINDOWS.H>
#include <TCHAR.H>
#include <TUX.H>
#include <KATOEX.H>

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

void LOG(LPWSTR szFmt, ...);

// externs
extern CKato            *g_pKato;
extern SPS_SHELL_INFO   *g_pShellInfo;

/*
 * WAVETEST
 */

#include <MMSYSTEM.H>			
#include <MATH.H>

#define MODULE_NAME TEXT("WAVETEST")	
#define BEGINTESTPROC \
    if( TPM_QUERY_THREAD_COUNT == uMsg ) { ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0; return SPR_HANDLED; } \
    else if( TPM_EXECUTE != uMsg ) return TPR_NOT_HANDLED;

extern HINSTANCE g_hInst;

typedef __int64 timer_type;	

#endif // __TUXMAIN_H__
