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

#include <creg.hxx>
#include "filter.h"
#include "proxydbg.h"

CProxyFilter* g_pProxyFilter;

//
// Implementation of CProxyFilter class
//

DWORD CallFilterInit(PFN_ProxyInitializeFilter pfn, PPROXY_HTTP_INFORMATION pInfo)
{
    DWORD dwRetVal = ERROR_SUCCESS;

    __try {
        dwRetVal = pfn(pInfo);
    }
    __except (1) {
        IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Exception occured in ProxyInitializeFilter.\n")));
        dwRetVal = ERROR_INTERNAL_ERROR;
    }

    return dwRetVal;
}

DWORD CProxyFilter::AddFilter(WCHAR* wszFilter) 
{
    DWORD dwRetVal = 0;
    FILTER_DATA data;

    memset(&data, 0, sizeof(FILTER_DATA));

    data.hlibFilter = LoadLibrary(wszFilter);
    if (NULL == data.hlibFilter) {
        dwRetVal = GetLastError();
        IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error loading filter library: Error: %d\n"), dwRetVal));
        goto exit;
    }

    data.ProxyInitializeFilter = (PFN_ProxyInitializeFilter) GetProcAddress(data.hlibFilter, FILTER_INIT_PROC_SZ);
    if (! data.ProxyInitializeFilter) {
        IFDBG(DebugOut(ZONE_FILTER, _T("WebProxy: In Module %s could not GetProcAddress %s. Error: %d\n"), wszFilter, FILTER_INIT_PROC_SZ, GetLastError()));
    }
    data.ProxyUninitializeFilter = (PFN_ProxyUninitializeFilter) GetProcAddress(data.hlibFilter, FILTER_UNINIT_PROC_SZ);
    if (! data.ProxyUninitializeFilter) {
        IFDBG(DebugOut(ZONE_FILTER, _T("WebProxy: In Module %s could not GetProcAddress %s. Error: %d\n"), wszFilter, FILTER_UNINIT_PROC_SZ, GetLastError()));
    }
    data.ProxyFilterHttpRequest = (PFN_ProxyFilterHttpRequest) GetProcAddress(data.hlibFilter, FILTER_REQUEST_PROC_SZ);
    if (! data.ProxyFilterHttpRequest) {
        IFDBG(DebugOut(ZONE_FILTER, _T("WebProxy: In Module %s could not GetProcAddress %s. Error: %d\n"), wszFilter, FILTER_REQUEST_PROC_SZ, GetLastError()));
    }
    data.ProxyNotifyAddrChange = (PFN_ProxyNotifyAddrChange) GetProcAddress(data.hlibFilter, FILTER_ADDR_CHANGE_PROC_SZ);
    if (! data.ProxyNotifyAddrChange) {
        IFDBG(DebugOut(ZONE_FILTER, _T("WebProxy: In Module %s could not GetProcAddress %s. Error: %d\n"), wszFilter, FILTER_ADDR_CHANGE_PROC_SZ, GetLastError()));
    }
    data.ProxySignalFilter = (PFN_ProxySignalFilter) GetProcAddress(data.hlibFilter, FILTER_SIGNAL_FILTER_PROC_SZ);
    if (! data.ProxySignalFilter) {
        IFDBG(DebugOut(ZONE_FILTER, _T("WebProxy: In Module %s could not GetProcAddress %s. Error: %d\n"), wszFilter, FILTER_SIGNAL_FILTER_PROC_SZ, GetLastError()));
    }

    if (data.ProxyInitializeFilter) {
        PROXY_HTTP_INFORMATION info;
        info.dwSize = sizeof(info);
        info.dwProxyVersion = PROXY_VERSION;
        
        dwRetVal = CallFilterInit(data.ProxyInitializeFilter, &info);
        if (ERROR_SUCCESS != dwRetVal) {
            IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error initializing filter: Error: %d\n"), dwRetVal));
            goto exit;
        }
    }

    if (! m_FilterList.push_back(data)) {
        IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error inserting filter data in list.\n"), dwRetVal));
        goto exit;
    }
    
exit:
    return dwRetVal;
}

DWORD CallFilterUninit(PFN_ProxyUninitializeFilter pfn)
{
    DWORD dwRetVal = ERROR_SUCCESS;

    __try {
        dwRetVal = pfn();
    }
    __except (1) {
        IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Exception occured in ProxyUninitializeFilter.\n")));
        dwRetVal = ERROR_INTERNAL_ERROR;
    }

    return dwRetVal;
}

DWORD CProxyFilter::RemoveAllFilters(void)
{
    for (ce::list<FILTER_DATA>::iterator it = m_FilterList.begin(), itEnd = m_FilterList.end(); it != itEnd;) {
        if (it->ProxyUninitializeFilter) {
            CallFilterUninit(it->ProxyUninitializeFilter);
        }
        FreeLibrary(it->hlibFilter);
        m_FilterList.erase(it++);
    }

    return ERROR_SUCCESS;
}

DWORD CallFilterRequest(PFN_ProxyFilterHttpRequest pfn, PPROXY_HTTP_REQUEST pRequest)
{
    DWORD dwRetVal = ERROR_SUCCESS;
    
    __try {
        dwRetVal = pfn(pRequest);
    }
    __except (1) {
        IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Exception occured in ProxyFilterHttpRequest.\n")));
        dwRetVal = ERROR_INTERNAL_ERROR;
    }

    return dwRetVal;
}

DWORD CProxyFilter::FilterRequest(PPROXY_HTTP_REQUEST pRequest)
{
    DWORD dwRetVal = ERROR_SUCCESS;
    
    int cbOrigURLOut = pRequest->cbURLOut;
    
    for (ce::list<FILTER_DATA>::iterator it = m_FilterList.begin(), itEnd = m_FilterList.end(); it != itEnd; ++it) {
        if (it->ProxyFilterHttpRequest) {
            // Reset out paramters
            pRequest->szURLOut[0] = '\0';
            pRequest->cbURLOut = cbOrigURLOut;

            // Call into filter
            dwRetVal = CallFilterRequest(it->ProxyFilterHttpRequest, pRequest);
            if (ERROR_SUCCESS == dwRetVal) {
                if ((0 == pRequest->cbURLOut) || (0 != strcmp(pRequest->szURL, pRequest->szURLOut))) {
                    // Request was handled by first filter.  Break from the loop and do not pass the request to
                    // other filters.
                    break;
                }
            }
            else if (ERROR_INSUFFICIENT_BUFFER == dwRetVal) {
                break;                
            }
            else if (ERROR_NOT_AUTHENTICATED == dwRetVal) {
                break;
            }
            // If none of the above cases are true, just ignore error and continue to next filter
        }
    }
    
    return dwRetVal;
}

DWORD CallFilterNotifyAddrChange(PFN_ProxyNotifyAddrChange pfn)
{
    DWORD dwRetVal = ERROR_SUCCESS;
    
    __try {
        dwRetVal = pfn();
    }
    __except (1) {
        IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Exception occured in ProxyNotifyAddrChange.\n")));
        dwRetVal = ERROR_INTERNAL_ERROR;
    }

    return dwRetVal;
}

DWORD CProxyFilter::NotifyAddrChange(void)
{
    for (ce::list<FILTER_DATA>::iterator it = m_FilterList.begin(), itEnd = m_FilterList.end(); it != itEnd; ++it) {
        if (it->ProxyNotifyAddrChange) {
            CallFilterNotifyAddrChange(it->ProxyNotifyAddrChange);
        }
    }

    return ERROR_SUCCESS;
}

DWORD CallSignalFilter(PFN_ProxySignalFilter pfn, DWORD dwSignal)
{
    DWORD dwRetVal = ERROR_SUCCESS;

    __try {
        dwRetVal = pfn(dwSignal);
    }
    __except (1) {
        IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Exception occured in ProxySignalFilter.\n")));
        dwRetVal = ERROR_INTERNAL_ERROR;
    }

    return dwRetVal;
}

// BUGBUG: We might not want to signal all filters.  If so, need to have a mechanism for custom messages
DWORD CProxyFilter::Signal(DWORD dwSignal)
{
    for (ce::list<FILTER_DATA>::iterator it = m_FilterList.begin(), itEnd = m_FilterList.end(); it != itEnd; ++it) {
        if (it->ProxySignalFilter) {
            CallSignalFilter(it->ProxySignalFilter, dwSignal);
        }
    }

    return ERROR_SUCCESS;
}

int CProxyFilter::GetFilterCount(void)
{
    return m_FilterList.size();
}

void CProxyFilter::LoadFilters(void)
{
    CReg regFilters;
    WCHAR wszName[MAX_PATH];
    WCHAR wszFilter[MAX_PATH];

    if (regFilters.Open(HKEY_LOCAL_MACHINE, RK_FILTERS)) {
        BOOL fSuccess = TRUE;
        while (fSuccess) {
            fSuccess = regFilters.EnumValue(wszName, MAX_PATH, wszFilter, MAX_PATH);
            if (fSuccess) {
                DWORD dwErr = this->AddFilter(wszFilter);
#ifdef DEBUG
                if (ERROR_SUCCESS != dwErr) {
                    DEBUGMSG(ZONE_WARN, (_T("WebProxy: Warning -- Failed to add filter %s. Error:%d\n"), wszFilter, dwErr));
                }
                else {
                    DEBUGMSG(ZONE_FILTER, (_T("WebProxy: Added filter %s.\n"), wszFilter));
                }
#endif // DEBUG            
            }
        }        
    }
}

