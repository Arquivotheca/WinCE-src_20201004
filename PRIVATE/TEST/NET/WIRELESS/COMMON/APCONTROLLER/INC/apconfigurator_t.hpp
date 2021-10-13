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
// Definitions and declarations for the APConfigurator_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_APConfigurator_t_
#define _DEFINED_APConfigurator_t_
#pragma once

#include "AccessPointState_t.hpp"

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Provides the basic implementation of the APController's Access-Point
// configuration data-store interface.
//
class APConfigurator_t
{
private:

    // AP's friendly name:
    ce::tstring m_APName;

    // Localized and null-terminated string values:
    TCHAR m_VendorName  [AccessPointState_t::MaxVendorNameChars  +1];
    TCHAR m_ModelNumber [AccessPointState_t::MaxModelNumberChars +1];
    TCHAR m_Ssid        [AccessPointState_t::MaxSsidChars        +1];
    TCHAR m_RadiusSecret[AccessPointState_t::MaxRadiusSecretChars+1];
    TCHAR m_Passphrase  [AccessPointState_t::MaxPassphraseChars  +1];

    // Copy and assignment are deliberately disabled:
    APConfigurator_t(const APConfigurator_t &src);
    APConfigurator_t &operator = (const APConfigurator_t &src);

protected:

    // Current configuration state:
    AccessPointState_t m_State;

public:

    // Data-store keys:
    static const TCHAR * const ConfiguratorKey;
    static const TCHAR * const AuthenticationKey;
    static const TCHAR * const BSsidKey;
    static const TCHAR * const CapsEnabledKey;
    static const TCHAR * const CapsImplementedKey;
    static const TCHAR * const CipherKey;
    static const TCHAR * const ModelNumberKey;
    static const TCHAR * const PassphraseKey;
    static const TCHAR * const RadioStateKey;
    static const TCHAR * const RadiusPortKey;
    static const TCHAR * const RadiusSecretKey;
    static const TCHAR * const RadiusServerKey;
    static const TCHAR * const SsidKey;
    static const TCHAR * const VendorNameKey;
    static const TCHAR * const WEPKeyKey;
    static const TCHAR * const WEPIndexKey;

    // Constructor and destructor:
    APConfigurator_t(const TCHAR *pAPName);
    virtual
   ~APConfigurator_t(void);

    // Sends the updated configuration values (if any) to the Access
    // Point device and stores them in the configuration store:
    virtual HRESULT
    SaveConfiguration(void) = 0;

    // (Re)connects to the access point device and gets the current
    // configuration values:
    // The default implementation does nothing.
    virtual HRESULT
    Reconnect(void);

    // Gets the AP's "friendly" name:
    const TCHAR *
    GetAPName(void) const
    {
        return m_APName;
    }
 
    // Gets the AP vendor name or model number:
    const TCHAR *
    GetVendorName(void);
    const TCHAR *
    GetModelNumber(void);

    // Gets or sets the SSID (WLAN Service Set Identifier / Name):
    const TCHAR *
    GetSsid(void);
    HRESULT
    SetSsid(const TCHAR *pSsid);

    // Gets or sets the BSSID (Basic Service Set Identifier):
    const MACAddr_t &
    GetBSsid(void);
    HRESULT
    SetBSsid(const MACAddr_t &BSsid);

    // Gets or sets the bit-map describing the AP's capabilities:
    int
    GetCapabilitiesImplemented(void);
    int
    GetCapabilitiesEnabled(void);
    HRESULT
    SetCapabilitiesEnabled(int Capabilities);
    
    // Gets or sets the AP's radio on/off status:
    bool
    IsRadioOn(void);
    HRESULT
    SetRadioState(bool State);

    // Gets or sets the security modes:
    APAuthMode_e 
    GetAuthMode(void);
    const TCHAR * 
    GetAuthName(void) { return APAuthMode2String(GetAuthMode()); }
    APCipherMode_e
    GetCipherMode(void);
    const TCHAR *
    GetCipherName(void) { return APCipherMode2String(GetCipherMode()); }
    HRESULT
    SetSecurityMode(
        APAuthMode_e   AuthMode,
        APCipherMode_e CipherMode);

    // Gets or sets the RADIUS Server information:
    DWORD
    GetRadiusServer(void);
    int
    GetRadiusPort(void);
    const TCHAR *
    GetRadiusSecret(void);
    HRESULT
    SetRadius(
        const TCHAR *pServer,
        int          Port,
        const TCHAR *pKey);

    // Gets or sets the WEP (Wired Equivalent Privacy) key:
    virtual const WEPKeys_t &
    GetWEPKeys(void);
    HRESULT
    SetWEPKeys(const WEPKeys_t &KeyData);

    // Gets or sets the pass-phrase:
    const TCHAR *
    GetPassphrase(void);
    HRESULT
    SetPassphrase(const TCHAR *pPassphrase);
};

};
};
#endif /* _DEFINED_APConfigurator_t_ */
// ----------------------------------------------------------------------------
