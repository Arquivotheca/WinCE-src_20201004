//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------

#include <TUXMAIN.H>

extern const DWORD     BAUD_RATES[];
extern const LPTSTR    SZ_BAUD_RATES[];
extern const DWORD     BAUD_RATES_VAL[];
extern const UINT      NUM_BAUD_RATES;

extern const WORD      DATA_BITS[];
extern const LPTSTR    SZ_DATA_BITS[];
extern const BYTE      DATA_BITS_VAL[];
extern const UINT      NUM_DATA_BITS;

extern const WORD      STOP_BITS[];
extern const LPTSTR    SZ_STOP_BITS[];
extern const BYTE      STOP_BITS_VAL[];
extern const UINT      NUM_STOP_BITS;

extern const WORD      PARITY[];
extern const LPTSTR    SZ_PARITY[];
extern const BYTE      PARITY_VAL[];
extern const UINT      NUM_PARITY;

extern const DWORD     PROVIDER_CAPS[];
extern const LPTSTR    SZ_PROVIDER_CAPS[];
extern const UINT      NUM_PROVIDER_CAPS;

extern const DWORD     SETTABLE_PARAMS[];
extern const LPTSTR    SZ_SETTABLE_PARAMS[];
extern const UINT      NUM_SETTABLE_PARAMS;

extern const DWORD     COMM_EVENTS[];
extern const LPTSTR    SZ_COMM_EVENTS[];
extern const UINT      NUM_COMM_EVENTS;

extern const DWORD     COMM_FUNCTIONS[];
extern const LPTSTR    SZ_COMM_FUNCTIONS[];
extern const UINT      NUM_COMM_FUNCTIONS;

HANDLE Util_OpenCommPort(UINT unCommPort);
UINT Util_QueryCommPortCount(VOID);
