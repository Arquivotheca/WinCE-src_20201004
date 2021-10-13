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
// Definitions and declarations of the basic Wireless Zero Configuration
// types and translators.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WZCTypes_h_
#define _DEFINED_WZCTypes_h_
#pragma once

#include "WiFiTypes.hpp"

#include <ntddndis.h>

#if (WZC_VERSION > 0)

// WPA2 definitions for backwards-compatibility:
//
#ifndef INTF_ENTRY_V1
    #define WZC_INTF_ENTRY INTF_ENTRY
#else
    #define WZC_INTF_ENTRY INTF_ENTRY_EX
    #define WIFI_IMPLEMENTS_WPA2  1
#endif

#endif /* if (WZC_VERSION > 0) */

#include "eapol.h"

namespace ce {
namespace qa {

// Translates the specified authentication-mode into text form:
//
const TCHAR *
WZCAuthMode2Name(
    NDIS_802_11_AUTHENTICATION_MODE AuthMode);

const TCHAR *
WZCAuthMode2Desc(
    NDIS_802_11_AUTHENTICATION_MODE AuthMode);

// Translates the specified authentication-mode from text form:
// Returns (NDIS_802_11_AUTHENTICATION_MODE)-1 on failure.
//
NDIS_802_11_AUTHENTICATION_MODE
WZCAuthName2Mode(
    const TCHAR *pAuthName);

NDIS_802_11_AUTHENTICATION_MODE
WZCAuthDesc2Mode(
    const TCHAR *pAuthDesc);

// Translates between APController and WZC authenication modes:
// Defaults to Open authentication.
//
NDIS_802_11_AUTHENTICATION_MODE
APAuthMode2WZC(
    APAuthMode_e AuthMode);

APAuthMode_e
WZCAuthMode2AP(
    NDIS_802_11_AUTHENTICATION_MODE AuthMode);

// Translates the specified encryption-mode into text form:
//
const TCHAR *
WZCCipherMode2Name(
    NDIS_802_11_ENCRYPTION_STATUS CipherMode);

const TCHAR *
WZCCipherMode2Desc(
    NDIS_802_11_ENCRYPTION_STATUS CipherMode);

// Translates the specified encryption-mode from text form:
// Returns (NDIS_802_11_ENCRYPTION_STATUS)-1 on failure.
//
NDIS_802_11_ENCRYPTION_STATUS
WZCCipherName2Mode(
    const TCHAR *pCipherName);

NDIS_802_11_ENCRYPTION_STATUS
WZCCipherDesc2Mode(
    const TCHAR *pCipherDesc);

// Translates between APController and WZC encryption modes:
// Defaults to no encryption.
//
NDIS_802_11_ENCRYPTION_STATUS 
APCipherMode2WZC(
    APCipherMode_e CipherMode);

APCipherMode_e
WZCCipherMode2AP(
    NDIS_802_11_ENCRYPTION_STATUS CipherMode);

// Translates the specified infrastructure-mode into text form:
//
const TCHAR *
WZCInfrastructureMode2Name(
    NDIS_802_11_NETWORK_INFRASTRUCTURE InfraMode);

const TCHAR *
WZCInfrastructureMode2Desc(
    NDIS_802_11_NETWORK_INFRASTRUCTURE InfraMode);

// Translates the specified infrastructure-mode from text form:
// Returns (NDIS_802_11_NETWORK_INFRASTRUCTURE)-1 on failure.
//
NDIS_802_11_NETWORK_INFRASTRUCTURE
WZCInfrastructureName2Mode(
    const TCHAR *pInfraName);

NDIS_802_11_NETWORK_INFRASTRUCTURE
WZCInfrastructureDesc2Mode(
    const TCHAR *pInfraDesc);

// Translates between WiFi and WZC connection modes:
// Defaults to any network.
//
NDIS_802_11_NETWORK_INFRASTRUCTURE 
WiFiConnectMode2WZC(
    WiFiConnectMode_e ConnectMode);

WiFiConnectMode_e
WZCConnectMode2WiFi(
    NDIS_802_11_NETWORK_INFRASTRUCTURE ConnectMode);

// Translates the specified network-type into text form:
//
const TCHAR *
WZCNetworkType2Name(
    NDIS_802_11_NETWORK_TYPE NetworkType);

const TCHAR *
WZCNetworkType2Desc(
    NDIS_802_11_NETWORK_TYPE NetworkType);

// Translates the specified network-type from text form:
// Returns (NDIS_802_11_NETWORK_TYPE)-1 on failure.
//
NDIS_802_11_NETWORK_TYPE
WZCNetworkName2Type(
    const TCHAR *pNetworkName);

NDIS_802_11_NETWORK_TYPE
WZCNetworkDesc2Type(
    const TCHAR *pNetworkDesc);

// Translates the specified WEP-status code into text form:
//
const TCHAR *
WZCWEPStatus2Name(
    NDIS_802_11_WEP_STATUS WEPStatus);

const TCHAR *
WZCWEPStatus2Desc(
    NDIS_802_11_WEP_STATUS WEPStatus);

// Translates the specified WEP-status code from text form:
// Returns (NDIS_802_11_WEP_STATUS)-1 on failure.
//
NDIS_802_11_WEP_STATUS
WZCWEPName2Status(
    const TCHAR *pWEPStatusName);

NDIS_802_11_WEP_STATUS
WZCWEPDesc2Status(
    const TCHAR *pWEPStatusDesc);

};
};
#endif /* _DEFINED_WZCTypes_h_ */
// ----------------------------------------------------------------------------
