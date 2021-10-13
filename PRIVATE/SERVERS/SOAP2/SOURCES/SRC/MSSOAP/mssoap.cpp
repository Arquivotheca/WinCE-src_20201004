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
//      mssoap.cpp
//
// Contents:
//
//      Implementation of DLL Exports.
//
//----------------------------------------------------------------------------------

#define INIT_SOAP_GLOBALS
#define MYINIT_GUID                 // Includes GUID instances

#ifdef UNDER_CE
#include "objbase.h"
#include "WinCEUtils.h"
#endif


#include "headers.h"


extern "C" HINSTANCE Get_ResourceHINSTANCE();

#ifdef _CRTDBG_MAP_ALLOC
    HANDLE    hfile; 
#endif

#ifdef UNDER_CE
 CRITICAL_SECTION g_csGIT;
 DWORD            g_dwGITRefCnt;
#endif 


BOOL    DllProcessAttach(HINSTANCE hInstDLL);
BOOL    DllProcessDetach();

STDAPI SoapMsg_RegisterServer(CHAR *pModName);
STDAPI SoapMsg_UnregisterServer(void);
STDAPI SoapConn_RegisterServer(CHAR *pModName);
STDAPI SoapConn_UnregisterServer(void);
STDAPI SoapSer_RegisterServer(CHAR *pModName);
STDAPI SoapSer_UnregisterServer(void);
STDAPI SoapReader_RegisterServer(CHAR *pModName);
STDAPI SoapReader_UnregisterServer(void);
STDAPI Wsdl_RegisterServer(CHAR * pModName);
STDAPI Wsdl_UnregisterServer(void);




BEGIN_FACTORY_MAP(CClassFactory)
    //  ADD_FACTORY_PRODUCT_FTM(clsid, class)
    ADD_FACTORY_PRODUCT_FTM(CLSID_SoapConnectorFactory, CSoapConnectorFactory)
    ADD_FACTORY_PRODUCT_FTM(CLSID_SoapServer, CSoapServer)
    ADD_FACTORY_PRODUCT_FTM(CLSID_SoapClient, CSoapClient)
    ADD_FACTORY_PRODUCT_FTM(CLSID_SoapSerializer, CSoapSerializer)
    ADD_FACTORY_PRODUCT_FTM(CLSID_SoapReader, CSoapReader)
    ADD_FACTORY_PRODUCT_FTM(CLSID_WSDLReader, CWSDLReader)
    ADD_FACTORY_PRODUCT_FTM(CLSID_EnumWSDLService, CEnumWSDLService)
    ADD_FACTORY_PRODUCT_FTM(CLSID_WSDLService, CWSDLService)
    ADD_FACTORY_PRODUCT_FTM(CLSID_WSDLPort, CWSDLPort)
    ADD_FACTORY_PRODUCT_FTM(CLSID_WSDLOperation, CWSDLOperation)
    ADD_FACTORY_PRODUCT_FTM(CLSID_EnumWSDLOperations, CEnumWSDLOperations)
    ADD_FACTORY_PRODUCT_FTM(CLSID_EnumSoapMappers, CEnumSoapMappers)
    ADD_FACTORY_PRODUCT_FTM(CLSID_EnumWSDLPorts, CEnumWSDLPorts)
    ADD_FACTORY_PRODUCT_FTM(CLSID_SoapMapper, CSoapMapper)
    ADD_FACTORY_PRODUCT_FTM(CLSID_SoapTypeMapperFactory, CTypeMapperFactory)    

    { &CLSID_HttpConnector, CHttpConnector::CreateObject, CHttpConnector::CreateObject },

END_FACTORY_MAP(CClassFactory)



////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: static bool isWin9x(void)
//
//  parameters:
//          
//  description:
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef UNDER_CE
static bool isWin9x(void)
{    
    bool bIsWin9x;
    OSVERSIONINFOA osver;

    osver.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
    GetVersionExA(&osver);

    bIsWin9x = !!(osver.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS);
    TRACEL((1, "Running under Win9x: %s\n", bIsWin9x ? "true" : "false"));
    return bIsWin9x;
}
#endif




////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
//
//  parameters:
//          
//  description:    DLL Entry Point
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
    BOOL    fRetVal = TRUE;        // assume successful.
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


#ifndef UNDER_CE
bool g_fIsWin9x = false;
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: DllProcessAttach(HINSTANCE hInstDLL)
//
//  parameters:
//          
//  description:
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL DllProcessAttach(HINSTANCE hInstDLL)
{
    BOOL fRetVal = TRUE;
#ifndef UNDER_CE   
    HRESULT hr = S_OK;
    char  szModuleName[MAX_RES_STRING_SIZE + 1];
    char *  psz;
    char *  pszlocale;
    char *  pszcurpath;
    DWORD   cch;
    LCID    lcid;
#endif 

    SET_TRACE_LEVEL(3);

    // Disable the DLL_THREAD_ATTACH and DLL_THREAD_DETACH 
    // notifications for this DLL
    DisableThreadLibraryCalls(hInstDLL);

#ifdef UNDER_CE
    InitializeCriticalSection(&g_csGIT);
    g_dwGITRefCnt = 0;
#endif 

    // initialize Soaputil DLL
    InitSoaputil();

#ifndef UNDER_CE
    g_fIsWin9x = isWin9x();
   
    //
    // Load Localization DLL
    //
    cch = GetModuleFileNameA(hInstDLL, szModuleName, MAX_RES_STRING_SIZE);
    if (cch == 0)
    {
        hr = GetLastError();
        fRetVal = FALSE;
        goto Cleanup;
    }
    // Find the path of the file
    psz = &(szModuleName[cch - 1]);
    while ((psz >= szModuleName) && (*psz != '\\') && (*psz != '/'))
        psz--;
    psz++; 
    pszcurpath = psz;
    strncpy(psz, "Resources\\", (MAX_RES_STRING_SIZE - (psz - szModuleName)));
    lcid = GetSystemDefaultLCID();
    pszlocale = szModuleName + strlen(szModuleName);
    ltoa (lcid, pszlocale, 10);
    psz = szModuleName + strlen(szModuleName);
    *psz = (char)'\\';
    psz++;
    strncpy(psz, "MSSOAPR.DLL", (MAX_RES_STRING_SIZE - (psz - szModuleName)));     
    
    g_hInstance_language = LoadLibraryExA(szModuleName, NULL, 
            LOAD_WITH_ALTERED_SEARCH_PATH);

    if( !g_hInstance_language )
    {
        // If not found, try the local default 1033 (US English) 
        strncpy(pszlocale, "1033\\MSSOAPR.DLL", (MAX_RES_STRING_SIZE - (pszlocale - szModuleName)));
        g_hInstance_language = LoadLibraryExA(szModuleName, NULL, 
            LOAD_WITH_ALTERED_SEARCH_PATH);
        if( !g_hInstance_language )
        {
            // Finally look for it in the current directory 
            strncpy(pszcurpath, "MSSOAPR.DLL", (MAX_RES_STRING_SIZE - (pszcurpath - szModuleName)));
            g_hInstance_language = LoadLibraryExA(szModuleName, NULL, 
                LOAD_WITH_ALTERED_SEARCH_PATH);
                
            if( !g_hInstance_language )
            {
#ifdef DEBUG
                // For DEBUG version only, check the path ..\..\..\mssoapres\objd\i386\mssoapr.dll
                // This hack is convenient for development debugging.
                // Go up three directories
                psz = pszcurpath - 2;
                for ( int i = 0 ; i < 3 ; i++)
                {
                    while ((psz >= szModuleName) && (*psz != '\\') && (*psz != '/'))
                        psz--;
                    psz--;    
                }    
                psz++; 
                psz++;
                strncpy(psz, "mssoapres\\objd\\i386\\MSSOAPR.DLL", 
                        (MAX_RES_STRING_SIZE - (psz - szModuleName)));
                g_hInstance_language = LoadLibraryExA(szModuleName, NULL, 
                    LOAD_WITH_ALTERED_SEARCH_PATH);
                if (g_hInstance_language)
                {
                    fRetVal = TRUE;
                }
                else
                {
#endif
                    fRetVal = FALSE;
                    goto Cleanup;
#ifdef DEBUG
                }    
#endif
            }
        }
    }    

Cleanup:

#ifdef _CRTDBG_MAP_ALLOC
    WCHAR buffer[1024];
    hfile = CreateFile(_T("soapmemdump.log"), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_ALWAYS , FILE_ATTRIBUTE_NORMAL, 0); 
    if (GetEnvironmentVariable(_T("SOAP_CHECKMEM"), buffer,1024)!=0)
    {
        _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF |  _CRTDBG_DELAY_FREE_MEM_DF);        
    }
    else
    {
        _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF );        
    }

    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, hfile); 
#endif
#endif
    

    return fRetVal;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: SafeFreeLibrary(HINSTANCE h)
//
//  parameters:
//          
//  description:
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void SafeFreeLibrary(HINSTANCE h)
{
    __try
    {
        FreeLibrary(h);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        TRACE(("DllMain: Exception fired during DllProcessDetach()\n"));
    }
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


    // Don't free the Localized Resource Strings on Win95
    globalDeleteErrorList();
#ifndef UNDER_CE    
    if (!g_fIsWin9x)
        SafeFreeLibrary(g_hInstance_language);
    else
        CloseHandle(g_hInstance_language); 
#else
    DeleteCriticalSection(&g_csGIT); 
#endif

#ifdef _CRTDBG_MAP_ALLOC    
    ASSERT(_CrtCheckMemory());
    ASSERT(!_CrtDumpMemoryLeaks());
    CloseHandle(hfile);
#endif 
    TRACEL((1, "Nr of objects still around=%d\n", g_cObjects));



    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: DllUnregisterServer(void)
//
//  parameters:
//          
//  description:    Removes entries from the system registry
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDAPI DllUnregisterServer(void)
{
#ifndef CE_NO_EXCEPTIONS
    try
#else
    __try
#endif 
    {

#ifndef UNDER_CE
        UnRegisterTypeLib(LIBID_MSSOAPLib, SOAP_SDK_VERSION_MAJOR, SOAP_SDK_VERSION_MINOR, LANG_NEUTRAL, SYS_WIN32);
#endif 

        SoapMsg_UnregisterServer();
        SoapConn_UnregisterServer();
        SoapSer_UnregisterServer();
        SoapReader_UnregisterServer();
        Wsdl_UnregisterServer();

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


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: DllRegisterServer(void)
//
//  parameters:
//          
//  description:    Adds entries to the system registry
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDAPI DllRegisterServer(void)
{
    CComPointer<ITypeLib> pTypeLib;

#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif
        HRESULT     hr = S_OK;
        CHAR  pModName[MAX_PATH + 1] = { 0 };
        WCHAR wModName[MAX_PATH + 1] = { 0 };
        
        if (! GetModuleFileNameA(g_hInstance, pModName, MAX_PATH))
        {
            return E_FAIL;
        }

        //make a clean start
        DllUnregisterServer();
        
        hr = SoapMsg_RegisterServer(pModName);
        if (SUCCEEDED(hr))
            hr = SoapConn_RegisterServer(pModName);
            
        if (SUCCEEDED(hr))
            hr = SoapSer_RegisterServer(pModName);

        if (SUCCEEDED(hr))
            hr = SoapReader_RegisterServer(pModName);
            
        if (SUCCEEDED(hr))
            hr = Wsdl_RegisterServer(pModName);

        if (SUCCEEDED(hr))
        {
#ifndef UNDER_CE
            if (MultiByteToWideChar(CP_ACP, 0, pModName, -1, wModName, sizeof(wModName)) == 0)
#else
            if (MultiByteToWideChar(CP_ACP, 0, pModName, -1, wModName, MAX_PATH) == 0)
#endif
            {
                return E_FAIL;
            }
#ifndef UNDER_CE
            hr = LoadTypeLibEx(wModName, REGKIND_REGISTER, &pTypeLib);
#else
            hr = LoadTypeLib(wModName, &pTypeLib);
            if(SUCCEEDED(hr))
                hr = RegisterTypeLib(pTypeLib,wModName, NULL);
#endif

        }

        if (!SUCCEEDED(hr))
            {
            // no cleanup -- all xxx_RegisterServer-calls cleaned up behind themself
            // in the case of an error
            return E_FAIL;
            }
        return S_OK;
#ifndef CE_NO_EXCEPTIONS
    }    
    catch (...)
    {
        DllUnregisterServer();
        return E_FAIL;
    }
#endif    
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv)
//
//  parameters:
//          
//  description:
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv)
{
    if(! ppv)
        return E_INVALIDARG;

    *ppv = 0;

    CSoapObject<CClassFactory> *pClassFactory = new CSoapObject<CClassFactory>;
    if (pClassFactory == 0)
    {
        return E_OUTOFMEMORY;
    }

    HRESULT hr = S_OK;

    if(FAILED(hr = pClassFactory->Init(rclsid)) ||
       FAILED(hr = pClassFactory->QueryInterface(riid, ppv)))
    {
        delete pClassFactory;
        return hr;
    }

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
#ifndef UNDER_CE
STDAPI DllCanUnloadNow()
{
    static HRESULT s_result[] = { S_OK, S_FALSE };
    HRESULT hr = s_result[g_cLock || g_cObjects];

    if (hr == S_OK)
    {
        hr = SoapConn_DllCanUnloadNow();
    }

    return hr;
}
#else
STDAPI DllCanUnloadNow()
{
    HRESULT hr = S_FALSE;

    if (!g_cLock && !g_cObjects)
        hr = SoapConn_DllCanUnloadNow();
  
    return hr;
}
#endif


