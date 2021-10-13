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
//******************************************************************************
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************
/*++

THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name: 

        SPECIALCASES.H

Abstract:

       USB Funciton Loopback Driver -- head file for special test cases 
        
--*/

#pragma once

#include <windows.h>
#include <usbfntypes.h>

#define HOST_TIMING 0
#define DEV_TIMING 1
#define SYNC_TIMING 2

typedef struct  _TIME_REC{
    LARGE_INTEGER   startTime;
    LARGE_INTEGER   endTime;
    LARGE_INTEGER   perfFreq;
}TIME_REC, *PTIME_REC;

typedef struct _PERFDATATRANS{
    PUFN_FUNCTIONS  pUfnFuncs;
    UFN_HANDLE         hDevice;
    PSPTREHADINFO pThread;
    PIPEPAIR_INFO PairInfo; 
    UFN_PIPE Pipe; 
    UFN_PIPE ResultPipe; 
    BOOL    fOUT; 
    USHORT  usRepeat; 
    DWORD  dwBlockSize;
    BOOL fDBlocking;
    int iTiming;
    BOOL fPhysMem;
}PERFDATATRANS, *PPERFDATATRANS;

typedef struct _PERFCALLBACKPARAM{
    HANDLE hAllCompleteEvent;
    UFN_TRANSFER hTransfer;
    UCHAR   uDone;
    PVOID pTimeRec;
}PERFCALLBACKPARAM, *PPERFCALLBACKPARAM;

DWORD WINAPI
PerfDTCallback(LPVOID dParam);

BOOL
IssueContinuousTransfer(
PUFN_FUNCTIONS pUfnFuncs, UFN_HANDLE hDevice, BOOL fOUT, UFN_PIPE hPipe, 
PBYTE  pBuf, USHORT usRepeat, DWORD dwBlockSize);

VOID               
SP_StartPerfDataTrans(PUFL_CONTEXT pContext,
PIPEPAIR_INFO PairInfo, 
UFN_PIPE OutPipe,  UFN_PIPE InPipe, BOOL fOUT,
USHORT usRepeat, DWORD dwBlockSize, BOOL fDBlocking,
int iTiming, BOOL fPhysMem);

DWORD WINAPI 
PerfDataTransThread (LPVOID lpParameter) ;

VOID TimeCounting(PTIME_REC pTimeRec, BOOL bStart);
double CalcAvgTime(TIME_REC TimeRec, USHORT usRounds);
double CalcAvgThroughput(TIME_REC TimeRec, USHORT usRounds, DWORD dwBlockSize);

DWORD WINAPI GenericNotify(LPVOID lpvNotifyParameter);

