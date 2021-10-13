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

