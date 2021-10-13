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
//------------------------------------------------------------------------------
// 
//      Bluetooth Client API Layer
// 
// 
// Module Name:
// 
//      btdrtstubs.cxx
// 
// Abstract:
// 
//      This file implements Bluetooth Client API Layer
// 
// 
//------------------------------------------------------------------------------
#include <windows.h>
#include <svsutil.hxx>
#include <bt_buffer.h>
#include <bt_ddi.h>
#include <bt_api.h>
#include <bt_ioctl.h>

// for componentization
extern "C" const BOOL g_fBthApiCOM = FALSE;

extern "C"
#ifdef UNDER_CE
BOOL WINAPI BthApiDllMain(HANDLE hInstance, DWORD dwReason, LPVOID lpReserved)
#else
BOOL WINAPI BthApiDllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
#endif
{
    SVSUTIL_ASSERT(0);
    return FALSE;
}

STDAPI BthApiDllCanUnloadNow(void) {
    SVSUTIL_ASSERT(0);
    return FALSE;
}

STDAPI BthApiDllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    SVSUTIL_ASSERT(0);
    return FALSE;
}

STDAPI BthApiDllRegisterServer(void) {
    SVSUTIL_ASSERT(0);
    return FALSE;
}

STDAPI BthApiDllUnregisterServer(void) {
    SVSUTIL_ASSERT(0);
    return FALSE;
}

