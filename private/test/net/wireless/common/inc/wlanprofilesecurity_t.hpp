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
// Definitions and declarations for the WLANProfileSecurity_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WLANProfileSecurity_t_
#define _DEFINED_WLANProfileSecurity_t_
#pragma once

#include "WLANTypes.hpp"
#if (WLAN_VERSION > 0)

#include <WLANProfileAuthEncryption_t.hpp>
#include <WLANProfileSharedKey_t.hpp>
#include <WLANProfileOneX_t.hpp>

#include <vector.hxx>
#include <wlanapi.h>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Represents a Security element within an MSM profile.
//
class WiFiAccount_t;
class WiFiConfig_t;
class WLANProfileSecurity_t
{
private:

    // Authentication/Encryption mode(s):
    //
    vector<WLANProfileAuthEncryption_t, ce::allocator, ce::incremental_growth<0>> m_AuthEncrModes;

    // Shared key(s):
    //
    vector<WLANProfileSharedKey_t, ce::allocator, ce::incremental_growth<4>> m_SharedKeys;

    // WEP key index:
    //
    static const int sm_MinKeyIndex = 0;
    static const int sm_DefKeyIndex = 0;
    static const int sm_MaxKeyIndex = 4;
    int m_KeyIndex;

    // Are PMKs being cached?
    // (default = true for WPA2, false otherwise)
    //
    bool m_bPMKCacheMode;

    // Lifetime (in minutes) for PMK cache entries:
    //
    static const int sm_MinPMKCacheTTL =    5;
    static const int sm_DefPMKCacheTTL =  720;
    static const int sm_MaxPMKCacheTTL = 1441;
    int m_PMKCacheTTL;

    // Size of PMK cache:
    //
    static const int sm_MinPMKCacheSize =   1;
    static const int sm_DefPMKCacheSize = 128;
    static const int sm_MaxPMKCacheSize = 256;
    int m_PMKCacheSize;

    // Is preauthentication enabled?
    //
    static const bool sm_DefPreAuthMode = false;
    bool m_bPreAuthMode;

    // Number times to pre-authenticate:
    //
    static const int sm_MinPreAuthThrottle =  1;
    static const int sm_DefPreAuthThrottle =  3;
    static const int sm_MaxPreAuthThrottle = 17;
    int m_PreAuthThrottle;

    // 802.1X authentication information:
    //
    vector<WLANProfileOneX_t, ce::allocator, ce::incremental_growth<0>> m_OneXs;

public:

    // Constructor / destructor:
    //
    WLANProfileSecurity_t(void) { Clear(); }
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

    // Authentication/Encryption mode(s):
    //
    int
    GetAuthEncryptionCount(void) const { return m_AuthEncrModes.size(); }
    const WLANProfileAuthEncryption_t *
    GetAuthEncryption(int IX) const;
    DWORD
    AddAuthEncryption(const WLANProfileAuthEncryption_t &AuthEncryption);
    DWORD
    SetAuthEncryption(int IX, const WLANProfileAuthEncryption_t &AuthEncryption);
    DWORD
    RemoveAuthEncryption(int IX);

    // Shared key(s):
    //
    int
    GetSharedKeyCount(void) const { return m_SharedKeys.size(); }
    const WLANProfileSharedKey_t *
    GetSharedKey(int IX) const;
    DWORD
    AddSharedKey(const WLANProfileSharedKey_t &SharedKey);
    DWORD
    SetSharedKey(int IX, const WLANProfileSharedKey_t &SharedKey);
    DWORD
    RemoveSharedKey(int IX);

    // WEP key index:
    //
    int
    GetKeyIndex(void) const { return m_KeyIndex; }
    void
    SetKeyIndex(int KeyIndex) { m_KeyIndex = KeyIndex; }

    // Are PMKs being cached?
    // (default = true for WPA2, false otherwise)
    //
    bool
    IsPMKCacheMode(void) const { return m_bPMKCacheMode; }
    void
    SetPMKCacheMode(bool bPMKCacheMode) { m_bPMKCacheMode = bPMKCacheMode; }

    // Lifetime (in minutes) for PMK cache entries:
    //
    int
    GetPMKCacheTTL(void) const { return m_PMKCacheTTL; }
    void
    SetPMKCacheTTL(int PMKCacheTTL) { m_PMKCacheTTL = PMKCacheTTL; }

    // Size of PMK cache:
    //
    int
    GetPMKCacheSize(void) const { return m_PMKCacheSize; }
    void
    SetPMKCacheSize(int PMKCacheSize) { m_PMKCacheSize = PMKCacheSize; }

    // Is preauthentication enabled?
    //
    bool
    IsPreAuthMode(void) const { return m_bPreAuthMode; }
    void
    SetPreAuthMode(bool bPreAuthMode) { m_bPreAuthMode = bPreAuthMode; }

    // Number times to pre-authenticate:
    //
    int
    GetPreAuthThrottle(void) const { return m_PreAuthThrottle; }
    void
    SetPreAuthThrottle(int PreAuthThrottle) { m_PreAuthThrottle = PreAuthThrottle; }

    // 802.1X authentication information:
    //
    int
    GetOneXCount(void) const { return m_OneXs.size(); }
    const WLANProfileOneX_t *
    GetOneX(int IX) const;
    DWORD
    AddOneX(const WLANProfileOneX_t &OneX);
    DWORD
    SetOneX(int IX, const WLANProfileOneX_t &OneX);
    DWORD
    RemoveOneX(int IX);

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
#endif /* _DEFINED_WLANProfileSecurity_t_ */
// ----------------------------------------------------------------------------
