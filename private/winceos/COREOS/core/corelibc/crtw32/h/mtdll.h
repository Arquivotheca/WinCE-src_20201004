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
*mtdll.h - DLL/Multi-thread include
*
*
*Purpose:
*
*       [Internal]
*
****/

#ifndef _INC_MTDLL
#define _INC_MTDLL

#ifdef __cplusplus
extern "C" {
#endif

#include <cruntime.h>

#include <windows.h>

extern CRITICAL_SECTION csStdioInitLock;
extern CRITICAL_SECTION csIobScanLock;

/* macros */

/*
 * TODO: The desktop CRT uses _lock_locale() to control access to locale.
 *       This should be brought over to CE.
 */


#define _lock_str(s)              EnterCriticalSection(&((s)->lock))
#define _lock_str2(i,s)           _lock_str(s)
#define _unlock_str(s)            LeaveCriticalSection(&((s)->lock))
#define _unlock_str2(i,s)         _unlock_str(s)

#ifdef __cplusplus
}
#endif

#endif  /* _INC_MTDLL */

