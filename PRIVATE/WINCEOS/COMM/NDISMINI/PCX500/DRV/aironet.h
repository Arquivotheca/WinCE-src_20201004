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
////	aironet.H

//	.h
#ifndef __aironet_h__
#define __aironet_h__

#include "NDISVER.h"

extern "C" {
#include <ndis.h>
}
#include <802_11.h>

	
#define MIN_IRQ 2
#define MAX_IRQ 15

#define BASIC_OIDS					\
    OID_GEN_SUPPORTED_LIST,			\
    OID_GEN_HARDWARE_STATUS,		\
    OID_GEN_MEDIA_SUPPORTED,		\
    OID_GEN_MEDIA_IN_USE,			\
    OID_GEN_MAXIMUM_LOOKAHEAD,		\
    OID_GEN_MAXIMUM_FRAME_SIZE,		\
    OID_GEN_MAXIMUM_TOTAL_SIZE,		\
    OID_GEN_MAC_OPTIONS,			\
    OID_GEN_PROTOCOL_OPTIONS,		\
    OID_GEN_LINK_SPEED,				\
    OID_GEN_TRANSMIT_BUFFER_SPACE,	\
    OID_GEN_RECEIVE_BUFFER_SPACE,	\
    OID_GEN_TRANSMIT_BLOCK_SIZE,	\
    OID_GEN_RECEIVE_BLOCK_SIZE,		\
    OID_GEN_VENDOR_DESCRIPTION,		\
    OID_GEN_VENDOR_ID,				\
    OID_GEN_DRIVER_VERSION,			\
    OID_GEN_CURRENT_PACKET_FILTER,	\
    OID_GEN_CURRENT_LOOKAHEAD,		\
    OID_GEN_XMIT_OK,				\
    OID_GEN_RCV_OK,					\
    OID_GEN_XMIT_ERROR,				\
    OID_GEN_RCV_ERROR,				\
    OID_GEN_RCV_NO_BUFFER,			\
    OID_802_3_PERMANENT_ADDRESS,	\
    OID_802_3_CURRENT_ADDRESS,		\
    OID_802_3_MULTICAST_LIST,		\
    OID_802_3_MAXIMUM_LIST_SIZE,	\
    OID_802_3_RCV_ERROR_ALIGNMENT,	\
    OID_802_3_XMIT_ONE_COLLISION,	\
    OID_802_3_XMIT_MORE_COLLISIONS,	\
	OID_GEN_MAXIMUM_SEND_PACKETS, \
	OID_GEN_MEDIA_CONNECT_STATUS, \
	OID_GEN_VENDOR_DRIVER_VERSION		


/*
#define _802_11_OIDS					\
	OID_802_11_BSSID					,\
	OID_802_11_SSID						,\
	OID_802_11_NETWORK_TYPES_SUPPORTED	,\
	OID_802_11_NETWORK_TYPE_IN_USE		,\
	OID_802_11_TX_POWER_LEVEL			,\
	OID_802_11_RSSI						,\
	OID_802_11_RSSI_TRIGGER				,\
	OID_802_11_INFRASTRUCTURE_MODE		,\
	OID_802_11_FRAGMENTATION_THRESHOLD	,\
	OID_802_11_RTS_THRESHOLD			,\
	OID_802_11_NUMBER_OF_ANTENNAS		,\
	OID_802_11_RX_ANTENNA_SELECTED		,\
	OID_802_11_TX_ANTENNA_SELECTED		,\
	OID_802_11_RATES_SUPPORTED			,\
	OID_802_11_BASIC_RATES				,\
	OID_802_11_DESIRED_RATES			,\
	OID_802_11_CONFIGURATION			,\
	OID_802_11_STATISTICS				,\
	OID_802_11_ADD_WEP					,\
	OID_802_11_REMOVE_WEP				,\
	OID_802_11_DISASSOCIATE				,\
	OID_802_11_POWER_MODE				,\
	OID_802_11_BSSID_LIST				,\
	OID_802_11_AUTHENTICATION_MODE		
*/

#define _802_11_OIDS   			       \
    OID_802_11_BSSID                  ,\
    OID_802_11_SSID                   ,\
    OID_802_11_NETWORK_TYPES_SUPPORTED,\
    OID_802_11_NETWORK_TYPE_IN_USE    ,\
    OID_802_11_TX_POWER_LEVEL         ,\
    OID_802_11_RSSI                   ,\
    OID_802_11_RSSI_TRIGGER           ,\
    OID_802_11_INFRASTRUCTURE_MODE    ,\
    OID_802_11_FRAGMENTATION_THRESHOLD,\
    OID_802_11_RTS_THRESHOLD          ,\
    OID_802_11_NUMBER_OF_ANTENNAS     ,\
    OID_802_11_RX_ANTENNA_SELECTED    ,\
    OID_802_11_TX_ANTENNA_SELECTED    ,\
    OID_802_11_RATES_SUPPORTED        ,\
    OID_802_11_BASIC_RATES            ,\
    OID_802_11_DESIRED_RATES          ,\
    OID_802_11_CONFIGURATION          ,\
    OID_802_11_STATISTICS             ,\
    OID_802_11_ADD_WEP                ,\
    OID_802_11_REMOVE_WEP             ,\
    OID_802_11_WEP_STATUS             ,\
    OID_802_11_DISASSOCIATE           ,\
    OID_802_11_POWER_MODE             ,\
    OID_802_11_BSSID_LIST             ,\
    OID_802_11_AUTHENTICATION_MODE    ,\
    OID_802_11_BSSID_LIST_SCAN


#define _SUPPORTED_802_11_OIDS   			  \
    OID_802_11_BSSID                  ,\
    OID_802_11_SSID                   ,\
    OID_802_11_NETWORK_TYPE_IN_USE    ,\
    OID_802_11_RSSI                   ,\
    OID_802_11_INFRASTRUCTURE_MODE    ,\
    OID_802_11_RATES_SUPPORTED        ,\
    OID_802_11_BASIC_RATES            ,\
    OID_802_11_CONFIGURATION          ,\
    OID_802_11_ADD_WEP                ,\
    OID_802_11_REMOVE_WEP             ,\
    OID_802_11_WEP_STATUS             ,\
    OID_802_11_DISASSOCIATE           ,\
    OID_802_11_BSSID_LIST             ,\
    OID_802_11_AUTHENTICATION_MODE    ,\
    OID_802_11_BSSID_LIST_SCAN        ,\
    OID_802_11_RELOAD_DEFAULTS




/*
#define _802_11_SUBOIDS					\
	OID_802_11_BSSID,					\
	OID_802_11_ADD_WEP,					\
	OID_802_11_REMOVE_WEP							
*/

#if NDISVER == 3					
#	define	STANDARD_OIDS	BASIC_OIDS,				\
							_SUPPORTED_802_11_OIDS
#else
#	define 	STANDARD_OIDS	BASIC_OIDS,				\
							OID_PNP_CAPABILITIES,	\
                            OID_PNP_ENABLE_WAKE_UP, \
                            OID_PNP_ADD_WAKE_UP_PATTERN, \
                            OID_PNP_REMOVE_WAKE_UP_PATTERN, \
							OID_PNP_QUERY_POWER,	\
							OID_PNP_SET_POWER,		\
							OID_GEN_PHYSICAL_MEDIUM,\
							OID_TCP_TASK_OFFLOAD,	\
							_SUPPORTED_802_11_OIDS
#endif								



typedef struct _DRIVER_BLOCK {
    NDIS_HANDLE			NdisMacHandle;          // returned from NdisRegisterMac
    NDIS_HANDLE			NdisWrapperHandle;      // returned from NdisInitializeWrapper
    struct	_ADAPTER	* AdapterQueue;			// Adapters registered for this Miniport driver.
} DRIVER_BLOCK, * PDRIVER_BLOCK;

       
VOID
cbHalt(
	IN  NDIS_HANDLE	Context
    );

NDIS_STATUS
cbInitialize(
    OUT PNDIS_STATUS OpenErrorStatus,
    OUT PUINT SelectedMediumIndex,
    IN PNDIS_MEDIUM MediumArray,
    IN UINT MediumArraySize,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE ConfigurationHandle
    );
#endif

 