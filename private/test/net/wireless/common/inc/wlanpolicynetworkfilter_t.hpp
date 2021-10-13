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
// Definitions and declarations for the WLANPolicyNetworkFilter_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WLANPolicyNetworkFilter_t_
#define _DEFINED_WLANPolicyNetworkFilter_t_
#pragma once

#include "WLANTypes.hpp"
#if (WLAN_VERSION > 0)

#include <WLANPolicyNetwork_t.hpp>

#include <vector.hxx>
#include <wlanapi.h>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Represents a network-filter criteria specification for a WLAN policy.
//
class WLANPolicyNetworkFilter_t
{
private:

    // Allow/Block networks lists:
    //
    vector<WLANPolicyNetwork_t, ce::allocator, ce::incremental_growth<0>> m_Allows;
    vector<WLANPolicyNetwork_t, ce::allocator, ce::incremental_growth<0>> m_Blocks;

    // Deny access to all IBSS (Ad Hoc) networks?
    //
    bool m_bDenyIBSS;
    
    // Deny access to all ESS (Infrastructure) networks?
    //
    bool m_bDenyESS;

public:

    // Constructor / destructor:
    //
    WLANPolicyNetworkFilter_t(void) { Clear(); }
    void
    Clear(void);

    // Allow/Block networks lists:
    //
    int
    GetAllowCount(void) const { return m_Allows.size(); }
    const WLANPolicyNetwork_t *
    GetAllow(int IX) const;
    DWORD
    AddAllow(const WLANPolicyNetwork_t &Allow);
    DWORD
    SetAllow(int IX, const WLANPolicyNetwork_t &Allow);
    DWORD
    RemoveAllow(int IX);
    
    int
    GetBlockCount(void) const { return m_Blocks.size(); }
    const WLANPolicyNetwork_t *
    GetBlock(int IX) const;
    DWORD
    AddBlock(const WLANPolicyNetwork_t &Block);
    DWORD
    SetBlock(int IX, const WLANPolicyNetwork_t &Block);
    DWORD
    RemoveBlock(int IX);

    // Deny access to all IBSS (Ad Hoc) networks?
    //
    bool 
    IsDenyIBSS(void) const { return m_bDenyIBSS; }
    void
    SetDenyIBSS(bool DenyIBSS) { m_bDenyIBSS = DenyIBSS; }
    
    // Deny access to all ESS (Infrastructure) networks?
    //
    bool 
    IsDenyESS(void) const { return m_bDenyESS; }
    void
    SetDenyESS(bool DenyESS) { m_bDenyESS = DenyESS; }

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
#endif /* _DEFINED_WLANPolicyNetworkFilter_t_ */
// ----------------------------------------------------------------------------
