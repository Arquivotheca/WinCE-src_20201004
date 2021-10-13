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
// Definitions and declarations for the WLANProfileSsid_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WLANProfileSsid_t_
#define _DEFINED_WLANProfileSsid_t_
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
// Represents an Ssid element within a WLAN profile.
//
class WiFiAccount_t;
class WiFiConfig_t;
class WLANProfileSsid_t
{
private:

    // SSID name (wide characters):
    //
    WCHAR m_WCName[DOT11_SSID_MAX_LENGTH+1];
    DWORD m_WCLength;

    // SSID name (multi-byte characters):
    //
    char  m_MBName[DOT11_SSID_MAX_LENGTH+1];
    DWORD m_MBLength;

    // Is SSID being broadcast?
    //
    static const bool sm_DefBroadcast = true;
    bool m_bBroadcast;

public:

    // Constructor:
    //
    WLANProfileSsid_t(void) { Clear(); }
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

    // Converts the object to or from WLAN format:
    //
    void
    ToDot11(
        DOT11_SSID *pDot11);
    void
    FromDot11(
        const DOT11_SSID &Dot11);

    // Gets or sets the SSID name:
    //
    const TCHAR *
    GetSsid(void) const
    {
#ifdef UNICODE
        return m_WCName;
#else
        return m_MBName;
#endif
    }
    DWORD
    GetSsidLength(void) const
    {
#ifdef UNICODE
        return m_WCLength;
#else
        return m_MBLength;
#endif
    }
    
    const WCHAR *
    GetWCSsid(void) const { return m_WCName; }
    DWORD
    GetWCSsidLength(void) const { return m_WCLength; }
    
    const char *
    GetMBSsid(void) const { return m_MBName; }
    DWORD
    GetMBSsidLength(void) const { return m_MBLength; }
    
    void
    SetSsid(
        const BYTE *pSsidName,
        DWORD        SsidChars);    
    void
    SetSsid(
        const WCHAR *pSsidName,
        int           SsidChars = -1);    
    void
    SetSsid(
        const char *pSsidName,
        int          SsidChars = -1);

    // Gets or sets the broadcast flag:
    //
    bool
    IsBroadcast(void) const { return m_bBroadcast; }
    void
    SetBroadcast(bool bBroadcast) { m_bBroadcast = bBroadcast; }

    // Compares this SSID to another:
    //
    int
    Compare(const WLANProfileSsid_t &rhs) const;

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

// ----------------------------------------------------------------------------
//
// Compares two profile-SSID objects:
//
inline bool
operator < (const WLANProfileSsid_t &lhs, const WLANProfileSsid_t &rhs)
{
    return lhs.Compare(rhs) < 0;
}
inline bool
operator > (const WLANProfileSsid_t &lhs, const WLANProfileSsid_t &rhs)
{
    return lhs.Compare(rhs) > 0;
}
inline bool
operator <= (const WLANProfileSsid_t &lhs, const WLANProfileSsid_t &rhs)
{
    return lhs.Compare(rhs) <= 0;
}
inline bool
operator >= (const WLANProfileSsid_t &lhs, const WLANProfileSsid_t &rhs)
{
    return lhs.Compare(rhs) >= 0;
}
inline bool
operator == (const WLANProfileSsid_t &lhs, const WLANProfileSsid_t &rhs)
{
    return lhs.Compare(rhs) == 0;
}
inline bool
operator != (const WLANProfileSsid_t &lhs, const WLANProfileSsid_t &rhs)
{
    return lhs.Compare(rhs) != 0;
}

};
};

#endif /* if (WLAN_VERSION > 0) */
#endif /* _DEFINED_WLANProfileSsid_t_ */
// ----------------------------------------------------------------------------
