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
// Definitions and declarations of the basic WiFi types and translators.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WiFiTypes_h_
#define _DEFINED_WiFiTypes_h_
#pragma once

#ifdef UNDER_CE
#include <bldver.h>
#else
#include <inc/bldver.h>
#endif

#include <APCTypes.hpp>

// WZC support was added in CE 4.0:
//
#if (CE_MAJOR_VER >= 4)
#define WZC_VERSION 1
#else
#define WZC_VERSION 0
#endif

// WLAN and CM/CSP-WiFi support were added in Seven:
//
#if (CE_MAJOR_VER >= 7)
#define WLAN_VERSION   1
#define CMWIFI_VERSION 1
#else
#define WLAN_VERSION   0
#define CMWIFI_VERSION 0
#endif

// WLAN supports WPA2:
//
#if (WLAN_VERSION > 0)
#define WIFI_IMPLEMENTS_WPA2  1
#endif

// WZC may not support WPA2:
// (We include WZCTypes.hpp at the end of this file to decide.)

// Neither WZC nor WLAN support MD5 EAP authentication:
//
#undef WIFI_SUPPORTS_EAP_MD5

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Wireless configuration service types:
//
enum WiFiServiceType_e
{
    WiFiServiceWZC         = 1,     // Wireless Zero Configuration
    WiFiServiceWLAN        = 2,     // Native WiFi / AutoConfig
    WiFiServiceCM          = 3,     // AutoConfig with ConnMgr/CSPWiFi
    WiFiServiceConfigSP    = 4,     // Configuration Service Provider for WiFi
    WiFiServiceAny         = 9,     // Use any available wireless service
    NumberWiFiServiceTypes = 4      
};

// Translates the specified wireless configuration service into text form:
//
const TCHAR *
WiFiServiceType2Name(
    WiFiServiceType_e ServiceType);

const TCHAR *
WiFiServiceType2Desc(
    WiFiServiceType_e ServiceType);

// Translates the specified wireless configuration service from text form:
// Returns (WiFiServiceType_e)-1 on failure.
//
WiFiServiceType_e
WiFiServiceName2Type(
    const TCHAR *pServiceName);

WiFiServiceType_e
WiFiServiceDesc2Type(
    const TCHAR *pServiceDesc);

// ----------------------------------------------------------------------------
//
// Wireless interface connection status:
// (Order is important. Don't change without very good reason.)
//
enum WiFiConnectionStatus_e
{
    WiFiConnStatusIdle = 0,         // State machine initializing
    WiFiConnStatusDisconnected,     // WiFi hdwe idle or scanning
    WiFiConnStatusAssociating,      // Associating with AP or Ad Hoc
    WiFiConnStatusAssociated,       // Association completed
    WiFiConnStatusAuthenticating,   // Authenticating with AP or Ad Hoc
    WiFiConnStatusAuthenticated,    // Authentication completed
    WiFiConnStatusAwaitngAdHocPeer, // Waiting for connection by Ad Hoc peer
    WiFiConnStatusCorrectProfile,   // Associated with correct profile
    WiFiConnStatusAddressAssigned,  // IP address assigned by DHCP
    WiFiCOnnStatusMaximum
};

// Translates the specified interface conneciton status into text form:
//
const TCHAR *
WiFiConnectionStatus2Name(
    WiFiConnectionStatus_e ConnectionStatus);

const TCHAR *
WiFiConnectionStatus2Desc(
    WiFiConnectionStatus_e ConnectionStatus);

// Translates the specified interface connection status from text form:
// Returns (WiFiConnectionStatus_e)-1 on failure.
//
WiFiConnectionStatus_e
WiFiConnectionStatusName2Status(
    const TCHAR *pStatusName);

WiFiConnectionStatus_e
WiFiConnectionStatusDesc2Status(
    const TCHAR *pStatusDesc);

// ----------------------------------------------------------------------------
//
// EAP (Extensible Authentication Protocol) protocol types:
//
enum WiFiEapAuthMode_e
{
    WiFiEapAuthMD5      =  4,
    WiFiEapAuthTLS      = 13,
    WiFiEapAuthEAPSIM   = 18,
    WiFiEapAuthPEAP     = 25,
    WiFiEapAuthMSCHAPv2 = 26,
    NumberWiFiEapAuthModes = 5
};

// Translates the specified EAP authentication-mode into text form:
//
const TCHAR *
WiFiEapAuthMode2Name(
    WiFiEapAuthMode_e EapAuthMode);

const TCHAR *
WiFiEapAuthMode2Desc(
    WiFiEapAuthMode_e EapAuthMode);

// Translates the specified EAP authentication-mode from text form:
// Returns (WiFiEapAuthMode_e)-1 on failure.
//
WiFiEapAuthMode_e
WiFiEapAuthName2Mode(
    const TCHAR *pEapAuthName);

WiFiEapAuthMode_e
WiFiEapAuthDesc2Mode(
    const TCHAR *pEapAuthDesc);

// Translates between APController and WiFi EAP authentication modes:
// Defaults to TLS on failure.
//
WiFiEapAuthMode_e 
APEapAuthMode2WiFi(
    APEapAuthMode_e EapAuthMode);

APEapAuthMode_e
WiFiEapAuthMode2AP(
    WiFiEapAuthMode_e EapAuthMode);

// ----------------------------------------------------------------------------
//
// Network connection modes:
//
enum WiFiConnectMode_e
{
    WiFiConnectInfrastructure = 1, // Only connect to infrastructure (AP) networks
    WiFiConnectAdHoc          = 2, // Only connect to ad hoc networks
    WiFiConnectAny            = 3, // Connect to any available network
    NumberWiFiConnectModes = 3
};

// Translates the specified infrastructure-mode into text form:
//
const TCHAR *
WiFiConnectMode2Name(
    WiFiConnectMode_e InfraMode);

const TCHAR *
WiFiConnectMode2Desc(
    WiFiConnectMode_e InfraMode);

// Translates the specified infrastructure-mode from text form:
// Returns (DOT11_BSS_TYPE)-1 on failure.
//
WiFiConnectMode_e
WiFiConnectName2Mode(
    const TCHAR *pInfraName);

WiFiConnectMode_e
WiFiConnectDesc2Mode(
    const TCHAR *pInfraDesc);

// ----------------------------------------------------------------------------
//
// Miscellaneous:
//

// Maximum characters in a WiFi adapter name or GUID:
//
static const int MaxWiFiAdapterNameChars = 128;

// WiFi adapter name and GUID:
//
typedef struct __WiFiAdapterName
{
    TCHAR AdapterName[MaxWiFiAdapterNameChars];
    GUID  AdapterGuid;
    
} WiFiAdapterName,
*PWiFiAdapterName;

// List of WiFi adapter/GUID names:
//
typedef struct __WiFiAdapterNamesList
{
    DWORD NumberAdapterNames;
    
    WiFiAdapterName Names[1];

#ifdef UNDER_CE
    static DWORD
    CalculateLength(DWORD NumberNames)
    {
        return offsetof(WiFiAdapterNamesList, Names)
             + (sizeof(WiFiAdapterName) * NumberNames);
    }
    DWORD
    GetLength(void) const
    {
        return CalculateLength(NumberAdapterNames);
    }
#endif

} WiFiAdapterNamesList,
*PWiFiAdapterNamesList;

// Translates between 802.11 frequency (in kHz) and channel-number:
// Returns 0 for invalid frequency range or channel number.
//
int
ChannelFreq2Number(
    long Frequency);

long
ChannelNumber2Freq(
    int Channel);

// Formats the specified SSID into text form:
//
HRESULT
SSID2Text(         const BYTE  *pData,
                            DWORD   DataLen,
  __out_ecount(BufferChars) TCHAR *pBuffer,
                            int     BufferChars);

};
};

#include <wzctypes.hpp>

#endif /* _DEFINED_WiFiTypes_h_ */
// ----------------------------------------------------------------------------
