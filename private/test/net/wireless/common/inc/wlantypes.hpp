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
// Definitions and declarations of the basic WLAN types and translators.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WLANTypes_hpp_
#define _DEFINED_WLANTypes_hpp_
#pragma once

#include <WiFiTypes.hpp>
#if (WLAN_VERSION > 0)

#include <wlanapi.h>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Authentication mode:
//

// Translates the specified authentication-mode into text form:
//
const TCHAR *
WLANAuthMode2Name(
    DOT11_AUTH_ALGORITHM AuthMode);

const TCHAR *
WLANAuthMode2Desc(
    DOT11_AUTH_ALGORITHM AuthMode);

// Translates the specified authentication-mode from text form:
// Returns (DOT11_AUTH_ALGORITHM)-1 on failure.
//
DOT11_AUTH_ALGORITHM
WLANAuthModeName2Mode(
    const TCHAR *pModeName);

DOT11_AUTH_ALGORITHM
WLANAuthModeDesc2Mode(
    const TCHAR *pModeDesc);

// Translates between APController and WLAN authenication modes:
// Defaults to Open authentication.
//
DOT11_AUTH_ALGORITHM
APAuthMode2WLAN(
    APAuthMode_e AuthMode);

APAuthMode_e
WLANAuthMode2AP(
    DOT11_AUTH_ALGORITHM AuthMode);

// ----------------------------------------------------------------------------
//
// Encryption mode:
//

// Translates the specified encryption-mode into text form:
//
const TCHAR *
WLANCipherMode2Name(
    DOT11_CIPHER_ALGORITHM CipherMode);

const TCHAR *
WLANCipherMode2Desc(
    DOT11_CIPHER_ALGORITHM CipherMode);

// Translates the specified encryption-mode from text form:
// Returns (DOT11_CIPHER_ALGORITHM)-1 on failure.
//
DOT11_CIPHER_ALGORITHM
WLANCipherModeName2Mode(
    const TCHAR *pModeName);

DOT11_CIPHER_ALGORITHM
WLANCipherModeDesc2Mode(
    const TCHAR *pModeDesc);

// Translates between APController and WLAN encryption modes:
// Defaults to no encryption.
//
DOT11_CIPHER_ALGORITHM
APCipherMode2WLAN(
    APCipherMode_e CipherMode);

APCipherMode_e
WLANCipherMode2AP(
    DOT11_CIPHER_ALGORITHM CipherMode);

// ----------------------------------------------------------------------------
//
// BSS (Network) Type:
//

// Translates the specified BSS-type into text form:
//
const TCHAR *
WLANBssType2Name(
    DOT11_BSS_TYPE BssType);

const TCHAR *
WLANBssType2Desc(
    DOT11_BSS_TYPE BssType);

// Translates the specified BSS-type from text form:
// Returns (DOT11_BSS_TYPE)-1 on failure.
//
DOT11_BSS_TYPE
WLANBssTypeName2Type(
    const TCHAR *pTypeName);

DOT11_BSS_TYPE
WLANBssTypeDesc2Type(
    const TCHAR *pTypeDesc);

// Translates between WiFi-generic connection-mode and WLAN BSS-type:
// Defaults to ANY.
//
DOT11_BSS_TYPE
WiFiConnectMode2WLANBssType(
    WiFiConnectMode_e ConnectMode);

WiFiConnectMode_e
WLANBssType2WiFiConnectMode(
    DOT11_BSS_TYPE BssType);

// ----------------------------------------------------------------------------
//
// Connection Mode:
//

// Translates the specified connection-mode into text form:
//
const TCHAR *
WLANConnectionMode2Name(
    WLAN_CONNECTION_MODE ConnectionMode);

const TCHAR *
WLANConnectionMode2Desc(
    WLAN_CONNECTION_MODE ConnectionMode);

// Translates the specified connection-mode from text form:
// Returns (WLAN_CONNECTION_MODE)-1 on failure.
//
WLAN_CONNECTION_MODE
WLANConnectionModeName2Mode(
    const TCHAR *pModeName);

WLAN_CONNECTION_MODE
WLANConnectionModeDesc2Mode(
    const TCHAR *pModeDesc);

// ----------------------------------------------------------------------------
//
// Interface state:
//

// Translates the specified interface-state type into text form:
//
const TCHAR *
WLANInterfaceState2Name(
    WLAN_INTERFACE_STATE InterfaceState);

const TCHAR *
WLANInterfaceState2Desc(
    WLAN_INTERFACE_STATE InterfaceState);

// Translates the specified interface-state type from text form:
// Returns (WLAN_INTERFACE_STATE)-1 on failure.
//
WLAN_INTERFACE_STATE
WLANInterfaceStateName2State(
    const TCHAR *pStateName);

WLAN_INTERFACE_STATE
WLANInterfaceStateDesc2State(
    const TCHAR *pStateDesc);

// ----------------------------------------------------------------------------
//
// Notification code:
//

// Translates the specified notification code into text form:
//
const TCHAR *
WLANNotificationCode2Name(
    DWORD NotificationSource,
    DWORD NotificationCode);

const TCHAR *
WLANNotificationCode2Desc(
    DWORD NotificationSource,
    DWORD NotificationCode);

// Translates the specified notification code from text form:
// Returns (DWORD)-1 on failure.
//
DWORD
WLANNotificationCodeName2Code(
    DWORD NotificationSource,
    const TCHAR *pCodeName);

DWORD
WLANNotificationCodeDesc2Code(
    DWORD NotificationSource,
    const TCHAR *pCodeDesc);

// ----------------------------------------------------------------------------
//
// Notification source:
//

// Translates the specified notification source into text form:
//
const TCHAR *
WLANNotificationSource2Name(
    DWORD NotificationSource);

const TCHAR *
WLANNotificationSource2Desc(
    DWORD NotificationSource);

// Translates the specified notification source from text form:
// Returns (DWORD)-1 on failure.
//
DWORD
WLANNotificationSourceName2Source(
    const TCHAR *pSourceName);

DWORD
WLANNotificationSourceDesc2Source(
    const TCHAR *pSourceDesc);

// ----------------------------------------------------------------------------
//
// Interface type:
//

// Translates the specified interface type into text form:
//
const TCHAR *
WLANInterfaceType2Name(
    WLAN_INTERFACE_TYPE InterfaceType);

const TCHAR *
WLANInterfaceType2Desc(
    WLAN_INTERFACE_TYPE InterfaceType);

// Translates the specified interface type from text form:
// Returns (WLAN_INTERFACE_TYPE)-1 on failure.
//
WLAN_INTERFACE_TYPE
WLANInterfaceTypeName2Type(
    const TCHAR *pTypeName);

WLAN_INTERFACE_TYPE
WLANInterfaceTypeDesc2Type(
    const TCHAR *pTypeDesc);

// ----------------------------------------------------------------------------
//
// Query op-code value type:
//

// Translates the specified query value type into text form:
//
const TCHAR *
WLANOpCodeValueType2Name(
    WLAN_OPCODE_VALUE_TYPE OpCodeValueType);

const TCHAR *
WLANOpCodeValueType2Desc(
    WLAN_OPCODE_VALUE_TYPE OpCodeValueType);

// Translates the specified query value type from text form:
// Returns (WLAN_OPCODE_VALUE_TYPE)-1 on failure.
//
WLAN_OPCODE_VALUE_TYPE
WLANOpCodeValueTypeName2Type(
    const TCHAR *pTypeName);

WLAN_OPCODE_VALUE_TYPE
WLANOpCodeValueTypeDesc2Type(
    const TCHAR *pTypeDesc);

// ----------------------------------------------------------------------------
//
// Physical-medium type:
//

// Translates the specified physical-medium type into text form:
//
const TCHAR *
WLANPhysicalType2Name(
    DOT11_PHY_TYPE PhysicalType);

const TCHAR *
WLANPhysicalType2Desc(
    DOT11_PHY_TYPE PhysicalType);

// Translates the specified physical-medium type from text form:
// Returns (DOT11_PHY_TYPE)-1 on failure.
//
DOT11_PHY_TYPE
WLANPhysicalTypeName2Type(
    const TCHAR *pTypeName);

DOT11_PHY_TYPE
WLANPhysicalTypeDesc2Type(
    const TCHAR *pTypeDesc);

// ----------------------------------------------------------------------------
//
// Power setting:
//

// Translates the specified power setting into text form:
//
const TCHAR *
WLANPowerSetting2Name(
    WLAN_POWER_SETTING PowerSetting);

const TCHAR *
WLANPowerSetting2Desc(
    WLAN_POWER_SETTING PowerSetting);

// Translates the specified power setting from text form:
// Returns (WLAN_POWER_SETTING)-1 on failure.
//
WLAN_POWER_SETTING
WLANPowerSettingName2Setting(
    const TCHAR *pSettingName);

WLAN_POWER_SETTING
WLANPowerSettingDesc2Setting(
    const TCHAR *pSettingDesc);

// ----------------------------------------------------------------------------
//
// Radio-state:
//

// Translates the specified interface-state type into text form:
//
const TCHAR *
WLANRadioState2Name(
    DOT11_RADIO_STATE RadioState);

const TCHAR *
WLANRadioState2Desc(
    DOT11_RADIO_STATE RadioState);

// Translates the specified interface-state type from text form:
// Returns (DOT11_RADIO_STATE)-1 on failure.
//
DOT11_RADIO_STATE
WLANRadioStateName2State(
    const TCHAR *pStateName);

DOT11_RADIO_STATE
WLANRadioStateDesc2State(
    const TCHAR *pStateDesc);

};
};

#endif /* if (WLAN_VERSION > 0) */
#endif /* _DEFINED_WLANTypes_hpp_ */
// ----------------------------------------------------------------------------
