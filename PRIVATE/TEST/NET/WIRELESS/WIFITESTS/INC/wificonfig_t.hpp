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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
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
// Definitions and declarations for the WiFiConfig_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WiFiConfig_t_
#define _DEFINED_WiFiConfig_t_
#pragma once

#include "Utils.hpp"
#include <APController_t.hpp>
#include "WZCService_t.hpp"

// Indicates whether MD5-Challenge EAP authentication is supported:
//#define EAP_MD5_SUPPORTED 1

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Contains all the data-elements and methods required to connect to a
// wireless network.
//
class WiFiAccount_t;
class WiFiConfig_t
{
public:

    // Maximum WEP key or PSK passphrase length:
    enum { MaximumKeyChars = 128 };

private:

    // Security modes:
    APAuthMode_e    m_AuthMode;
    APCipherMode_e  m_CipherMode;
    APEapAuthMode_e m_EapAuthMode;

    // WEP key or PSK passphrase:
    int   m_KeyIndex;
    TCHAR m_Key[MaximumKeyChars];

    // Access Point SSID name:
    TCHAR m_SSIDName[MAX_SSID_LEN+1];

public:

    // Constructor and destructor:
    WiFiConfig_t(
        APAuthMode_e    AuthMode    = APAuthOpen,
        APCipherMode_e  CipherMode  = APCipherClearText,
        APEapAuthMode_e EapAuthMode = APEapAuthTLS,
        int             KeyIndex    = 0,
        const TCHAR    *pKey        = NULL,
        const TCHAR    *pSSIDName   = NULL);
   ~WiFiConfig_t(void);

    // Accessors:
    APAuthMode_e
    GetAuthMode(void) const
    {
        return m_AuthMode;
    }
    const TCHAR *
    GetAuthName(void) const
    {
        return APAuthMode2String(m_AuthMode);
    }
    void
    SetAuthMode(APAuthMode_e Value)
    {
        m_AuthMode = Value;
    }

    APCipherMode_e
    GetCipherMode(void) const
    {
        return m_CipherMode;
    }
    const TCHAR *
    GetCipherName(void) const
    {
        return APCipherMode2String(m_CipherMode);
    }
    void
    SetCipherMode(APCipherMode_e Value)
    {
        m_CipherMode = Value;
    }

    APEapAuthMode_e
    GetEapAuthMode(void) const
    {
        return m_EapAuthMode;
    }
    const TCHAR *
    GetEapAuthName(void) const
    {
        return APEapAuthMode2String(m_EapAuthMode);
    }
    void
    SetEapAuthMode(APEapAuthMode_e Value)
    {
        m_EapAuthMode = Value;
    }

    int
    GetKeyIndex(void) const
    {
        return m_KeyIndex;
    }
    void
    SetKeyIndex(int Value)
    {
        m_KeyIndex = Value;
    }

    const TCHAR *
    GetKey(void) const
    {
        return m_Key;
    }
    void
    SetKey(const TCHAR *pValue)
    {
        SafeCopy(m_Key, pValue, COUNTOF(m_Key));
    }

    const TCHAR *
    GetSSIDName(void) const
    {
        return m_SSIDName;
    }
    void
    SetSSIDName(const TCHAR *pValue)
    {
        SafeCopy(m_SSIDName, pValue, COUNTOF(m_SSIDName));
    }

    // Translates the APController security modes to WZC codes:
    NDIS_802_11_AUTHENTICATION_MODE
    GetWZCAuthMode(void) const
    {
        return APAuthMode2WZC(m_AuthMode);
    }

    ULONG
    GetWZCCipherMode(void) const
    {
        return APCipherMode2WZC(m_CipherMode);
    }

    DWORD
    GetWZCEapAuthMode(void) const
    {
        return APEapAuthMode2WZC(m_EapAuthMode);
    }

  // Utility methods:

    // Configures the WZC service to connect the specified Access Point:
    DWORD
    ConnectWiFi(
        WZCService_t  *pWZCService,
        WiFiAccount_t *pEapCredentials);

    // Translates between APController and WZC security modes:
    static NDIS_802_11_AUTHENTICATION_MODE
    APAuthMode2WZC(
        APAuthMode_e AuthMode);
    static APAuthMode_e
    WZCAuthMode2AP(
        NDIS_802_11_AUTHENTICATION_MODE AuthMode);

    static ULONG 
    APCipherMode2WZC(
        APCipherMode_e CipherMode);
    static APCipherMode_e
    WZCCipherMode2AP(
        ULONG CipherMode);

    static DWORD 
    APEapAuthMode2WZC(
        APEapAuthMode_e EapAuthMode);
    static APEapAuthMode_e
    WZCEapAuthMode2AP(
        DWORD EapAuthMode);
};

};
};
#endif /* _DEFINED_WiFiConfig_t_ */
// ----------------------------------------------------------------------------
