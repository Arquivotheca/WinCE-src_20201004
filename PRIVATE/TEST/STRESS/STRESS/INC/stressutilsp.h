//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef _STRESSUTILSP_H__
#define _STRESSUTILSP_H__



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


LONG IncrementRunningModuleCount (LPCTSTR tszModule);
LONG DecrementRunningModuleCount (LPCTSTR tszModule);

#endif // _STRESSUTILSP_H__