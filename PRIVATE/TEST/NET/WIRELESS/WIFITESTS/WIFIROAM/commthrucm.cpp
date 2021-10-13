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
// Connection management for the WiFi Roaming test.
//
// ----------------------------------------------------------------------------

#include "WiFiRoam_t.hpp"

#include "Config.hpp"
#include "ConnMgr_t.hpp"
#include "NetlogAnalyser_t.hpp"

#include <Utils.hpp>

#include <APPool_t.hpp>
#include <connmgr.h>
#include <ndp_lib.h>

using namespace ce::qa;

// Connection Manager interface:
static ConnMgr_t s_ConnMgr;

// ----------------------------------------------------------------------------
//
// Detects a network change based on current and past status.
//
static bool
ChangeDetected(
    IN        long          RunDuration,
    IN  const ConnStatus_t *pOldStatus,
    OUT       ConnStatus_t *pNewStatus,
    OUT       DWORD        *pdwNumNetIfConnected)
{
    TCHAR connStatusString[MAX_PATH] = TEXT("");
    bool  fChanged = false;
    DWORD dwCurr, dwPast;

    // 1. current connected state != past connected state.
    dwCurr = pNewStatus->connStatus & CONN_CONNECTED;
    dwPast = pOldStatus->connStatus & CONN_CONNECTED;
    if (dwCurr != dwPast)
    {
        // Change detected.
        LogDebug(TEXT("Connection status changed: %hs ==> %hs"),
                 dwPast? "CONNECTED": "NOT CONNECTED",
                 dwCurr? "CONNECTED": "NOT CONNECTED");
        fChanged = true;
    }

    // 2. DTPT status changed.
    bool fDTPTNewConn = false;
    dwCurr = pNewStatus->connStatus & CONN_DTPT;
    dwPast = pOldStatus->connStatus & CONN_DTPT;
    if (dwCurr)
    {
        SafeAppend(connStatusString, TEXT("dtpt "), COUNTOF(connStatusString));
        ++(*pdwNumNetIfConnected);
    }

    if (dwCurr != dwPast)
    {
        // Change detected.
        LogDebug(TEXT("DTPT connection status changed: %hs ==> %hs"),
                 dwPast? "CONNECTED": "NOT CONNECTED",
                 dwCurr? "CONNECTED": "NOT CONNECTED");

        fChanged = true;
        if (dwCurr)
        {
            fDTPTNewConn = true;
            ++(pNewStatus->countDTPTConn);
        }
    }

    // 3. WiFi status changed.
    const WiFiConn_t *pOldWiFiConn = &(pOldStatus->wifiConn);
    const WiFiConn_t *pNewWiFiConn = &(pNewStatus->wifiConn);
    
    bool fWiFiNewConn = false;    // new WiFi connection
    bool fWiFiNewSSID = false;    // new Access Point SSID
    bool fWiFiNewRoam = false;    // new WiFi roaming
    
    dwCurr = pNewStatus->connStatus & CONN_WIFI_LAN;
    dwPast = pOldStatus->connStatus & CONN_WIFI_LAN;
    if (dwCurr)
    {
        SafeAppend(connStatusString, TEXT("wifi_lan "), COUNTOF(connStatusString));
        ++(*pdwNumNetIfConnected);
    }

    if (dwCurr != dwPast)
    {
        // Change detected.
        LogDebug(TEXT("WiFi connection status changed: %hs ==> %hs"),
                 dwPast? "CONNECTED": "NOT CONNECTED",
                 dwCurr? "CONNECTED": "NOT CONNECTED");
        fChanged = true;
        if (dwCurr)
        {
            fWiFiNewConn = true;
        }
    }

    // 4. If WiFi is connected, find out if WiFi has roamed to a new SSID.
    if (dwCurr && _tcscmp(pOldWiFiConn->GetAPSsid(),
                          pNewWiFiConn->GetAPSsid()) != 0)
    {
        // Change detected.
        LogDebug(TEXT("  WiFi roamed to new SSID: %s ==> %s"),
                 pOldWiFiConn->GetAPSsid(), 
                 pNewWiFiConn->GetAPSsid());
        fChanged = true;

        fWiFiNewSSID = true;

        // This is considered a new WiFi connection.
        fWiFiNewConn = true;
    }

    // 5. If it's the same SSID then find out if BSSID is different,
    //    i.e. whether WiFi has roamed within the same SSID.
    if (dwCurr && _tcscmp(pOldWiFiConn->GetAPMAC(),
                          pNewWiFiConn->GetAPMAC()) != 0)
    {
        // Change detected.
        LogDebug(TEXT("  WiFi roamed to new BSSID: %s ==> %s"),
                 pOldWiFiConn->GetAPMAC(), 
                 pNewWiFiConn->GetAPMAC());
        
        // We only care if the BSSID has changed if SSID is the same.
        if (!fWiFiNewSSID)
        {
            fChanged = true;
            
            if (_tcsicmp(pNewWiFiConn->GetAPSsid(), Config::GetOfficeSSID()) == 0)
            {
                // Roaming within Office SSID is NOT considered a new 
                // WiFi connection. If fWiFiNewConn is true, it must be
                // because we switched from disconnected to connected.
                // Possibilities:
                //    1. GPRS/CDMA connected while roaming within Office SSID.
                //    2. BSSID detection read incorrect/invalid MAC.
                if (!fWiFiNewConn)
                     fWiFiNewRoam = true;
                else LogStatus(TEXT("new wifi & roaming too"));
            }
            else
            {
                // Error.
                // Since it is not office AP, we are roaming over wrong SSID.
                LogError(TEXT("Roamed over wrong SSID: %s BSSID: %s"),
                         pNewWiFiConn->GetAPSsid(), 
                         pNewWiFiConn->GetAPMAC());
                LogStatus(TEXT("err: roamed ssid: %s bssid: %s"),
                          pNewWiFiConn->GetAPSsid(), 
                          pNewWiFiConn->GetAPMAC());
            }
        }
    }

    // If the BSSID hasn't changed, but the SSID has, we have probably
    // received a bogus SSID from CellMgr.
    else
    if (dwCurr && fWiFiNewSSID)
    {
        LogError(TEXT("Error: new SSID, but same BSSID"));
        LogError(TEXT("err: new ssid, same bssid"));
    }

    if (fWiFiNewConn)
    {
        if (_tcsicmp(pNewWiFiConn->GetAPSsid(), Config::GetHomeSSID()) == 0)
            ++(pNewStatus->countHomeWiFiConn);
        else
        if (_tcsicmp(pNewWiFiConn->GetAPSsid(), Config::GetOfficeSSID()) == 0)
            ++(pNewStatus->countOfficeWiFiConn);
        else
        if (_tcsicmp(pNewWiFiConn->GetAPSsid(), Config::GetHotspotSSID()) == 0)
            ++(pNewStatus->countHotSpotWiFiConn);
        else
        {
            // Error; We connected to an unknown SSID.
            ++(pNewStatus->failedUnknownAP);
            LogError(TEXT("Connected to unknown SSID: %s BSSID: %s"),
                     pNewWiFiConn->GetAPSsid(), pNewWiFiConn->GetAPMAC());
            LogStatus(TEXT("err: unknown ssid: %s bssid: %s"),
                      pNewWiFiConn->GetAPSsid(), pNewWiFiConn->GetAPMAC());
        }
    }
    else
    if (fWiFiNewRoam)
    {
        ++(pNewStatus->countOfficeWiFiRoam);
        SafeAppend(connStatusString, TEXT("roamed "), COUNTOF(connStatusString));
    }

    // 6. If Cell connection has changed.
    dwCurr = pNewStatus->connStatus & CONN_CELL_RADIO_CONN;
    dwPast = pOldStatus->connStatus & CONN_CELL_RADIO_CONN;

    if (dwCurr)
    {
        SafeAppend(connStatusString, TEXT("celluar "), COUNTOF(connStatusString));
        ++(*pdwNumNetIfConnected);
    }

    if (dwCurr != dwPast)
    {
        // Change detected.
        LogDebug(TEXT("CELL connection status changed: %hs ==> %hs"),
                 dwPast? "CONNECTED": "NOT CONNECTED",
                 dwCurr? "CONNECTED": "NOT CONNECTED");
        fChanged = true;

        if (dwCurr)
        {
            // We don't count the first network change.
            if (0 == pNewStatus->netChangesSeen)
                pNewStatus->lastCellConnectionTime = -9999; // cell's been up for awhile
            else
            {
                pNewStatus->lastCellConnectionTime = RunDuration;
                ++(pNewStatus->countCellConn);  
            }
        }
        else
        if (pNewStatus->lastCellConnectionClosed)
        {
            // Find out how long GPRS/CDMA connection was there.
            // That is find out if it is glitch in the cell connection.
            // That is while roaming over WiFi or having DTPT connected,
            // if things take too long then GPRS/CDMA may get connected
            // for very short period of time and then get brought down
            // once WiFi roaming or DTPT connection happens successfully.
            long cellActive = RunDuration - pNewStatus->lastCellConnectionTime;
            if (cellActive < MAX_CELL_GLITCH_TIME)
            {
                LogDebug(TEXT("Glitch in cell.")
                         TEXT(" Cell connection has been up for just %d sec"),
                         cellActive);
                ++(pNewStatus->failedCellGlitch);

                // Don't count these as cell connections.
                --(pNewStatus->countCellConn);
            }
        }
        
        pNewStatus->lastCellConnectionClosed = false;
    }

    // If the cell-connection hasn't changed and it's currently active
    // and WiFi or DTPT is connected, close it.
    else
    if ((dwCurr) // currently Cell (GPRS/CDMA) connected
     && (pNewStatus->connStatus & (CONN_DTPT | CONN_WIFI_LAN))) // and WiFi or DTPT connected
    {
        LogDebug(TEXT("Signaling ConnMgr to close cell connection"));
     // LogStatus(TEXT("closing cell connection"));
        pNewStatus->lastCellConnectionClosed = true;
    }

    // 7. If nothing is connected then find out if cell is available
    if ((!(pNewStatus->connStatus & CONN_CONNECTED)) 
       && (pNewStatus->connStatus & CONN_CELL_RADIO_AVA))
    {
        SafeAppend(connStatusString, TEXT("celluar ava"), COUNTOF(connStatusString));
    }

    // If necessary, capture the time i.e. our global second counter to 
    // mark the start of a new connection. We will use this to track how
    // long one connection lasts.
    if (fDTPTNewConn || fWiFiNewConn || fWiFiNewRoam)
    {
        ++(pNewStatus->netChangesSeen);
        pNewStatus->timeOfLastConnection = RunDuration;
        LogDebug(TEXT("Captured TimeOfLastConnection %ld"), 
                 pNewStatus->timeOfLastConnection);
    }

    LogDebug(TEXT("Time since Last change: %ld seconds"),
             RunDuration - pNewStatus->timeOfLastConnection);
    return fChanged;
}

// ----------------------------------------------------------------------------
//
// Called ever second or so by ThreadProc to ask Connection-Manager
// the current connection status.
//
HRESULT
WiFiRoam_t::
ConnectCMConnection(
    IN long RunDuration)
{
    HRESULT hr = S_OK;

    // Get the current connection status.
    ConnStatus_t newStatus = m_Status;
    MemBuffer_t  connInfo;
    hr = s_ConnMgr.UpdateConnStatus(&newStatus, &connInfo);
    if (FAILED(hr))
        return hr;

    LogDebug(TEXT("Got Connection id; Connection Status: 0x%x"),
             newStatus.connStatus);

    // Determine if and how the connection status has changed.
    DWORD numberNetIfsConnected = 0;
    if (!ChangeDetected(RunDuration, & m_Status, 
                                     &newStatus, &numberNetIfsConnected))
    {
        LogDebug(TEXT("No Change Detected"));

        // If so far we have not seen even a single change in network
        // connections then its OK to wait further.
        // Our operator might be taking time to launch the test.
        if (0 != newStatus.netChangesSeen)
        {
            // Once test is started at the most we can wait for
            // MAX_CONNECTION_TIME_ALLOWED.
            long idleSeconds = RunDuration - newStatus.timeOfLastConnection;

            // Only exception is current condition where DTPT gets enabled
            // for every CE device in turn with one OFFICE AP still turned
            // ON. First of all DTPT for all CE devices can not be turned
            // on as one desktop can handle only one CE device at a time
            // over DTPT. Secondly when DTPT is connected we want to test
            // if WiFi gets brought down or not and hence we leave office
            // AP ON. So while DTPT on one device is turned on other devices
            // may see WiFi connection to office AP being on for longer than
            // MAX_CONNECTION_TIME_ALLOWED. So lets give that condition waiver
            // here.
            bool fException = ((newStatus.connStatus & CONN_WIFI_LAN)
                           && (_tcsicmp(newStatus.wifiConn.GetAPSsid(),
                                        Config::GetOfficeSSID()) == 0));
            if ((idleSeconds > MAX_CONNECTION_TIME_ALLOWED)
             && (newStatus.longConnErrorNetChanges != newStatus.netChangesSeen)
             && (!fException))
            {
                newStatus.longConnErrorNetChanges = newStatus.netChangesSeen;

                LogError(TEXT("Detected %ld seconds with no changes seen"), 
                         idleSeconds);

                ++(newStatus.numberErrorsInThisCycle);
                if (!newStatus.fErrorLoggedInThisCycle)
                {
                    newStatus.fErrorLoggedInThisCycle = true;
                    if (newStatus.connStatus & CONN_CONNECTED)
                         ++(newStatus.failedLongConn);
                    else ++(newStatus.failedNoConn);
                }
            }
        }
    }

    LogDebug(TEXT("closeCell=%hs, connected=%hs, numberNetIfsConnected=%d"),
            (newStatus.lastCellConnectionClosed?      "true" : "false"),
            ((newStatus.connStatus & CONN_CONNECTED)? "true" : "false"),
             numberNetIfsConnected);

    // We expect only one network interface to be connected at a time.
    // Associate this issue with the current number of network changes
    // and see how long this lasts. If we see this for longer then
    // MAX_MULTI_CONN_TIME_ALLOWED, this is a real issue that we need to
    // flag. Eg. GPRS is not shutting down and both DTPT & GPRS are on.
    if ((numberNetIfsConnected < 2)
     || (newStatus.multiConnErrorNetChanges == newStatus.netChangesSeen))
    {
        newStatus.numberMultiConnErrorsSeen = 0;
    }
    else
    if (++(newStatus.numberMultiConnErrorsSeen) > MAX_MULTI_CONN_TIME_ALLOWED)
    {
        newStatus.multiConnErrorNetChanges = newStatus.netChangesSeen;
        newStatus.numberMultiConnErrorsSeen = 0;

        LogError(TEXT("Multiple network interfaces connected at once:"));
        LogDebug(TEXT("  DTPT=%hs WiFi=%hs Cellular=%hs"),
                (newStatus.connStatus & CONN_DTPT)? "CONNECTED"
                                                  : "NOT CONNECTED",
                (newStatus.connStatus & CONN_WIFI_LAN)? "CONNECTED"
                                                      : "NOT CONNECTED",
                (newStatus.connStatus & CONN_CELL_RADIO_CONN)? "CONNECTED"
                                                             : "NOT CONNECTED");
        LogStatus(TEXT("err: too many connections"));
  
        ++(newStatus.numberErrorsInThisCycle);
        if (!newStatus.fErrorLoggedInThisCycle)
        {
            newStatus.fErrorLoggedInThisCycle = true;
            ++(newStatus.failedMultiConn);
        }
    }

    // If currently there is no connection and cell Radio is not
    // available then return.
    if ((!(newStatus.connStatus & CONN_CONNECTED))
     && (!(newStatus.connStatus & CONN_CELL_RADIO_AVA)))
    {
        LogDebug(TEXT("Nothing connected"));
     // LogStatus(TEXT("nothing connected"));
        return S_OK;
    }

    // If necessary, close the cell connection before establishing another.
    if (newStatus.lastCellConnectionClosed && newStatus.cellConnectionHandle)
    {
        LogDebug(TEXT("Closing cell connection 0x%x"), newStatus.cellConnectionHandle);
        hr = ConnMgrReleaseConnection(newStatus.cellConnectionHandle, 1);
        if (SUCCEEDED(hr))
        {
            LogStatus(TEXT("closed cell 0x%x"), newStatus.cellConnectionHandle);
        }
        else
        {
            LogError(TEXT("Failed to release connection: %s"), HRESULTErrorText(hr));
            LogStatus(TEXT("can't close cell: 0x%x"), hr);
            // Don't return an error.
            hr = S_OK;
        }
        newStatus.cellConnectionHandle = NULL;
    }

    // Determine the connection type and whether we need to connect the cell.
    bool isWiFiOrDTPT = false;
    bool needCellular = false;
    const char *connType = "cell";
    if (newStatus.connStatus & CONN_DTPT)
    {
        connType = "DTPT";
        isWiFiOrDTPT = true;
    }
    else
    if (newStatus.connStatus & CONN_WIFI_LAN)
    {
        connType = "WiFi";
        isWiFiOrDTPT = true;
    }
    else
    if (!(newStatus.connStatus & CONN_CONNECTED) 
      && (newStatus.connStatus & CONN_CELL_RADIO_AVA))
    {
        needCellular = true;
    }
    
    // If we have a WiFi or DTPT connection OR there is no connection and
    // cellular is available then establish the WiFi, DTPT or cell connection.
    HANDLE hConnection = NULL;
    if (isWiFiOrDTPT || needCellular)
    {
        LogDebug(TEXT("Establishing ConnMgr %hs connection"), connType);
        
        CONNMGR_CONNECTIONINFO *pConnInfo = (CONNMGR_CONNECTIONINFO *)connInfo.GetData();

        DWORD connMgrStatus;
        hr = ConnMgrEstablishConnectionSync(pConnInfo, &hConnection,
                                            30000, &connMgrStatus);
        if (SUCCEEDED(hr))
        {
            LogDebug(TEXT("Esatblished %hs connection 0x%x"), connType, hConnection);

            // If cell connection was established, note down the handle.
            if (!isWiFiOrDTPT && needCellular)
            {
                assert(newStatus.cellConnectionHandle == NULL);

                // Save the cell connection handle.
                newStatus.cellConnectionHandle = hConnection;
                LogDebug(TEXT("Saved %hs connection handle 0x%x"),
                         connType, newStatus.cellConnectionHandle);

                // The test will NOT consider connection to cell as valid
                // network change or connection, if its the first change
                // or connection seen. This helps all the tests running
                // on different devices to start with one common count.
                // That is one should start this test on any device when
                // all APs & DTPT is turned off & in that case the test
                // will connect using cellular but then that won't get
                // considered as network change & will wait for next change.
                if (newStatus.netChangesSeen)
                {
                    ++(newStatus.netChangesSeen);
                    newStatus.timeOfLastConnection = RunDuration;
                    LogDebug(TEXT("Captured TimeOfLastConnection %ld"),
                             newStatus.timeOfLastConnection);
                }
            }
        }
        else
        if (connMgrStatus == CONNMGR_STATUS_WAITINGCONNECTION)
        {
            LogStatus(TEXT("%hs is connecting"), connType);
        }
        else
        {
            LogError(TEXT("EstablishConnectionSync: failed to establish")
                     TEXT(" %hs connection: status:0x%x: %s"),
                     connType, connMgrStatus, HRESULTErrorText(hr));
            LogStatus(TEXT("%hs connect failed: 0x%x"), connType, hr);
            goto Cleanup;
        }
    }

    // Do data transfer and verify...
    if (isWiFiOrDTPT)
    {
        APPool_t *pAPPool = NULL;
        DWORD returnedStatus = 0;

        hr = Config::GetAPPool(newStatus.connStatus, &pAPPool);
        if (SUCCEEDED(hr))
        {
            ASSERT(pAPPool != NULL);
            hr = pAPPool->GetTestStatus(&returnedStatus);
            pAPPool->Disconnect();
        }

        if (FAILED(hr))
        {
            LogError(TEXT("GetTestStatus Failed: %s"), HRESULTErrorText(hr));
            LogStatus(TEXT("GetTestStatus failed: 0x%x"), hr);
            // Don't return an error.
            hr = S_OK;
        }
        else
        {
            // Note that this test comes to know about the cycle number
            // from Server running on desktop. The test does NOT try to
            // guess what cycle it could be based on the network changes
            // seen during the run.
            DWORD serverCycle  = HIWORD(returnedStatus);
            DWORD serverStatus = LOWORD(returnedStatus);

            if (serverCycle != newStatus.currentCycle)
            {
                ++(newStatus.cycleTransitions);

                LogDebug(TEXT("New Cycle detected. Last=%u New=%u Changes=%u"),
                         newStatus.currentCycle, serverCycle,
                         newStatus.cycleTransitions);
                LogDebug(TEXT("Number different errors in cycle = %d"),
                         newStatus.numberErrorsInThisCycle);

                newStatus.numberErrorsInThisCycle = 0;
                newStatus.fErrorLoggedInThisCycle = false;

                m_pNetlogAnalyser->StartCapture(serverCycle);

                if (newStatus.firstCycle == 0)
                    newStatus.firstCycle = serverCycle;
            }

            newStatus.currentCycle = serverCycle;
            LogDebug(TEXT("GetTestStatus Succeeded: cycle %u, status %u"),
                   newStatus.currentCycle, serverStatus);
        }
    }

    // If it's not a cell connection, we can release it.
    if (isWiFiOrDTPT)
    {
        LogDebug(TEXT("Releasing ConnMgr connection 0x%x"), hConnection);
        hr = ConnMgrReleaseConnection(hConnection, MAX_CONNECTION_TIME_ALLOWED);
        if (SUCCEEDED(hr))
        {
            LogDebug(TEXT("Released the ConnMgr %hs connection"), connType);
        }
        else
        {
            LogError(TEXT("ConnMgrReleaseConnection: failed to release")
                     TEXT(" %hs connection: %s"),
                     connType, HRESULTErrorText(hr));
            goto Cleanup;
        }
    }

Cleanup:

    // Store the updated status information.
    m_Status = newStatus;

    return hr;
}

// ----------------------------------------------------------------------------
