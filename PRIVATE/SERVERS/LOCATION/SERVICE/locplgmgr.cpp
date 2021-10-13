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

// Abstract: Location framework plugin manager

#include <locCore.hpp>

// Global plugin  manager
pluginMgr_t * g_pPluginMgr;

//*****************************************************************************
//  Init and deinit routines for the location framework
//*****************************************************************************

// Main initialization for the plugin manager.
pluginMgr_t::pluginMgr_t() {
    DEBUGCHK(g_pLocLock->IsLocked());
    DEBUGCHK(g_serviceState == SERVICE_STATE_STARTING_UP);

    m_initSuccess = FALSE;

    CReg reg(HKEY_LOCAL_MACHINE,g_rkLocBase);
    if (!reg.IsOK()) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: Unable to start because base reg key <%s> is not present\r\n",g_rkLocBase));
        return;
    }

    if (! InitProviders(&reg) ||
        ! InitResolvers(&reg) ||
        ! AssignPluginOrder() ||
        ! InitReportCollectors())
    {
        return;
    }

    m_initSuccess = TRUE;
}

// All pointers are smart pointers and auto-freed, so no work in destructor
pluginMgr_t::~pluginMgr_t() {
    DEBUGCHK(g_pLocLock->IsLocked());
    DEBUGCHK((g_serviceState != SERVICE_STATE_ON) && (g_serviceState != SERVICE_STATE_STARTING_UP));
}

BOOL pluginMgr_t::IsPlgMgrInited(void) {
    return m_initSuccess;
}

// Inserts plugin into the list based on its order.  Simple insert algorithm
// since # of plugins will be small.
BOOL pluginMgr_t::InsertPluginOrdered(pluginSmartPtr_t pPlugin) {
    pluginListIter_t it     = m_pluginList.begin();
    pluginListIter_t itEnd  = m_pluginList.end();

    DWORD newPref = pPlugin->GetPreference();

    for (; it != itEnd; ++it) {
        if ((*it)->GetPreference() > newPref) {
            if (! m_pluginList.insert(it,pPlugin)) {
                DEBUGMSG_OOM();
                return FALSE;
            }
            return TRUE;
        }
    }

    if (! m_pluginList.push_back(pPlugin)) {
        DEBUGMSG_OOM();
        return FALSE;
    }
    return TRUE;
}

// Now that plugins have been loaded in a sorted list, indicate to 
// each one what its load order was.  This order will be used later
// when determining when to stop & start plugins after failures.
BOOL pluginMgr_t::AssignPluginOrder(void) {
    pluginListIter_t it    = m_pluginList.begin();
    pluginListIter_t itEnd = m_pluginList.end();

    DWORD index = 0;

    for (; it != itEnd; ++it) {
        (*it)->SetStartOrder(index);
        index++;
    }

    return TRUE;
}

// Read all provider DLL config options, load the DLLs, and get required exports.
BOOL pluginMgr_t::InitProviders(CReg *pBaseReg) {
    CReg providersReg(*pBaseReg,g_rkProviders);
    WCHAR providerName[MAX_PATH];

    DEBUGMSG(ZONE_INIT,(L"LOC: Begin initializing providers...\r\n"));

    // Each provider is represented by a reg subkey under ...\Providers
    while (providersReg.EnumKey(providerName,SVSUTIL_ARRLEN(providerName))) {
        CReg providerReg(providersReg, providerName);

        pluginSmartPtr_t pProv = new provider_t(&providerReg,providerName);
        if (! pProv.valid()) {
            DEBUGMSG_OOM();
            return FALSE;
        }

        if (pProv->GetState() == PLUGIN_STATE_ERROR) {
            // This usually indicates plugin misconfiguration (not a fatal OOM)
            // Don't put plugin into m_pluginList (so it will be deleted
            // automatically) and move on to initialize next plugin.
            continue;
        }

        if (! InsertPluginOrdered(pProv))
            return FALSE;
    }

    DEBUGMSG(ZONE_INIT,(L"LOC: Completed initializing providers...\r\n"));
    return TRUE;
}

// Read all resolver DLL config options, load the DLLs, and get required exports.
BOOL pluginMgr_t::InitResolvers(CReg *pBaseReg) {
    CReg resolversReg(*pBaseReg,g_rkResolvers);
    WCHAR resolverName[MAX_PATH];

    DEBUGMSG(ZONE_INIT,(L"LOC: Begin initializing resolvers...\r\n"));

    // Each resolver is represented by a reg subkey under ...\resolver
    while (resolversReg.EnumKey(resolverName,SVSUTIL_ARRLEN(resolverName))) {
        CReg resolverReg(resolversReg, resolverName);

        pluginSmartPtr_t pRes = new resolver_t(&resolverReg,resolverName);
        if (! pRes.valid()) {
            DEBUGMSG_OOM();
            return FALSE;
        }

        if (pRes->GetState() == PLUGIN_STATE_ERROR) {
            // This usually indicates plugin misconfiguration (not a fatal OOM)
            // Don't put plugin into m_pluginList (so it will be deleted
            // automatically) and move on to initialize next plugin.
            continue;
        }

        if (! InsertPluginOrdered(pRes))
            return FALSE;
    }

    DEBUGMSG(ZONE_INIT,(L"LOC: Completed initializing resolver...\r\n"));
    return TRUE;
}


// Determines what report types each plugin on the system can generate
// and create a report collector for each unique type.
BOOL pluginMgr_t::InitReportCollectors(void) {
    DEBUGMSG(ZONE_REPCOL,(L"LOC: Beginning to initialize reports collectors...\r\n"));

    DEBUGCHK(g_pLocLock->IsLocked());

    pluginListIter_t it    = m_pluginList.begin();
    pluginListIter_t itEnd = m_pluginList.end();

    //
    // Iterate through each plugin on the system.  
    // For each plugin, create an RC for each report type it 
    // generates (assuming one hasn't been created already)
    //
    for (; it != itEnd; ++it) {
        GUID generatedReports[MAX_PLUGIN_REPORT_TYPES];
        DWORD numRepTypes;
        (*it)->GetGeneratedReportInfo(generatedReports,&numRepTypes);

        // Iterate through each report type plugin generates
        for (DWORD i = 0; i < numRepTypes; i++) {
            if (FindRC(&generatedReports[i])) {
                // A report collector for this type has already been 
                // created because another plugin also generates it.
                continue; 
            }

            reportColSmartPtr_t pNewReport = new reportCol_t(&generatedReports[i]);
            if (! pNewReport.valid()) {
                DEBUGMSG_OOM();
                return FALSE;
            }

            if (! m_reportColList.push_back(pNewReport)) {
                DEBUGMSG_OOM();
                // Auto-deleted because it's a smart pointer going out of scope.
                return FALSE;
            }

            DEBUGMSG(ZONE_REPCOL,(L"LOC: Created report collector for report type <" SVSUTIL_GUID_FORMAT_W L">\r\n",
                     SVSUTIL_RGUID_ELEMENTS(generatedReports[i])));
        }
    }


    //
    // For each resolver on the system, for each report type that it supports
    // add it to the appropriate RC's resolver list.
    //

    for (it = m_pluginList.begin(); it != itEnd; ++it) {
        if ((*it)->IsAProvider()) {
            // We only care about resolvers.
            continue;
        }

        resolver_t *pRes = (resolver_t*)((plugin_t*)(*it));
        GUID  supportedReports[MAX_PLUGIN_REPORT_TYPES];
        DWORD numSupportedReports;
        pRes->GetSupportedReportInfo(supportedReports,&numSupportedReports);

        // For each supported report each resolver accepts, find RC (if exists)
        // and associate with it.
        for (DWORD i = 0; i < numSupportedReports; i++) {
            reportCol_t *pRC = FindRC(&supportedReports[i]);
            if (NULL == pRC) {
                // Just because a resolver supports a report doesn't mean anything 
                // on the system generates it (and hence no reports collector)
                continue; 
            }

            if (ERROR_SUCCESS != pRC->AddToResolversList(pRes))
                return FALSE;
        }
    }

    DEBUGMSG(ZONE_REPCOL,(L"LOC: Done initializing reports collectors...\r\n"));
    return TRUE;
}


//*****************************************************************************
//  Misc plugin manager helper functions
//*****************************************************************************

// Finds reports collector associated with a given GUID
reportCol_t * pluginMgr_t::FindRC(const GUID *pGUID) {
    reportColListIter_t it    = m_reportColList.begin();
    reportColListIter_t itEnd = m_reportColList.end();

    for (; it != itEnd; ++it) {
        if (0 == memcmp(pGUID,(*it)->GetReportType(),sizeof(GUID)))
            return (*it);
    }

    return NULL;
}

// Helper to find a plugin based on context passed in from a callback
plugin_t *pluginMgr_t::FindPlugin(HANDLE pluginContext) {
    pluginListIter_t it    = m_pluginList.begin();
    pluginListIter_t itEnd = m_pluginList.end();

    for (; it != itEnd; ++it) {
        if ((*it) == (plugin_t*)pluginContext)
            return (*it);
    }

    DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: Callback Context <0x%08x> is invalid\r\n",pluginContext));
    return NULL;
}


// Returns the plugin that has the specified plugin guid
plugin_t *pluginMgr_t::FindPlugin(const GUID *pPluginGuid) {
    pluginListIter_t it    = m_pluginList.begin();
    pluginListIter_t itEnd = m_pluginList.end();

    for (; it != itEnd; ++it) {
        if ((*it)->IsGuid(pPluginGuid))
            return (*it);
    }

    return NULL;
}

// This function is called when a plugin has had some fatal error,
// such as throwing an exception.  This will call into the plugin itself
// telling it to change its state to PLUGIN_STATE_ERROR and will
// try to load the next plugins for reports this plugin can't generate anymore.

// Once a plugin enters PLUGIN_STATE_ERROR, nothing short of refreshing the LFSVC
// will ever get it out of that state.
void pluginMgr_t::FatalPluginError(plugin_t *pPlugin) {
    DEBUGCHK(g_pLocLock->IsLocked());

    if (pPlugin->GetState() == PLUGIN_STATE_ERROR) {
        // It is possible for this to be called multiple times (i.e. for
        // a plugin that throws multiple exceptions).  Only take action
        // first time plugin gets into this state.
        return;
    }

    DEBUGMSG(ZONE_ERROR,(L"LOC: Plugin <%s> has entered PLUGIN_ERROR_STATE.  Trying to start next available plugins\r\n",
                           pPlugin->GetName()));

    pPlugin->SetFatalError();

    if (g_serviceState == SERVICE_STATE_ON) {
        // If service is on still, then start up next plugins in line.
        StartNextPluginsOnFailure(pPlugin);
    }

    if (pPlugin->IsAResolver()) {
        // A failed resolver may have loaded up providers to do work for it.
        // Stop any providers that are no longer needed
        StopDependentProvidersForResolver((resolver_t*)pPlugin);
    }
    else {
        // If this is last plugin that generated reports for a given resolver,
        // indicate the resolver as unavailable now.
        ProcessProviderFailureOnResolvers((provider_t*)pPlugin);
    }
}


// When an application closes its LocationOpen handle (either via LocationClose call
// or by process shutting-down cleanup), unregister any reports it may have
// forgot to unregister manually.
void pluginMgr_t::UnRegisterEventsFromHandle(locOpenHandle_t *pLocHandle) {
    DEBUGCHK(g_pLocLock->IsLocked());

    reportColListIter_t it      = m_reportColList.begin();
    reportColListIter_t itEnd   = m_reportColList.end();

    for (; it != itEnd; ++it) {
        if (NULL != (*it)->FindRegisteredApp(pLocHandle)) {
            DWORD err = UnRegisterAppForReport(pLocHandle,(*it)->GetReportType());
            // Even if we don't stop the plugin, this should always succeed
            // since only error cases involve bogus input (which we know is valid)
            DEBUGCHK(err == ERROR_SUCCESS);
        }
    }
}

//*****************************************************************************
//  Routines for determining relationships between plugins - especially 
//  with regards to whether certain plugins need to be on at a given time
//*****************************************************************************

// Get the iteration pointer to the plugin of specified start order.
pluginListIter_t pluginMgr_t::GetPluginOfStartOrder(DWORD minStartOrder) {
    DEBUGCHK(minStartOrder <= (m_pluginList.size()));

    if (minStartOrder == (m_pluginList.size())) {
        // This indicates that we're requesting an index one past max
        // number of plugins on system.  This happens when the lowest priority
        // plugin asks to load the next lower prio (which doesn't exist).
        // To avoid sprinkling special case checks for lowestPrio plugin all over the 
        // place, just return iterator::end directly.
        return m_pluginList.end();
    }

    return m_pluginList.begin() + minStartOrder;
}


// Determines if plugin is required for pReportType.  See IsPluginNeeded()
// for details about when/why we call this.
BOOL pluginMgr_t::IsPluginNeededForReport(plugin_t *pPlugin, const GUID *pReportType, BOOL checkRegAppsOnly) {
    // If nothing has registered for this report type, then we do not nee
    DEBUGCHK(pPlugin->CanGenerateReportType(pReportType));
    reportCol_t *pRC = FindRC(pReportType);

    if (checkRegAppsOnly || pPlugin->IsAResolver()) {
        // We only care if a user application 
        // has registered for this report type.  We do NOT care if another resolver
        // has registered for this report type because even if the pPlugin
        // generated this report, the other resolver would never get it
        // since resolvers don't send reports to each other.
        if (! pRC->HasRegisteredApps())
            return FALSE;
    }
    else {
        // For a provider (and checkRegAppsOnly=0), either a resolver or an 
        // application being registered for this report type is interesting.
        if (! pRC->HasRegisteredAppsOrResolvers())
            return FALSE;
    }

    // In this case, there is some client that cares about this report
    // Determine if a higher priority plugin can generate it.
    BOOL highPrioPluginAvail = ExistsHigherPrioPluginForReport(pPlugin,pReportType);

    if (! highPrioPluginAvail) {
        // If this plugin is highest priority on the system that 
        // can generate this particular report type, it's still needed
        // Unless...

        if (pPlugin->IsAProvider() && 
            IsProviderRedundantForResolvers((provider_t*)pPlugin,pReportType))
        {
            // ... there is a special case for providers where
            // the provider may not be needed after all.  See 
            // IsProviderRedundantForResolvers for details.
            return FALSE;
        }
        return TRUE;
    }

    // If none of above conditions are met, then plugin isn't needed
    // in order to generate pReportType.
    return FALSE;
}

// When a plugin goes from the unavailable to available state, plugins
// of a lower priority that generate the same report should be stopped.
// This function determines if a given plugin is in such a state - namely
// are all the reports that it has been requested to generate already
// being generated by a higher priority plugin.

// This is complicated by the fact that plugins can generate multiple report
// types.  Consider for instance plugin P1 that generates reports R1, R2, R3.
// Plugin P2 generates R2, R3, and R4.  If P1 was asked to generate R2 but
// becomes unavailable, then P2 will be started.  If P1 becomes available again,
// we would stop P2 now UNLESS another app requested report R4 to be generated.
// Since P1 doesn't generate R4, we would need to leave P2 on to do this.
BOOL pluginMgr_t::IsPluginNeeded(plugin_t *pPlugin) {
    GUID  generatedReports[MAX_PLUGIN_REPORT_TYPES];
    DWORD numGeneratedReports;
    DWORD minStartOrder = pPlugin->GetStartOrder();

    DEBUGCHK(pPlugin->GetState() >= PLUGIN_STATE_SHUTTING_DOWN);

    pPlugin->GetGeneratedReportInfo(generatedReports, &numGeneratedReports);

    // For each possible report the plugin can generate, determine whether plugin
    // needs to be left on for it.
    for (DWORD i = 0; i < numGeneratedReports; i++) {
        if (IsPluginNeededForReport(pPlugin,&generatedReports[i],FALSE))
            return TRUE;
    }

    return FALSE;
}


// IsProviderRedundantForResolvers determines if provider is no longer 
// required for dependent resolvers since a suitable substitute has been found.

// For instance, consider a system with Providers P1 and P2, P1 higher prio.
// P1 generates report type Rep1, P2 generates report type Rep2.  Resolver Res1
// can support either R1 or R2.
// P1 was unavailable and P2 is on to service Res1.  However once P1 becomes
// available again, we no longer need P2 (assuming no apps registered for Rep2)
// since P1 will generate reports for Res1.

// The base IsPluginNeeded() logic will not detect this subtle relationship
// between providers and resolvers, so we do this extra check here.
BOOL pluginMgr_t::IsProviderRedundantForResolvers(provider_t *pProv, const GUID *pReportType) {
    DEBUGCHK(g_pLocLock->IsLocked());
    DEBUGCHK((pProv->GetState() >= PLUGIN_STATE_UNAVAILABLE) || (pProv->GetState() == PLUGIN_STATE_ERROR));
    DEBUGCHK(! ExistsHigherPrioPluginForReport(pProv,pReportType));

    reportCol_t *pRC = FindRC(pReportType);
    DEBUGCHK(pRC->HasRegisteredAppsOrResolvers());

    if (pRC->HasRegisteredApps()) {
        // If an application has registered for this report type
        // and this is highest priority plugin on system that 
        // can generate it, then the resolver redandancy does not apply
        return FALSE;
    }

    // For each resolver that supports this report type, see if there is an 
    // active provider that can generate reports for the resolver as well.

    pluginListIter_t it    = m_pluginList.begin();
    pluginListIter_t itEnd = m_pluginList.end();

    for (; it != itEnd; ++it) {
        if ((*it)->IsAProvider()) {
            // Only care about resolvers
            continue;
        }

        resolver_t *pRes = (resolver_t*)((plugin_t*)(*it));

        if (pRes->GetState() < PLUGIN_STATE_UNAVAILABLE) {
            // If the resolver is attempting to determine location, then 
            // no provider is needed for it.
            continue;
        }

        if (! pRes->CanSupportReportType(pReportType)) {
            // If resolver can't generate this type anyway, then this provider
            // generating it is irrelevant
            continue;
        }

        // Now see if there is a provider of higher priority than pProv
        // that is active and can generate a report the resolver supports.
        // If one does not exist, then this provider is needed (hence not redundant)
        if (! ExistsHigherPrioActiveProviderForResolver(pRes,pProv->GetStartOrder()))
            return FALSE;
    }

    // For each resolver on the system that supports pReportType, we have 
    // verified that pProv is not needed.
    return TRUE;
}

// Determines if for pReportType, whether there is a higher priority
// plugin AVAILABLE on the system to service it than pPlugin.
BOOL pluginMgr_t::ExistsHigherPrioPluginForReport(plugin_t *pPlugin, const GUID *pReportType) {
    pluginListIter_t it    = m_pluginList.begin();
    pluginListIter_t itEnd = GetPluginOfStartOrder(pPlugin->GetStartOrder());

    // Iterate through each plugin on the system, from highest prio
    // until we reach the current pPlugin.
    for (; it != itEnd; it++) {
        if (! (*it)->CanGenerateReportType(pReportType)) {
            // Plugin can't generate this report in any event.
            continue;
        }

        PLUGIN_STATE pluginState = (*it)->GetReportTypeState(pReportType);
        if (pluginState >= PLUGIN_STATE_STARTING_UP) {
            // There is a higher priority plugin on the system that
            // can generate this report type.
            return TRUE;
        }
    }

    // No higher priority plugin for this report found
    return FALSE;
}

// Determines if there is a provider with start order < minStartOrder AND
// is active AND can generate a report that the given resolver supports.
BOOL pluginMgr_t::ExistsHigherPrioActiveProviderForResolver(resolver_t *pRes, DWORD minStartOrder) {
    pluginListIter_t it    = m_pluginList.begin();
    pluginListIter_t itEnd = GetPluginOfStartOrder(minStartOrder);

    // For each provider...
    for (; it != itEnd; ++it) {
        if ((*it)->IsAResolver()) {
            // Only care about providers
            continue;
        }

        if ((*it)->GetState() < PLUGIN_STATE_STARTING_UP) {
            // Plugin is not active
            continue;
        }

        GUID  generatedReports[MAX_PLUGIN_REPORT_TYPES];
        DWORD numGeneratedReports;
        (*it)->GetGeneratedReportInfo(generatedReports, &numGeneratedReports);

        for (DWORD i = 0; i < numGeneratedReports; i++) {
            if (pRes->CanSupportReportType(&generatedReports[i])) {
                // This resolver could be service by provider (*it).
                return TRUE;
            } 
        }
    }

    // No suitable providers found that can generate reports for this resolver.
    return FALSE;
}


//*****************************************************************************
//  Routines for starting up and stopping plugins 
//*****************************************************************************

// Tries to start a particular plugin for the given reportType.  This function
// verifies that the plugin can support the report type and if it can, will
// determine how to proceeed based on the current plugin state.
BOOL pluginMgr_t::StartPluginIfPossible(plugin_t *pPlugin, const GUID *pReportType) {
    DEBUGCHK(g_pLocLock->IsLocked());
    DEBUGCHK(g_serviceState == SERVICE_STATE_ON);

    BOOL ret;

    // If plugin can't even support requested type, then we're done
    if (! pPlugin->CanGenerateReportType(pReportType))
        return FALSE;

    PLUGIN_STATE reportState = pPlugin->GetReportTypeState(pReportType);

    // If this is a resolver, check its state for particular type
    // against unavailable first.  Resolver as a whole may be on
    // but this report may already have been tried but marked unavailable.
    if (pPlugin->IsAResolver() && (reportState == PLUGIN_STATE_UNAVAILABLE))
        return FALSE;

    // The plugin can generate this report type.  How we proceed is 
    // based on the current state the plugin is in.
    switch (pPlugin->GetState()) {
    case PLUGIN_STATE_ON:
    {
        // If plugin is on already, then we're where we want to be.

        // Explicitly (possibly redundant) register for this report in case 
        // this is a resolver, since resolver may be on for another report but
        // not this one.
        pPlugin->RequestReportGeneration(pReportType);
        ret = TRUE;
    }
    break;
    
    case PLUGIN_STATE_STARTING_UP:
    {
        // If plugin starting up, then we'll use it while realizing
        // it may be some time before it's ready. 
        pPlugin->RequestReportGeneration(pReportType);
        ret = TRUE;
    }
    break;

    case PLUGIN_STATE_SHUTTING_DOWN:
    {
        // If plugin was in middle of shutdown, just let it know
        // that we want to restart right away.  We'll have to let the
        // shutdown complete before we can do anything else, so just indicate this for now.
        pPlugin->RequestReportGeneration(pReportType);
        pPlugin->SetImmediateRestart();
        ret = TRUE;
    }
    break;

    case PLUGIN_STATE_ERROR:
    {
        // Plugin is in non-recoverable state.  Nothing we can do.
        ret = FALSE;
    }
    break;

    case PLUGIN_STATE_UNAVAILABLE:
    {
        // Plugin is temporarily unavailable (i.e. GPS device is inside)
        // then we can't start it.  When the device becomes available again,
        // our retry logic will stop any lower-prio plugins automatically.
        pPlugin->RequestReportGeneration(pReportType);
        ret = FALSE;
    }
    break;

    case PLUGIN_STATE_OFF:
    {
        // Plugin is off but we want it on.
        if (pPlugin->IsAProvider()) {
            // For provider case, need to spin up a thread to call
            // ProviderGetLocation export
            provider_t *pProv = (provider_t*)pPlugin;

            if (ERROR_SUCCESS != pProv->ScheduleWorkerIfNeeded())
                return FALSE;

            pProv->RequestReportGeneration(pReportType);
            pProv->StartingUp();
            ret = TRUE;
        }
        else {
            // For a resolver, first we need to try and start providers
            // that can generate reports that this resolver can resolve
            resolver_t *pRes = (resolver_t*)pPlugin;
            pRes->RequestReportGeneration(pReportType);
            ret = StartDependentProviderIfNeeded(pReportType,pRes);
        }
    }
    break;

    default:
    {
        // Should not be able to get here.
        DEBUGCHK(0);
    }
    }

    // Returning TRUE indicates that we're going to use this plugin
    // to generate the report.  Notify the reports collector to this fact if
    // there has been a state change
    if (ret && (reportState != pPlugin->GetReportTypeState(pReportType))) {
        reportCol_t *pRC = FindRC(pReportType);
        pRC->PluginStateChange();
    }

    return ret;
}

// Either an app is explicitly requesting pReportType in this call, or
// we're trying to start up a new plugin because the current one generating
// this report has become unavailable.  

// minStartOrder indicates which plugin in the list we should start with. 
// When attempting to restart after a plugin went unavailable, this saves 
// us from rescanning over higher priority plugins that are known to be unavailable.
DWORD pluginMgr_t::StartPluginForReportType(const GUID *pReportType, DWORD minStartOrder) {
    DEBUGCHK(g_pLocLock->IsLocked());
    DEBUGCHK(g_serviceState == SERVICE_STATE_ON);

    pluginListIter_t it    = GetPluginOfStartOrder(minStartOrder);
    pluginListIter_t itEnd = m_pluginList.end();

    // Iterate through each plugin, in priority order, and try to start
    // it for this report type.
    for (; it != itEnd; ++it) {
        if (StartPluginIfPossible(*it,pReportType))
            return ERROR_SUCCESS;
    }

    return ERROR_DEV_NOT_EXIST;
}

// When a plugin fails (either UNAVAILABLE or ERROR), attempt to start
// the next plugin in line to generate any registered reports
void pluginMgr_t::StartNextPluginsOnFailure(plugin_t *pPlugin) {
    DEBUGCHK(g_pLocLock->IsLocked());
    DEBUGCHK(g_serviceState == SERVICE_STATE_ON);

    GUID generatedReports[MAX_PLUGIN_REPORT_TYPES];
    DWORD numGeneratedReports;
    pPlugin->GetGeneratedReportInfo(generatedReports,&numGeneratedReports);

    DWORD minStartOrder = pPlugin->GetStartOrder() + 1;
    DWORD i;

    DEBUGMSG(ZONE_PLUGIN,(L"LOC: Plugin <%s> is no longer available.  Trying to start next plugins in line\r\n",
             pPlugin->GetName()));

    // Try to restart next possible plugin for any report types that this 
    // plugin can generate that were explicitly requested by an APPLICATION
    // (via LocationRegisterForReport).  Restarting based on RESOLVER dependancy
    // logic is NOT handled in this function, but elsewhere.

    for (i = 0; i < numGeneratedReports; i++) {
        // Indicate to reports collectors the fact that current plugin is failing.
        reportCol_t *pRC = FindRC(&generatedReports[i]);
        pRC->PluginStateChange();

        if (! IsPluginNeededForReport(pPlugin,&generatedReports[i],TRUE))
            continue;

        StartPluginForReportType(&generatedReports[i], minStartOrder);
    }      
}


// The specified resolver has been requested to register for a certain report.
// First we (may) need to start required providers for this resolver.
// This function iterates through each provider on the system and attempts
// to start it for each supportedType the resolver has, stopping if it 
// has a successful startup.
BOOL pluginMgr_t::StartDependentProviderIfNeeded(const GUID *pReportType, resolver_t *pRes) {
#ifdef DEBUG
    PLUGIN_STATE pluginState = pRes->GetState();
    // If resolver is shutting down or in a fatal error, then we should
    // not be trying to startup new providers for it in the first place. 
    DEBUGCHK((pluginState != PLUGIN_STATE_SHUTTING_DOWN) || (pluginState != PLUGIN_STATE_ERROR));
#endif

    DWORD i;

    DEBUGMSG(ZONE_PLUGIN,(L"LOC: Attempting to start dependent provider for resolver <%s>\r\n",
             pRes->GetName()));

    // The resolver has not registered at all for any report types.
    // Do so now.
    pluginListIter_t it    = m_pluginList.begin();
    pluginListIter_t itEnd = m_pluginList.end();

    GUID  supportedReports[MAX_PLUGIN_REPORT_TYPES];
    DWORD numSupportedReports;
    pRes->GetSupportedReportInfo(supportedReports,&numSupportedReports);

    // 
    // Register resolver with each possible report type
    //
    for (i = 0; i < numSupportedReports; i++) {
        // Do this because ultimately the resolver doesn't care where the report
        // comes from, it just needs to be tied into this.
        reportCol_t *pRC = FindRC(&supportedReports[i]);
        if (NULL == pRC) {
            // Just because a resolver supports a report doesn't mean anything 
            // on the system generates it (and hence no reports collector)
            continue; 
        }
        pRC->RegisterResolverForReport(pRes);
    }

    //
    // Visit each plugin in priority order.  From there, determine for
    // each report that the resolver generates whether it can be done
    //

    for (; it != itEnd; ++it) {
        // We can only generate provider generated reports, not those
        // from other resolvers, so filter these out now.
        if ((*it)->IsAResolver())
            continue;

        // Foreach report type, try to start it for this plugin.
        for (i = 0; i < numSupportedReports; i++) {
            if (StartPluginIfPossible(*it,&supportedReports[i])) {
                if (pReportType)
                    pRes->RequestReportGeneration(pReportType);
                return TRUE;
            }
        }
    }

    // Unable to find a provider that was on or could be started up
    // to service this resolver.
    if (pReportType)
        pRes->ReportTypeUnavailable(pReportType);

    DEBUGMSG(ZONE_PLUGIN,(L"LOC: Failed to start dependent provider for resolver <%s>\r\n",
             pRes->GetName()));

    return FALSE;
}


// Stops all plugins registered for given report if possible.  This can
// be either when an application deregisters for a given event type, or when
// a higher priority plugin ((minStartOrder-1) element in list) becomes available
// and lower prio plugins should be stopped if possible.
void pluginMgr_t::StopPlugins(const GUID *pReportType, pluginList_t *pPluginsToStop, DWORD minStartOrder) {
    DEBUGCHK(g_pLocLock->IsLocked());
    DEBUGCHK(g_serviceState == SERVICE_STATE_ON);

    // Do not begin with start of list, but rather minStartOrder as passed by caller.
    // This is for instance if a higher prio plugin has become available
    // for a report, it would indicate to stop all plugins generating
    // same report type only if they had a lower priority.
    pluginListIter_t it    = GetPluginOfStartOrder(minStartOrder);
    pluginListIter_t itEnd = m_pluginList.end();
    // Whether we should alert associated RC of a state change
    BOOL             stateChange = FALSE;

    // For each plugin, indicate (if needed) that it no longer needs to generate
    // this report type and then determine if the plugin can be stopped
    // because nothing else requires it now.
    for (; it != itEnd; ++it) {
        if (! (*it)->CanGenerateReportType(pReportType)) {
            // If we can't generate this report type, no point continuing.
            continue;
        }

        PLUGIN_STATE pluginState = (*it)->GetState();
        if (pluginState < PLUGIN_STATE_UNAVAILABLE) {
            // If the plugin is off or shutting down or in fatal error,
            // then telling it to stop again is redundant.
            continue;
        }

        (*it)->CancelReportGeneration(pReportType);

        if (IsPluginNeeded((*it))) {
            // Some other app/resolver needs another report this plugin
            // can generate, so we can't stop it now.
            continue;
        }
            
        pPluginsToStop->push_back(*it);
        (*it)->StopPlugin();
        stateChange = TRUE;
    }

    if (stateChange) {
        reportCol_t *pRC = FindRC(pReportType);
        pRC->PluginStateChange();
    }
}

// Indicate to plugins with prio order greater than minStartOrder that
// they no longer need to generate this report.
void pluginMgr_t::StopPluginsForReportType(const GUID *pReportType, DWORD minStartOrder) {
    pluginList_t  pluginsToStop;

    StopPlugins(pReportType,&pluginsToStop,minStartOrder);

    pluginListIter_t it    = pluginsToStop.begin();
    pluginListIter_t itEnd = pluginsToStop.end();

    // While stopping plugins above, it's possible that we stopped a resolver
    // that itself had registered to receive other reports from providers.
    // Unregister these reports as well.
    for (; it != itEnd; ++it) {
        // Only resolvers add dependencies, so skip providers
        if ((*it)->IsAProvider())
            continue;

        resolver_t  *pRes   = (resolver_t*)((plugin_t*)(*it));
        StopDependentProvidersForResolver(pRes);
    }
}

// When a provider becomes available after having been unavailable, 
// this function will stop any plugins of lower priority that were started
// for it.  This will stop plugins for ALL report types the provider
// generates (not just the one it received a report for) since providers
// can either generate all report types or none.
void pluginMgr_t::StopPluginsOnProviderAvailable(provider_t *pProv) {
    DEBUGCHK(g_pLocLock->IsLocked());
    DEBUGCHK(pProv->GetState() == PLUGIN_STATE_ON);

    DEBUGMSG(ZONE_PROVIDER,(L"LOC: Stoping any lower priority plugins started now that provider <%s> is available\r\n",
             pProv->GetName()));

    GUID  generatedReports[MAX_PLUGIN_REPORT_TYPES];
    DWORD numGeneratedReports;

    pluginList_t pluginsToStop;
    pProv->GetGeneratedReportInfo(generatedReports,&numGeneratedReports);
    DWORD  minStartOrder = pProv->GetStartOrder() + 1;

    for (DWORD i = 0; i < numGeneratedReports; i++)
        StopPluginsForReportType(&generatedReports[i], minStartOrder);

    StopRedundantProvidersForResolvers(minStartOrder);
}

// When a provider becomes available again, it's possible that some additional
// providers are not needed anymore and that these providers were not detected
// during the initial StopPluginForReportType() calls.  See 
// IsProviderRedundantForResolvers comments for details on this scenario.

// This function runs through each lower priority plugin and determines
// if it needs to be stopped.
void pluginMgr_t::StopRedundantProvidersForResolvers(DWORD minStartOrder) {
    DEBUGCHK(g_pLocLock->IsLocked());

    pluginListIter_t it    = GetPluginOfStartOrder(minStartOrder);
    pluginListIter_t itEnd = m_pluginList.end();

    for (; it != itEnd; ++it) {
        if ((*it)->IsAResolver()) {
            // Only care about providers;
            continue;
        }

        if ((*it)->GetState() < PLUGIN_STATE_UNAVAILABLE) {
            // If the plugin is off or shutting down or in fatal error,
            // then telling it to stop again is redundant.
            continue;
        }

        if (IsPluginNeeded((*it))) {
            // Some other app/resolver needs another report this plugin
            // can generate, so we can't stop it now.
            continue;
        }

        DEBUGMSG(ZONE_PLUGIN,(L"LOC: Detecting provider <%s> is redundant since another provider will take care of dependent resolvers\r\n",
                              (*it)->GetName()));

        (*it)->StopPlugin();

        // Unlike StopPlugins(), no need to shutdown resolvers at this stage
        // since we know that provider going down is only being taken down
        // because there's another provider for its resolvers.
    }
}

// Stop providers that resolver started now that resolver is unavailable/error state/shutting down.
// We do this in order to save battery life.  For instance, assume that
// an Active Directory Resolver had an 802.11 Provider started to get
// nearby access points.  If the AD can no longer get data (or app deregisters for it), 
// then it's pointless to keep the 802.11 driver active to create reports no one
// will ever use.

void pluginMgr_t::StopDependentProvidersForResolver(resolver_t *pRes) {
    DEBUGCHK(g_pLocLock->IsLocked());
    DEBUGCHK(pRes->GetState() <= PLUGIN_STATE_UNAVAILABLE);

    pluginList_t  pluginsToStop;

    DEBUGMSG(ZONE_PLUGIN,(L"LOC: Resolver <%s> is unavailable.  Stopping any plugins it may have loaded\r\n",
             pRes->GetName()));

    DWORD numSupportedReports;
    GUID  supportedReports[MAX_PLUGIN_REPORT_TYPES];
    pRes->GetSupportedReportInfo(supportedReports,&numSupportedReports);

    // For each report type that the resolver can support, unregister
    // from RC (if it was) and then stop associated plugins.

    for (DWORD i = 0; i < numSupportedReports; i++) {
        reportCol_t *pRC = FindRC(&supportedReports[i]);
        if (NULL == pRC) {
            // Just because a resolver supports a particular report type
            // does not mean a report collector for it was created.  We only
            // create an RC when a plugin actually generates that report.
            continue;
        }

        if (ERROR_SUCCESS != pRC->UnRegisterResolverForReport(pRes)) {
            // If the resolver wasn't registered with the RC in 1st place, continue
            continue;
        }

        if (pRC->HasRegisteredAppsOrResolvers()) {
            // If the report type has other clients, we can't deregister it.
            continue; 
        }

        // Stop since this was the last client associated with the report
        StopPlugins(&supportedReports[i],&pluginsToStop,0);
    }
}


// When a provider becomes unavailable, it is possible that it had 
// resolvers depending on it to generate reports.  In this function,
// we will see if no other running providers generate this report and if they
// do not then we will try to start one.

// If it is not possible to start another plugin and hence a given resolver
// has no possibility of having a provider generate reports for it,
// then indicate to the resolver that is de facto unavailable (even
// if resolver could still in theory operate).

// For instance, suppose an Active Directory resolver relies on 802.11
// reports that the 802.11 provider generates.  If the 802.11 provider
// becomes unavailable, then we should mark the AD Resolver as unavailable
// if and only if there are no other providers that could generate a report 
// the AD supports. 
void pluginMgr_t::ProcessProviderFailureOnResolvers(provider_t *pProv) {
    DEBUGCHK(g_pLocLock->IsLocked());
    DEBUGCHK((pProv->GetState() == PLUGIN_STATE_UNAVAILABLE) ||
             (pProv->GetState() == PLUGIN_STATE_ERROR));

    pluginListIter_t it    = m_pluginList.begin();
    pluginListIter_t itEnd = m_pluginList.end();

    DEBUGMSG(ZONE_PLUGIN,(L"LOC: Determining which resolvers depended on unavailable provider <%s> and trying to restart\r\n",
             pProv->GetName()));

    // Iterate through each resolver, detecting if this provider was the last
    // one that could generate reports.

    for (; it != itEnd; ++it) {
        if ((*it)->IsAProvider()) {
            // Providers generate reports independent of one another.  If 
            // one provider is unavailable it has no effect on other providers.
            continue;
        }

        if ((*it)->GetState() < PLUGIN_STATE_UNAVAILABLE) {
            // The resolver was not actively trying to determine new 
            // reports, so continue.
            continue;
        }

        resolver_t *pRes = (resolver_t*)((plugin_t*)(*it));
        BOOL        noProvsAvailable = TRUE;

        if (! pRes->HasProvidersAvailable()) {
            // Resolver is already in state where nothing is available.
            // Don't bother recomputing all this.
            continue;
        }

        DWORD numSupportedReports;
        GUID  supportedReports[MAX_PLUGIN_REPORT_TYPES];
        pRes->GetSupportedReportInfo(supportedReports,&numSupportedReports);

        // For each report type that the resolver can support, determine
        // if there is any *provider* that can generate the given type.
        // Even one match is enough to indicate that the resolver is still valid.
        for (DWORD i = 0; i < numSupportedReports; i++) {
            PLUGIN_STATE repState = GetPluginInfoForReport(&supportedReports[i],NULL,TRUE);
            if (repState >= PLUGIN_STATE_STARTING_UP) {
                noProvsAvailable = FALSE;
                break;
            }
        }

        if (noProvsAvailable) {
            // First, try to startup a new provider to do this work.
            if (! StartDependentProviderIfNeeded(NULL,pRes)) {
                // No providers can support this resolver even after this attempt.
                // Indicate this to the resolver so it can take appropriate action.
                pRes->NoProvidersAvailable();
                // Try to startup next plugin for any reports that were registered
                // for this resolver.  Even if resolver would've been available
                // otherwise, it's dead in the water without a provider.
                StartNextPluginsOnFailure(pRes);
            }
        }
    }
}


//*****************************************************************************
//  Callback implementations
//*****************************************************************************

// A new provider report is available via NewProviderReport.  Alert
// registered apps & resolvers, possibly stop lower priority plugins that 
// were started up if this plugin was previously unavailable.
DWORD pluginMgr_t::NewProviderReportInd(HANDLE provContext, LOCATION_REPORT *pNewReport) {
    DEBUGMSG(ZONE_PROVIDER,(L"LOC: Received NewProviderReport callback.  Context=<0x%08x>, report=<0x%08x>\r\n",
                          provContext,pNewReport));

    if (pNewReport == NULL) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: NewProviderReportInd passed NULL report, invalid\r\n"));
        return ERROR_INVALID_PARAMETER;
    }

    provider_t *pProv;
    reportCol_t *pRC;
    LOCATION_REPORT_BASE *pNewReportBase = (LOCATION_REPORT_BASE *)pNewReport;
    GUID                 *pNewReportType = &pNewReportBase->type;

    DWORD err;

    if (pNewReportBase->size < sizeof(LOCATION_REPORT_BASE)) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: NewProviderReportInd passed in report that is too small\r\n"));
        return ERROR_INVALID_PARAMETER;
    }

    g_pLocLock->Lock();

    if (g_serviceState != SERVICE_STATE_ON) {
        // This indicates that the location service is shutting down.  The last
        // thing we want to do is start stopping & starting plugins, because
        // otherwise the shutdown logic becomes much harder.
        DEBUGMSG(ZONE_RESOLVER,(L"LOC: Ignoring NewProviderReport since loc service is not on\r\n"));
        err = ERROR_SERVICE_NOT_ACTIVE;
        goto done;
    }

    pProv = FindProvider(provContext);
    if (!pProv) {
        err = ERROR_INVALID_PARAMETER;
        goto done;
    }

    PLUGIN_STATE oldState = pProv->GetState();
    if (oldState < PLUGIN_STATE_UNAVAILABLE) {
        // In this case, plugin isn't registered anymore so throw out
        // any reports it may still be generating.
        DEBUGMSG(ZONE_PROVIDER,(L"LOC: Ignoring Provider Report for <%s> because state is off/error\r\n",
                                  pProv->GetName()));
        err = ERROR_SERVICE_NOT_ACTIVE;
        goto done;
    }

    // Indicate to reports collector monitoring this report type
    if (! pProv->CanGenerateReportType(pNewReportType)) {
        // For instance, if a lat/long only provider generates an 802.11 address or
        // if we're not currently registered for the report, then ignore.
        DEBUGMSG(ZONE_PROVIDER,(L"LOC: Provider report for <%s> is not a report the provider is configured to generate\r\n",
                                  pProv->GetName()));
        err = ERROR_INVALID_PARAMETER;
        goto done;
    }

    pRC = FindRC(pNewReportType);
    
    pProv->ReportsAvailable();
    if (pRC->NewReport(pProv,pNewReport)) {
        // NewReport returns TRUE if the RC will use this report 
        // (i.e. not a higher prio plugin available).
        if (! pProv->HasReceivedReport(pNewReportType)) {
            // This is the first time on this ProviderGetLocation cycle 
            // that this report has been generated OR it's coming back
            // from unavailable state.  Indicate this state change to applications
            // only the first time we transition to ON.
            pRC->PluginStateChange();
        }
    }

    pProv->IndicateReceivedReport(pNewReportType);

    if ((oldState == PLUGIN_STATE_UNAVAILABLE) || (oldState == PLUGIN_STATE_STARTING_UP)) {
        // In this case, the plugin was temporarily unavailable and
        // lower priority plugins may have been started.  Stop them.
        StopPluginsOnProviderAvailable(pProv);
    }

    err = ERROR_SUCCESS;
done:
    g_pLocLock->Unlock();
    return err;
}

// Provider calls ProviderUnavailable callback.  Move provider to 
// the unavailable state.
DWORD pluginMgr_t::ProviderUnavailableInd(HANDLE provContext) {
    DEBUGMSG(ZONE_PROVIDER,(L"LOC: Received ProviderUnavailable callback.  Context=<0x%08x>\r\n",
                             provContext));

    provider_t *pProv;
    DWORD err;

    g_pLocLock->Lock();

    if (g_serviceState != SERVICE_STATE_ON) {
        DEBUGMSG(ZONE_RESOLVER,(L"LOC: Ignoring ProviderUnavailable since loc service is not on\r\n"));
        err = ERROR_SERVICE_NOT_ACTIVE;
        goto done;
    }

    pProv = FindProvider(provContext);
    if (!pProv) {
        err = ERROR_INVALID_PARAMETER;
        goto done;
    }

    PLUGIN_STATE oldState = pProv->GetState();
    if (oldState <= PLUGIN_STATE_UNAVAILABLE) {
        // In this case, provider has already been put into the unavailable
        // state.  The provider is basically calling unavaibale a 2nd time
        // (before giving new data) and hence we ignore this report.
        DEBUGMSG(ZONE_PROVIDER,(L"LOC: Ignoring Provider Unavialable for <%s> because state is off/error/unavail\r\n",
                                  pProv->GetName()));
        err = ERROR_SERVICE_NOT_ACTIVE;
        goto done;
    }

    pProv->ReportsUnavailable();

    DEBUGMSG(ZONE_PROVIDER,(L"LOC: Marking provider <%s> as unavailable\r\n",pProv->GetName()));

    // It doesn't matter if plugin was on or just starting up at this point -
    // either way we need to try to start next plugin in line.  
    StartNextPluginsOnFailure(pProv);
    // If there were resolvers that were relying on this provider to 
    // generate reports for them, need to either startup new providers
    // or if that fails, disable the resolvers.
    ProcessProviderFailureOnResolvers(pProv);

    err = ERROR_SUCCESS;
done:
    g_pLocLock->Unlock();
    return err;
}

// Resolver cannot resolve a particular report.
DWORD pluginMgr_t::NewResolverReportInd(HANDLE resContext, LOCATION_REPORT *pNewReport) {
    DEBUGMSG(ZONE_RESOLVER,(L"LOC: Received NewResolverReport callback.  Context=<0x%08x>, report=<0x%08x>\r\n",
                             resContext,pNewReport));

    DWORD err;

    if (pNewReport == NULL) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: NewResolverReport passed NULL report, invalid\r\n"));
        return ERROR_SUCCESS;
    }

    LOCATION_REPORT_BASE *pNewReportBase = (LOCATION_REPORT_BASE*)pNewReport;

    if (pNewReportBase->size < sizeof(LOCATION_REPORT_BASE)) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: NewResolverReport passed in report that is too small\r\n"));
        return ERROR_INVALID_PARAMETER;
    }

    g_pLocLock->Lock();

    if (g_serviceState != SERVICE_STATE_ON) {
        DEBUGMSG(ZONE_RESOLVER,(L"LOC: Ignoring NewResolverReport since loc service is not on\r\n"));
        err = ERROR_SERVICE_NOT_ACTIVE;
        goto done;
    }

    resolver_t *pRes;
    reportCol_t *pRC;
    GUID                 *pNewReportType = &pNewReportBase->type;

    pRes = FindResolver(resContext);
    if (!pRes) {
        err = ERROR_INVALID_PARAMETER;
        goto done;
    }

    if (! pRes->CanGenerateReportType(pNewReportType)) {
        DEBUGMSG(ZONE_RESOLVER,(L"LOC: Resolver report for <%s> is not a report the resolver is requested to generate\r\n",
                 pRes->GetName()));
        err = ERROR_INVALID_PARAMETER;
        goto done;
    }

    PLUGIN_STATE oldStatePlugin = pRes->GetState();
    PLUGIN_STATE oldStateReport = pRes->GetReportTypeState(pNewReportType);

    if (oldStateReport < PLUGIN_STATE_UNAVAILABLE) {
        DEBUGMSG(ZONE_RESOLVER,(L"LOC: Resolver <%s> is not requested to generate this report type, ignoring report\r\n",
                 pRes->GetName()));
        err = ERROR_SERVICE_NOT_ACTIVE;
        goto done;
    }

    if (! pRes->HasProvidersAvailable()) {
        // If the resolver has all plugins inactive during its ResolverGetLocation
        // call, then just throw out reports the resolver generates in the interim
        // In theory we could still push these up to the app but it would greatly
        // complicate the state maintenence for little gain.
        DEBUGMSG(ZONE_RESOLVER,(L"LOC: Resolver <%s> does not have providers available, ignoring report\r\n",
                 pRes->GetName()));
        err = ERROR_SERVICE_NOT_ACTIVE;
        goto done;
    }

    // Indicate to reports collector monitoring this report type
    pRC = FindRC(pNewReportType);

    // Regardless of how we process report, always have resolver
    // remember the fact it has received it so that when ResolverGetLocation
    // callback returns resolver will not mark it as unavailable.
    pRes->IndicateReceivedReport(pNewReportType);

    if (oldStateReport == PLUGIN_STATE_ON) {
        // If the plugin for this type is already in the ON state, then 
        // no further action is required.
        pRC->NewReport(pRes,pNewReport);
        err = ERROR_SUCCESS;
        goto done;
    }

    if (oldStatePlugin == PLUGIN_STATE_UNAVAILABLE) {
        // When the resolver entered the unavailable state, it stopped 
        // all the plugins that could register reports for it in order
        // to save them from generating reports for nothing.  Now that
        // the resolver has returned, we need to start at least 1 up again.
        if (! StartDependentProviderIfNeeded(pNewReportType,pRes)) {
            // If we were unable to start providers, then we're dead in the water
            // again.  Take no further action
            err = ERROR_SERVICE_NOT_ACTIVE;
            goto done;
        }
    }

    pRes->ReportTypeAvailable(pNewReportType);
    if (pRC->NewReport(pRes,pNewReport))
        pRC->PluginStateChange();

    if (oldStateReport == PLUGIN_STATE_UNAVAILABLE) {
        // In this case, the plugin was temporarily unavailable for this 
        // report type (not necessarily others) and lower priority plugins may have 
        // been started for this type.  Stop them.
        StopPluginsForReportType(pNewReportType, pRes->GetStartOrder() + 1);
    }

    err = ERROR_SUCCESS;
done:
    g_pLocLock->Unlock();
    return err;
}

// Called when a resolver cannot generate the resolved report.  Note
// that resolvers do not call this function directly, but instead the LF
// will have it called when a resolver returns from a resolution without
// resolving a particular report type
DWORD pluginMgr_t::ResolverReportTypeUnavailable(resolver_t *pRes, const GUID *pReportType) {
    DEBUGMSG(ZONE_RESOLVER,(L"LOC: ResolverUnavailable for resolver DLL <%s>",
                            pRes->GetName()));

    // LF itself (not a 3rd party plugin) is calling this, so we can assume
    // that reportType is reasonably well formatted.
    DEBUGCHK(g_pLocLock->IsLocked());
    DEBUGCHK(pReportType);
    DEBUGCHK(g_serviceState == SERVICE_STATE_ON);
    DEBUGCHK(FindResolver(pRes));

    DWORD err;

    PLUGIN_STATE oldReportState = pRes->GetReportTypeState(pReportType);
    if (oldReportState <= PLUGIN_STATE_UNAVAILABLE) {
        // In this case, resolver has already been put into the unavailable
        // state.  The resolver is basically calling unavaibale a 2nd time
        // (before giving new data) and hence we ignore this report.
        DEBUGMSG(ZONE_RESOLVER,(L"LOC: Ignoring Resolver Unavialable for <%s> because state is off/error/unavail\r\n",
                                  pRes->GetName()));
        err = ERROR_SERVICE_NOT_ACTIVE;
        goto done;
    }

    pRes->ReportTypeUnavailable(pReportType);

    // Determines whether any report type in resolver is still active
    PLUGIN_STATE currentState = pRes->GetState(); 

    // Current state goes to unavilable if NO reports this resolver generates
    // can be resolved at this point
    if (currentState == PLUGIN_STATE_UNAVAILABLE) {
        // It doesn't matter if plugin was on or just starting up at this point -
        // either way we need to try to start next plugin in line.
        StartNextPluginsOnFailure(pRes);

        // Stop providers if they were only spun up for this report
        StopDependentProvidersForResolver(pRes);
    }

    err = ERROR_SUCCESS;
done:
    return err;
}

//*****************************************************************************
//  Routines that process Location Framework API calls.
//*****************************************************************************


// An application has indicated via LocationRegisterForReport that it wants
// plugin(s) needed to generate this report started.
DWORD pluginMgr_t::RegisterAppForReport(const GUID *pReportType, locOpenHandle_t *pLocHandle,  
                                        HANDLE hNewLocationReport, HANDLE hStateChangeEvent)
{
    DEBUGCHK(g_pLocLock->IsLocked());

    reportCol_t *pRC = FindRC(pReportType);
    if (! pRC) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: No plugin supports report type <" SVSUTIL_GUID_FORMAT_W L">\r\n",
                             SVSUTIL_PGUID_ELEMENTS(pReportType)));
        return ERROR_DEV_NOT_EXIST;
    }

    PLUGIN_STATE reportState = GetPluginInfoForReport(pReportType, NULL,FALSE);
    if (reportState == PLUGIN_STATE_ERROR) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: All plugins that support report type <" SVSUTIL_GUID_FORMAT_W L"> are in PLUGIN_ERROR state\r\n",
                             SVSUTIL_PGUID_ELEMENTS(pReportType)));
        return ERROR_DEV_NOT_EXIST;
    }

    if (pRC->HasRegisteredAppsOrResolvers()) {
        // Another application has already registered for this report type
        // The plugin manager has already started appropriate plugins to process
        // this, so just note that there's a new client requesting reports.
        return pRC->RegisterAppForReport(pLocHandle, hNewLocationReport, hStateChangeEvent);
    }

    // Otherwise, this is first instance of anything registering for this report type.

    // Register now, even though we don't know if we'll succeed, so that if we hit
    // an OOM here we don't bother starting up plugin manager
    DWORD err = pRC->RegisterAppForReport(pLocHandle, hNewLocationReport, hStateChangeEvent);
    if (err != ERROR_SUCCESS)
        return err;

    err = StartPluginForReportType(pReportType,0);
    if (err == ERROR_SUCCESS)
        return ERROR_SUCCESS;

    // StartPluginForReportType could fail if for instance a resolver 
    // was only plugin that supported this type, but the resolver
    // couldn't startup any providers that it required.

    DEBUGMSG(ZONE_ERROR,(L"LOC: No plugins were available to service report type <" SVSUTIL_GUID_FORMAT_W L">",
             SVSUTIL_PGUID_ELEMENTS(pReportType)));

    // Cleanup
    pRC->UnRegisterAppForReport(pLocHandle);
    return ERROR_DEV_NOT_EXIST;
}


// An application no longer wishes to the particular report type to be
// active.  Deregister and potentially close plugins that were generating 
// the report.
DWORD pluginMgr_t::UnRegisterAppForReport(locOpenHandle_t *pLocHandle, const GUID *pReportType) {
    reportCol_t *pRC = FindRC(pReportType);
    if (! pRC) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: No plugin supports report type <" SVSUTIL_GUID_FORMAT_W L">\r\n",
                                SVSUTIL_PGUID_ELEMENTS(pReportType)));
        return ERROR_DEV_NOT_EXIST;
    }

    DWORD err = pRC->UnRegisterAppForReport(pLocHandle);
    if (err != ERROR_SUCCESS)
        return err;

    if (pRC->HasRegisteredAppsOrResolvers()) {
        // If this report has other applications or resolvers
        // registered on it, then we take no further action
        return ERROR_SUCCESS; 
    }

    // No longer require this report type, so tell plugins generating it to stop.
    StopPluginsForReportType(pReportType, 0);
    return ERROR_SUCCESS;
}


// Fill in fields of LOCATION_SERVICE_STATE
void pluginMgr_t::GetServiceState(LOCATION_SERVICE_STATE *pServiceState) {
    DEBUGCHK(g_pLocLock->IsLocked());

    pServiceState->serviceState       = g_serviceState;
    pServiceState->numLoadedProviders = pServiceState->numLoadedResolvers =
    pServiceState->numActiveProviders = pServiceState->numActiveResolvers = 0;

    // Iterate through each plugin, incrementing count and examining state.
    pluginListIter_t it    = m_pluginList.begin();
    pluginListIter_t itEnd = m_pluginList.end();
        
    for (; it != itEnd; ++it) {
        PLUGIN_STATE pluginState = (*it)->GetState();
        BOOL isActive = (pluginState >= PLUGIN_STATE_STARTING_UP);
        if ((*it)->IsAProvider()) {
            pServiceState->numLoadedProviders++;
            if (isActive)
                pServiceState->numActiveProviders++;
        }
        else {
            DEBUGCHK((*it)->IsAResolver());
            pServiceState->numLoadedResolvers++;
            if (isActive)
                pServiceState->numActiveResolvers++;
        }
    }
}


// Retrieves information about all providers loaded on the system,
// as the result of LocationGetProvidersInfo call
DWORD pluginMgr_t::GetProvidersInfo(PROVIDER_INFORMATION *pProviders, DWORD *pcbBuffer) {
    DEBUGCHK(g_pLocLock->IsLocked());

    //
    // Determine if passed buffer is big enough
    //
    LOCATION_SERVICE_STATE locServiceState;
    GetServiceState(&locServiceState);

    DWORD numBytesToWrite = locServiceState.numLoadedProviders*sizeof(PROVIDER_INFORMATION);

    if ((*pcbBuffer) < numBytesToWrite) {
#ifdef DEBUG
        if ((*pcbBuffer) != 0) { 
            // if app passed 0 bytes, they're expecting an error so don't DEBUGMSG
            DEBUGMSG(ZONE_ERROR,(L"LOC: LocationGetProvidersInfo passed <%d> byte buffer size but needs <%d> bytes\r\n",
                     *pcbBuffer,numBytesToWrite));
        }
#endif
        *pcbBuffer = numBytesToWrite;
        return ERROR_INSUFFICIENT_BUFFER;
    }
    *pcbBuffer = numBytesToWrite;

    //
    // Actually retrieve the data into the buffer
    //
    pluginListIter_t it    = m_pluginList.begin();
    pluginListIter_t itEnd = m_pluginList.end();

    for (; it != itEnd; ++it) {
        if ((*it)->IsAResolver())
            continue;

        provider_t *pProv = (provider_t*)((plugin_t*)(*it));

        pProv->GetProviderInfo(pProviders);
        pProviders++;
    }

    return ERROR_SUCCESS;
}

// Retrieves information about all resolvers loaded on the system,
// as the result of LocationGetResolversInfo call
DWORD pluginMgr_t::GetResolversInfo(RESOLVER_INFORMATION *pResolvers, DWORD *pcbBuffer) {
    DEBUGCHK(g_pLocLock->IsLocked());

    //
    // Determine if passed buffer is big enough
    //
    LOCATION_SERVICE_STATE locServiceState;
    GetServiceState(&locServiceState);

    DWORD numBytesToWrite = locServiceState.numLoadedResolvers*sizeof(RESOLVER_INFORMATION);

    if ((*pcbBuffer) < numBytesToWrite) {
#ifdef DEBUG
        if ((*pcbBuffer) != 0) { 
            // if app passed 0 bytes, they're expecting an error so don't DEBUGMSG
            DEBUGMSG(ZONE_ERROR,(L"LOC: LocationGetResolversInfo passed <%d> byte buffer size but needs <%d> bytes\r\n",
                     *pcbBuffer,numBytesToWrite));
        }
#endif
        *pcbBuffer = numBytesToWrite;
        return ERROR_INSUFFICIENT_BUFFER;
    }
    *pcbBuffer = numBytesToWrite;

    //
    // Actually retrieve the data into the buffer
    //
    pluginListIter_t it    = m_pluginList.begin();
    pluginListIter_t itEnd = m_pluginList.end();

    for (; it != itEnd; ++it) {
        if ((*it)->IsAProvider())
            continue;

        resolver_t *pRes = (resolver_t*)((plugin_t*)(*it));

        pRes->GetResolverInfo(pResolvers);
        pResolvers++;
    }

    return ERROR_SUCCESS;
}

// Determines state of the plugin that can generate the given report,
// if the system supports it.  Since multiple plugins may be able to support
// the same report type, we return the "highest" state (based on int value
// of the PLUGIN_STATE enum).  This means if highest prio plugin is in ERROR
// state, next is UNAVAILABLE, 3rd is starting up, and 4th is OFF, we 
// chose the starting up.
PLUGIN_STATE pluginMgr_t::GetPluginInfoForReport(const GUID *pReportType, GUID *pPluginGuid, BOOL providersOnly) {
    DEBUGCHK(g_pLocLock->IsLocked());

    PLUGIN_STATE retState = PLUGIN_STATE_NOT_SUPPORTED;
    const GUID  *pCurPlugin = NULL;

    pluginListIter_t it    = m_pluginList.begin();
    pluginListIter_t itEnd = m_pluginList.end();

    for (; it != itEnd; ++it) {
        if (! (*it)->CanGenerateReportType(pReportType))
            continue;

        if (providersOnly && (*it)->IsAResolver())
            continue;

        PLUGIN_STATE pluginState = (*it)->GetReportTypeState(pReportType);
        if (pluginState > retState) {
            pCurPlugin = (*it)->GetGuid();
            retState = pluginState;
        }
    }

    if (pPluginGuid) {
        if (pCurPlugin)
            memcpy(pPluginGuid,pCurPlugin,sizeof(GUID));
        else
            memset(pPluginGuid,0,sizeof(GUID));
    }

    return retState;
}



#ifdef DEBUG
// Verify that every item on the pluginList is unique.  At various
// times LF creates such lists and assumes that the list has only 
// unique items on it.  Failing this check indicates a serious bug in the LF
// that could cause plugins to be called multiple times for the same event.
void DbgVerifyPluginListItemsUnique(pluginList_t *pPluginList) {
    pluginListIter_t it    = pPluginList->begin();
    pluginListIter_t itEnd = pPluginList->end();

    for (; it != itEnd; ++it) {
        // For each item in list, run through list again and verify
        // that it is not present
        pluginListIter_t itTrav = it;
        ++itTrav;
        for (; itTrav != itEnd; ++itTrav)
            DEBUGCHK(itTrav != it);
    }
}
#endif

