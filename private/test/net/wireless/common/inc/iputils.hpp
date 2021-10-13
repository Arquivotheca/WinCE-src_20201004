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

#include "WiFUtils.hpp"

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// IP-network utility functions.
//
struct MACAddr_t;
class IPUtils
{
public:    

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
    static const long MinimumEchoQueueSize = 8*1024; // max un-echoed data
    static const long DefaultEchoQueueSize = 64*1024;// (only used for UDP)
    static const long MaximumEchoQueueSize = 1024*1024;
    static DWORD
    SendEchoMessages(
        const TCHAR  *pHostName,
        const TCHAR  *pInterfaceName = NULL,               // Name of the Interface to which Messages should be sent
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
    static const long DefaultEchoTimeDuration = 20;
    static DWORD
    SendEchoMessagesForTime(
        const TCHAR  *pHostName,
        const TCHAR  *pInterfaceName = NULL, // Name of the Interface to which Messages should be sent
        DWORD         HostPort       = DefaultEchoPort,
        long          ReplyTimeout   = DefaultEchoTimeout,   // ms to await replies
        bool          ConnectTCP     = false,                // true=TCP, false=UDP
        long          SendSize       = DefaultEchoSendSize,  // packet size
        long          TimeDuration   = DefaultEchoTimeDuration);

    // Sends Voip echo messages to the specified host
    // until the specified time-limit expires with special packet type
    // Returns ERROR_CONNECTION_INVALID to signal a failure which could
    // be caused by the normal negative WZC API tests.
    static const long DefaultEchoInterval  = 20;   // Default Voip packet interval
    static const long DefaultVoipEchoSize  = 180;  // Default Voip packet payload
    static const long DefaultVoipTimeDuration = 20; // Default Echo Time
    static DWORD
    SendVoipEchoMessages(
        const TCHAR  *pHostName,
        const TCHAR  *pInterfaceName = NULL,                     // Name of the Interface to which Messages should be sent
        long          TimeDurationInSec  = DefaultVoipTimeDuration,  // how long to send data in sec 
        DWORD         HostPort           = DefaultEchoPort,
        long          SendSize           = DefaultVoipEchoSize,  // packet size
        long          PacketIntervalInMs = DefaultEchoInterval); // Time interval between two packets in MilliSec        
        
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
    static const long MaximumPingSendCount = 10*1000*1000;
    static DWORD
    SendPingMessages(
        const TCHAR  *pHostName,
        const TCHAR  *pInterfaceName = NULL,             // Name of the Interface to which Messages should be sent
        DWORD         HostPort     = DefaultPingPort,
        long          ReplyTimeout = DefaultPingTimeout, // ms to await replies
        long          SendSize  = DefaultPingSendSize,   // packet size
        long          SendCount = DefaultPingSendCount,  // packets to send
        UCHAR         TimeToLive = 32,                   // IP TTL value
        UCHAR         TypeOfService = 0,                 // IP TOS value
        UCHAR         IPHeaderFlags = 0,                 // IP header flags
        UCHAR        *pOptionsData = NULL,               // IP options
        UCHAR         OptionsSize = 0);                  // size of IP options
};

};
};
#endif /* _DEFINED_IPUtils_ */
// ----------------------------------------------------------------------------
