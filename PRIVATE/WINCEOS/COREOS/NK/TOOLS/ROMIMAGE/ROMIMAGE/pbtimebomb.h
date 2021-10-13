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
//////////////////////////////////////////////////////////////////////////////////////////////
//  PB timebomb.  This code compares the MPC in the resource of the current module with     //
//  the known PB Pro MPC in the PB devshl.dll resource.                                     //
//  If PB Pro, then IsMakeImageEnabled returns                                              //
//  TRUE. Any MPC that doesn't match the PB Pro is considered a Trial MPC.  If it is a      //
//  Trial, the number of days remaining in the Trial must be greater than 0.  If equal to or//
//  less than 0, the IsMakeImageEnabled function will return FALSE, otherwise it will return//
//  TRUE (For the sixty dayes).                                                             //
//////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __PBTIMEBOMB_H__
#define __PBTIMEBOMB_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef PBTIMEBOMB

#include <windows.h>
#include <tchar.h>

//If PB setup changes, these defines may need to be updated.
#define PBVERSION "6.00"

#if PBTIMEBOMB == 180
#define TRIALDAYS PBTIMEBOMB
#elif PBTIMEBOMB == 120
#define TRIALDAYS PBTIMEBOMB
#else
#error incorrectly set PBTIMEBOMB evironment variable
#endif

#define RPC_SIZE            5
#define PID_RES_NUM         20000
#define PID_RES_TYPE        20001
#define PID_BUFFER_SIZE     149
#define cbCDEncrypt    128  /* encrypted byte count in SETUP.INI */


BOOL IsRomImageEnabled();

#endif

#ifdef __cplusplus
    }
#endif

#endif  //__PBTIMEBOMB_H__
