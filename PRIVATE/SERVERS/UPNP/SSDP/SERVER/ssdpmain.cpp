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

#include <bldver.h>
#include "notify.h"
#include "url_verifier.h"
#include "upnp_config.h"

LONG cInitialized = FALSE; 

url_verifier* g_pURL_Verifier;

CRITICAL_SECTION g_csSSDP;

extern SOCKET g_socketTcp;
extern LONG bShutdown;

bool init_url_verifier();

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
        
    	if (!HttpRequest::Initialize("Windows CE %d.%d UPnP device", CE_MAJOR_VER, CE_MINOR_VER))
    	{
    	    goto cleanup;
    	}
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
    ASSERT(dwCode == SSDP_IOCTL_INVOKE);
    
    LONG            status = ERROR_NOT_SUPPORTED;
    ce::psl_stub<>  stub(pBufIn, dwLenIn);

    switch(stub.function())
    {
        case SSDP_IOCTL_REGISTERNOTIFICATION:
        
    	    return stub.call(RegisterNotificationSink, (HANDLE)dwOpenData);
    	    
        case SSDP_IOCTL_DEREGISTERNOTIFICATION:
        
            return stub.call(DeregisterNotificationSink);

        case SSDP_IOCTL_REGISTER_SERVICE:
        
            return stub.call(RegisterUpnpServiceImpl);

        case SSDP_IOCTL_DEREGISTER_SERVICE:
        
            return stub.call(DeregisterUpnpServiceImpl);
    }
    
    if (status != NO_ERROR)
    	SetLastError(status);
    	
    return !status;
}
