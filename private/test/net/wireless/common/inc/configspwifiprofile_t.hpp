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
// Definitions and declarations for the WLANProfile_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_ConfigSPWiFiProfile_t_
#define _DEFINED_ConfigSPWiFiProfile_t_
#pragma once

#include "WLANTypes.hpp"
#if (CMWIFI_VERSION > 0)

#include <WiFUtils.hpp>
#include <WlanProfile_t.hpp>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Represents a WLAN network-profile.
//
class WiFiAccount_t;
class WiFiConfig_t;
class ConfigSPWiFiProfile_t
{
private:

    // Profile name:
    //
    ce::tstring m_Name;
    
    // Key :
    ce::tstring m_Key;

    // Connection mode:
    //
    static const DOT11_BSS_TYPE sm_DefConnectType = dot11_BSS_type_infrastructure;
    DOT11_BSS_TYPE m_ConnectType;

    // SSID
    //
    WLANProfileSsid_t m_Ssid;

    // MSM
    //
    WLANProfileMSM_t m_Msm;


    DWORD
    RemoveSsid();

    const WLANProfileSecurity_t*
    GetProfileSecurity() const;

    const WLANProfileAuthEncryption_t*
    GetProfileAuthEncryption() const;

    DOT11_AUTH_ALGORITHM
    GetAuth() const;

    int
    GetKeyIndex() const;

    DOT11_CIPHER_ALGORITHM
    GetCipher() const;

    int
    GetEap() const;

    int
    IsHidden() const;

    // Use 802.1X for authentication?
    //
    bool 
    IsUseOneX(void) const;

public:

    // Constructor / destructor:
    //
    ConfigSPWiFiProfile_t(void){ Clear(); }
   ~ConfigSPWiFiProfile_t(void) {;}

    static HRESULT
    GetDeleteAllProfile(
        litexml::XmlElement_t **ppRoot);

    static HRESULT
    GetDeleteNetworkProfile(
        litexml::XmlElement_t **ppRoot,
        WLANProfile_t&      wlanProfile);

    void
    Clear(void);

    // Initializes the profile from the specified network configuration:
    // If necessary, uses the optional WiFiAccount object to supply EAP
    // authenticaiton credentials.
    //
    DWORD
    Initialize(
        const WiFiConfig_t  &Network,
        const WiFiAccount_t *pEapCredentials = NULL);

    // Key :
    const TCHAR *
    GetKey(void) const { return m_Key; }
    void
    SetKey(const TCHAR *pKey) { m_Key.assign(pKey); }


    // Profile name:
    //
    const TCHAR *
    GetName(void) const { return m_Name; }
    void
    SetName(const TCHAR *pName) { m_Name.assign(pName); }

    const WLANProfileSsid_t *
    GetSsid() const;
    DWORD
    SetSsid(const WLANProfileSsid_t &Ssid);
    
    // Connection mode:
    //
    DOT11_BSS_TYPE
    GetConnectType(void) const { return m_ConnectType; }
    void
    SetConnectType(DOT11_BSS_TYPE ConnectType) { m_ConnectType = ConnectType; }

  // XML encoding/decoding:

    // XML tags:
    //
    static const WCHAR XmlTagName[];

    // Encodes the object into a DOM element:
    //
    HRESULT
    EncodeToXml(
        litexml::XmlElement_t **ppRoot) const;

    // Initializes the object from the specified DOM element:
    //
    HRESULT
    DecodeFromXml(
        const litexml::XmlElement_t &Root);
    
};

};
};

#endif /* if (CMWIFI_VERSION > 0) */
#endif /* _DEFINED_ConfigSPWiFiProfile_t_ */
// ----------------------------------------------------------------------------
