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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//

// Abstract: Location framework core logic

#include <locCore.hpp>


//
//  Location framework debug info
//

#if defined(UNDER_CE) && defined (DEBUG)
  DBGPARAM dpCurSettings = {
    TEXT("LFSVC"), {
    TEXT("Init"),TEXT("Service"),TEXT("Provider"),TEXT("Resolver"),
    TEXT("Report Collector"),TEXT("Plugin Mgr"),TEXT("API"),TEXT("Thread"),
    TEXT(""),TEXT(""),TEXT(""),TEXT("Verbose"),
    TEXT("Alloc"),TEXT("Function"),TEXT("Error"),TEXT("Warn") },
    0xC000
  }; 
#endif

// 
//  Define registry key and value constants
//

const WCHAR *g_rkLocBase   = L"System\\CurrentControlSet\\Location Framework";
const WCHAR *g_rkProviders = L"Providers";
const WCHAR *g_rkResolvers = L"Resolvers";

const WCHAR *g_rvDll                     = L"DLL";
const WCHAR *g_rvFriendlyName            = L"FriendlyName";
const WCHAR *g_rvGuid                    = L"GUID";
const WCHAR *g_rvPreference              = L"Preference";
const WCHAR *g_rvPluginFlags             = L"PluginFlags";
const WCHAR *g_rvVersion                 = L"Version";
const WCHAR *g_rvProviderFlags           = L"ProviderFlags";
const WCHAR *g_rvResolverFlags           = L"ResolverFlags";
const WCHAR *g_rvPollInterval            = L"PollInterval";
const WCHAR *g_rvRetryOnFailure          = L"RetryOnFailure";
const WCHAR *g_rvMaximumInitialWait      = L"MaximumInitialWait";
const WCHAR *g_rvGeneratedReports        = L"GeneratedReports";
const WCHAR *g_rvSupportedReports        = L"SupportedReports";
const WCHAR *g_rvMinimumRequery          = L"MinimumRequery";

//
//  Global variables
//

// Global lock
SVSSynch *g_pLocLock;
// Thread pool 
SVSThreadPool *g_pThreadPool;
// Current service state of location framework (from service.h)
DWORD g_serviceState;
// Maps handles between application layer and this service
handleMgr_t *g_pHandleMgr;

// Wait this many milliseconds on each poll to see if we can unload LF.
const DWORD g_pollOnUnload = 100;
// When trying to unload, if we have to go into a wait loop > 50 times, DEBUGCHK
const DWORD g_pollRetriesWarnOnUnload = 50;
// Maximum number of simultaneous worker threads for location framework.
static const DWORD g_maxWorkerThreads = 40;

//
//  Startup and shutdown related functionality
//

// Initializes service.  Only called once from DllMain().  All functions
// called here MUST be DllMain safe.
BOOL InitLocService(void) {
    // Init global critical section, other truly global stuff...
    svsutil_Initialize();
 
    g_pLocLock = new SVSSynch();
    if (g_pLocLock == NULL) {
        DEBUGMSG_OOM();
        return FALSE;
    }

    g_pHandleMgr = new handleMgr_t();
    if ((g_pHandleMgr == NULL) || (!g_pHandleMgr->IsHandleMgrInited())) {
        DEBUGMSG_OOM();
        return FALSE;
    }

    g_pThreadPool = new SVSThreadPool(g_maxWorkerThreads);
    if (g_pThreadPool == NULL) {
        DEBUGMSG_OOM();
        delete g_pLocLock;
        return FALSE;
    }

    g_serviceState = SERVICE_STATE_UNINITIALIZED;
    return TRUE;
}

// DeInitializes service.  Only called once from DllMain().  Make sure
// everything is DllMain safe.
void DeInitLocService(void) {
    DEBUGCHK(g_serviceState == SERVICE_STATE_UNLOADING);

#ifdef SVSUTIL_DEBUG_THREADS
	// DEBUG verify that that threadpool has no running threads.  
    // This function is called from DllMain() where we must not block 
	SVS_THREAD_POOL_STATS threadPoolStats;
	g_pThreadPool->GetStatistics(&threadPoolStats);
	DEBUGCHK(threadPoolStats.cThreadsRunning == 0);
#endif

    if (g_pThreadPool) {
        delete g_pThreadPool;
        g_pThreadPool = NULL;
    }

    if (g_pHandleMgr) {
        delete g_pHandleMgr;
        g_pHandleMgr = NULL;
    }

    if (g_pLocLock) {
        delete g_pLocLock;
        g_pLocLock = NULL;
    }

    svsutil_DeInitialize();
}

// Frees globals that are created/destroyed when service is stopped/started
// (but not loaded or unloaded).
void FreeLocGlobals() {
    DEBUGCHK(g_pLocLock->IsLocked());
    DEBUGCHK((g_serviceState != SERVICE_STATE_ON) && (g_serviceState != SERVICE_STATE_STARTING_UP));

    // Need to clear out any existing handles at this stage, but do NOT
    // delete this object since we still need it to process LOC_Open()
    // service handles
    g_pHandleMgr->ResetLocHandles();

    if (g_pPluginMgr) {
        delete g_pPluginMgr;
        g_pPluginMgr = NULL;
    }
}

// Takes service from stopped->on state
static DWORD StartLocService(BOOL firstStartup) {
    DEBUGMSG(ZONE_INIT,(L"LOC: Starting Location Framework Service\r\n"));
    DEBUGCHK(g_pLocLock->IsLocked());

    DWORD err;

    if (! firstStartup) {
        // The first time we're started, state=SERVICE_STATE_STARTING_UP.
        // After that, we need to verify we're not already on or shutting down
        // or whatever before begining service startup.
        if (g_serviceState != SERVICE_STATE_OFF) {
            DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: Starting service fails because service state must be off, but is instead <%d>\r\n",g_serviceState));
            err = ERROR_SERVICE_CANNOT_ACCEPT_CTRL;
            // Do not cleanup global state in this case.
            goto doneNoCleanup;
        }
        g_serviceState = SERVICE_STATE_STARTING_UP;
    }

    DEBUGCHK(g_serviceState == SERVICE_STATE_STARTING_UP);

    //
    // Initialize global objects 
    //
    g_pPluginMgr = new pluginMgr_t();
    if (g_pPluginMgr == NULL)  {
        DEBUGMSG_OOM();
        err = ERROR_OUTOFMEMORY;
        goto done;
    }

    if (! g_pPluginMgr->IsPlgMgrInited()) {
        // Init normally fails because of some misconfiguration in the system
        // such as the LF registry key not being present.
        err = ERROR_INTERNAL_ERROR;
        goto done;
    }

    err = ERROR_SUCCESS;
    g_serviceState = SERVICE_STATE_ON;
done:
    if (err != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: Starting location service failed.  Error=0x%08x\r\n",err));
        g_serviceState = SERVICE_STATE_OFF;
        FreeLocGlobals();
    }
    else
        DEBUGMSG(ZONE_INIT,(L"LOC: Location Framework Service started successfully\r\n"));

doneNoCleanup:
    return err;
}

// Do real work of stopping and starting location framework on a worker
// thread because some plugins may do unsafe operations on PSL thread
// of caller (in particular COM + TLS)
DWORD WINAPI StartLocServiceThread(LPVOID lpv) {
    g_pLocLock->Lock();
    DWORD err = StartLocService((BOOL)lpv);
    DEBUGCHK(g_serviceState==SERVICE_STATE_ON || g_serviceState==SERVICE_STATE_OFF);
    g_pLocLock->Unlock();

    return err;
}

// Takes service from the running state->off/unloading state
static DWORD StopLocService(BOOL unloadService) {
    DEBUGMSG(ZONE_INIT,(L"LOC: Stopping Location Framework Service\r\n"));
    DEBUGCHK(g_pLocLock->IsLocked());

    DWORD err;

    if (unloadService) {
        // When service is requested to unload, then we MUST block until
        // service can be unloaded (and particular until worker threads are stopped)
        // If service was starting up or shutting down already, wait until it is complete
        // before continuing with shutdown

#ifdef DEBUG
		// If it takes more than 20 cycles through loop below to get into a steady
		// state, then something is likely wrong with LF.
		DWORD pollAttempts = 0;
#endif

		while ((g_serviceState != SERVICE_STATE_ON) && (g_serviceState != SERVICE_STATE_OFF)) {
#ifdef DEBUG
			pollAttempts++;
			DEBUGCHK(g_pollRetriesWarnOnUnload != pollAttempts);
#endif
			g_pLocLock->Unlock();
			DEBUGMSG(ZONE_SERVICE,(L"LF: Sleeping <%d> ms on LF unload, waiting to be in state off or state on\r\n",g_pollOnUnload));
			Sleep(g_pollOnUnload);
			g_pLocLock->Lock();
		}
    }
    else {
        // If service is not being unloaded but just stopped (i.e. IOCTL_SERVICE_STOP),
        // then service must be ON in order to proceed.
        if (g_serviceState != SERVICE_STATE_ON) {
            DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: Attempt to stop service fails since state is not ON, state is <%d>\r\n",g_serviceState));
            err = ERROR_SERVICE_CANNOT_ACCEPT_CTRL;
            goto done;
        }
    }

    g_serviceState = unloadService ? SERVICE_STATE_UNLOADING : SERVICE_STATE_SHUTTING_DOWN;

    FreeLocGlobals();

#ifdef SVSUTIL_DEBUG_THREADS
	// DEBUG verify that that threadpool has no running threads.  
    // Deleting g_pPluginMgr should wait until all workers have stopped.
	SVS_THREAD_POOL_STATS threadPoolStats;
	g_pThreadPool->GetStatistics(&threadPoolStats);
	DEBUGCHK(threadPoolStats.cThreadsRunning == 0);
#endif

    // The thread pool may have idle worker threads still available.
    // Make these close down now also.  This is needed because on a service
    // DeInit(), the thread pool will try to destroy these threads which
    // can lead to deadlock should we be trying this in DllMain().
    g_pThreadPool->Shutdown();
    g_pThreadPool->Reset();

    if (!unloadService)
        g_serviceState = SERVICE_STATE_OFF;

    err = ERROR_SUCCESS;
done:
    return err;
}

// Stop location framework on a worker thread in services.exe --
// see StartLocServiceThread comments
DWORD WINAPI StopLocServiceThread(LPVOID lpv) {
    g_pLocLock->Lock();
    DWORD err = StopLocService((BOOL)lpv);
#ifdef DEBUG
    if (lpv) { // unloading service
        DEBUGCHK(g_serviceState==SERVICE_STATE_UNLOADING);
    }
    else {
        DEBUGCHK(g_serviceState==SERVICE_STATE_ON || g_serviceState==SERVICE_STATE_OFF);
    }
#endif
    g_pLocLock->Unlock();

    return err;
}

// Refresh location framework on a worker thread in services.exe --
// see StartLocServiceThread comments
DWORD WINAPI RefreshLocServiceThread(LPVOID lpv) {
    g_pLocLock->Lock();
    DEBUGMSG(ZONE_INIT,(L"LOC: Refreshing Location Framework Service\r\n"));
    DWORD err = StopLocService(FALSE);
    if (err == ERROR_SUCCESS)
        err = StartLocService(FALSE);

    g_pLocLock->Unlock();
    return err;
}

// Spin up a thread with the specified function, blocking until it has completed.
BOOL LocationSpinThreadAndWait(PFN_LOCATION_THREAD pFunc, LPVOID lpv) {
    DEBUGCHK(! g_pLocLock->IsLocked());
    HANDLE hThread = CreateThread(NULL,0,pFunc,lpv,0,NULL);

    if (! hThread) {
        DEBUGMSG(ZONE_ERROR,(L"LOCATION: CreateThread failed, GLE=<0x%08x>\r\n",GetLastError()));
        return ERROR_OUTOFMEMORY;
    }

    DWORD err;
    WaitForSingleObject(hThread,INFINITE);

    if (! GetExitCodeThread(hThread,&err)) { 
        DEBUGMSG(ZONE_ERROR,(L"LOCATION: GetExitCodeThread failed, GLE=<0x%08x>\r\n",GetLastError()));
        DEBUGCHK(0); // Something is seriously wrong with system or services.exe itself
        err = ERROR_SERVICE_CANNOT_ACCEPT_CTRL;
    }

    if (err != ERROR_SUCCESS)
        SetLastError(err);

    CloseHandle(hThread);
    return (err==ERROR_SUCCESS);
}




// Implements IOCTL_SERVICE_STATUS processing.
BOOL GetServiceStatus(PBYTE pBufOut, DWORD lenOut, DWORD *pActualLenOut) {
    DEBUGCHK(g_pLocLock->IsLocked());
    if (!pBufOut || lenOut < sizeof(DWORD)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    __try {
        *(DWORD *)pBufOut = g_serviceState;
        if (pActualLenOut)
            *pActualLenOut = sizeof(DWORD);
    }
    __except (REPORT_EXCEPTION_TO_WATSON()) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    return TRUE;
}


// 
//  Handeling of user mode APIs calls
//

BOOL LocationOpenIntrnl(
    serviceHandle_t *pServiceHandle, 
    DWORD version, 
    ce::PSL_HANDLE pReserved, // No PSL marshalling of pReserved since it's only checked vs NULL
    DWORD flags, 
    ce::marshal_arg<ce::copy_out,ce::PSL_HANDLE *> phOutHandle
) 
{
    DEBUGMSG(ZONE_API,(L"LOC: LocationOpen(0x%08x,0x%08x,0x%08x) called\r\n",version,pReserved,flags));
    DEBUGCHK(g_pLocLock->IsLocked());

    if (version != LOCATION_FRAMEWORK_VERSION_1) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: LocationOpen() failed because version = <%d>, not required <%d>\r\n",version,LOCATION_FRAMEWORK_VERSION_1));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if ((pReserved != 0) || (flags != 0)) {
        // pReserved is a PSL_HANDLE because a VOID* because certain VOID* non-NULL vals may get 
        // marshalled as NULL by the PSL marshaller templates.
        DEBUGMSG(ZONE_ERROR,(L"LOC: LocationOpen() failed because pReserved and/or flags are defined\r\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (phOutHandle == NULL) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: LocationOpen() failed because phOutHandle is NULL\r\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    HLOCATION h = g_pHandleMgr->AllocLocationHandle(pServiceHandle);
    if (h == 0)
        return FALSE; // Caller SetLastError()

    *phOutHandle = (ce::PSL_HANDLE)h;

    DEBUGMSG(ZONE_API,(L"LOC: LocationOpen succeeded.  New Location Handle = <0x%08x>\r\n",h));
    return TRUE;
}


BOOL LocationCloseIntrnl(
    serviceHandle_t *pServiceHandle, 
    ce::PSL_HANDLE hLocation
) 
{
    DEBUGMSG(ZONE_API,(L"LOC: LocationClose(0x%08x) called\r\n",hLocation));
    DEBUGCHK(g_pLocLock->IsLocked());

    return (ERROR_SUCCESS == g_pHandleMgr->FreeLocationHandle((HLOCATION)hLocation));
}


BOOL LocationRegisterForReportIntrnl(
    serviceHandle_t *pServiceHandle, 
    ce::PSL_HANDLE hLocation, 
    ce::PSL_HANDLE hNewLocationReport,
    ce::PSL_HANDLE hStateChangeEvent, 
    MY_PSL_COPYIN_GUID pReportType,
    DWORD flags
)
{
    DEBUGMSG(ZONE_API,(L"LOC: LocationRegisterForReport(0x%08x," SVSUTIL_GUID_FORMAT_W L") called\r\n",
                       hLocation,SVSUTIL_PGUID_ELEMENTS(pReportType)));
    DEBUGCHK(g_pLocLock->IsLocked());

    DWORD err;
    locOpenHandle_t *pLocHandle;

    if (flags != 0) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: LocationRegisterForReport set flags = <0x%08x>, currently must be 0\r\n",flags));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    pLocHandle = g_pHandleMgr->FindLocationHandle((HLOCATION)hLocation);
    if (!pLocHandle)
        return FALSE; // caller SetLastError()

    err = g_pPluginMgr->RegisterAppForReport(pReportType, pLocHandle, hNewLocationReport, hStateChangeEvent);

    if (err != ERROR_SUCCESS) {
        SetLastError(err);
        return FALSE;
    }

    DEBUGMSG(ZONE_API,(L"LOC: LocationRegisterForReport succeeded\r\n"));
    return TRUE;
}

BOOL LocationUnRegisterForReportIntrnl(
    serviceHandle_t *pServiceHandle, 
    ce::PSL_HANDLE hLocation, 
    MY_PSL_COPYIN_GUID pReportType, 
    DWORD flags
) 
{
    DEBUGMSG(ZONE_API,(L"LOC: LocationUnRegisterForReport(0x%08x," SVSUTIL_GUID_FORMAT_W L") called\r\n",
                       hLocation,SVSUTIL_PGUID_ELEMENTS(pReportType)));
    DEBUGCHK(g_pLocLock->IsLocked());

    DWORD err;
    locOpenHandle_t *pLocHandle;

    if (flags != 0) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: LocationUnRegisterForReport set flags = <0x%08x>, currently must be 0\r\n",flags));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    pLocHandle = g_pHandleMgr->FindLocationHandle((HLOCATION)hLocation);
    if (!pLocHandle)
        return FALSE; // caller SetLastError()

    err = g_pPluginMgr->UnRegisterAppForReport(pLocHandle,pReportType);

    if (err != ERROR_SUCCESS) {
        SetLastError(err);
        return FALSE;
    }

    DEBUGMSG(ZONE_API,(L"LOC: LocationUnRegisterForReport succeeded\r\n"));
    return TRUE;
}


BOOL LocationGetReportIntrnl(
    serviceHandle_t *pServiceHandle, 
    ce::PSL_HANDLE hLocation, 
    MY_PSL_COPYIN_GUID pReportType, 
    DWORD maximumAge, 
    ce::marshal_arg<ce::copy_out,ce::psl_buffer_wrapper<BYTE*> > pLocationReportBuffer,
    ce::marshal_arg<ce::copy_in_out,DWORD*> pcbLocationReport, 
    DWORD flags
)
{
    DEBUGMSG(ZONE_API,(L"LOC: LocationGetReport(0x%08x," SVSUTIL_GUID_FORMAT_W L", 0x%08x) called\r\n",
                       hLocation,SVSUTIL_PGUID_ELEMENTS(pReportType),maximumAge));
    DEBUGCHK(g_pLocLock->IsLocked());

    LOCATION_REPORT *pLocationReport = (LOCATION_REPORT *)(pLocationReportBuffer.buffer());
    DWORD err;

    reportCol_t *pRC;

    if (flags != 0) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: LocationGetReport set flags = <0x%08x>, currently must be 0\r\n",flags));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (pcbLocationReport == NULL) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: LocationGetReport has pLocationReport and/or reportLen = NULL ptr\r\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    DEBUGCHK(pLocationReportBuffer.count() == *pcbLocationReport);

    if (!g_pHandleMgr->FindLocationHandle((HLOCATION)hLocation))
        return FALSE; // caller SetLastError()

    pRC = g_pPluginMgr->FindRC(pReportType);
    if (! pRC) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: No plugin supports report type <" SVSUTIL_GUID_FORMAT_W L">\r\n",
                                SVSUTIL_PGUID_ELEMENTS(pReportType)));

        SetLastError(ERROR_DEV_NOT_EXIST);
        return FALSE;
    }

    err = pRC->GetReport(maximumAge, pLocationReport, pcbLocationReport);

    if (err != ERROR_SUCCESS) {
        SetLastError(err);
        return FALSE;
    }

    DEBUGMSG(ZONE_API,(L"LOC: LocationGetReport succeeded\r\n"));
    return TRUE;
}

BOOL LocationGetServiceStateIntrnl(
    serviceHandle_t *pServiceHandle, 
    ce::PSL_HANDLE hLocation, 
    ce::marshal_arg<ce::copy_out,LOCATION_SERVICE_STATE*> pServiceState
)
{
    DEBUGMSG(ZONE_API,(L"LOC: LocationGetServiceState(0x%08x)\r\n",hLocation));
    DEBUGCHK(g_pLocLock->IsLocked());

    if (! g_pHandleMgr->FindLocationHandle((HLOCATION)hLocation))
        return FALSE; // Caller SetLastError()

    if (pServiceState == NULL) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: LocationGetServiceState pointer is NULL\r\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    //
    // Fill in the structure
    //
    g_pPluginMgr->GetServiceState(pServiceState);

    DEBUGMSG(ZONE_API,(L"LOC: LocationGetServiceState succeeded\r\n"));
    return TRUE;
}

BOOL LocationGetPluginInfoForReportIntrnl(
    serviceHandle_t *pServiceHandle, 
    ce::PSL_HANDLE hLocation, 
    MY_PSL_COPYIN_GUID pReportType, 
    ce::marshal_arg<ce::copy_out,PLUGIN_STATE *> pPluginState, 
    ce::marshal_arg<ce::copy_out,GUID *> pPluginGuid
)
{
    DEBUGMSG(ZONE_API,(L"LOC: LocationGetPluginInfoForReport(0x%08x," SVSUTIL_GUID_FORMAT_W L") called\r\n",
                       hLocation,SVSUTIL_PGUID_ELEMENTS(pReportType)));
    DEBUGCHK(g_pLocLock->IsLocked());

    if (! g_pHandleMgr->FindLocationHandle((HLOCATION)hLocation))
        return FALSE;

    if (pPluginState == NULL) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: LocationGetReportState pointer is NULL\r\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    *pPluginState = g_pPluginMgr->GetPluginInfoForReport(pReportType,pPluginGuid,FALSE);

    DEBUGMSG(ZONE_API,(L"LOC: LocationGetReportState suceeds.  Report type status is <0x%08x>\r\n",*pPluginState));
    return TRUE;
}

BOOL LocationGetProvidersInfoIntrnl(
    serviceHandle_t *pServiceHandle, 
    ce::PSL_HANDLE hLocation, 
    ce::marshal_arg<ce::copy_out,ce::psl_buffer_wrapper<BYTE*> > pProvidersBuffer,
    ce::marshal_arg<ce::copy_in_out,DWORD*> pcbBuffer
)
{
    DEBUGMSG(ZONE_API,(L"LOC: LocationGetProvidersInfo(0x%08x)\r\n",hLocation));
    DEBUGCHK(g_pLocLock->IsLocked());

    PROVIDER_INFORMATION *pProviders = (PROVIDER_INFORMATION *)pProvidersBuffer.buffer();

    if (! g_pHandleMgr->FindLocationHandle((HLOCATION)hLocation))
        return FALSE;

    if (pcbBuffer==NULL) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: LocationGetProvidersInfo was passed a NULL for pcbBuffer\r\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (((*pcbBuffer)!= 0) && (pProviders==NULL)) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: LocationGetProvidersInfo was passed a NULL for pProviders but non zero buffer len\r\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    DWORD err = g_pPluginMgr->GetProvidersInfo(pProviders, pcbBuffer);

    if (err != ERROR_SUCCESS) {
        SetLastError(err);
        return FALSE;
    }

    DEBUGMSG(ZONE_API,(L"LOC: LocationGetProvidersInfo succeeded\r\n"));
    return TRUE;    
}

BOOL LocationGetResolversInfoIntrnl(
    serviceHandle_t *pServiceHandle, 
    ce::PSL_HANDLE hLocation, 
    ce::marshal_arg<ce::copy_out,ce::psl_buffer_wrapper<BYTE*> > pResolversBuffer,
    ce::marshal_arg<ce::copy_in_out,DWORD*> pcbBuffer
)
{
    DEBUGMSG(ZONE_API,(L"LOC: LocationGetResolversInfo(0x%08x)\r\n",hLocation));
    DEBUGCHK(g_pLocLock->IsLocked());

    RESOLVER_INFORMATION *pResolvers = (RESOLVER_INFORMATION *)pResolversBuffer.buffer();

    if (pcbBuffer==NULL) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: LocationGetResolversInfo was passed a NULL for pcbBuffer\r\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (((*pcbBuffer)!= 0) && (pResolvers==NULL)) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: LocationGetResolversInfo was passed a NULL for pResolvers but non zero buffer len\r\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (! g_pHandleMgr->FindLocationHandle((HLOCATION)hLocation))
        return FALSE; // caller SetLastError()

    DWORD err = g_pPluginMgr->GetResolversInfo(pResolvers, pcbBuffer);

    if (err != ERROR_SUCCESS) {
        SetLastError(err);
        return FALSE;
    }

    DEBUGMSG(ZONE_API,(L"LOC: LocationGetResolversInfo succeeded\r\n"));
    return TRUE;
}

BOOL LocationPluginOpenIntrnl(
    serviceHandle_t *pServiceHandle, 
    HLOCATION hLocation, 
    const GUID *pPluginGuid, 
    HLOCATIONPLUGIN *phOutHandle
) 
{
    DEBUGMSG(ZONE_API,(L"LOC: LocationPluginOpen(0x%08x,0x%08x) called\r\n",hLocation,pPluginGuid));
    DEBUGCHK(g_pLocLock->IsLocked());

    if (pPluginGuid == NULL) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: LocationPluginOpen fails because pPluginGuid=NULL\r\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (phOutHandle == NULL) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: LocationPluginOpen fails because phOutHandle=NULL\r\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    locOpenHandle_t *pLocHandle = g_pHandleMgr->FindLocationHandle(hLocation);
    if (! pLocHandle) 
        return FALSE; // Caller SetLastError()

    plugin_t *pPlugin = g_pPluginMgr->FindPlugin(pPluginGuid);
    if (NULL == pPlugin) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR:  LocationPluginOpen fails because GUID " SVSUTIL_GUID_FORMAT_W 
                             L" is not installed on system\r\n",SVSUTIL_PGUID_ELEMENTS(pPluginGuid)));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    SetLastError(0); // Reset last error code.

    HLOCATIONPLUGIN hPluginReturned = (HLOCATIONPLUGIN)pPlugin->CallIoctlOpen();
    if (hPluginReturned == 0) {
        if (GetLastError() == 0) {
            // Plugins should always set an error code here, but in case they 
            // don't we have to set one here so that apps don't get confused
            DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: PluginIoctlOpen did not set last error.  Setting ERROR_INTERNAL_ERROR for it\r\n"));
            SetLastError(ERROR_INTERNAL_ERROR);
            DEBUGCHK(0);
        }

        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: PluginIoctlOpen call failed.  GLE=<0x%08x>\r\n",GetLastError()));
        return FALSE;
    }

    HLOCATIONPLUGIN hPlugin = g_pHandleMgr->AllocPluginHandle(pLocHandle,pPlugin,hPluginReturned);
    if (hPlugin == 0) { // Caller SetLastError()
        pPlugin->CallIoctlClose(hPluginReturned);
        g_pHandleMgr->FreePluginHandle(hPluginReturned);
        return FALSE;
    }

    DEBUGMSG(ZONE_API,(L"LOC: LocationPluginOpen succeeded, pluginHandle = <0x%08x>\r\n",hPlugin));
    *phOutHandle = hPlugin;
    return TRUE;
}

BOOL LocationPluginOpenPSL(
    serviceHandle_t *pServiceHandle, 
    ce::PSL_HANDLE hLocation, 
    MY_PSL_COPYIN_GUID pPluginGuid, 
    ce::marshal_arg<ce::copy_out,ce::PSL_HANDLE *> phOutHandle
) 
{
    return LocationPluginOpenIntrnl(pServiceHandle, (HLOCATION)hLocation, pPluginGuid, (HLOCATIONPLUGIN*)((ce::PSL_HANDLE *)phOutHandle));
}


BOOL LocationPluginIOCTLIntrnl(
    serviceHandle_t *pServiceHandle, 
    HLOCATION hLocation, 
    HLOCATIONPLUGIN hPlugin, 
    DWORD dwCode,
    BYTE* pbIn,
    DWORD cbIn,
    BYTE* pbOut,
    DWORD* pcbOut
)
{
    DEBUGMSG(ZONE_API,(L"LOC: LocationPluginIOCTL(0x%08x,0x%08x,0x%08x) called\r\n",hLocation,hPlugin,dwCode));
    DEBUGCHK(g_pLocLock->IsLocked());

    if (dwCode < IOCTL_LOCATION_PLUGIN_USER) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: LocationPluginIOCTL passed IOCTL code <0x%08x>, less than <%d>.  This range is reserved for LF direct generation\r\n",
                 dwCode,IOCTL_LOCATION_PLUGIN_USER));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (! g_pHandleMgr->FindLocationHandle(hLocation)) 
        return FALSE; // Caller SetLastError()

    pluginHandle_t *pPluginHandle = g_pHandleMgr->FindPluginHandle(hPlugin);
    if (! pPluginHandle) 
        return FALSE; // Caller SetLastError()

    plugin_t *pPlugin = pPluginHandle->GetPlugin();
    DWORD err = pPlugin->CallIoctlCall(pPluginHandle->GetBaseHandle(),dwCode, pbIn, 
                                       cbIn, pbOut, pcbOut);

    if (err != ERROR_SUCCESS) {
        SetLastError(err);
        return FALSE;
    }

    DEBUGMSG(ZONE_API,(L"LOC: LocationPluginIOCTL succeeded\r\n"));
    return TRUE;
}

BOOL LocationPluginIOCTLPSL(
    serviceHandle_t *pServiceHandle, 
    ce::PSL_HANDLE hLocation, 
    ce::PSL_HANDLE hPlugin, 
    DWORD dwCode,
    ce::marshal_arg<ce::copy_in,ce::psl_buffer_wrapper<BYTE*> > pbIn,
    ce::marshal_arg<ce::copy_out,ce::psl_buffer_wrapper<BYTE*> > pbOut,
    ce::marshal_arg<ce::copy_in_out,DWORD*> pcbOut
)
{
    return LocationPluginIOCTLIntrnl(pServiceHandle, (HLOCATION)hLocation, 
                                    (HLOCATIONPLUGIN)hPlugin, dwCode, 
                                     pbIn.buffer(),pbIn.count(),pbOut.buffer(),pcbOut);
}

BOOL LocationPluginCloseIntrnl(
    serviceHandle_t *pServiceHandle, 
    HLOCATION hLocation, 
    HLOCATIONPLUGIN hPlugin
)
{
    DEBUGMSG(ZONE_API,(L"LOC: LocationPluginClose(0x%08x,0x%08x) called\r\n",hLocation,hPlugin));
    DEBUGCHK(g_pLocLock->IsLocked());

    if (! g_pHandleMgr->FindLocationHandle(hLocation)) 
        return FALSE;  // Caller SetLastError()

    pluginHandle_t *pPluginHandle = g_pHandleMgr->FindPluginHandle(hPlugin);
    if (! pPluginHandle) 
        return FALSE; // Caller SetLastError()

    plugin_t *pPlugin = pPluginHandle->GetPlugin();
    DWORD err = pPlugin->CallIoctlClose(pPluginHandle->GetBaseHandle());

    g_pHandleMgr->FreePluginHandle((HLOCATIONPLUGIN)hPlugin);

    if (err != ERROR_SUCCESS) {
        SetLastError(err);
        return FALSE;
    }

    DEBUGMSG(ZONE_API,(L"LOC: LocationPluginClose succeeded\r\n"));
    return TRUE;
}

BOOL LocationPluginClosePSL(
    serviceHandle_t *pServiceHandle, 
    ce::PSL_HANDLE hLocation, 
    ce::PSL_HANDLE hPlugin
)
{
    return LocationPluginCloseIntrnl(pServiceHandle,(HLOCATION)hLocation,
                                     (HLOCATIONPLUGIN)hPlugin);
}
