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
// Definitions and declarations for the WLANPolicy_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WLANPolicy_t_
#define _DEFINED_WLANPolicy_t_
#pragma once

#include "WLANTypes.hpp"
#if (WLAN_VERSION > 0)

#include <WLANPolicyNetworkFilter_t.hpp>
#include <WLANProfile_t.hpp>

#include <vector.hxx>
#include <wlanapi.h>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Represents a WLAN policy specification.
//
class WLANPolicy_t
{
private:

    // Policy name and description:
    //
    ce::tstring m_Name;
    ce::tstring m_Description;

    // Is AutoConfig to be used?
    //
    static const bool sm_DefEnableAutoConfig = true;
    bool m_bEnableAutoConfig;

    // Should AutoConfig show Denied Networks?
    //
    static const bool sm_DefShowDeniedNetworks = true;
    bool m_bShowDeniedNetworks;

    // Should AutoConfig all users to create all-user profiles?
    //
    static const bool sm_DefAllowAllToCreateAllUserProfiles = true;
    bool m_bAllowAllToCreateAllUserProfiles;

    // Network allow/block filters:
    //
    vector<WLANPolicyNetworkFilter_t, ce::allocator, ce::incremental_growth<0>> m_Filters;

    // List of profiles within the policy:
    //
    vector<WLANProfile_t, ce::allocator, ce::incremental_growth<0>> m_Profiles;

public:

    // Constructor / destructor:
    //
    WLANPolicy_t(void) { Clear(); }
    void
    Clear(void);

    // Policy name and description:
    //
    const TCHAR *
    GetName(void) const { return m_Name; }
    void
    SetName(const TCHAR *pName) { m_Name.assign(pName); }
    
    const TCHAR *
    GetDescription(void) const { return m_Description; }
    void
    SetDescription(const TCHAR *pDescription) { m_Description.assign(pDescription); }

    // Is AutoConfig to be used?
    //
    bool
    IsEnableAutoConfig(void) const { return m_bEnableAutoConfig; }
    void
    SetEnableAutoConfig(bool bEnableAutoConfig) { m_bEnableAutoConfig = bEnableAutoConfig; }

    // Should AutoConfig show Denied Networks?
    //
    bool
    IsShowDeniedNetworks(void) const { return m_bShowDeniedNetworks; }
    void
    SetShowDeniedNetworks(bool bShowDeniedNetworks) { m_bShowDeniedNetworks = bShowDeniedNetworks; }

    // Should AutoConfig all users to create all-user profiles?
    //
    bool
    IsAllowAllToCreateAllUserProfiles(void) const { return m_bAllowAllToCreateAllUserProfiles; }
    void
    SetAllowAllToCreateAllUserProfiles(bool bAllowAllToCreateAllUserProfiles) { m_bAllowAllToCreateAllUserProfiles = bAllowAllToCreateAllUserProfiles; }

    // Network allow/block filters:
    //
    int
    GetFilterCount(void) const { return m_Filters.size(); }
    const WLANPolicyNetworkFilter_t *
    GetFilter(int IX) const;
    DWORD
    AddFilter(const WLANPolicyNetworkFilter_t &Filter);
    DWORD
    SetFilter(int IX, const WLANPolicyNetworkFilter_t &Filter);
    DWORD
    RemoveFilter(int IX);

    // List of profiles within the policy:
    //
    int
    GetProfileCount(void) const { return m_Profiles.size(); }
    const WLANProfile_t *
    GetProfile(int IX) const;
    DWORD
    AddProfile(const WLANProfile_t &Profile);
    DWORD
    SetProfile(int IX, const WLANProfile_t &Profile);
    DWORD
    RemoveProfile(int IX);

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
#endif /* _DEFINED_WLANPolicy_t_ */
// ----------------------------------------------------------------------------
