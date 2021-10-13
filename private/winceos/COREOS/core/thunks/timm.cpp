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
#define BUILDING_USER_STUBS 1
#include <windows.h>
#include <imm.h>
#include <GweApiSet1.hpp>

extern  GweApiSet1_t*    pGweApiSet1Entrypoints;
extern  GweApiSet2_t*    pGweApiSet2Entrypoints;

extern "C"
HIMC
ImmGetContextFromWindowGwe(
    HWND    hwnd
    )
{
    return pGweApiSet1Entrypoints->m_pImmGetContextFromWindowGwe(hwnd);
}


extern "C"
DWORD
ImmAssociateValueWithGwesMessageQueue(
    DWORD dwValue,
    UINT Flags
    )
{
    return pGweApiSet2Entrypoints->m_pImmAssociateValueWithGwesMessageQueue(
        dwValue,
        Flags
        );
}


extern "C"
BOOL
ImmAssociateContextWithWindowGwe(
    HWND    hwnd,
    HIMC    himc,
    DWORD   dwFlags,
    __out_opt HIMC  *phimcPrev,
    __out_opt HWND  *phwndFocus,
    __out_opt HIMC  *phimcFocusPrev,
    __out_opt HIMC  *phimcFocusNew
    )
{
    return pGweApiSet1Entrypoints->m_pImmAssociateContextWithWindowGwe(
                hwnd,
                himc,
                dwFlags,
                phimcPrev,
                phwndFocus,
                phimcFocusPrev,
                phimcFocusNew);
}


extern "C"
BOOL
ImmSetHotKey(
    DWORD    dwHotKeyId,
    UINT    uModifiers,
    UINT    uVkey,
    HKL        hkl
    )
{
    return pGweApiSet1Entrypoints->m_pImmSetHotKey(dwHotKeyId, uModifiers, uVkey, hkl);
}


extern "C"
BOOL
ImmGetHotKey(
    DWORD    dwHotKeyId,
    __out UINT    *puModifiers,
    __out UINT    *puVkey,
    __out_opt HKL        *phkl
    )
{
    return pGweApiSet1Entrypoints->m_pImmGetHotKey(dwHotKeyId, puModifiers, puVkey, phkl);
}


