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
// Wireless LAN device-control library.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WiFiDevice_hpp
#define _DEFINED_WiFiDevice_hpp
#pragma once

#include <WiFUTils.hpp>

#include <windev.h>
#include <ndis.h>

namespace ce {
namespace qa {

namespace WiFiDevice {
    
    // Translates the specified device power state into text form:
    //
    const TCHAR *
    PowerState2Name(
        CEDEVICE_POWER_STATE State);

    const TCHAR *
    PowerState2Desc(
        CEDEVICE_POWER_STATE State);

    // Translates the specified device power state from text form:
    // Returns (CEDEVICE_POWER_STATE)-1 on failure.
    //
    CEDEVICE_POWER_STATE
    PowerStateName2State(
        const TCHAR *pStateName);

    CEDEVICE_POWER_STATE
    PowerStateDesc2State(
        const TCHAR *pStateDesc);

    // Execute an NDIS IO control operation:
    // Optional fShowErrors flag indicates whether we want to see errors.
    //
    DWORD
    NdisIoctl(
        DWORD  Command,
        void  *pInBuffer,
        DWORD   InBufferBytes,
        void  *pOutBuffer,
        DWORD *pOutBufferBytes,
        bool   fShowErrors = true);


    // Binds or unbinds a network adapter to or from NDIS:
    // Optional fShowErrors flag indicates whether we want to see errors.
    //
    DWORD
    AdapterBindUnbind(
        const char *pAdapterName,
        bool        fBindAdapter, // true = bind, false = unbind
        bool        fShowErrors = true);
    DWORD
    AdapterBindUnbind(
        const WCHAR *pAdapterName,
        bool         fBindAdapter, // true = bind, false = unbind
        bool         fShowErrors = true);

    // Registers or unregisters a network adapter to or from NDIS:
    // If DriverInstance is zero, uses last digit of driver name.
    // Optional fShowErrors flag indicates whether we want to see errors.
    //
    DWORD
    AdapterLoadUnload(
        const char *pDriverName,      // e.g., "xwifi11b"
        int          DriverInstance,  // e.g., 1 = "xwifi11b1"
        bool        fRegisterAdapter, // true = register, false = unregister
        bool        fShowErrors = true);
    DWORD
    AdapterLoadUnload(
        const WCHAR *pDriverName,      // e.g., "xwifi11b"
        int           DriverInstance,  // e.g., 1 = "xwifi11b1"
        bool         fRegisterAdapter, // true = register, false = unregister
        bool         fShowErrors = true);

    // Power on or off a network adapter:
    // Optional fShowErrors flag indicates whether we want to see errors.
    //
    DWORD
    AdapterPowerOnOff(
        const char *pAdapterName,
        bool        fTurnPowerOn,
        bool        fShowErrors = true);
    DWORD
    AdapterPowerOnOff(
        const WCHAR *pAdapterName,
        bool         fTurnPowerOn,
        bool         fShowErrors = true);
    
    // Query a device's power state:
    // Optional fShowErrors flag indicates whether we want to see errors.
    // Returns ERROR_NOT_SUPPORTED if adapter does not support power mgmt.
    //
    DWORD
    AdapterPowerQuery(
        const char           *pAdapterName,
        CEDEVICE_POWER_STATE *pState,
        bool                  fShowErrors = true);
    DWORD
    AdapterPowerQuery(
        const WCHAR          *pAdapterName,
        CEDEVICE_POWER_STATE *pState,
        bool                  fShowErrors = true);
    
    // Releases and/or renews the IP address for the specified adapter:
    // Returns ERROR_NOT_FOUND if the adapter is not loaded.
    //
    DWORD
    IpAddressReleaseOrRenew(
        const char *pAdapterName,  // adapter name 'NE20001', 'CISCO1'
        bool        fRenewLease,   // false=release, true=renew
        bool        fShowErrors = true);
    DWORD
    IpAddressReleaseOrRenew(
        const WCHAR *pAdapterName,  // adapter name 'NE20001', 'CISCO1'
        bool         fRenewLease,   // false=release, true=renew
        bool         fShowErrors = true);
    
};

};
};
#endif /* _DEFINED_WiFiDevice_hpp */
// ----------------------------------------------------------------------------
