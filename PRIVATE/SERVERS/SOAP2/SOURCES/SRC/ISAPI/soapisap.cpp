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
// File:    soapisap.cpp
//
// Contents:
//
//  ISAPI Extension that listens to SOAP packages and routes them to the
//  appropriate SOAP driver.
//
//
//
//-----------------------------------------------------------------------------

#define INIT_ISAPI_GLOBALS
#define MYINIT_GUID                 // Includes GUID instances

#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif 
#include "isapihdr.h"

#ifndef UNDER_CE
#include "iiscnfg.h"
#include "iadmw.h"
#else
#define SOAP_ISAPI_DBG                    DEBUGZONE(0)
#define SOAP_PROCESS_ATTACH               DEBUGZONE(1)
#define SOAP_PROCESS_REQUEST              DEBUGZONE(2)
#endif 


typedef HRESULT (STDAPICALLTYPE * PGETCLASSOBJ)(REFCLSID, REFIID, LPVOID);

BOOL DllProcessAttach(HINSTANCE hInstDLL);
BOOL DllProcessDetach();
void InitRegInfo();
void SetRegInfo();
void RemoveRegInfo();

#ifndef UNDER_CE
HRESULT GetMetadataExtensions(IMSAdminBase *pAdminBase,
                                    METADATA_HANDLE *phMD,
                                    METADATA_RECORD *pmdRec,
                                    WCHAR ** tempbuf);
#endif 

HRESULT RemoveSoapExt(WCHAR * pwstr, DWORD *pwch);

BOOL IsIISVersion4(WCHAR * pwstr, DWORD wch);

extern "C" BOOL __cdecl DbgDllMain(HANDLE hinst, DWORD dwReason, LPVOID lpReason);

#ifdef UNDER_CE
HANDLE g_hTerminateThread;
HANDLE g_hTerminateEvent;
DWORD TerminateISAPIThread(LPVOID param);
#endif 

#ifndef UNDER_CE
#ifdef _CRTDBG_MAP_ALLOC
	HANDLE	hfile;
#endif
#endif


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: DllMain
//
//  parameters:
//
//
//  description:
//          @func DLL Entry point where Instance and Thread attach/detach notifications
//          takes place.  Some current activity is OLE is being initialized and the
//          IMalloc Interface pointer is obtained.
//
//          @rdesc Boolean Flag
//              @flag TRUE  | Successful initialization
//              @flag FALSE | Failure to intialize
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL WINAPI DllMain
    (
    HANDLE   hInstDLL,       //@parm IN | Application Instance Handle         
    DWORD       fdwReason,      //@parm IN | Indicated Process or Thread activity
    LPVOID      lpvReserved     //@parm IN | Reserved...                         
    )
{
    BOOL        fRetVal = TRUE;        // assume successful.

    switch( fdwReason )
    {
        case DLL_PROCESS_ATTACH:  
#ifndef UNDER_CE // cant call coinit from process attach!  moving to GetExtensionVersion()
            if (FAILED(CoInitializeEx(NULL,COINIT_MULTITHREADED)))
                {
                    DEBUGMSG(SOAP_ISAPI_DBG,(L"[SOAPISAPI] DLL_PROCESS_ATTACH -- cant CoInitialize\n"));
                    return FALSE;
                }
            fRetVal = DllProcessAttach((HINSTANCE)hInstDLL);
            CoUninitialize(); 
#else
            g_hInstance = (HINSTANCE)hInstDLL;
#endif 
            break;
        case DLL_PROCESS_DETACH:            
            fRetVal = DllProcessDetach();
            
            DEBUGMSG(SOAP_ISAPI_DBG,(L"[SOAPISAPI] DLL_PROCESS_DETACH -- isapi is gone\n"));                  
            break;
    }
    return fRetVal;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: DllProcessAttach(HINSTANCE hInstDLL)
//
//  parameters:
//
//
//  description:
//
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL DllProcessAttach(HINSTANCE hInstDLL)
{
#ifdef UNDER_CE
    DEBUGMSG(SOAP_PROCESS_ATTACH, (L"[SOAPISAPI] DllProcessAttach"));
#endif 
    BOOL    fRetVal = TRUE;
#ifndef UNDER_CE
    HRESULT hr = S_OK;    
    char    szModuleName[MAX_RES_STRING_SIZE + 1];
    char *  psz;
    char *  pszlocale;
    char *  pszcurpath;
    DWORD   cch;
    OSVERSIONINFOA osver;
    LCID    lcid;
#endif 

	SET_TRACE_LEVEL(3);

    InitializeCriticalSection(&g_cs);

    InitSoaputil();
#ifdef UNDER_CE
    DEBUGMSG(SOAP_PROCESS_ATTACH, (L"[SOAPISAPI] Finishised SoapUtil init"));
    g_hTerminateThread = NULL;
#endif 

#ifndef UNDER_CE //this is already set from dllmain
    g_hInstance = hInstDLL;
#endif 

    // Disable the DLL_THREAD_ATTACH and DLL_THREAD_DETACH
    // notifications for this DLL
    DisableThreadLibraryCalls(hInstDLL);

#ifndef UNDER_CE
    GetSystemInfo(&g_si);
#endif 

    // Initialize the registry values
#ifndef UNDER_CE
    g_cThreads = (2 * g_si.dwNumberOfProcessors)+ 1;
#else
    g_cThreads = 2 + 1;
#endif 
    g_cObjCachePerThread = 5;       // default value
    g_cbMaxPost = 100 * 1024;       // 100 Kb is the default max

    // Get the number of threads from the registry, if any.
    InitRegInfo();

#ifndef UNDER_CE
    osver.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);

    GetVersionExA(&osver);

    g_fIsWin9x = !!(osver.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS);
	TRACEL((1, "Running under Win9x: %s\n", g_fIsWin9x ? "true" : "false"));


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
    strncpy(psz, "MSSOAPR.DLL", (MAX_RES_STRING_SIZE - (psz - szModuleName)))
;

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
                // For DEBUG version only, check the path ..\..\..\mssoapr\objd\i386\mssoapr.dll
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

	hfile = CreateFile(_T("soapisapimemdump.log"), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_ALWAYS , FILE_ATTRIBUTE_NORMAL, 0);
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF |  _CRTDBG_DELAY_FREE_MEM_DF);
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_WARN, hfile);
#endif


#endif //UNDER_CE
    if (g_cThreads < 1)
        g_cThreads = 1;
    else if (g_cThreads > 32)
        g_cThreads = 32;

    if (!fRetVal)
    {
        DeleteCriticalSection(&g_cs);

#ifndef UNDER_CE
        if (g_hInstance_language)
            FreeLibrary(g_hInstance_language);
#endif 
    }

    return fRetVal;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: SafeFreeLibrary(HINSTANCE h)
//
//  parameters:
//
//
//  description:
//
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void SafeFreeLibrary(HINSTANCE h)
{
    __try
    {
        FreeLibrary(h);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        TRACEL((1, "DllMain: Exception fired during DllProcessDetach()\n"));
    }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: SafeRelease(IUnknown * ptr, size_t sz)
//
//  parameters:
//
//
//  description:
//
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void SafeRelease(IUnknown * ptr, size_t sz)
{
    if(ptr && !IsBadWritePtr(ptr, sz))
    {
        __try
        {
            ptr->Release();
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            TRACEL((1, "DllMain: Exception fired during Interface release \n"));
        }
    }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: DllProcessDetach()
//
//  parameters:
//
//
//  description:
//
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL DllProcessDetach()
{

    globalDeleteErrorList();
    DeleteCriticalSection(&g_cs);

#ifndef UNDER_CE
    // Don't free the Localized Resource Strings on Win95
    if (!g_fIsWin9x)
        SafeFreeLibrary(g_hInstance_language);
    else
        CloseHandle(g_hInstance_language);
#endif 

#ifndef UNDER_CE
#ifdef _CRTDBG_MAP_ALLOC
	ASSERT(_CrtCheckMemory());
	ASSERT(!_CrtDumpMemoryLeaks());
	CloseHandle(hfile);
#endif
#endif
	TRACEL((1, "Nr of objects still around=%d\n", g_cObjects));

    return TRUE;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: DllCanUnloadNow
//
//  parameters:
//
//  description:
//          @func Indicates whether the DLL is no longer in use and
//          can be unloaded.
//
//          @rdesc HResult indicating status of routine
//              @flag S_OK | DLL can be unloaded now.
//              @flag S_FALSE | DLL cannot be unloaded now.
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
STDAPI DllCanUnloadNow(void)
{
   	static HRESULT s_result[] = { S_OK, S_FALSE };
	return s_result[g_cLock || g_cObjects];

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: GetExtensionVersion( HSE_VERSION_INFO *pVer)
//
//  parameters:
//
//          Arguments:   pVer - poins to extension version info structure
//          Returns:     TRUE (always)
//
//  description:
//          This is required ISAPI Extension DLL entry point.
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL WINAPI GetExtensionVersion (HSE_VERSION_INFO *pVer)
{
    BOOL                        fRetVal = TRUE;
    
#ifdef UNDER_CE  //moved from DLLMain (cant call coinit from dllmain)
        if (FAILED(CoInitializeEx(NULL,COINIT_MULTITHREADED)))
            {
                DEBUGMSG(SOAP_ISAPI_DBG,(L"[SOAPISAPI] DLL_PROCESS_ATTACH -- cant CoInitialize\n"));
                return FALSE;
            }
        fRetVal = DllProcessAttach(g_hInstance);
        CoUninitialize();
#endif 

   

    pVer->dwExtensionVersion = HSE_VERSION;

#ifndef UNDER_CE
    lstrcpynA( pVer->lpszExtensionDesc, "MSSOAP Isapi Listener ", HSE_MAX_EXT_DLL_NAME_LEN );
#else
    strncpy( pVer->lpszExtensionDesc, "MSSOAP Isapi Listener ", HSE_MAX_EXT_DLL_NAME_LEN );
#endif 

    EnterCriticalSection(&g_cs);

#ifdef UNDER_CE
    if(NULL == g_hTerminateThread)
    {
       g_hTerminateThread = CreateThread(NULL, 0, TerminateISAPIThread, 0, 0, 0);
       ASSERT(NULL != g_hTerminateThread);

       g_hTerminateEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
       ASSERT(NULL != g_hTerminateEvent);       
    }
#endif 


    if (!g_fThreadPoolInitialized)
    {
        if (InitializeThreadPool(g_cThreads, g_cObjCachePerThread) != 0)
        {
            fRetVal = FALSE;
            goto Cleanup;
        }
    }

Cleanup:
    ++g_cExtensions;
    LeaveCriticalSection(&g_cs);

    if (!fRetVal)
    {
        TerminateExtension(0);
    }

    return fRetVal;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: TerminateExtension ( DWORD dwFlags )
//
//  parameters:
//          Arguments:
//              dwFlags - specifies whether the DLL can refuse to unload or not
//          Returns:
//              TRUE, if the DLL can be unloaded
//
//  description:
//          This is optional ISAPI extension DLL entry point.
//          If present, it will be called before unloading the DLL,
//          giving it a chance to perform any shutdown procedures.
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL WINAPI TerminateExtension ( DWORD dwFlags )
{
#ifdef UNDER_CE
    DEBUGMSG(SOAP_ISAPI_DBG,(L"[SOAPISAPI] TerminateExtension--setting terminate event\n"));               
    SetEvent(g_hTerminateEvent);
    #ifdef DEBUG
    DWORD dwRet = 
    #endif
    WaitForSingleObject(g_hTerminateEvent, INFINITE);
    DEBUGMSG(SOAP_ISAPI_DBG,(L"[SOAPISAPI] --TerminateExtension--I'm awake, exiting\n"));               
    
    ASSERT(dwRet == WAIT_OBJECT_0);  
#else
    EnterCriticalSection(&g_cs);

    if (--g_cExtensions == 0)
    {

        TerminateThreadPool();
    }

    LeaveCriticalSection(&g_cs);
#endif 
    return TRUE;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HttpExtensionProc(EXTENSION_CONTROL_BLOCK *pECB)
//
//  parameters:
//          Arguments:   pECB - pointer to the extenstion control block
//
//          Returns:     HSE_STATUS_PENDING on successful transmission completion
//                       HSE_STATUS_ERROR on failure
//
//  description:
//          Http extension proc called by the web server on request.
//          Queue's up a request to the worker queue for async
//          execution of the query.
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
DWORD WINAPI HttpExtensionProc (EXTENSION_CONTROL_BLOCK *pECB )
{
    if (g_pThreadPool->pWorkerQueue->dwSize < MAX_REQUEST_QUEUE_SIZE)
    {
        PWORKERITEM pItem = CreateWorkerItem(ProcessRequest, (void *)pECB);

        if (pItem)
        {
#ifdef UNDER_CE
            //NOTE: we have to cache this because as soon as we put the
            //  WORKERITEM on the queue we lose ownership
            HANDLE hProcessingHandle = pItem->hFinishedProcessing;
            WorkerEnqueue(g_pThreadPool->pWorkerQueue, pItem);
           
            DWORD dwRet = WaitForSingleObject(hProcessingHandle, INFINITE);
            ASSERT(dwRet == WAIT_OBJECT_0);
    
             if(NULL != hProcessingHandle)
                CloseHandle(hProcessingHandle);

 
            if(dwRet == WAIT_OBJECT_0)
            {
                BOOL fCanKeepAlive = FALSE;
                //do a check to see if we CAN do a keepalive
                pECB->ServerSupportFunction(pECB->ConnID,
                    HSE_REQ_IS_KEEP_CONN,
                    &fCanKeepAlive, NULL, NULL );                
                
                if(fCanKeepAlive)
                    return HSE_STATUS_SUCCESS_AND_KEEP_CONN;
                else
                    return HSE_STATUS_SUCCESS;
            }
            else
                return HSE_STATUS_ERROR;

#else
            WorkerEnqueue(g_pThreadPool->pWorkerQueue, pItem);
            return HSE_STATUS_PENDING;
#endif             
        }
    }

    return HSE_STATUS_ERROR;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: ProcessRequest(void * pContext)
//
//  parameters:
//          Arguments:   pContext - pointer to the extenstion control block
//
//  description:
//          Process a query request, a callback called by a thread from
//          the thread pool when a query is queued up by iis.
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////

DWORD ProcessRequest(void * pContext)
{

    PREQUESTINFO pRequestInfo = (PREQUESTINFO)pContext;
    PTHREADINFO  pThreadInfo  = (PTHREADINFO)pRequestInfo->m_pvThreadInfo;
    EXTENSION_CONTROL_BLOCK *pECB = (EXTENSION_CONTROL_BLOCK*)pRequestInfo->m_pvContext;
    HRESULT     hr = S_OK;
    CRequest	cReq(pECB, pThreadInfo);
    CRequest*   pRequest = &cReq;
#ifndef UNDER_CE
    HANDLE      hRequestToken = NULL;
    BOOL        fImpersonate = FALSE;
#endif 
    CAutoBSTR bstrSoapAction;

    TRACEL((2, (const LPCSTR) _T("Processing request: %S\r\n"), pECB->lpszQueryString));

#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif 
        CHK (pRequest->Validate());
        //
        // Get the request token from IIS and set the token for this thread
        //

#ifndef UNDER_CE
        if(pECB->ServerSupportFunction(
                    pECB->ConnID,
                    HSE_REQ_GET_IMPERSONATION_TOKEN,
                    &hRequestToken,
                    NULL,
                    NULL
                    ))
        {
            //
            //Impersonate the logged on user
            //
            if (ImpersonateLoggedOnUser(hRequestToken))
                fImpersonate = TRUE;
            else
            {
                pRequest->SetErrorCode(Error_AccessDenied);
                CHK(E_FAIL);
            }
        }
#endif 

        // We only process GET, POST or HEAD requests here
        if(stricmp(pECB->lpszMethod, "POST"))
        {
            DEBUGMSG(SOAP_PROCESS_REQUEST,(L"[SOAPISAPI] got a POST\n"));

            if (!stricmp(pECB->lpszMethod, "GET"))
            {
                hr = pRequest->ProcessGet(FALSE);
            }
            else if (!stricmp(pECB->lpszMethod, "HEAD"))
            {

                hr = pRequest->ProcessGet(TRUE);
            }
            else
            {
                pRequest->SetErrorCode(Error_MethodNotAllowed);
            }
            goto Cleanup;
        }

        // We only process POST from here on
        if (FAILED(hr = pRequest->ProcessPathInfo()))
        {
            CHK (bstrSoapAction.Assign(pRequest->GetHeaderParam("SOAPAction"), FALSE));
            pRequest->WriteFaultMessage(hr, bstrSoapAction);
            goto Cleanup;
        }

        if (FAILED(hr = pRequest->ProcessParams()))
        {
            CHK (bstrSoapAction.Assign(pRequest->GetHeaderParam("SOAPAction"), FALSE));
            pRequest->WriteFaultMessage(hr, bstrSoapAction);
            goto Cleanup;
        }

        if (FAILED(hr = pRequest->ExecuteRequest()))
        {
            CHK (bstrSoapAction.Assign(pRequest->GetHeaderParam("SOAPAction"), FALSE));
            pRequest->WriteFaultMessage(hr, bstrSoapAction);
            goto Cleanup;
        }

#ifndef UNDER_CE
    }
    catch (...)
    {
        ASSERT(0);
        // We come here if we get an Access Violation. At this point we cannot trust any
        // memory or cache in the thread. Just clear the thread's buffer and its
        // cache.
        // Don't try to release memory since it may be corrupted and cause further AVs.
        if (pThreadInfo && pThreadInfo->pObjCache)
            pThreadInfo->pObjCache->ClearCache();
        pRequest->SetErrorCode(Error_InternalError);
    }
#endif 

Cleanup:

    if(pRequest)
        pRequest->FlushContents();

#ifndef UNDER_CE
    if(fImpersonate)
    {
        //
        // ReverToSelf() will set the security context to the IIS security Context
        //
        RevertToSelf();
    }
    pECB->ServerSupportFunction(
            pECB->ConnID,
           HSE_REQ_DONE_WITH_SESSION,
            NULL,
            NULL,
            NULL
            );
#endif 
    TRACEL((2, (const LPCSTR) _T("End Processing request")));
    
#ifdef UNDER_CE
     ASSERT(NULL != pRequestInfo->hFinishedProcessing); 
     SetEvent(pRequestInfo->hFinishedProcessing);
#endif 

    //note: this return isnt used in CE
    return HSE_STATUS_SUCCESS_AND_KEEP_CONN;
}


WCHAR g_wszThreadsRegKey[] = L"Software\\Microsoft\\MSSOAP\\SOAPISAP";
WCHAR g_wszThreadsParentRegKey[] = L"Software\\Microsoft\\MSSOAP";

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: InitRegInfo()
//
//  parameters:
//
//  description:
//          Read the registry Software\\Microsoft\\MSSOAP\\SOAPISAP
//          and get the number of threads.
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void InitRegInfo()
{
    HKEY    hKey = (HKEY) INVALID_HANDLE_VALUE;
    DWORD   dwVal, dwLen = sizeof(DWORD);

    if( (ERROR_SUCCESS != RegOpenKeyEx(HKEY_LOCAL_MACHINE, g_wszThreadsRegKey, 0,
                            KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS,
                            &hKey)) || hKey == INVALID_HANDLE_VALUE )
    {
        GetLastError();
        return;
    }


    // Read num threads value.
    if(ERROR_SUCCESS == RegQueryValueEx(hKey, L"NumThreads", NULL, NULL, (LPBYTE) &dwVal, &dwLen))
    {
        g_cThreads = dwVal;
    }

    // Read num objs cahced per thread
    if(ERROR_SUCCESS == RegQueryValueEx(hKey, L"ObjCachedPerThread", NULL, NULL, (LPBYTE) &dwVal, &dwLen))
    {
        g_cObjCachePerThread = dwVal;
    }

    // Read max post size allowed
    if(ERROR_SUCCESS == RegQueryValueEx(hKey, L"MaxPostSize", NULL, NULL, (LPBYTE) &dwVal, &dwLen))
    {
        g_cbMaxPost = dwVal;
    }

#ifndef UNDER_CE
    if (RegQueryValueEx(hKey, L"NoNagling", NULL, NULL, (LPBYTE) &dwVal, &dwLen) == ERROR_SUCCESS)
    {
        g_dwNaglingFlags = dwVal ? HSE_IO_SYNC | HSE_IO_NODELAY : 0;
    }
#endif 

    RegCloseKey(hKey);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: SetRegInfo()
//
//  parameters:
//
//  description:
//          Initialize the registry Software\\Microsoft\\MSSOAP\\SOAPISAP
//          and the number of threads.
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void SetRegInfo()
{
    HRESULT     hr;
    HKEY    hKey = (HKEY) INVALID_HANDLE_VALUE;
    DWORD   dwVal, dwLen = sizeof(DWORD);

    if( (ERROR_SUCCESS != RegCreateKeyEx(HKEY_LOCAL_MACHINE, g_wszThreadsRegKey,
                            0, NULL, REG_OPTION_NON_VOLATILE,
                            KEY_SET_VALUE | KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS,
                            NULL, &hKey, &dwVal)) || hKey == INVALID_HANDLE_VALUE )
    {
        GetLastError();
        return;
    }

    //Read/set num threads value.
    hr = RegQueryValueEx(hKey, L"NumThreads", NULL, NULL, (LPBYTE) &dwVal, &dwLen);
    if (hr != ERROR_SUCCESS)
    {
        dwVal = (2 * g_si.dwNumberOfProcessors)+ 1;
        hr = RegSetValueEx( hKey, L"NumThreads", NULL, REG_DWORD, (LPBYTE)&dwVal, dwLen);
    }

    hr = RegQueryValueEx(hKey, L"ObjCachedPerThread", NULL, NULL, (LPBYTE) &dwVal, &dwLen);
    if(ERROR_SUCCESS != hr)
    {
        dwVal = 5;      // default = 5 objects cached per thread
        hr = RegSetValueEx( hKey, L"ObjCachedPerThread", NULL, REG_DWORD, (LPBYTE)&dwVal, dwLen);
    }

    hr = RegQueryValueEx(hKey, L"MaxPostSize", NULL, NULL, (LPBYTE) &dwVal, &dwLen);
    if(ERROR_SUCCESS != hr)
    {
        dwVal = 100 * 1024;      // default = 100 Kb.
        hr = RegSetValueEx( hKey, L"MaxPostSize", NULL, REG_DWORD, (LPBYTE)&dwVal, dwLen);
    }

    if (RegQueryValueEx(hKey, L"NoNagling", NULL, NULL, (LPBYTE) &dwVal, &dwLen) != ERROR_SUCCESS)
    {
        dwVal = 0;
        RegSetValueEx(hKey, L"NoNagling", NULL, REG_DWORD, (LPBYTE) &dwVal, dwLen);
    }

    RegCloseKey(hKey);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: RemoveRegInfo()
//
//  parameters:
//
//  description:
//          Removes the registry Software\\Microsoft\\MSSOAP\\SOAPISAP
//          and the number of threads.
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void RemoveRegInfo()
{
    HRESULT     hr;
    HKEY    hKey = (HKEY) INVALID_HANDLE_VALUE;
#ifndef UNDER_CE
    DWORD   dwVal;
#endif 

    if( (ERROR_SUCCESS != RegOpenKeyEx(HKEY_LOCAL_MACHINE, g_wszThreadsParentRegKey, 0,
                            KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS,
                            &hKey)) || hKey == INVALID_HANDLE_VALUE )
    {
        return;
    }

    if (ERROR_SUCCESS !=  RegDeleteKey( hKey, L"SOAPISAP"))
    {

        hr = GetLastError();
    }

    RegCloseKey(hKey);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: DllRegisterServer
//
//  parameters:
//
//  description:
//          Adds entries to the system registry
//          Registers .wsdl extension for POSTs to our ISAPI
//          IIS version 5 lists only the allowed verbs, while IIS version 4 lists only
//          the disallowed verbs
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
const  WCHAR  g_ScriptMapWStr[] = L".wsdl,%s,1,GET,POST,HEAD";
const  WCHAR  g_ScriptMapWStrIIS4[] = L".wsdl,%s,1,PUT,DELETE,TRACE";

STDAPI DllRegisterServer(void)
{
#ifndef UNDER_CE
    HRESULT     hr = S_OK;
    CAutoRefc<IMSAdminBase>  pAdminBase;
    METADATA_HANDLE hMD = 0;
    METADATA_RECORD mdRec;
    DWORD           wch;
    WCHAR           wszModuleName[MAX_RES_STRING_SIZE+1];
    WCHAR *         tempbuf = NULL;
    WCHAR *         pwstr;
    BOOL            bIsIIS4;

    // Skip the registration if this is a Win9x platform
    if (g_fIsWin9x)
    	return S_OK;

    SetRegInfo();
    // get a pointer to the IIS Admin Base Object
    hr = CoCreateInstance(
                            CLSID_MSAdminBase,
                            NULL, CLSCTX_ALL,
                            IID_IMSAdminBase,
                            (void **) &pAdminBase);
	if (hr != S_OK)
	{
		// No IIS is installed, return silently
		return S_OK;
	}

    // Obtain the Metadata File extensions list
    CHK(GetMetadataExtensions(pAdminBase, &hMD, &mdRec, &tempbuf));

    // Remove previously existing .soap extension
    wch = mdRec.dwMDDataLen / sizeof(WCHAR);
    RemoveSoapExt(tempbuf, &wch);

    bIsIIS4 = IsIISVersion4(tempbuf, wch);

    // Add current .soap extension
    pwstr = (WCHAR *)tempbuf + wch;
    // Remove all 0's, except leave one if there are other extensions
    while ( (pwstr > tempbuf) && (*(pwstr-1) == (WCHAR)0))
        pwstr--;
    if ( (pwstr > tempbuf) && (*(pwstr-1) != (WCHAR)0))
        *pwstr++ = (WCHAR) 0;
    mdRec.dwMDDataLen = (pwstr - tempbuf) * sizeof(WCHAR);
    wch = GetModuleFileName(g_hInstance, wszModuleName, MAX_RES_STRING_SIZE);
    if (bIsIIS4)
        wch = swprintf(pwstr, g_ScriptMapWStrIIS4, wszModuleName);
    else
        wch = swprintf(pwstr, g_ScriptMapWStr, wszModuleName);

    // Need two 0's at the end
    pwstr[wch++] = (WCHAR)0;
    pwstr[wch++] = (WCHAR)0;
    mdRec.dwMDDataLen += wch * sizeof(WCHAR);
    CHK(pAdminBase->SetData(
                        hMD,
                        L"/",
                        &mdRec));

Cleanup:
    if (hMD)
        pAdminBase->CloseKey(hMD);
    if (tempbuf)
        delete [] tempbuf;
#else
    HRESULT hr = S_OK;
#endif 
    return hr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: DllUnregisterServer()
//
//  parameters:
//
//  description:
//           Removes entries from the system registry
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
STDAPI DllUnregisterServer(void)
{
#ifndef UNDER_CE
    HRESULT     hr;
    CAutoRefc<IMSAdminBase>  pAdminBase;
    METADATA_HANDLE hMD = 0;
    METADATA_RECORD mdRec;
    DWORD           wch;
    CAutoRg<WCHAR> tempbuf(NULL);
    WCHAR *         pwstr;

    // Skip the un-registration if this is a Win9x platform
    if (g_fIsWin9x)
    	return S_OK;

    RemoveRegInfo();

    // get a pointer to the IIS Admin Base Object
    hr = CoCreateInstance(
                            CLSID_MSAdminBase,
                            NULL, CLSCTX_ALL,
                            IID_IMSAdminBase,
                            (void **) &pAdminBase);

	if (hr != S_OK)
	{
		// No IIS is installed, return silently
		return S_OK;
	}

    // Obtain the Metadata File extensions list
    CHK(GetMetadataExtensions(pAdminBase, &hMD, &mdRec, &tempbuf));

    // Remove previously existing .soap extension
    wch = mdRec.dwMDDataLen / sizeof(WCHAR);
    CHK ( RemoveSoapExt(tempbuf, &wch) );

    // Save the new data
    if (wch <= 1)
    {
        // If the set is empty, the server will not accept a single NULL
        tempbuf[0] = 0;
        tempbuf[1] = 0;
        wch = 2;
    }
    mdRec.dwMDDataLen = wch * sizeof(WCHAR);
    CHK(pAdminBase->SetData( hMD, L"/", &mdRec));

Cleanup:
    if (hMD)
        pAdminBase->CloseKey(hMD);
    return hr;
#else
    return S_OK;
#endif 
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: GetMetadataExtensions(IMSAdminBase *pAdminBase, METADATA_HANDLE *phMD, METADATA_RECORD *pmdRec,
//                                  WCHAR ** ppwstrbuf)
//
//  parameters:
//
//  description:
//           Opens the IIS Metadata and obtains a list of registered file extensions
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef UNDER_CE
HRESULT GetMetadataExtensions(IMSAdminBase *pAdminBase,
                                    METADATA_HANDLE *phMD,
                                    METADATA_RECORD *pmdRec,
                                    WCHAR ** ppwstrbuf)
{
    HRESULT     hr = S_OK;
    DWORD       wch = 0;
    WCHAR *     tempbuf = NULL;

    CHK(pAdminBase->OpenKey(
                        METADATA_MASTER_ROOT_HANDLE,
                        L"/LM/W3SVC",
                        METADATA_PERMISSION_READ|METADATA_PERMISSION_WRITE,
                        10000,
                        phMD));

    pmdRec->dwMDIdentifier = MD_SCRIPT_MAPS;
    pmdRec->dwMDAttributes = METADATA_INHERIT;
    pmdRec->dwMDUserType   = IIS_MD_UT_FILE;
    pmdRec->dwMDDataType   = MULTISZ_METADATA;
    pmdRec->dwMDDataLen    = 0;
    pmdRec->pbMDData       = (BYTE *)tempbuf;
    pmdRec->dwMDDataTag    = 0;

    hr = pAdminBase->GetData(
                        *phMD,
                        L"/",
                        pmdRec,
                        &wch);

    if (HRESULT_CODE(hr) == ERROR_INSUFFICIENT_BUFFER)
    {
        // Allocate extra space for new .soap string addition
        tempbuf = (WCHAR *) new WCHAR[wch/sizeof(WCHAR) + MAX_RES_STRING_SIZE + 256];
        CHK_BOOL (tempbuf, E_OUTOFMEMORY);

        pmdRec->pbMDData = (BYTE *)tempbuf;
        pmdRec->dwMDDataLen = wch;
        pmdRec->dwMDDataTag = 0;

        CHK(pAdminBase->GetData(*phMD, L"/", pmdRec, &wch));
    }

Cleanup:
    *ppwstrbuf = tempbuf;
    return hr;

}
#endif 

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: RemoveSoapExt(WCHAR * pwchbuf, DWORD *pwch)
//
//  parameters:
//
//  description:
//           Removes the entry .soap extension from the given string buffer and returns the new length
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT RemoveSoapExt(WCHAR * pwchbuf, DWORD *pwch)
{
    WCHAR   *pwstr;
    WCHAR   *pwstr2;
#ifndef UNDER_CE
    DWORD   idx;
#endif 

    pwstr = pwchbuf;
    while (pwstr < (pwchbuf + *pwch - 6))
    {
        if (wcsncmp(pwstr, L".wsdl,", 6) == 0)
        {
            // Found the extension
            pwstr2 = pwstr;
            while ( (pwstr < (pwchbuf + *pwch)) && *pwstr && (*pwstr != (WCHAR)0))
                pwstr++;
            if (*pwstr == (WCHAR)0)
                pwstr++;
            // Copy over the rest of the string
            while (pwstr < (pwchbuf + *pwch))
            {
                *pwstr2++ = *pwstr++;
            }
            *pwch = pwstr2 - pwchbuf;
            return S_OK;
        }
        while (pwstr < (pwchbuf + *pwch)  && (*pwstr != (WCHAR)0))
            pwstr++;
        if (pwstr < (pwchbuf + *pwch))
            pwstr++;    // Skip the NULL character
    }

    // No .soap extension
    return S_FALSE;

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: IsIISVersion4(WCHAR * pwchbuf, DWORD wch)
//
//  parameters:
//
//  description:
//           Checks to see if we are running on IIS version 4 or 5
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef UNDER_CE
BOOL IsIISVersion4(WCHAR * pwchbuf, DWORD wch)
{
    WCHAR   *pwstr;
    WCHAR   *pwstr2;

    // IIS version 5 lists only the allowed verbs, while IIS version 4 lists only
    //      the disallowed verbs
    // Find any of the .asp, .idc, .idq, .shtm extensions. If they list
    //  'GET' then we are running on version 5 or later.

    pwstr = pwchbuf;
    while (pwstr < (pwchbuf + wch - 6))
    {
        if ((_wcsnicmp(pwstr, L".htw,", 5) == 0) ||
            (_wcsnicmp(pwstr, L".asp,", 5) == 0) ||
            (_wcsnicmp(pwstr, L".idc,", 5) == 0) ||
            (_wcsnicmp(pwstr, L".idq,", 5) == 0) ||
            (_wcsnicmp(pwstr, L".shtm,", 6) == 0) ||
            (_wcsnicmp(pwstr, L".asa,", 5) == 0))
        {
            // Skip the extension part
            while ( (pwstr < (pwchbuf + wch)) && *pwstr && (*pwstr != (WCHAR)','))
                pwstr++;
            if (*pwstr == (WCHAR)',')
            {
                pwstr++;

                // Skip the file path
                while ( (pwstr < (pwchbuf + wch)) && *pwstr && (*pwstr != (WCHAR)','))
                    pwstr++;
                if (*pwstr == (WCHAR)',')
                {
                    pwstr++;
                    // Skip the number
                    while ( (pwstr < (pwchbuf + wch)) && *pwstr && (*pwstr != (WCHAR)','))
                        pwstr++;
                    if (*pwstr == (WCHAR)',')
                    {
                        pwstr++;
                        // We now have the list of verbs
                        pwstr2 = wcsstr(pwstr, L"GET");
                        if (!pwstr2)
                            pwstr2 = wcsstr(pwstr, L"get");
                        if (pwstr2 && ((pwstr2[3] == (WCHAR)',') || (pwstr2[3] == (WCHAR)0)))
                        {
                            // ALLOWED verbs are listed, so it must be IIS version 5
                            return FALSE;
                        }
                    }
                }
            }
        }
        while (pwstr < (pwchbuf + wch)  && (*pwstr != (WCHAR)0))
            pwstr++;
        if (pwstr < (pwchbuf + wch))
            pwstr++;    // Skip the NULL character
    }

    return TRUE;
}
#endif 


#ifdef UNDER_CE
DWORD TerminateISAPIThread(LPVOID param)
{
     if (FAILED(CoInitializeEx(NULL,COINIT_MULTITHREADED)))
     {
        DEBUGMSG(SOAP_ISAPI_DBG,(L"[SOAPISAPI] DLL_PROCESS_ATTACH -- cant CoInitialize\n"));
        return FALSE;
     }

     DEBUGMSG(SOAP_ISAPI_DBG,(L"[SOAPISAPI] starting to sleep on terminate event... ZZZzzzz\n"));    
     #ifdef DEBUG          
     DWORD dwRet = 
     #endif
     WaitForSingleObject(g_hTerminateEvent, INFINITE);
     ASSERT(dwRet == WAIT_OBJECT_0);  
     DEBUGMSG(SOAP_ISAPI_DBG,(L"[SOAPISAPI] terminate thread AWAKE\n"));              
     
     EnterCriticalSection(&g_cs);

     if (--g_cExtensions == 0)
     {           
       TerminateThreadPool();
     }
    
     LeaveCriticalSection(&g_cs);
     CoUninitialize();  

     DEBUGMSG(SOAP_ISAPI_DBG,(L"[SOAPISAPI] terminate thread waking up terminate extension\n"));              
     SetEvent(g_hTerminateEvent);
     return 0;
}


#ifdef DEBUG
//note: these are all represented as bits, the first in the list is the least sig
//  of the 16 
DBGPARAM dpCurSettings = {
    TEXT("SoapISAPI"), {
    /*D*/
    TEXT("General Debug"),    
    TEXT("Process Attach"), 
    TEXT(""), 
    TEXT(""),
    /*C*/
    TEXT(""),
    TEXT(""), 
    TEXT(""),
    TEXT(""),
    /*B*/
    TEXT(""),
    TEXT(""),
    TEXT(""),
    TEXT(""),
    /*A*/
    TEXT(""),
    TEXT(""),
    TEXT(""),
    TEXT("") 
    },0xFFFF
    //0x060A
    //0x0448
    //0x0608
    //0x0408
    //0x0880
    //0x0E34
    /*0xABCD*/
};
//0x4080 does 7 and 14 (header collection and first empty...remember to start cnting at 0)
#endif

#endif

