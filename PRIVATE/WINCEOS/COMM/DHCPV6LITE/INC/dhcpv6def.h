//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


Module Name:

    dhcpv6def.h

Abstract:

    DHCPv6 Client Definitions



    FrancisD

Revision History:



--*/

#ifndef __DHCPV6_DEF_H__
#define __DHCPV6_DEF_H__


//
// Definitions for DHCPv6 Ports
//
#define DHCPV6_CLIENT_LISTEN_PORT  546
#define DHCPV6_SERVER_LISTEN_PORT  547


//
// Definitions for DHCPv6 Addresses
//
#define All_DHCP_Relay_Agents_and_Servers   L"FF02::1:2"


//
// Timeout Definitions (seconds)
//
#define DHCPV6_SOL_MAX_DELAY     1
#define DHCPV6_SOL_TIMEOUT       1
#define DHCPV6_SOL_MAX_RT      120
#define DHCPV6_REQ_TIMEOUT       1
#define DHCPV6_REQ_MAX_RT       30
#define DHCPV6_REQ_MAX_RC       10
#define DHCPV6_CNF_MAX_DELAY     1
#define DHCPV6_CNF_TIMEOUT       1
#define DHCPV6_CNF_MAX_RT        4
#define DHCPV6_CNF_MAX_RD       10
#define DHCPV6_REN_TIMEOUT      10
#define DHCPV6_REN_MAX_RT      600
#define DHCPV6_REB_TIMEOUT      10
#define DHCPV6_REB_MAX_RT      600
#define DHCPV6_INF_MAX_DELAY     1
#define DHCPV6_INF_TIMEOUT       1
#define DHCPV6_INF_MAX_RT      120
#define DHCPV6_REL_TIMEOUT       1
#define DHCPV6_REL_MAX_RC        5
#define DHCPV6_DEC_TIMEOUT       1
#define DHCPV6_DEC_MAX_RC        5
#define DHCPV6_REC_TIMEOUT       2
#define DHCPV6_REC_MAX_RC        8
#define DHCPV6_HOP_COUNT_LIMIT  32


#endif // __DHCPV6_DEF_H__




















