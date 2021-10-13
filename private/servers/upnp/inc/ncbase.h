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
//+---------------------------------------------------------------------------
//
//  Microsoft Windows
//
//  File:       N C B A S E . H
//
//  Contents:   Basic common code.
//
//  Notes:      Pollute this under penalty of death.
//
//  Author:     shaunco   20 Sep 1997
//
//----------------------------------------------------------------------------

#pragma once
#ifndef _NCBASE_H_
#define _NCBASE_H_

#include "ncdefine.h"   // for NOTHROW
//#include "ncstring.h"   // For string functions
#include <unknwn.h>     // For IUnknown

ULONG
AddRefObj (
    IUnknown* punk) NOTHROW;

ULONG
ReleaseObj (
    IUnknown* punk) NOTHROW;


DWORD
DwWin32ErrorFromHr (
    HRESULT hr) NOTHROW;


inline
BOOL
FDwordWithinRange (
    DWORD   dwLower,
    DWORD   dw,
    DWORD   dwUpper)
{
    return ((dw >= dwLower) && (dw <= dwUpper));
}


HRESULT
HrFromLastWin32Error () NOTHROW;


HRESULT
HrGetProcAddress (
    HMODULE     hModule,
    PCSTR       pszaFunction,
    FARPROC*    ppfn);

HRESULT
HrLoadLibAndGetProcs (
    PCTSTR          pszLibPath,
    UINT            cFunctions,
    const PCSTR*    apszaFunctionNames,
    HMODULE*        phmod,
    FARPROC*        apfn);

inline
HRESULT
HrLoadLibAndGetProc (
    PCTSTR      pszLibPath,
    PCSTR       pszaFunctionName,
    HMODULE*    phmod,
    FARPROC*    ppfn)
{
    return HrLoadLibAndGetProcs (pszLibPath, 1, &pszaFunctionName, phmod, ppfn);
}

HRESULT
HrGetProcAddressesV(
    HMODULE hModule, ...);

HRESULT
HrLoadLibAndGetProcsV(
    PCTSTR      pszLibPath,
    HMODULE*    phModule,
    ...);

HRESULT
HrCreateEventWithWorldAccess(PCWSTR pszName, BOOL fManualReset,
        BOOL fInitialState, BOOL* pfAlreadyExists, HANDLE* phEvent);

HRESULT
HrCreateMutexWithWorldAccess(PCWSTR pszName, BOOL fInitialOwner,
        BOOL* pfAlreadyExists, HANDLE* phMutex);

BOOL FFileExists(LPTSTR pszFileName);

LONGLONG GenerateUUID64(void);

#endif // _NCBASE_H_

