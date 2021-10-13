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

// Abstract: Implements routines required to process plugin DLLs

#include <locCore.hpp>



// 
// Callbacks that plugins call
// 

// Process NewProviderReport callback
static DWORD NewProviderReportInd(HANDLE provContext, LOCATION_REPORT *pNewReport) {
    return g_pPluginMgr->NewProviderReportInd(provContext,pNewReport);
}

// Process ProviderUnavailable callback
static DWORD ProviderUnavailableInd(HANDLE provContext) {
    return g_pPluginMgr->ProviderUnavailableInd(provContext);
}

// Process NewResolverReport callback
static DWORD NewResolverReportInd(HANDLE resContext, LOCATION_REPORT *pNewReport) {
    return g_pPluginMgr->NewResolverReportInd(resContext, pNewReport);
}

//
// Helper functions
//

// Helper that reads a REG_MULTI_SZ of GUIDs, parses, and stores in pReportTypes.
static BOOL ReadGuidsFromReg(CReg *pRegPlugin, const WCHAR *pPluginName, const WCHAR *pRegValueName, 
                             GUID *pGuids, DWORD *pNumGuids, DWORD maxToRead) 
{
    const WCHAR *pRegString = pRegPlugin->ValueSZ(pRegValueName);

    DEBUGCHK(*pNumGuids == 0);

    if (NULL == pRegString) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: Plugin <%s> does not have value <%s> set\r\n",pPluginName,pRegValueName));
        return FALSE;
    }

    const WCHAR *pTrav = pRegString;

    while (*pTrav) {
        if (*pNumGuids >= maxToRead) {
            DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: Plugin <%s> has > <%d> guids for <%s>\r\n",pPluginName,maxToRead,pRegValueName));
            return FALSE;
        }

        if (! svsutil_ReadGuidW(pTrav,&pGuids[*pNumGuids],TRUE)) {
            DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: Plugin <%s> has misformatted GUID report type number: <%d>\r\n",pPluginName,*pNumGuids));
            return FALSE;
        }

        *pNumGuids = (*pNumGuids) + 1;
        // For REG_SZ case, we're done after reading our first GUID
        if (maxToRead == 1)
            return TRUE;

        // REG_MULTI_SZ case, move on to next string.
        pTrav += wcslen(pTrav) + 1;
    }

    return ((*pNumGuids) > 0);
}

//
//  Threads spun up to process GetLocation & Stop calls into the plugins.
//

// Thread spun when a provider is requested to generate reports
static DWORD GetProviderLocationThread(LPVOID lpv) {
    provider_t *pProv = (provider_t*)lpv;
    pProv->GetProviderLocation();
    return 0;
}

// Thread spun when a resolver is requested to resolve reports
static DWORD GetResolverLocationThread(LPVOID lpv) {
    resolver_t *pRes = (resolver_t*)((plugin_t*)lpv);
    pRes->GetResolverLocation();
    return 0;
}


//
// plugin_t implementation - basic plugin object.  The provider
// and resolver inherit from this implementation
//

// Create base plugin, reading as much as possible.
plugin_t::plugin_t(CReg *pRegPlugin, const WCHAR *keyName) {
    DEBUGCHK(pRegPlugin->IsOK());

    DWORD regValueType;
    DWORD regValueLen;

    m_numActiveIoctlCallers = 0;
    m_workerThrdCookie      = 0;
    m_loadSuccess           = FALSE;
    m_pluginState           = PLUGIN_STATE_ERROR;
    m_immediateRestart      = FALSE;

    memset(m_receivedReport,0,sizeof(m_receivedReport));

    m_dllName = pRegPlugin->ValueSZ(g_rvDll);
    if (m_dllName.empty()) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: Plugin <%s> does not have DLL set\r\n",keyName));
        return;
    }

    DWORD numGuids = 0; // dummy variable...
    if (! ReadGuidsFromReg(pRegPlugin,keyName,g_rvGuid,&m_guid,&numGuids,1))
        return;

    if (! pRegPlugin->GetLengthAndType(g_rvPreference,&regValueLen,&regValueType)) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: Plugin <%s> does not have Preference set\r\n",keyName));
        return;
    }
    m_preference = pRegPlugin->ValueDW(g_rvPreference);

    
    m_pluginFlags = pRegPlugin->ValueDW(g_rvPluginFlags);
    if (PLUGIN_FLAGS_DISABLED & m_pluginFlags) {
        DEBUGMSG(ZONE_PLUGIN,(L"LOC: ERROR: Plugin <%s> has been disabled via PLUGIN_FLAGS_DISABLED setting\r\n",keyName));
        return;
    }


    if (! pRegPlugin->GetLengthAndType(g_rvRetryOnFailure,&regValueLen,&regValueType)) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: Plugin <%s> does not have RetryOnFailure set.\r\n",keyName));
        return;
    }
    m_retryOnFailure = pRegPlugin->ValueDW(g_rvRetryOnFailure);

			
    if (! pRegPlugin->GetLengthAndType(g_rvMaximumInitialWait,&regValueLen,&regValueType)) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: Plugin <%s> does not have MaximumInitialWait set.\r\n",keyName));
        return;
    }
    m_initialWaitTime = pRegPlugin->ValueDW(g_rvMaximumInitialWait);


    m_numGeneratedReportTypes = 0;
    if (! ReadGuidsFromReg(pRegPlugin,keyName,g_rvGeneratedReports,m_generatedReportTypes,
                           &m_numGeneratedReportTypes,MAX_PLUGIN_REPORT_TYPES)) {
        return;
    }

    m_friendlyName = pRegPlugin->ValueSZ(g_rvFriendlyName);    
    if (m_friendlyName.size() >= MAX_PLUGIN_FRIENDLY_NAME) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: Plugin <%s> has friendly name > <%d> characters\r\n",keyName,MAX_PLUGIN_FRIENDLY_NAME));
        // This is not a fatal error.  However we do need to clear this, since
        // later we assume the length is < MAX_PLUGIN_FRIENDLY_NAME chars.
        m_friendlyName = NULL;
    }

    m_version = pRegPlugin->ValueDW(g_rvVersion,1);
    m_pluginState = PLUGIN_STATE_OFF; // Indicate no errors so far.
    memset(&m_lastUpdate,0,sizeof(m_lastUpdate));
}

// Helper to load the plugin DLL itself
BOOL plugin_t::LoadPlugin(void) {
    DEBUGMSG(ZONE_PLUGIN,(L"LOC: Loading plugin for DLL <%s>\r\n",GetName()));

    m_dllHInstance = LoadLibrary(m_dllName);
    if (!m_dllHInstance.valid()) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: LoadLibrary on plugin DLL <%s> failed.  GLE=0x%08x\r\n",GetName(),GetLastError()));
        return FALSE;
    }

    return TRUE;
}

// Returns index int m_generatedReportTypes of given report, -1 if not supported
DWORD plugin_t::FindGeneratedReportIndex(const GUID *pReportType) {
    for (DWORD i = 0; i < m_numGeneratedReportTypes; i++) {
        if (0 == memcmp(&m_generatedReportTypes[i],pReportType,sizeof(GUID)))
            return i;
    }
    return (DWORD)-1;
}

// Returns TRUE if the plugin can generate (in theory, even if it plugin
// is not available at the moment) a report.
BOOL plugin_t::CanGenerateReportType(const GUID *pReportType) {
    return (FindGeneratedReportIndex(pReportType) != (DWORD)-1);
}

// When a plugin has a new report arrived, mark its type as such as such.
void plugin_t::IndicateReceivedReport(const GUID *pReportType) {
    DWORD index = FindGeneratedReportIndex(pReportType);
    DEBUGCHK(index < MAX_PLUGIN_REPORT_TYPES);
    m_receivedReport[index] = TRUE;
    GetCurrentFT(&m_lastUpdate);
}

// Determines whether a plugin has received a callback indicating
// report of this type.
BOOL plugin_t::HasReceivedReport(const GUID *pReportType) {
    DWORD index = FindGeneratedReportIndex(pReportType);
    DEBUGCHK(index < MAX_PLUGIN_REPORT_TYPES);
    return m_receivedReport[index];
}

// Fills out PLUGIN_INFORMATION structure for this plugin 
void plugin_t::GetPluginInfo(PLUGIN_INFORMATION *pPluginInfo) {
    pPluginInfo->version             = m_version;
    memcpy(&pPluginInfo->guid,&m_guid,sizeof(m_guid));
    wcscpy(pPluginInfo->friendlyName,m_friendlyName);
    pPluginInfo->pluginState         = m_pluginState;
    pPluginInfo->retryInterval       = m_retryOnFailure;
	pPluginInfo->maximumInitialWait = m_initialWaitTime;
    memcpy(&pPluginInfo->lastUpdate,&m_lastUpdate,sizeof(FILETIME));
    pPluginInfo->numReportsGenerated = m_numGeneratedReportTypes;
    memcpy(&pPluginInfo->reportsGenerated,m_generatedReportTypes,
           sizeof(GUID)*m_numGeneratedReportTypes);
    pPluginInfo->pluginFlags         = m_pluginFlags;
}

// Returns which reportTypes the plugin can possibly generate
void plugin_t::GetGeneratedReportInfo(GUID *pReportTypes, DWORD *numReportTypes) {
    if ((m_numGeneratedReportTypes == 0) || (m_numGeneratedReportTypes > MAX_PLUGIN_REPORT_TYPES)) {
        // pReportTypes is guaranteed to be MAX_PLUGIN_REPORT_TYPES*sizeof(GUID)
        // We do run-time check, rather only having DEBUGCHK, to satisfy prefix.
        DEBUGCHK(0);
        *numReportTypes = 0;
        return;
    }

    *numReportTypes = m_numGeneratedReportTypes;
    memcpy(pReportTypes,m_generatedReportTypes,m_numGeneratedReportTypes*sizeof(GUID));
}

// Alerts the plugin manager that a fatal error has occured during
// a callback.  The plugin manager processes this and in turn calls 
// SetFatalError back on the callback
void plugin_t::ReportFatalError() {
    // For locking, most of the time this function is called in the exception
    // handler for one of the callbacks where lock will not be held.  There
    // are some cases where g_pLocLock already is held by caller, but extra
    // lock is OK because FatalPluginError never releases the lock.
    g_pLocLock->Lock();
    g_pPluginMgr->FatalPluginError(this);
    g_pLocLock->Unlock();
}

// Called when a plugin is in the process of shutting down when a new
// application has registered for it and hence it needs to restart right away.
void plugin_t::SetImmediateRestart(void) {
    m_immediateRestart = TRUE;
}


// Blocks until any PSL calls into the plugin IOCTL processor have returned.
void plugin_t::WaitOnOpenIoctlCalls(void) {
    DEBUGCHK(g_pLocLock->IsLocked());

#ifdef DEBUG
    // Number of times we've polled while waiting to shutdown.
    DWORD numRetries = 0;
#endif

    while (m_numActiveIoctlCallers) {
#ifdef DEBUG
        numRetries++;
        // Taking this many retries indicates plugin IOCTL worker(s) blocked too long.
        // This is not fatal but merits investigation.
        DEBUGCHK(g_pollRetriesWarnOnUnload != numRetries);
#endif

        DEBUGMSG(ZONE_PLUGIN | ZONE_THREAD,(L"LOC: Waiting <%d> ms for plugin <%s> IOCTL calls to complete\r\n",
                                            g_pollOnUnload,GetName()));
        g_pLocLock->Unlock();
        Sleep(g_pollOnUnload);
        g_pLocLock->Lock();
    }
}

// Block until the plugin's ProviderGetLocation/ResolverGetLocation
// thread comes to a halt
void plugin_t::WaitOnRunningGetLocThread(void) {
    DEBUGCHK(g_pLocLock->IsLocked());
#ifdef DEBUG
    // Number of times we've polled while waiting to shutdown.
    DWORD numRetries = 0;
#endif

    while (m_workerThrdCookie) {
#ifdef DEBUG
        numRetries++;
        // Taking this many retries indicate plugin stop thread is blocked for too long.
        // This is not fatal but merits investigation.
        DEBUGCHK(g_pollRetriesWarnOnUnload != numRetries);
#endif

        DEBUGMSG(ZONE_PLUGIN | ZONE_THREAD,(L"LOC: Waiting <%d> ms for plugin <%s> GetLocationReport thread to complete\r\n",
                                            g_pollOnUnload,GetName()));
        g_pLocLock->Unlock();
        Sleep(g_pollOnUnload);
        g_pLocLock->Lock();
    }

}

#ifdef DEBUG
// Print GUIDs in a list to DEBUGOUT
void DbgPrintGuids(const GUID *pGuids, DWORD numGuids) {
    for (DWORD i = 0; i < numGuids; i++) {
        DEBUGMSG(1,(L"LOC:     GUID = <" SVSUTIL_GUID_FORMAT_W L">\r\n",
                  SVSUTIL_RGUID_ELEMENTS(pGuids[i])));
    }
}

// Prints PLUGIN_INFORMATION to DEBUGOUT
void plugin_t::DbgPrintSettings(void) {
    DEBUGMSG(1,(L"LOC:   DllName = <%s>, GUID=<" SVSUTIL_GUID_FORMAT_W L">\r\n",
                GetName(), SVSUTIL_RGUID_ELEMENTS(m_guid)));
    DEBUGMSG(1,(L"LOC:   Preference = <%d>, PluginFlags = <0x%08x>, Retry On Failure = <%d>\r\n",
                m_preference,m_pluginFlags,m_retryOnFailure));
    DEBUGMSG(1,(L"LOC:   Generated GUIDs: \r\n"));
    DbgPrintGuids(m_generatedReportTypes,m_numGeneratedReportTypes);
}
#endif


// Delete resources associated with plugin.  If plugin is on,
// then take care of alerting it to the new state. 
// This BLOCKS until the plugin is ready to be unloaded - i.e. all
// open threads are finished running.

provider_t::~provider_t() {
    if (! m_loadSuccess) {
        DEBUGCHK(g_serviceState == SERVICE_STATE_STARTING_UP);
        // If loading the plugin failed, then we shouldn't call into
        // Stop/Uninitialize at all.
        return;
    }
    DEBUGMSG(ZONE_INIT | ZONE_PROVIDER,(L"LOC: Beginning to uninit Provider DLL <%s>\r\n",GetName()));

    if (m_pluginState >= PLUGIN_STATE_UNAVAILABLE) {
        // The plugin's stop still needs to be called.  Do here, rather
        // than spawning up a thread.
        SetState(PLUGIN_STATE_SHUTTING_DOWN);
        CallProvStop();
    }

    // Wait until any outstanding references to the provider have stopped
    WaitOnOpenIoctlCalls();
    WaitOnRunningGetLocThread();

    // Finally, call the uninit.
    CallProvUnInit();

    DEBUGMSG(ZONE_INIT | ZONE_PROVIDER,(L"LOC: Completed uninit Provider DLL <%s>\r\n",GetName()));
}

//
// provider_t implements provider wrapper
//

// Reads provider registry settings, loads DLLs, and call ProviderInitialize
void provider_t::InitProvider(CReg *pRegPlugin, const WCHAR *keyName) {
    DEBUGMSG(ZONE_PROVIDER,(L"LOC: Initializing provider <%s>\r\n",keyName));
    DEBUGCHK(m_pluginState == PLUGIN_STATE_ERROR); // Should've been reset before calling here

    DWORD regValueType;
    DWORD regValueLen;


    // Read registry settings
    if (! pRegPlugin->GetLengthAndType(g_rvPollInterval, &regValueLen, &regValueType)) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: Provider <%s> does not have Poll Interval set.\r\n",keyName));
        return;
    }
    m_pollInterval = pRegPlugin->ValueDW(g_rvPollInterval);

    m_providerFlags = pRegPlugin->ValueDW(g_rvProviderFlags);

    // Load dll and get function exports
    if (! LoadPlugin())
        return;

    m_pfnInitialize   = (PFN_PROVINITIALIZE)GetProcAddress(m_dllHInstance,L"ProviderInitialize");
    m_pfnGetLocation  = (PFN_PROVGETLOCATION)GetProcAddress(m_dllHInstance,L"ProviderGetLocation");
    m_pfnStop         = (PFN_PROVSTOP)GetProcAddress(m_dllHInstance,L"ProviderStop");
    m_pfnUnInitialize = (PFN_PROVUNINITIALIZE)GetProcAddress(m_dllHInstance,L"ProviderUnInitialize");
    m_pfnIoctlOpen    = (PFN_PROVOPEN)GetProcAddress(m_dllHInstance,L"ProviderIoctlOpen");
    m_pfnIoctlCall    = (PFN_PROVIOCTL)GetProcAddress(m_dllHInstance,L"ProviderIoctlCall");
    m_pfnIoctlClose   = (PFN_PROVCLOSE)GetProcAddress(m_dllHInstance,L"ProviderIoctlClose");

    if (! (m_pfnInitialize && m_pfnGetLocation && m_pfnStop && 
           m_pfnUnInitialize && m_pfnIoctlOpen && m_pfnIoctlCall && m_pfnIoctlClose))
    {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: Provider <%s> does not export all required functions\r\n",keyName));
        return;
    }

    PROVIDER_INFORMATION provInfo;
    GetProviderInfo(&provInfo);
    DWORD err;

    provInfo.pluginInfo.pluginState = PLUGIN_STATE_OFF;

    if (ERROR_SUCCESS != (err = CallProvInit(&provInfo))) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: ProviderInitialize for <%s> returned <0x%08x>.  Will not use this provider\r\n",
                              GetName(),err));
        return;
    }

    // Plugin successfully initialized.
    m_pluginState    = PLUGIN_STATE_OFF;
    m_loadSuccess    = TRUE;

#ifdef DEBUG
    if (ZONE_PROVIDER) {
        DEBUGMSG(1,(L"LOC: Loaded Provider from reg key <%s>.  Settings:\r\n",keyName));
        DbgPrintSettings();
        DEBUGMSG(1,(L"LOC:   Poll Interval = <0x%08x>, ProviderFlags = <0x%08x>\r\n",m_pollInterval,m_providerFlags));
    }
#endif
    
}

// Fills out PROVIDER_INFORMATION structure for this provider
void provider_t::GetProviderInfo(PROVIDER_INFORMATION *pProvInfo) {
    GetPluginInfo(&pProvInfo->pluginInfo);
    pProvInfo->pollInterval  = m_pollInterval;
    pProvInfo->providerFlags = m_providerFlags;
}

// The provider is entering a new state
void provider_t::SetState(PLUGIN_STATE newState) {
    if (m_pluginState == PLUGIN_STATE_ERROR) {
        // Once plugin has entered an error condition, it can never get out of it.
        return;
    }

#ifdef DEBUG
    if (m_pluginState != newState) {
        DEBUGMSG(ZONE_PROVIDER && ZONE_VERBOSE,(L"LOC: Changing provider <%s> from state <%d> to state <%d>\r\n",
                 GetName(),m_pluginState,newState));
    }
#endif

    // For a provider, all report types are set to be in the same state
    // and hence there is no need to track state per report (unlike resolvers)
    m_pluginState = newState;
}

void provider_t::ReportsUnavailable(void) {
    SetState(PLUGIN_STATE_UNAVAILABLE);
    // Go back to initial state as to not having received any reports
    memset(m_receivedReport,0,sizeof(m_receivedReport));
}

// Called when provider has received a report and is now on.
void provider_t::ReportsAvailable(void) {
    SetState(PLUGIN_STATE_ON);
}

// Called when provider is starting up
void provider_t::StartingUp(void) {
    SetState(PLUGIN_STATE_STARTING_UP);
}

// Indicates that a fatal error has occured 
void provider_t::SetFatalError() {
    SetState(PLUGIN_STATE_ERROR);
}

// Indicates that the plugin should generate the requested report type
// Do nothing on providers since once a provider is on it will try to 
// generate all report types
void provider_t::RequestReportGeneration(const GUID *pReportType) {
    DEBUGCHK(CanGenerateReportType(pReportType));
    DEBUGCHK(m_pluginState != PLUGIN_STATE_ERROR);
}

// Like RequestReportGeneration, do nothing for provider case
void provider_t::CancelReportGeneration(const GUID *pReportType) {
    DEBUGCHK(CanGenerateReportType(pReportType));
}



// A provider can either generate all of its m_generatedReports or 
// none of them.  For this reason, just return "global" plugin state 
// for this provider.  Implement this function in the first place to make
// inheritance easier, since resolvers may have a different per reportType.
PLUGIN_STATE provider_t::GetReportTypeState(const GUID *pReportType) { 
    return m_pluginState; 
}

// Fills PROVIDER_CONTROL_BLOCK that will be sent to the provider
void provider_t::FillPCB(PROVIDER_CONTROL_BLOCK *pPcb, PROVIDER_INFORMATION *pProvInfo) {
    GetProviderInfo(pProvInfo);

    pPcb->provContext         = (HANDLE)this;
    pPcb->version             = LOCATION_FRAMEWORK_VERSION_CURRENT;
    pPcb->pProvInfo           = pProvInfo;
    pPcb->NewProviderReport   = NewProviderReportInd;
    pPcb->ProviderUnavailable = ProviderUnavailableInd;

}

// Thread spun up to call the provider's ProviderGetLocation export
void provider_t::GetProviderLocation(void) {
    DEBUGMSG(ZONE_PROVIDER| ZONE_THREAD,(L"LOC: Started Provider ProviderGetLocation thread for prov=<%s>\r\n",GetName()));

    DEBUGCHK(! g_pLocLock->IsLocked());
    g_pLocLock->Lock();

    PROVIDER_CONTROL_BLOCK pcb;
    PROVIDER_INFORMATION   provInfo;

    // Plugin better not be on at this stage.
    DEBUGCHK(m_pluginState != PLUGIN_STATE_ON);

    if (m_pluginState != PLUGIN_STATE_STARTING_UP) {
        DEBUGMSG(ZONE_PROVIDER,(L"LOC: Will not start provider <%s> because its state was changed before it could be started\r\n",GetName()));
        goto done;
    }

    FillPCB(&pcb,&provInfo);    

    while (1) {
        DEBUGCHK(g_pLocLock->IsLocked());
        DEBUGCHK(m_pluginState == PLUGIN_STATE_STARTING_UP);

        // Call into the plugin export.  Plugin will enter the PLUGIN_STATE_ON
        // state the first time it returns a location report.
        // This call will block for a very long time - either until
        // the ProviderStop() is called or the provider exits abnormally.
        if (ERROR_SUCCESS != CallProvGetLocation(&pcb)) {
            // Indicates a fatal error.  This plugin won't be used anymore.
            ReportFatalError();
            break;
        }

        if (m_pluginState != PLUGIN_STATE_SHUTTING_DOWN) {
            DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: Plugin <%s> returned but was not called from ProviderStop() first.  Provider will be disabled\r\n",
                                  GetName()));
            ReportFatalError();
            break;
        }

        if (m_immediateRestart) {
            // Hit this case when we were in the plugin shutdown process, but
            // before we could stop another app requested plugin to start again.
            DEBUGMSG(ZONE_PROVIDER,(L"LOC: Was going to shutdown plugin <%s>, but another app requested it so restarting\r\n",GetName()));
            m_immediateRestart = FALSE;
            SetState(PLUGIN_STATE_STARTING_UP);
            continue;
        }
        break;
    }

    SetState(PLUGIN_STATE_OFF);

done:    
    m_workerThrdCookie = 0;
    g_pLocLock->Unlock();
    DEBUGMSG(ZONE_PROVIDER | ZONE_THREAD,(L"LOC: Ending Provider ProviderGetLocation thread for prov=<%s>\r\n",GetName()));
}


// Schedules a worker to generate provider reports if one is not already running
DWORD provider_t::ScheduleWorkerIfNeeded(void) {
    DEBUGCHK(g_pLocLock->IsLocked());

    if (m_workerThrdCookie != 0) // worker already running
        return ERROR_SUCCESS;

    m_workerThrdCookie = g_pThreadPool->ScheduleEvent(::GetProviderLocationThread,this);
    
    if (! m_workerThrdCookie) {
        DEBUGMSG_OOM();
        return ERROR_OUTOFMEMORY;
    }

    DEBUGMSG(ZONE_PROVIDER | ZONE_THREAD,(L"LOC: Spinning GetProviderLocation thread for provider <%s>, cookie=<0x%08x>\r\n",
             GetName(),m_workerThrdCookie));
    return ERROR_SUCCESS;
}

void provider_t::StopPlugin(void) {
    DEBUGCHK(g_pLocLock->IsLocked());
    DEBUGCHK(m_workerThrdCookie != 0);

    // Go back to initial state as to not having received any reports
    memset(m_receivedReport,0,sizeof(m_receivedReport));
    // The provider Stop() call will return immediately.  We transfer
    // to the off state once the provider returns.
    SetState(PLUGIN_STATE_SHUTTING_DOWN);

    CallProvStop();
}

// Calls ProviderInitialize export
DWORD provider_t::CallProvInit(PROVIDER_INFORMATION *pProvInfo) {
    DEBUGCHK(g_pLocLock->IsLocked());

    DWORD err;
    __try {
        DEBUGMSG(ZONE_PROVIDER,(L"LOC: Calling ProviderInitialize on DLL=<%s>\r\n",GetName()));
        // Initialize is only called on LF startup.  OK to hold lock.
        err = m_pfnInitialize(pProvInfo);
        DEBUGMSG(ZONE_PROVIDER,(L"LOC: ProviderInitialize on DLL=<%s> returned <0x%08x>\r\n",GetName(),err));
    } __except(REPORT_EXCEPTION_TO_WATSON()) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: ProviderInitialize caused an exception in DLL=<%s>\r\n",GetName()));
        // ReportFatalError(); // Do not call this - plugin constructor will immediately unload it on any error on init.
        err = ERROR_INTERNAL_ERROR;
    }

    return err;
}

// Calls ProviderGetLocation export
DWORD provider_t::CallProvGetLocation(PROVIDER_CONTROL_BLOCK *pProvControlBlock) {
    // This call could block a very long time so release lock.
    g_pLocLock->Unlock();

    DWORD err;
    __try {
        DEBUGMSG(ZONE_PROVIDER,(L"LOC: Calling ProviderGetLocation on DLL=<%s>\r\n",GetName()));
        err = m_pfnGetLocation(pProvControlBlock);
        DEBUGMSG(ZONE_PROVIDER,(L"LOC: ProviderGetLocation on DLL=<%s> returned <0x%08x>\r\n",GetName(),err));
    } __except(REPORT_EXCEPTION_TO_WATSON()) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: ProviderGetLocation caused an exception.  DLL=<%s>\r\n",GetName()));
        ReportFatalError();
        err = ERROR_INTERNAL_ERROR;
    }

    g_pLocLock->Lock();
    return err;
}

// Calls ProviderStop export.  Note that we do NOT release the lock
// when calling into this function, so it must not block
// Generally not a good idea to call 3rd party code with locks held,
// but we can get away with it here because not that many pluggins are 
// ever going to be written and they will be done by advanced programmers.  
// This no-block requirement is clearly specced.

// Holding the lock enormously simplifies implementation of LF, in particular
// the case where apps try to register for a plugin as it enters shutdown state.

DWORD provider_t::CallProvStop(void) {
    DEBUGCHK(g_pLocLock->IsLocked());

    DWORD err;
    __try {
        DEBUGMSG(ZONE_PROVIDER,(L"LOC: Calling ProviderStop on DLL=<%s>\r\n",GetName()));
        err = m_pfnStop();
        DEBUGMSG(ZONE_PROVIDER,(L"LOC: ProviderStop on DLL=<%s> returned <0x%08x>\r\n",GetName(),err));
    } __except(REPORT_EXCEPTION_TO_WATSON()) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: ProviderStop caused an exception.  DLL=<%s>\r\n",GetName()));
        ReportFatalError();
        err = ERROR_INTERNAL_ERROR;
    }

    return err;
}

// Calls ProviderUnInitialize export
DWORD provider_t::CallProvUnInit(void) {
    DEBUGCHK(g_pLocLock->IsLocked());

    DWORD err;
    __try {
        DEBUGMSG(ZONE_PROVIDER,(L"LOC: Calling ProviderUninitialize on DLL=<%s>\r\n",GetName()));
        // Uninitialize is only called on LF shutdown.  OK to hold lock.
        err = m_pfnUnInitialize();
        DEBUGMSG(ZONE_PROVIDER,(L"LOC: ProviderUninitialize on DLL=<%s> returned <0x%08x>\r\n",GetName(),err));
    } __except(REPORT_EXCEPTION_TO_WATSON()) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: ProviderUninitialize caused an exception.  DLL=<%s>\r\n",GetName()));
        // ReportFatalError(); - Don't bother at this stage since plugin is going to be unloaded anyway.
        err = ERROR_INTERNAL_ERROR;
    }

    return err;
}

// Calls ProviderIoctlOpen export
DWORD provider_t::CallProvIoctlOpen(void) {
    m_numActiveIoctlCallers++;
    g_pLocLock->Unlock();

    DWORD err;
    __try {
        DEBUGMSG(ZONE_PROVIDER,(L"LOC: Calling ProviderIoctlOpen on DLL=<%s>\r\n",GetName()));
        err = m_pfnIoctlOpen();
        DEBUGMSG(ZONE_PROVIDER,(L"LOC: ProviderIoctlOpen on DLL=<%s> returned <0x%08x>\r\n",GetName(),err));
    } __except(REPORT_EXCEPTION_TO_WATSON()) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: ProviderIoctlOpen caused an exception.  DLL=<%s>\r\n",GetName()));
        ReportFatalError();
        err = ERROR_INTERNAL_ERROR;
    }

    g_pLocLock->Lock();
    m_numActiveIoctlCallers--;
    return err;
}

// Calls ProviderIoctlCall export
DWORD provider_t::CallProvIoctlCall(HANDLE h, DWORD dwCode, BYTE *pBufIn, DWORD cbIn, BYTE *pBufOut, DWORD *pcbOut) {
    m_numActiveIoctlCallers++;
    g_pLocLock->Unlock();

    DWORD err;
    __try {
        DEBUGMSG(ZONE_PROVIDER,(L"LOC: Calling ProviderIoctlCall on DLL=<%s>\r\n",GetName()));
        err = m_pfnIoctlCall(h, dwCode, pBufIn, cbIn, pBufOut, pcbOut);
        DEBUGMSG(ZONE_PROVIDER,(L"LOC: ProviderIoctlCall on DLL=<%s> returned <0x%08x>\r\n",GetName(),err));
    } __except(REPORT_EXCEPTION_TO_WATSON()) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: ProviderIoctlCall caused an exception.  DLL=<%s>\r\n",GetName()));
        ReportFatalError();
        err = ERROR_INTERNAL_ERROR;
    }

    g_pLocLock->Lock();
    m_numActiveIoctlCallers--;
    return err;
}

// Calls ProviderIoctlClose export
DWORD provider_t::CallProvIoctlClose(HANDLE h) {
    m_numActiveIoctlCallers++;
    g_pLocLock->Unlock();

    DWORD err;
    __try {
        DEBUGMSG(ZONE_PROVIDER,(L"LOC: Calling ProviderClose on DLL=<%s>\r\n",GetName()));
        err = m_pfnIoctlClose(h);
        DEBUGMSG(ZONE_PROVIDER,(L"LOC: ProviderClose on DLL=<%s> returned <0x%08x>\r\n",GetName(),err));
    } __except(REPORT_EXCEPTION_TO_WATSON()) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: ProviderClose caused an exception.  DLL=<%s>\r\n",GetName()));
        ReportFatalError();
        err = ERROR_INTERNAL_ERROR;
    }

    g_pLocLock->Lock();
    m_numActiveIoctlCallers--;
    return err;
}


//
// resolver_t implements resolver wrapper
//

resolver_t::~resolver_t() {
    if (! m_loadSuccess) {
        DEBUGCHK(g_serviceState == SERVICE_STATE_STARTING_UP);
        // If loading the plugin failed, then we shouldn't call into
        // Stop/Uninitialize at all.
        return;
    }
    DEBUGMSG(ZONE_INIT | ZONE_RESOLVER,(L"LOC: Beginning to uninit Resolver DLL <%s>\r\n",GetName()));

    if (m_pluginState >= PLUGIN_STATE_UNAVAILABLE) {
        // The plugin's stop still needs to be called.
        if (m_getRepThrdCalledFirstTime) {
            m_pluginState = PLUGIN_STATE_SHUTTING_DOWN;
            CallResStop();
        }
    }

    // Wait until any outstanding references to the resolver have stopped
    WaitOnOpenIoctlCalls();
    WaitOnRunningGetLocThread();

    // Finally, call the uninit.
    CallResUnInit();

    DEBUGMSG(ZONE_INIT | ZONE_RESOLVER,(L"LOC: Completed uninit Resolver DLL <%s>\r\n",GetName()));
}

// Called on initialization of resolver - reads in base settings & calls 
// ResolverInitialize
void resolver_t::InitResolver(CReg *pRegPlugin, const WCHAR *keyName) {
    DEBUGMSG(ZONE_RESOLVER,(L"LOC: Initializing resolver <%s>\r\n",keyName));
    DEBUGCHK(m_pluginState == PLUGIN_STATE_ERROR); // Should've been reset before calling here

    DWORD regValueType;
    DWORD regValueLen;

    m_lastResolution = 0; 
    m_getRepThrdCalledFirstTime = FALSE;

    SetAllReportStates(PLUGIN_STATE_OFF); 
    m_pluginState = PLUGIN_STATE_ERROR;

    // Only indicate no providers when we actually have providers indicating failure.
    // By default assume providers can in theory work, only set this FALSE when
    // all providers we would care about actually fail on us.
    m_providersAvailable = TRUE;

    m_providerGenerator = NULL;
    m_reportCollector   = NULL;

    // Fill out remainder of array with report types not generated
    for (DWORD i = m_numGeneratedReportTypes; i < MAX_PLUGIN_REPORT_TYPES; i++)
        m_reportStates[i] = PLUGIN_STATE_NOT_SUPPORTED;

    // Read registry settings
    if (! pRegPlugin->GetLengthAndType(g_rvMinimumRequery, &regValueLen, &regValueType)) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: Resolver <%s> does not have MinimumRequery set.\r\n",keyName));
        return;
    }
    m_minRequery = pRegPlugin->ValueDW(g_rvMinimumRequery);

    m_resolverFlags = pRegPlugin->ValueDW(g_rvResolverFlags);

    m_numSupportedReportTypes = 0;
    if (! ReadGuidsFromReg(pRegPlugin,keyName,g_rvSupportedReports,m_supportedReportTypes,
                           &m_numSupportedReportTypes,MAX_PLUGIN_REPORT_TYPES)) {
        return;
    }

    // Load dll and get function exports
    if (! LoadPlugin())
        return;

    m_pfnInitialize   = (PFN_RESINITIALIZE)GetProcAddress(m_dllHInstance,L"ResolverInitialize");
    m_pfnGetLocation  = (PFN_RESGETLOCATION)GetProcAddress(m_dllHInstance,L"ResolverGetLocation");
    m_pfnStop         = (PFN_RESSTOP)GetProcAddress(m_dllHInstance,L"ResolverStop");
    m_pfnUnInitialize = (PFN_RESUNINITIALIZE)GetProcAddress(m_dllHInstance,L"ResolverUnInitialize");
    m_pfnIoctlOpen    = (PFN_RESOPEN)GetProcAddress(m_dllHInstance,L"ResolverIoctlOpen");
    m_pfnIoctlCall    = (PFN_RESIOCTL)GetProcAddress(m_dllHInstance,L"ResolverIoctlCall");
    m_pfnIoctlClose   = (PFN_RESCLOSE)GetProcAddress(m_dllHInstance,L"ResolverIoctlClose");

    if (! (m_pfnInitialize && m_pfnGetLocation && m_pfnStop && 
           m_pfnUnInitialize && m_pfnIoctlOpen && m_pfnIoctlCall && m_pfnIoctlClose))
    {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: Provider <%s> does not export all required functions\r\n",keyName));
        return;
    }

    RESOLVER_INFORMATION resInfo;
    GetResolverInfo(&resInfo);
    DWORD err;

    resInfo.pluginInfo.pluginState = PLUGIN_STATE_OFF;

    if (ERROR_SUCCESS != (err = CallResInit(&resInfo))) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: ResolverInitialize for <%s> returned <0x%08x>.  Will not use this provider\r\n",
                              GetName(),err));
        return;
    }

    m_pluginState = PLUGIN_STATE_OFF;
    m_loadSuccess = TRUE;

#ifdef DEBUG
    if (ZONE_RESOLVER) {
        DEBUGMSG(1,(L"LOC: Loaded Resolver from reg key <%s>.  Settings:\r\n",keyName));
        DbgPrintSettings();
        DEBUGMSG(1,(L"LOC:   MinRequery = <0x%08x>, ResolverFlags = <0x%08x>\r\n",m_minRequery,m_resolverFlags));
        DEBUGMSG(1,(L"LOC:   Supported Guids\r\n"));
        DbgPrintGuids(m_supportedReportTypes,m_numSupportedReportTypes);
    }
#endif

    return;
}

// Fills out RESOLVER_INFORMATION structure for this resolver
void resolver_t::GetResolverInfo(RESOLVER_INFORMATION *pResInfo) {
    GetPluginInfo(&pResInfo->pluginInfo);
    pResInfo->minRequeryInterval  = m_minRequery;
    pResInfo->numReportsSupported = m_numSupportedReportTypes;
    memcpy(&pResInfo->reportsSupported,m_supportedReportTypes,sizeof(m_supportedReportTypes));
    pResInfo->resolverFlags       = m_resolverFlags;
}

// Returns which reportTypes the plugin can possibly generate
void resolver_t::GetSupportedReportInfo(GUID *pReportTypes, DWORD *numReportTypes) {
    if ((m_numSupportedReportTypes == 0) || (m_numSupportedReportTypes > MAX_PLUGIN_REPORT_TYPES)) {
        // pReportTypes is guaranteed to be MAX_PLUGIN_REPORT_TYPES*sizeof(GUID)
        // We do run-time check, rather only having DEBUGCHK, to satisfy prefix.
        DEBUGCHK(0);
        *numReportTypes = 0;
        return;
    }

    *numReportTypes = m_numSupportedReportTypes;
    memcpy(pReportTypes,m_supportedReportTypes,m_numSupportedReportTypes*sizeof(GUID));
}

// Returns TRUE if the resolver can (in theory) support reports of a particular
// report type for resolution.
BOOL resolver_t::CanSupportReportType(const GUID *pReportType) {
    for (DWORD i = 0; i < m_numSupportedReportTypes; i++) {
        if (0 == memcmp(&m_supportedReportTypes[i],pReportType,sizeof(GUID)))
            return TRUE;
    }
    return FALSE;
}

// For resolvers, since each report type may or may not be available independent
// of the other report types *for the same resolver DLL*, need a little more
// complicated logic here.  (Note that provider implementation of this function
// just returns the global state for that provider)
PLUGIN_STATE resolver_t::GetReportTypeState(const GUID *pReportType) {
    for (DWORD i = 0; i < m_numGeneratedReportTypes; i++) {
        if (0 == memcmp(pReportType,&m_generatedReportTypes[i],sizeof(GUID)))
            return m_reportStates[i];
    }

    // Indicates plugin does not support this type.
    return PLUGIN_STATE_NOT_SUPPORTED;
}

// Indicates particular report type isn't available anymore
void resolver_t::ReportTypeUnavailable(const GUID *pReportType) {
    SetState(pReportType,PLUGIN_STATE_UNAVAILABLE);
}

void resolver_t::ReportTypeAvailable(const GUID *pReportType) {
    SetState(pReportType,PLUGIN_STATE_ON);
}

// Indicates that the Resolver should generate the requested report type
void resolver_t::RequestReportGeneration(const GUID *pReportType) {
    DEBUGCHK(m_pluginState != PLUGIN_STATE_ERROR);

    DWORD index = FindGeneratedReportIndex(pReportType);
    DEBUGCHK(index != (DWORD)-1);

    if (m_reportStates[index] == PLUGIN_STATE_OFF) {
        // Only update the state if we're transitioning from the OFF state.
        // If we're in another state (say already ON or Unavail) then leave it as it previously was.
        SetState(pReportType,PLUGIN_STATE_STARTING_UP);
    }
}

void resolver_t::CancelReportGeneration(const GUID *pReportType) {
    DEBUGCHK(CanGenerateReportType(pReportType));
    DEBUGCHK(m_pluginState >= PLUGIN_STATE_UNAVAILABLE);

    // Set individual report state to shutting down for now.  resolver_t::StopPlugin
    // and/or GetResolverLocation will ultimately set this state to OFF.
    SetState(pReportType,PLUGIN_STATE_SHUTTING_DOWN);
}


// Change the state of particular reportType in resolver.  Note that report
// types in the same resolver may be independent of each other; i.e. generated
// report type #1 may be PLUGIN_STATE_ON while #2 report is PLUGIN_STATE_OFF/UNAVAIL/...
void resolver_t::SetState(const GUID *pReportType, PLUGIN_STATE newState) {
    if (m_pluginState == PLUGIN_STATE_ERROR) {
        // Once plugin has entered an error condition, it can never get out of it.
        return;
    }

    DWORD index = FindGeneratedReportIndex(pReportType);
    // Plugin Manager will only call this function if resolver supports it.
    DEBUGCHK(index < m_numGeneratedReportTypes);

#ifdef DEBUG
    if (newState != m_reportStates[index]) {
        DEBUGMSG(ZONE_RESOLVER && ZONE_VERBOSE,(L"LOC: Changing plugin <%s>, for Report type <" SVSUTIL_GUID_FORMAT_W 
                                                L"> from state <%d> to state <%d>\r\n",
                 GetName(),SVSUTIL_PGUID_ELEMENTS(pReportType),
                 m_pluginState,newState));
    }
#endif

    m_reportStates[index] = newState;

    // What we change the "global" resolver state to (i.e. what we tell 
    // apps querying this plugin state) is based on the "highest" available
    // report.  The PLUGIN_STATE enum is ordered with the highest 
    // (PLUGIN_STATE_ON) coming last.
    m_pluginState = PLUGIN_STATE_ERROR;

    for (DWORD i = 0; i < m_numGeneratedReportTypes; i++)
        m_pluginState = max(m_pluginState,m_reportStates[i]);
}

// Helper to change all report states at the same type
void resolver_t::SetAllReportStates(PLUGIN_STATE newState) {
    for (DWORD i = 0; i < m_numGeneratedReportTypes; i++)
        m_reportStates[i] = newState;

    m_pluginState = newState;
}

// Indicates that a fatal error has occured 
void resolver_t::SetFatalError() {
    SetAllReportStates(PLUGIN_STATE_ERROR);
}

// Function that sets up resolver control block to be sent to the resolver
void resolver_t::FillRCB(RESOLVER_CONTROL_BLOCK *pRcb, RESOLVER_INFORMATION *pResInfo, reportSmartPtr_t &pProvReport) {
    pRcb->numTypesToResolve   = 0;

    // Determine which report types the resolver should be asked to return.
    for (DWORD i = 0; i < m_numGeneratedReportTypes; i++) {
        PLUGIN_STATE repState = m_reportStates[i];

        if ((repState == PLUGIN_STATE_ON) || (repState == PLUGIN_STATE_STARTING_UP) ||
            (repState == PLUGIN_STATE_UNAVAILABLE))
        {
            // In this case we try to resolve this report
            memcpy(&pRcb->typesToResolve[pRcb->numTypesToResolve],&m_generatedReportTypes[i],sizeof(GUID));
            pRcb->numTypesToResolve++;
        }
    }

    if (pRcb->numTypesToResolve == 0) {
        // No work to do in this case; caller will return immediately
        return;
    }

    GetResolverInfo(pResInfo);

    pRcb->version           = LOCATION_FRAMEWORK_VERSION_CURRENT;
    pRcb->resContext        = (HANDLE)this;
    pRcb->pResInfo          = pResInfo;

    pProvReport             = m_reportCollector->GetReport();
    pRcb->provReport        = pProvReport;

    pRcb->NewResolverReport = NewResolverReportInd;
}

// Calls into the resolver's ResolverGetLocation to resolve reports.
void resolver_t::GetResolverLocation(void) {
    RESOLVER_CONTROL_BLOCK rcb;
    RESOLVER_INFORMATION   resInfo;
    DWORD                  i;
    FILETIME               reportCreationTime;

    g_pLocLock->Lock();

    DEBUGCHK(m_workerThrdCookie != 0); 

    // Hold a smart pointer reference to the memory returned by the RC.
    // Do this so that should the RC receive a new report during processing
    // of this function, the memory won't be freed because we'll have an 
    // extra ref count.
    reportSmartPtr_t pProvReport = NULL;

    DEBUGMSG(ZONE_RESOLVER | ZONE_THREAD,(L"LOC: Beginning ResolverGetLocation thread for resolver <%s>\r\n",GetName()));

    if (m_pluginState <= PLUGIN_STATE_SHUTTING_DOWN) {
        DEBUGMSG(ZONE_RESOLVER,(L"LOC: Exiting thread for ResolveLookup for <%s> since state = <%d>\r\n",
                                GetName(),m_pluginState));
        m_workerThrdCookie = 0;
        goto done;
    }

    if (m_providersAvailable == FALSE) {
        // There is a small time window where the last provider could've become unavailable
        // before this worker thread could be unscheduled.  Since we ignore 
        // new resolver reports once we reach this stage anyway, don't even
        // bother calling resolver DLL export.
        DEBUGMSG(ZONE_RESOLVER,(L"LOC: Exiting thread for ResolveLookup for <%s> since no providers are available\r\n",GetName()));
        m_workerThrdCookie = 0;
        goto done;
    }

    // Fill out data structures to pass to LF.
    FillRCB(&rcb,&resInfo,pProvReport);
    memcpy(&reportCreationTime,&((LOCATION_REPORT_BASE*)rcb.provReport)->creationTime,sizeof(reportCreationTime));
    // Reset our received reports for the next pass
    memset(m_receivedReport,0,sizeof(m_receivedReport));


    // If we don't need any report types resolved, then stop now.
    if (rcb.numTypesToResolve == 0) {
        DEBUGMSG(ZONE_RESOLVER,(L"LOCATION: GetResolverLocation thread for <%s> returning since no reports needed to resolve\r\n",GetName()));
        m_workerThrdCookie = 0;
        goto done;
    }

#ifdef DEBUG
    if (ZONE_RESOLVER) {
        DEBUGMSG(1,(L"LOC: About to call ResolverGetLocation for Resolver <%s> with following report type(s) to resolve\r\n",GetName()));
        DbgPrintGuids(rcb.typesToResolve,rcb.numTypesToResolve);
    }
#endif

    // Indicate that this thread has now been called.
    m_getRepThrdCalledFirstTime = TRUE;

    // Actually call the DLL export.  This may block for a long time potentially,
    // but it is not designed to block as long as there are apps registered
    // for it (unlike ProviderGetLocation()).
    DWORD err = CallResGetLocation(&rcb);
    if (err != ERROR_SUCCESS) {
        ReportFatalError();
        m_workerThrdCookie = 0;
        goto done;
    }

    // During the resolution cycle, the LF service itself has 
    // been shutdown.  No point in continuing.
    if (g_serviceState != SERVICE_STATE_ON) {
        m_workerThrdCookie = 0;
        goto done;
    }

    // Now that call has returned, for any registered report type for 
    // which a report was not returned, mark as unavailable and have
    // plugin manager start the next in line.
    for (i = 0; i < m_numGeneratedReportTypes; i++) {
        if ((m_reportStates[i] >= PLUGIN_STATE_STARTING_UP) && !m_receivedReport[i]) {
            // We wanted a report for this type but resolver did not generate one.
            // The first time this occurs, alert plugin manager.
            g_pPluginMgr->ResolverReportTypeUnavailable(this, &m_generatedReportTypes[i]);
            DEBUGCHK(m_reportStates[i] == PLUGIN_STATE_UNAVAILABLE);
        }
    }

    m_workerThrdCookie = 0;
    GetCurrentFT((FILETIME*)&m_lastResolution);

    if (m_providersAvailable == FALSE) {
        // During ResolverGetLocationCall(), all possible providers that could
        // generate reports for this resolver have become unavailable.
        // Do not reschedule another worker thread - wait for provider 
        // to become available again.
        goto done;
    }

    if (m_pluginState == PLUGIN_STATE_UNAVAILABLE) {
        // If we failed to resolve any reports in our last resolution
        // attempt, then we need to try again. 
        ScheduleNextResolution();
    }
    else if ((m_pluginState >= PLUGIN_STATE_STARTING_UP) && (m_reportCollector->HasNewReport(&reportCreationTime))) {
        // A new provider report has arrived since we have completed this resolution.
        // Schedule a new worker thread to do the lookup
        ScheduleNextResolution();
    }
    else {
        ;
        // else {...} we have resolved a report & no new provider reports have 
        // since arrived, so no need to reschedule.  This worker will be rescheduled
        // when the next provider report comes in.
    }

done:
    for (DWORD i = 0; i < m_numGeneratedReportTypes; i++) {
        // Resolver stop may have been called on another thread.  This takes them
        // to state PLUGIN_STATE_SHUTTING_DOWN, it's this thread's responsibility
        // to turn them off.

        // If yet another thread registered for a report type this resolver
        // generates after Resolver.Stop() but before we got here, then 
        // it will have its state set as PLUGIN_STATE_STARTING_UP and it will be
        // available for scheduling just as it should be.
        if (m_reportStates[i] == PLUGIN_STATE_SHUTTING_DOWN)
            SetState(&m_generatedReportTypes[i],PLUGIN_STATE_OFF);
    }

    // Reset this regardless of previous value.
    m_immediateRestart = FALSE;

    DEBUGMSG(ZONE_RESOLVER | ZONE_THREAD,(L"LOC: Ending ResolverGetLocation thread for resolver <%s>\r\n",GetName()));
    g_pLocLock->Unlock();
}

// A provider that generates a report that the resolver can resolve
// indicates it (via the report collector) by calling this function.
// Possibly save new provider report and possibly spin worker thread
void resolver_t::NewReportFromProvider(provider_t *pProv, reportCol_t *pRC) {
    DEBUGCHK(g_pLocLock->IsLocked());
    DEBUGCHK(pProv->GetState() == PLUGIN_STATE_ON);

    if (m_pluginState <= PLUGIN_STATE_SHUTTING_DOWN) {
        // Resolvers are always given indications of new reports, even when
        // they're not active.  Check that we care about report here.
        return;
    }

    // Do checks to make sure that the provider generating this report is one
    // that we care about.  Consider for instance a system with Providers P1
    // and P2, P1 higher priority, both capable of generating reports for this
    // resolver.  If P1 has given us a report already, when P2 calls this function
    // we want to IGNORE P2's report since P1 was higher priority.

    // Relying on the reports collector's priority filtering to do this work
    // for the resolver will not work in all cases, so do it again here.

    if (m_providerGenerator == NULL) {
        // No provider registered yet, so use this provider
        ;
    }
    else if (m_providerGenerator->GetState() != PLUGIN_STATE_ON) {
        // We have another (possibly higher priority) plugin associated
        // with this RC already, however that plugin is not active.
        // In this case, take what we can get and use the plugin that is 
        // currently generating data for us.
        DEBUGCHK(m_providersAvailable);
    }
    else if (m_providerGenerator->GetStartOrder() < pProv->GetStartOrder())
    {
        // We have another plugin generating reports and it is of higher
        // priority, so ignore any reports this plugin happens to generate.
        DEBUGMSG(ZONE_PLUGIN,(L"LOC: Resolver <%s> ignores provider <%s> report since a higher prio plugin generates a supported type for this resolver\r\n",
                 GetName(),pProv->GetName()));
        DEBUGCHK(m_providersAvailable);
        return;
    }

    // There must be a provider available for us to have got this reports.
    m_providersAvailable = TRUE;

    m_providerGenerator = pProv;
    m_reportCollector   = pRC;

    // Store the report the provider generated
    DEBUGMSG(ZONE_PLUGIN,(L"LOC: Provider <%s> has given resolver <%s> a new report to resolve\r\n",
             pProv->GetName(),GetName()));

    if (m_workerThrdCookie == 0) {
        // m_workerThrdCookie != 0 means thread has already been scheduled.
        // Only schedule one thread per resolver.
        ScheduleNextResolution();
    }
}

// When there are no providers that generate any reports this resolver
// can support, then record this and take resolver to an unavailable state.
void resolver_t::NoProvidersAvailable(void) {
    DEBUGCHK(g_pLocLock->IsLocked());
    DEBUGCHK(m_providersAvailable);

    DEBUGMSG(ZONE_RESOLVER,(L"LOC: Resolver <%s> does not have any providers available currently.  Moving to unavailable state\r\n",
             GetName()));

    m_providersAvailable = FALSE;

    // Mark any report type that is starting up or ON as unavailable.
    for (DWORD i = 0; i < m_numSupportedReportTypes; i++) {
        if (m_reportStates[i] > PLUGIN_STATE_UNAVAILABLE)
            SetState(&m_generatedReportTypes[i],PLUGIN_STATE_UNAVAILABLE);
    }

    // Unschedule the worker thread, if there is one and if we can.
    // This will NOT terminate a running ResolverGetLocation thread, but if 
    // one was scheduled to run in the future it will make sure we don't bother
    // doing this work.
    if (m_workerThrdCookie && g_pThreadPool->UnScheduleEvent(m_workerThrdCookie)) {
        DEBUGMSG(ZONE_RESOLVER | ZONE_THREAD, (L"LOC: Unscheduled ResolverGetLocation worker cookie <%d> because of no available provs\r\n",
                 m_workerThrdCookie));
        m_workerThrdCookie = 0;
    }
}

// Determine how far into the future the next call to GetResolverLocation worker
// thread should be.  This is based on the last time the resolver was called,
// whether the call was successful (at least one report resolved) or not, 
// and the resolver's registry config for requery timeouts.
DWORD resolver_t::DetermineDelayUntilNextResolution(void) {
    DWORD timeout;

    if (GetState() == PLUGIN_STATE_UNAVAILABLE) {
        // Last attempt to query plugin failed
        timeout = m_retryOnFailure;
    }
    else {
        // Last attempt to query plugin was successful (for at least one report)
        // or this is the first time we're starting up the plugin after it 
        // has been stopped for a while.
        timeout = m_minRequery;
    }

    __int64 currentTime;
    GetCurrentFT((FILETIME*)&currentTime);

    __int64 resDeltaFT = currentTime - m_lastResolution;

    if ((resDeltaFT < 0) || ((resDeltaFT / SVS_FILETIME_TO_MILLISECONDS) > MAXDWORD)) {
        // The system clock has moved backwards or way into future since 
        // the last resolution.  In this case just 
        // schedule the next resolution to be immediate.
        return 0;
    }

    // Number of milliseconds since last resolution.
    DWORD resDeltaMS = (resDeltaFT / SVS_FILETIME_TO_MILLISECONDS);
    
    if (resDeltaMS < timeout) {
        // The next time we should be scheduled is in the future
        return (timeout - resDeltaMS);
    }

    // Otherwise we should schedule the resolution to run immediately.
    return 0;
}

// Schedule the next thread to call ResolverGetLocation on this resolver.
void resolver_t::ScheduleNextResolution(void) {
    DEBUGCHK(g_pLocLock->IsLocked());
    DEBUGCHK(m_workerThrdCookie == 0);

    DWORD delay;

    if (m_getRepThrdCalledFirstTime == TRUE) {
        delay = DetermineDelayUntilNextResolution();
    }
    else {
        // This is the first time ResolverGetLocation is to be scheduled.
        // Schedule it immediately.
        delay = 0;
    }

    DEBUGMSG(ZONE_RESOLVER,(L"LOC: Scheduling resolver thread for <%s> to run <%d> ms from now\r\n",
                              GetName(),delay));

    m_workerThrdCookie = g_pThreadPool->ScheduleEvent(GetResolverLocationThread,this,delay);

#ifdef DEBUG
    if (m_workerThrdCookie == 0) {
        DEBUGMSG_OOM(); 
    }
    else {
        DEBUGMSG(ZONE_RESOLVER | ZONE_THREAD,(L"LOC: Spinning GetResolverLocation thread for resolver <%s>, cookie=<0x%08x>\r\n",
                 GetName(),m_workerThrdCookie));
    }
#endif
}

// Thread spun to call resolver's ResolverStop export.  We have to
// check to make sure resolver hasn't been restarted in the interim.
void resolver_t::StopPlugin(void) {
    DEBUGCHK(g_pLocLock->IsLocked());
    DEBUGCHK(m_immediateRestart == FALSE);

    // UnScheduleEvent only returns TRUE if the scheduled worker thread
    // was not running.  So try to unschedule the worker if possible.
    if (m_workerThrdCookie && g_pThreadPool->UnScheduleEvent(m_workerThrdCookie)) {
        DEBUGMSG(ZONE_RESOLVER | ZONE_THREAD,(L"LOC: Unscheduled GetResolverLocation thread for <%s>, cookie=<0x%08x>\r\n",
                 GetName(),m_workerThrdCookie));
        m_workerThrdCookie = 0;
    }

    // Reset this, regardless of whether provider(s) is available or not, because
    // the other place we reset this (NewReportFromProvider) will not be called
    // now that resolver has been stopped.  If no providers are available
    // the next time resolver is attempted to be started, then the startup
    // logic will do the right thing and not try to restart this resolver.
    m_providersAvailable = TRUE;

    if (m_getRepThrdCalledFirstTime == FALSE) {
        // Possible in case where the stop was called before the worker
        // thread got started.  Since resolver's GetResolverLocation was never
        // called, don't bother calling its stop routine.

        // This could happen if a resolver requested a provider to generate
        // reports for it, but the resolver was asked to stop before the provider 
        // generated a report.

        // Transition straight to OFF - no more work required here
        SetAllReportStates(PLUGIN_STATE_OFF);
        return;
    }

    if (m_workerThrdCookie == 0) {
        // If the worker thread isn't running, then we can immediately transition
        // to the off-state.
        SetAllReportStates(PLUGIN_STATE_OFF);
    }
    else {
        // If the worker thread is running, then we can't immediately indicate
        // the report states as off.  We need instead to to mark them as 
        // shutting down and leave it to worker to set them as off once it has
        // returned from its call to ResolverGetLocation.

        for (DWORD i = 0; i < m_numGeneratedReportTypes; i++) {
            // Update state only if it was previously on (don't take
            // a state from Off->ShuttingDown for instance)
            if (m_reportStates[i] >= PLUGIN_STATE_UNAVAILABLE)
                m_reportStates[i] = PLUGIN_STATE_SHUTTING_DOWN;
        }
        m_pluginState = PLUGIN_STATE_SHUTTING_DOWN;
    }

    CallResStop();
    m_getRepThrdCalledFirstTime = FALSE;

    DEBUGMSG(ZONE_RESOLVER | ZONE_THREAD,(L"LOC: Completed ResolverStop thread for resolver <%s>\r\n",GetName()));
}

// Calls ResolverInitialize export
DWORD resolver_t::CallResInit(RESOLVER_INFORMATION *pResInfo) {
    DEBUGCHK(g_pLocLock->IsLocked());

    DWORD err;
    __try {
        DEBUGMSG(ZONE_RESOLVER,(L"LOC: Calling ResolverInitialize on DLL=<%s>\r\n",GetName()));
        // Initialize is only called on LF startup.  OK to hold lock.
        err = m_pfnInitialize(pResInfo);
        DEBUGMSG(ZONE_RESOLVER,(L"LOC: ResolverInitialize on DLL=<%s> returned <0x%08x>\r\n",GetName(),err));
    } __except(REPORT_EXCEPTION_TO_WATSON()) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: ResolverInitialize caused an exception in DLL=<%s>\r\n",GetName()));
        // ReportFatalError(); // Do not call this - plugin constructor will immediately unload it on any error on init.
        err = ERROR_INTERNAL_ERROR;
    }
    return err;
}

// Calls ResolverGetLocation export 
DWORD resolver_t::CallResGetLocation(RESOLVER_CONTROL_BLOCK *pResControlBlock) {
    g_pLocLock->Unlock();

    DWORD err;
    __try {
        DEBUGMSG(ZONE_RESOLVER,(L"LOC: Calling ResolverGetLocation on DLL=<%s>\r\n",GetName()));
        err = m_pfnGetLocation(pResControlBlock);
        DEBUGMSG(ZONE_RESOLVER,(L"LOC: ResolverGetLocation on DLL=<%s> returned <0x%08x>\r\n",GetName(),err));
    } __except(REPORT_EXCEPTION_TO_WATSON()) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: ResolverGetLocation caused an exception in DLL=<%s>\r\n",GetName()));
        ReportFatalError();
        err = ERROR_INTERNAL_ERROR;
    }

    g_pLocLock->Lock();
    return err;
}

// Calls ResolverStop export.  See CallProvStop() comments for more info.
DWORD resolver_t::CallResStop(void) {
    DEBUGCHK(g_pLocLock->IsLocked());
    DEBUGCHK(m_getRepThrdCalledFirstTime == TRUE);

    DWORD err;
    __try {
        DEBUGMSG(ZONE_RESOLVER,(L"LOC: Calling ResolverStop on DLL=<%s>\r\n",GetName()));
        err = m_pfnStop();
        DEBUGMSG(ZONE_RESOLVER,(L"LOC: ResolverStop on DLL=<%s> returned <0x%08x>\r\n",GetName(),err));
    } __except(REPORT_EXCEPTION_TO_WATSON()) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: ResolverStop caused an exception in DLL=<%s>\r\n",GetName()));
        ReportFatalError();
        err = ERROR_INTERNAL_ERROR;
    }

    return err;
}

// Calls ResolverUnInitialize export
DWORD resolver_t::CallResUnInit(void) {
    DEBUGCHK(g_pLocLock->IsLocked());

    DWORD err;
    __try {
        DEBUGMSG(ZONE_RESOLVER,(L"LOC: Calling ResolverUnInitialize on DLL=<%s>\r\n",GetName()));
        // Uninit is only called on LF shutdown.  OK to hold lock.
        err = m_pfnUnInitialize();
        DEBUGMSG(ZONE_RESOLVER,(L"LOC: ResolverUnInitialize on DLL=<%s> returned <0x%08x>\r\n",GetName(),err));
    } __except(REPORT_EXCEPTION_TO_WATSON()) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: ResolverUnInitialize caused an exception in DLL=<%s>\r\n",GetName()));
        err = ERROR_INTERNAL_ERROR;
        // ReportFatalError(); - since the plugin is being unloaded anyway, don't bother
    }
    
    return err;
}

// Calls ResolverIoctlOpen export
DWORD resolver_t::CallResIoctlOpen(void) {
    m_numActiveIoctlCallers++;
    g_pLocLock->Unlock();
    
    DWORD err;
    __try {
        DEBUGMSG(ZONE_RESOLVER,(L"LOC: Calling ResolverIoctlOpen on DLL=<%s>\r\n",GetName()));
        err = m_pfnIoctlOpen();
        DEBUGMSG(ZONE_RESOLVER,(L"LOC: ResolverIoctlOpen on DLL=<%s> returned <0x%08x>\r\n",GetName(),err));
    } __except(REPORT_EXCEPTION_TO_WATSON()) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: ResolverIoctlOpen caused an exception in DLL=<%s>\r\n",GetName()));
        ReportFatalError();
        err = ERROR_INTERNAL_ERROR;
    }

    g_pLocLock->Lock();
    m_numActiveIoctlCallers--;
    return err;
}

// Calls ResolverIoctlCall export
DWORD resolver_t::CallResIoctlCall(HANDLE h, DWORD dwCode, BYTE *pBufIn, DWORD cbIn, BYTE *pBufOut, DWORD *pcbOut) {
    m_numActiveIoctlCallers++;
    g_pLocLock->Unlock();

    DWORD err;
    __try {
        DEBUGMSG(ZONE_RESOLVER,(L"LOC: Calling ResolverIoctlCall on DLL=<%s>\r\n",GetName()));
        err = m_pfnIoctlCall(h, dwCode, pBufIn, cbIn, pBufOut, pcbOut);
        DEBUGMSG(ZONE_RESOLVER,(L"LOC: ResolverIoctlCall on DLL=<%s> returned <0x%08x>\r\n",GetName(),err));
    } __except(REPORT_EXCEPTION_TO_WATSON()) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: ResolverIoctlCall caused an exception in DLL=<%s>\r\n",GetName()));
        ReportFatalError();
        err = ERROR_INTERNAL_ERROR;
    }

    g_pLocLock->Lock();
    m_numActiveIoctlCallers--;
    return err;
}

// Calls ResolverIoctlClose export
DWORD resolver_t::CallResIoctlClose(HANDLE h) {
    m_numActiveIoctlCallers++;
    g_pLocLock->Unlock();

    DWORD err;
    __try {
        DEBUGMSG(ZONE_RESOLVER,(L"LOC: Calling ResolverIoctlClose on DLL=<%s>\r\n",GetName()));
        err = m_pfnIoctlClose(h);
        DEBUGMSG(ZONE_RESOLVER,(L"LOC: ResolverIoctlClose on DLL=<%s> returned <0x%08x>\r\n",GetName(),err));
    } __except(REPORT_EXCEPTION_TO_WATSON()) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: ResolverIoctlClose caused an exception in DLL=<%s>\r\n",GetName()));
        ReportFatalError();
        err = ERROR_INTERNAL_ERROR;
    }

    g_pLocLock->Lock();
    m_numActiveIoctlCallers--;
    return err;
}


