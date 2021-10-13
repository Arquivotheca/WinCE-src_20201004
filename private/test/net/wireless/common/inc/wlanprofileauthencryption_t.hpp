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
// Definitions and declarations for the WLANProfileAuthEncryption_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WLANProfileAuthEncryption_t_
#define _DEFINED_WLANProfileAuthEncryption_t_
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
// Represents an Authentication/Encryption element within an MSM-Security
// profile.
//
class WiFiAccount_t;
class WiFiConfig_t;
class WLANProfileAuthEncryption_t
{
private:

    // Authentication mode:
    //
    DOT11_AUTH_ALGORITHM m_AuthMode;

    // Encryption mode:
    //
    DOT11_CIPHER_ALGORITHM m_CipherMode;

    // Use 802.1X for authentication?
    //
    bool m_bUseOneX;

    // Does connection satisfy FIPS security requirements?
    //
    bool m_bFIPSMode;

public:

    // Constructor:
    //
    WLANProfileAuthEncryption_t(void) { Clear(); }
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

    // Authentication mode:
    //
    DOT11_AUTH_ALGORITHM
    GetAuthMode(void) const { return m_AuthMode; }
    const TCHAR *
    GetAuthName(void) const { return WLANAuthMode2Desc(m_AuthMode); }
    void
    SetAuthMode(DOT11_AUTH_ALGORITHM AuthMode) { m_AuthMode = AuthMode; }

    // Encryption mode:
    //
    DOT11_CIPHER_ALGORITHM
    GetCipherMode(void) const { return m_CipherMode; }
    const TCHAR *
    GetCipherName(void) const { return WLANCipherMode2Desc(m_CipherMode); }
    void
    SetCipherMode(DOT11_CIPHER_ALGORITHM CipherMode) { m_CipherMode = CipherMode; }

    // Use 802.1X for authentication?
    //
    bool 
    IsUseOneX(void) const { return m_bUseOneX; }
    void
    SetUseOneX(bool bUseOneX) { m_bUseOneX = bUseOneX; }

    // Does connection satisfy FIPS security requirements?
    //
    bool 
    IsFIPSMode(void) const { return m_bFIPSMode; }
    void
    SetFIPSMode(bool bFIPSMode) { m_bFIPSMode = bFIPSMode; }

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
#endif /* _DEFINED_WLANProfileAuthEncryption_t_ */
// ----------------------------------------------------------------------------
