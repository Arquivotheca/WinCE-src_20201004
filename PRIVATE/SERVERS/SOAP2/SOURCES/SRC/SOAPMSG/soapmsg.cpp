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
//      soapmsg.cpp
//
// Contents:
//
//      Implementation of DLL Exports.
//
//----------------------------------------------------------------------------------
#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif


#include "soaphdr.h"


static const REGENTRY g_SoapMsgRegistryEntries[] = /* updated */
{
        
    { "CLSID", 0, { 0, NULL, 0 }, 
        REG_ROOT, REG_EXISTING | UNREG_OPEN   },
        
        { REGKEY(CLSID_SOAPSERVER),  0, { REG_SZ, "MSSOAP.DLL SoapServer class", 0 }, 
            REG_ROOT+1, REG_DEFAULT },

            { "InProcServer32",  0, { REG_SZ, "%s", 0 }, 
                REG_ROOT+2, REG_DEFAULT },
            { "InProcServer32",  "ThreadingModel", { REG_SZ, "Both", 0 }, 
                REG_ROOT+2, REG_DEFAULT },
            { "ProgID",  0, { REG_SZ, "MSSOAP.SoapServer.1", 0 }, 
                REG_ROOT+2, REG_DEFAULT },
            { "VersionIndependentProgID",  0, { REG_SZ, "MSSOAP.SoapServer", 0 }, 
                REG_ROOT+2, REG_DEFAULT },
            { "TypeLib",  0, { REG_SZ, REGKEY(LIBID_MSSOAP_TYPELIB), 0 }, 
                REG_ROOT+2, REG_DEFAULT },
      
        { REGKEY(CLSID_SOAPCLIENT),  0, { REG_SZ, "MSSOAP.DLL SoapClient class", 0 }, 
            REG_ROOT+1, REG_DEFAULT },
            
            { "InProcServer32",  0, { REG_SZ, "%s", 0 }, 
                REG_ROOT+2, REG_DEFAULT },
            { "InProcServer32",  "ThreadingModel", { REG_SZ, "Both", 0 }, 
                REG_ROOT+2, REG_DEFAULT },
            { "ProgID",  0, { REG_SZ, "MSSOAP.SoapClient.1", 0 }, 
                REG_ROOT+2, REG_DEFAULT },
            { "VersionIndependentProgID",  0, { REG_SZ, "MSSOAP.SoapClient", 0 }, 
                REG_ROOT+2, REG_DEFAULT },
            { "TypeLib",  0, { REG_SZ, REGKEY(LIBID_MSSOAP_TYPELIB), 0 }, 
                REG_ROOT+2, REG_DEFAULT },
  
    { "TypeLib",  0,  { 0, NULL, 0 }, 
        REG_ROOT, REG_EXISTING | UNREG_OPEN   },
        
        { REGKEY(LIBID_MSSOAP_TYPELIB),  0, { REG_SZ, 0, 0 }, 
            REG_ROOT+1, REG_DEFAULT },
            
            { "1.0",  0, { REG_SZ, "Microsoft Soap Type Library (Ver 1.0)", 0 }, 
                REG_ROOT+2, REG_DEFAULT },
                
                { "0",  0, { REG_SZ, 0, 0 }, 
                    REG_ROOT+3, REG_DEFAULT },
                    
                    { "win32", 0, { REG_SZ, "%s", 0 }, 
                        REG_ROOT+4, REG_DEFAULT },

    { "MSSOAP.SoapServer",  0, { REG_SZ, "Microsoft Soap SoapServer class", 0 }, 
        REG_ROOT, REG_DEFAULT },
        
        { "Clsid", 0, { REG_SZ, REGKEY(CLSID_SOAPSERVER), 0 }, 
            REG_ROOT+1, REG_DEFAULT },
        { "CurVer", 0, { REG_SZ, "MSSOAP.SoapServer.1", 0 }, 
            REG_ROOT+1, REG_DEFAULT },
            
    { "MSSOAP.SoapServer.1",  0, { REG_SZ, "Microsoft Soap SoapServer class Version 1" , 0 }, 
        REG_ROOT, REG_DEFAULT },
        
        { "Clsid",  0, { REG_SZ, REGKEY(CLSID_SOAPSERVER), 0 }, 
            REG_ROOT+1, REG_DEFAULT },
            
    { "MSSOAP.SoapClient",  0, { REG_SZ, "Microsoft Soap SoapClient class", 0 }, 
        REG_ROOT, REG_DEFAULT },
        
        { "Clsid",  0, { REG_SZ, REGKEY(CLSID_SOAPCLIENT), 0 }, 
            REG_ROOT+1, REG_DEFAULT },
            
        { "CurVer",  0, { REG_SZ, "MSSOAP.SoapClient.1", 0 }, 
            REG_ROOT+1, REG_DEFAULT },
            
   { "MSSOAP.SoapClient.1",  0, { REG_SZ, "Microsoft Soap SoapClient class Version 1" , 0 }, 
        REG_ROOT, REG_DEFAULT },
        
        { "Clsid",  0, { REG_SZ, REGKEY(CLSID_SOAPCLIENT), 0 }, 
            REG_ROOT+1, REG_DEFAULT },
};


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: SoapMsg_UnregisterServer(void)
//
//  parameters:
//          
//  description:
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDAPI SoapMsg_UnregisterServer(void)
{
#ifndef CE_NO_EXCEPTIONS
    try 
#else
    __try
#endif 
    {

        UnregisterRegEntry(g_SoapMsgRegistryEntries, countof(g_SoapMsgRegistryEntries), HKEY_CLASSES_ROOT);
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
//  function: SoapMsg_RegisterServer(CHAR *pModName)
//
//  parameters:
//          
//  description:
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDAPI SoapMsg_RegisterServer(CHAR *pModName)
{
#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif 
        // no UnregisterServer here - the caller did it already 
        
        RegisterRegEntry(g_SoapMsgRegistryEntries, countof(g_SoapMsgRegistryEntries), pModName, 0, HKEY_CLASSES_ROOT);
        return S_OK;
#ifndef CE_NO_EXCEPTIONS
    }
    catch (...)
    {
        SoapMsg_UnregisterServer();
        return E_FAIL;
    }        
#endif 
}

