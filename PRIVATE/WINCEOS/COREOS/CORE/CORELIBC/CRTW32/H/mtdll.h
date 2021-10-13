//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

