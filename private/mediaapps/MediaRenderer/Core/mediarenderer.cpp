//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

// MediaRenderer.cpp : Implementation of DLL Exports.

#include "stdafx.h"

#include <initguid.h>
#include "MediaRenderer_i.c"

CComModule _Module;

BEGIN_OBJECT_MAP(DeviceMap)
    OBJECT_ENTRY(CLSID_Device, CDevice)
END_OBJECT_MAP()


// Debug zones for this module
DBGPARAM dpCurSettings =
{
    TEXT("UPnPDevice.Device"),
    {
        ZONE_AV_ERROR_NAME,
        ZONE_AV_WARN_NAME,
        ZONE_AV_TRACE_NAME,
        L"",
        L"",
        L"",
        L"",
        L"",
        L"",
        L"",
        L"",
        L"",
        TEXT("AVTransport"),
        TEXT("Trace"),
        TEXT("Warning"),
        TEXT("Error")
    },

    DEFAULT_AV_ZONES | DEFAULT_ZONES
};

/////////////////////////////////////////////////////////////////////////////
// DLL Entry Point

extern "C"
BOOL WINAPI DllMain(IN PVOID hInstance, IN ULONG dwReason, IN PVOID Context OPTIONAL)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        _Module.Init(DeviceMap, (HINSTANCE)hInstance, &LIBID_UPNPMediaRendererLib);
        DisableThreadLibraryCalls((HINSTANCE)hInstance);

        DEBUGREGISTER((HINSTANCE)hInstance);

        DEBUGMSG(ZONE_TRACE, (TEXT("MediaRenderer: Coming to life.")));
        // init code here
        
        srand(GetTickCount());
    }
    else if (dwReason == DLL_PROCESS_DETACH)
    {
        _Module.Term();

        DEBUGMSG(ZONE_TRACE, (TEXT("MediaRenderer: Cleaning up.")));
        // cleanup code here

    }   
    return TRUE;    // ok
}

/////////////////////////////////////////////////////////////////////////////
// Used to determine whether the DLL can be unloaded by OLE

STDAPI DllCanUnloadNow(void)
{
    return (_Module.GetLockCount()==0) ? S_OK : S_FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// Returns a class factory to create an object of the requested type

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    return _Module.GetClassObject(rclsid, riid, ppv);
}

/////////////////////////////////////////////////////////////////////////////
// DllRegisterServer - Adds entries to the system registry

STDAPI DllRegisterServer(void)
{
    return _Module.RegisterServer(TRUE);
}

/////////////////////////////////////////////////////////////////////////////
// DllUnregisterServer - Removes entries from the system registry

STDAPI DllUnregisterServer(void)
{
    return _Module.UnregisterServer(TRUE);
}


