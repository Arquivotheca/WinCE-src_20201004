//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//------------------------------------------------------------------------------
// 
//      Bluetooth Client API Layer
// 
// 
// Module Name:
// 
//      btdrt.cxx
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

