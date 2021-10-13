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

// Abstract: Location framework reports collector

#include <locCore.hpp>


//
//  registeredApp_t - when an application calls LocationRegisterForReport, a corresponding
//  registeredApp_t object is created to track it.
//

registeredApp_t::registeredApp_t(locOpenHandle_t *pLocHandle, HANDLE hNewLoc, HANDLE hNewState) {
    m_isInited           = FALSE;
    m_hNewLocationReport = 0;
    m_hStateChange       = 0;
    m_pLocHandle         = pLocHandle;

    // Duplicate handles (needed because handles come to us from caller process)
	HANDLE hCallerProc   = GetCallerProcess();
	HANDLE hCurrentProc  = GetCurrentProcess();

	if (hCallerProc == 0) {
		// We're being called from the same process.
		hCallerProc = hCurrentProc;
	}

    if (hNewLoc && !DuplicateHandle(hCallerProc,hNewLoc,hCurrentProc,&m_hNewLocationReport,0,FALSE,DUPLICATE_SAME_ACCESS)) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: DuplicateHandle() failed, GLE=0x%08x\r\n",GetLastError()));
        return;
    }

    if (hNewState && !DuplicateHandle(hCallerProc,hNewState,hCurrentProc,&m_hStateChange,0,FALSE,DUPLICATE_SAME_ACCESS)) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: DuplicateHandle() failed, GLE=0x%08x\r\n",GetLastError()));
        return;
    }

    m_isInited = TRUE;
}

// When a new report has been passed to RC, alert any apps registered.
void registeredApp_t::IndicateNewLocationReport() {
    if (m_hNewLocationReport) {
        DEBUGMSG(ZONE_REPCOL,(L"LOC: Indicating new loc report for pLoc=<0x%08x>, setting event=<0x%08x>\r\n",
                 m_pLocHandle,m_hNewLocationReport));
        SetEvent(m_hNewLocationReport);
    }
}

// When a plugin state has changed, alert application registered
void registeredApp_t::IndicateStateChange() {
    if (m_hStateChange) {
        DEBUGMSG(ZONE_REPCOL,(L"LOC: Indicating location state change for pLoc=<0x%08x>, setting event=<0x%08x>\r\n",
                 m_pLocHandle,m_hStateChange));
        SetEvent(m_hStateChange);
    }
}

registeredApp_t::~registeredApp_t() {
    // Handles are automatically closed via auto_handle destructor
    ;
}

//
// reportCol_t - Reports Collector (RC) object implementation
//
reportCol_t::reportCol_t(GUID *pReportType) {
    memcpy(&m_reportType,pReportType,sizeof(m_reportType));
    m_pluginGenerator = NULL;

#ifdef DEBUG
    wsprintf(m_reportTypeString,SVSUTIL_GUID_FORMAT_W,SVSUTIL_RGUID_ELEMENTS(m_reportType));
#endif
}

reportCol_t::~reportCol_t() {
}

// Returns whether or not given LocationOpen handle has registered with 
// this particular report type or not.
registeredApp_t * reportCol_t::FindRegisteredApp(locOpenHandle_t *pLocHandle) {
    registeredAppIter_t it    = m_registeredApplications.begin();
    registeredAppIter_t itEnd = m_registeredApplications.end();

    for (; it != itEnd; ++it) {
        if ((*it)->IsLocHandle(pLocHandle))
            return (*it);
    }
    return NULL;
}

// If pRes is in the list of registered resolvers return it.  Otherwise return NULL.
resolver_t *reportCol_t::FindRegisteredResolver(resolver_t *pRes) {
    resolverIter_t it    = m_registeredResolvers.begin();
    resolverIter_t itEnd = m_registeredResolvers.end();

    for (; it != itEnd; ++it) {
        if ((*it) == pRes)
            return pRes;
    }
    return NULL;
}

// An application registers to receive state change events when 
// new reports of this reportType arrive or when plugins generating
// this report type have a state change.
DWORD reportCol_t::RegisterAppForReport(locOpenHandle_t *pLocHandle,  HANDLE hNewLocationReport,
                                        HANDLE hStateChangeEvent)
{
    if (NULL != FindRegisteredApp(pLocHandle)) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: A location open handle may only register once per report type.  Handle already registered\r\n"));
        return ERROR_ALREADY_REGISTERED;
    }

    registeredApp_t *pRegApp = new registeredApp_t(pLocHandle,hNewLocationReport,hStateChangeEvent);
    if (NULL == pRegApp) {
        DEBUGMSG_OOM();
        return ERROR_OUTOFMEMORY;
    }

    if ((! pRegApp->IsInited()) || (! m_registeredApplications.push_back(pRegApp))) {
        DEBUGMSG_OOM();
        delete pRegApp;
        return ERROR_OUTOFMEMORY;
    }

    return ERROR_SUCCESS;
}

// A resolver wants to receive notifications when a new report 
// of this type is generated by a provider.
DWORD reportCol_t::RegisterResolverForReport(resolver_t *pRes) {
    if (NULL != FindRegisteredResolver(pRes)) {
        // Only let resolver be registered once per report type.  Not having
        // ref-counting simplifies implementation enormously.
        return ERROR_ALREADY_REGISTERED;
    }

    if (! m_registeredResolvers.push_back(pRes)) {
        DEBUGMSG_OOM();
        return ERROR_OUTOFMEMORY;
    }
    return ERROR_SUCCESS;
}

// On initialization, add all resolvers that can support
// report that this RC stores to the RC's m_resolvers list.
DWORD reportCol_t::AddToResolversList(resolver_t *pRes) {
    if (! m_resolvers.push_back(pRes)) {
        DEBUGMSG_OOM();
        return ERROR_OUTOFMEMORY;
    }
    return ERROR_SUCCESS;
}

// Application calls into this when it no longer wants a particular
// report type to be registered.
DWORD reportCol_t::UnRegisterAppForReport(locOpenHandle_t *pLocHandle) {
    registeredApp_t *pRegApp = FindRegisteredApp(pLocHandle);
    if (NULL == pRegApp) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: UnRegisterAppForReport fails because a location open handle was not registered with this report type\r\n"));
        return ERROR_INVALID_PARAMETER;
    }

    m_registeredApplications.remove(pRegApp);
    delete pRegApp;

    return ERROR_SUCCESS;
}

// When a resolver no longer needs a provider to generate a particular report
// type, it calls this function.
DWORD reportCol_t::UnRegisterResolverForReport(resolver_t *pRes) {
    pRes = FindRegisteredResolver(pRes);
    if (NULL == pRes)
        return ERROR_INVALID_PARAMETER;

    // Do NOT free up this memory.  Resolvers are only added and deleted
    // at service init and uninit.
    m_registeredResolvers.remove(pRes);
    return ERROR_SUCCESS;
}

// Application wants to retrieve the latest report that this RC has received,
// assuming report was generated within maximumAge timeframe.
DWORD reportCol_t::GetReport(DWORD maximumAge, LOCATION_REPORT *pLocationReport, 
                             DWORD *pcbLocationReport)
{
    DEBUGCHK(g_pLocLock->IsLocked());

    if (m_report == NULL) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: Report type <%s> does not have any location reports.\r\n",
                 m_reportTypeString));
        return ERROR_NO_DATA;
    }

    LOCATION_REPORT_BASE *pReportBase = (LOCATION_REPORT_BASE*)((LOCATION_REPORT*)m_report);

    DWORD reportAge = LocationGetReportAge(pReportBase);

    if (reportAge > maximumAge) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: Report type <%s> is too old <%d ms> for app <requested %d or newer ms>.\r\n",
                   m_reportTypeString,reportAge,maximumAge));
        return ERROR_TIMEOUT;
    }

    if (*pcbLocationReport < pReportBase->size) {
        *pcbLocationReport = pReportBase->size;
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: Report type <%s> last report is <%d> bytes, app passed in <%d> bytes.\r\n",
                   m_reportTypeString,pReportBase->size,*pcbLocationReport));
        return ERROR_INSUFFICIENT_BUFFER;
    }

    *pcbLocationReport = pReportBase->size;
    memcpy(pLocationReport,pReportBase,pReportBase->size);
    return ERROR_SUCCESS;
}

// Returns raw pointer to report data
reportSmartPtr_t reportCol_t::GetReport(void) {
    DEBUGCHK(m_report != NULL);
    return m_report;
}

// Determines if a new report has arrived with a different creation
// time than the one that is currently being considered
BOOL reportCol_t::HasNewReport(FILETIME *pCreationTime) {
    LOCATION_REPORT_BASE *pReportBase = (LOCATION_REPORT_BASE*)((LOCATION_REPORT*)m_report);
    return ! (pReportBase->creationTime.dwLowDateTime == (*pCreationTime).dwLowDateTime &&
              pReportBase->creationTime.dwHighDateTime == (*pCreationTime).dwHighDateTime);
}

// Indicates whether or not applications or resolvers currently have
// requested the report type be generated. 
BOOL reportCol_t::HasRegisteredAppsOrResolvers(void) {
    return (! (m_registeredApplications.empty() && m_registeredResolvers.empty()) ); 
}

// Indicates whether any applications have requested this report type.
BOOL reportCol_t::HasRegisteredApps(void) {
    return (! m_registeredApplications.empty()); 
}

BOOL reportCol_t::HasRegisteredResolvers(void) {
    return (! m_registeredResolvers.empty()); 
}


// Plugin manager calls this when it receives a new report from pPlugin
// Returns TRUE when it accepts the new report (i.e. it's from a plugin
// of proper priority, no OOM, etc..) and FALSE if it is discarding the report.
BOOL reportCol_t::NewReport(plugin_t *pPlugin, LOCATION_REPORT *pNewReport) {
    DEBUGMSG(ZONE_REPCOL,(L"LOC: ReportCol type <%s> has received new report <0x%08x> from plugin <%s>.\r\n",
             m_reportTypeString,pNewReport,pPlugin->GetName()));

#ifdef DEBUG
    DEBUGCHK(g_pLocLock->IsLocked());
    DEBUGCHK(pPlugin->GetState() == PLUGIN_STATE_ON);
    DEBUGCHK(((LOCATION_REPORT_BASE *)pNewReport)->type == m_reportType);
#endif

    if (m_pluginGenerator == NULL)    {
        // This is the first time a new report has come in.
        m_pluginGenerator = pPlugin;
    }
    else if (m_pluginGenerator->GetState() != PLUGIN_STATE_ON) {
        // We have another (possibly higher priority) plugin associated
        // with this RC already, however that plugin is not active.
        // In this case, take what we can get and use the plugin that is 
        // currently generating data for us.
        m_pluginGenerator = pPlugin;
    }

    if (m_pluginGenerator->GetStartOrder() < pPlugin->GetStartOrder())
    {
        // We have another plugin generating reports and it is of higher
        // priority, so ignore any reports this plugin happens to generate.
        DEBUGMSG(ZONE_REPCOL,(L"LOC: Ignoring new report for type <%s> since a higher prio plugin generates same type\r\n",
                 m_reportTypeString));
        return FALSE;
    }

    // Potentially reset RC's plugin generator, in the event that we were
    // relying on a lower priority plugin but now have a higher priority one.
    m_pluginGenerator = pPlugin;

    DWORD reportSize = ((LOCATION_REPORT_BASE *)pNewReport)->size;

    m_report = new BYTE[reportSize];
    if (! m_report.valid()) {
        DEBUGMSG_OOM();
        return FALSE;
    }
    memcpy(m_report,pNewReport,reportSize);

    registeredAppIter_t it     = m_registeredApplications.begin();
    registeredAppIter_t itEnd  = m_registeredApplications.end();
    // Indicate new report to registered applications.
    for (; it != itEnd; ++it)
        (*it)->IndicateNewLocationReport();

    if (pPlugin->IsAProvider()) {
        // If new report is from a provider, indicate to all resolvers 
        // that support this report type the new report.
        resolverIter_t itRes    = m_resolvers.begin();
        resolverIter_t itResEnd = m_resolvers.end();

        for (; itRes != itResEnd; ++itRes)
            (*itRes)->NewReportFromProvider((provider_t*)pPlugin,this);
    }

    return TRUE;
}

// Called when a plugin's state changes in order to alert registered applications
void reportCol_t::PluginStateChange(void) {
    DEBUGCHK(g_pLocLock->IsLocked());

    registeredAppIter_t it    = m_registeredApplications.begin();
    registeredAppIter_t itEnd = m_registeredApplications.end();

    // Indicate new report to registered applications.
    for (; it != itEnd; ++it)
        (*it)->IndicateStateChange();
}


