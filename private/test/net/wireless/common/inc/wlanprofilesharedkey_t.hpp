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
// Definitions and declarations for the WLANProfileSharedKey_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WLANProfileSharedKey_t_
#define _DEFINED_WLANProfileSharedKey_t_
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
// Represents a shared key element within a MSM-Security profile.
//
class WiFiAccount_t;
class WiFiConfig_t;
class WLANProfileSharedKey_t
{
public:

    // Key type:
    //
    enum KeyType_e
    {
        NetworkKey = 1, // Network (WEP) key
        Passphrase = 2, // Passphrase (WPA/WPA2) key
    };

    // Maximum key-material bytes:
    //
    static const int MaxKeyMaterialSize = 256;
    
private:

    // Key type:
    //
    KeyType_e m_KeyType;

    // Is key protected?
    //
    bool m_bProtected;

    // Key material:
    //
    BYTE  m_KeyMaterial[MaxKeyMaterialSize];
    DWORD m_KeyMaterialLength;

public:

    // Constructor:
    //
    WLANProfileSharedKey_t(void) { Clear(); }
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

    // Key type:
    //
    KeyType_e
    GetKeyType(void) const { return m_KeyType; }
    void
    SetKeyType(KeyType_e KeyType) { m_KeyType = KeyType; }

    // Is key protected?
    //
    bool
    IsProtected(void) const { return m_bProtected; }
    void
    SetProtected(bool bProtected) { m_bProtected = bProtected; }

    // Key material:
    //
    const BYTE *
    GetKeyMaterial(void) const { return m_KeyMaterial; }
    DWORD
    GetKeyMaterialLength(void) const { return m_KeyMaterialLength; }
    
    DWORD
    GetKeyMaterial(
      __out_ecount(*pBufferBytes) BYTE  *pBuffer,
                                  DWORD *pBufferBytes) const;

    DWORD
    SetKeyMaterial(
        const BYTE *pKey,
        DWORD        KeyBytes);

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
#endif /* _DEFINED_WLANProfileSharedKey_t_ */
// ----------------------------------------------------------------------------
