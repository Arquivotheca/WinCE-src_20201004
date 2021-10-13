//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <ssdppch.h>
#pragma hdrstop

#include "notify.h"
#include "url_verifier.h"
#include "upnp_config.h"

LONG cInitialized = FALSE; 

url_verifier* g_pURL_Verifier;

CRITICAL_SECTION g_csSSDP;

extern SOCKET g_socketTcp;
extern LONG bShutdown;

bool init_url_verifier();
/*BOOL _LookupCache( LPSTR szType,
                   SERVICE_CALLBACK_FUNC pfCallback,
                   PVOID pvContext);*/

//+---------------------------------------------------------------------------
//
//  Function:   SsdpStartup
//
//  Purpose:    Initializes global state for the SSDP api functions.
//
//  Arguments:  <none>
//
//  Returns:    If the function succeeds, the return value is nonzero.
//
//              If the function fails, the return value is zero.
//              To get extended error information, call GetLastError. 
//

BOOL WINAPI SsdpStartup()
{
    int		iRetVal;

    EnterCriticalSection(&g_csSSDP); 

    iRetVal = FALSE;

    if (!cInitialized)
    {
		init_url_verifier();
		
		InitializeListNetwork();
	    InitializeListAnnounce();
	    //InitializeListCache();
		
		Assert(g_pNotificationMgr == NULL);
		if (!(g_pNotificationMgr = new notification_mgr))
		{
	        TraceTag(ttidInit, "Failed to initialize notify", GetLastError());
	        goto cleanup;
	    }

	    if (FALSE == InitializeListEventSource())
	    {
	        TraceTag(ttidInit, "Failed to initialize EventSource list", GetLastError());
	        goto cleanup;
	    }

	    // SocketInit() returns 0 on success, and places failure codes in
	    // GetLastError()
	    //
	    if (SocketInit() !=0)
	    {
	        goto cleanup;
	    }
    		
	    if (ListenOnAllNetworks(NULL) == FALSE)
	    {
	        goto cleanup;
	    }

        if (StartNetworkMonitorThread()	== FALSE)
        {
            goto cleanup;
        }
        
    	HttpRequest::Initialize("Windows CE 4.0 UPnP device");
    }
	iRetVal = TRUE;
	cInitialized = TRUE;

Exit:
    LeaveCriticalSection(&g_csSSDP); 

    return iRetVal; 
cleanup:
    // we hit an error
    //
    HttpRequest::Uninitialize();

	StopNetworkMonitorThread();
	
	StopListenOnAllNetworks();
	
	SocketFinish();
	
    CleanupListEventSource();

	delete g_pNotificationMgr;
	g_pNotificationMgr = NULL;
	
    //DestroyListCache();

    CleanupListAnnounce();

    CleanupListNetwork();
    
    delete g_pURL_Verifier;
    g_pURL_Verifier = NULL;

	goto Exit;			
}


// SsdpCleanup
VOID WINAPI SsdpCleanup()
{
    EnterCriticalSection(&g_csSSDP); 

    if (cInitialized)
    {
        TraceTag(ttidInit,"SsdpCleanup - cleaning up\n");
        
        bShutdown = 1;
        cInitialized = FALSE;
        
    	HttpRequest::Uninitialize();

		StopNetworkMonitorThread();

		StopListenOnAllNetworks();

        SocketFinish();

	    CleanupListEventSource();

		delete g_pNotificationMgr;
		g_pNotificationMgr = NULL;

		//DestroyListCache();

		CleanupListAnnounce();

		CleanupListNetwork();
		
		delete g_pURL_Verifier;
		g_pURL_Verifier = NULL;
    }

	bShutdown = 0;
	
    LeaveCriticalSection(&g_csSSDP); 
}


// init_url_verifier
bool init_url_verifier()
{
	Assert(!g_pURL_Verifier);

	g_pURL_Verifier = new url_verifier;
	
	if(!g_pURL_Verifier)
	    return false;

	int scope = upnp_config::scope();
	
	if(scope >= 1)
		g_pURL_Verifier->allow_private();
    	
	if(scope >= 2)
		g_pURL_Verifier->allow_ttl(upnp_config::TTL());
    	
	if(scope >= 3)
		g_pURL_Verifier->allow_any();
		
    g_pURL_Verifier->allow_site_scope(upnp_config::site_scope());
		
    return true;
}


#define MAP(x) MapPtrToProcess(x, hMappedProc)

/*static void CopyMappedSsdpMessage(
	PSSDP_MESSAGE pMappedMsg,
	PSSDP_MESSAGE pMsg,
	HANDLE hMappedProc)
{
	pMsg = (PSSDP_MESSAGE )MAP(pMsg);
	pMappedMsg->szType = (LPSTR)MAP(pMsg->szType);
	pMappedMsg->szLocHeader = (LPSTR)MAP(pMsg->szLocHeader);
	pMappedMsg->szAltHeaders = (LPSTR)MAP(pMsg->szAltHeaders);
	pMappedMsg->szUSN = (LPSTR)MAP(pMsg->szUSN);
	pMappedMsg->szSid = (LPSTR) MAP(pMsg->szSid);
	pMappedMsg->iSeq = pMsg->iSeq;
	pMappedMsg->iLifeTime = pMsg->iLifeTime;
}*/

BOOL WINAPI
SDP_IOControl(
    DWORD  dwOpenData, 
    DWORD  dwCode, 
    PBYTE  pBufIn,
    DWORD  dwLenIn, 
    PBYTE  pBufOut, 
    DWORD  dwLenOut,
    PDWORD pdwActualOut
    )
{

    LONG status = NO_ERROR;
    HANDLE hMappedProc = GetCallerProcess();
    switch (dwCode)
    {
    case SSDP_IOCTL_REGISTERNOTIFICATION:
    {
    	struct RegisterNotificationParams *pParms = (struct RegisterNotificationParams *) pBufIn;
    	
		ASSERT(dwLenIn == sizeof(struct RegisterNotificationParams));
    	
		*((HANDLE*)MAP(pParms->phNotify)) = RegisterNotificationSink(pParms->nt,
																	 (LPCWSTR)MAP((void*)pParms->pwszUSN),
																	 (LPCWSTR)MAP((void*)pParms->pwszQueryString),
																	 (LPCWSTR)MAP((void*)pParms->pwszMsgQueue),
                                                                     (HANDLE)dwOpenData);
    	break;
    }
    case SSDP_IOCTL_DEREGISTERNOTIFICATION:
    {
    	struct DeregisterNotificationParams *pParms = (struct DeregisterNotificationParams *) pBufIn;
    	
		ASSERT(dwLenIn == sizeof(struct DeregisterNotificationParams));
		
		DeregisterNotificationSink(pParms->hNotify);

		break;
    }
    case SSDP_IOCTL_GETSUBSCRIPTIONID:
    {
    	status = ERROR_NOT_SUPPORTED;

    	break;
    }
    /*case SSDP_IOCTL_LOOKUPCACHE:
    {
    	struct LookupCacheParams *pParms = (struct LookupCacheParams *)pBufIn;
    	ASSERT(dwLenIn == sizeof(struct LookupCacheParams));
    	
		if (!_LookupCache(
			(LPSTR)MAP(pParms->szType),
			pParms->pfSearchCallback,
			pParms->pvContext))
		{
			status = ERROR_FILE_NOT_FOUND;
		}

		break;
			
    }
    case SSDP_IOCTL_UPDATECACHE:
    {
    	struct UpdateCacheParams *pParms = (struct UpdateCacheParams *)pBufIn;
    	ASSERT(dwLenIn == sizeof(struct UpdateCacheParams));
    	SSDP_MESSAGE msg;
    	CopyMappedSsdpMessage(&msg, pParms->pSsdpMessage, GetCallerProcess());
    	_UpdateCache(&msg);

    	break;
    }
    case SSDP_IOCTL_CLEANUPCACHE:
    {
		_CleanupCache();

		break;
    }*/
    
    default:
    {
    	status = ERROR_NOT_SUPPORTED;
    	break;
    }

    }
    if (status != NO_ERROR)
    	SetLastError(status);
    return !status;
}

/*BOOL _LookupCache(
                   LPSTR szType,
                   SERVICE_CALLBACK_FUNC pfCallback,
                   PVOID pvContext)
{
	SSDP_MESSAGE **pMsgList;
	SSDP_MESSAGE *pMsg = NULL;
	INT nHits, i;
	
	CALLBACKINFO cbi = {GetCallerProcess(), (FARPROC)pfCallback, NULL};
	
    nHits =  SearchListCache(szType, &pMsgList);

    if (!nHits)
    	return FALSE;

	pMsg = new SSDP_MESSAGE;
	if (!pMsg)
	{
		FreeSsdpMessageList(pMsgList, nHits);
		return FALSE;
	}

    	
    for (i=0; i < nHits; i++) {
    	__try {
    		if (pMsgList[i]) {
    			CopyMappedSsdpMessage(pMsg, pMsgList[i], GetCurrentProcess());
    			TraceTag(1, "pMsg = 0x%08x.  Mapped pMsg = 0x%08x\n", pMsg, MapPtrToProcess(pMsg, GetCurrentProcess()) );
    			PerformCallBack(&cbi, (SSDP_MESSAGE *)MapPtrToProcess(pMsg, GetCurrentProcess()), pvContext);
    		}
    	} 
    	__except (EXCEPTION_EXECUTE_HANDLER)
    	{
    		TraceTag(ttidError,"Exception n LookupCache Callback\n");
    	}
    }
    FreeSsdpMessageList(pMsgList, nHits);
	delete pMsg;
    return TRUE;
}*/




