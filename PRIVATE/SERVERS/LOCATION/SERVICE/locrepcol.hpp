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

class locOpenHandle_t;

//
// Maintain information about each application that registered for an RC 
// via the LocationRegisterForReport API.
//
class registeredApp_t {
private:
    // Location handle that owns this LocationRegisterForReport data
    locOpenHandle_t *m_pLocHandle;
    // Handle to signal when a new location report is available
    ce::auto_handle  m_hNewLocationReport;
    // Handle to signal when a plugin causes a state change (coming up, going down, ...)
    ce::auto_handle  m_hStateChange;
    // Was object created successfully?
    BOOL             m_isInited;
public:
    registeredApp_t(locOpenHandle_t *pLocHandle, HANDLE hNewLoc, HANDLE hNewState);
    ~registeredApp_t();

    BOOL IsLocHandle(locOpenHandle_t *pLocHandle) { return m_pLocHandle==pLocHandle; }
    BOOL IsInited(void) { return m_isInited; }

    void IndicateNewLocationReport();
    void IndicateStateChange();
};


//
// A report collector (RC) object.  Each report type the system can
// generate has a unique RC object allocated for it.  The RC interfaces
// between applications and the Plugin Manager.  It also notifies 
// resolvers registered to receive reports from a particular provider 
// as those reports arrive.
//

class reportCol_t {
private:
    // Type of report (Lat/Long (LOCATION_LATLONG_GUID), 802.11, etc...)
    // that this RC holds reports for
    GUID             m_reportType;
    // Last location report that a plugin indicated to us
    reportSmartPtr_t m_report;
    // Tracks applications that receive events based on this report type
    ce::list<registeredApp_t*> m_registeredApplications;
    // Tracks resolvers that receive notifications based on this report type
    ce::list<resolver_t*>      m_registeredResolvers;
    // All resolvers that can theoretically receive reports from this 
    // report collector, registered or otherwise.
    ce::list<resolver_t*>      m_resolvers;
    // Tracks the current plugin that is generating reports for us.  This
    // is used in how to filter incoming reports.  For instance if a
    // lower prio plugin generates a report of this type, then we ignore it.
    plugin_t                  *m_pluginGenerator;

    resolver_t *FindRegisteredResolver(resolver_t *pRes);

#ifdef DEBUG
    // String value of GUID m_reportType, to simplify DEBUGOUT formatting.
    WCHAR       m_reportTypeString[SVSUTIL_GUID_STR_LENGTH+1];
#endif

public:
    reportCol_t(GUID *pReportType);
    ~reportCol_t();

    const GUID *GetReportType(void) { return &m_reportType; }
    registeredApp_t *FindRegisteredApp(locOpenHandle_t *pLocHandle);

    reportSmartPtr_t GetReport(void);
    BOOL HasNewReport(FILETIME *pCreationTime);

    //
    // Control resolvers associated with this RC
    //
    DWORD AddToResolversList(resolver_t *pRes);
    DWORD UnRegisterResolverForReport(resolver_t *pRes);
    DWORD RegisterResolverForReport(resolver_t *pRes);

    //
    // Called in response to API calls
    //
    DWORD RegisterAppForReport(locOpenHandle_t *pLocHandle, HANDLE hNewLocationReport,
                               HANDLE hStateChangeEvent);

    DWORD UnRegisterAppForReport(locOpenHandle_t *pLocHandle);


    DWORD GetReport(DWORD maximumAge, LOCATION_REPORT *pLocationReport,
                    DWORD *pcbLocationReport);

    //
    // Determine whether particular report type is required by some
    // app or resolver (or both) on the system
    //
    BOOL HasRegisteredAppsOrResolvers(void);
    BOOL HasRegisteredApps(void);
    BOOL HasRegisteredResolvers(void);

    //
    // Called in response to callbacks from the plugins
    //
    BOOL NewReport(plugin_t *pPlugin, LOCATION_REPORT *pNewReport);
    void PluginStateChange(void);
};


typedef ce::smart_ptr<reportCol_t>           reportColSmartPtr_t;
typedef ce::list<reportColSmartPtr_t>        reportColList_t;
typedef reportColList_t::iterator            reportColListIter_t;
typedef ce::list<registeredApp_t*>::iterator registeredAppIter_t;
typedef ce::list<resolver_t*>::iterator      resolverIter_t;


