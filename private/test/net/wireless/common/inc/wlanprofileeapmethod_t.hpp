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
// Definitions and declarations for the WLANProfileEapMethod_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WLANProfileEapMethod_t_
#define _DEFINED_WLANProfileEapMethod_t_
#pragma once

#include "WLANTypes.hpp"
#if (WLAN_VERSION > 0)

#include <wlanapi.h>

namespace litexml
{
    class XmlElement_t;
};

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Represents an EAP-method element within an EAP-Configuration profile.
//
class WiFiAccount_t;
class WiFiConfig_t;
class WLANProfileEapMethod_t
{
private:

    // Method type:
    //
    static const UINT sm_MinType = 0;
    static const UINT sm_DefType = 0;
    static const UINT sm_MaxType = UINT_MAX;
    UINT m_Type;

    // Vendor ID:
    //
    static const UINT sm_MinVendorId = 0;
    static const UINT sm_DefVendorId = 0;
    static const UINT sm_MaxVendorId = UINT_MAX;
    UINT m_VendorId;

    // Vendor type:
    //
    static const UINT sm_MinVendorType = 0;
    static const UINT sm_DefVendorType = 0;
    static const UINT sm_MaxVendorType = UINT_MAX;
    UINT m_VendorType;

    // Authod ID:
    //
    static const UINT sm_MinAuthorId = 0;
    static const UINT sm_DefAuthorId = 0;
    static const UINT sm_MaxAuthorId = UINT_MAX;
    UINT m_AuthorId;

public:

    // Constructor:
    //
    WLANProfileEapMethod_t(void) { Clear(); }
    void
    Clear(void);

    // Initializes the profile from the specified network configuration:
    // If necessary, uses the optional WiFiAccount object to supply EAP
    // authenticaiton credentials.
    // Returns ERROR_INSUFFICIENT_BUFFER if it will require multiple objects
    // to represent the network data. In that case, the caller should copy
    // this instance of the object and re-call the method again.
    //
    DWORD
    Initialize(
        const WiFiConfig_t  &Network,
        const WiFiAccount_t *pEapCredentials = NULL);

    // Method type:
    //
    UINT
    GetType(void) const { return m_Type; }
    void
    SetType(UINT Type) { m_Type = Type; }

    // Vendor ID:
    //
    UINT
    GetVendorId(void) const { return m_VendorId; }
    void
    SetVendorId(UINT VendorId) { m_VendorId = VendorId; }

    // Vendor type:
    //
    UINT
    GetVendorType(void) const { return m_VendorType; }
    void
    SetVendorType(UINT VendorType) { m_VendorType = VendorType; }

    // Authod ID:
    //
    UINT
    GetAuthorId(void) const { return m_AuthorId; }
    void
    SetAuthorId(UINT AuthorId) { m_AuthorId = AuthorId; }

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
#endif /* _DEFINED_WLANProfileEapMethod_t_ */
// ----------------------------------------------------------------------------
