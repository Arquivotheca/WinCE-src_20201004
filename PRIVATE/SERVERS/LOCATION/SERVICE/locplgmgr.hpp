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


// 
//  The Plugin Manager (PM) acts as the glue that holds the LF together.
//
//  The PM's main responsibilities are:
//  *  Process most API requests and forward on appropriately
//  *  Load all plugins and determine when to stop & start them
//  *  Process plugin callbacks and push them on to appropriate plugins
//  *  Load all report collectors, push new plugin reports to them


class pluginMgr_t {
#ifdef LOC_WHITEBOX_TESTING
    // Allow our white-box tests direct access to these variables.
public:
#else
    // For real development, follow C++ rules of data encapsulation
private:
#endif

    // List of all plugins loaded in the system.  The highest priority
    // plugin is first in list, next highest is next, etc..
    pluginList_t    m_pluginList;
    // List of reports collectors in the system.
    reportColList_t m_reportColList;
    // Was initialization of PM successful?
    BOOL            m_initSuccess;

    // 
    //  Initialization of plugin manager
    // 
    BOOL InsertPluginOrdered(pluginSmartPtr_t pPlugin);
    BOOL InitProviders(CReg *pBaseReg);
    BOOL InitResolvers(CReg *pBaseReg);
    BOOL AssignPluginOrder(void);
    BOOL InitReportCollectors(void);

    //
    //  Determine relationships between various plugins
    //
    BOOL IsPluginNeededForReport(plugin_t *pPlugin, const GUID *pReportType, BOOL checkRegAppsOnly);
    BOOL IsPluginNeeded(plugin_t *pPlugin);
    BOOL IsProviderRedundantForResolvers(provider_t *pPlugin, const GUID *pReportType);
    BOOL ExistsHigherPrioPluginForReport(plugin_t *pPlugin, const GUID *pReportType);
    BOOL ExistsHigherPrioActiveProviderForResolver(resolver_t *pRes, DWORD minStartOrder);
    pluginListIter_t GetPluginOfStartOrder(DWORD minStartOrder);

    //
    //  Startup plugins
    //
    BOOL  StartPluginIfPossible(plugin_t *pPlugin, const GUID *pReportType);
    DWORD StartPluginForReportType(const GUID *pReportType, DWORD minStartOrder);
    BOOL  StartDependentProviderIfNeeded(const GUID *pReportType, resolver_t *pRes);
    void  StartNextPluginsOnFailure(plugin_t *pPlugin);

    //
    //  Stop plugins
    // 
    void  StopPlugins(const GUID *pReportType, pluginList_t *pPluginsToStop, DWORD minStartOrder);
    void  StopPluginsForReportType(const GUID *pReportType, DWORD minStartOrder);
    void  StopPluginsOnProviderAvailable(provider_t *pProv);
    void  StopRedundantProvidersForResolvers(DWORD minStartOrder);
    void  StopDependentProvidersForResolver(resolver_t *pRes);
    void  ProcessProviderFailureOnResolvers(provider_t *pProv);

public:
    pluginMgr_t();
    ~pluginMgr_t();

    BOOL IsPlgMgrInited(void);

    //
    //  Helper functions
    //
    plugin_t *FindPlugin(const GUID *pPluginGuid);
    plugin_t *FindPlugin(HANDLE pluginContext);

    provider_t *FindProvider(HANDLE provContext) { 
        return (provider_t *)FindPlugin(provContext); 
    }

    resolver_t *FindResolver(HANDLE resContext) { 
        return (resolver_t*)FindPlugin(resContext); 
    }

    reportCol_t *FindRC(const GUID *pGUID);

    DWORD ResolverReportTypeUnavailable(resolver_t *pRes, const GUID *pReportType);
    void  UnRegisterEventsFromHandle(locOpenHandle_t *pLocHandle);
    void  FatalPluginError(plugin_t *pPlugin);

    //
    //  Process plugin callbacks
    //
    DWORD NewProviderReportInd(HANDLE provContext, LOCATION_REPORT *pNewReport);
    DWORD ProviderUnavailableInd(HANDLE provContext);
    DWORD NewResolverReportInd(HANDLE resContext, LOCATION_REPORT *pNewReport);

    //
    //  Process API calls
    // 
    // Implement LocationGetServiceState
    void  GetServiceState(LOCATION_SERVICE_STATE *pServiceState);

    // Implement LocationRegisterForReport
    DWORD RegisterAppForReport(const GUID *pReportType, locOpenHandle_t *pLocHandle,  
                               HANDLE hNewLocationReport, HANDLE hStateChangeEvent);

    // Implement LocationUnRegisterForReport
    DWORD UnRegisterAppForReport(locOpenHandle_t *pLocHandle, const GUID *pReportType);

    // Implement LocationGetReport
    DWORD GetReport(const GUID *pReportType, DWORD maximumAge, 
                    LOCATION_REPORT *pLocationReport, DWORD *pcbLocationReport);

    // Implement LocationGetProvidersInfo
    DWORD GetProvidersInfo(PROVIDER_INFORMATION *pProviders, DWORD *pcbBuffer);

    // Implement LocationGetResolversInfo
    DWORD GetResolversInfo(RESOLVER_INFORMATION *pResolvers, DWORD *pcbBuffer);

    // Implements LocationGetPluginInfoForReport
    PLUGIN_STATE GetPluginInfoForReport(const GUID *pReportType, GUID *pPluginGuid, BOOL providersOnly);

};

#ifdef DEBUG
void DbgVerifyPluginListItemsUnique(pluginList_t *pPluginList);
#endif


extern pluginMgr_t *g_pPluginMgr;

