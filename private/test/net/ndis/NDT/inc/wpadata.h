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
#ifndef _WPADATA_H
#define _WPADATA_H

#define SUPPLICANT_SUCCESS                   0x00000000
#define SUPPLICANT_FAILURE                   0x00000001
#define SUPPLICANT_HANDSHAKE_COMPLETE        0x00000002
#define SUPPLICANT_GROUP_KEY_ADDED           0x00000003

#define MAX_BOUND_INTERFACES              10
#define MAX_NDISUIO_DEVICE_NAME_LEN       255
#define MAC_ADDR_LEN                      6
#define EAPOL_8021P_TAG_TYPE              0x0081
#define SIZE_NONCE                        32
#define SIZE_ETHERNET_TYPE                2
#define MAX_SESSION_KEY_LENGTH            32
#define MAX_MIC_KEY_LENGTH                16
#define MAX_SSID_LEN                      32
#define MAX_KEY_MATERIAL_LEN              32

#define EAPOL_KEY_DESCRIPTOR_TYPE_WPA     254
#define EAPOL_KEY_DESCRIPTOR_TYPE_WPA2    2

#define RSN_KEY_DATA_ENCAPSULATION_TAG    0xDD
#define RSN_KEY_DATA_OUI_TYPE_GKEY        0x00000001

static UCHAR RSN_KEY_DATA_OUI[3] = {0x00,0x0F,0xAC}; 

#define GTK_INDEX_BITS                    0x0003
#define GTK_TX_BITS                       0x0004

#define SSN_KEY_DESC_VERSION_TKIP         0x0001
#define SSN_KEY_DESC_VERSION_AES          0x0002

//#define SSN_KEY_DESC_VERSION2             0x0002

#define SSN_KEY_TYPE_PAIRWISE             0x0008
#define SSN_KEY_TYPE_GROUP                0x0000      // bit 3 = 0 ==> Group key
#define SSN_KEY_DESC_VERSION_BITS         0x0007
#define SSN_KEY_DESC_KEY_TYPE_BIT         0x0008
#define SSN_KEY_DESC_KEY_INDEX_BITS       0x0030
#define SSN_KEY_DESC_KEY_USAGE_BIT        0x0040
#define SSN_KEY_DESC_ACK_BIT              0x0080
#define SSN_KEY_DESC_MIC_BIT              0x0100
#define SSN_KEY_DESC_SECURE_BIT           0x0200
#define SSN_KEY_DESC_ERROR_BIT            0x0400
#define SSN_KEY_DESC_REQUEST_BIT          0x0800
#define SSN_KEY_DESC_ENCRYPT_BIT          0x1000
#define SSN_KEY_DESC_RESERVED_BITS        0xF000


#define SSN_KEY_DESC_INDEX_BITS_OFFSET  4

#define KCK_LENGTH                        16
#define KEK_LENGTH                        16

typedef struct _PRESHARED_KEY
{
   ULONG KeyLength;
   BYTE  KeyMaterial[MAX_KEY_MATERIAL_LEN];
}
PRESHARED_KEY, *PPRESHARED_KEY;

typedef struct _PAIRWISE_TEMPEROL_KEY
{
    DWORD   KeyLength;
    BYTE    MICKey[16];
    BYTE    EncryptionKey[16];
    BYTE    Key[MAX_SESSION_KEY_LENGTH];
} 
PAIRWISE_TEMPEROL_KEY, *PPAIRWISE_TEMPEROL_KEY;

typedef enum _EAPOL_PACKET_TYPE 
{
    EAP_Packet = 0,              
    EAPOL_Start,            
    EAPOL_Logoff,          
    EAPOL_Key            
}
EAPOL_PACKET_TYPE;

typedef struct _EAPOL_PACKET 
{
    BYTE        EtherType[2];
    BYTE        ProtocolVersion;
    BYTE        PacketType;
    BYTE        PacketBodyLength[2];
    BYTE        PacketBody[1];
}
EAPOL_PACKET, *PEAPOL_PACKET;

typedef struct _WPA_KEY_DESCRIPTOR
{
    BYTE    DescriptorType;
    BYTE    KeyInformation[2];
    BYTE    KeyLength[2];
    BYTE    ReplayCounter[8];
    BYTE    KeyNonce[SIZE_NONCE];
    BYTE    KeyIV[16];
    BYTE    KeyRSC[8];
    BYTE    KeyID[8];
    BYTE    KeyMIC[16];
    BYTE    KeyDataLength[2];
    BYTE    KeyData[1];
} 
WPA_KEY_DESCRIPTOR, *PWPA_KEY_DESCRIPTOR;

typedef struct _NDISUIO_PACKET
{
   BYTE  DestinationAddr[MAC_ADDR_LEN];
   BYTE  SourceAddr[MAC_ADDR_LEN];
   BYTE  EtherType[2];
}
NDISUIO_PACKET, *PNDISUIO_PACKET;

// This is an ethernet header structure minus the ethertype 
typedef struct _ETHERNET_HEADER
{
    BYTE   DestinationAddr[MAC_ADDR_LEN];
    BYTE   SourceAddr[MAC_ADDR_LEN];
}
ETHERNET_HEADER, *PETHERNET_HEADER;

typedef struct _INFO_ELEMENT
{
   BYTE  id;
   BYTE  length;
   BYTE  data[1];
}
INFO_ELEMENT, *PINFO_ELEMENT;

typedef struct _AUTH_CONTEXT
{
   PRESHARED_KEY           PreSharedKey;
   BYTE                    SupplicantAddress[MAC_ADDR_LEN];
   BYTE                    SNonce[SIZE_NONCE];
   BYTE                    AuthenticatorAddress[MAC_ADDR_LEN];
   BYTE                    ANonce[SIZE_NONCE];
   PAIRWISE_TEMPEROL_KEY   PTK;
   LONG                    GroupKeyIndex;

   //NDIS_802_11_KEY         PairwiseKey;
   //NDIS_802_11_KEY         GroupKey;

   INFO_ELEMENT*           pIE;
}
AUTH_CONTEXT, *PAUTH_CONTEXT;

typedef struct _THREADINFO
{
   HANDLE                  NdisUio;
   WCHAR                   DeviceName[MAX_NDISUIO_DEVICE_NAME_LEN];
   BOOL                    KillThread;
   BOOL                    HandshakeSuccessful;
   BOOL                    IsGroupKeyActive;
  // BOOL                    AddPairwiseKey;
   //BOOL                    AddGroupKey;
   BOOL                    IsThreadRunning;
}
THREADINFO, *PTHREADINFO;
               
#endif
