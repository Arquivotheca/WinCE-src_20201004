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
// Implementation of the APConfigurator_t class.
//
// ----------------------------------------------------------------------------

#include "APConfigurator_t.hpp"

#include <assert.h>

#ifdef UNDER_CE
#include <winsock2.h>
#endif

using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Registry keys:
//
const TCHAR * const APConfigurator_t::ConfiguratorKey    = TEXT("Configurator");
const TCHAR * const APConfigurator_t::AuthenticationKey  = TEXT("Authentication");
const TCHAR * const APConfigurator_t::BSsidKey           = TEXT("BSsid");
const TCHAR * const APConfigurator_t::CapsEnabledKey     = TEXT("CapsEnabled");
const TCHAR * const APConfigurator_t::CapsImplementedKey = TEXT("CapsImplemented");
const TCHAR * const APConfigurator_t::CipherKey          = TEXT("Cipher");
const TCHAR * const APConfigurator_t::ModelNumberKey     = TEXT("Model");
const TCHAR * const APConfigurator_t::PassphraseKey      = TEXT("Passphrase");
const TCHAR * const APConfigurator_t::RadioStateKey      = TEXT("RadioState");
const TCHAR * const APConfigurator_t::RadiusPortKey      = TEXT("RadiusPort");
const TCHAR * const APConfigurator_t::RadiusSecretKey    = TEXT("RadiusSecret");
const TCHAR * const APConfigurator_t::RadiusServerKey    = TEXT("RadiusServer");
const TCHAR * const APConfigurator_t::SsidKey            = TEXT("Ssid");
const TCHAR * const APConfigurator_t::VendorNameKey      = TEXT("Vendor");
const TCHAR * const APConfigurator_t::WEPKeyKey          = TEXT("WEPKey");
const TCHAR * const APConfigurator_t::WEPIndexKey        = TEXT("WEPIndex");

// ----------------------------------------------------------------------------
//
// Constructor.
//
APConfigurator_t::
APConfigurator_t(const TCHAR *pAPName)
    : m_APName(pAPName)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
APConfigurator_t::
~APConfigurator_t(void)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// (Re)connects to the access point device and gets the current
// configuration values. This default implementation does nothing.
//
HRESULT
APConfigurator_t::
Reconnect(void)
{
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Gets the AP vendor name or model number.
//
const TCHAR *
APConfigurator_t::
GetVendorName(void)
{
    Reconnect();
    m_State.GetVendorName(m_VendorName, COUNTOF(m_VendorName),
                          AccessPointState_t::TranslationFailuresAreLogged);
    return m_VendorName;
}
const TCHAR *
APConfigurator_t::
GetModelNumber(void)
{
    Reconnect();
    m_State.GetModelNumber(m_ModelNumber, COUNTOF(m_ModelNumber),
                           AccessPointState_t::TranslationFailuresAreLogged);
    return m_ModelNumber;
}

// ----------------------------------------------------------------------------
//
// Gets or sets the SSID (WLAN Service Set Identifier / Name):
//
const TCHAR *
APConfigurator_t::
GetSsid(void)
{
    Reconnect();
    m_State.GetSsid(m_Ssid, COUNTOF(m_Ssid),
                    AccessPointState_t::TranslationFailuresAreLogged);
    return m_Ssid;
}
HRESULT
APConfigurator_t::
SetSsid(const TCHAR *pSsid)
{
    HRESULT hr = Reconnect();
    if (FAILED(hr))
        return hr;

    if (NULL == pSsid || TEXT('\0') == pSsid[0])
    {
        return E_INVALIDARG;
    }
    else
    {
        return m_State.SetSsid(pSsid);
    }
}

// ----------------------------------------------------------------------------
//
// Gets or sets the BSSID (Basic Service Set Identifier):
//
const MACAddr_t &
APConfigurator_t::
GetBSsid(void)
{
    Reconnect();
    return m_State.GetBSsid();
}
HRESULT
APConfigurator_t::
SetBSsid(const MACAddr_t &BSsid)
{
    HRESULT hr = Reconnect();
    if (FAILED(hr))
        return hr;

    m_State.SetBSsid(BSsid);
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Gets or sets the bit-map describing the AP's capabilities:
//
int
APConfigurator_t::
GetCapabilitiesImplemented(void)
{
    Reconnect();
    return m_State.GetCapabilitiesImplemented();
}
int
APConfigurator_t::
GetCapabilitiesEnabled(void)
{
    Reconnect();
    return m_State.GetCapabilitiesEnabled();
}
HRESULT
APConfigurator_t::
SetCapabilitiesEnabled(int Capabilities)
{
    HRESULT hr = Reconnect();
    if (FAILED(hr))
        return hr;

    if ((~m_State.GetCapabilitiesImplemented() & Capabilities) != 0)
    {
        return E_INVALIDARG;
    }
    else
    {
        m_State.SetCapabilitiesEnabled(Capabilities);
        return ERROR_SUCCESS;
    }
}

// ----------------------------------------------------------------------------
//
// Gets or sets the AP's radio on/off status:
//
bool
APConfigurator_t::
IsRadioOn(void)
{
    Reconnect();
    return m_State.IsRadioOn();
}
HRESULT
APConfigurator_t::
SetRadioState(bool State)
{
    HRESULT hr = Reconnect();
    if (FAILED(hr))
        return hr;

    m_State.SetRadioState(State);
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Gets or sets the security modes:
//
APAuthMode_e
APConfigurator_t::
GetAuthMode(void)
{
    Reconnect();
    return m_State.GetAuthMode();
}
APCipherMode_e
APConfigurator_t::
GetCipherMode(void)
{
    Reconnect();
    return m_State.GetCipherMode();
}
HRESULT
APConfigurator_t::
SetSecurityMode(
    APAuthMode_e   AuthMode,
    APCipherMode_e CipherMode)
{
    HRESULT hr = Reconnect();
    if (FAILED(hr))
        return hr;

    if (!ValidateSecurityModes(AuthMode, CipherMode,
                               m_State.GetCapabilitiesEnabled()))
    {
        return E_INVALIDARG;
    }
    else
    {
        m_State.SetAuthMode(AuthMode);
        m_State.SetCipherMode(CipherMode);
        return ERROR_SUCCESS;
    }
}

// ----------------------------------------------------------------------------
//
// Gets or sets the RADIUS Server information:
//
DWORD
APConfigurator_t::
GetRadiusServer(void)
{
    Reconnect();
    return m_State.GetRadiusServer();
}
int
APConfigurator_t::
GetRadiusPort(void)
{
    Reconnect();
    return m_State.GetRadiusPort();
}
const TCHAR *
APConfigurator_t::
GetRadiusSecret(void)
{
    Reconnect();
    m_State.GetRadiusSecret(m_RadiusSecret, COUNTOF(m_RadiusSecret),
                            AccessPointState_t::TranslationFailuresAreLogged);
    return m_RadiusSecret;
}
HRESULT
APConfigurator_t::
SetRadius(
    const TCHAR *pServer,
    int          Port,
    const TCHAR *pKey)
{
    HRESULT hr = Reconnect();
    if (FAILED(hr))
        return hr;

    if (NULL == pServer || NULL == pKey)
        return E_INVALIDARG;

    ce::string mbServer;
    hr = WiFUtils::ConvertString(&mbServer, pServer);
    if (FAILED(hr))
        return hr;

    DWORD server = inet_addr(mbServer);
    if (server == INADDR_NONE)
    {
        LogError(TEXT("Cannot interpret radius-server address \"%s\""),
                 pServer);
        return E_FAIL;
    }

    m_State.SetRadiusServer(server);
    m_State.SetRadiusPort(Port);
    return m_State.SetRadiusSecret(pKey);
}

// ----------------------------------------------------------------------------
//
// Gets or sets the WEP (Wired Equivalent Privacy) key.
//
const WEPKeys_t &
APConfigurator_t::
GetWEPKeys(void)
{
    Reconnect();
    return m_State.GetWEPKeys();
}
HRESULT
APConfigurator_t::
SetWEPKeys(const WEPKeys_t &KeyData)
{
    HRESULT hr = Reconnect();
    if (FAILED(hr))
        return hr;

    if (0 > KeyData.m_KeyIndex || KeyData.m_KeyIndex >= 4)
        return E_INVALIDARG;

    for (int kx = 0 ; kx < 4 ; ++kx)
    {
        int keySize = static_cast<int>(KeyData.m_Size[kx]);
        if (keySize && !KeyData.ValidKeySize(keySize))
        {
            return E_INVALIDARG;
        }
    }

    m_State.SetWEPKeys(KeyData);
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Gets or sets the pass-phrase.
//
const TCHAR *
APConfigurator_t::
GetPassphrase(void)
{
    Reconnect();
    m_State.GetPassphrase(m_Passphrase, COUNTOF(m_Passphrase),
                          AccessPointState_t::TranslationFailuresAreLogged);
    return m_Passphrase;
}
HRESULT
APConfigurator_t::
SetPassphrase(const TCHAR *Passphrase)
{
    HRESULT hr = Reconnect();
    if (FAILED(hr))
        return hr;

    if (NULL == Passphrase)
        return E_INVALIDARG;

    return m_State.SetPassphrase(Passphrase);
}

// ----------------------------------------------------------------------------
