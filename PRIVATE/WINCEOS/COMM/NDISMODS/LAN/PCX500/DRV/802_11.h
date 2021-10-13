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
//  802_11.h
#ifndef __802_h__
#define __802_h__

/************************************************************************************
OID_802_11_BSSID

This object is the MAC address of the associated Access point. Setting is useful when doing a site survey.

Data type:  NDIS_802_11_MAC_ADDRESS
Query:      Returns the current Access Point MAC address.
Set:        Sets the MAC address of the desired Access Point.
Indication:     Not supported

OID_802_11_SSID

This object defines the Service Set Identifier. The SSID is a string, up to 32 characters. It identifies a set of interconnected Basic Service Sets. Passing in an empty string means associate with any SSID.

Data type:  NDIS_802_11_SSID
Query:      Returns the SSID.
Set:        Sets the SSID.
Indication:     Not supported

OID_802_11_NETWORK_TYPES_SUPPORTED

Data type:  NDIS_802_11_NETWORK_TYPE_LIST
Query:  Returns an array of all NDIS_802_11_NETWORK_TYPE(s) supported by the driver and the device.
Set:        Not supported
Indication:     Not supported

OID_802_11_NETWORK_TYPE_IN_USE

Data type:  NDIS_802_11_NETWORK_TYPE
Query:      Returns the current NDIS_802_11_NETWORK_TYPE used by the device.
Set:        Will set the network type that should be used for the driver. 
Indication:     Not supported.

OID_802_11_POWER_MODE
Data type:  NDIS_802_11_POWER_MODE
Query:      Returns the current NDIS_802_11_POWER_MODE.
Set:        Sets the current NDIS_802_11_POWER_MODE. 
Indication:     Not supported.

OID_802_11_TX_POWER_LEVEL

Transmit power level in mW.

Data type:  NDIS_802_11_TX_POWER_LEVEL
Query:      Returns the current NDIS_802_11_TX_POWER_LEVEL value.
Set:    Sets the current NDIS_802_11_TX_POWER_LEVEL value. Device will not exceed regulatory levels.
Indication:     Not supported.

OID_802_11_RSSI
Returns the Received Signal Strength Indication in dBm. 

Data type:  NDIS_802_11_RSSI
Query:      Returns the current RSSI value.
Set:        Not supported.
Indication: If an indication request is enabled, then an event is triggered according to the value as given in the set. 

OID_802_11_RSSI_TRIGGER
Queries or sets a trigger value for the RSSI event. If the trigger value is < current RSSI value then the indication occurs when the current value is >= trigger value. If the trigger value is > current value then the indication occurs when the current value is <= trigger value. If the trigger value is equal to the current value then the indication occurs immediately. NdisMIndicateStatus will be called with NDIS_STATUS_MEDIA_SPECIFIC_INDICATION as the GeneralStatus and the StatusBuffer will point to a NDIS_802_1_RSSI buffer.

Data type:  NDIS_802_11_RSSI
Query:      Returns the current RSSI trigger value.
Set:        Sets the Received Signal Strength Indication trigger value for an event.
Indication: Not supported. 

OID_802_11_BSSID_LIST
Returns list of all BSSID in range, their SSID and their RSSI.

Data type:  NDIS_802_11_BSSID_LIST
Query:      Returns a NDIS_802_11_BSSID_LIST structure, containing BSSIDs, RSSI, etc.
Set:        Not supported 
Indication:     Not supported.

OID_802_11_INFRASTRUCTURE_MODE
Query or Set how an 802.11 NIC connects to the network. Will also reset the network association algorithm.

Data type:  NDIS_802_11_NETWORK_INFRASTRUCTURE
Query:      Returns either Infrastructure or Independent Basic Service Set (IBSS), unknown.
Set:        Sets Infrastructure or IBSS, or automatic switch between the two.
Indication: Not supported.


//OID_802_11_FRAGMENTATION_THRESHOLD

The WLAN will fragment packets that exceed this threshold. Zero removes fragmentation.

Data type:  NDIS_802_11_FRAGMENTATION_THRESHOLD
Query:      Returns the current fragmentation threshold, in bytes
Set:        Sets the fragmentation threshold, in bytes
Indication:     Not supported.

OID_802_11_RTS_THRESHOLD

Packets exceeding this threshold will invoke the RTS/CTS mechanism by the WLAN. Zero removes RTS/CTS.

Data type:  NDIS_802_11_RTS_THRESHOLD
Query:      Returns the current RTS threshold, in bytes
Set:        Sets the RTS threshold, in bytes
Indication:     Not supported.

OID_802_11_NUMBER_OF_ANTENAS

Returns the number of antennas on the radio.

Data type:  ULONG
Query:      Returns the number of antennas on the radio.
Set:        Not supported.
Indication:     Not supported.

OID_802_11_RX_ANTENNA_SELECTED

Returns the antenna selected for receiving on the radio.

Data type:  NDIS_802_11_ANTENNA
Query:      Returns the antenna selected for receiving.
Set:        Sets the antenna used for receiving.
Indication:     Not supported.

Note: -1 is all antennas (i.e. Diversity)

OID_802_11_TX_ANTENNA_SELECTED

Returns the antennae selected for transmitting on the radio.

Data type:  NDIS_802_11_ANTENNA
Query:      Returns the antennae selected for transmitting.
Set:        Sets the antennae used for transmitting.
Indication:     Not supported.

Note: -1 is all antennas (i.e. Diversity)

OID_802_11_RATES_SUPPORTED

A list of that data rates that the radio is capable of running at. The units are .5 Mbps.

Data type:  NDIS_802_11_RATE_LIST
Query:      Returns the rates the radio is capable of running.
Set:        Not supported.
Indication:     Not supported.

OID_802_11_BASIC_RATES

A list of the data rates that is mandatory for another radio to communicate with in a system. These rates are used for packets such as, control packets and broadcast packets. These packets need to be received by the system of radios. The units are .5 Mbps.

Data type:  NDIS_802_11_RATE_LIST
Query:      Returns the list of basic rates.
Set:        Sets the list of basic rates.
Indication:     Not supported.

OID_802_11_DESIRED_RATES

A list of the data rates that is desirable for the radio to operate. Packets that are directed to the radio can run at a different value than the basic rates. The units are .5 Mbps.

Data type:  NDIS_802_11_RATE_LIST
Query:      Returns the list of rates.
Set:        Sets the list of rates.
Indication:     Not supported.

OID_802_11_CONFIGURATION

Configures the radio parameters.

Data type:  NDIS_802_11_CONFIGURATION
Query:      Returns the current radio configuration.
Set:        Sets the radio configuration.
Indication:     Not supported.

OID_802_11_STATISTICS

Gets the current statistics.

Data type:  NDIS_802_11_STATISTICS
Query:      Returns the current statistics.
Set:        Not supported.
Indication:     Not supported.


OID_802_11_ADD_WEP
The WEP key should not be held in permanent storage but should be lost as soon as the card disassociates with all BSSIDs or if shared key authentication using the WEP key fails. Calling twice for the same index should overwrite the previous value.

Data type:  NDIS_802_11_WEP
Query:      Not supported
Set:        Sets the desired WEP key.
Indication: Not supported

OID_802_11_REMOVE_WEP
The WEP key should not be held in permanent storage but should be lost as soon as the card disassociates with all BSSIDs.

Data type:  NDIS_802_11_KEY_INDEX
Query:      Not supported
Set:        Removes the desired WEP key.
Indication: Not supported


OID_802_11_DISASSOCIATE
Disassociate with current SSID.

Data type:  No data is associated with this Set.
Query:      Not supported
Set:        Disassociates with current SSID.
Indication: Not supported


OID_802_11_AUTHENTICATION_MODE

Sets the 802.11 authentication mode.

Data type:  NDIS_802_11_AUTHENTICATION_MODE
Query:      Current mode
Set:        Set to open or shared or auto-switch
Indication: Not supported


OID_802_11_RELOAD_DEFAULTS

A call to this OID will cause the IEEE 802.11 NIC Driver/Firmware to reload the available default settings for the specified type field. For example, when reload defaults is set to WEP keys, the IEEE 802.11 NIC Driver/Firmware shall reload the available default WEP key settings from the permanent storage, i.e., registry key or flash settings, into the running configuration of the IEEE 802.11 NIC that is not held in the permanent storage. Therefore, a call to this OID with the reload defaults set to WEP keys ensures that the WEP key settings in the IEEE 802.11 NIC running configuration are returned to the configuration following the enabling of the interface from a disabled state, i.e., when the IEEE 802.11 Driver is loaded upon enabling the IEEE 802.11 NIC.   

Data type:	NDIS_802_11_RELOAD_DEFAULTS.
Query:		Not supported.
Set:		Set results in the reloading of the available defaults for specified type field.
Indication:	Not supported. 
************************************************************************************/


#ifndef UNDER_CE

typedef enum _NDIS_POWER_PROFILE
{
    NdisPowerProfileBattery,
    NdisPowerProfileAcOnLine,

} NDIS_POWER_PROFILE, *PNDIS_POWER_PROFILE;

typedef enum NDIS_802_11_OID
{
    OID_802_11_BSSID                    =0x0D010101,
    OID_802_11_SSID                     =0x0D010102,
    OID_802_11_NETWORK_TYPES_SUPPORTED  =0x0D010203,
    OID_802_11_NETWORK_TYPE_IN_USE      =0x0D010204,
    OID_802_11_TX_POWER_LEVEL           =0x0D010205,
    OID_802_11_RSSI                     =0x0D010206,
    OID_802_11_RSSI_TRIGGER             =0x0D010207,
    OID_802_11_INFRASTRUCTURE_MODE      =0x0D010108,
    OID_802_11_FRAGMENTATION_THRESHOLD  =0x0D010209,
    OID_802_11_RTS_THRESHOLD            =0x0D01020A,
    OID_802_11_NUMBER_OF_ANTENNAS       =0x0D01020B,
    OID_802_11_RX_ANTENNA_SELECTED      =0x0D01020C,
    OID_802_11_TX_ANTENNA_SELECTED      =0x0D01020D,
    OID_802_11_RATES_SUPPORTED          =0x0D01020E,
    OID_802_11_BASIC_RATES              =0x0D01020F,
    OID_802_11_DESIRED_RATES            =0x0D010210,
    OID_802_11_CONFIGURATION            =0x0D010211,
    OID_802_11_STATISTICS               =0x0D020212,
    OID_802_11_ADD_WEP                  =0x0D010113,
    OID_802_11_REMOVE_WEP               =0x0D010114,
    OID_802_11_WEP_STATUS               =0x0D01011B,
    OID_802_11_DISASSOCIATE             =0x0D010115,
    OID_802_11_POWER_MODE               =0x0D010216,
    OID_802_11_BSSID_LIST               =0x0D010217,
    OID_802_11_AUTHENTICATION_MODE      =0x0D010118,
    OID_802_11_BSSID_LIST_SCAN          =0x0D01011A,
    OID_802_11_RELOAD_DEFAULTS          =0x0D01011C,

}NDIS_802_11_OID, *PNDIS_802_11_OID;


typedef enum _NDIS_802_11_NETWORK_TYPE {    // currently defined network subtypes,
    Ndis802_11FH,                           // or radio type
    Ndis802_11DS,
    Ndis802_11NetworkTypeMax                // not a real type, defined as an upper bound

} NDIS_802_11_NETWORK_TYPE, *PNDIS_802_11_NETWORK_TYPE;


typedef struct _NDIS_802_11_NETWORK_TYPE_LIST {
    ULONG               NumberOfItems;          // in list below, at least 1
    NDIS_802_11_NETWORK_TYPE    NetworkType [1];
} NDIS_802_11_NETWORK_TYPE_LIST, *PNDIS_802_11_NETWORK_TYPE_LIST;

typedef enum _NDIS_802_11_POWER_MODE {
    Ndis802_11PowerModeCAM,
    Ndis802_11PowerModeMAX_PSP,
    Ndis802_11PowerModeFast_PSP,
    Ndis802_11PowerModeMax                      // not a real mode, defined as an upper bound
} NDIS_802_11_POWER_MODE, *PNDIS_802_11_POWER_MODE;

typedef ULONG   NDIS_802_11_TX_POWER_LEVEL; // in milliwatts

//
// Received Signal Strength Indication
//
typedef LONG  NDIS_802_11_RSSI;         // in dBm

typedef struct _NDIS_802_11_CONFIGURATION_FH {
    ULONG           Length;                         // Length of structure
    ULONG           HopPattern;                     // as defined by 802.11, MSB set
    ULONG           HopSet;                         // to one if non-802.11
    ULONG           DwellTime;                      // units are kusec
} NDIS_802_11_CONFIGURATION_FH, *PNDIS_802_11_CONFIGURATION_FH;

typedef struct _NDIS_802_11_CONFIGURATION {
    ULONG           Length;
    ULONG           BeaconPeriod;                   // units are kusec
    ULONG           ATIMWindow;                     // units are kusec
    ULONG           DSConfig;                       // Frequency, units are kHz
    NDIS_802_11_CONFIGURATION_FH    FHConfig;
} NDIS_802_11_CONFIGURATION, *PNDIS_802_11_CONFIGURATION;

typedef struct _NDIS_802_11_STATISTICS {
    ULONG           Length;                         // set to sizeof(NDIS_802_11_STATISTICS)
    LARGE_INTEGER   TransmittedFragmentCount;
    LARGE_INTEGER   MulticastTransmittedFrameCount;
    LARGE_INTEGER   FailedCount;
    LARGE_INTEGER   RetryCount;
    LARGE_INTEGER   MultipleRetryCount;
    LARGE_INTEGER   RTSSuccessCount;
    LARGE_INTEGER   RTSFailureCount;
    LARGE_INTEGER   ACKFailureCount;
    LARGE_INTEGER   FrameDuplicateCount;
    LARGE_INTEGER   ReceivedFragmentCount;
    LARGE_INTEGER   MulticastReceivedFrameCount;
    LARGE_INTEGER   FCSErrorCount;
} NDIS_802_11_STATISTICS, *PNDIS_802_11_STATISTICS;

typedef  ULONG  NDIS_802_11_KEY_INDEX;

typedef struct _NDIS_802_11_WEP {
    ULONG           Length;
    ULONG           KeyIndex;                   // 0 is the per-client key, 1-N are the global
                                        // keys
    ULONG           KeyLength;                  // length of key in bytes
    UCHAR           KeyMaterial [1];            // variable length depending on above field
} NDIS_802_11_WEP, *PNDIS_802_11_WEP;
//
//
//
typedef UCHAR   NDIS_802_11_MAC_ADDRESS[6];

//typedef UCHAR NDIS_802_11_SSID[33];   // null terminated string, max 32 characters
typedef struct _NDIS_802_11_SSID {
    ULONG   SsidLength;
    UCHAR   Ssid[32];
} NDIS_802_11_SSID, *PNDIS_802_11_SSID;

//
//
//
/*
typedef struct _NDIS_802_11_BSSID_LIST {
    ULONG           NumberOfItems;  // in list below, at least 1
    
    struct _NDIS_WLAN_BSSID {

        NDIS_802_11_MAC_ADDRESS MacAddress;
        UCHAR               Reserved[2];    // padding
        NDIS_802_11_SSID    Ssid;
        ULONG               Rssi;       // receive signal strength in db
    } Bssid [1];
} NDIS_802_11_BSSID_LIST, *PNDIS_802_11_BSSID_LIST;
*/

//
//
//
typedef enum _NDIS_802_11_NETWORK_INFRASTRUCTURE {

    Ndis802_11IBSS,
    Ndis802_11Infrastructure,
    Ndis802_11AutoUnknown,
    Ndis802_11InfrastructureMax // Not a real value, defined as upper bound

} NDIS_802_11_NETWORK_INFRASTRUCTURE, *PNDIS_802_11_NETWORK_INFRASTRUCTURE;
//
//
//
typedef enum _NDIS_802_11_AUTHENTICATION_MODE {
    Ndis802_11AuthModeOpen,
    Ndis802_11AuthModeShared,
    Ndis802_11AuthModeAutoSwitch,
    Ndis802_11AuthModeMax   // Not a real mode, defined as upper bound
} NDIS_802_11_AUTHENTICATION_MODE, *PNDIS_802_11_AUTHENTICATION_MODE;
//
//
//

typedef  ULONG   NDIS_802_11_FRAGMENTATION_THRESHOLD;
typedef  ULONG   NDIS_802_11_RTS_THRESHOLD;
typedef  ULONG   NDIS_802_11_ANTENNA;
//
//
//
typedef struct _NDIS_802_11_RATE_LIST {

    ULONG           NumberOfItems;  // in list below, at least 1
    ULONG           Rate [1];       // in .5 Mbps

} NDIS_802_11_RATE_LIST, *PNDIS_802_11_RATE_LIST;


typedef UCHAR   NDIS_802_11_RATES[8];

typedef struct _NDIS_WLAN_BSSID
{
    ULONG                               Length;             // Length of structure
    NDIS_802_11_MAC_ADDRESS             MacAddress;         // BSSID
    UCHAR                               Reserved[2];
    NDIS_802_11_SSID                    Ssid;               // SSID
    ULONG                               Privacy;            // WEP encryption rqmt.
    NDIS_802_11_RSSI                    Rssi;               // receive signal strength
                                                            // indication in dBm
    NDIS_802_11_NETWORK_TYPE            NetworkTypeInUse;   // Network_Type_In_Use 
    NDIS_802_11_CONFIGURATION           Configuration;      // Configuration
    NDIS_802_11_NETWORK_INFRASTRUCTURE  InfrastructureMode; // Infrastructure_Mode
    NDIS_802_11_RATES                   SupportedRates;     // Supported_Rates
} NDIS_WLAN_BSSID, *PNDIS_WLAN_BSSID;

typedef struct _NDIS_802_11_BSSID_LIST
{
    ULONG           NumberOfItems;  // in list below, at least 1
    NDIS_WLAN_BSSID Bssid[1];
} NDIS_802_11_BSSID_LIST, *PNDIS_802_11_BSSID_LIST;

typedef enum _NDIS_802_11_PRIVACY_FILTER
{
    Ndis802_11PrivFilterAcceptAll,
    Ndis802_11PrivFilter8021xWEP
} NDIS_802_11_PRIVACY_FILTER, *PNDIS_802_11_PRIVACY_FILTER;

typedef enum _NDIS_802_11_WEP_STATUS
{
    Ndis802_11WEPEnabled,       
    Ndis802_11WEPDisabled,       
    Ndis802_11WEPKeyAbsent,     
    Ndis802_11WEPNotSupported        
} NDIS_802_11_WEP_STATUS, *PNDIS_802_11_WEP_STATUS;



typedef enum _NDIS_802_11_RELOAD_DEFAULTS
{
    Ndis802_11ReloadWEPKeys         // reload available default WEP keys from permanent storage
} NDIS_802_11_RELOAD_DEFAULTS, *PNDIS_802_11_RELOAD_DEFAULTS;


#else	//	UNDER_CE


//
//	Not sure where CISCO got this from..
//

#define	OID_802_11_RATES_SUPPORTED		0x0D01020E
#define	OID_802_11_BASIC_RATES			0x0D01020F

#endif

#endif