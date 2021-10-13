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
#ifndef __MAIN_H__
#define __MAIN_H__

//
// Include any headers needed by this test group
//
#include <windows.h>
#include <tchar.h>
#include <tux.h>
#include <katoex.h>
#include <cred.h>

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

#define HKEY_CRED_PROV_PRIMITIVE   TEXT("Comm\\Security\\Credman\\Types\\Primitive")
#define HKEY_CRED_PROV_VIRTUAL   TEXT("Comm\\Security\\Credman\\Types\\Virtual")
#define HKEY_CRED_DLL   TEXT("dll")
#define HKEY_CRED_V_ID   TEXT("ids")
#define MAX_USER_PRIM_CREDS      10  // Confiure it. 10 is reasonable
#define MAX_USER_VIRT_CREDS       5  // Confiure it. 5 is reasonable
#define MAX_CREDS                30  // Confiure it. 30 is reasonable

#define CRED_TYPES_PRIMITIVE      1
#define CRED_TYPES_VIRTUAL        2
#define CRED_TYPES_ALL            3

#define countof(x) (sizeof(x)/sizeof(*(x)))


#endif // __MAIN_H__
