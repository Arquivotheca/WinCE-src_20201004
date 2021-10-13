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
// Definitions and declarations for the IPUtils class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_IPUtils_
#define _DEFINED_IPUtils_
#pragma once

#include "Utils.hpp"

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// IP-network utility functions.
//
class WZCService_t;
class IPUtils
{
public:

    // Initializes or cleans up static resources:
    static void StartupInitialize(void);
    static void ShutdownCleanup  (void);

    // Gets the IP addresses assigned to the specified wireless adapter:
    // Returns ERROR_NOT_CONNECTED if the adapter hasn't been connected
    // and assigned a valid IP address yet.
    static DWORD
    GetIPAddresses(
                                 const TCHAR *pAdapterName,
      __out_ecount(AddressChars) TCHAR       *pIPAddress,
      __out_ecount(AddressChars) TCHAR       *pGatewayAddress, // optional
                                 int          AddressChars);

    // Monitors the specified wireless adapter's connection status
    // until either the specified time tuns out or it associates with
    // the specified SSID and is assigned a valid IP address:
    // Returns ERROR_TIMEOUT if the connection isn't essablished within 
    // the time-limit.
    static const long MinimumConnectTimeMS = 5*1000;// ms to associate and assign IP
    static const long DefaultConnectTimeMS = 80*1000;
    static const long MaximumConnectTimeMS = 900*1000;
    static const long MinimumStableTimeMS  = 1000;  // ms connection must remain stable
    static const long DefaultStableTimeMS  = 6*1000;
    static const long MaximumStableTimeMS  = 60*1000;
    static const long MinimumCheckIntervalMS = 100; // granularity between state checks
    static const long DefaultCheckIntervalMS = 250;
    static const long MaximumCheckIntervalMS = 1000;
    static DWORD
    MonitorConnection(
        WZCService_t *pWZCService,
        const TCHAR  *pExpectedSSID,
        long          ConnectTimeMS   = DefaultConnectTimeMS,
        long          StableTimeMS    = DefaultStableTimeMS,
        long          CheckIntervalMS = DefaultCheckIntervalMS);
    
    // Sends one or more UDP or TCP echo messages to the specified host:
    // Returns ERROR_CONNECTION_INVALID to signal a failure which could
    // be caused by the normal negative WZC API tests.
    static const long DefaultEchoPort = 7;
    static const long MinimumEchoTimeout = 100;       // ms to await completion
    static const long DefaultEchoTimeout = 5*1000;
    static const long MaximumEchoTimeout = 24*60*60*1000;
    static const long MinimumEchoSendSize = 32;       // packet size
    static const long DefaultEchoSendSize = 1024;
    static const long MaximumEchoSendSize = 64*1024;
    static const long MinimumEchoSendCount = 1;       // number packets to send
    static const long DefaultEchoSendCount = 5*1000;
    static const long MaximumEchoSendCount = 10*1000*1000;
    static const long MinimumEchoQueueSize  = 8*1024; // max un-echoed data
    static const long DefaultEchoQueueSize  = 64*1024;// (only used for UDP)
    static const long MaximumEchoQueueSize  = 1024*1024;
    static DWORD
    SendEchoMessages(
        const TCHAR *pHostName,
        DWORD         HostPort     = DefaultEchoPort,
        long          ReplyTimeout = DefaultEchoTimeout,   // ms to await replies
        bool          ConnectTCP   = false,                // true=TCP, false=UDP
        long          SendSize     = DefaultEchoSendSize,  // packet size
        long          SendCount    = DefaultEchoSendCount, // packets to send
        long          QueueSize    = DefaultEchoQueueSize);// max un-echoed data 

    // Sends one or more UDP or TCP echo messages to the specified host
    // until the specified time-limit expires:
    // Returns ERROR_CONNECTION_INVALID to signal a failure which could
    // be caused by the normal negative WZC API tests.
    static DWORD
    SendEchoMessagesForTime(
        const TCHAR *pHostName,
        DWORD         HostPort,
        long          ReplyTimeout,  // ms to await replies
        bool          ConnectTCP,    // true=TCP, false=UDP
        long          SendSize,      // packet size
        long          TimeDuration); // how long to send data in sec
        
    // Sends one or more ICMP ping messages to the specified host:
    // Returns ERROR_CONNECTION_INVALID to signal a failure which could
    // be caused by the normal negative WZC API tests.
    static const long DefaultPingPort = 5000;
    static const long MinimumPingTimeout = 100;       // ms to await completion
    static const long DefaultPingTimeout = 5*1000;
    static const long MaximumPingTimeout = 24*60*60*1000;
    static const long MinimumPingSendSize = 32;       // packet size
    static const long DefaultPingSendSize = 1024;
    static const long MaximumPingSendSize = 4*1024;
    static const long MinimumPingSendCount = 1;       // number pings to send
    static const long DefaultPingSendCount = 1000;
    static const long MaximumPingSendCount = 100*1000;
    static DWORD
    SendPingMessages(
        const TCHAR *pHostName,
        DWORD         HostPort     = DefaultPingPort,
        long          ReplyTimeout = DefaultPingTimeout, // ms to await replies
        long          SendSize  = DefaultPingSendSize,   // packet size
        long          SendCount = DefaultPingSendCount,  // packets to send
        UCHAR         TimeToLive = 32,     // IP TTL value
        UCHAR         TypeOfService = 0,   // IP TOS value
        UCHAR         IPHeaderFlags = 0,   // IP header flags
        UCHAR       *pOptionsData = NULL,  // IP options
        UCHAR         OptionsSize = 0);    // size of IP options
};

};
};
#endif /* _DEFINED_IPUtils_ */
// ----------------------------------------------------------------------------
