//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <Winsock2.h>
#include <upnp.h>

#define DLLSVC	1		// for combook to use in-proc COM
#include <combook.h>
#include <combook.cpp> 	// include only in this file

#include "main.h"

#include "finder.h"
#include "DeviceDescription.h"
#include "ServiceImpl.h"
#include "HttpRequest.h"
#include "url_verifier.h"
#include "upnp_config.h"

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

void *operator new(UINT size)
{
	if(size)
        return g_funcAlloc(size, &dwData);
    else
        return NULL;
}

void operator delete(void *pVoid)
{
	if(pVoid)
        g_funcFree(pVoid, &dwData);
}

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
	0x0000ffff          
};

#endif

// init_reg
void init_reg()
{
    int scope = upnp_config::scope();
        
	if(scope >= 1)
		g_pURL_Verifier->allow_private();
    	
	if(scope >= 2)
		g_pURL_Verifier->allow_ttl(upnp_config::TTL());
    	
	if(scope >= 3)
		g_pURL_Verifier->allow_any();
    
    g_pURL_Verifier->allow_site_scope(upnp_config::site_scope());
}

// DllMain
extern "C"
BOOL WINAPI DllMain(IN PVOID DllHandle, IN ULONG Reason, IN PVOID Context OPTIONAL)
{
    switch (Reason)
    {
    case DLL_PROCESS_ATTACH:

		DisableThreadLibraryCalls((HINSTANCE)DllHandle);

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

		// init HttpRequest
		HttpRequest::Initialize("Windows CE 4.0 UPnP control point");

		// create connection point object
		g_pConnectionPoint = new ConnectionPoint;
		
		g_pURL_Verifier = new url_verifier;
		
		// read registry settings
		init_reg();
        
        break; 

    case DLL_PROCESS_DETACH:

		// destroy connection point object
		delete g_pConnectionPoint;

        delete g_pURL_Verifier;
        
		// uninit HttpRequest
		HttpRequest::Uninitialize();

		// cleanup winsock
		WSACleanup();

#if defined(DEBUG) || defined(_DEBUG)
		DEBUGMSG(1, (L"\nUNFREED MEMORY = %d bytes\n\n", svsutil_TotalAlloc()));  
		svsutil_LogCallStack();
		ASSERT(0 == svsutil_TotalAlloc());
		svsutil_DeInitialize();
#endif
        break;

    }
    
	return TRUE;
}
