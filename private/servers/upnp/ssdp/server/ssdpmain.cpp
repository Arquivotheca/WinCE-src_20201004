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
#include <ssdppch.h>
#pragma hdrstop

#include <bldver.h>
#include "notify.h"
#include "url_verifier.h"
#include "upnp_config.h"
#include "dlna.h"

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
    int     iRetVal;

    EnterCriticalSection(&g_csSSDP); 

    iRetVal = FALSE;

    if (!cInitialized)
    {
        init_url_verifier();
        
        InitializeListNetwork();
        InitializeListAnnounce();
        InitializeListEventSource();
        //InitializeListCache();
        
        Assert(g_pNotificationMgr == NULL);
        if (!(g_pNotificationMgr = new notification_mgr))
        {
            TraceTag(ttidInit, "Failed to initialize notify", GetLastError());
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

        if (StartNetworkMonitorThread() == FALSE)
        {
            goto cleanup;
        }
        
        if (!HttpBase::Initialize("Microsoft-Windows-CE/%d.%02d UPnP control point DLNADOC/%d.%02d", 
                                CE_MAJOR_VER, CE_MINOR_VER, DLNA_MAJOR_VER, DLNA_MINOR_VER))
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
    HttpBase::Uninitialize();

    StopNetworkMonitorThread();
    
    StopListenOnAllNetworks();
    
    SocketFinish();
    
    CleanupListEventSource();

    if(g_pNotificationMgr)
    {
        delete g_pNotificationMgr;
    }
    g_pNotificationMgr = NULL;
    
    //DestroyListCache();

    CleanupListAnnounce();

    CleanupListNetwork();

    if(g_pURL_Verifier)
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

        HttpBase::Uninitialize();

        StopNetworkMonitorThread();

        StopListenOnAllNetworks();

        SocketFinish();

        CleanupListEventSource();

        if(g_pNotificationMgr)
        {
            delete g_pNotificationMgr;
        }
        g_pNotificationMgr = NULL;

        //DestroyListCache();

        CleanupListAnnounce();

        CleanupListNetwork();

        if(g_pURL_Verifier)
        {
            delete g_pURL_Verifier;
        }
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
    {
        return false;
    }

    int scope = upnp_config::scope();
    
    if(scope >= 1)
    {
        g_pURL_Verifier->allow_private();
    }
        
    if(scope >= 2)
    {
        g_pURL_Verifier->allow_ttl(upnp_config::TTL());
    }
        
    if(scope >= 3)
    {
        g_pURL_Verifier->allow_any();
    }
        
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
        
            return stub.call(RegisterUpnpServiceIoctl);

        case SSDP_IOCTL_DEREGISTER_SERVICE:
        
            return stub.call(DeregisterUpnpServiceImpl);
    }
    
    if (status != NO_ERROR)
    {
        SetLastError(status);
    }
        
    return !status;
}
