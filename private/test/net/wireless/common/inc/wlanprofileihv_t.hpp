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
// Definitions and declarations for the WLANProfileIHV_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WLANProfileIHV_t_
#define _DEFINED_WLANProfileIHV_t_t_
#pragma once

#include "WLANTypes.hpp"
#if (WLAN_VERSION > 0)

#include <wlanapi.h>
#include <WLANProfileMSM_t.hpp>

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
class WLANProfileIHV_t : public WLANProfileMSM_t
{
private:

public:

    // Constructor / destructor:
    //
    WLANProfileIHV_t(void) { Clear(); }

  // XML encoding/decoding:

    // XML tags:
    //
    static const WCHAR XmlTagName[];
    static const WCHAR XmlNamespace[];
    
  __override virtual const WCHAR*
    GetXmlTagName() const;
  __override virtual const WCHAR*
    GetXmlNamespace() const;

    // Encodes the object into a DOM element:
    //
  __override virtual HRESULT
    EncodeToXml(
        litexml::XmlElement_t **ppRoot,
        const TCHAR            *pNamespace = XmlNamespace) const;

    // Initializes the object from the specified DOM element:
    //
  __override virtual HRESULT
    DecodeFromXml(
        const litexml::XmlElement_t &Root);
    
};


};
};

#endif /* if (WLAN_VERSION > 0) */
#endif /* _DEFINED_WLANProfileIHV_t_ */
// ----------------------------------------------------------------------------
