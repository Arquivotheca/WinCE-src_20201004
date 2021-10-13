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
/*--
Module Name: ISAPI.H
Abstract: ISAPI Exten & Filter handling classes
--*/


typedef enum {
    SCRIPT_TYPE_NONE = 0,
    SCRIPT_TYPE_EXTENSION,
    SCRIPT_TYPE_ASP,
    SCRIPT_TYPE_FILTER
}
SCRIPT_TYPE;

typedef DWORD (WINAPI *PFN_HTTPFILTERPROC)(HTTP_FILTER_CONTEXT* pfc, DWORD NotificationType, VOID* pvNotification);
typedef BOOL  (WINAPI *PFN_GETFILTERVERSION)(HTTP_FILTER_VERSION* pVer);
typedef BOOL  (WINAPI *PFN_TERMINATEFILTER)(DWORD dwFlags);
typedef void  (WINAPI *PFN_TERMINATEASP)();
typedef DWORD (WINAPI *PFN_EXECUTEASP)(PASP_CONTROL_BLOCK pASPBlock);

class CISAPICache; 
BOOL InitExtensions(CReg *pWebsite);
int RecvToBuf(SOCKET socket,PVOID pv, DWORD dw,DWORD dwTimeout);

class CISAPI {
friend CISAPICache;

// ISAPI extension & filter handling data members
    SCRIPT_TYPE  m_scriptType;   // Extension, Filter, or ASP?
    HINSTANCE    m_hinst;
    FARPROC      m_pfnGetVersion;
    FARPROC      m_pfnHttpProc;
    FARPROC      m_pfnTerminate;
    DWORD dwFilterFlags;         // Flags negotiated on GetFilterVersion    

    // Used by the caching class 
    PWSTR        m_wszDLLName;   // full path of ISAPI Dll
    DWORD        m_cRef;         // Reference Count
    __int64      m_ftLastUsed;   // Last used, treat as FILETIME struct
    BOOL         m_fCanTimeout;  // If ISAPI is to permanently stay in cache.

public:
    CISAPI(SCRIPT_TYPE st) 
    { 
        ZEROMEM(this); 
        m_scriptType = st; 
    }

    ~CISAPI()     
    { 
        MyFree(m_wszDLLName);
        MyFreeLib(m_hinst);
    }

    void Unload(PWSTR wszDLLName=NULL);
    BOOL Load(PWSTR wszPath);
    void CoFreeUnusedLibrariesIfASP() 
    {
        // Calls into ASP exported function, which calls CoFreeUnusedLibraries
        // but does nothing else.  We don't call CoFreeUnusedLibraries from the web 
        // server directly because we don't want to have to link it to COM.
        // Does not free asp.dll, because we have a reference to it in the server.

        // Do nothing because services.exe spun up thread does this already.
        // ((PFN_TERMINATEASP)m_pfnTerminate)();
        ;
    }

    BOOL LoadExports(HINSTANCE hInst, BOOL fVerifyOnly);
    
    DWORD CallExtension(EXTENSION_CONTROL_BLOCK * pECB)    
    { 
        return ((PFN_HTTPEXTENSIONPROC)m_pfnHttpProc)(pECB); 
    }
    DWORD CallFilter(HTTP_FILTER_CONTEXT* pfc, DWORD n, VOID* pv) 
    { 
        return ((PFN_HTTPFILTERPROC)m_pfnHttpProc)(pfc, n, pv); 
    }
    DWORD CallASP(PASP_CONTROL_BLOCK pACB)  
    { 
        return ((PFN_EXECUTEASP) m_pfnHttpProc)(pACB); 
    }
    DWORD GetFilterFlags()  
    { 
        return dwFilterFlags; 
    }
};


typedef struct _isapi_node 
{
    CISAPI *m_pISAPI;        // class has library and entry point functions
    struct _isapi_node *m_pNext;
} ISAPINODE, *PISAPINODE;

typedef struct _SCRIPT_MAP_NODE 
{
    WCHAR *wszFileExtension;
    WCHAR *wszISAPIPath;
    BOOL  fASP;
} SCRIPT_MAP_NODE, *PSCRIPT_MAP_NODE;

typedef struct _SCRIPT_MAP 
{
    PSCRIPT_MAP_NODE pScriptNodes;
    DWORD            dwEntries;
} SCRIPT_MAP, *PSCRIPT_MAP;


typedef struct _SCRIPT_NOUNLOAD 
{
    WCHAR**               ppszNoUnloadEntry;
    DWORD                 dwEntries;
} SCRIPT_NOUNLOAD, *PSCRIPT_NOUNLOAD;

void InitScriptMapping(PSCRIPT_MAP pTable);
void InitScriptNoUnloadList(PSCRIPT_NOUNLOAD pNoUnloadList);
BOOL MapScriptExtension(WCHAR *wszFileExt, WCHAR **pwszISAPIPath, BOOL *pfIsASP);

class CISAPICache 
{
private:
    PISAPINODE m_pHead;
    CRITICAL_SECTION m_CritSec;

public:
    CISAPICache()     
    { 
        ZEROMEM(this);      
        InitializeCriticalSection(&m_CritSec);
    }
    ~CISAPICache() 
    {
        DeleteCriticalSection(&m_CritSec);
    }

    
    HINSTANCE Load(PWSTR wszDLLName, CISAPI **ppISAPI, SCRIPT_TYPE st=SCRIPT_TYPE_EXTENSION);
    void Unload(CISAPI *pISAPI) 
    {
        SYSTEMTIME st;
        if (NULL == pISAPI)
        {
            return;
        }
            
        EnterCriticalSection(&m_CritSec);
        
        GetSystemTime(&st);
        SystemTimeToFileTime(&st,(FILETIME *) &pISAPI->m_ftLastUsed);
        pISAPI->m_cRef--;

        LeaveCriticalSection(&m_CritSec);
    }

    void RemoveUnusedISAPIs(BOOL fRemoveAll);
    friend DWORD WINAPI RemoveUnusedISAPIs(LPVOID lpv);
};
