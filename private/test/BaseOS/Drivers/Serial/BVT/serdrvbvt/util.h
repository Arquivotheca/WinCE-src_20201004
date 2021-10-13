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

HANDLE Util_OpenCommPort(INT unCommPort);
UINT Util_QueryCommPortCount(VOID);
UINT Util_QueryUSBSerialPort(VOID);
UINT Util_QueryCommIRRAWPort(VOID);
UINT Util_QueryCommIRCOMMPort(VOID);
UINT Util_SetSerialPort(_In_z_ LPCTSTR *pcmdline , _Out_cap_(cch) TCHAR *pg_lpszCommPort, size_t cch = COM_PORT_SIZE);
VOID Util_SerialPortConfig(VOID);
