//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
//  
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

