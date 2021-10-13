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
// Definitions and declarations of the basic Connection Manager - CSP WiFi
// types and translators.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_CMWiFiTypes_hpp_
#define _DEFINED_CMWiFiTypes_hpp_
#pragma once

#include <WiFiTypes.hpp>
#if (CMWIFI_VERSION > 0)

#include <CMNet.h>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Connection Manager connection-characteristic options:
//

// Translates the specified connection-characteristic option into text form.
//
const TCHAR *
CMWiFiConnectCharacteristic2Name(
    CM_CHARACTERISTIC ConnectCharacteristic);

const TCHAR *
CMWiFiConnectCharacteristic2Desc(
    CM_CHARACTERISTIC ConnectCharacteristic);

// Translates the specified connection-characteristic option from text form:
// Returns (CM_CHARACTERISTIC)-1 on failure.
//
CM_CHARACTERISTIC
CMWiFiConnectCharacteristicName2Code(
    const TCHAR *pCharacter);

CM_CHARACTERISTIC
CMWiFiConnectCharacteristicDesc2Code(
    const TCHAR *pCharacter);

// ----------------------------------------------------------------------------
//
// Connection Manager connection-update options:
//

// Translates the specified connection-update option into text form.
//
const TCHAR *
CMWiFiConnectOption2Name(
    CM_CONFIG_OPTION ConnectOption);

const TCHAR *
CMWiFiConnectOption2Desc(
    CM_CONFIG_OPTION ConnectOption);

// Translates the specified connection-update option from text form:
// Returns (CM_CONFIG_OPTION)-1 on failure.
//
CM_CONFIG_OPTION
CMWiFiConnectOptionName2Option(
    const TCHAR *pOptionName);

CM_CONFIG_OPTION
CMWiFiConnectOptionDesc2Option(
    const TCHAR *pOptionDesc);

// ----------------------------------------------------------------------------
//
// Connection Manager connection states:
//

// Translates the specified connection state into text form.
//
const TCHAR *
CMWiFiConnectState2Name(
    CM_CONNECTION_STATE ConnectState);

const TCHAR *
CMWiFiConnectState2Desc(
    CM_CONNECTION_STATE ConnectState);

// Translates the specified connection state from text form:
// Returns (CM_CONNECTION_STATE)-1 on failure.
//
CM_CONNECTION_STATE
CMWiFiConnectStateName2State(
    const TCHAR *pStateName);

CM_CONNECTION_STATE
CMWiFiConnectStateDesc2State(
    const TCHAR *pStateDesc);

// ----------------------------------------------------------------------------
//
// Connection Manager notification types:
//

// Translates the specified notification type into text form.
//
const TCHAR *
CMWiFiNotifyType2Name(
    CM_NOTIFICATION_TYPE NotifyType);

const TCHAR *
CMWiFiNotifyType2Desc(
    CM_NOTIFICATION_TYPE NotifyType);

// Translates the specified notification type from text form:
// Returns (CM_NOTIFICATION_TYPE)-1 on failure.
//
CM_NOTIFICATION_TYPE
CMWiFiNotifyTypeName2Type(
    const TCHAR *pTypeName);

CM_NOTIFICATION_TYPE
CMWiFiNotifyTypeDesc2Type(
    const TCHAR *pTypeDesc);

};
};

#endif /* if (CMWIFI_VERSION > 0) */
#endif /* _DEFINED_CMWiFiTypes_hpp_ */
// ----------------------------------------------------------------------------
