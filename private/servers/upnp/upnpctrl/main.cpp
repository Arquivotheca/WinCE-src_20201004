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
#include <windows.h>
#include <Winsock2.h>
#include <upnp.h>
#include <bldver.h>

#define DLLSVC  1       // for combook to use in-proc COM
#include <combook.h>
#include <combook.cpp>  // include only in this file

#include "main.h"

#include "finder.h"
#include "DeviceDescription.h"
#include "ServiceImpl.h"
#include "HttpRequest.h"
#include "url_verifier.h"
#include "upnp_config.h"
#include "Dlna.h"
#include "cetls.h"

namespace ce
{
    HINSTANCE g_hInstance;
};

url_verifier* g_pURL_Verifier;

// map CLSID to GetClassObject/UpdateRegistry routine
BEGIN_COCLASS_TABLE(ClassTable)
    COCLASS_TABLE_ENTRY(&CLSID_UPnPDeviceFinder, CUPnPDeviceFinder::GetClassObject, CUPnPDeviceFinder::UpdateRegistry)
    COCLASS_TABLE_ENTRY(&CLSID_UPnPDescriptionDocument, DeviceDescription::GetClassObject, DeviceDescription::UpdateRegistry)
END_COCLASS_TABLE()

// implement ModuleAddRef/ModuleRelease/ModuleIsStopping/ModuleIsIdle
IMPLEMENT_DLL_MODULE_ROUTINES()

// implement DllGetClassObject/DllCanUnloadNow/Dll[Un]RegisterServer
IMPLEMENT_DLL_ENTRY_POINTS(ce::g_hInstance, ClassTable, 0, TRUE)


#if defined (DEBUG) || defined (_DEBUG)

DWORD dwData = 'UPNP';

DBGPARAM dpCurSettings =
{
    TEXT("UPnPCtrl"),
    {
        TEXT("Misc"),
        TEXT("Init"),
        TEXT("Enum"),
        TEXT("Finder"),
        TEXT("Device"),
        TEXT("Devices"),
        TEXT("Service"),
        TEXT("Services"),
        TEXT("Document"),
        TEXT("Callback"),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT("Trace"),
        TEXT("Error")
    },
    0x00008000
};

#endif

// init_reg
void init_reg()
{
    int scope = upnp_config::scope();

    if(scope >= 1)
    {
        g_pURL_Verifier->allow_private();
    }

    if(scope >= 2)
    {
        g_pURL_Verifier->allow_ttl(upnp_config::TTL());
    }

    if(scope >= 3)
    {
        g_pURL_Verifier->allow_any();
    }

    g_pURL_Verifier->allow_site_scope(upnp_config::site_scope());
}


// DllMain
extern "C"
BOOL WINAPI DllMain(IN PVOID DllHandle, IN ULONG Reason, IN PVOID Context OPTIONAL)
{
    switch (Reason)
    {
    case DLL_THREAD_ATTACH:
        CeTlsThreadAttach();
        break;

    case DLL_THREAD_DETACH:
        CeTlsThreadDetach();
        break;

    case DLL_PROCESS_ATTACH:

        if(!UpnpHeapCreate())
        {
            return FALSE;
        }

        if (!CeTlsProcessAttach())
        {
            UpnpHeapDestroy();
            return FALSE;
        }

        ce::g_hInstance = (HINSTANCE)DllHandle;
        DEBUGREGISTER((HINSTANCE)DllHandle);

#if defined(DEBUG) || defined(_DEBUG)
        svsutil_Initialize ();
        svsutil_SetAllocData(&dwData, &dwData);
#endif

        // initialize winsock
        {
            WORD wVersionRequested = MAKEWORD(2, 0);
            WSADATA wsadata;

            WSAStartup(wVersionRequested, &wsadata);
        }

        // init HttpBase
        if(!HttpBase::Initialize("Microsoft-Windows-CE/%d.%02d UPnP control point DLNADOC/%d.%02d", CE_MAJOR_VER, CE_MINOR_VER, DLNA_MAJOR_VER, DLNA_MINOR_VER))
        {
            return FALSE;
        }

        // create connection point object
        if(!(g_pConnectionPoint = new ConnectionPoint))
        {
            HttpBase::Uninitialize();
            return FALSE;
        }

        if(!(g_pURL_Verifier = new url_verifier))
        {
            HttpBase::Uninitialize();
            delete g_pConnectionPoint;
            g_pConnectionPoint = NULL;
            return FALSE;
        }

        // read registry settings
        init_reg();

        break;

    case DLL_PROCESS_DETACH:

        // destroy connection point object
        delete g_pConnectionPoint;

        delete g_pURL_Verifier;
        
        // destroy Cookie jars
        g_CJSink.GlobalShutdown();
        g_CJHttpRequest.GlobalShutdown();
        g_CJHttpChannel.GlobalShutdown();

        // uninit HttpBase
        HttpBase::Uninitialize();

        // cleanup winsock
        WSACleanup();

#if defined(DEBUG) || defined(_DEBUG)
        DEBUGMSG(1, (L"\nUNFREED MEMORY = %d bytes\n\n", svsutil_TotalAlloc()));
        svsutil_LogCallStack();
        ASSERT(0 == svsutil_TotalAlloc());
        svsutil_DeInitialize();
#endif

        CeTlsProcessDetach();
        UpnpHeapDestroy();
        break;

    }

    return TRUE;
}
