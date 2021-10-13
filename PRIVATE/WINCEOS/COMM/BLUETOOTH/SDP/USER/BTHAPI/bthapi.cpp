//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// BthAPI.cpp : Implementation of DLL Exports.


// Note: Proxy/Stub Information
//      To merge the proxy/stub code into the object DLL, add the file 
//      dlldatax.c to the project.  Make sure precompiled headers 
//      are turned off for this file, and add _MERGE_PROXYSTUB to the 
//      defines for the project.  
//
//      If you are not running WinNT4.0 or Win95 with DCOM, then you
//      need to remove the following define from dlldatax.c
//      #define _WIN32_WINNT 0x0400
//
//      Further, if you are running MIDL without /Oicf switch, you also 
//      need to remove the following define from dlldatax.c.
//      #define USE_STUBLESS_PROXY
//
//      Modify the custom build rule for BthAPI.idl by adding the following 
//      files to the Outputs.
//          BthAPI_p.c
//          dlldata.c
//      To build a separate proxy/stub DLL, 
//      run nmake -f BthAPIps.mk in the project directory.

#include "stdafx.h"
#include "resource.h"
#if ! defined (UNDER_CE)
#include <initguid.h>
#endif
#include "BthAPI.h"
#include "dlldatax.h"

#if defined (UNDER_CE) || defined (WINCE_EMULATION)
#include <svsutil.hxx>
#include "bt_sdp.h"
// for componentization
extern "C" const BOOL g_fBthApiCOM = TRUE;
#endif // UNDER_CE

#include "sdplib.h"
#include "SdpNodeList.h"
#include "SdpNodeContainer.h"
#include "SdpRecord.h"
#include "SdpSearch.h"
#include "SdpStream.h"
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
#include "ShellPropSheetExt.h"
#include "BluetoothDevice.h"
#include "BluetoothAuthenticate.h"
#endif


#if ! defined (UNDER_CE)
// On WinCE, sdpuser.dll and btdrt.dll are one.  On NT BT emulation there is no
// btdrt.dll, since all calls to BT core go through RPC and not through
// btdrt shim.  In this case define BthApiDllXXX functions to behave as regular DllXXX com
// exposed functions.
#define BthApiDllMain             DllMain
#define BthApiDllCanUnloadNow     DllCanUnloadNow
#define BthApiDllGetClassObject   DllGetClassObject
#define BthApiDllRegisterServer   DllRegisterServer
#define BthApiDllUnregisterServer DllUnregisterServer
#endif // UNDER_CE


#ifdef _MERGE_PROXYSTUB
extern "C" HINSTANCE hProxyDll;
#endif

CComModule _Module;

BEGIN_OBJECT_MAP(ObjectMap)
OBJECT_ENTRY(CLSID_SdpNodeContainer, CSdpNodeContainer)
OBJECT_ENTRY(CLSID_SdpSearch, CSdpSearch)
OBJECT_ENTRY(CLSID_SdpStream, CSdpStream)
OBJECT_ENTRY(CLSID_SdpRecord, CSdpRecord)
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
OBJECT_ENTRY(CLSID_ShellPropSheetExt, CShellPropSheetExt)
OBJECT_ENTRY(CLSID_BluetoothDevice, CBluetoothDevice)
OBJECT_ENTRY(CLSID_BluetoothAuthenticate, CBluetoothAuthenticate)
#endif
END_OBJECT_MAP()

/////////////////////////////////////////////////////////////////////////////
// DLL Entry Point

extern "C"
#ifdef UNDER_CE
BOOL WINAPI BthApiDllMain(HANDLE hInstance, DWORD dwReason, LPVOID lpReserved)
#else
BOOL WINAPI BthApiDllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
#endif
{
#ifdef _MERGE_PROXYSTUB
    if (!PrxDllMain((HINSTANCE)hInstance, dwReason, lpReserved))
        return FALSE;
#endif
    if (dwReason == DLL_PROCESS_ATTACH)
    {
#if defined (UNDER_CE) || defined (WINCE_EMULATION)
        // Use Atl 2.1 on WinCE

        _Module.Init(ObjectMap, (HINSTANCE)hInstance);
        svsutil_Initialize();
#else
        _Module.Init(ObjectMap, hInstance, &LIBID_BTHAPILib);
        DisableThreadLibraryCalls(hInstance);
        SHFusionInitializeFromModuleID(hInstance, 123);
#endif

#if defined (UNDER_CE) && (_WIN32_WCE >= 300)
        // This call was unavailable before WinCE 3.0
        DisableThreadLibraryCalls((HINSTANCE)hInstance);
#endif
    }
    else if (dwReason == DLL_PROCESS_DETACH) {
        _Module.Term();
	svsutil_DeInitialize ();
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
        SHFusionUninitialize();
#endif
    }


    return TRUE;    // ok
}

/////////////////////////////////////////////////////////////////////////////
// Used to determine whether the DLL can be unloaded by OLE

STDAPI BthApiDllCanUnloadNow(void)
{
#ifdef _MERGE_PROXYSTUB
    if (PrxDllCanUnloadNow() != S_OK)
        return S_FALSE;
#endif
    return (_Module.GetLockCount()==0) ? S_OK : S_FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// Returns a class factory to create an object of the requested type

STDAPI BthApiDllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
#ifdef _MERGE_PROXYSTUB
    if (PrxDllGetClassObject(rclsid, riid, ppv) == S_OK)
        return S_OK;
#endif
    return _Module.GetClassObject(rclsid, riid, ppv);
}

/////////////////////////////////////////////////////////////////////////////
// DllRegisterServer - Adds entries to the system registry

STDAPI BthApiDllRegisterServer(void)
{
#ifdef _MERGE_PROXYSTUB
    HRESULT hRes = PrxDllRegisterServer();
    if (FAILED(hRes))
        return hRes;
#endif

    // registers object, typelib and all interfaces in typelib
    return _Module.RegisterServer(TRUE);
}

/////////////////////////////////////////////////////////////////////////////
// DllUnregisterServer - Removes entries from the system registry

STDAPI BthApiDllUnregisterServer(void)
{
#ifdef _MERGE_PROXYSTUB
    PrxDllUnregisterServer();
#endif

#ifdef UNDER_CE
        // Use Atl 2.1 on WinCE
    return _Module.UnregisterServer();
#else
    return _Module.UnregisterServer(TRUE);
#endif    
}


