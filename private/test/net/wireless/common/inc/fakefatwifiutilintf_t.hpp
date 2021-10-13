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
// Header file for FakeWiFiUtil functions
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_FAKEFATWIFIUTIL_
#define _DEFINED_FAKEFATWIFIUTIL_
#pragma once

namespace ce {
namespace qa {
class FakeWifiUtilIntf_t;
class WiFiRegHelper;

class FakeFatWifiUtilIntf_t : public FakeWifiUtilIntf_t
{
private:
// ----------------------------------------------------------------------------
//
// Sets up the proper registry setting for the Fake WiFi miniport driver
// and calls the proper miniport driver registration function to register
// the miniport adapter.
// This is the same as physically inserting the card.
// If DriverInstance is zero, uses last digit of driver name instead.
//
DWORD 
InsertFakeWiFi(
    const WCHAR *pDriverName,      // e.g.,     "xwifi11b"  ||     "xwifi11bN"
    int           DriverInstance,  // e.g., 1 = "xwifi11b1" || 0 = "xwifi11bN"
    const WCHAR *pIpAddress,
    const WCHAR *pSubnetMask,
    const WCHAR *pCommandQueueName,
    const WCHAR *pStatusQueueName,
    const WCHAR *pPhyType,
    long         PauseAfterMS);

public:

FakeFatWifiUtilIntf_t();

// Sets up the proper registry setting for the Fake WiFi miniport driver
// and calls the proper miniport driver registration function to register
// the miniport adapter.
// This is the same as physically inserting the card.
// If DriverInstance is zero, uses last digit of driver name instead.
//n buid
virtual DWORD InsertFakeWiFi(
    const WCHAR *pDriverName,      // e.g.,     "xwifi11b"  ||     "xwifi11bN"
    int           DriverInstance,  // e.g., 1 = "xwifi11b1" || 0 = "xwifi11bN"
    const WCHAR *pCommandQueueName = NULL,
    const WCHAR *pStatusQueueName  = NULL,
    const WCHAR *pPhyType          = NULL,
    long         PauseAfterMS      = 5*1000);
virtual DWORD
InsertFakeWiFi_StaticIp(
    const WCHAR *pDriverName,      // e.g.,     "xwifi11b"  ||     "xwifi11bN"
    int           DriverInstance,  // e.g., 1 = "xwifi11b1" || 0 = "xwifi11bN"
    const WCHAR *pIpAddress,       // Adapter's static IP address
    const WCHAR *pSubnetMask,
    long         PauseAfterMS = 5*1000);

// Sets up a Access Point in the Fake WiFi registry according to the
// settings of the specified Network or Config:
// pSsidKeyName writes to a key other than the config's SSID.
// pRandomMAC will output the randomly generated BSSID.
//
virtual DWORD
InstallFakeAP(
    const xwifiNetwork &Network,
    const WCHAR        *pSsidKeyName = NULL);
virtual DWORD
InstallFakeAP(
    const WiFiConfig_t &APConfig,
    const WCHAR        *pSsidKeyName = NULL,
    MACAddr_t          *pRandomMAC   = NULL,
    LONG                Rssi         = -50); // Max Signal Strength

// Deletes the specified Access Point from the Fake WiFi registry:
//
virtual DWORD
RemoveFakeAP(
    const WCHAR *pAPSSID);
};
};
};
#endif /* _DEFINED_FAKEFATWIFIUTIL_ */