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


Module Name:
    eapol.h

Abstract:
    EAPOL interface definitions

--*/

#pragma once

#include <cxport.h>
#include <wincrypt.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SESSION_KEY_LENGTH  32

#define MAX_SSID_LEN                      32

//
//    EAPOL Registry configuration key and value names
//
#define EAPOL_REGISTRY_KEY                                    TEXT("Comm\\EAPOL")
#define EAPOL_REGISTRY_VALUENAME_MAX_START                    TEXT("MaxStart")
#define EAPOL_REGISTRY_VALUENAME_START_PERIOD                TEXT("StartPeriodSeconds")
#define EAPOL_REGISTRY_VALUENAME_START_DELAY_MS             TEXT("StartDelayMilliseconds")
#define EAPOL_REGISTRY_VALUENAME_AUTH_PERIOD                TEXT("AuthPeriodSeconds")
#define EAPOL_REGISTRY_VALUENAME_HELD_PERIOD                TEXT("HeldPeriodSeconds")
#define EAPOL_REGISTRY_VALUENAME_CONFIRM_IDENTITY            TEXT("ConfirmIdentityOnNewConnection")
#define EAPOL_REGISTRY_VALUENAME_WPA_PERIOD                 TEXT("WPAPeriodSeconds")
#define EAPOL_REGISTRY_VALUENAME_WPA_MAX_4_WAY_FAILURES     TEXT("WPAMax4WayHandshakeFailures")
#define EAPOL_REGISTRY_VALUENAME_WPA2_MAX_4_WAY_FAILURES     TEXT("WPA2Max4WayHandshakeFailures")

//    Per-SSID connection information is stored in the
//    registry key:
//
//    [HKLM\Comm\EAPOL\Config\<SSID>]
//        "Enable8021x"=dword
//        "DisplayMessage"="Request Identity Display Message"
//        "EapTypeId"=dword
//        \<EapTypeId>
//            ConnectionData=binary
//
#define EAPOL_REGISTRY_KEY_CONFIG                    TEXT("Comm\\EAPOL\\Config")
#define EAPOL_REGISTRY_VALUENAME_ENABLE_8021X        TEXT("Enable8021x")
#define EAPOL_REGISTRY_VALUENAME_DISPLAY_MESSAGE    TEXT("DisplayMessage")
#define EAPOL_REGISTRY_VALUENAME_IDENTITY            TEXT("Identity")
#define EAPOL_REGISTRY_VALUENAME_PASSWORD            TEXT("Password")
#define EAPOL_REGISTRY_VALUENAME_LAST_AUTH_SUCCESS    TEXT("LastAuthSuccessful")
#define EAPOL_REGISTRY_VALUENAME_EAPTYPE            TEXT("EapTypeId")
#define EAPOL_REGISTRY_VALUENAME_CONNECTIONDATA        TEXT("ConnectionData")
#define EAPOL_REGISTRY_VALUENAME_USERDATA            TEXT("UserData")

//
// EAPOL packet types
//
typedef enum _EAPOL_PACKET_TYPE 
{
    EAPOL_EAP_Packet = 0,              
    EAPOL_Start,            
    EAPOL_Logoff,          
    EAPOL_Key            
} EAPOL_PACKET_TYPE;

//
//    EAPOL PDU format
//

typedef struct _EAPOL_PDU
{
    BYTE    ProtocolVersion;
#define            EAPOL_PROTOCOL_VERSION            1
    BYTE    PacketType;
    BYTE    PacketBodyLength[2];

    //
    //    Following the header is the PacketBody,
    //    which is PacketBodyLength bytes long.
    //
} EAPOL_PDU, *PEAPOL_PDU;


//
//  Structure: EAPOL_PACKET
//  Should be exactly the same as EAPOL_PDU.
//  EAPOL_PACKET is used by EAPOL SSN.
//

typedef struct _EAPOL_PACKET 
{
    BYTE        ProtocolVersion;
    BYTE        PacketType;
    BYTE        PacketBodyLength[2];
    BYTE        PacketBody[1];
} EAPOL_PACKET, *PEAPOL_PACKET;


#define EAPOL_PDU_HEADER_SIZE    sizeof(EAPOL_PDU)

#define REPLAY_COUNTER_LENGTH    8
#define KEY_SIGNATURE_LENGTH    16
#define KEY_IV_LENGTH            16

typedef struct _EAPOL_KEY_DESCRIPTOR 
{
    BYTE        DescriptorType;
    BYTE        KeyLength[2];                            // Key length in octets, MSB first
    BYTE        ReplayCounter[REPLAY_COUNTER_LENGTH];
    BYTE        Key_IV[KEY_IV_LENGTH];
    BYTE        KeyIndex;
    BYTE        KeySignature[KEY_SIGNATURE_LENGTH];
    BYTE        Key[1];
} EAPOL_KEY_DESC, *PEAPOL_KEY_DESC;

//
// Format of EAPOL_KEY_DESC.Key field when
//    DescriptorType == EAPOL_KEY_DESCRIPTOR_TYPE_PER_STA
//
typedef struct _EAPOL_KEY_MATERIAL
{
    BYTE        KeyMaterialLength[2];
    BYTE        KeyMaterial[1];
} EAPOL_KEY_MATERIAL, *PEAPOL_KEY_MATERIAL;


////////////////////////////////////////////////////////////////////////////////
//  EAPOL SSN support.
////////////////////////////////////////////////////////////////////////////////

#define SIZE_NONCE                  32
#define SIZE_ETHERNET_TYPE          2

//
// Structure:   EAPOL_KEY
// This structure holds a buffer used for storing the PTK
//

typedef struct _EAPOL_KEYS
{
    DWORD                   cbTemporalKey;
    BYTE                    pbMICKey[16];
    BYTE                    pbEncryptionKey[16];
    BYTE                    pbTemporalKey[MAX_SESSION_KEY_LENGTH];
} EAPOL_KEYS, *PEAPOL_KEYS;

#define MAX_SSNIE_LENGTH            255

typedef struct _SSN_DATA 
{
    BYTE                    pbMICKey[16];
    BYTE                    pbEncryptionKey[16];
    EAPOL_KEYS              pbTempPTK;
    DWORD                   dwSizeOfSSNIE;
    BYTE                    pbSSNIE[MAX_SSNIE_LENGTH];
    ULONGLONG               ullLastSSNReplayCounter;
    USHORT                  usKeyDescription;
} SSN_DATA, *PSSN_DATA;


#define EAPOL_KEY_DESC_VERSION          0x0000
#define SSN_KEY_DESC_VERSION_WEP        0x0001
#define SSN_KEY_DESC_VERSION_TKIP       0x0001
#define SSN_KEY_DESC_VERSION_AES        0x0002

#define SSN_KEY_TYPE_PAIRWISE           0x0008
#define SSN_KEY_TYPE_GROUP              0x0000      // bit 3 = 0 ==> Group key

#define SSN_KEY_DESC_VERSION_BITS       0x0007
#define SSN_KEY_DESC_KEY_TYPE_BIT       0x0008
#define SSN_KEY_DESC_KEY_INDEX_BITS     0x0030
#define SSN_KEY_DESC_KEY_USAGE_BIT      0x0040
#define SSN_KEY_DESC_ACK_BIT            0x0080
#define SSN_KEY_DESC_MIC_BIT            0x0100
#define SSN_KEY_DESC_SECURE_BIT         0x0200
#define SSN_KEY_DESC_ERROR_BIT          0x0400
#define SSN_KEY_DESC_REQUEST_BIT        0x0800
#define SSN_KEY_DESC_RESERVED_BITS      0xF000

#define SSN_KEY_DESC_INDEX_BITS_OFFSET  4

typedef struct _EAPOL_KEY_SSN_DESCRIPTOR
{
    BYTE                    DescriptorType;
    BYTE                    KeyInfo[2];
    BYTE                    KeyLength[2];
    BYTE                    ReplayCounter[8];
    BYTE                    KeyNonce[SIZE_NONCE];
    BYTE                    Key_IV[16];
    BYTE                    Key_RSC[8];
    BYTE                    Key_ID[8];    
    BYTE                    KeyMIC[16];
    BYTE                    KeyMaterialLength[2];
    BYTE                    KeyMaterial[1];
} EAPOL_KEY_SSN_DESC, *PEAPOL_KEY_SSN_DESC;

#define SIZE_MAC_ADDR               6

#define EAPOL_KEY_DESC_RC4          1
#define EAPOL_KEY_DESC_WPA2         2
#define EAPOL_KEY_DESC_WPA          254
#define EAPOL_KEY_DESC_RSN          EAPOL_KEY_DESC_WPA2
#define EAPOL_KEY_DESC_SSN          EAPOL_KEY_DESC_WPA

#define MAX_KEY_DESC                EAPOL_KEY_DESC_SSN


#ifdef __cplusplus
}
#endif

