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
// Definitions and declarations for the ConnStatus_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_ConnStatus_T_
#define _DEFINED_ConnStatus_T_
#pragma once

#include "WiFiConn_t.hpp"

namespace ce {
namespace qa {
    
// ----------------------------------------------------------------------------
//
// Connection statistics.
//
class ConnStatus_t
{
public:
    
    ConnStatus_t(void);

  // Current Status:
        
    // Total cycles reported by Server:
    DWORD firstCycle;       // first cycle number seen
    DWORD currentCycle;     // latest cycle number seen
    DWORD cycleTransitions; // count of cycle-number transitions
    
    // Connection status flags and string:
    DWORD connStatus;
    TCHAR connStatusString[128];

    // WiFi interface connection information:
    WiFiConn_t wifiConn;

    // Cellular interface connection information:
    TCHAR  cellNetType   [64];   // GPRS, GSM, etc.
    TCHAR  cellNetDetails[128];  // T-Mobile, Sprint, etc.
    HANDLE cellConnectionHandle; // Handle to current cell connection

  // Historical Record:
    
    // Total number of changes seen by this test:
    DWORD netChangesSeen;

    // NetChangesSeen counter the last time we saw a long connection
    // or multiple connections at once:
    DWORD  longConnErrorNetChanges;
    DWORD multiConnErrorNetChanges;

    // Number times we've seen multiple connections at once:
    DWORD numberMultiConnErrorsSeen;

    // Number errors which occurred during last cycle:
    DWORD numberErrorsInThisCycle;
    bool  fErrorLoggedInThisCycle;
    
    // Error counters:
    DWORD failedNoConn;         // No connection for MAX_SECONDS_WITHOUT_CHANGE
    DWORD failedLongConn;       // No changes for MAX_SECONDS_WITHOUT_CHANGE
    DWORD failedMultiConn;      // Multiple connections at once
    DWORD failedUnknownAP;      // Connected to unknown Access Point
    DWORD failedCellGlitch;     // WiFi roam took too long, GPRS connected instead

    // Connection counters:
    DWORD countHomeWiFiConn;    // WiFi connected to Home AP
    DWORD countOfficeWiFiConn;  // WiFi connected to Office AP
    DWORD countHotSpotWiFiConn; // WiFi Connected to Hotspot AP
    DWORD countOfficeWiFiRoam;  // WiFi roamed within Office SSID
    DWORD countDTPTConn;        // Connected to DTPT
    DWORD countCellConn;        // Connected to cellular

    // Time of last connection:
    long timeOfLastConnection;

    // Time of last cell connection and flag indicating the connection
    // was brought down in favor of a WiFi or DTPT connection:
    long lastCellConnectionTime;
    bool lastCellConnectionClosed;
};

};
};

// ----------------------------------------------------------------------------
//
// Constants:
//

// Connection status flags:
static const DWORD CONN_NOT_CONNECTED    = 0x00;
static const DWORD CONN_CONNECTED        = 0x01;
static const DWORD CONN_DTPT             = 0x02; // DTPT (USB ActiveSync) Connected
static const DWORD CONN_WIFI_LAN         = 0x04; // WiFi connected
static const DWORD CONN_CELL_RADIO_AVA   = 0x08; // Cellular Available
static const DWORD CONN_CELL_RADIO_CONN  = 0x10; // Cellular Connected

// Maximum seconds without a connection state change before logging
// either a "no-connection" or "long-connection" error:
static const unsigned int MAX_CONNECTION_TIME_ALLOWED = (2*60); // 2 minutes

// Maximum seconds spent with two or more active connections before we
// log a "multi-connection" error:
static const unsigned int MAX_MULTI_CONN_TIME_ALLOWED = (2); // 2 seconds

// If a cell connection has been up less than this time when it is brought
// down in favor of a WiFi connection, we assume it was used as a stopgap
// during a particularly long roam between APs:
static const unsigned int MAX_CELL_GLITCH_TIME        = (30); // 30 seconds

#endif /* _DEFINED_ConnStatus_T_ */
// ----------------------------------------------------------------------------
