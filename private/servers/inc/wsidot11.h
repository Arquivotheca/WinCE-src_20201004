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
/*++

    Copyright (C) 2002  Microsoft Corporation

Module Name:

    wsidot11.h

Abstract:

    Common header file for 802.11 Software Infrastructure for Windows.

--*/

#ifndef __WSIDOT11_H__
#define __WSIDOT11_H__

#pragma once

#define MAX_DOT11_ADAPTER_ENUM_COUNT 100

#ifdef __cplusplus
extern "C" {
#endif


typedef struct _DOT11_PHY_LIST {
    ULONG uNumOfEntries;
#ifdef __midl
    [size_is(uNumOfEntries)] PDOT11_PHY_TYPE pDot11PHYType;
#else
    PDOT11_PHY_TYPE pDot11PHYType;
#endif
} DOT11_PHY_LIST, * PDOT11_PHY_LIST;


typedef struct _DOT11_REG_DOMAINS_LIST {
    ULONG uNumOfEntries;
#ifdef __midl
    [size_is(uNumOfEntries)] PDOT11_REG_DOMAIN_VALUE pDot11RegDomainValue;
#else
    PDOT11_REG_DOMAIN_VALUE pDot11RegDomainValue;
#endif
} DOT11_REG_DOMAINS_LIST, * PDOT11_REG_DOMAINS_LIST;


typedef struct _DOT11_ANTENNA_LIST {
    ULONG uNumOfEntries;
#ifdef __midl
    [size_is(uNumOfEntries)] PDOT11_SUPPORTED_ANTENNA pDot11SupportedAntenna;
#else
    PDOT11_SUPPORTED_ANTENNA pDot11SupportedAntenna;
#endif
} DOT11_ANTENNA_LIST, * PDOT11_ANTENNA_LIST;


typedef struct _DOT11_DIV_SELECT_RX_LIST {
    ULONG uNumOfEntries;
#ifdef __midl
    [size_is(uNumOfEntries)] PDOT11_DIVERSITY_SELECTION_RX pDot11DivSelectRx;
#else
    PDOT11_DIVERSITY_SELECTION_RX pDot11DivSelectRx;
#endif
} DOT11_DIV_SELECT_RX_LIST, * PDOT11_DIV_SELECT_RX_LIST;


typedef enum _DOT11_SMT_NOTIFY_TYPE {
    dot11_smt_notify_type_dissassociate = 1,
    dot11_smt_notify_type_deauthenticate = 2,
    dot11_smt_notify_type_authenticate_fail = 3
} DOT11_SMT_NOTIFY_TYPE, * PDOT11_SMT_NOTIFY_TYPE;


typedef struct _DOT11_DISCONNECTED_PEER {
    ULONG uReason;
    DOT11_MAC_ADDRESS dot11Station;
} DOT11_DISCONNECTED_PEER, * PDOT11_DISCONNECTED_PEER;


typedef enum _DOT11_INTEGRITY_FAIL_TYPE {
    dot11_integrity_fail_type_unicast_key = 1,
    dot11_integrity_fail_type_default_key = 2
} DOT11_INTEGRITY_FAIL_TYPE, *PDOT11_INTEGRITY_FAIL_TYPE;


typedef struct _DOT11_INTEGRITY_FAIL {
    DOT11_INTEGRITY_FAIL_TYPE dot11IntegrityFailType;
    DOT11_MAC_ADDRESS dot11PeerMacAddress;
} DOT11_INTEGRITY_FAIL, *PDOT11_INTEGRITY_FAIL;



typedef enum _DOT11_ROW_STATUS {
    dot11_row_status_unknown = 0,
    dot11_row_status_active = 1,
    dot11_row_status_notInService = 2,
    dot11_row_status_notReady = 3,
    dot11_row_status_createAndGo = 4,
    dot11_row_status_createAndWait = 5,
    dot11_row_status_destroy = 6
} DOT11_ROW_STATUS, * PDOT11_ROW_STATUS;


typedef struct _DOT11_GROUP_ADDRESS {
    ULONG uAddressesIndex;
    DOT11_MAC_ADDRESS dot11MacAddress;
    DOT11_ROW_STATUS GroupAddressesStatus;
} DOT11_GROUP_ADDRESS, * PDOT11_GROUP_ADDRESS;


#define MIN_WEP_KEY_MAPPING_LENGTH              10
#define MAX_NUM_OF_GROUP_ADDRESSES              20
#define MAX_NUM_OF_AUTH_ALGOS                   10


typedef struct _DOT11_AUTH_ALGO {
    ULONG uAuthAlgoIndex;
    ULONG uAuthAlgo;
    BOOL bAuthAlgoEnabled;
} DOT11_AUTH_ALGO, * PDOT11_AUTH_ALGO;


typedef struct _DOT11_AUTH_LIST {
    ULONG uNumOfEntries;
#ifdef __midl
    [size_is(uNumOfEntries)] PDOT11_AUTH_ALGO pDot11AuthAlgo;
#else
    PDOT11_AUTH_ALGO pDot11AuthAlgo;
#endif
} DOT11_AUTH_LIST, * PDOT11_AUTH_LIST;


#define MAX_WEP_KEY_INDEX                       4
#define MAX_WEP_KEY_LENGTH                      1024 // Bytes


typedef struct _DOT11_WEP_KEY_DATA {
    ULONG uKeyLength;
#ifdef __midl
    [size_is(uKeyLength)] PUCHAR pucKey;
#else
    __field_ecount(uKeyLength) PUCHAR pucKey;
#endif
} DOT11_WEP_KEY_DATA, * PDOT11_WEP_KEY_DATA;


typedef struct _DOT11_WEP_KEY_ENTRY {
    BOOL bPersist;
    ULONG uWEPKeyIndex;
    DWORD dwAlgorithm;
    DOT11_MAC_ADDRESS MacAddr;
    DOT11_ROW_STATUS WEPKeyStatus;
    PDOT11_WEP_KEY_DATA pDot11WEPKeyData;
} DOT11_WEP_KEY_ENTRY, * PDOT11_WEP_KEY_ENTRY;


typedef struct _DOT11_WEP_KEY_MAPPING_ENTRY {
    ULONG uWEPKeyMappingIndex;
    DOT11_MAC_ADDRESS WEPKeyMappingAddress;
    DWORD dwAlgorithm;
    union {
        DOT11_DIRECTION Direction;      // ExtSTA use Direction
        BOOL bWEPRowIsOutbound;         // STA/AP use bWEPRowIsOutbound
    };
    BOOL bStaticWEPKey;
    DOT11_ROW_STATUS WEPKeyMappingStatus;
    PDOT11_WEP_KEY_DATA pDot11WEPKeyMappingData;
} DOT11_WEP_KEY_MAPPING_ENTRY, * PDOT11_WEP_KEY_MAPPING_ENTRY;



typedef struct _DOT11_SEND_8021X_PKT {
    GUID gAdapterId;
    DOT11_MAC_ADDRESS PeerMacAddress;
    ULONG uContext;
    ULONG uBufferLength;
#ifdef __midl
    [size_is(uBufferLength)] UCHAR ucBuffer[];
#else
    UCHAR ucBuffer[1];                         // Must be the last parameter.
#endif
} DOT11_SEND_8021X_PKT, * PDOT11_SEND_8021X_PKT;


//
// 802.1X Filtering -
// On receive path, use the source mac address which can only be unicast.
// On send path, use the destination mac address only if it is an unicast address.
//

typedef struct _DOT11_8021X_FILTER {
    DOT11_MAC_ADDRESS PeerMacAddress; // Unicast mac address of the peer
    BOOL bIsPortControlled;           // TRUE, if the port is controlled by 802.1X
    BOOL bIsPortAuthorized;           // TRUE, if the port is authorized for data packets
} DOT11_8021X_FILTER, * PDOT11_8021X_FILTER;



#define MAX_DOT11_ASSOC_INFO_ENUM_COUNT         20

typedef enum _DOT11_ASSOC_UPCALL_INFO_TYPE {
    dot11_assoc_upcall_info_type_default_key_value,
    dot11_assoc_upcall_info_type_negotiated_ie,
    dot11_assoc_upcall_info_type_offered_ie,
    dot11_assoc_upcall_info_type_last_tx_tsc
} DOT11_ASSOC_UPCALL_INFO_TYPE, * PDOT11_ASSOC_UPCALL_INFO_TYPE;

typedef struct _DOT11_ASSOC_INDICATION_UPCALL {
    DOT11_MAC_ADDRESS PeerMacAddress;
    USHORT usAID;
    USHORT usDefaultKeyID;
} DOT11_ASSOC_INDICATION_UPCALL, *PDOT11_ASSOC_INDICATION_UPCALL;

#define DOT11_DISASSOC_INDICATION_SUCCESSFUL_ROAM        0x00000001

typedef struct _DOT11_DISASSOC_INDICATION_UPCALL {
    DOT11_MAC_ADDRESS PeerMacAddress;
    DWORD dwFlags;
} DOT11_DISASSOC_INDICATION_UPCALL, *PDOT11_DISASSOC_INDICATION_UPCALL;

typedef struct _DOT11_UPCALL_TLV {
    ULONG uType;
    ULONG uLength;
    UCHAR ucValue[1];       // must be the last field
} DOT11_UPCALL_TLV, *PDOT11_UPCALL_TLV;

#define MAX_RECEIVE_UPCALL_BUFFER_SIZE          sizeof(DOT11_MAC_ADDRESS)+1500

#define DOT11_UPCALL_OP_MODE_STATION            0x00000001
#define DOT11_UPCALL_OP_MODE_AP                 0x00000002
#define DOT11_UPCALL_OP_MODE_REPEATER_AP        0x00000003

typedef struct _DOT11_RECEIVE_UPCALL {
    GUID gAdapterId;
    ULONG uUpcallType;
    ULONG uCurOpMode;
    ULONG uActualBufferLength;
    UCHAR ucBuffer[MAX_RECEIVE_UPCALL_BUFFER_SIZE];
} DOT11_RECEIVE_UPCALL, * PDOT11_RECEIVE_UPCALL;


typedef struct _DOT11_DISASSOCIATE_REQUEST {
    USHORT  AID;
    DOT11_MAC_ADDRESS   PeerMacAddress;
    USHORT  usReason;
} DOT11_DISASSOCIATE_REQUEST, *PDOT11_DISASSOCIATE_REQUEST;


typedef struct _DOT11_CIPHER_ALGO {
    ULONG uCipherAlgoIndex;
    ULONG uCipherAlgo;
    BOOL bCipherAlgoEnabled;
} DOT11_CIPHER_ALGO, * PDOT11_CIPHER_ALGO;

// Cipher Algorithm Id (see section 7.3.2.9.1 in 802.11i Rev 7.0)

#define DOT11_ALGO_AUTO                         0x00000000              // Required for fallback in multicast cipher (WPA STA Artifact)
#define DOT11_ALGO_WEP_RC4_40                   0x00000001
#define DOT11_ALGO_TKIP_MIC                     0x00000002
#define DOT11_ALGO_RESERVED                     0x00000003
#define DOT11_ALGO_CCMP                         0x00000004
#define DOT11_ALGO_WEP_RC4_104                  0x00000005

typedef struct _DOT11_CIPHER_LIST {
    ULONG uNumOfEntries;
#ifdef __midl
    [size_is(uNumOfEntries)] PDOT11_CIPHER_ALGO pDot11CipherAlgo;
#else
    PDOT11_CIPHER_ALGO pDot11CipherAlgo;
#endif
} DOT11_CIPHER_LIST, * PDOT11_CIPHER_LIST;


typedef struct _DOT11_NIC_SPECIFIC_EXTN_LIST {
    ULONG uNumOfBytes;
#ifdef __midl
    [size_is(uNumOfBytes)] PUCHAR pucBuffer;
#else
    PUCHAR pucBuffer;
#endif
} DOT11_NIC_SPECIFIC_EXTN_LIST, * PDOT11_NIC_SPECIFIC_EXTN_LIST;


typedef enum _DOT11_MAC_FILTER_CONTROL_MODE {
    dot11_mac_filter_control_mode_allow_except = 1,
    dot11_mac_filter_control_mode_deny_except = 2
} DOT11_MAC_FILTER_CONTROL_MODE, *PDOT11_MAC_FILTER_CONTROL_MODE;

typedef struct _DOT11_MAC_FILTER_CONTROL {
    DOT11_MAC_ADDRESS MacAddress;
    DOT11_ROW_STATUS  FilterEntryStatus;
} DOT11_MAC_FILTER_CONTROL, * PDOT11_MAC_FILTER_CONTROL;


#define DOT11_EXTD_ASSOC_INFO_TYPE_WPA_NEG        0x00000001

typedef struct _DOT11_EXTD_ASSOC_INFO_TLV {
    ULONG uAssocInfoType;
    ULONG uLength;
    UCHAR ucBuffer[1];  // Must be last field
} DOT11_EXTD_ASSOC_INFO_TLV, * PDOT11_EXTD_ASSOC_INFO_TLV;

typedef struct _DOT11_EXTD_ASSOC_INFO {
    ULONG uNumOfBytes;
#ifdef __midl
    [size_is(uNumOfBytes)] PUCHAR pucBuffer;
#else
    PUCHAR pucBuffer;
#endif
} DOT11_EXTD_ASSOC_INFO, * PDOT11_EXTD_ASSOC_INFO;

typedef enum _DOT11_DSSS_OFDM_MODE {
    dot11_dsss_ofdm_mode_enabled = 1,
    dot11_dsss_ofdm_mode_disabled = 2,
    dot11_dsss_ofdm_mode_only = 3,
} DOT11_DSSS_OFDM_MODE, * PDOT11_DSSS_OFDM_MODE;

typedef enum _DOT11_SHORT_SLOT_TIME_MODE {
    dot11_short_slot_time_mode_enabled = 1,
    dot11_short_slot_time_mode_disabled = 2,
    dot11_short_slot_time_mode_only = 3,
} DOT11_SHORT_SLOT_TIME_MODE, * PDOT11_SHORT_SLOT_TIME_MODE;

// The maximum value of NativeWiFi RSSI
// NativeWiFi RSSI is calculated from RSSI reported
// by NIC. NIC's RSSI has different range, but
// NativeWiFi RSSI is always ranged from 0 to 255
#define STA_MAX_RSSI                    (255)

#define STA_MAX_QUALITY                 (255)
#define STA_MIN_QUALITY                 (0)


typedef struct _DOT11_WME_AC_PARAMETER_RECORD {
    DOT11_AC_PARAM dot11ACParam;
    BOOL bAdmissionControlMandatory;
    UCHAR ucAIFSN;
    UCHAR ucECWmin;             // CWmin = (2 ^ ucECWman) - 1
    UCHAR ucECWmax;             // CWmax = (2 ^ ucECWman) - 1
    USHORT usTXOPLimit;         // in units of 32us
} DOT11_WME_AC_PARAMETER_RECORD, * PDOT11_WME_AC_PARAMETER_RECORD;

typedef struct _DOT11_WME_AC_PARAMETER_RECORD_LIST {
    ULONG uNumOfEntries;
#ifdef __midl
    [size_is(uNumOfEntries)] PDOT11_WME_AC_PARAMETER_RECORD pDot11WMEACParameterRecord;
#else
    PDOT11_WME_AC_PARAMETER_RECORD pDot11WMEACParameterRecord;
#endif
} DOT11_WME_AC_PARAMETER_RECORD_LIST, * PDOT11_WME_AC_PARAMETER_RECORD_LIST;

typedef enum _DOT11_RESV_DIRECTION {
    dot11_resv_direction_uplink = 1,
    dot11_resv_direction_downlink = 2,
    dot11_resv_direction_bidirectional = 3,
} DOT11_RESV_DIRECTION, * PDOT11_RESV_DIRECTION;

typedef struct _DOT11_WME_ADMISSION_REQUEST {
    UCHAR uc8021D;
    DOT11_ROW_STATUS dot11RequestStatus;
    DOT11_RESV_DIRECTION dot11ResvDirection;
    BOOL bIsFixedSize;
    USHORT usNominalMSDUSize;
    ULONG uMeanDataRate;                    // in bits per second
    ULONG uMinPhyRate;                      // in bits per second
    USHORT usSurplusBandwidthAllowance;     // binary point after 3rd MSB
} DOT11_WME_ADMISSION_REQUEST, * PDOT11_WME_ADMISSION_REQUEST;


typedef struct _DOT11_AUTH_ALGORITHM_NAME {
    DOT11_AUTH_ALGORITHM dot11AuthAlgorithm;
#ifdef __midl
    [string] LPWSTR pwszFriendlyName;
#else
    LPWSTR pwszFriendlyName;
#endif
} DOT11_AUTH_ALGORITHM_NAME, * PDOT11_AUTH_ALGORITHM_NAME;



typedef struct _DOT11_AUTH_CIPHER_ALGO_PAIR_LIST {
    ULONG uNumOfEntries;
#ifdef __midl
    [size_is(uNumOfEntries)] PDOT11_AUTH_CIPHER_PAIR pDot11AuthCipherPair;
#else
    PDOT11_AUTH_CIPHER_PAIR pDot11AuthCipherPair;
#endif
} DOT11_AUTH_CIPHER_ALGO_PAIR_LIST, * PDOT11_AUTH_CIPHER_ALGO_PAIR_LIST;

typedef struct _DOT11_ASSOCIATION_INFO {
    DOT11_MAC_ADDRESS PeerMacAddress;
    USHORT usCapabilityInformation;
    USHORT usListenInterval;
    UCHAR ucPeerSupportedRates[MAX_NUM_SUPPORTED_RATES];
    USHORT usAssociationID;
    DOT11_ASSOCIATION_STATE dot11AssociationState;
    DOT11_POWER_MODE dot11PowerMode;
    LARGE_INTEGER liAssociationUpTime;
    ULONGLONG ulNumOfTxPacketSuccesses;
    ULONGLONG ulNumOfTxPacketFailures;
    ULONGLONG ulNumOfRxPacketSuccesses;
    ULONGLONG ulNumOfRxPacketFailures;
} DOT11_ASSOCIATION_INFO, * PDOT11_ASSOCIATION_INFO;


typedef struct _DOT11_PMKID_ENTRY_LIST {
    ULONG uNumOfEntries;
#ifdef __midl
    [size_is(uNumOfEntries)] PDOT11_PMKID_ENTRY pPMKIDs;
#else
    PDOT11_PMKID_ENTRY pPMKIDs;
#endif
} DOT11_PMKID_ENTRY_LIST, *PDOT11_PMKID_ENTRY_LIST;

typedef struct DOT11_SUPPORTED_COUNTRY_OR_REGION_STRING_LIST {
    ULONG uNumOfEntries;
    PDOT11_COUNTRY_OR_REGION_STRING pCountryOrRegionStrings;
} DOT11_SUPPORTED_COUNTRY_OR_REGION_STRING_LIST, * PDOT11_SUPPORTED_COUNTRY_OR_REGION_STRING_LIST;



#ifdef __cplusplus
}
#endif



#define DD_WSIDOT11_DEVICE_NAME L"\\Device\\NativeWiFiP"

// Unique Name for the LWF.
// This GUID must also present in the INF file as well
#define FILTER_UNIQUE_NAME      L"{E475CF9A-60CD-4439-A75F-0079CE0E18A1}"
#define FILTER_FRIENDLY_NAME    L"Native WiFi Filter Driver"

// GUID for wireless power management
// This GUID must also present in the INF file as well
//DEFINE_GUID(WLAN_GUID_POWER_MANAGEMENT,      0xE475CF9A, 0x60CD, 0x4439, 0xA7, 0x5F, 0x00, 0x79, 0xCE, 0x0E, 0x18, 0xA0);
// DEFINE_GUID(WLAN_GUID_POWER_MANAGEMENT ,0x64DB870C, 0xEB1C, 0x452A, 0xB9, 0xEB, 0x11, 0x86, 0xDE, 0x3E, 0xC4, 0xBB);

#define WLAN_POWER_MANAGEMENT_FRIENDLY_NAME    L"Wireless Power Savings Setting"

//
// Format of Buffers for 802.11 Software Infrastructure IOCTLs
//
// All 802.11 Software Infrastructure IOCTLs will contain the following buffer format
// for querying or setting data unless explicitly mentioned otherwise:
//
// Every buffer will begin with -
//          GUID gDeviceId;
//          DOT11_IOCTL_REQ_TYPE dot11IoctlReqType;
//          Followed by a buffer same as the OID buffer to which the IOCTL maps to.
//
// If the IOCTL doesn't map to an OID and if there is an explicit mention that the buffer
// for that IOCTL will not begin in the format mentioned above then the buffer will begin
// with the structure defined for that IOCTL.
//
// If the IOCTL doesn't map to an OID and if there is no explicit mention about the buffer
// format to be used for that IOCTL, then the buffer for that IOCTL will begin in the format
// mentioned above and the inner buffer will point to the structure defined for that IOCTL.
//

typedef enum _DOT11_IOCTL_REQ_TYPE {
    dot11_ioctl_req_type_query = 0,
    dot11_ioctl_req_type_set = 1,
    dot11_ioctl_req_type_indicate = 2,
    dot11_ioctl_req_type_max = 3		// Not a real type. Defined as an upper bound.
} DOT11_IOCTL_REQ_TYPE, * PDOT11_IOCTL_REQ_TYPE;

#include <packon.h>
typedef struct _DOT11_IOCTL_OID_HEADER {
    DOT11_IOCTL_REQ_TYPE dot11IoctlReqType;
} DOT11_IOCTL_OID_HEADER, * PDOT11_IOCTL_OID_HEADER;
#include <packoff.h>

#define WSIDOT11_BASE_CTL_CODE FILE_DEVICE_NETWORK

#define WSIDOT11_CTL_CODE(function, method, access) \
    CTL_CODE(WSIDOT11_BASE_CTL_CODE, function, method, access)

//
// 802.11 Offload Engine
//

//
// Offload Capability IOCTLs
//

#define IOCTL_DOT11_OFFLOAD_CAPABILITY \
    WSIDOT11_CTL_CODE(0, METHOD_BUFFERED, FILE_READ_ACCESS)

#define IOCTL_DOT11_CURRENT_OFFLOAD_CAPABILITY \
    WSIDOT11_CTL_CODE(1, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

//
// Fragmentation/Defragmentation Offload
//

#define IOCTL_DOT11_MPDU_MAX_LENGTH \
    WSIDOT11_CTL_CODE(2, METHOD_BUFFERED, FILE_READ_ACCESS)

//
// 802.11 Configuration IOCTLs
//

//
// IOCTLs for Mandatory Functions
//

#define IOCTL_DOT11_OPERATION_MODE_CAPABILITY \
    WSIDOT11_CTL_CODE(3, METHOD_BUFFERED, FILE_READ_ACCESS)

#define IOCTL_DOT11_CURRENT_OPERATION_MODE \
    WSIDOT11_CTL_CODE(4, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DOT11_ATIM_WINDOW \
    WSIDOT11_CTL_CODE(5, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DOT11_SCAN_REQUEST \
    WSIDOT11_CTL_CODE(6, METHOD_BUFFERED, FILE_WRITE_ACCESS)

#define IOCTL_DOT11_CURRENT_PHY_TYPE \
    WSIDOT11_CTL_CODE(7, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DOT11_RESET_REQUEST \
    WSIDOT11_CTL_CODE(8, METHOD_BUFFERED, FILE_WRITE_ACCESS)

//
// IOCTLs for Optional Functions
//

#define IOCTL_DOT11_OPTIONAL_CAPABILITY \
    WSIDOT11_CTL_CODE(9, METHOD_BUFFERED, FILE_READ_ACCESS)

#define IOCTL_DOT11_CURRENT_OPTIONAL_CAPABILITY \
    WSIDOT11_CTL_CODE(10, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

//
// 802.11 MIB IOCTLs
//

//
// IOCTLs for dot11StationConfigEntry
//

#define IOCTL_DOT11_STATION_ID \
    WSIDOT11_CTL_CODE(11, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DOT11_MEDIUM_OCCUPANCY_LIMIT \
    WSIDOT11_CTL_CODE(12, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DOT11_CF_POLLABLE \
    WSIDOT11_CTL_CODE(13, METHOD_BUFFERED, FILE_READ_ACCESS)

#define IOCTL_DOT11_CFP_PERIOD \
    WSIDOT11_CTL_CODE(14, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DOT11_CFP_MAX_DURATION \
    WSIDOT11_CTL_CODE(15, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DOT11_POWER_MGMT_MODE \
    WSIDOT11_CTL_CODE(16, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // ULONG for ExtSTA

#define IOCTL_DOT11_OPERATIONAL_RATE_SET \
    WSIDOT11_CTL_CODE(17, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DOT11_BEACON_PERIOD \
    WSIDOT11_CTL_CODE(18, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DOT11_DTIM_PERIOD \
    WSIDOT11_CTL_CODE(19, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

//
// IOCTLs for Dot11PrivacyEntry
//

#define IOCTL_DOT11_WEP_ICV_ERROR_COUNT \
    WSIDOT11_CTL_CODE(20, METHOD_BUFFERED, FILE_READ_ACCESS)

//
// IOCTLs for dot11OperationEntry
//

#define IOCTL_DOT11_MAC_ADDRESS \
    WSIDOT11_CTL_CODE(21, METHOD_BUFFERED, FILE_READ_ACCESS)

#define IOCTL_DOT11_RTS_THRESHOLD \
    WSIDOT11_CTL_CODE(22, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DOT11_SHORT_RETRY_LIMIT \
    WSIDOT11_CTL_CODE(23, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DOT11_LONG_RETRY_LIMIT \
    WSIDOT11_CTL_CODE(24, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DOT11_FRAGMENTATION_THRESHOLD \
    WSIDOT11_CTL_CODE(25, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DOT11_MAX_TRANSMIT_MSDU_LIFETIME \
    WSIDOT11_CTL_CODE(26, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DOT11_MAX_RECEIVE_LIFETIME \
    WSIDOT11_CTL_CODE(27, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

//
// IOCTLs for dot11CountersEntry
//

#define IOCTL_DOT11_COUNTERS_ENTRY \
    WSIDOT11_CTL_CODE(28, METHOD_BUFFERED, FILE_READ_ACCESS)

//
// IOCTLs for dot11PhyOperationEntry
//

#define IOCTL_DOT11_SUPPORTED_PHY_TYPES \
    WSIDOT11_CTL_CODE(29, METHOD_BUFFERED, FILE_READ_ACCESS)

#define IOCTL_DOT11_CURRENT_REG_DOMAIN \
    WSIDOT11_CTL_CODE(30, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DOT11_TEMP_TYPE \
    WSIDOT11_CTL_CODE(31, METHOD_BUFFERED, FILE_READ_ACCESS)

//
// IOCTLs for dot11PhyAntennaEntry
//

#define IOCTL_DOT11_CURRENT_TX_ANTENNA \
    WSIDOT11_CTL_CODE(32, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DOT11_DIVERSITY_SUPPORT \
    WSIDOT11_CTL_CODE(33, METHOD_BUFFERED, FILE_READ_ACCESS)

#define IOCTL_DOT11_CURRENT_RX_ANTENNA \
    WSIDOT11_CTL_CODE(34, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

//
// IOCTLs for dot11PhyTxPowerEntry
//

#define IOCTL_DOT11_SUPPORTED_POWER_LEVELS \
    WSIDOT11_CTL_CODE(35, METHOD_BUFFERED, FILE_READ_ACCESS)

#define IOCTL_DOT11_CURRENT_TX_POWER_LEVEL \
    WSIDOT11_CTL_CODE(36, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

//
// IOCTLs for dot11PhyFHSSEntry
//
#define IOCTL_DOT11_HOP_TIME \
    WSIDOT11_CTL_CODE(37, METHOD_BUFFERED, FILE_READ_ACCESS)

#define IOCTL_DOT11_CURRENT_CHANNEL_NUMBER \
    WSIDOT11_CTL_CODE(38, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DOT11_MAX_DWELL_TIME \
    WSIDOT11_CTL_CODE(39, METHOD_BUFFERED, FILE_READ_ACCESS)

#define IOCTL_DOT11_CURRENT_DWELL_TIME \
    WSIDOT11_CTL_CODE(40, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DOT11_CURRENT_SET \
    WSIDOT11_CTL_CODE(41, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DOT11_CURRENT_PATTERN \
    WSIDOT11_CTL_CODE(42, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DOT11_CURRENT_INDEX \
    WSIDOT11_CTL_CODE(43, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

//
// IOCTLs for dot11PhyDSSSEntry
//

#define IOCTL_DOT11_CURRENT_CHANNEL \
    WSIDOT11_CTL_CODE(44, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DOT11_CCA_MODE_SUPPORTED \
    WSIDOT11_CTL_CODE(45, METHOD_BUFFERED, FILE_READ_ACCESS)

#define IOCTL_DOT11_CURRENT_CCA_MODE \
    WSIDOT11_CTL_CODE(46, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DOT11_ED_THRESHOLD \
    WSIDOT11_CTL_CODE(47, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

//
// IOCTLs for dot11PhyIREntry
//

#define IOCTL_DOT11_CCA_WATCHDOG_TIMER_MAX \
    WSIDOT11_CTL_CODE(48, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DOT11_CCA_WATCHDOG_COUNT_MAX \
    WSIDOT11_CTL_CODE(49, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DOT11_CCA_WATCHDOG_TIMER_MIN \
    WSIDOT11_CTL_CODE(50, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DOT11_CCA_WATCHDOG_COUNT_MIN \
    WSIDOT11_CTL_CODE(51, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

//
// IOCTLs for dot11RegDomainsSupportEntry
//

#define IOCTL_DOT11_REG_DOMAINS_SUPPORT_VALUE \
    WSIDOT11_CTL_CODE(52, METHOD_BUFFERED, FILE_READ_ACCESS)

//
// IOCTLs for dot11AntennaListEntry
//

#define IOCTL_DOT11_SUPPORTED_TX_ANTENNA \
    WSIDOT11_CTL_CODE(53, METHOD_BUFFERED, FILE_READ_ACCESS)

#define IOCTL_DOT11_SUPPORTED_RX_ANTENNA \
    WSIDOT11_CTL_CODE(54, METHOD_BUFFERED, FILE_READ_ACCESS)

#define IOCTL_DOT11_DIVERSITY_SELECTION_RX \
    WSIDOT11_CTL_CODE(55, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

//
// IOCTLs for dot11SupportedDataRatesTxEntry and dot11SupportedDataRatesRxEntry
//

#define IOCTL_DOT11_SUPPORTED_DATA_RATES_VALUE \
    WSIDOT11_CTL_CODE(56, METHOD_BUFFERED, FILE_READ_ACCESS)

//
// 802.11 Software Infrastructure Configuration IOCTLs
//

//
// IOCTLs for Mandatory Functions
//

#define IOCTL_DOT11_CURRENT_BSSID \
    WSIDOT11_CTL_CODE(57, METHOD_BUFFERED, FILE_READ_ACCESS)
    // DOT11_MAC_ADDRESS

#define IOCTL_DOT11_DESIRED_BSSID \
    WSIDOT11_CTL_CODE(58, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // DOT11_MAC_ADDRESS

#define IOCTL_DOT11_CURRENT_SSID \
    WSIDOT11_CTL_CODE(59, METHOD_BUFFERED, FILE_READ_ACCESS)
    // DOT11_SSID

#define IOCTL_DOT11_CURRENT_BSS_TYPE \
    WSIDOT11_CTL_CODE(60, METHOD_BUFFERED, FILE_READ_ACCESS)
    // DOT11_BSS_TYPE

#define IOCTL_DOT11_EXCLUDE_8021X \
    WSIDOT11_CTL_CODE(61, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // BOOL

#define IOCTL_DOT11_ASSOCIATE \
    WSIDOT11_CTL_CODE(62, METHOD_BUFFERED, FILE_WRITE_ACCESS)
    // No internal data buffer

#define IOCTL_DOT11_DISASSOCIATE \
    WSIDOT11_CTL_CODE(63, METHOD_BUFFERED, FILE_WRITE_ACCESS)
    // DOT11_DISASSOCIATE_REQUEST

#define IOCTL_DOT11_QUERY_ADAPTER_LIST \
    WSIDOT11_CTL_CODE(64, METHOD_OUT_DIRECT, FILE_READ_ACCESS)
    // Uses the below buffer format and not the default one.
    typedef struct _DOT11_QUERY_ADAPTER {
        GUID gDeviceId;
        DOT11_CURRENT_OPERATION_MODE dot11CurrentOpMode;
    } DOT11_QUERY_ADAPTER, * PDOT11_QUERY_ADAPTER;
    typedef struct _DOT11_QUERY_ADAPTER_LIST {
        ULONG uNumOfEntries;
        ULONG uTotalNumOfEntries;
        DOT11_QUERY_ADAPTER Dot11QueryAdapter[1];
    } DOT11_QUERY_ADAPTER_LIST, * PDOT11_QUERY_ADAPTER_LIST;

#define IOCTL_DOT11_QUERY_BSSID_LIST \
    WSIDOT11_CTL_CODE(65, METHOD_OUT_DIRECT, FILE_READ_ACCESS)
    typedef struct _DOT11_BSS_DESCRIPTION_LIST {
        ULONG uNumOfBytes;
        ULONG uTotalNumOfBytes;
        UCHAR ucBuffer[1];                          // Must be the last field.
    } DOT11_BSS_DESCRIPTION_LIST, * PDOT11_BSS_DESCRIPTION_LIST;

#define IOCTL_DOT11_SEND_8021X_PKT \
    WSIDOT11_CTL_CODE(66, METHOD_IN_DIRECT, FILE_WRITE_ACCESS)
    // Uses the below buffer format and not the default one.
    // DOT11_SEND_8021X_PKT

#define IOCTL_DOT11_RECEIVE_UPCALL \
    WSIDOT11_CTL_CODE(67, METHOD_OUT_DIRECT, FILE_READ_ACCESS)

    // Uses the below buffer format and not the default one.
    #define STATUS_DOT11_SCAN_CONFIRM                   1
    #define STATUS_DOT11_RESET_CONFIRM                  2
    #define STATUS_DOT11_8021X_PKT_SEND_CONFIRM         3
    #define STATUS_DOT11_8021X_PKT_RECV_INDICATE        4
    #define STATUS_DOT11_DISASSOCIATE_NOTIFICATION      5
    #define STATUS_DOT11_DEAUTH_NOTIFICATION            6
    #define STATUS_DOT11_AUTH_FAIL_NOTIFICATION         7
    #define STATUS_DOT11_ASSOCIATE_INDICATION           8
    #define STATUS_DOT11_DISASSOCIATE_INDICATION        9
    #define STATUS_DOT11_INTEGRITY_FAIL_NOTIFICATION    10

//
// Additional 802.11 Software Infrastructure MIB IOCTLs
//

//
// IOCTLs for dot11StationConfigEntry
//

#define IOCTL_DOT11_AUTHENTICATION_RESPONSE_TIME_OUT \
    WSIDOT11_CTL_CODE(68, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // ULONG (in TUs)

#define IOCTL_DOT11_PRIVACY_OPTION_IMPLEMENTED \
    WSIDOT11_CTL_CODE(69, METHOD_BUFFERED, FILE_READ_ACCESS)
    // BOOL

#define IOCTL_DOT11_DESIRED_SSID \
    WSIDOT11_CTL_CODE(70, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // DOT11_SSID

#define IOCTL_DOT11_DESIRED_BSS_TYPE \
    WSIDOT11_CTL_CODE(71, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // DOT11_BSS_TYPE

#define IOCTL_DOT11_ASSOCIATION_RESPONSE_TIME_OUT \
    WSIDOT11_CTL_CODE(72, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // ULONG (in TUs)

#define IOCTL_DOT11_DISASSOCIATED_PEER \
    WSIDOT11_CTL_CODE(73, METHOD_BUFFERED, FILE_READ_ACCESS)
    // DOT11_DISCONNECTED_PEER

#define IOCTL_DOT11_DEAUTHENTICATED_PEER \
    WSIDOT11_CTL_CODE(74, METHOD_BUFFERED, FILE_READ_ACCESS)
    // DOT11_DISCONNECTED_PEER

#define IOCTL_DOT11_AUTHENTICATION_FAILED_PEER \
    WSIDOT11_CTL_CODE(75, METHOD_BUFFERED, FILE_READ_ACCESS)
    // DOT11_DISCONNECTED_PEER


//
//IOCTLs for dot11AuthenticationAlgorithmsEntry
//

#define IOCTL_DOT11_AUTHENTICATION_ALGORITHM \
    WSIDOT11_CTL_CODE(76, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    typedef struct _DOT11_AUTH_ALGO_LIST {
        ULONG uNumOfEntries;
        ULONG uTotalNumOfEntries;
        DOT11_AUTH_ALGO dot11AuthAlgo[1];
    } DOT11_AUTH_ALGO_LIST, * PDOT11_AUTH_ALGO_LIST;

//
// IOCTLs for dot11WEPDefaultKeysEntry
//

#define IOCTL_DOT11_WEP_DEFAULT_KEY_VALUE \
    WSIDOT11_CTL_CODE(77, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    typedef struct _DOT11_WEP_KEY_TYPE {
        ULONG uKeyLength;
        UCHAR ucKey[1];                             // Must be the last field.
    } DOT11_WEP_KEY_TYPE, * PDOT11_WEP_KEY_TYPE;
    typedef struct _DOT11_WEP_KEY_VALUE {
        BOOL bPersist;
        ULONG uWEPKeyIndex;
        DWORD dwAlgorithm;
        DOT11_MAC_ADDRESS MacAddr;
        DOT11_ROW_STATUS WEPKeyStatus;
        DOT11_WEP_KEY_TYPE WEPKeyType;	            // Must be the last field.
    } DOT11_WEP_KEY_VALUE, * PDOT11_WEP_KEY_VALUE;

//
// IOCTLs for Dot11WEPKeyMappingsEntry
//

#define IOCTL_DOT11_WEP_KEY_MAPPING \
    WSIDOT11_CTL_CODE(78, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    typedef struct _DOT11_WEP_KEY_MAPPING {
        ULONG uWEPKeyMappingIndex;
        DOT11_MAC_ADDRESS WEPKeyMappingAddress;
        DWORD dwAlgorithm;
        union {
            DOT11_DIRECTION Direction;      // ExtSTA use Direction
            BOOL bWEPRowIsOutbound;         // STA/AP use bWEPRowIsOutbound
        };
        BOOL bStaticWEPKey;
        DOT11_ROW_STATUS WEPKeyMappingStatus;
        DOT11_WEP_KEY_TYPE dot11WEPKeyMappingType; // Must be the last parameter.
    } DOT11_WEP_KEY_MAPPING, * PDOT11_WEP_KEY_MAPPING;
    typedef struct _DOT11_KM_WEP_KEY_MAPPING {
        ULONG uNumOfEntries;
        ULONG uTotalNumOfEntries;
        DOT11_WEP_KEY_MAPPING dot11WEPKeyMapping[1];
    } DOT11_KM_WEP_KEY_MAPPING, * PDOT11_KM_WEP_KEY_MAPPING;

//
// IOCTLs for Dot11PrivacyEntry
//

#define IOCTL_DOT11_PRIVACY_INVOKED \
    WSIDOT11_CTL_CODE(79, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // BOOL

#define IOCTL_DOT11_WEP_DEFAULT_KEY_ID \
    WSIDOT11_CTL_CODE(80, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // ULONG (between 0 to 3)

#define IOCTL_DOT11_WEP_KEY_MAPPING_LENGTH \
    WSIDOT11_CTL_CODE(81, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // ULONG (between 0xA to 0xFFFFFFFF)

#define IOCTL_DOT11_EXCLUDE_UNENCRYPTED \
    WSIDOT11_CTL_CODE(82, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // BOOL

#define IOCTL_DOT11_WEP_EXCLUDED_COUNT \
    WSIDOT11_CTL_CODE(83, METHOD_BUFFERED, FILE_READ_ACCESS)
    // ULONG

//
// IOCTLs for dot11SMTnotification
//

#define IOCTL_DOT11_DISASSOCIATE_NOTIFICATION \
    WSIDOT11_CTL_CODE(84, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // BOOL

#define IOCTL_DOT11_DEAUTHENTICATE_NOTIFICATION \
    WSIDOT11_CTL_CODE(85, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // BOOL

#define IOCTL_DOT11_AUTHENTICATE_FAIL_NOTIFICATION \
    WSIDOT11_CTL_CODE(86, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // BOOL

//
// IOCTLs for dot11CountersEntry
//

#define IOCTL_DOT11_WEP_UNDECRYPTABLE_COUNT \
    WSIDOT11_CTL_CODE(87, METHOD_BUFFERED, FILE_READ_ACCESS)
    // ULONG

//
// IOCTLs for dot11GroupAddressesEntry
//

#define IOCTL_DOT11_GROUP_ADDRESS \
    WSIDOT11_CTL_CODE(88, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    typedef struct _DOT11_KM_GROUP_ADDRESS {
        ULONG uNumOfEntries;
        ULONG uTotalNumOfEntries;
        DOT11_GROUP_ADDRESS dot11GroupAddress[1];
    } DOT11_KM_GROUP_ADDRESS, * PDOT11_KM_GROUP_ADDRESS;

#define IOCTL_DOT11_CHECK_ADAPTER \
    WSIDOT11_CTL_CODE(89, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // Uses the below buffer format and not the default one.
    // DOT11_QUERY_ADAPTER

#define IOCTL_DOT11_8021X_STATE \
    WSIDOT11_CTL_CODE(90, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // BOOL

#define IOCTL_DOT11_8021X_FILTER \
    WSIDOT11_CTL_CODE(91, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // DOT11_8021X_FILTER
    typedef struct _DOT11_KM_8021X_FILTER {
        ULONG uNumOfEntries;
        ULONG uTotalNumOfEntries;
        DOT11_8021X_FILTER dot118021XFilter[1];
    } DOT11_KM_8021X_FILTER, * PDOT11_KM_8021X_FILTER;

#define IOCTL_DOT11_ASSOCIATION_INFO \
    WSIDOT11_CTL_CODE(92, METHOD_OUT_DIRECT, FILE_READ_ACCESS)
    // DOT11_ASSOCIATION_INFO

//
// IOCTLs for dot11PhyOFDMEntry
//

#define IOCTL_DOT11_CURRENT_FREQUENCY \
    WSIDOT11_CTL_CODE(93, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // ULONG

#define IOCTL_DOT11_TI_THRESHOLD \
    WSIDOT11_CTL_CODE(94, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // ULONG

#define IOCTL_DOT11_FREQUENCY_BANDS_SUPPORTED \
    WSIDOT11_CTL_CODE(95, METHOD_BUFFERED, FILE_READ_ACCESS)
    // ULONG

//
// IOCTLs for dot11PhyHRDSSSEntry
//

#define IOCTL_DOT11_SHORT_PREAMBLE_OPTION_IMPLEMENTED \
    WSIDOT11_CTL_CODE(96, METHOD_BUFFERED, FILE_READ_ACCESS)
    // BOOL

#define IOCTL_DOT11_PBCC_OPTION_IMPLEMENTED \
    WSIDOT11_CTL_CODE(97, METHOD_BUFFERED, FILE_READ_ACCESS)
    // BOOL

#define IOCTL_DOT11_CHANNEL_AGILITY_PRESENT \
    WSIDOT11_CTL_CODE(98, METHOD_BUFFERED, FILE_READ_ACCESS)
    // BOOL

#define IOCTL_DOT11_CHANNEL_AGILITY_ENABLED \
    WSIDOT11_CTL_CODE(99, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // BOOL

#define IOCTL_DOT11_HR_CCA_MODE_SUPPORTED \
    WSIDOT11_CTL_CODE(100, METHOD_BUFFERED, FILE_READ_ACCESS)
    // ULONG

//
// IOCTls for NIC Specific
//

#define IOCTL_DOT11_NIC_SPECIFIC_EXTENSION \
    WSIDOT11_CTL_CODE(101, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // DOT11_NIC_SPECIFIC_EXTENSION

//
// IOCTLs for dot11StationConfigEntry (Cont from above)
// Support of 802.11d
//

#define IOCTL_DOT11_MULTI_DOMAIN_CAPABILITY_IMPLEMENTED \
    WSIDOT11_CTL_CODE(102, METHOD_BUFFERED, FILE_READ_ACCESS)
    // BOOL

#define IOCTL_DOT11_MULTI_DOMAIN_CAPABILITY_ENABLED \
    WSIDOT11_CTL_CODE(103, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // BOOL

#define IOCTL_DOT11_COUNTRY_STRING \
    WSIDOT11_CTL_CODE(104, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // DOT11_COUNTRY_OR_REGION_STRING


//
// IOCTLs for dot11MultiDomainCapabilityEntry
// Support of 802.11d
//

#define IOCTL_DOT11_MULTI_DOMAIN_CAPABILITY \
    WSIDOT11_CTL_CODE(105, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // DOT11_MD_CAPABILITY_ENTRY_LIST

//
// IOCTLs for dot11PhyFHSSEntry (Con't from above)
// Support of 802.11d
//

#define IOCTL_DOT11_EHCC_PRIME_RADIX \
    WSIDOT11_CTL_CODE(106, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // ULONG

#define IOCTL_DOT11_EHCC_NUMBER_OF_CHANNELS_FAMILY_INDEX \
    WSIDOT11_CTL_CODE(107, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // ULONG

#define IOCTL_DOT11_EHCC_CAPABILITY_IMPLEMENTED \
    WSIDOT11_CTL_CODE(108, METHOD_BUFFERED, FILE_READ_ACCESS)
    // BOOL

#define IOCTL_DOT11_EHCC_CAPABILITY_ENABLED \
    WSIDOT11_CTL_CODE(109, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // BOOL

#define IOCTL_DOT11_HOP_ALGORITHM_ADOPTED \
    WSIDOT11_CTL_CODE(110, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // DOT11_HOP_ALGO_ADOPTED
    #define HOP_ALGORITHM_CURRENT       0
    #define HOP_ALGORITHM_HOP_INDEX     1
    #define HOP_ALGORITHM_HCC           2

#define IOCTL_DOT11_RANDOM_TABLE_FLAG \
    WSIDOT11_CTL_CODE(111, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // BOOL

#define IOCTL_DOT11_NUMBER_OF_HOPPING_SETS \
    WSIDOT11_CTL_CODE(112, METHOD_BUFFERED, FILE_READ_ACCESS)
    // ULONG

#define IOCTL_DOT11_HOP_MODULUS \
    WSIDOT11_CTL_CODE(113, METHOD_BUFFERED, FILE_READ_ACCESS)
    // ULONG

#define IOCTL_DOT11_HOP_OFFSET \
    WSIDOT11_CTL_CODE(114, METHOD_BUFFERED, FILE_READ_ACCESS)
    // ULONG

//
// IOCTLs for dot11HoppingPatternEntry
//

#define IOCTL_DOT11_HOPPING_PATTERN \
    WSIDOT11_CTL_CODE(115, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // DOT11_HOPPING_PATTERN_ENTRY_LIST

#define IOCTL_DOT11_ASSOCIATION_IDLE_TIMEOUT \
    WSIDOT11_CTL_CODE(116, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // ULONG

#define IOCTL_DOT11_NIC_POWER_STATE \
    WSIDOT11_CTL_CODE(117, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // BOOL

//
// IOCTLs for dot11SSN
//

#define IOCTL_DOT11_UNICAST_CIPHER_ALGORITHM \
    WSIDOT11_CTL_CODE(119, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    typedef struct _DOT11_CIPHER_ALGO_LIST {
        ULONG uNumOfEntries;
        ULONG uTotalNumOfEntries;
        DOT11_CIPHER_ALGO dot11CipherAlgo[1];
    } DOT11_CIPHER_ALGO_LIST, * PDOT11_CIPHER_ALGO_LIST;

#define IOCTL_DOT11_MULTICAST_CIPHER_ALGORITHM \
    WSIDOT11_CTL_CODE(120, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // DOT11_CIPHER_ALGO_LIST

//
// IOCTLs for Repeater AP
//

#define IOCTL_DOT11_REPEATER_AP     \
    WSIDOT11_CTL_CODE(121, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // BOOL

#define IOCTL_DOT11_REPEATER_WEP_DEFAULT_KEY_VALUE \
    WSIDOT11_CTL_CODE(122, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // DOT11_WEP_KEY_VALUE

#define IOCTL_DOT11_REPEATER_WEP_DEFAULT_KEY_ID \
    WSIDOT11_CTL_CODE(123, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // ULONG (between 0 to 3)

#define IOCTL_DOT11_REPEATER_AUTHENTICATION_ALGORITHM \
    WSIDOT11_CTL_CODE(125, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // DOT11_AUTH_ALGO_LIST


//
// IOCTLs for RSSI
//

#define IOCTL_DOT11_RSSI \
    WSIDOT11_CTL_CODE(126, METHOD_BUFFERED, FILE_READ_ACCESS)
    // ULONG

//
// IOCTLs for AP Max Assocations Allowed
//

#define IOCTL_DOT11_MAX_ASSOCIATIONS \
    WSIDOT11_CTL_CODE(127, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // ULONG

//
// IOCTLs for BSS Basic Rate Set
//
#define IOCTL_DOT11_BSS_BASIC_RATE_SET \
    WSIDOT11_CTL_CODE(128, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

//
// IOCTLs for start and stop AP
//
#define IOCTL_DOT11_AP_STATE \
    WSIDOT11_CTL_CODE(129, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // BOOL

//
// IOCTLs for RF Usage
//
#define IOCTL_DOT11_RF_USAGE \
    WSIDOT11_CTL_CODE(130, METHOD_BUFFERED, FILE_READ_ACCESS)
    // ULONG

#define IOCTL_DOT11_MAC_FILTER_CONTROL_MODE \
    WSIDOT11_CTL_CODE(131, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // DOT11_MAC_FILTER_CONTROL_MODE

#define IOCTL_DOT11_MAC_FILTER_CONTROL_LIST \
    WSIDOT11_CTL_CODE(132, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    typedef struct _DOT11_MAC_FILTER_CONTROL_LIST {
        ULONG uNumOfEntries;
        ULONG uTotalNumOfEntries;
        DOT11_MAC_ADDRESS FilterControlMacAddress[1];
    } DOT11_MAC_FILTER_CONTROL_LIST, * PDOT11_MAC_FILTER_CONTROL_LIST;

#define IOCTL_DOT11_EXTD_ASSOC_INFO \
    WSIDOT11_CTL_CODE(133, METHOD_BUFFERED, FILE_READ_ACCESS)
    typedef struct _DOT11_KM_EXTD_ASSOC_INFO {
        ULONG uBufferLength;
        ULONG uTotalBufferLength;
        UCHAR ucBuffer[1];
    } DOT11_KM_EXTD_ASSOC_INFO, * PDOT11_KM_EXTD_ASSOC_INFO;


//
// IOCTLs for 802.11G
//
#define IOCTL_DOT11_DSSS_OFDM_IMPLEMENTED \
    WSIDOT11_CTL_CODE(134, METHOD_BUFFERED, FILE_READ_ACCESS)
    // BOOL

#define IOCTL_DOT11_DSSS_OFDM_MODE \
    WSIDOT11_CTL_CODE(135, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // DOT11_DSSS_OFDM_MODE

#define IOCTL_DOT11_SHORT_SLOT_TIME_IMPLEMENTED \
    WSIDOT11_CTL_CODE(136, METHOD_BUFFERED, FILE_READ_ACCESS)
    // BOOL

#define IOCTL_DOT11_SHORT_SLOT_TIME_MODE \
    WSIDOT11_CTL_CODE(137, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // DOT11_SHORT_SLOT_TIME_MODE

#define IOCTL_DOT11_ERP_PBCC_OPTION_IMPLEMENTED \
    WSIDOT11_CTL_CODE(138, METHOD_BUFFERED, FILE_READ_ACCESS)
    // BOOL

//
// IOCTLs for association admission to stations with short preamble only
//
#define IOCTL_DOT11_SHORT_PREAMBLE_ONLY \
    WSIDOT11_CTL_CODE(139, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // BOOL

#define IOCTL_DOT11_ENUM_ASSOC_HISTORY \
    WSIDOT11_CTL_CODE(140, METHOD_OUT_DIRECT, FILE_READ_ACCESS)
typedef enum DOT11_DISASSOC_ORIGINATOR {

    STA_ASSOC_HISTORY_ASSOCIATED,

    // disassociated by a DISASSOCIATION request from the AP
    STA_ASSOC_HISTORY_DISASSOC_REQUEST,

    // disassociated by a DEAUTH request from the AP
    STA_ASSOC_HISTORY_DEAUTH_REQUEST,

    // disassociated by an IOCTL disassoc request from upper layer
    STA_ASSOC_HISTORY_IOCTL_DISASSOCIATE,

    // disassociated by an IOCTL assoc request from upper layer
    STA_ASSOC_HISTORY_IOCTL_ASSOCIATE,

    // disassociated by IOCTL reset or internal reset
    STA_ASSOC_HISTORY_RESET,

    // disassociated by roaming engine
    STA_ASSOC_HISTORY_ROAMING,

    // disassociated by dead AP detection
    STA_ASSOC_HISTORY_DEAD_AP_DETECTION,

    // disassociated because of 1X state has change
    STA_ASSOC_HISTORY_1X_STATE_CHANGE,

    // disassociated because of new session
    STA_ASSOC_HISTORY_NEW_SESSION

} DOT11_DISASSOC_ORIGINATOR, * PDOT11_DISASSOC_ORIGINATOR;

typedef struct DOT11_ASSOC_HISTORY_ENTRY {
    // MAC address of this entry
    DOT11_MAC_ADDRESS MacAddress;

    // the time when station is associated with the AP
    LONGLONG liAssocTime;

    // the time when station is disassociated with the AP
    LONGLONG liDisAssocTime;

    LONGLONG liDuration;

    // who originates the disassociation
    DOT11_DISASSOC_ORIGINATOR DisassocOriginator;

    // the smoothed RSSI value at the disassociating time
    ULONG uDisAssocRSSI;

    // the smoothed RSSI value at the time we decide to roam
    ULONG uRoamingRSSI;

    // applicable only for STA_ASSOC_HISTORY_DISASSOC_REQUEST
    // and STA_ASSOC_HISTORY_DEAD_AP_DETECTION
    USHORT usReason;
} DOT11_ASSOC_HISTORY_ENTRY, * PDOT11_ASSOC_HISTORY_ENTRY;

typedef struct DOT11_KM_ASSOC_HISTORY {
    ULONG uNumOfEntries;
    ULONG uTotalNumOfEntries;
    DOT11_ASSOC_HISTORY_ENTRY dot11AssocHistoryEntries[1];
} DOT11_KM_ASSOC_HISTORY, * PDOT11_KM_ASSOC_HISTORY;

//
// IOCTL for packet filter used by Network Monitor or other user mode applications
//
#define IOCTL_DOT11_USER_PACKET_FILTER \
    WSIDOT11_CTL_CODE(141, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
typedef struct DOT11_USER_PACKET_FILTER {
    // 802.11 packet filter requested by user
    ULONG uDot11UserPacketFilter;

    // whether decrypt packets or not
    BOOL bDecryptPackets;
} DOT11_USER_PACKET_FILTER, * PDOT11_USER_PACKET_FILTER;

//
// IOCTLs for WME
//
#define IOCTL_DOT11_WME_IMPLEMENTED \
    WSIDOT11_CTL_CODE(142, METHOD_BUFFERED, FILE_READ_ACCESS)
    // BOOL

#define IOCTL_DOT11_WME_ENABLED \
    WSIDOT11_CTL_CODE(143, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // BOOL

#define IOCTL_DOT11_WME_BEACON_AC_PARAMETER_RECORD \
    WSIDOT11_CTL_CODE(144, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    typedef struct DOT11_KM_WME_AC_PARAMETER_RECORD_LIST {
        ULONG uNumOfEntries;
        ULONG uTotalNumOfEntries;
        DOT11_WME_AC_PARAMETER_RECORD dot11WMEACParameterRecord[1];
    } DOT11_KM_WME_AC_PARAMETER_RECORD_LIST, * PDOT11_KM_WME_AC_PARAMETER_RECORD_LIST;

#define IOCTL_DOT11_WME_AP_AC_PARAMETER_RECORD \
    WSIDOT11_CTL_CODE(145, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // DOT11_KM_WME_AC_PARAMETER_RECORD_LIST

#define IOCTL_DOT11_WME_ADMISSION_REQUEST \
    WSIDOT11_CTL_CODE(146, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    typedef struct _DOT11_KM_WME_ADMISSION_REQUEST {
        ULONG uNumOfEntries;
        ULONG uTotalNumOfEntries;
        DOT11_WME_ADMISSION_REQUEST dot11WMEAdmissionRequest[1];
    } DOT11_KM_WME_ADMISSION_REQUEST, * PDOT11_KM_WME_ADMISSION_REQUEST;


//
// IOCTL for automatic frequency selection
//
#define IOCTL_DOT11_AUTO_FREQUENCY_SELECTION_ENABLED \
    WSIDOT11_CTL_CODE(147, METHOD_BUFFERED, FILE_READ_ACCESS)
    // BOOL

#define IOCTL_DOT11_REPEATER_DESIRED_BSSID \
    WSIDOT11_CTL_CODE(148, METHOD_BUFFERED, FILE_READ_ACCESS)
    // DOT11_MAC_ADDRESS

#define IOCTL_DOT11_HIDDEN_SSID_ENABLED \
    WSIDOT11_CTL_CODE(149, METHOD_BUFFERED, FILE_READ_ACCESS)
    // BOOL

#define IOCTL_DOT11_REPEATER_CONNECTION_STATE \
    WSIDOT11_CTL_CODE(150, METHOD_BUFFERED, FILE_READ_ACCESS)
    // BOOL

#define IOCTL_DOT11_SECURITY_PROVIDER_PARAMETERS \
    WSIDOT11_CTL_CODE(200, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // A BOOL Variable followed by DOT11_PRIVACY_EXEMPTION_LIST
    // The BOOL Variable indicates whether the provider is an
    // MS standard provider or an IHV provider.
    typedef struct DOT11_SEC_PROV_PARAMS {
        // TRUE when the security module is an IHV module
        BOOL bIHV: 1;

        // TRUE when the security module expects 802.11 packets
        BOOL bDot11Packet: 1;

        // The maximum number of incoming security packets
        // which can be queued. When uMaxBacklog is reached,
        // the oldest packet in the queue will be discarded.
        ULONG uMaxBacklog;

        // The exemption table and the registration table.
        //
        // The exemption table must come before the
        // registration table (to simplify buffer
        // validation)

        // Ethertype exemptions table. Each entry is DOT11_PRIVACY_EXEMPTION.
        //      Ethertype of packets which must be exempted.
        ULONG uNumOfExemptions;
        ULONG uExemptionsOffset;

        // Ethertype registation table. Each entry is a USHORT
        // ethertype (in network order)
        //      Ethertype of packets which the security module wants
        ULONG uNumOfRegistrations;
        ULONG uRegistrationsOffset;
    } DOT11_SEC_PROV_PARAMS, * PDOT11_SEC_PROV_PARAMS;

#define IOCTL_DOT11_ACTIVE_SECURITY_PROVIDER \
    WSIDOT11_CTL_CODE(201, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // ULONG
    //
    // The lowest 2-bit specifies the security/authentication provider
    //  0 - there is no authentication for the network
    //  1 - use MS 802.1X as the authentication service for the network
    //  2 - use IHV Service as the authentication service for the network
    //
    // The highest bit specifies whether IHV opaque channel should be deserialized or not.
    #define DOT11_AUTH_SERVICE_NONE             0
    #define DOT11_AUTH_SERVICE_MS_8021X         1
    #define DOT11_AUTH_SERVICE_IHV              2
    #define DOT11_AUTH_SERVICE_IHV_DESERIALIZED 0x80000000U

#define IOCTL_DOT11_EXTSTA_CAPABILITY \
    WSIDOT11_CTL_CODE(202, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DOT11_PORT_STATE \
    WSIDOT11_CTL_CODE(203, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // DOT11_PORT_STATE
    typedef struct DOT11_KM_PORT_STATE_ENTRY {
        DOT11_MAC_ADDRESS PeerMacAddress;   // Unicast mac address of the peer
        ULONG uSessionId;
        ULONG bControlled: 1;                // TRUE, if the port is controlled by 802.1X
        ULONG bAuthorized: 1;                // TRUE, if the port is authorized for data packets
        ULONG bAuthFailed: 1;                // TRUE, if the port is in AuthFailed state
        ULONG bReserved: 29;
    } DOT11_KM_PORT_STATE_ENTRY, * PDOT11_KM_PORT_STATE_ENTRY;
    typedef struct DOT11_KM_PORT_STATE {
        ULONG uNumOfEntries;
        ULONG uTotalNumOfEntries;
        DOT11_KM_PORT_STATE_ENTRY dot11PortState[1];
    } DOT11_KM_PORT_STATE, * PDOT11_KM_PORT_STATE;

//
//IOCTLs for dot11AuthCipherPair
//

#define IOCTL_DOT11_AUTH_CIPHER_PAIR_LIST \
    WSIDOT11_CTL_CODE(204, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // DOT11_AUTH_CIPHER_PAIR_LIST
    typedef struct DOT11_AUTH_CIPHER_PAIR_LIST_FILTER {
        // 4 combinations:
        //  <infra, ucast>
        //  <infra, mcast>
        //  <adhoc, ucast>
        //  <adhoc, mcast>
        BOOL bAdhoc;
        BOOL bMulticast;
    } DOT11_AUTH_CIPHER_PAIR_LIST_FILTER, * PDOT11_AUTH_CIPHER_PAIR_LIST_FILTER;

#define IOCTL_DOT11_CURRENT_PHY_ID \
    WSIDOT11_CTL_CODE(205, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // ULONG

#define IOCTL_DOT11_DESIRED_PHY_LIST \
    WSIDOT11_CTL_CODE(206, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // DOT11_PHY_ID_LIST

#define IOCTL_DOT11_PMKID_LIST \
    WSIDOT11_CTL_CODE(207, METHOD_BUFFERED, FILE_WRITE_ACCESS)
    // DOT11_PMKID_LIST

#define IOCTL_DOT11_SUPPORTED_COUNTRY_OR_REGION_STRING \
    WSIDOT11_CTL_CODE(208, METHOD_BUFFERED, FILE_READ_ACCESS)
    // DOT11_COUNTRY_OR_REGION_STRING_LIST

#define IOCTL_DOT11_DESIRED_COUNTRY_OR_REGION_STRING \
    WSIDOT11_CTL_CODE(209, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // DOT11_COUNTRY_OR_REGION_STRING

#define IOCTL_DOT11_IBSS_PARAMS \
    WSIDOT11_CTL_CODE(210, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // DOT11_IBSS_PARAMS

#define IOCTL_DOT11_MEDIA_STREAMING_ENABLED \
    WSIDOT11_CTL_CODE(211, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    // BOOLEAN

#define IOCTL_DOT11_STATISTICS \
    WSIDOT11_CTL_CODE(212, METHOD_BUFFERED, FILE_READ_ACCESS)
    // DOT11_STATISTICS

#define IOCTL_DOT11_GET_RESET_PERF_STATISTICS \
    WSIDOT11_CTL_CODE(213, METHOD_BUFFERED, FILE_READ_ACCESS)
    // DOT11_PERF_STATISTICS
    typedef struct DOT11_PERF_STATISTICS {
        ULONGLONG ullTotalTime;
        ULONGLONG ullBusyTime;
    } DOT11_PERF_STATISTICS, * PDOT11_PERF_STATISTICS;

#define IOCTL_DOT11_SET_PMK \
    WSIDOT11_CTL_CODE(214, METHOD_BUFFERED, FILE_WRITE_ACCESS)
    // DOT11_PMK_ENTRY
    #define DOT11_PMK_MAX_LENGTH            32

    // The PMK is valid for the lifetime of the connection
    #define PMK_EXPIRE_INFINITE             0xffffffffffffffffU
    typedef struct DOT11_PMK_ENTRY {
        // The MAC address of the peer with which the key is associated.
        //
        // If PeerMac is a broadcast address, the key is the default
        // PMK, which will match any peer. This is the case of WPA-PSK
        // and WPA2-PSK.
        DOT11_MAC_ADDRESS PeerMac;

        // The GMT time at which the key expires.
        // 
        // If its value is PMK_EXPIRE_INFINITE, the key is valid
        // for the life time of the connection and will be deleted
        // upon disconnecting from the network.
        //
        // If PeerMac is a broadcast address, ullExpiry must be PMK_EXPIRE_INFINITE.
        // Otherwise, the adding request will be failed.
        ULONGLONG ullExpiry;

        // PMKID if ullExpiry is not PMK_EXPIRE_INFINITE
        DOT11_PMKID_VALUE PMKID;

        ULONG uKeyLength;
        UCHAR ucKey[DOT11_PMK_MAX_LENGTH];
    } DOT11_PMK_ENTRY, * PDOT11_PMK_ENTRY;

#define IOCTL_DOT11_FLUSH_BSS_LIST \
    WSIDOT11_CTL_CODE(215, METHOD_BUFFERED, FILE_WRITE_ACCESS)

#define IOCTL_DOT11_HARDWARE_PHY_STATE \
    WSIDOT11_CTL_CODE(216, METHOD_BUFFERED, FILE_READ_ACCESS)

#define IOCTL_DOT11_PHY_ATTRIBUTES \
    WSIDOT11_CTL_CODE(217, METHOD_BUFFERED, FILE_READ_ACCESS)

#define IOCTL_DOT11_SAFE_MODE_ENABLED \
    WSIDOT11_CTL_CODE(218, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DOT11_HIDDEN_NETWORK_ENABLED \
    WSIDOT11_CTL_CODE(219, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DOT11_SAFE_MODE_IMPLEMENTED \
    WSIDOT11_CTL_CODE(220, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#ifdef UNDER_CE

#define IOCTL_DOT11_ADAPTIVE_CTRL_SETTINGS \
    WSIDOT11_CTL_CODE(221, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DOT11_OPEN_DEVICE \
    WSIDOT11_CTL_CODE(400, METHOD_BUFFERED, FILE_READ_ACCESS)

#define IOCTL_DOT11_REQUEST_NOTIFICATION \
    WSIDOT11_CTL_CODE(401, METHOD_BUFFERED, FILE_READ_ACCESS)
    typedef struct _DOT11_REQUEST_NOTIFICATION {
        HANDLE	hMsgQueue;
        DWORD	dwNotificationTypes;
    } DOT11_REQUEST_NOTIFICATION, * PDOT11_REQUEST_NOTIFICATION;
    
    #define DOT11_NOTIFICATION_DEVICE_UP        0x00000001
    #define DOT11_NOTIFICATION_DEVICE_DOWN      0x00000002
    #define DOT11_NOTIFICATION_ASYNC_RESULT     0x00000004

    typedef struct _DOT11_DEVICE_NOTIFICATION {
        DWORD	dwType;
        DWORD   dwParam;
        DWORD   dwError;
        UINT    dwBufferOffset;
        UINT    dwBufferSize;
    } DOT11_DEVICE_NOTIFICATION, *PDOT11_DEVICE_NOTIFICATION;

#define IOCTL_DOT11_CANCEL_READ \
    WSIDOT11_CTL_CODE(402, METHOD_BUFFERED, FILE_READ_ACCESS)

#define IOCTL_DOT11_CANCEL_IOCTL \
    WSIDOT11_CTL_CODE(403, METHOD_BUFFERED, FILE_READ_ACCESS)

#endif //UNDER_CE

//
// Definitions for RNWF Upcall (IRP_MJ_READ)
//
typedef enum DOT11_AUTH_UPCALL_TYPE {
    // DOT11_AUTH_UPCALL_TYPE_NDIS_INDICATION,

    DOT11_AUTH_UPCALL_TYPE_PORT_UP,
    DOT11_AUTH_UPCALL_TYPE_PORT_DOWN,
    DOT11_AUTH_UPCALL_TYPE_PACKET,

    // This event indicates to MSM that IM driver
    // has disconnected from the network. MSM (or
    // precisely autoconf) need to pick a new network
    // profile.
    // This event doesn't have any parameter.
    DOT11_AUTH_UPCALL_TYPE_DISCONNECT,

    DOT11_AUTH_UPCALL_TYPE_IHV,

    DOT11_AUTH_UPCALL_TYPE_TKIPMIC_FAILURE,

    DOT11_AUTH_UPCALL_TYPE_PMKID_CANDIDATE_LIST,

    DOT11_AUTH_UPCALL_TYPE_PHY_STATE_CHANGE,

    DOT11_AUTH_UPCALL_TYPE_LINK_QUALITY,

    // This is essentially the IRP connection completion.
    // The difference is that this is serialized against
    // other upcall message.
    DOT11_AUTH_UPCALL_TYPE_CONNECT_COMPLETION,

    // Kernel mode 4-way shake started
    DOT11_AUTH_UPCALL_TYPE_4_WAY_START,

    DOT11_AUTH_UPCALL_TYPE_4_WAY_COMPLETION,

    DOT11_AUTH_UPCALL_TYPE_PSK_MISMATCH,

    DOT11_UPCALL_QUICK_CONNECT
} DOT11_AUTH_UPCALL_TYPE, * PDOT11_AUTH_UPCALL_TYPE;

#ifndef __midl
    // push/pop doesn't work as expected.
    // #pragma warning(push)
    #pragma warning(disable:4200)
    typedef struct DOT11_AUTH_UPCALL_HEADER {
        DOT11_AUTH_UPCALL_TYPE UpcallType;
        ULONG uMsgSize;

        // A variable-sized DOT11_AUTH_UPCALL_BODY
        UCHAR ucMsg[0];
    } DOT11_AUTH_UPCALL_HEADER, * PDOT11_AUTH_UPCALL_HEADER;
    // #pragma warning(pop)
    #define DOT11_AUTH_UPCALL_HEADER_SIZE   (FIELD_OFFSET(DOT11_AUTH_UPCALL_HEADER,ucMsg))

    typedef struct DOT11_UPCALL_PORTDOWN_PARAMS {
        DOT11_MAC_ADDRESS MacAddr;
        ULONG uSessionId;

        DOT11_ASSOC_STATUS uReason;

        // AssocStatus code from the most recent assoc-completion indication
        DOT11_ASSOC_STATUS uLastAssocStatusCode;
    } DOT11_UPCALL_PORTDOWN_PARAMS, * PDOT11_UPCALL_PORTDOWN_PARAMS;

    typedef struct DOT11_UPCALL_PORTUP_PARAMS {
        DOT11_PORT_STATE PortState;

        // This must be the last field since AssocCmplIndication is variable size
        DOT11_ASSOCIATION_COMPLETION_PARAMETERS AssocCmplIndication;
    } DOT11_UPCALL_PORTUP_PARAMS, * PDOT11_UPCALL_PORTUP_PARAMS;

    typedef struct DOT11_UPCALL_CONNECT_COMPLETION {

        // Same as the NTSTATUS returned to IRP connection completion.
        NDIS_STATUS ntStatusConnectCompletion;

        // AssocStatus code from the most recent assoc-completion indication
        DOT11_ASSOC_STATUS uDot11Status;
    } DOT11_UPCALL_CONNECT_COMPLETION, * PDOT11_UPCALL_CONNECT_COMPLETION;

    typedef struct DOT11_UPCALL_4_WAY_START {
        DOT11_MAC_ADDRESS MacAddr;
        ULONG uSessionId;
    } DOT11_UPCALL_4_WAY_START, * PDOT11_UPCALL_4_WAY_START;

    typedef struct DOT11_UPCALL_4_WAY_COMPLETION {
        DOT11_MAC_ADDRESS MacAddr;
        ULONG uSessionId;
        DWORD ntStatus;

        // range between WLAN_REASON_CODE_MSMSEC_MIN and WLAN_REASON_CODE_MSMSEC_MAX
        DWORD dwL2ReasonCode;
    } DOT11_UPCALL_4_WAY_COMPLETION, * PDOT11_UPCALL_4_WAY_COMPLETION;

    typedef struct DOT11_UPCALL_PSK_MISMATCH {
        DOT11_MAC_ADDRESS MacAddr;
        ULONG uSessionId;
    } DOT11_UPCALL_PSK_MISMATCH, * PDOT11_UPCALL_PSK_MISMATCH;

    typedef union DOT11_AUTH_UPCALL_BODY {

        // PortUp and PortDown
        DOT11_UPCALL_PORTUP_PARAMS PortUp;
        DOT11_UPCALL_PORTDOWN_PARAMS PortDown;

        DOT11_PMKID_CANDIDATE_LIST_PARAMETERS PMKCandidateList;
        DOT11_TKIPMIC_FAILURE_PARAMETERS TkipMicFailure;
        DOT11_PHY_STATE_PARAMETERS PHYStateChange;

        // DOT11_UPCALL_NDIS_IND_PARAMS NdisIndicationParams;

        UCHAR ucPkt[0];
    } DOT11_AUTH_UPCALL_BODY, * PDOT11_AUTH_UPCALL_BODY;
#endif  // __midl

typedef enum _DOT11_ADAPTIVE_CTRL_RESTORE
{
    RESTORE_SETTINGS_NOT_SET = 0,
    RESTORE_SETTINGS_AT_ASSOCIATION,
    RESTORE_SETTINGS_AT_CONNECT_SUCCESS,
    RESTORE_SETTINGS_DURING_DISCONNECT
} DOT11_ADAPTIVE_CTRL_RESTORE;
typedef struct _DOT11_INTERNAL_REGISTRY_ADAPTIVE_CTRL
{
    DOT11_PUBLIC_REGISTRY_ADAPTIVE_CTRL publicRegistryAdaptiveControl;
    BOOL                                bReleaseIPOnSuspendResume;
    BOOL                                bEnableDelayReleasePowerOnState;
    DWORD                               dwMaxQuickConnectRetries;
    DWORD                               dwMaxScanResultErrorCount;
    BOOL                                bIntersilCard;
} DOT11_INTERNAL_REGISTRY_ADAPTIVE_CTRL, *PDOT11_INTERNAL_REGISTRY_ADAPTIVE_CTRL;
typedef struct _DOT11_ADAPTIVE_SCAN_CTRL
{
    BOOL    bUseSavedScanResults;
    BOOL    bCheckScanResultErrors;
    BOOL    bDisableFlushBssList;
    BOOL    bNotifyUIScanData;
    DWORD   dwScanResultErrorCount;
} DOT11_ADAPTIVE_SCAN_CTRL, *PDOT11_ADAPTIVE_SCAN_CTRL;
typedef struct _DOT11_ADAPTIVE_CONNECT_CTRL
{
    BOOL                        bForceReset;
    BOOL                        bForceResetFailed;
    DWORD                       dwQuickConnectTry;
    DOT11_ADAPTIVE_CTRL_RESTORE restoreSettingsAt;
} DOT11_ADAPTIVE_CONNECT_CTRL, *PDOT11_ADAPTIVE_CONNECT_CTRL;
typedef struct _DOT11_INTERNAL_ADAPTIVE_CTRL
{
    DOT11_INTERNAL_REGISTRY_ADAPTIVE_CTRL   storedRegistryAdaptiveControl;
    DOT11_INTERNAL_REGISTRY_ADAPTIVE_CTRL   currentAdjustedRegistryAdaptiveControl;
    DOT11_PUBLIC_NON_REGISTRY_ADAPTIVE_CTRL publicNonRegistryAdaptiveControl;
    DOT11_ADAPTIVE_SCAN_CTRL                adaptiveScanControl;
    DOT11_ADAPTIVE_CONNECT_CTRL             adaptiveConnectControl;
} DOT11_INTERNAL_ADAPTIVE_CTRL, *PDOT11_INTERNAL_ADAPTIVE_CTRL;

#ifdef NDIS_SUPPORT_NDIS6
    // This is a private indication between NWIFI and LLTD. It should be replaced
    // when we have a kernel mode WLAN API.

    #define NDIS_STATUS_DOT11_RESTART_INDICATION    ((NDIS_STATUS)0x4003FFFFL)
    typedef struct DOT11_EXTSTA_RESTART_INDICATION {
    #define DOT11_EXTSTA_RESTART_INDICATION_REVISION_1  1
        NDIS_OBJECT_HEADER Header;

        // Number of peer stations
        ULONG NumPeers;

        DOT11_BSS_TYPE CurrentBSSType;
        DOT11_SSID CurrentSSID;
        DOT11_MAC_ADDRESS CurrentBSSID;

        LONG RSSI;
        ULONG LinkQuality;
    } DOT11_EXTSTA_RESTART_INDICATION, * PDOT11_EXTSTA_RESTART_INDICATION;
#endif

#endif // __WSIDOT11_H__

