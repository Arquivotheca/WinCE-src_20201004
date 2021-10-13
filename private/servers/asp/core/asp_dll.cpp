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
// asp_dll.cpp : Implementation of DLL Exports.


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

#if 0
// Since .rgs files aren't included in the resource, no point in bringing these exports in.

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
#endif // 0


