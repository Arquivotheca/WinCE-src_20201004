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
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2002 BSQUARE Corporation. All rights reserved.
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
//
//
// Main.h
//
//
#ifndef __MAIN_H__
#define __MAIN_H__

////////////////////////////////////////////////////////////////////////////////
// Included files

#include <windows.h>
#include <stdlib.h>
#include <tchar.h>
#include <tux.h>
#include <kato.h>
#include <sdcommon.h>
#include <sd_tst_cmn.h>
#include <clparse.h>

////////////////////////////////////////////////////////////////////////////////
// Suggested log verbosities

#define LOG_EXCEPTION        0
#define LOG_FAIL             2
#define LOG_ABORT            4
#define LOG_SKIP             6
#define LOG_NOT_IMPLEMENTED  8
#define LOG_PASS             10
#define LOG_DETAIL           12
#define LOG_COMMENT          14

////////////////////////////////////////////////////////////////////////////////
//Errors

#define CS_OK                   0
#define CS_No_HC_Or_Bus         1
#define CS_Cant_Get_Slot_Count  2
#define CS_No_Slots             3
#define CS_Too_Many_Slots       4
#define CS_Cant_Get_Slot_Info   5
#define CS_Cant_Get_Card_Type   6
#define CS_WrongType_MMC        7
#define CS_WrongType_SD         8
#define CS_WrongType_SDIO       9
#define CS_WrongType_Combo      10
#define CS_WrongType_Unknown    11
#define CS_Out_Of_Memory        12

#define SDTST_DEVNAME  TEXT("JLP")
#define SDTST_DEVINDEX 1
#define MAX_UINT32     0xffffffff

////////////////////////////////////////////////////////////////////////////////
//Misc Defines
#define GCL_HBRBACKGROUND   (-10)

// macros
#define NO_MESSAGES if (uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED

////////////////////////////////////////////////////////////////////////////////
#endif // __MAIN_H__

