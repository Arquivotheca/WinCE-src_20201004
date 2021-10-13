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

typedef struct _STALLDATATRANS{
    PUFN_FUNCTIONS  pUfnFuncs;
    UFN_HANDLE         hDevice;
    PSPTREHADINFO pThread;
    PIPEPAIR_INFO PairInfo; 
    UFN_PIPE OutPipe; 
    UFN_PIPE InPipe; 
    UCHAR uStallRepeat; 
    USHORT wBlockSize;
}STALLDATATRANS, *PSTALLDATATRANS;

DWORD WINAPI
StallDTCallback(LPVOID dParam);

BOOL
IssueSynchronizedTransfer(
PUFN_FUNCTIONS pUfnFuncs, UFN_HANDLE hDevice, UFN_PIPE hPipe, 
PBYTE  pBuf, USHORT  cbSize, DWORD dwFlags);

BOOL
IssueSynchronizedShortTransfer(
PUFN_FUNCTIONS pUfnFuncs, UFN_HANDLE hDevice, UFN_PIPE hPipe, 
PBYTE  pBuf, USHORT  cbSize, USHORT cbExpSize, DWORD dwFlags);


VOID               
SP_StartDataTransWithStalls(
PUFL_CONTEXT pContext,
PIPEPAIR_INFO PairInfo, 
UFN_PIPE OutPipe, 
UFN_PIPE InPipe, 
UCHAR uStallRepeat, 
USHORT wBlockSize);

DWORD WINAPI 
StallDataTransThread (LPVOID lpParameter) ;

VOID               
SP_StartDataShortTrans(PUFL_CONTEXT pContext, PIPEPAIR_INFO PairInfo, 
UFN_PIPE OutPipe,  UFN_PIPE InPipe);


DWORD WINAPI 
ShortDataTransThread (LPVOID lpParameter);

