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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#ifndef _STRESSUTILSP_H__
#define _STRESSUTILSP_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


// Module duration

#define MAX_TIME					0xFFFFFFFF
#define MODULE_LIFE_INDEFINITE		0xFFFFFFFF
#define MODULE_LIFE_MAX_SEC			0xFFFFF000
#define MODULE_LIFE_MAX_MINUTES		0xFFFF05A0


#define RESULTS_MMDATA				L"CEStressModuleResults"

/*
Logging levels

0-15	Logging zone
16-19	Logging level
20-23	Harness logging level
24-31   --- unused ---
*/

#define LOG_ZONE_MASK				0x0000FFFF
#define LOG_LEVEL_MASK				0x000F0000
#define HARNESS_LOG_MASK			0x00F00000


#define MASK_LOG_ZONE(dw)			(dw & LOG_ZONE_MASK)
#define MASK_LOG_LEVEL(dw)			((UINT) ((dw & LOG_LEVEL_MASK) >> 16))
#define MASK_HARNESS_LOG(dw)		((UINT) ((dw & HARNESS_LOG_MASK) >> 20))

#define ADD_LOG_ZONE(dw, zone)		((dw & ~LOG_ZONE_MASK) | (0x0000FFFF & zone))
#define ADD_LOG_LEVEL(dw, log)		((dw & ~LOG_LEVEL_MASK) | ((0x0000000F & log) << 16))
#define ADD_HARNESS_LOG(dw, log)	((dw & ~HARNESS_LOG_MASK) | ((0x00000003 & log) << 20))

/*
Additonal info

0-7		Module slot assignement
8-11	Break level
12-31   --- unused ---
*/

#define SLOT_MASK					0x000000FF
#define BREAK_MASK					0x00000F00

#define MASK_SLOT(dw)				((UINT) (dw & SLOT_MASK))
#define MASK_BREAK_LEVEL(dw)		((UINT) ((dw & BREAK_MASK) >> 8))

#define ADD_SLOT(dw, slot)			((dw & ~SLOT_MASK) | (0x000000FF & slot))
#define ADD_BREAK_LEVEL(dw, ubreak)	((dw & ~BREAK_MASK) | ((0x0000000F & ubreak) << 8))

//	Macro used for GetProcAddress
//	Windows CE uses unicode text in GPA, the desktop uses ANSI
#ifdef UNDER_CE
#define _GPA_TEXT(x) L##x
#else
#define _GPA_TEXT(x) x
#endif


LONG IncrementRunningModuleCount (LPCTSTR tszModule);
LONG DecrementRunningModuleCount (LPCTSTR tszModule);
void GetTerminateEventName (LPTSTR tsz, LPTSTR tszId=NULL);
void GetResultsFileName (LPTSTR tsz, LPTSTR tszId=NULL);
BOOL ParseCmdLine_WinMain_I (LPTSTR tszCmdLine, MODULE_PARAMS* pmp);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // _STRESSUTILSP_H__