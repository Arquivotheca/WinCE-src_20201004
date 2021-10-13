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

// av_dll.cpp : Implementation of DLL Exports.


#include <atlbase.h>
//You may derive a class from CComModule and use it if you want to override
//something, but do not change the name of _Module
CComModule _Module;
#include <atlcom.h>
#include "cetls.h"

#include "av_dcp.h"

#include "av_dcp_i.c"


BEGIN_OBJECT_MAP(DeviceMap)
END_OBJECT_MAP()

/////////////////////////////////////////////////////////////////////////////
// DLL Entry Point

extern "C"
BOOL WINAPI DllMain(IN PVOID hInstance, IN ULONG dwReason, IN PVOID Context OPTIONAL)
{
    if (dwReason == DLL_THREAD_ATTACH)
    {
        CeTlsThreadAttach();
    }   
    else if (dwReason == DLL_THREAD_DETACH)
    {
        CeTlsThreadDetach();
    }   
    else if (dwReason == DLL_PROCESS_ATTACH)
    {
        if (!CeTlsProcessAttach())
        {
            return FALSE;
        }
        _Module.Init(DeviceMap, (HINSTANCE)hInstance, &LIBID_UPNPAVTOOLKITlib);
    }
    else if (dwReason == DLL_PROCESS_DETACH)
    {
        _Module.Term();
        CeTlsProcessDetach();
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
