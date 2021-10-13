//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+----------------------------------------------------------------------------
//
//
// File:
//      SoapConn.cpp
//
// Contents:
//
//      Dll's Main functions and class factory
//
//-----------------------------------------------------------------------------

#include "Headers.h"


////////////////////////////////////////////////////////////////////////////////////////////////////
// Registration table
////////////////////////////////////////////////////////////////////////////////////////////////////
static const REGENTRY g_SoapConnRegistryEntries[] =
{
    { "CLSID", 0, { 0, NULL, 0 },
        REG_ROOT, REG_EXISTING | UNREG_OPEN   },
        
        { REGKEY(CLSID_SOAPCONNECTORFACTORY), 0, { REG_SZ, "Soap Port Connector Factory", 0 },
            REG_ROOT + 1, REG_DEFAULT },

            { "InprocServer32", 0, { REG_SZ, "%s", 0 },
                REG_ROOT + 2, REG_DEFAULT },
                
            { "InprocServer32", "ThreadingModel", { REG_SZ, "Both", 0 },
                REG_ROOT + 2, REG_DEFAULT },

            { "ProgID", 0, { REG_SZ, "MSSOAP.ConnectorFactory.1", 0 },
                REG_ROOT + 2, REG_DEFAULT },

            { "VersionIndependentProgID", 0, { REG_SZ, "MSSOAP.ConnectorFactory",  0 },
                REG_ROOT + 2, REG_DEFAULT },

            { "TypeLib", 0, { REG_SZ, REGKEY(LIBID_MSSOAP_TYPELIB), 0 },
                REG_ROOT + 2, REG_DEFAULT },
                
    { "MSSOAP.ConnectorFactory", 0, { REG_SZ, "Soap Port Connector Factory", 0 },
        REG_ROOT, REG_DEFAULT },

        { "Clsid", 0, { REG_SZ, REGKEY(CLSID_SOAPCONNECTORFACTORY), 0 },
            REG_ROOT + 1, REG_DEFAULT },
 
        { "CurVer", 0, { REG_SZ, "MSSOAP.ConnectorFactory.1", 0 },
            REG_ROOT + 1, REG_DEFAULT },

    { "MSSOAP.ConnectorFactory.1", 0, { REG_SZ, "Soap Port Connector Factory", 0 },
            REG_ROOT, REG_DEFAULT },

        { "Clsid", 0, { REG_SZ, REGKEY(CLSID_SOAPCONNECTORFACTORY), 0 },
            REG_ROOT + 1, REG_DEFAULT },

    { "CLSID", 0, { 0, NULL, 0 },
        REG_ROOT, REG_EXISTING | UNREG_OPEN   },

        { REGKEY(CLSID_HTTPCONNECTOR), 0, { REG_SZ, "Microsoft Soap Http Connector", 0 },
            REG_ROOT+1, REG_DEFAULT },

            { "InprocServer32", 0, { REG_SZ, "%s", 0 },
               REG_ROOT+2, REG_DEFAULT },
               
            { "InprocServer32", "ThreadingModel", { REG_SZ, "Both", 0 },
               REG_ROOT+2, REG_DEFAULT },

            { "ProgID", 0, { REG_SZ, "MSSOAP.HttpConnector.1", 0 },
               REG_ROOT+2, REG_DEFAULT },

            { "VersionIndependentProgID", 0, { REG_SZ, "MSSOAP.HttpConnector", 0 },
               REG_ROOT+2, REG_DEFAULT },

            { "TypeLib", 0, { REG_SZ, REGKEY(LIBID_MSSOAP_TYPELIB), 0 },
                REG_ROOT + 2, REG_DEFAULT },
                
    { "MSSOAP.HttpConnector", 0, { REG_SZ, "Microsoft Soap Http Connector", 0 },
        REG_ROOT, REG_DEFAULT },

        { "Clsid", 0, { REG_SZ, REGKEY(CLSID_HTTPCONNECTOR), 0 },
            REG_ROOT+1, REG_DEFAULT },

        { "CurVer", 0, { REG_SZ, "MSSOAP.HttpConnector.1", 0 },
            REG_ROOT+1, REG_DEFAULT },

    { "MSSOAP.HttpConnector.1", 0, { REG_SZ, "Microsoft Soap Http Connector", 0 },
        REG_ROOT, REG_DEFAULT },

        { "Clsid", 0, { REG_SZ, REGKEY(CLSID_HTTPCONNECTOR), 0 },
            REG_ROOT+1, REG_DEFAULT },
};
////////////////////////////////////////////////////////////////////////////////////////////////////

HRESULT SoapConn_DllCanUnloadNow()
{
    return CHttpConnector::CanUnloadNow();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDAPI SoapConn_UnregisterServer(void)
//
//  parameters:
//          
//  description:
//          Unregistration code for Soap Port Connector Factory
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDAPI SoapConn_UnregisterServer(void)
{

#ifndef CE_NO_EXCEPTIONS
    try
#else
    __try
#endif   
    {

        UnregisterRegEntry(g_SoapConnRegistryEntries, countof(g_SoapConnRegistryEntries), HKEY_CLASSES_ROOT);
        return S_OK;
    }

#ifndef CE_NO_EXCEPTIONS
    catch(...)
#else
    __except(1)
#endif    
    {
        return E_FAIL;
    }

}

////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDAPI SoapConn_RegisterServer(CHAR *pModName)
//
//  parameters:
//          
//  description:
//          Registration code for Soap Port Connector Factory
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDAPI SoapConn_RegisterServer(CHAR *pModName)
{
#ifdef CE_NO_EXCEPTIONS
    HRESULT hr = RegisterRegEntry(g_SoapConnRegistryEntries, countof(g_SoapConnRegistryEntries), pModName, 0, HKEY_CLASSES_ROOT);
    if(SUCCEEDED(hr))
        return S_OK;
    else
    {
        SoapConn_UnregisterServer();
        return E_FAIL;
    }
#else
    try 
    {
        // no UnregisterServer here - the caller did it already     
        RegisterRegEntry(g_SoapConnRegistryEntries, countof(g_SoapConnRegistryEntries), pModName, 0, HKEY_CLASSES_ROOT);
        return S_OK;
    }

    catch(...)
    {
        SoapConn_UnregisterServer();
        return E_FAIL;
    }
#endif

}
////////////////////////////////////////////////////////////////////////////////////////////////////


