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
// Definitions and declarations for the WiFiConfig_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WiFiConfig_t_
#define _DEFINED_WiFiConfig_t_
#pragma once

#include "APCTypes.hpp"
#include "WiFiTypes.hpp"
#include "WiFUtils.hpp"

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Contains all the data-elements and methods required to connect to a
// wireless network.
//
class APController_t;
class CmdArgList_t;
class WiFiConfig_t
{
public:

    // Maximum WEP key or PSK passphrase length:
    //
    enum { MaximumKeyChars = 128 };

private:

    // Security modes:
    //
    APAuthMode_e    m_AuthMode;
    APCipherMode_e  m_CipherMode;
    APEapAuthMode_e m_EapAuthMode;

    // Is this an Ad Hoc connection?
    //
    bool m_fAdHoc;

    // Should the connection be attempted whenever the network is visible?
    //
    bool m_fAutoConnect;

    // Should the connection use an IHV Extensions profile?
    //
    bool m_fIhvProfile;
    
    // Is the Access Point broadcasting its SSID?
    //
    bool m_fSsidBroadcast;
    
    // WEP key or PSK passphrase:
    //
    int   m_KeyIndex;
    TCHAR m_Key[MaximumKeyChars];

    // Access Point SSID name:
    //
    TCHAR m_Ssid[MAX_SSID_CHARS+1];

public:

    // Constructor and destructor:
    //
    WiFiConfig_t(
        APAuthMode_e    AuthMode       = APAuthOpen,
        APCipherMode_e  CipherMode     = APCipherClearText,
        APEapAuthMode_e EapAuthMode    = APEapAuthTLS,
        int             KeyIndex       = 0,
        const TCHAR    *pKey           = NULL,
        const TCHAR    *pSsidName      = NULL,
        bool            fSsidBroadcast = true,
        bool            fAdHoc         = false);
   ~WiFiConfig_t(void);

    // Modify the configuration to connect to the specified Access Point:
    //
    DWORD
    FromAccessPoint(
        const APController_t &APConfig,
        APEapAuthMode_e       EapAuthMode = APEapAuthTLS);

    // Generates the CmdArg objects required to configure the object:
    //
    CmdArgList_t *
    GenerateCmdArgList(
        const TCHAR  *pKeyPrefix,
        const TCHAR  *pArgPrefix,
        const TCHAR  *pDescPrefix);

  // Accessors:

    // SSID name:
    // (default = none)
    //
    const TCHAR *
    GetSsid(void) const { return m_Ssid; }
    void
    SetSsid(const TCHAR *pValue) { SafeCopy(m_Ssid, pValue, COUNTOF(m_Ssid)); }

    // Is this an Ad Hoc connection?
    // (default = false)
    //
    bool
    IsAdHoc(void) const { return m_fAdHoc; }
    void
    SetAdHoc(bool Value) { m_fAdHoc = Value; }
    
    // Authentication mode:
    // (default = Open)
    //
    APAuthMode_e
    GetAuthMode(void) const { return m_AuthMode; }
    const TCHAR *
    GetAuthName(void) const { return APAuthMode2String(m_AuthMode); }
    void
    SetAuthMode(APAuthMode_e Value) { m_AuthMode = Value; }

    // Encryption mode:
    // (default = ClearText)
    //
    APCipherMode_e
    GetCipherMode(void) const { return m_CipherMode; }
    const TCHAR *
    GetCipherName(void) const { return APCipherMode2String(m_CipherMode); }
    void
    SetCipherMode(APCipherMode_e Value) { m_CipherMode = Value; }

    // EAP-authentication (802.1X) mode:
    // (default = TLS)
    //
    APEapAuthMode_e
    GetEapAuthMode(void) const { return m_EapAuthMode; }
    const TCHAR *
    GetEapAuthName(void) const { return APEapAuthMode2String(m_EapAuthMode); }
    void
    SetEapAuthMode(APEapAuthMode_e Value) { m_EapAuthMode = Value; }

    // Should the connection be attempted whenever the network is visible?
    // (default = true)
    //
    bool
    IsAutoConnect(void) const { return m_fAutoConnect; }
    void
    SetAutoConnect(bool Value) { m_fAutoConnect = Value; }

    // Should the connection use an IHV Extensions profile?
    // (default = false)
    //
    bool
    IsIhvProfile(void) const { return m_fIhvProfile; }
    void
    SetIhvProfile(bool Value) { m_fIhvProfile = Value; }

    // Is the Access Point broadcasting its SSID?
    // (default = true)
    //
    bool
    IsSsidBroadcast(void) const { return m_fSsidBroadcast; }
    void
    SetSsidBroadcast(bool Value) { m_fSsidBroadcast = Value; }

    // WEP key:
    //
    int
    GetKeyIndex(void) const { return m_KeyIndex; }
    void
    SetKeyIndex(int Value) { m_KeyIndex = Value; }

    const TCHAR *
    GetKey(void) const { return m_Key; }
    void
    SetKey(const TCHAR *pValue) { SafeCopy(m_Key, pValue, COUNTOF(m_Key)); }
 
};

};
};
#endif /* _DEFINED_WiFiConfig_t_ */
// ----------------------------------------------------------------------------
