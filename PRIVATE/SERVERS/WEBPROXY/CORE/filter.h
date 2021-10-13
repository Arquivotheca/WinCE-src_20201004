//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include "global.h"
#include "list.hxx"

typedef DWORD (*PFN_ProxyFilterHttpRequest) (PPROXY_HTTP_REQUEST pRequest);
typedef DWORD (*PFN_ProxyInitializeFilter) (PPROXY_HTTP_INFORMATION pInfo);
typedef DWORD (*PFN_ProxyUninitializeFilter) (void);
typedef DWORD (*PFN_ProxyNotifyAddrChange) (void);
typedef DWORD (*PFN_ProxySignalFilter) (DWORD dwSignal);

typedef struct _FILTER_DATA {
    HINSTANCE hlibFilter;
    PFN_ProxyFilterHttpRequest ProxyFilterHttpRequest;
    PFN_ProxyInitializeFilter ProxyInitializeFilter;
    PFN_ProxyUninitializeFilter ProxyUninitializeFilter;
    PFN_ProxyNotifyAddrChange ProxyNotifyAddrChange;
    PFN_ProxySignalFilter ProxySignalFilter;
} FILTER_DATA;


class CProxyFilter {
public:
    void LoadFilters(void);
    DWORD AddFilter(WCHAR* wszFilter);
    DWORD RemoveAllFilters(void);
    DWORD FilterRequest(PPROXY_HTTP_REQUEST pRequest);
    DWORD NotifyAddrChange(void);
    DWORD Signal(DWORD dwSignal);
    int GetFilterCount(void);
    
private:
    ce::list<FILTER_DATA> m_FilterList;

};

extern CProxyFilter* g_pProxyFilter;

