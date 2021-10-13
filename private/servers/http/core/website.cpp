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
Module Name: Website.CPP
Abstract:    Maps IP addresses to a particular website
--*/


#include "httpd.h"


// 
// iphlpapi helper routines
//


//
// Init/deinitialization of web server
//
void CleanupGlobalMemory(DWORD dwWebsites) 
{
    EnterCriticalSection(&g_CritSect);

#if defined (DEBUG) || defined (_DEBUG)
    DEBUGCHK(g_fState != SERVICE_STATE_ON);
    
    if (!g_pWebsites)
    {
        DEBUGCHK(g_cWebsites == 0);
    }

    DEBUGCHK(dwWebsites <= g_cWebsites);
    if (g_pWebsites)
    {
        DEBUGCHK(g_pVars);
    }

    if (dwWebsites == 0)
    {
        DEBUGCHK(g_cWebsites == 0);
    }
#endif

    CHttpRequest::DeleteAllRequestObjects();

    // We must cleanup any non-default websites first.
    // We use dwWebsites and not g_cWebsites as the index counter in the event 
    // that something went wrong on startup and not all the values are initialized.

    for (DWORD i = 0; i < dwWebsites; i++)
    {
        g_pWebsites[i].DeInitGlobals();
    }

    MyFree(g_pWebsites);
    g_cWebsites = 0;

    if (g_pVars) 
    {
        delete g_pVars;
        g_pVars = NULL;
    }
    LeaveCriticalSection(&g_CritSect);
}


BOOL InitializeGlobals() 
{
    EnterCriticalSection(&g_CritSect);
    BOOL fRet = FALSE;
    CReg webSites;
    DWORD dwWebsites;

    DEBUGCHK (g_fState == SERVICE_STATE_STARTING_UP && !g_pVars && !g_pRequestList);
    g_pRequestList = NULL;

    svsutil_ResetInterfaceMapper();

    g_pVars = new CGlobalVariables(NULL,L"");

    if (NULL == g_pVars || !g_pVars->m_fAcceptConnections) 
    {
        SetWebServerState(SERVICE_STATE_SHUTTING_DOWN);
        CleanupGlobalMemory(0);
        goto done;
    }

    // Filters may make reference to logging global var, so make call to filters 
    // outside constructor.  Filters are global and are not setup per-website, so this
    // is the only time we call this function.
    g_pVars->m_fFilters = InitFilters();  

    webSites.Open(HKEY_LOCAL_MACHINE,RK_WEBSITES);

    if (webSites.IsOK() && (0 != (dwWebsites = webSites.NumSubkeys()))) 
    {
        WCHAR szSiteName[MAX_PATH];
        if (NULL == (g_pWebsites = MyRgAllocNZ(CGlobalVariables,dwWebsites))) 
        {
            CleanupGlobalMemory(0);

            SetWebServerState(SERVICE_STATE_OFF);
            goto done;
        }

        for (DWORD i = 0; i < dwWebsites; i++) 
        {
            webSites.EnumKey(szSiteName,SVSUTIL_ARRLEN(szSiteName));
            CReg subReg(webSites,szSiteName);

            if (! subReg.IsOK())
            {
                break;
            }

            g_pWebsites[i].InitGlobals(&subReg,szSiteName);

            if (! g_pWebsites[i].m_fAcceptConnections)
            {
                break;
            }
            g_cWebsites++;
        }

        if (g_cWebsites != i) 
        {
            CleanupGlobalMemory(i);
            SetWebServerState(SERVICE_STATE_OFF);
            goto done;
        }
    }

    SetWebServerState(SERVICE_STATE_ON);

    fRet = TRUE;
done:
    LeaveCriticalSection(&g_CritSect);
    return fRet;
}

// Called on incoming connection, maps to appropriate website.
CGlobalVariables* MapWebsite(SOCKET sock, PSTR szHostHeader, CVRoots **ppVRootTable) 
{
    CGlobalVariables       *pWebsite = NULL;

    PCSTR szAdapterName;
    DWORD i;

    *ppVRootTable = NULL;

    // Need to hold critical section because HandleNotifyAddrChange() can come in 
    // at any time with an IP address notification change.
    EnterCriticalSection(&g_CritSect);
    svsutil_LockInterfaceMapper();

    szAdapterName = svsutil_GetAdapterNameOfConnection(sock);

    if (g_pVars->IsMapped(szAdapterName,szHostHeader)) 
    {
        pWebsite = g_pVars;
        goto done;
    }

    for (i = 0; i < g_cWebsites; i++) 
    {
        if (g_pWebsites[i].IsMapped(szAdapterName,szHostHeader)) 
        {
            pWebsite = &g_pWebsites[i];
            goto done;
        }
    }
    DEBUGCHK(pWebsite == NULL);

done:
    if (!pWebsite) 
    {
        // If no website was found, potentially use default.
        pWebsite = g_pVars->m_fUseDefaultSite ? g_pVars : NULL;
    }

    if (pWebsite) 
    {
        // Request object keeps a reference to VRoot table because 
        // apps can refresh the Website's table in middle of the request.
        pWebsite->m_pVroots->AddRef();
        *ppVRootTable = pWebsite->m_pVroots;
    }

    svsutil_UnLockInterfaceMapper();
    LeaveCriticalSection(&g_CritSect);
    return pWebsite;
}


