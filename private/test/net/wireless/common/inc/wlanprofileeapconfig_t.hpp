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
// Definitions and declarations for the WLANProfileEapConfig_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WLANProfileEapConfig_t_
#define _DEFINED_WLANProfileEapConfig_t_
#pragma once

#include "WLANTypes.hpp"
#if (WLAN_VERSION > 0)

#include <WLANProfileEapMethod_t.hpp>

#include <MemBuffer_t.hpp>
#include <wlanapi.h>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Represents an EAP-Authentication configuration element within an
// 802.1X authentication profile.
//
class WiFiAccount_t;
class WiFiConfig_t;
class WLANProfileEapConfig_t
{
private:

    // EAP configuration data:
    //
    MemBuffer_t m_ConfigData;

    // Is EAP data in binary (true) or XML (false):
    //
    bool m_bConfigBinary;

    // EAP method configuration:
    //
    WLANProfileEapMethod_t m_EapMethod;

public:

    // Constructor / destructor:
    //
    WLANProfileEapConfig_t(void) { Clear(); }
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

    // Is EAP data in binary (true) or XML (false):
    //
    bool
    IsConfigBinary(void) const { return m_bConfigBinary; }

    // EAP Configuration data:
    //
    const TCHAR *
    GetConfigXML(void) const;
    DWORD
    GetConfigBinary(
      __out_ecount(*pBufferBytes) BYTE  *pBuffer,
                                  DWORD *pBufferBytes) const;

    DWORD
    SetConfigXML(
        const TCHAR *pConfig);
    DWORD
    SetConfigBinary(
        const BYTE *pConfig,
        DWORD        ConfigBytes);

    // EAP Method:
    //
    const WLANProfileEapMethod_t &
    GetEapMethod(void) const { return m_EapMethod; }
    void
    SetEapMethod(const WLANProfileEapMethod_t &EapMethod) { m_EapMethod = EapMethod; }

  // XML encoding/decoding:

    // XML tags:
    //
    static const WCHAR s_XmlTagName[];

    static const WCHAR s_EapHostConfigTagName  [];
    static const WCHAR s_EapHostConfigNamespace[];


    // Encodes the object into a DOM element:
    //
    HRESULT
    EncodeToXml(
        litexml::XmlElement_t **ppRoot,
        const TCHAR            *pNamespace = s_EapHostConfigNamespace) const;

    // Initializes the object from the specified DOM element:
    //
    HRESULT
    DecodeFromXml(
        const litexml::XmlElement_t &Root);
    
};

};
};

#endif /* if (WLAN_VERSION > 0) */
#endif /* _DEFINED_WLANProfileEapConfig_t_ */
// ----------------------------------------------------------------------------
