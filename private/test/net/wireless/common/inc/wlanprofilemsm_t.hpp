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
// Definitions and declarations for the WLANProfileMSM_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WLANProfileMSM_t_
#define _DEFINED_WLANProfileMSM_t_
#pragma once

#include "WLANTypes.hpp"
#if (WLAN_VERSION > 0)

#include <WLANProfileSecurity_t.hpp>

#include <vector.hxx>
#include <wlanapi.h>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Represents an MSM element within a WLAN profile.
//
class WiFiAccount_t;
class WiFiConfig_t;
class WLANProfileMSM_t
{
public:

    // Maximum supported physycal types:
    //
    static const int MaxPhysicalTypes = 4;
    
protected:

    // Network's physical medium type:
    //
    DOT11_PHY_TYPE m_PhysicalTypes[MaxPhysicalTypes];
    int            m_PhysicalTypeCount;

    // Security elements:
    //
    vector<WLANProfileSecurity_t, ce::allocator, ce::incremental_growth<0>> m_SecurityModes;

public:

    // Constructor / destructor:
    //
    WLANProfileMSM_t(void) { Clear(); }
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

    // Physical type(s):
    //
    int
    GetPhysicalTypesCount(void) const { return m_PhysicalTypeCount; }
    DOT11_PHY_TYPE
    GetPhysicalType(int IX) const;
    DWORD
    AddPhysicalType(DOT11_PHY_TYPE PhysicalType);
    DWORD
    SetPhysicalType(int IX, DOT11_PHY_TYPE PhysicalType);
    DWORD
    RemovePhysicalType(int IX);
    
    // Security elements:
    //
    int
    GetSecurityCount(void) const { return m_SecurityModes.size(); }
    const WLANProfileSecurity_t *
    GetSecurity(int IX) const;
    DWORD
    AddSecurity(const WLANProfileSecurity_t &Security);
    DWORD
    SetSecurity(int IX, const WLANProfileSecurity_t &Security);
    DWORD
    RemoveSecurity(int IX);

  // XML encoding/decoding:

    // XML tags:
    //
    static const WCHAR XmlTagName[];
    static const WCHAR XmlNamespace[];
    
    virtual const WCHAR*
    GetXmlTagName() const;
    virtual const WCHAR*
    GetXmlNamespace() const;

    // Encodes the object into a DOM element:
    //
    virtual HRESULT
    EncodeToXml(
        litexml::XmlElement_t **ppRoot,
        const TCHAR            *pNamespace = XmlNamespace) const;

    // Initializes the object from the specified DOM element:
    //
    virtual HRESULT
    DecodeFromXml(
        const litexml::XmlElement_t &Root);
    
};

};
};

#endif /* if (WLAN_VERSION > 0) */
#endif /* _DEFINED_WLANProfileMSM_t_ */
// ----------------------------------------------------------------------------
