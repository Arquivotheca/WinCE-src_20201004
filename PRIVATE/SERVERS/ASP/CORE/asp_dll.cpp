//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// asp.cpp : Implementation of DLL Exports.


// Note: Proxy/Stub Information
//      To build a separate proxy/stub DLL, 
//      run nmake -f aspps.mk in the project directory.

#include "aspmain.h"

CComModule _Module;

BEGIN_OBJECT_MAP(ObjectMap)
OBJECT_ENTRY(CLSID_Request, CRequest)
OBJECT_ENTRY(CLSID_Response, CResponse)
OBJECT_ENTRY(CLSID_Server, CServer)
OBJECT_ENTRY(CLSID_RequestDictionary, CRequestDictionary)
OBJECT_ENTRY(CLSID_RequestStrList, CRequestStrList)
END_OBJECT_MAP()



CRITICAL_SECTION g_scriptCS;


/////////////////////////////////////////////////////////////////////////////
// DLL Entry Point

BOOL WINAPI DllMain(HANDLE hInstance, DWORD dwReason, LPVOID /*lpReserved*/)
{
	switch (dwReason)
	{
		case DLL_PROCESS_ATTACH:
	    {	
			DEBUGREGISTER((HINSTANCE)hInstance);
			_Module.Init(ObjectMap, (HINSTANCE) hInstance);

#ifndef OLD_CE_BUILD
	        DisableThreadLibraryCalls((HMODULE) hInstance);
#endif

			DEBUGMSG(ZONE_INIT,(L"ASP.dll:  Attaching to Process\r\n"));
			svsutil_Initialize(); 
			InitializeCriticalSection(&g_scriptCS);

			if (-1 == (g_dwTlsSlot = TlsAlloc()))
					return FALSE;

		    if (NULL == (g_pScriptSite = new CScriptSite()))
		    	return FALSE;

		    g_pScriptSite->AddRef();   
    	}
    	break;
    	
    	case DLL_PROCESS_DETACH:
    	{
			DEBUGMSG(ZONE_INIT,(L"ASP.dll:  Detaching from process\r\n"));
    		if (g_pScriptSite)
    			g_pScriptSite->Release();


			DeleteCriticalSection(&g_scriptCS);
			TlsFree(g_dwTlsSlot);
    	    _Module.Term();
    	}
    	break;
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
    // registers object, typelib and all interfaces in typelib
    return _Module.RegisterServer(TRUE);
}

/////////////////////////////////////////////////////////////////////////////
// DllUnregisterServer - Removes entries from the system registry

STDAPI DllUnregisterServer(void)
{
	_Module.UnregisterServer();
	return S_OK;
}


