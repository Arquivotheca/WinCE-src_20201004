//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// Copyright 2001, Cisco Systems, Inc.  All rights reserved.
// No part of this source, or the resulting binary files, may be reproduced,
// transmitted or redistributed in any form or by any means, electronic or
// mechanical, for any purpose, without the express written permission of Cisco.
//
//	AiroDef.h
#ifndef __AiroDef_h__
#define __AiroDef_h__

#define	ETHERNET_ADDRESS_LENGTH			6
#define MAXIMUM_ETHERNET_PACKET_SIZE    1514
#define	FORCE(b)						FALSE		
#define AIRONET_HEADER_SIZE 			14		// Size of the ethernet header
#define AIRONET_LENGTH_OF_ADDRESS 		6		// Size of the ethernet address
#define AIRONET_MAX_LOOKAHEAD 			(256 - AIRONET_HEADER_SIZE)
#define MAX_XMIT_BUFS   				16
typedef UINT							XMIT_BUF;
#define TX_BUF_SIZE						2048
#define RX_BUF_SIZE						(MAXIMUM_ETHERNET_PACKET_SIZE+4)
#define ARRAY_MULTICASTLISTMAX			128
#define	CARDS_MAX						32

#define BASE_FREQUENCY_MHZ              2412

#define MAX_ENTERPRISE_WEP_KEY_INDEX    3


#ifdef UNDER_CE
#ifdef	DEBUG
//
// Windows CE debug zones
//
#define ZONE_ERROR      DEBUGZONE(0)
#define ZONE_WARN       DEBUGZONE(1)
#define ZONE_FUNCTION   DEBUGZONE(2)
#define ZONE_INIT       DEBUGZONE(3)
#define ZONE_INTR       DEBUGZONE(4)
#define ZONE_RCV        DEBUGZONE(5)
#define ZONE_XMIT       DEBUGZONE(6)
#define	ZONE_QUERY		DEBUGZONE(7)
#define	ZONE_SET		DEBUGZONE(8)
#endif
#endif


#endif