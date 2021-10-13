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

// Abstract: Declares classes related to wrapping plugin DLLs.


class plugin_t;

typedef ce::smart_ptr<plugin_t>      pluginSmartPtr_t; 
typedef ce::vector<pluginSmartPtr_t> pluginList_t;
typedef pluginList_t::iterator       pluginListIter_t;
typedef ce::smart_ptr<BYTE>          reportSmartPtr_t;

//
// Implements base plugin wrapper for common functionality between
// providers and resolvers.  Plugins are dlls (potentially 3rd party)
// that actually determine the device's physical location.
//

class plugin_t {
protected:
    //
    //  Plugin information that is set at plugin init (either from reading
    //  registry or immediately after reading registry).
    //
    // Is this object a provider or resolver?
    BOOL m_isAProvider; 
    // Name of the DLL that implements plugin
    ce::wstring m_dllName;
    // Friendly name of this plugin
    ce::wstring m_friendlyName;
    // Unique GUID for this DLL
    GUID m_guid;
    // Preference of start order with DLL.  Lower #'s mean plugin is preferred
    DWORD m_preference;
    // Plugin specific settings
    DWORD m_pluginFlags;
    // Version of the plugin
    DWORD m_version;
    // Period of time to wait on a failure, in milliseconds.
    DWORD m_retryOnFailure;
	// Period of time to wait before intial location query to allow hardware time to initialize
    DWORD m_initialWaitTime;
    // GUID report types that this plugin generates.  e.g. LOCATION_LATLONG_GUID
    // specifies the plugin is capable of generating lat/long reports.
    GUID  m_generatedReportTypes[MAX_PLUGIN_REPORT_TYPES];
    // Number of m_generatedReportTypes
    DWORD m_numGeneratedReportTypes;
    // HINSTANCE of the DLL
    ce::auto_hlibrary m_dllHInstance;

    //
    //  Plugin information that changes at runtime.
    //
    // Current plugin state
    PLUGIN_STATE m_pluginState;
    // Last time that the plugin received a new report
    FILETIME     m_lastUpdate;
    // Used to track the thread in thread pool for calling (Plugin)GetLocation
    SVSCookie    m_workerThrdCookie;
    // Indicates whether loadup succeeded and hence whether we should call DeInit() or not
    BOOL         m_loadSuccess;
    // Number of *active* calls blocked in the plugin's IOCTL routines.
    DWORD        m_numActiveIoctlCallers;
    // Indicate that even though plugin is shutting down, we want to
    // restart it once it's completed shutdown sequence because another app needs it.
    BOOL         m_immediateRestart;
    

    // Keeps track if we have received a report of particular type.  Maps 
    // with m_generatedReportTypes ordered entries.
    // For resolvers, this only tracks the current resolution cycle.  For providers,
    // tracks the current state during ProviderGetLocation call.
    BOOL         m_receivedReport[MAX_PLUGIN_REPORT_TYPES];
    
    // Order that this plugin should be started.  This is relative
    // to all other plugins on the system, where a plugin with lower
    // startOrder is started before a plugin with a higher order.
    // Note that this corresponds to the plugins index in the pluginMgr_t::m_pluginList
    DWORD        m_startOrder;

    BOOL LoadPlugin(void);

    DWORD FindGeneratedReportIndex(const GUID *pReportType);

#ifdef DEBUG
    void DbgPrintSettings(void);
#endif

    void ReportFatalError();

    void WaitOnOpenIoctlCalls(void);
    void WaitOnRunningGetLocThread(void);
       
public:
    plugin_t(CReg *pRegPlugin, const WCHAR *keyName);
    virtual ~plugin_t() { ; }

#ifdef LOC_WHITEBOX_TESTING
    // For white box tests, allow plugin_t objects to be created directly 
    // (without calling into the registry to get config options)
    // The implementation for this constructor is in the WB tests themselves, not LF.
    typedef struct _pluginSettings_t pluginSettings_t;
    plugin_t(pluginSettings_t *pPluginSettings);
#endif

    // Accessors
    BOOL  IsAProvider(void)     { return m_isAProvider;  }
    BOOL  IsAResolver(void)     { return !m_isAProvider; }
    DWORD GetPreference(void)   { return m_preference;   }
    const WCHAR *GetName(void)  { return m_dllName;      }
    PLUGIN_STATE GetState(void) { return m_pluginState;  }

    virtual PLUGIN_STATE GetReportTypeState(const GUID *pReportType) = 0;

    void IndicateReceivedReport(const GUID *pReportType);
    BOOL HasReceivedReport(const GUID *pReportType);

    BOOL IsGuid(const GUID *pPluginGuid) { return (0 == memcmp(&m_guid,pPluginGuid,sizeof(GUID))); }
    const GUID *GetGuid(void)   { return &m_guid; }

    void GetGeneratedReportInfo(GUID *pReportTypes, DWORD *numReportTypes);

    virtual void SetFatalError() = 0;

    void  SetStartOrder(DWORD startOrder) { m_startOrder = startOrder; }
    DWORD GetStartOrder(void)             { return m_startOrder; }

    virtual void RequestReportGeneration(const GUID *pReportType) = 0;
    virtual void CancelReportGeneration(const GUID *pReportType) = 0;

    void SetImmediateRestart(void);
    virtual void StopPlugin(void) = 0;

    BOOL  CanGenerateReportType(const GUID *pReportType);
    void  GetPluginInfo(PLUGIN_INFORMATION *pPluginInfo);

    virtual DWORD CallIoctlOpen(void) = 0;
    virtual DWORD CallIoctlCall(HANDLE h, DWORD dwCode, BYTE *pBufIn, DWORD cbIn, BYTE *pBufOut, DWORD *pcbOut) = 0;
    virtual DWORD CallIoctlClose(HANDLE h) = 0;
};

//
// Implements provider specific functionality.  Providers are capable of 
// generating location reports directly - i.e. a GPS device or 802.11 
// receiver can get their data directly from a device driver.
//

// Function types for provider DLL callback functions
typedef DWORD (WINAPI *PFN_PROVINITIALIZE)(PROVIDER_INFORMATION *pProvInfo);
typedef DWORD (WINAPI *PFN_PROVGETLOCATION)(PROVIDER_CONTROL_BLOCK *pProvControlBlock);
typedef DWORD (WINAPI *PFN_PROVSTOP)(void);
typedef DWORD (WINAPI *PFN_PROVUNINITIALIZE)(void);

typedef DWORD (WINAPI *PFN_PROVOPEN)(void);
typedef DWORD (WINAPI *PFN_PROVIOCTL)(HANDLE hProv, DWORD dwCode, BYTE *pBufIn, DWORD cbIn, BYTE *pBufOut, DWORD *pcbOut);
typedef DWORD (WINAPI *PFN_PROVCLOSE)(HANDLE h);

class provider_t : public plugin_t {
protected:
    // Period (in ms) with which to requery
    BOOL  m_pollInterval;
    // Provider specific settings
    DWORD m_providerFlags;
    // Provider DLL exported function pointers
    PFN_PROVINITIALIZE   m_pfnInitialize;
    PFN_PROVGETLOCATION  m_pfnGetLocation;
    PFN_PROVSTOP         m_pfnStop;
    PFN_PROVUNINITIALIZE m_pfnUnInitialize;
    PFN_PROVOPEN         m_pfnIoctlOpen;
    PFN_PROVIOCTL        m_pfnIoctlCall;
    PFN_PROVCLOSE        m_pfnIoctlClose;
        
    void InitProvider(CReg *pRegPlugin, const WCHAR *keyName);
    void FillPCB(PROVIDER_CONTROL_BLOCK *pPcb, PROVIDER_INFORMATION *pProvInfo);
    void SetState(PLUGIN_STATE newState);

    // Wrappers for calling the provider DLL itself
    // In practice the CallProvXXX functions will never over-ridden.
    // White box tests do do this to make testing easier so make these virtual.
    virtual DWORD CallProvInit(PROVIDER_INFORMATION *pProvInfo);
    virtual DWORD CallProvGetLocation(PROVIDER_CONTROL_BLOCK *pProvControlBlock);
    virtual DWORD CallProvStop(void);
    virtual DWORD CallProvUnInit(void);
    virtual DWORD CallProvIoctlOpen(void);
    virtual DWORD CallProvIoctlCall(HANDLE h, DWORD dwCode, BYTE *pBufIn, DWORD cbIn, BYTE *pBufOut, DWORD *pcbOut);
    virtual DWORD CallProvIoctlClose(HANDLE h);

public:
    provider_t(CReg *pRegPlugin, const WCHAR *keyName) : plugin_t(pRegPlugin,keyName)  {
        if (PLUGIN_STATE_ERROR == m_pluginState) // Verify plugin_t constructor was successful
            return;

        m_isAProvider = true;
        m_pluginState = PLUGIN_STATE_ERROR;
        InitProvider(pRegPlugin, keyName);
    }

#ifdef LOC_WHITEBOX_TESTING
    // For white box tests, allow provider_t objects to be created directly 
    // (without calling into the registry to get config options)
    // The implementation for this constructor is in the WB tests themselves, not LF.
    provider_t(pluginSettings_t *pPluginSettings) : plugin_t(pPluginSettings) { ; }
#endif

    virtual ~provider_t();

    DWORD ScheduleWorkerIfNeeded(void);

    void StopPlugin(void);

    void ReportsUnavailable(void);
    void ReportsAvailable(void);
    void StartingUp(void);

    void RequestReportGeneration(const GUID *pReportType);
    void CancelReportGeneration(const GUID *pReportType);

    void GetProviderInfo(PROVIDER_INFORMATION *pProvInfo);
    void GetProviderLocation(void);

    void SetFatalError();

    virtual DWORD CallIoctlOpen(void) {
        return CallProvIoctlOpen();
    }
    virtual DWORD CallIoctlCall(HANDLE h, DWORD dwCode, BYTE *pBufIn, DWORD cbIn, BYTE *pBufOut, DWORD *pcbOut) {
        return CallProvIoctlCall(h, dwCode, pBufIn, cbIn, pBufOut, pcbOut);
    }
    virtual DWORD CallIoctlClose(HANDLE h) {
        return CallProvIoctlClose(h);
    }

    PLUGIN_STATE GetReportTypeState(const GUID *pReportType);
};

//
// Implements resolver specific functionality.  Resolvers require reports
// generated by a provider in order to function.  For instance a Web Services
// resolver could be able to determine country/city/address based on current
// lat/long, but first a provider must give the resolver the lat/long to resolve.
//

// Function types for resolver DLL callback functions
typedef DWORD (WINAPI *PFN_RESINITIALIZE)(RESOLVER_INFORMATION *pResInfo);
typedef DWORD (WINAPI *PFN_RESGETLOCATION)(RESOLVER_CONTROL_BLOCK *pResControlBlock);
typedef DWORD (WINAPI *PFN_RESSTOP)(void);
typedef DWORD (WINAPI *PFN_RESUNINITIALIZE)(void);

typedef DWORD (WINAPI *PFN_RESOPEN)(void);
typedef DWORD (WINAPI *PFN_RESIOCTL)(HANDLE h, DWORD dwCode, BYTE *pBufIn, DWORD cbIn, BYTE *pBufOut, DWORD *pcbOut);
typedef DWORD (WINAPI *PFN_RESCLOSE)(HANDLE h);

// Forward declaration of reports collector class
class reportCol_t;

class resolver_t : public plugin_t {
protected:
    // Minimum period (in ms) with which to requery.
    DWORD m_minRequery;
    // Provider specific settings
    DWORD m_resolverFlags;
    // GUID report types that this resolver can support - i.e. reports
    // it is capable of resolving.  For instance, a resolver may be able to 
    // process lat/long reports (LOCATION_LATLONG_GUID).
    GUID  m_supportedReportTypes[MAX_PLUGIN_REPORT_TYPES];
    // Number of m_supportedReportTypes
    DWORD m_numSupportedReportTypes;

    // Last time (GetCurrentFT()) the resolver thread ran (regardless of success)
    __int64 m_lastResolution;

    // Which report types has this been requested to do lookups for.  
    // Maps with m_generatedReportTypes ordered entries.  A resolver
    // (unlike a provider) can have some report states be active while
    // others are unavailable or not even requested to be on.
    PLUGIN_STATE m_reportStates[MAX_PLUGIN_REPORT_TYPES];
    // Are there providers (in theory) capable of generating a report this
    // resolver supports.  Only mark as FALSE AFTER we try all providers
    // that could help this resolver and they all fail.
    BOOL         m_providersAvailable;

    // Has the GetResolverReport thread been called at least once?  This
    // determines for how long we delay on lookup for 1st time and whether 
    // we need a stop call or not.
    BOOL         m_getRepThrdCalledFirstTime;

    // Current provider that is generating reports for this resolver.  This
    // is used in case multiple provider generate a supported report type,
    // the plugin knows which one to use based on highest priority.
    provider_t   *m_providerGenerator;
    // Associated reports collector with current report to be resolved.
    reportCol_t  *m_reportCollector;

    // Resolver DLL exported function pointers
    PFN_RESINITIALIZE   m_pfnInitialize;
    PFN_RESGETLOCATION  m_pfnGetLocation;
    PFN_RESSTOP         m_pfnStop;
    PFN_RESUNINITIALIZE m_pfnUnInitialize;
    PFN_RESOPEN         m_pfnIoctlOpen;
    PFN_RESIOCTL        m_pfnIoctlCall;
    PFN_RESCLOSE        m_pfnIoctlClose;

    void  SetAllReportStates(PLUGIN_STATE newState);
    void  InitResolver(CReg *pRegPlugin, const WCHAR *keyName);
    void  FillRCB(RESOLVER_CONTROL_BLOCK *pRcb, RESOLVER_INFORMATION *pResInfo, reportSmartPtr_t &pProvReport);
    DWORD DetermineDelayUntilNextResolution(void);

    void SetState(const GUID *pReportType, PLUGIN_STATE newState);

    // Wrappers for calling the resolver DLL itself
    // In practice the CallResXXX functions will never over-ridden.
    // White box tests do do this to make testing easier so make these virtual.
    virtual DWORD CallResInit(RESOLVER_INFORMATION *pResInfo);
    virtual DWORD CallResGetLocation(RESOLVER_CONTROL_BLOCK *pResControlBlock);
    virtual DWORD CallResStop(void);
    virtual DWORD CallResUnInit(void);
    virtual DWORD CallResIoctlOpen(void);
    virtual DWORD CallResIoctlCall(HANDLE h, DWORD dwCode, BYTE *pBufIn, DWORD cbIn, BYTE *pBufOut, DWORD *pcbOut);
    virtual DWORD CallResIoctlClose(HANDLE h);

public:
    resolver_t(CReg *pRegPlugin, const WCHAR *keyName) : plugin_t(pRegPlugin,keyName) {
        if (PLUGIN_STATE_ERROR == m_pluginState) // Verify plugin_t constructor was successful
            return;

        m_isAProvider = false;
        m_pluginState = PLUGIN_STATE_ERROR;
        InitResolver(pRegPlugin, keyName);
    }

    // Returns which reportTypes the plugin can possibly generate
    void GetSupportedReportInfo(GUID *pReportTypes, DWORD *numReportTypes);

#ifdef LOC_WHITEBOX_TESTING
    // For white box tests, allow resolver_t objects to be created directly 
    // (without calling into the registry to get config options)
    // The implementation for this constructor is in the WB tests themselves, not LF.
    resolver_t(pluginSettings_t *pPluginSettings) : plugin_t(pPluginSettings) { ; }
#endif

    virtual ~resolver_t();

    void GetResolverLocation(void);
    void GetResolverInfo(RESOLVER_INFORMATION *pResInfo);
    BOOL CanSupportReportType(const GUID *pReportType);

    PLUGIN_STATE GetReportTypeState(const GUID *pReportType);

    void SetFatalError();

    void ReportTypeUnavailable(const GUID *pReportType);
    void ReportTypeAvailable(const GUID *pReportType);
    void RequestReportGeneration(const GUID *pReportType);
    void CancelReportGeneration(const GUID *pReportType);

    void NewReportFromProvider(provider_t *pProv, reportCol_t *pRC);
    void NoProvidersAvailable(void);
    BOOL HasProvidersAvailable(void) { return m_providersAvailable; }
        
    void ScheduleNextResolution(void);

    void StopPlugin(void);

    virtual DWORD CallIoctlOpen(void) {
        return CallResIoctlOpen();
    }
    virtual DWORD CallIoctlCall(HANDLE h, DWORD dwCode, BYTE *pBufIn, DWORD cbIn, BYTE *pBufOut, DWORD *pcbOut) {
        return CallResIoctlCall(h, dwCode, pBufIn, cbIn, pBufOut, pcbOut);
    }
    virtual DWORD CallIoctlClose(HANDLE h) {
        return CallResIoctlClose(h);
    }
};


#ifdef DEBUG
// Make sure that plugin is in a "workable" state
inline void DbgChkPluginStateValid(PLUGIN_STATE pluginState) {
    switch (pluginState) {
        case PLUGIN_STATE_ON:
        case PLUGIN_STATE_STARTING_UP:
        case PLUGIN_STATE_SHUTTING_DOWN:
        case PLUGIN_STATE_UNAVAILABLE:
        case PLUGIN_STATE_OFF:
            break;

        default:
            // PLUGIN_STATE_ERROR & PLUGIN_STATE_NOT_SUPPORTED are
            // not acceptable states under most conditions.
            DEBUGCHK(0);
    }
}
#else
inline void DbgChkPluginStateValid(plugin_t *pPlugin) {
    ;
}
#endif

