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
// File:    soapreaderdll.cpp
// 
// Contents:
//
//  implementation file 
//
//		ISoapReaderDll implemenation
//	
//
//-----------------------------------------------------------------------------
#include "headers.h"


REGENTRY g_SoapReaderRegistryEntries[] = /* updated */
{
    // SoapReader class and typelibs                        
    { "MSSOAP.SoapReader",  0, { REG_SZ, "Microsoft Soap SoapReader class", 0 }, 
        REG_ROOT, REG_DEFAULT },
        
        { "Clsid",  0, { REG_SZ, REGKEY(CLSID_SOAPREADER), 0 }, 
            REG_ROOT+1, REG_DEFAULT },
        { "CurVer",  0, { REG_SZ, "MSSOAP.SoapReader.1", 0 }, 
            REG_ROOT+1, REG_DEFAULT },
            
    { "MSSOAP.SoapReader.1",  0, { REG_SZ, "Microsoft Soap SoapReader class Version 1" , 0 }, 
        REG_ROOT, REG_DEFAULT },
        
        { "Clsid",  0, { REG_SZ, REGKEY(CLSID_SOAPREADER), 0 }, 
            REG_ROOT+1, REG_DEFAULT },
        
	{ "CLSID", 0, { 0, NULL, 0 }, 
	    REG_ROOT, REG_EXISTING | UNREG_OPEN},
	    
        { REGKEY(CLSID_SOAPREADER),  0, { REG_SZ, "MSSOAP.DLL SoapReader class", 0 }, 
            REG_ROOT+1, REG_DEFAULT   },
            
            { "InProcServer32", 0, { REG_SZ, "%s", 0 }, 
                REG_ROOT+2, REG_DEFAULT  },
            { "InProcServer32", "ThreadingModel", { REG_SZ, "Both", 0 }, 
                REG_ROOT+2, REG_DEFAULT  },
            { "ProgID",  0, { REG_SZ, "MSSOAP.SoapReader.1", 0 }, 
                REG_ROOT+2, REG_DEFAULT  },
            { "VersionIndependentProgID", 0, { REG_SZ, "MSSOAP.SoapReader", 0 }, 
                REG_ROOT+2, REG_DEFAULT  },
            { "TypeLib",  0, { REG_SZ, REGKEY(LIBID_MSSOAP_TYPELIB), 0 }, 
                REG_ROOT+2, REG_DEFAULT  },
      
};


STDAPI SoapReader_UnregisterServer(void)
{
#ifndef CE_NO_EXCEPTIONS
    try
#else
    __try
#endif 
    {
        UnregisterRegEntry(g_SoapReaderRegistryEntries, countof(g_SoapReaderRegistryEntries), HKEY_CLASSES_ROOT);
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


STDAPI SoapReader_RegisterServer(CHAR *pModName)
{
#ifndef CE_NO_EXCEPTIONS
    try
#else
    __try
#endif     
    {
        // no UnregisterServer here - the caller did it already 
        RegisterRegEntry(g_SoapReaderRegistryEntries, countof(g_SoapReaderRegistryEntries), pModName, 0, HKEY_CLASSES_ROOT);
        return S_OK;        
    }
#ifndef CE_NO_EXCEPTIONS    
    catch (...)
#else
    __except(1)
#endif      
    {
        SoapReader_UnregisterServer();
        return E_FAIL;
    }
  
}


