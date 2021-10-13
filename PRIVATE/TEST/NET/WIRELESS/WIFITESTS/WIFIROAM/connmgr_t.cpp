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
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// ----------------------------------------------------------------------------
//
// Implementation of the ConnMgr_t class.
//
// ----------------------------------------------------------------------------

#include "ConnMgr_t.hpp"

#include <Utils.hpp>

#include <assert.h>
#include <connmgr.h>
#include <Connmgr_status.h>
#include <inc/sync.hxx>

using namespace ce::qa;

// Connection Manager synchronization object:
static ce::critical_section s_ConnMgrLocker;

// ----------------------------------------------------------------------------
//
// Makes sure Connection Manager is ready.
//
static bool
IsConnMgrReady(void)
{
    static bool readyChecked = false;
    static bool connMgrReady = false;
    if (!readyChecked)
    {
        static HANDLE readyEvent = NULL;
        if (NULL == readyEvent)
        {
            readyEvent = ConnMgrApiReadyEvent();
            if (NULL == readyEvent)
            {
                LogError(TEXT("ConnMgrApiReadyEvent failed!"));
                readyChecked = true;
                connMgrReady = false;
                return false;
            }
        }
        if (WAIT_OBJECT_0 == WaitForSingleObject(readyEvent, 0))
        {
            CloseHandle(readyEvent);
            readyChecked = true;
            connMgrReady = true;
        }
    }
    return connMgrReady;
}

// ----------------------------------------------------------------------------
//
// Constructor.
//
ConnMgr_t::
ConnMgr_t(void)
{
    // Nothing to do
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
ConnMgr_t::
~ConnMgr_t(void)
{
    // Nothing to do
}

// ----------------------------------------------------------------------------
//
// Asks Connection Manager for the current connection status.
//
static HRESULT
QueryDetailedStatus(
    OUT CONNMGR_CONNECTION_DETAILED_STATUS **ppDetailedStatus,
    OUT DWORD                                *pDetailedBytes)
{
    HRESULT hr = S_OK;
    assert(NULL != ppDetailedStatus);
    assert(NULL !=  pDetailedBytes);

    // If the caller doesn't alloc memory and specify the size, we call
    // ConnMgrQueryDetailedStatus once to get the required size.
    if ((NULL == *ppDetailedStatus) && (0 == *pDetailedBytes))
    {
        hr = ConnMgrQueryDetailedStatus(*ppDetailedStatus, pDetailedBytes);
        if (hr != HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER))
        {
            LogError(TEXT("Failed to query detailed status: %s"), HRESULTErrorText(hr));
            LogStatus(TEXT("status query failed: 0x%x"), hr);
            return hr;
        }
    }

    // If there's any status to retrieve...
    if (0 != *pDetailedBytes)
    {
        // Alloc memory if it hasn't been allocated no matter the caller
        // passed in the size or we just got the size ourselves.
        if (NULL == *ppDetailedStatus)
        {
            void *pAlloc = LocalAlloc(0, *pDetailedBytes);
            if (NULL == pAlloc)
            {
                LogError(TEXT("Not enough memory to run the test"));
               *pDetailedBytes = 0;
                return E_OUTOFMEMORY;
            }
           *ppDetailedStatus = (CONNMGR_CONNECTION_DETAILED_STATUS *) pAlloc;
        }

        // Actually get the detailed status
        hr = ConnMgrQueryDetailedStatus(*ppDetailedStatus, pDetailedBytes);
        if (FAILED(hr))
        {
            LogError(TEXT("Failed to query detailed status: %s"), HRESULTErrorText(hr));
            LogStatus(TEXT("status query failed: 0x%x"), hr);
            LocalFree(*ppDetailedStatus);
           *ppDetailedStatus = NULL;
           *pDetailedBytes = 0;
        }
    }
    
    return hr;
}

// ----------------------------------------------------------------------------
//    
// A few methods to simplify parsing of the Connection Manager status.
//
inline bool
IsConnected(
    IN const CONNMGR_CONNECTION_DETAILED_STATUS *pDS)
{
    return ((pDS->dwParams & CONNMGRDETAILEDSTATUS_PARAM_CONNSTATUS)
         && (pDS->dwConnectionStatus == CONNMGR_STATUS_CONNECTED));
}

inline bool
IsDTPT(
    IN const CONNMGR_CONNECTION_DETAILED_STATUS *pDS)
{
    return ((pDS->dwParams & CONNMGRDETAILEDSTATUS_PARAM_TYPE)
         && (pDS->dwType == CM_CONNTYPE_PC)
         && (pDS->dwParams & CONNMGRDETAILEDSTATUS_PARAM_SUBTYPE)
         && (pDS->dwSubtype == CM_CONNSUBTYPE_PC_DESKTOPPASSTHROUGH));
}

inline bool
IsWiFi(
    IN const CONNMGR_CONNECTION_DETAILED_STATUS *pDS)
{
    return ((pDS->dwParams & CONNMGRDETAILEDSTATUS_PARAM_TYPE)
         && (pDS->dwType == CM_CONNTYPE_NIC)
         && (pDS->dwParams & CONNMGRDETAILEDSTATUS_PARAM_SUBTYPE)
      // && (pDS->dwSubtype == CM_CONNSUBTYPE_NIC_WIFI)
         && (pDS->dwParams & CONNMGRDETAILEDSTATUS_PARAM_ADAPTERNAME));
}

inline bool
IsCell(
    IN const CONNMGR_CONNECTION_DETAILED_STATUS *pDS)
{
    return ((pDS->dwParams & CONNMGRDETAILEDSTATUS_PARAM_TYPE)
         && (pDS->dwType == CM_CONNTYPE_CELLULAR)
         && (pDS->dwParams & CONNMGRDETAILEDSTATUS_PARAM_SUBTYPE)
         && (pDS->dwSubtype == CM_CONNSUBTYPE_CELLULAR_GPRS ||
             pDS->dwSubtype == CM_CONNSUBTYPE_CELLULAR_1XRTT)
         && (pDS->dwParams & CONNMGRDETAILEDSTATUS_PARAM_DESCRIPTION));
}

// ----------------------------------------------------------------------------
//
// Updates the current connection status information and selects
// the next connection to be used.
//
HRESULT
ConnMgr_t::
UpdateConnStatus(
    OUT ConnStatus_t *pStatus,
    OUT MemBuffer_t  *pConnection)
{
    HRESULT hr = S_OK;
    
    bool bConnected = false;
    TCHAR connDetails[MAX_PATH] = {0};
    
    assert(NULL != pStatus);
    assert(NULL != pConnection);

    // Lock and make sure the Connection Manager API is ready.
    ce::gate<ce::critical_section> connMgrLock(s_ConnMgrLocker);
    if (!IsConnMgrReady())
    {
        LogDebug(TEXT("Connection Manager is not ready yet."));
        return S_OK;
    }

    // Determine whether WiFi was connected previously.
    bool wifiWasConnected = (pStatus->connStatus & CONN_WIFI_LAN) != 0;
    
    // Initialize.
    pStatus->connStatus = 0;
    pConnection->Clear();

    // Get the detailed connection status from Connection Manager.
    CONNMGR_CONNECTION_DETAILED_STATUS* pDetailedStatus = NULL;
    DWORD                                detailedBytes = 0;
    hr = QueryDetailedStatus(&pDetailedStatus, &detailedBytes);
    if (FAILED(hr))
        goto Cleanup;

    // Check whether there are any active connections or any available
    // cellular connections.
    const CONNMGR_CONNECTION_DETAILED_STATUS* pDS;
    for (pDS = pDetailedStatus ; pDS != NULL; pDS = pDS->pNext)
    {
        if (IsConnected(pDS))
        {
            pStatus->connStatus |= CONN_CONNECTED;
        }
        else
        if (IsCell(pDS))
        {
            pStatus->connStatus |= CONN_CELL_RADIO_AVA;
            SafeCopy(connDetails, pDS->szDescription, COUNTOF(connDetails));

            // Store the cellular information even though it may be
            // supplanted later by an active connection.
            SafeCopy(pStatus->cellNetDetails, pDS->szDescription, COUNTOF(pStatus->cellNetDetails));

            if (pDS->dwSubtype == CM_CONNSUBTYPE_CELLULAR_GPRS)
                 SafeCopy(pStatus->cellNetType, TEXT("GPRS"),  COUNTOF(pStatus->cellNetType));
            else SafeCopy(pStatus->cellNetType, TEXT("1XRTT"), COUNTOF(pStatus->cellNetType));
        }
    }

    // If there were any active connections...
    if (pStatus->connStatus & CONN_CONNECTED)
    {
        // Check for a DTPT connection.
        for (pDS = pDetailedStatus ; pDS != NULL; pDS = pDS->pNext)
        {
            if (IsConnected(pDS) && IsDTPT(pDS))
            {
                bConnected = true;
                pStatus->connStatus |= CONN_DTPT;
                
                SafeCopy(connDetails, pDS->szDescription, COUNTOF(connDetails));
                
                LogDebug(TEXT("Found DTPT connection"));
             // LogStatus(TEXT("found dtpt connection"));
            }
        }

        // Check for a WiFi connection.
        // Only if DTPT is not available.
        if (!bConnected)
        {
            for (pDS = pDetailedStatus ; pDS != NULL; pDS = pDS->pNext)
            {
                if (IsConnected(pDS) && IsWiFi(pDS))
                {
                    // Get all the remaining WiFi connection information.
                    hr = pStatus->wifiConn.UpdateInterfaceInfo(pDS->szAdapterName);
                    if (SUCCEEDED(hr))
                        wifiWasConnected = true;
                    else
                    {
                        // If the information retrieval failed and WiFi was
                        // connected before through the same interface, leave
                        // the information the way it was. If it fails and we
                        // don't already have information about this interface,
                        // pretend WiFi wasn't connected.
                        if (wifiWasConnected
                         && _tcscmp(pDS->szAdapterName,
                                    pStatus->wifiConn.GetInterfaceName()) != 0)
                        {
                            wifiWasConnected = false;
                        }

                        // Don't return an error.
                        hr = S_OK;
                    }
                    
                    if (wifiWasConnected)
                    {
                        bConnected = true;
                        pStatus->connStatus |= CONN_WIFI_LAN;
                 
                        SafeCopy(connDetails, pStatus->wifiConn.GetInterfaceName(), COUNTOF(connDetails));
                    
                        LogDebug(TEXT("Found WiFi connection: %s"), connDetails);
                     // LogStatus(TEXT("found wifi %s"), connDetails);
                    }
                }
            }
        }

        // Check for a cellular connection.
        // Only if nothing else is available.
        if (!bConnected)
        {
            for (pDS = pDetailedStatus ; pDS != NULL; pDS = pDS->pNext)
            {
                if (IsConnected(pDS) && IsCell(pDS))
                {
                    bConnected = true;
                    pStatus->connStatus &= ~CONN_CELL_RADIO_AVA;
                    pStatus->connStatus |=  CONN_CELL_RADIO_CONN;
                    
                    SafeCopy(connDetails,             pDS->szDescription, COUNTOF(connDetails));
                    SafeCopy(pStatus->cellNetDetails, pDS->szDescription, COUNTOF(pStatus->cellNetDetails));

                    if (pDS->dwSubtype == CM_CONNSUBTYPE_CELLULAR_GPRS)
                         SafeCopy(pStatus->cellNetType, TEXT("GPRS"),  COUNTOF(pStatus->cellNetType));
                    else SafeCopy(pStatus->cellNetType, TEXT("1XRTT"), COUNTOF(pStatus->cellNetType));
                    
                    LogDebug(TEXT("Found Cellular connection: %s"), connDetails);
                 // LogStatus(TEXT("found cell %s"), connDetails); 
                }
            }
        }
    }

#define DIAGNOSE_CELL_STALL
#ifdef  DIAGNOSE_CELL_STALL
    if (pStatus->connStatus & (CONN_CONNECTED
                              |CONN_CELL_RADIO_AVA
                              |CONN_CELL_RADIO_CONN))
    {
        LogDebug(TEXT("ConnMgr detailed status: 0x%x"), pStatus->connStatus);
        pDS = pDetailedStatus;
        for (int sx = 0 ; pDS != NULL; pDS = pDS->pNext, sx += 1)
        {
            LogDebug(TEXT("  status[%d]:"), sx);
            if (pDS->dwParams & CONNMGRDETAILEDSTATUS_PARAM_TYPE)
                LogDebug(TEXT("    dwType             =  0x%lx"),
                        (unsigned long)pDS->dwType);
            if (pDS->dwParams & CONNMGRDETAILEDSTATUS_PARAM_SUBTYPE)
                LogDebug(TEXT("    dwSubtype          =  0x%lx"),
                        (unsigned long)pDS->dwSubtype);
            if (pDS->dwParams & CONNMGRDETAILEDSTATUS_PARAM_FLAGS)
                LogDebug(TEXT("    dwFlags            =  0x%lx"),
                        (unsigned long)pDS->dwFlags );
            if (pDS->dwParams & CONNMGRDETAILEDSTATUS_PARAM_SECURE)
                LogDebug(TEXT("    dwSecure           =  0x%lx"),
                        (unsigned long)pDS->dwSecure);
      //    if (pDS->dwParams & CONNMGRDETAILEDSTATUS_PARAM_DESTNET)
      //        LogDebug(TEXT("    guidDestNet        =  0x%lx"),
      //                (unsigned long)pDS->guidDestNet);
      //    if (pDS->dwParams & CONNMGRDETAILEDSTATUS_PARAM_SOURCENET)
      //        LogDebug(TEXT("    guidSourceNet      =  0x%lx"),
      //                (unsigned long)pDS->guidSourceNet); 
            if (pDS->dwParams & CONNMGRDETAILEDSTATUS_PARAM_DESCRIPTION)
                LogDebug(TEXT("    szDescription      = \"%s\""),
                         pDS->szDescription);
            if (pDS->dwParams & CONNMGRDETAILEDSTATUS_PARAM_ADAPTERNAME)
                LogDebug(TEXT("    szAdapterName      = \"%s\""),
                         pDS->szAdapterName);
            if (pDS->dwParams & CONNMGRDETAILEDSTATUS_PARAM_CONNSTATUS)
                LogDebug(TEXT("    dwConnectionStatus =  0x%lx"),
                        (unsigned long)pDS->dwConnectionStatus);
        }
    }
#endif

    // Map the connection (if any) to a Connection Manager GUID.
    if (pStatus->connStatus & (CONN_DTPT
                              |CONN_WIFI_LAN
                              |CONN_CELL_RADIO_CONN
                              |CONN_CELL_RADIO_AVA))
    {
        CONNMGR_CONNECTIONINFO info;
        memset(&info, 0, sizeof(CONNMGR_CONNECTIONINFO));
        info.cbSize = sizeof(CONNMGR_CONNECTIONINFO);
        info.dwParams = CONNMGR_PARAM_GUIDDESTNET;
        info.dwPriority = CONNMGR_PRIORITY_USERINTERACTIVE;
        info.bExclusive = false;
        info.bDisabled = false;
        
        hr = ConnMgrMapConRef(ConRefType_NAP, connDetails, &(info.guidDestNet));
        if (SUCCEEDED(hr))
        {
            LogDebug(TEXT("ConnMgrMapConRef mapped \"%s\""), connDetails);
            if (!pConnection->Assign((BYTE *)(&info), sizeof(info)))
            {
                hr = E_OUTOFMEMORY;
                LogError(TEXT("Can't allocate %u bytes for connection info"),
                         sizeof(info));
            }
        }
        else
        {
            LogError(TEXT("ConnMgrMapConRef for \"%s\" failed: %s"),
                     connDetails, HRESULTErrorText(hr));
            // Don't return an error.
            hr = S_OK;
        }
    }
    else
    {
        if (pStatus->connStatus & CONN_CONNECTED)
        {
            pStatus->connStatus = 0;
            LogError(TEXT("ConnMgr indicated un-recognized connection type"));
        }
        LogError(TEXT("Did not find connected WiFi, DTPT or GPRS/1xRTT"));
        LogStatus(TEXT("err: no connections detected"));
    }
    
Cleanup:

    if (pDetailedStatus)
    {
        LocalFree(pDetailedStatus);
    }

    return hr;
}

// ----------------------------------------------------------------------------
