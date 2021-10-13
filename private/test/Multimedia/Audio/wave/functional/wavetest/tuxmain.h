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

#include <windows.h>
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

void LOG(LPCWSTR szFmt, ...);

// externs
extern CKato            *g_pKato;
extern SPS_SHELL_INFO   *g_pShellInfo;

/*
 * WAVETEST
 */

#include <MMSYSTEM.H>
#include <MATH.H>

#define MAXNOTHREADS 40
#define MODULE_NAME TEXT("WAVETEST")
#define BEGINTESTPROC \
    if( TPM_QUERY_THREAD_COUNT == uMsg ) { ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0; return SPR_HANDLED; } \
    else if( TPM_EXECUTE != uMsg ) return TPR_NOT_HANDLED;

extern BOOL      g_bSave_WAV_file;
extern HINSTANCE g_hInst;
extern TCHAR*    g_pszWAVFileName;

typedef __int64 timer_type;

DWORD AskUserAboutSoundQuality();
DWORD GetUserEvaluation( const TCHAR *szPrompt );

#endif // __TUXMAIN_H__
