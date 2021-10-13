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
// File:    soapserdll.cpp
// 
// Contents:
//
//  implementation file 
//
//		ISoapSerializerDll implemenation
//	
//
//-----------------------------------------------------------------------------
#include "headers.h"

static const REGENTRY g_SoapSerRegistryEntries[] =
{
    // SoapSerializer class and typelibs                        
    { "MSSOAP.SoapSerializer",  0, { REG_SZ, "Microsoft Soap SoapSerializer class", 0 }, 
        REG_ROOT, REG_DEFAULT },
        
        { "Clsid",  0, { REG_SZ, REGKEY(CLSID_SOAPSERIALIZER), 0 }, 
            REG_ROOT+1, REG_DEFAULT },
        { "CurVer",  0, { REG_SZ, "MSSOAP.SoapSerializer.1", 0 }, 
            REG_ROOT+1, REG_DEFAULT }
            ,
    { "MSSOAP.SoapSerializer.1",  0, { REG_SZ, "Microsoft Soap SoapSerializer class Version 1" , 0 }, 
        REG_ROOT, REG_DEFAULT },

        { "Clsid",  0, { REG_SZ, REGKEY(CLSID_SOAPSERIALIZER), 0 }, 
            REG_ROOT+1, REG_DEFAULT },
        
    { "CLSID",  0, { 0, NULL, 0 }, 
        REG_ROOT, REG_EXISTING | UNREG_OPEN   },
        
        { REGKEY(CLSID_SOAPSERIALIZER),  0, { REG_SZ, "MSSOAP.DLL SoapSerializer class", 0 }, 
            REG_ROOT+1, REG_DEFAULT },
            
            { "InProcServer32",  0, { REG_SZ, "%s", 0 }, 
                REG_ROOT+2, REG_DEFAULT },
            { "InProcServer32",  "ThreadingModel", { REG_SZ, "Both", 0 }, 
                REG_ROOT+2, REG_DEFAULT },
            { "ProgID",  0, { REG_SZ, "MSSOAP.SoapSerializer.1", 0 }, 
                REG_ROOT+2, REG_DEFAULT },
            { "VersionIndependentProgID",  0, { REG_SZ, "MSSOAP.SoapSerializer", 0 }, 
                REG_ROOT+2, REG_DEFAULT },
            { "TypeLib",  0, { REG_SZ, REGKEY(LIBID_MSSOAP_TYPELIB), 0 }, 
                REG_ROOT+2, REG_DEFAULT },
};




STDAPI SoapSer_UnregisterServer(void)
{
#ifndef CE_NO_EXCEPTIONS
    try
#else
    __try
#endif
    {
        UnregisterRegEntry(g_SoapSerRegistryEntries, countof(g_SoapSerRegistryEntries), HKEY_CLASSES_ROOT);
        return S_OK;
    }
#ifndef CE_NO_EXCEPTIONS    
    catch (...)
#else
    __except(1)
#endif   
    {
        return E_FAIL;
    }
 
}




STDAPI SoapSer_RegisterServer(CHAR *pModName)
{
#ifdef CE_NO_EXCEPTIONS
    __try
    {
        if(SUCCEEDED(RegisterRegEntry(g_SoapSerRegistryEntries, countof(g_SoapSerRegistryEntries), pModName, 0, HKEY_CLASSES_ROOT)))
            return S_OK;
        else
        {
            SoapSer_UnregisterServer();
            return E_FAIL;
        }
    }
    __except(1)
    {
        SoapSer_UnregisterServer();
        return E_FAIL;
    }
#else
    try
    {
        // no UnregisterServer here - the caller did it already     
        RegisterRegEntry(g_SoapSerRegistryEntries, countof(g_SoapSerRegistryEntries), pModName, 0, HKEY_CLASSES_ROOT);
        return S_OK;
    }

     catch(...)
    {
        SoapSer_UnregisterServer();
        return E_FAIL;
    }
#endif
}


