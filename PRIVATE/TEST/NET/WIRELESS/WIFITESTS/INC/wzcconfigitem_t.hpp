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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
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
// Definitions and declarations for the WZCConfigItem_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WZCConfigItem_t_
#define _DEFINED_WZCConfigItem_t_
#pragma once

#include "Utils.hpp"
#include "WZCStructPtr_t.hpp"

#include <MACAddr_t.hpp>
#include <eapol.h>
#include <wzcsapi.h>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// A class implementation of the WZC_WLAN_CONFIG structure.
// Describes a single network-configuration in either WZC's 
// Preferred or Available Networks list.
//
class WZCConfigItem_t
{
public:

    // Copies or deallocates the specified RAW_DATA structure:
    static DWORD
    CopyRawData(RAW_DATA *pDest, const RAW_DATA &Source);
    static void
    FreeRawData(RAW_DATA *pData);

    // Copies or deallocates the specified WZC_WLAN_CONFIG structure:
    static DWORD
    CopyConfigItem(WZC_WLAN_CONFIG *pDest, const WZC_WLAN_CONFIG &Source);
    static void
    FreeConfigItem(WZC_WLAN_CONFIG *pData);

private:

    // Reference-counted WZC_WLAN_CONFIG pointer:
    typedef WZCStructPtr_t<WZC_WLAN_CONFIG,
                           DWORD (*)(WZC_WLAN_CONFIG *,
                               const WZC_WLAN_CONFIG &),
                           WZCConfigItem_t::CopyConfigItem,
                           void (*)(WZC_WLAN_CONFIG *),
                           WZCConfigItem_t::FreeConfigItem> WZCConfigItemPtr_t;

    // WZC config data:
    WZCConfigItemPtr_t m_pData;

public:

    // Constructor and destructor:
    WZCConfigItem_t(void);
   ~WZCConfigItem_t(void);

    // Copy and assignment:
    WZCConfigItem_t(const WZCConfigItem_t &Source);
    WZCConfigItem_t &operator = (const WZCConfigItem_t &Source);

    // Construct or assign this object from a WZC network configuration
    // structure:
    explicit WZCConfigItem_t(const WZC_WLAN_CONFIG &Source);
    WZCConfigItem_t &operator = (const WZC_WLAN_CONFIG &Source);

    // Retrieves the underlying WZC network configuration structure:
    operator const WZC_WLAN_CONFIG & (void) const { return *m_pData; }

    // Makes sure this object contains a private copy of the WZC data:
    DWORD
    Privatize(void);

    // Initializes a wireless config object to connect to an infrastructure
    // network as indicated:
    // Returns two special error codes to signal errors which could be
    // caused by the normal negative WZC API tests:
    //     ERROR_INVALID_FLAGS - indicates security-modes are invalid
    //     ERROR_INVALID_DATA -- indicates the key-data is invalid
    DWORD
    InitInfrastructure(
        const TCHAR                    *pSsidName,
        NDIS_802_11_AUTHENTICATION_MODE AuthMode,
        ULONG                           CipherMode,
        DWORD                           EAPType,
        int                             KeyIndex,
        const TCHAR                    *pKey);      // NULL for EAP

    // Logs the network configuration:
    void
    Log(
        void (*LogFunc)(const TCHAR *pFormat, ...),
        const TCHAR *pLinePrefix = NULL,    // insert at front of each line
        bool         Verbose = true) const; // display all available info
};

};
};
#endif /* _DEFINED_WZCConfigItem_t_ */
// ----------------------------------------------------------------------------
