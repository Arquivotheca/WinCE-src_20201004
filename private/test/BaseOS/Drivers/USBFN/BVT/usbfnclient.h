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
#pragma once




//--------------function prototypies----------------------


extern "C"
DWORD
BVT_Init(
    LPCTSTR         pszRegKey
    );

extern "C"
BOOL
BVT_Deinit(
    DWORD               dwCtx);

static
BOOL
WINAPI
USBFNBVT_DeviceNotify(
    PVOID   pvNotifyParameter,
    DWORD   dwMsg,
    DWORD   dwParam
    ); 
extern "C"
DWORD
BVT_Close();
extern "C"
DWORD
BVT_Open(
    DWORD               dwCtx, 
    DWORD               dwAccMode, 
    DWORD               dwShrMode
    );
extern "C"
DWORD
BVT_Read(
    DWORD               dwA,
    LPVOID              lpA,
    DWORD               dwB
    );
extern "C"
DWORD
BVT_Write(
    DWORD               dwA,
    LPVOID              lpA,
    DWORD               dwB
    );
extern "C"
DWORD
BVT_Seek(
    DWORD               dwA,
    LPVOID              lpA,
    DWORD               dwB
    );
extern "C"
BOOL
BVT_IOControl(
    LPVOID              pHidKbd,
    DWORD               dwCode,
    PBYTE               pBufIn,
    DWORD               dwLenIn,
    PBYTE               pBufOut,
    DWORD               dwLenOut,
    PDWORD              pdwActualOut
    );

