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
#ifndef __NDT_LIB_WLAN_H
#define __NDT_LIB_WLAN_H

#include <ntddndis.h>

//------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------

#ifndef OID_802_11_MEDIA_STREAM_MODE
// For things to compile
#define OID_802_11_MEDIA_STREAM_MODE (OID_802_11_RELOAD_DEFAULTS+4)

typedef enum _NDIS_802_11_MEDIA_STREAM_MODE
{
    Ndis802_11MediaStreamOff,
    Ndis802_11MediaStreamOn
} NDIS_802_11_MEDIA_STREAM_MODE, *PNDIS_802_11_MEDIA_STREAM_MODE;

#endif

// until wpa2 supoort is added
#ifndef OID_802_11_CAPABILITY

typedef struct _NDIS_802_11_AUTHENTICATION_ENCRYPTION
{
  NDIS_802_11_AUTHENTICATION_MODE  AuthModeSupported;
  NDIS_802_11_ENCRYPTION_STATUS  EncryptStatusSupported;
} NDIS_802_11_AUTHENTICATION_ENCRYPTION, *PNDIS_802_11_AUTHENTICATION_ENCRYPTION;

typedef struct _NDIS_802_11_CAPABILITY
{
  ULONG  Length;
  ULONG  Version;
  ULONG  NoOfPMKIDs;
  ULONG  NoOfAuthEncryptPairsSupported;
  NDIS_802_11_AUTHENTICATION_ENCRYPTION AuthenticationEncryptionSupported[1];
} NDIS_802_11_CAPABILITY, *PNDIS_802_11_CAPABILITY;


#define OID_802_11_PMKID                            0x0D010123

typedef UCHAR NDIS_802_11_PMKID_VALUE[16];

#endif

typedef struct _BSSIDInfo
{
  NDIS_802_11_MAC_ADDRESS  BSSID;
  NDIS_802_11_PMKID_VALUE  PMKID;
} BSSIDInfo, *PBSSIDInfo;

#ifndef OID_802_11_CAPABILITY

typedef struct _NDIS_802_11_PMKID
{
  ULONG  Length;
  ULONG  BSSIDInfoCount;
  BSSIDInfo BSSIDInfo[1];
} NDIS_802_11_PMKID, *PNDIS_802_11_PMKID;

const UINT Ndis802_11AuthModeWPA2 = Ndis802_11AuthModeWPANone + 1;
const UINT Ndis802_11AuthModeWPA2PSK = Ndis802_11AuthModeWPA2+1;

#define OID_802_11_CAPABILITY                   0x0D010122


#endif


// For the tests
typedef enum _NDIS_802_11_DEVICE_TYPE
{
    WLAN_DEVICE_TYPE_802_11B,
    WLAN_DEVICE_TYPE_802_11G,
    WLAN_DEVICE_TYPE_802_11AB,
    WLAN_DEVICE_TYPE_802_11AG,
    WLAN_DEVICE_TYPE_802_11A
} NDIS_802_11_DEVICE_TYPE, *PNDIS_802_11_DEVICE_TYPE;

//------------------------------------------------------------------------------

HRESULT NDTWlanGetNetworkTypeInUse(HANDLE hAdapter, 
    PDWORD pdwNetworkTypeInUse);
HRESULT NDTWlanSetNetworkTypeInUse(HANDLE hAdapter,
    DWORD dwNetworkTypeInUse);
HRESULT NDTWlanGetNetworkTypesSuppported(HANDLE hAdapter,
    PVOID pdwNetworkTypesInUse, PDWORD pdwBufSize);
HRESULT NDTWlanAssociate(HANDLE hAdapter, DWORD dwInfrastructureMode, 
      DWORD dwAuthenticationMode, PDWORD pdwEncryption, ULONG ulKeyLength,
      ULONG ulKeyIndex, PBYTE pbKeyMaterial, NDIS_802_11_MAC_ADDRESS* pBssid, 
      NDIS_802_11_SSID ssid, BOOL bIsBssid, PULONG pulTimeout);
HRESULT NDTWPAAssociate(HANDLE hAdapter, DWORD dwInfrastructureMode, 
      DWORD dwAuthenticationMode, DWORD dwEncryption, 
      NDIS_802_11_MAC_ADDRESS* pBssid, NDIS_802_11_SSID ssid, BOOL bIsBssid, 
      PULONG pulTimeout);
HRESULT NDTWlanDisassociate(HANDLE hAdapter);
HRESULT NDTWlanGetInfrastructureMode(HANDLE hAdapter, 
    PDWORD pdwInfrastructureMode);
HRESULT NDTWlanSetInfrastructureMode(HANDLE hAdapter, 
    DWORD dwInfrastructureMode);
HRESULT NDTWlanGetAuthenticationMode(HANDLE hAdapter, 
    PDWORD pdwAuthenticationMode);
HRESULT NDTWlanSetAuthenticationMode(HANDLE hAdapter, 
    DWORD dwAuthenticationMode);
HRESULT NDTWlanGetEncryptionStatus(HANDLE hAdapter, 
    PDWORD pdwEncryptionStatus);
HRESULT NDTWlanSetEncryptionStatus(HANDLE hAdapter, 
    DWORD dwEncryptionStatus);
HRESULT NDTWlanGetPowerMode(HANDLE hAdapter, PDWORD pdwPowerMode);
HRESULT NDTWlanSetPowerMode(HANDLE hAdapter, PDWORD pdwPowerMode);
HRESULT NDTWlanGetSsid(HANDLE hAdapter, PNDIS_802_11_SSID psSSID);
HRESULT NDTWlanSetSsid(HANDLE hAdapter, PNDIS_802_11_SSID psSSID);
HRESULT NDTWlanGetBssid(HANDLE hAdapter, NDIS_802_11_MAC_ADDRESS* pbMac);
HRESULT NDTWlanSetBssid(HANDLE hAdapter, NDIS_802_11_MAC_ADDRESS* pbMac);
HRESULT NDTWlanSetRemoveKey(HANDLE hAdapter, 
    PNDIS_802_11_REMOVE_KEY psRemoveKey);
HRESULT NDTWlanSetBssidListScan(HANDLE hAdapter);
HRESULT NDTWlanGetBssidList(HANDLE hAdapter, 
    PNDIS_802_11_BSSID_LIST_EX psBSSIDListEx, PDWORD pdwSize);
HRESULT NDTWlanSetAddKey(HANDLE hAdapter, 
    PNDIS_802_11_KEY psKey, DWORD dwSize);
HRESULT NDTWlanGetAssociationInformation(HANDLE hAdapter, 
    PNDIS_802_11_ASSOCIATION_INFORMATION psAssoInfo,PDWORD pdwSize);
HRESULT NDTWlanSetAssociationInformation(HANDLE hAdapter, 
    PNDIS_802_11_ASSOCIATION_INFORMATION psAssoInfo,DWORD dwSize);
HRESULT NDTWlanSetReloadDefaults(HANDLE hAdapter, DWORD dwDefaults);
HRESULT NDTWlanGetConfiguration(HANDLE hAdapter, 
    PNDIS_802_11_CONFIGURATION psConfig, PDWORD pdwSize);
HRESULT NDTWlanSetConfiguration(HANDLE hAdapter, 
    PNDIS_802_11_CONFIGURATION psConfig, DWORD dwSize);
HRESULT NDTWlanGetCapability(HANDLE hAdapter,
    PNDIS_802_11_CAPABILITY psCapability,PDWORD pdwSize);
HRESULT NDTWlanGetPMKID(HANDLE hAdapter, PNDIS_802_11_PMKID psPmkid
      ,PDWORD pdwSize);
HRESULT NDTWlanSetPMKID(HANDLE hAdapter, PNDIS_802_11_PMKID psPmkid
     , DWORD dwSize);
HRESULT NDTWlanGetStatistics(HANDLE hAdapter, 
    PNDIS_802_11_STATISTICS psStats, PDWORD pdwSize);
HRESULT NDTWlanSetAddWep(HANDLE hAdapter, PNDIS_802_11_WEP psWEPKey
     , DWORD dwSize);
HRESULT NDTWlanSetRemoveWep(HANDLE hAdapter, DWORD dwKeyIndex);
HRESULT NDTWlanGetMediaStreamMode(HANDLE hAdapter, 
    PDWORD pdwMediaStreamMode);
HRESULT NDTWlanSetMediaStreamMode(HANDLE hAdapter, 
    DWORD dwMediaStreamMode);
HRESULT NDTWlanReset(HANDLE hAdapter,
    BOOL bRemoveKeys);
HRESULT NDTWlanInitializeTest(HANDLE hAdapter, BOOL bSupportCard=FALSE);
HRESULT NDTWlanCleanupTest(HANDLE hAdapter);
HRESULT NDTWlanGetDeviceType(HANDLE hAdapter, 
    PNDIS_802_11_DEVICE_TYPE pnWlanDeviceType);
HRESULT NDTWlanGetCardAddress(HANDLE hAdapter, 
    NDIS_802_11_MAC_ADDRESS* pbMac);
HRESULT NDTWlanIsWPASupported(HANDLE hAdapter, PBOOL pbResult);
HRESULT NDTWlanIsWPAAdhocSupported(HANDLE hAdapter, PBOOL pbResult);
HRESULT NDTWlanIsWPA2Supported(HANDLE hAdapter, PBOOL pbResult);
HRESULT NDTWlanIsAESSupported(HANDLE hAdapter, PBOOL pbResult);
HRESULT NDTWlanIsMediaStreamSupported(HANDLE hAdapter, PBOOL pbResult);
HRESULT NDTWlanGetNumPKIDs( HANDLE hAdapter, PULONG pulNumPKIDs);

HRESULT NDTWlanDirectedSend(HANDLE hSender, HANDLE hReceiver,ULONG cbAddr,
   NDIS_MEDIUM nNdisMedium, 
   ULONG ulPacketsToSend, PULONG pulPacketsSent, PULONG pulPacketsReceived);
HRESULT NDTWlanBroadcastSend(HANDLE hSender, HANDLE hReceiver, 
    ULONG cbAddr,NDIS_MEDIUM nNdisMedium,ULONG ulPacketsToSend, 
    PULONG pulPacketsSent, PULONG pulPacketsReceived);
HRESULT NDTWlanCreateIBSS(HANDLE hAdapter,  
      DWORD dwAuthenticationMode, DWORD dwEncryption, ULONG ulKeyLength,
      ULONG ulKeyIndex, PBYTE pbKeyMaterial, NDIS_802_11_MAC_ADDRESS* pBssid, 
      NDIS_802_11_SSID ssid);
HRESULT NDTWlanJoinIBSS(HANDLE hAdapter, DWORD dwAuthenticationMode, 
      DWORD dwEncryption, ULONG ulKeyLength, ULONG ulKeyIndex, PBYTE pbKeyMaterial,
      NDIS_802_11_MAC_ADDRESS* pBssid,   NDIS_802_11_SSID ssid, PULONG pulTimeout);
HRESULT NDTWlanGetEvents(HANDLE hAdapter);
HRESULT NDTWlanSetDummySsid(HANDLE hAdapter);
HRESULT NDTWlanPrintBssidList(HANDLE hdapter);

// This will enable (dwAction=0) or disable (dwAction=1) wzcsvc.
HRESULT NDTWlanChangeWzcsvc(LPCTSTR szAdapter, DWORD dwAction);
    
// Wlan test specific constants
const char WLAN_RADIO_ON_SSID[] = "NDTEST_RADIO_ON_SSID";
const BYTE WLAN_KEYTYPE_DEFAULT[]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
const char WLAN_KEY_TKIP[]={"0123456789012345678901234567890123456789012345678901234567890123"};
const char WLAN_KEY_AES[]={"01234567890123456789012345678901"};

#if 0
const UCHAR WLAN_WEP_AP1[] = "NDTEST_WEP_AP1";
const UCHAR WLAN_WEP_AP2[] = "NDTEST_WEP_AP2";
const UCHAR WLAN_WEP_AP3[] = "NDTEST_WEP_AP3";
const UCHAR WLAN_WPA_AP1[] = "NDTEST_WPA_AP1";
const UCHAR WLAN_WPA2_AP1[]= "NDTEST_WPA2_AP1";
const UCHAR WLAN_IBSS[] = "NDTEST_IBSS";
#else
const char *g_apszWlanSsid[];  // initialized in lib/wlantestutil.cxx
const int WLAN_WEP_AP1  = 0;
const int WLAN_WEP_AP2  = 1;
const int WLAN_WEP_AP3  = 2;
const int WLAN_WPA_AP1  = 3;
const int WLAN_WPA2_AP1 = 4;
const int WLAN_IBSS     = 5;

void         SetSsidSuffix(const TCHAR *pszSuffix);
const char*  GetSsid(int iWhich);
void         InitSsid(int iWhich, NDIS_802_11_SSID& ssid);

#endif


const UCHAR WLAN_KEY_WEP[] = {'0','1','2','3','4','5','6','7','8','9'};
const UCHAR WLAN_DEFAULT_KEY1[] = {'0','1','2','3','4','5','6','7','8','9'};
const UCHAR WLAN_DEFAULT_KEY2[] = {'9','8','7','6','5','4','3','2','1','0'};
const UCHAR WLAN_DEFAULT_KEY3[] = {'1','0','2','0','3','0','4','0','5','0'};
const UCHAR WLAN_DEFAULT_KEY4[] = {'5','0','4','0','3','0','2','0','1','0'};
const UCHAR WLAN_KEY_TEST[]={'1','2','3','4','5','6','7','8','9','0'};


//const UCHAR WLAN_PRESHARED_KEY[] = {0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9};

const ULONG WLAN_BSSID_LIST_SCAN_TIMEOUT=(1000 * 6);
const ULONG WLAN_ASSOCIATE_TIMEOUT=15;
const ULONG WLAN_PERCENT_TO_PASS_DIRECTED=85;
const ULONG WLAN_PERCENT_TO_PASS_BROADCAST=60;

// WPA definitions
const ULONG WLAN_HANDSHAKE_TIMEOUT= 10;  //seconds

const UCHAR WLAN_PRESHARED_KEY[] = {'0','1','2','3','4','5','6','7','8','9'};
const UCHAR WLAN_DEFAULT_PMK_PASSWORD[] = "0123456789";

const ULONG WLAN_IE_WPA=221;

// WPA2 definitions
const ULONG WLAN_IE_RSN= 48;

const ULONG WLAN_WPA2_CAPABILITY_VERSION=1;
const ULONG WLAN_MAX_NUM_PMKIDS=16;
const ULONG WLAN_MIN_NUM_PMKIDS=3;

//------------------------------------------------------------------------------

// General utility functions required by wlan tests

void GetSsidText(TCHAR szSsidText[],NDIS_802_11_SSID ssid);
void GetBssidText(TCHAR czBssidText[], BYTE bssid[]);
void GetBssidExText(TCHAR czBssidText[], NDIS_WLAN_BSSID_EX nBssid);
void GetNetworkTypeText(TCHAR szText[], DWORD WlanNetworkType);
void GetPowerModeText(TCHAR szText[], DWORD dwPowerMode);
void GetAuthModeText(TCHAR szText[], DWORD dwAuthMode);
void GetEncryptionText(TCHAR szText[], DWORD dwEncryption);
void GetInfrastructureModeText(TCHAR szText[], DWORD dwInfrastructureMode);


//------------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

//------------------------------------------------------------------------------

#endif
