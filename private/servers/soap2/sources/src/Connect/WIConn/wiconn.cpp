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
//+----------------------------------------------------------------------------
//
//
// File:
//      WIConn.cpp
//
// Contents:
//
//      Dll's Main functions and class factory
//
//-----------------------------------------------------------------------------


#include "Headers.h"

#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDAPI CreateConnector(REFCLSID clsid, IUnknown *pUnkOuter, REFIID riid, void **ppvObject)
//
//  parameters:
//          
//  description:
//          Function which creates particular connector
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDAPI CreateConnector(REFCLSID clsid, IUnknown *pUnkOuter, REFIID riid, void **ppvObject)
{
    HRESULT hr = S_OK;
    CSoapAggObject<CWinInetConnector> *pAggConnector = 0;
    CSoapObject<CWinInetConnector>    *pConnector    = 0;

    if (pUnkOuter)
    {
        pAggConnector = new CSoapAggObject<CWinInetConnector>(pUnkOuter, INITIAL_REFERENCE);
        CHK_MEM(pAggConnector);
        CHK(pAggConnector->InnerQueryInterface(riid, ppvObject));
    }
    else
    {
        pConnector = new CSoapObject<CWinInetConnector>(INITIAL_REFERENCE);
        CHK_MEM(pConnector);
        CHK(pConnector->QueryInterface(riid, ppvObject));
    }

Cleanup:
    ::ReleaseInterface((const CSoapAggObject<CWinInetConnector>*&)pAggConnector);
    ::ReleaseInterface((const CSoapObject<CWinInetConnector>*&)pConnector);

    return S_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDAPI DllCanUnloadNow()
//
//  parameters:
//          
//  description:
//          Function to determine whether it is safe to unload the Dll
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDAPI DllCanUnloadNow()
{
    return CanUnloadNow();
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDAPI DllRegisterServer()
//
//  parameters:
//          
//  description:
//          Register server (this connector doesn't register itself, therefore has only an empty function)
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDAPI DllRegisterServer()
{
    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDAPI DllUnregisterServer()
//
//  parameters:
//          
//  description:
//          Unregister server (this connector doesn't register itself, therefore has only an empty function)
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDAPI DllUnregisterServer()
{
    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: BOOL APIENTRY DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
//
//  parameters:
//          
//  description:
//          DllMain
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef UNDER_CE
BOOL APIENTRY DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
#else
BOOL APIENTRY DllMain(HANDLE hInstance, DWORD dwReason, LPVOID lpReserved)
#endif
{
    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH:        
#ifndef UNDER_CE
        DisableThreadLibraryCalls((HMODULE)hInstance);
        g_hInstance = hInstance;
#else
        g_hInstance = (HINSTANCE)hInstance;
#endif
        break;
    default:
        break;
    }
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
