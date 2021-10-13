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

#ifndef _DEFINED_WLANProfile_t_
#define _DEFINED_WLANProfile_t_
#pragma once

#include "WLANTypes.hpp"
#if (WLAN_VERSION > 0)

#include <WiFUtils.hpp>
#include <WLANProfileSsid_t.hpp>
#include <WLANProfileMSM_t.hpp>
#include <WLANProfileIHV_t.hpp>

#include <vector.hxx>
#include <wlanapi.h>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Represents a WLAN network-profile.
//
class WiFiAccount_t;
class WiFiConfig_t;
class WLANProfile_t
{
private:

    // Profile name:
    //
    ce::tstring m_Name;
    
    // Connection mode:
    //
    static const DOT11_BSS_TYPE sm_DefConnectType = dot11_BSS_type_any;
    DOT11_BSS_TYPE m_ConnectType;

    // Automatically connect when network is present?
    //
    static const bool sm_DefAutoConnect = true;
    bool m_bAutoConnect;

    // Automatically roam when more preferred network is in range?
    //
    static const bool sm_DefAutoSwitch = false;
    bool m_bAutoSwitch;

    // SSID name(s):
    //
    vector<WLANProfileSsid_t, ce::allocator, ce::incremental_growth<0>> m_Ssids;

    // MSM / Security objects:
    //
    vector<WLANProfileMSM_t, ce::allocator, ce::incremental_growth<0>> m_MSMs;

    // IHV Extension
    //
    vector<WLANProfileIHV_t, ce::allocator, ce::incremental_growth<0>> m_IHVs;

public:

    // Constructor / destructor:
    //
    WLANProfile_t(void) { Clear(); }

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

    // Profile name:
    //
    const TCHAR *
    GetName(void) const { return m_Name; }
    void
    SetName(const TCHAR *pName) { m_Name.assign(pName); }

    // SSID name(s):
    //
    int
    GetSsidCount(void) const { return m_Ssids.size(); }
    const WLANProfileSsid_t *
    GetSsid(int IX) const;
    DWORD
    AddSsid(const WLANProfileSsid_t &Ssid);
    DWORD
    SetSsid(int IX, const WLANProfileSsid_t &Ssid);
    DWORD
    RemoveSsid(int IX);
    
    // Connection mode:
    //
    DOT11_BSS_TYPE
    GetConnectType(void) const { return m_ConnectType; }
    void
    SetConnectType(DOT11_BSS_TYPE ConnectType) { m_ConnectType = ConnectType; }

    // Automatically connect when network is present?
    //
    bool
    IsAutoConnect(void) const { return m_bAutoConnect; }
    void
    SetAutoConnect(bool bAutoConnect) { m_bAutoConnect = bAutoConnect; }

    // Automatically roam when more preferred network is in range?
    //
    bool
    IsAutoSwitch(void) const { return m_bAutoSwitch; }
    void
    SetAutoSwitch(bool bAutoSwitch) { m_bAutoSwitch = bAutoSwitch; }

    // MSM / Security objects:
    //
    int
    GetMSMCount(void) const { return m_MSMs.size(); }
    const WLANProfileMSM_t *
    GetMSM(int IX) const;
    DWORD
    AddMSM(const WLANProfileMSM_t &MSM);
    DWORD
    SetMSM(int IX, const WLANProfileMSM_t &MSM);
    DWORD
    RemoveMSM(int IX);

    // IHV :
    //
    int
    GetIHVCount(void) const { return m_IHVs.size(); }
    const WLANProfileIHV_t *
    GetIHV(int IX) const;
    DWORD
    AddIHV(const WLANProfileIHV_t &IHV);
    DWORD
    SetIHV(int IX, const WLANProfileIHV_t &IHV);
    DWORD
    RemoveIHV(int IX);

  // XML encoding/decoding:

    // XML tags:
    //
    static const WCHAR XmlTagName[];
    static const WCHAR XmlNamespace[];

    // Encodes the object into a DOM element:
    //
    HRESULT
    EncodeToXml(
        litexml::XmlElement_t **ppRoot,
        const TCHAR            *pNamespace = XmlNamespace) const;

    // Initializes the object from the specified DOM element:
    //
    HRESULT
    DecodeFromXml(
        const litexml::XmlElement_t &Root);
    
};

};
};

#endif /* if (WLAN_VERSION > 0) */
#endif /* _DEFINED_WLANProfile_t_ */
// ----------------------------------------------------------------------------
