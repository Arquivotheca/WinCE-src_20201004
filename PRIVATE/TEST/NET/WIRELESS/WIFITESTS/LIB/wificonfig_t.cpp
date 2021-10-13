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
// Implementation of the WiFiConfig_t class.
//
// ----------------------------------------------------------------------------

#include "WiFiConfig_t.hpp"

#include "WiFiAccount_t.hpp"

#include <assert.h>

using namespace ce;
using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Constructor.
//
WiFiConfig_t::
WiFiConfig_t(
    APAuthMode_e    AuthMode,
    APCipherMode_e  CipherMode,
    APEapAuthMode_e EapAuthMode,
    int             KeyIndex,
    const TCHAR    *pKey,
    const TCHAR    *pSSID)
    : m_AuthMode   (AuthMode),
      m_CipherMode (CipherMode),
      m_EapAuthMode(EapAuthMode),
      m_KeyIndex   (KeyIndex)
{
    SafeCopy(m_Key, pKey, COUNTOF(m_Key));
    SafeCopy(m_SSIDName, pSSID, COUNTOF(m_SSIDName));
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
WiFiConfig_t::
~WiFiConfig_t(void)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Configures the WZC service to connect the specified Access Point.
//
DWORD
WiFiConfig_t::
ConnectWiFi(
    WZCService_t  *pWZCService,
    WiFiAccount_t *pEapCredentials)
{
    DWORD result;

    assert(TEXT('\0') != m_SSIDName[0]);

    // Generate the WZC configuration.
    WZCConfigItem_t config;
    result = config.InitInfrastructure(GetSSIDName(),
                                       GetWZCAuthMode(),
                                       GetWZCCipherMode(),
                                       GetWZCEapAuthMode(),
                                       GetKeyIndex(),
                                       GetKey());
    if (ERROR_SUCCESS != result)
        return result;

#if 0
    LogDebug(TEXT("New WZC config item:"));
    config.Log(LogDebug, TEXT("  "), true);
#endif

    // If necessary, start a sub-thread to log in the user during
    // the WiFi association.
    if (NULL != pEapCredentials)
    {
        result = pEapCredentials->StartUserLogon(GetSSIDName(),
                                                 m_EapAuthMode);
        if (ERROR_SUCCESS != result)
            return result;
    }

    // Add the WiFi config to the preferred list.
    return pWZCService->AddToHeadOfPreferredList(config);
}

// ----------------------------------------------------------------------------
//
// Translates between APController and WZC security modes.
//
NDIS_802_11_AUTHENTICATION_MODE
WiFiConfig_t::
APAuthMode2WZC(
    APAuthMode_e AuthMode)
{
    NDIS_802_11_AUTHENTICATION_MODE retval;
    switch (AuthMode)
    {
        default:               retval = Ndis802_11AuthModeOpen;    break;
        case APAuthShared:     retval = Ndis802_11AuthModeShared;  break;
        case APAuthWEP_802_1X: retval = Ndis802_11AuthModeOpen;    break;
        case APAuthWPA:        retval = Ndis802_11AuthModeWPA;     break;
        case APAuthWPA_PSK:    retval = Ndis802_11AuthModeWPAPSK;  break;
#ifdef WZC_IMPLEMENTS_WPA2
        case APAuthWPA2:       retval = Ndis802_11AuthModeWPA2;    break;
        case APAuthWPA2_PSK:   retval = Ndis802_11AuthModeWPA2PSK; break;
#endif
    }
    return retval;
}

APAuthMode_e
WiFiConfig_t::
WZCAuthMode2AP(
    NDIS_802_11_AUTHENTICATION_MODE AuthMode)
{
    APAuthMode_e retval;
    switch (AuthMode)
    {
        default:                        retval = APAuthOpen;     break;
        case Ndis802_11AuthModeShared:  retval = APAuthShared;   break;
        case Ndis802_11AuthModeWPA:     retval = APAuthWPA;      break;
        case Ndis802_11AuthModeWPAPSK:  retval = APAuthWPA_PSK;  break;
#ifdef WZC_IMPLEMENTS_WPA2
        case Ndis802_11AuthModeWPA2:    retval = APAuthWPA2;     break;
        case Ndis802_11AuthModeWPA2PSK: retval = APAuthWPA2_PSK; break;
#endif
    }
    return retval;
}

ULONG 
WiFiConfig_t::
APCipherMode2WZC(
    APCipherMode_e CipherMode)
{
    ULONG retval;
    switch (CipherMode)
    {
        default:           retval = Ndis802_11WEPDisabled;        break;
        case APCipherWEP:  retval = Ndis802_11WEPEnabled;         break;
        case APCipherTKIP: retval = Ndis802_11Encryption2Enabled; break;
        case APCipherAES:  retval = Ndis802_11Encryption3Enabled; break;
    }
    return retval;
}

APCipherMode_e
WiFiConfig_t::
WZCCipherMode2AP(
    ULONG CipherMode)
{
    APCipherMode_e retval;
    switch (CipherMode)
    {
        default:                           retval = APCipherClearText; break;
        case Ndis802_11WEPEnabled:         retval = APCipherWEP;       break;
        case Ndis802_11Encryption2Enabled: retval = APCipherTKIP;      break;
        case Ndis802_11Encryption3Enabled: retval = APCipherAES;       break;
    }
    return retval;
}

DWORD 
WiFiConfig_t::
APEapAuthMode2WZC(
    APEapAuthMode_e EapAuthMode)
{
    DWORD retval;
    switch (EapAuthMode)
    {
        default:            retval = EAP_TYPE_TLS;  break;
        case APEapAuthPEAP: retval = EAP_TYPE_PEAP; break;
#ifdef EAP_MD5_SUPPORTED
        case APEapAuthMD5:  retval = EAP_TYPE_MD5;  break;
#endif
    }
    return retval;
}

APEapAuthMode_e
WiFiConfig_t::
WZCEapAuthMode2AP(
    DWORD EapAuthMode)
{
    APEapAuthMode_e retval;
    switch (EapAuthMode)
    {
        default:            retval = APEapAuthTLS;  break;
        case EAP_TYPE_PEAP: retval = APEapAuthPEAP; break;
#ifdef EAP_MD5_SUPPORTED
        case EAP_TYPE_MD5:  retval = APEapAuthMD5;  break;
#endif
    }
    return retval;
}

// ----------------------------------------------------------------------------
