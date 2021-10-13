//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+---------------------------------------------------------------------------------
//
//
// File:
//      mssoapres.cpp
//
// Contents:
//
//      Implementation of Resource only DLL .
//
//----------------------------------------------------------------------------------


#define INIT_SOAP_GLOBALS
#define MYINIT_GUID                 // Includes GUID instances
#include "headers.h"

BOOL    DllProcessAttach(HINSTANCE hInstDLL);
BOOL    DllProcessDetach();

HINSTANCE g_hInstance = 0;


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
//
//  parameters:
//          
//  description:  DLL Entry Point
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef UNDER_CE
extern "C"
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
#else
BOOL APIENTRY DllMain(HANDLE hInstance, DWORD dwReason, LPVOID lpReserved) 
#endif
{
    BOOL        fRetVal = TRUE;        // assume successful.

#ifndef UNDER_CE
    g_hInstance = hInstance;
#else
    g_hInstance = (HINSTANCE)hInstance;
#endif
    switch( dwReason )
    {
        case DLL_PROCESS_ATTACH:
#ifndef UNDER_CE
            fRetVal = DllProcessAttach(hInstance);
#else
            fRetVal = DllProcessAttach((HINSTANCE)hInstance);
#endif
            break;
        case DLL_PROCESS_DETACH:
            fRetVal = DllProcessDetach();
            break;
    }
    return fRetVal;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:  DllProcessAttach(HINSTANCE hInstDLL)
//
//  parameters:
//          
//  description:
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL DllProcessAttach(HINSTANCE hInstDLL)
{

    // Disable the DLL_THREAD_ATTACH and DLL_THREAD_DETACH 
    // notifications for this DLL
    DisableThreadLibraryCalls(hInstDLL);

    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: DllProcessDetach()
//
//  parameters:
//          
//  description:
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL DllProcessDetach()
{

    return TRUE;
}
#ifndef UNDER_CE

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:  DllRegisterServer(void)
//
//  parameters:
//          
//  description:  Adds entries to the system registry
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDAPI DllRegisterServer(void)
{
    HRESULT     hr = S_OK;
    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:  DllUnregisterServer(void)
//
//  parameters:
//          
//  description:  Removes entries from the system registry
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDAPI DllUnregisterServer(void)
{
    HRESULT     hr = S_OK;
    
    return S_OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:  DllCanUnloadNow()
//
//  parameters:
//          
//  description:
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDAPI DllCanUnloadNow()
{
	return S_OK;
}

#endif

extern "C" 
int SoapRC_LoadString(UINT uID, LPTSTR lpBuffer, int nBufferMax)
{
    return LoadString(
        g_hInstance,
        uID,
        lpBuffer,
        nBufferMax);
}
