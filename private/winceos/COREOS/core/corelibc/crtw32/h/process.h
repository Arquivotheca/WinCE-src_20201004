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
/***
*process.h - definition and declarations for process control functions
*
*Purpose:
*       This file defines the modeflag values for spawnxx calls.
*       Also contains the function argument declarations for all
*       process control related routines.
*
****/

#if     _MSC_VER > 1000
#pragma once
#endif

#ifndef _INC_PROCESS
#define _INC_PROCESS

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Security check initialization and failure reporting used by /GS security
 * checks.
 */
void __cdecl __security_init_cookie(void);
#ifdef  _M_IX86
void __fastcall __security_check_cookie(UINT_PTR _StackCookie);
#else
void __cdecl __security_check_cookie(UINT_PTR _StackCookie);
#endif
extern UINT_PTR __security_cookie;

#ifdef __cplusplus
}
#endif

#endif
