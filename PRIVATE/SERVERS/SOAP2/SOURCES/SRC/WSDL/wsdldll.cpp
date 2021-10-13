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
// File:    wsdldll.cpp
//
// Contents:
//
//  implementation file
//
//        dll entry point
//
//-----------------------------------------------------------------------------
#include "headers.h"
#include "wsdlread.h"


static const REGENTRY g_RegistryEntries[] =
{
    // WSDLreader class and typelibs
    { "MSSOAP.WSDLReader",  0, { REG_SZ, "Microsoft Soap WSDLReader class", 0 },  REG_ROOT, REG_DEFAULT },
    { "Clsid",  0, { REG_SZ, REGKEY(CLSID_WSDLREADER), 0 },  REG_ROOT+1, REG_DEFAULT   },
    { "CurVer",  0, { REG_SZ, "MSSOAP.WSDLReader.1", 0 },  REG_ROOT+1, REG_DEFAULT   },
    { "MSSOAP.WSDLReader.1",  0,  { REG_SZ, "Microsoft Soap WSDLReader class Version 1" , 0 },  REG_ROOT, REG_DEFAULT   },
    { "Clsid",  0, { REG_SZ, REGKEY(CLSID_WSDLREADER), 0 }, REG_ROOT+1, REG_DEFAULT   },
    { "CLSID",  0, { 0, NULL, 0 },  REG_ROOT, REG_EXISTING | UNREG_OPEN   },
    { REGKEY(CLSID_WSDLREADER),  0, { REG_SZ, "MSSOAP.DLL WSDLReader class", 0 },  REG_ROOT+1, REG_DEFAULT   },
    { "InProcServer32",  0, { REG_SZ, "%s", 0 },  REG_ROOT+2, REG_DEFAULT   },
    { "InProcServer32",  "ThreadingModel", { REG_SZ, "Both", 0 }, REG_ROOT+2, REG_DEFAULT   },
    { "ProgID",  0, { REG_SZ, "MSSOAP.WSDLReader.1", 0 }, REG_ROOT+2, REG_DEFAULT   },
    { "VersionIndependentProgID",  0, { REG_SZ, "MSSOAP.WSDLReader", 0 },  REG_ROOT+2, REG_DEFAULT   },
    { "TypeLib",  0, { REG_SZ, REGKEY(LIBID_MSSOAP_TYPELIB), 0 },  REG_ROOT+2, REG_DEFAULT   },

    // typefactoryclass and typelibs
    { "MSSOAP.SoapTypeMapperFactory", 0, { REG_SZ, "Microsoft SoapTypeMapperFactoryclass", 0 }, REG_ROOT, REG_DEFAULT },
    { "Clsid",  0, { REG_SZ, REGKEY(CLSID_TYPEMAPPERFACT), 0 }, REG_ROOT+1, REG_DEFAULT },
    { "CurVer",  0, { REG_SZ, "MSSOAP.SoapTypeMapperFactory.1", 0 }, REG_ROOT+1, REG_DEFAULT   },
    { "MSSOAP.SoapTypefactory.1",  0,  { REG_SZ, "Microsoft SoapTypeMapperFactory Version 1" , 0 }, REG_ROOT, REG_DEFAULT},
    { "Clsid",  0, { REG_SZ, REGKEY(CLSID_TYPEMAPPERFACT), 0 },  REG_ROOT+1, REG_DEFAULT   },
    { "CLSID",  0, { 0, NULL, 0 },  REG_ROOT, REG_EXISTING | UNREG_OPEN   },
    { REGKEY(CLSID_TYPEMAPPERFACT),  0, { REG_SZ, "MSSOAP.DLL SoapTypeMapperFactory class", 0 },  REG_ROOT+1, REG_DEFAULT   },
    { "InProcServer32",  0, { REG_SZ, "%s", 0 },  REG_ROOT+2, REG_DEFAULT   },
    { "InProcServer32",  "ThreadingModel", { REG_SZ, "Both", 0 }, REG_ROOT+2, REG_DEFAULT   },
    { "ProgID",  0, { REG_SZ, "MSSOAP.SoapTypeMapperFactory.1", 0 }, REG_ROOT+2, REG_DEFAULT   },
    { "VersionIndependentProgID",  0, { REG_SZ, "MSSOAP.SoapTypeMapperFactory", 0 }, REG_ROOT+2, REG_DEFAULT   },
    { "TypeLib",  0, { REG_SZ, REGKEY(LIBID_MSSOAP_TYPELIB), 0 },  REG_ROOT+2, REG_DEFAULT   },
};



STDAPI Wsdl_UnregisterServer(void)
{
#ifndef CE_NO_EXCEPTIONS
    try
#else
    __try
#endif 
    {
        UnregisterRegEntry(g_RegistryEntries, countof(g_RegistryEntries), HKEY_CLASSES_ROOT);
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



STDAPI Wsdl_RegisterServer(CHAR * pModName)
{
#ifdef CE_NO_EXCEPTIONS
    if(SUCCEEDED(RegisterRegEntry(g_RegistryEntries, countof(g_RegistryEntries), pModName, 0, HKEY_CLASSES_ROOT)))
        return S_OK;
    else
    {
        Wsdl_UnregisterServer();
        return E_FAIL;
    }
#else
    try
    {
        // no UnregisterServer here - the caller did it already 
        RegisterRegEntry(g_RegistryEntries, countof(g_RegistryEntries), pModName, 0, HKEY_CLASSES_ROOT);
        return S_OK;
    }

     catch(...)
    {
        Wsdl_UnregisterServer();
        return E_FAIL;
    }
#endif
}



