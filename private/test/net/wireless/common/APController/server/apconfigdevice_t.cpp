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
// Implementation of the APConfigDevice_t class.
//
// ----------------------------------------------------------------------------

#include "APConfigDevice_t.hpp"

#include "APCUtils.hpp"
#include "HtmlAPController_t.hpp"
#include "ManualAPConfigDevice_t.hpp"

#include <CmdArg_t.hpp>

#include <assert.h>
#include <tchar.h>
#include <strsafe.h>

#ifdef UNDER_CE
#include <winsock2.h>
#endif

using namespace ce::qa;

// Only set this if you want a LOT of debug output:
//#define EXTRA_DEBUG 1

// ----------------------------------------------------------------------------
//
// Constructor.
//
APConfigDevice_t::
APConfigDevice_t(
    const TCHAR        *pAPName,
    const TCHAR        *pConfigKey,
    const TCHAR        *pDeviceType,
    const TCHAR        *pDeviceName,
    DeviceController_t *pDevice)
    : APConfigurator_t(pAPName),
      AccessPointController_t(pDeviceType, pDeviceName),
      m_ConfigKey(pConfigKey),
      m_pDevice(pDevice)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
APConfigDevice_t::
~APConfigDevice_t(void)
{
    if (NULL != m_pDevice)
    {
        delete m_pDevice;
        m_pDevice = NULL;
    }
}

// ----------------------------------------------------------------------------
//
// Generates an object from the registry.
//
HRESULT
APConfigDevice_t::
CreateConfigurator(
    const TCHAR       *pRootKey,
    const TCHAR       *pAPName,
    APConfigDevice_t **ppConfig)
{
    HRESULT hr;
    DWORD result;
    ce::tstring errors;

    auto_ptr<DeviceController_t> pDevice;
    auto_ptr<APConfigDevice_t>   pConfig;

    LogDebug(TEXT("CreateConfigurator(\"%s\\%s\")"), pRootKey, pAPName);

    // Open the registry.
    ce::tstring configKey;
    if (!configKey.assign(pRootKey)
     || !configKey.append(TEXT("\\"))
     || !configKey.append(pAPName))
        return E_OUTOFMEMORY;

    auto_hkey apHkey;
    result = RegOpenKeyEx(HKEY_CURRENT_USER, configKey, 0, KEY_READ, &apHkey);
    if (NO_ERROR != result)
    {
        LogError(TEXT("Cannot open AP-config \"%s\": %s"),
                 &configKey[0], Win32ErrorText(result));
        return HRESULT_FROM_WIN32(result);
    }

    // Load the configurator-type parameter.
    CmdArgString_t configuratorType(ConfiguratorKey, NULL,
                                    TEXT("AP-configurator type"));
    result = configuratorType.ReadRegistry(apHkey, configKey);
    if (NO_ERROR != result)
        return HRESULT_FROM_WIN32(result);

    // Separate the config-type and name/specifications.
    ce::tstring lstring(configuratorType);
    static const TCHAR semicolon[] = TEXT(";");
    const TCHAR *type, *name;
    type = name = NULL;
    TCHAR *nextToken = NULL;  
    for (TCHAR *token = _tcstok_s(lstring.get_buffer(), semicolon, &nextToken) ;
                token != NULL ;
                token = _tcstok_s(NULL, semicolon, &nextToken))
    {
        if (NULL == type) { type = WiFUtils::TrimToken(token); }
        else              { name = WiFUtils::TrimToken(token); break; }
    }
    if (NULL == type)
    {
        LogError(TEXT("Bad AP configurator type \"%s\""), &configuratorType[0]);
        return E_INVALIDARG;
    }
    if (NULL == name)
        name = TEXT("");

    // Create the device and registry interfaces.
    if (0 == _tcsicmp(type, TEXT("manual")))
    {
        pConfig = new ManualAPConfigDevice_t(pAPName, configKey,
                                             type, name);
    }
    else
    {
        result = APCUtils::CreateController(type, name, &pDevice);
        if (NO_ERROR != result)
            return HRESULT_FROM_WIN32(result);

        assert(pDevice.valid());
        pConfig = new APConfigDevice_t(pAPName, configKey,
                                       type, name, pDevice);
    }
    if (!pConfig.valid())
    {
        LogError(TEXT("Cannot allocate \"%s-%s\" device-controller"),
                 type, name);
        return E_OUTOFMEMORY;
    }
    pDevice.release();

    // Load the rest of the configuration.
    hr = pConfig->LoadConfiguration(apHkey);
    if (FAILED(hr))
        return hr;

   *ppConfig = pConfig.release();
    return S_OK;
}

// ----------------------------------------------------------------------------
//
// Loads the initial AP configuration from the registry.
//
HRESULT
APConfigDevice_t::
LoadConfiguration(HKEY apHkey)
{
    HRESULT hr;
    LONG    result;
    DWORD   dataType, dataSize;
    ce::tstring str;

    const TCHAR *pConfigKey = GetConfigKey();

    // Vendor name and model number:
    hr = WiFUtils::ReadRegString(apHkey, pConfigKey,
                                 VendorNameKey, &str, TEXT(""));
    if (FAILED(hr))
        return hr;
    hr = m_State.SetVendorName(str);
    if (FAILED(hr))
    {
        LogError(TEXT("AP-config %s\\%s \"%s\" is invalid"),
                 pConfigKey, VendorNameKey, &str[0]);
        return hr;
    }

    hr = WiFUtils::ReadRegString(apHkey, pConfigKey,
                                 ModelNumberKey, &str, TEXT(""));
    if (FAILED(hr))
        return hr;
    hr = m_State.SetModelNumber(str);
    if (FAILED(hr))
    {
        LogError(TEXT("AP-config %s\\%s \"%s\" is invalid"),
                 pConfigKey, ModelNumberKey, &str[0]);
        return hr;
    }

    // Capabilities implemented:
    int caps;
    hr = WiFUtils::ReadRegString(apHkey, pConfigKey,
                                 CapsImplementedKey, &str, TEXT(""));
    if (FAILED(hr))
        return hr;
    
    caps = String2APCapabilities(str);
    if (caps < 0)
    {
        LogError(TEXT("AP-config %s\\%s type \"%s\" is unrecognized"),
                 pConfigKey, CapsImplementedKey, &str[0]);
        return E_INVALIDARG;
    }
    m_State.SetCapabilitiesImplemented(caps);

    TCHAR implemented[128];
    hr = APCapabilities2String(m_State.GetCapabilitiesImplemented(),
                               implemented, COUNTOF(implemented));
    if (FAILED(hr))
        return hr;

    // Capabilities enabled:
    hr = WiFUtils::ReadRegString(apHkey, pConfigKey,
                                 CapsEnabledKey, &str, implemented);
    if (FAILED(hr))
        return hr;
    
    caps = String2APCapabilities(str);
    if (caps < 0)
    {
        LogError(L"AP-config %s\\%s \"%s\" is unrecognized",
                 pConfigKey, CapsEnabledKey, &str[0]);
        return hr;
    }

    hr = SetCapabilitiesEnabled(caps);
    if (FAILED(hr))
    {
        LogError(L"AP-config %s\\%s \"%s\" does not match implemented \"%s\"",
                 pConfigKey, CapsEnabledKey, &str[0], implemented);
        return hr;
    }

    // Radio status.
    DWORD radioState;
    hr = WiFUtils::ReadRegDword(apHkey, pConfigKey,
                                RadioStateKey, &radioState, 1);
    if (FAILED(hr))
        return hr;
    m_State.SetRadioState(radioState != 0);

    // SSID.
    hr = WiFUtils::ReadRegString(apHkey, pConfigKey,
                                 SsidKey, &str, NULL);
    if (FAILED(hr))
        return hr;
    hr = SetSsid(str);
    if (FAILED(hr))
    {
        LogError(TEXT("AP-config %s\\%s \"%s\" is invalid"),
                 pConfigKey, SsidKey, &str[0]);
        return hr;
    }

    // BSSID.
    MACAddr_t bssid;
    dataSize = sizeof(bssid.m_Addr);
    result = RegQueryValueEx(apHkey, BSsidKey, NULL, &dataType,
                             bssid.m_Addr, &dataSize);
    switch (result)
    {
        case ERROR_NOT_FOUND:
        case ERROR_FILE_NOT_FOUND:
            break;

        case NO_ERROR:
            if (dataType != REG_BINARY)
            {
                LogError(TEXT("Bad data-type %d for \"%s\\%s\""),
                         dataType, pConfigKey, BSsidKey);
                return E_INVALIDARG;
            }
            if (dataSize != sizeof(bssid.m_Addr))
            {
                LogError(TEXT("Bad data-size %d for \"%s\\%s\""),
                         dataSize, pConfigKey, BSsidKey);
                return E_INVALIDARG;
            }
#ifdef EXTRA_DEBUG
            LogDebug(TEXT("    setting %s to:"), BSsidKey);
            for (int bx = 0 ; bx < (int)sizeof(bssid.m_Addr) ; ++bx)
            {
                LogDebug(TEXT("      %02d = %02x"), bx,
                         (int)bssid.m_Addr[bx]);
            }
#endif
            break;

        default:
            LogError(TEXT("Cannot read reg value \"%s\\%s\": %s"),
                     pConfigKey, BSsidKey,
                     Win32ErrorText(result));
            return HRESULT_FROM_WIN32(result);
    }
    m_State.SetBSsid(bssid);

    // Authentication and cipher:
    hr = WiFUtils::ReadRegString(apHkey, pConfigKey,
                                 AuthenticationKey, &str, TEXT("Open"));
    if (FAILED(hr))
        return hr;

    APAuthMode_e authMode = String2APAuthMode(str);
    if ((int)authMode < 0)
    {
        LogError(TEXT("AP-config %s\\%s type \"%s\" is unrecognized"),
                 pConfigKey, AuthenticationKey, &str[0]);
        return E_INVALIDARG;
    }

    hr = WiFUtils::ReadRegString(apHkey, pConfigKey,
                                 CipherKey, &str, TEXT("ClearText"));
    if (FAILED(hr))
        return hr;

    APCipherMode_e cipherMode = String2APCipherMode(str);
    if ((int)cipherMode < 0)
    {
        LogError(TEXT("AP-config %s\\%s type \"%s\" is unrecognized"),
                 pConfigKey, CipherKey, &str[0]);
        return E_INVALIDARG;
    }

    hr = SetSecurityMode(authMode, cipherMode);
    if (FAILED(hr))
    {
        LogError(TEXT("AP-config %s security-modes \"%s:%s\" are invalid"),
                 pConfigKey, APAuthMode2String(authMode),
                             APCipherMode2String(cipherMode));
        return E_INVALIDARG;
    }

    // Radius
    DWORD       port;
    ce::tstring server;
    ce::tstring secret;
    hr = WiFUtils::ReadRegString(apHkey, pConfigKey,
                                 RadiusServerKey, &server, TEXT("10.10.0.1"));
    if (FAILED(hr))
        return hr;

    hr = WiFUtils::ReadRegDword (apHkey, pConfigKey,
                                 RadiusPortKey, &port, 1812);
    if (FAILED(hr))
        return hr;

    hr = WiFUtils::ReadRegString(apHkey, pConfigKey,
                                 RadiusSecretKey, &secret, TEXT("0123456789"));
    if (FAILED(hr))
        return hr;

    hr = SetRadius(server, port, secret);
    if (FAILED(hr))
    {
        LogError(TEXT("AP-config %s\\%s/%s/%s \"%s/%u/%s\" is invalid"),
                 pConfigKey, RadiusServerKey, RadiusPortKey, RadiusSecretKey,
                                  &server[0],       port,         &secret[0]);
        return hr;
    }

    // WEP Keys
    WEPKeys_t keyData;
    TCHAR     keyName[32];
    for (int kx = 0 ; kx < 4 ; ++kx)
    {
        keyData.m_Size[kx] = 0;

        hr = StringCchPrintf(keyName, COUNTOF(keyName),
                             TEXT("%s%d"), WEPKeyKey, kx);
        if (FAILED(hr))
            return hr;

        dataSize = sizeof(keyData.m_Material[kx]);
        result = RegQueryValueEx(apHkey, keyName, NULL, &dataType,
                                &keyData.m_Material[kx][0], &dataSize);
        switch (result)
        {
            case ERROR_NOT_FOUND:
            case ERROR_FILE_NOT_FOUND:
                continue;

            case NO_ERROR:
                if (dataType != REG_BINARY)
                {
                    LogError(TEXT("Bad data-type %d for \"%s\\%s\""),
                             dataType, pConfigKey, keyName);
                    return E_INVALIDARG;
                }
                if (!keyData.ValidKeySize(dataSize))
                {
                    LogError(TEXT("Bad data-size %d for \"%s\\%s\""),
                             dataSize, pConfigKey, keyName);
                    return E_INVALIDARG;
                }
                break;

            default:
                LogError(TEXT("Cannot read reg value \"%s\\%s\": %s"),
                         pConfigKey, keyName,
                         Win32ErrorText(result));
                return HRESULT_FROM_WIN32(result);
        }

        keyData.m_Size[kx] = static_cast<BYTE> (dataSize);
#if EXTRA_DEBUG
        LogDebug(TEXT("    loading key \"%s\""), keyName);
        for (DWORD dx = 0 ; dx < dataSize ; ++dx)
        {
            LogDebug(TEXT("      %s%d[%02u] = %02x"),
                     WEPKeyKey, kx, dx,
                     (int)keyData.m_Material[kx][dx]);
        }
#endif
    }

    DWORD wepKeyIndex;
    hr = WiFUtils::ReadRegDword(apHkey, pConfigKey,
                                WEPIndexKey, &wepKeyIndex, 0);
    if (FAILED(hr))
        return hr;
    if (0 <= wepKeyIndex && wepKeyIndex < 4)
    {
        keyData.m_KeyIndex = static_cast<BYTE>(wepKeyIndex);
    }
    else
    {
        LogError(TEXT("AP-config %s\\%s \"%u\" is invalid"),
                 pConfigKey, WEPIndexKey, wepKeyIndex);
        return E_INVALIDARG;
    }

    m_State.SetWEPKeys(keyData);

    // Passphrase
    hr = WiFUtils::ReadRegString(apHkey, pConfigKey,
                                 PassphraseKey, &str, TEXT(""));
    if (FAILED(hr))
        return hr;

    hr = SetPassphrase(str);
    if (FAILED(hr))
    {
        LogError(TEXT("AP-config %s\\%s \"%s\" is invalid"),
                 pConfigKey, PassphraseKey, &str[0]);
        return hr;
    }

    // Broadcast-SSID mode.
    DWORD broadcast;
    hr = WiFUtils::ReadRegDword(apHkey, pConfigKey,
                                BroadcastSSIDKey, &broadcast, 1);
    if (FAILED(hr))
        return hr;
    m_State.SetSsidBroadcast(broadcast != 0);

    // Initialize the device's state.
    m_State.SetFieldFlags(0);
    ((AccessPointController_t *)m_pDevice)->SetInitialState(m_State);
    return S_OK;
}

// ----------------------------------------------------------------------------
//
// Generates a configurator-type parameter for use by CreateConfigurator.
//
HRESULT
APConfigDevice_t::
CreateConfiguratorType(ce::tstring *pConfigParam) const
{
    TCHAR buff[64];
    int   buffChars = COUNTOF(buff);

    HRESULT hr = StringCchPrintf(buff, buffChars, TEXT("%s;%s"),
                                 GetDeviceType(), GetDeviceName());
    if (FAILED(hr))
        return hr;

    return pConfigParam->assign(buff)? NO_ERROR : E_OUTOFMEMORY;
}

// ----------------------------------------------------------------------------
//
// Gets the current configuration of an Access Point.
//
DWORD
APConfigDevice_t::
GetAccessPoint(
    AccessPointState_t *pResponse)
{
    if (NULL == m_pDevice)
    {
        assert(NULL != m_pDevice);
        return ERROR_INVALID_HANDLE;
    }
    
    ce::gate<ce::critical_section> locker(m_Locker);
    
    DWORD result = m_pDevice->GetAccessPoint(pResponse);
    if (NO_ERROR == result)
    {
        m_State = *pResponse;
        m_State.SetFieldFlags(0);
    }
    
    return result;
}

// ----------------------------------------------------------------------------
//
// Updates the configuration of an Access Point.
//
DWORD
APConfigDevice_t::
SetAccessPoint(
    const AccessPointState_t &NewState,
          AccessPointState_t *pResponse)
{
    if (NULL == m_pDevice)
    {
        assert(NULL != m_pDevice);
        return ERROR_INVALID_HANDLE;
    }

    ce::gate<ce::critical_section> locker(m_Locker);
    
    DWORD result = m_pDevice->SetAccessPoint(NewState, pResponse);
    if (NO_ERROR == result)
    {
        m_State = *pResponse;
        m_State.SetFieldFlags(NewState.GetFieldFlags());
        SaveConfiguration();
    }
    
    return result;
}

// ----------------------------------------------------------------------------
//
// Saves the updated configuration values to the registry.
//
HRESULT
APConfigDevice_t::
SaveConfiguration(void)
{
    HRESULT hr;
    LONG result;
    
    TCHAR buff[64];
    int   buffChars = COUNTOF(buff);

    const TCHAR *pConfigKey = GetConfigKey();

    // Open the AP's registry entry.
    auto_hkey apHkey;
    result = RegOpenKeyEx(HKEY_CURRENT_USER, pConfigKey,
                          0, KEY_WRITE, &apHkey);
    if (NO_ERROR != result)
    {
        LogError(TEXT("Cannot open AP-config \"%s\": %s"),
                 pConfigKey, Win32ErrorText(result));
        return HRESULT_FROM_WIN32(result);
    }

    // Initialize the config paramater and write it to the registry.
    ce::tstring configuratorType;
    hr = CreateConfiguratorType(&configuratorType);
    if (FAILED(hr))
        return hr;

    hr = WiFUtils::WriteRegString(apHkey, pConfigKey,
                                  ConfiguratorKey, configuratorType);
    if (FAILED(hr))
        return hr;

    // Capabilities.
    TCHAR caps[128];
    hr = APCapabilities2String(GetCapabilitiesImplemented(),
                               caps, COUNTOF(caps));
    if (FAILED(hr))
        return hr;

    hr = WiFUtils::WriteRegString(apHkey, pConfigKey, CapsImplementedKey, caps);
    if (FAILED(hr))
        return hr;

    hr = APCapabilities2String(GetCapabilitiesEnabled(),
                               caps, COUNTOF(caps));
    if (FAILED(hr))
        return hr;

    hr = WiFUtils::WriteRegString(apHkey, pConfigKey, CapsEnabledKey, caps);
    if (FAILED(hr))
        return hr;

    // Radio status.
    hr = WiFUtils::WriteRegDword(apHkey, pConfigKey, RadioStateKey,
                                 IsRadioOn()? 1:0);
    if (FAILED(hr))
        return hr;

    // SSID.
    hr = WiFUtils::WriteRegString(apHkey, pConfigKey, SsidKey, GetSsid());
    if (FAILED(hr))
        return hr;

    // BSSID.
#ifdef EXTRA_DEBUG
    LogDebug(L"  updating %s", BSsidKey);
#endif
    const MACAddr_t &bssid = GetBSsid();
    result = RegSetValueEx(apHkey, BSsidKey, 0, REG_BINARY,
                                  bssid.m_Addr,
                           sizeof(bssid.m_Addr));
    if (NO_ERROR != result)
    {
        LogError(TEXT("Cannot update reg value \"%s\\%s\": %s"),
                 pConfigKey, BSsidKey, Win32ErrorText(result));
        return HRESULT_FROM_WIN32(result);
    }

    // Authentication and cipher modes.
    hr = WiFUtils::WriteRegString(apHkey, pConfigKey, AuthenticationKey, 
                                  GetAuthName());
    if (FAILED(hr))
        return hr;

    hr = WiFUtils::WriteRegString(apHkey, pConfigKey, CipherKey, 
                                  GetCipherName());
    if (FAILED(hr))
        return hr;

    // Radius Server.
    in_addr inaddr;
    inaddr.s_addr = GetRadiusServer();
    hr = StringCchPrintf(buff, buffChars, TEXT("%u.%u.%u.%u"),
                         inaddr.s_net, inaddr.s_host,
                         inaddr.s_lh, inaddr.s_impno);
    if (FAILED(hr))
        return hr;

    hr = WiFUtils::WriteRegString(apHkey, pConfigKey, RadiusServerKey, buff);
    if (FAILED(hr))
        return hr;

    hr = WiFUtils::WriteRegDword(apHkey, pConfigKey,  RadiusPortKey,
                                 GetRadiusPort());
    if (FAILED(hr))
        return hr;

    hr = WiFUtils::WriteRegString(apHkey, pConfigKey, RadiusSecretKey, 
                                  GetRadiusSecret());
    if (FAILED(hr))
        return hr;

    // WEP keys.
    WEPKeys_t wepKeys = GetWEPKeys();
    for (int kx = 0 ; kx < 4 ; ++kx)
    {
        hr = StringCchPrintf(buff, buffChars, TEXT("%s%d"), WEPKeyKey, kx);
        if (FAILED(hr))
            return hr;
#ifdef EXTRA_DEBUG
        LogDebug(TEXT("  updating %s"), buff);
#endif
        int length = static_cast<int> (wepKeys.m_Size[kx]);
        if (length > sizeof(wepKeys.m_Material[kx]))
            length = sizeof(wepKeys.m_Material[kx]);
        if (length == 0)
        {
            length = 5;
            memset(wepKeys.m_Material[kx], 0, sizeof(wepKeys.m_Material[kx]));
        }
        result = RegSetValueEx(apHkey, buff, 0, REG_BINARY,
                               wepKeys.m_Material[kx], length);
        if (NO_ERROR != result)
        {
            LogError(TEXT("Cannot update reg value \"%s\\%s\": %s"),
                     pConfigKey, buff, Win32ErrorText(result));
            return HRESULT_FROM_WIN32(result);
        }
    }
    hr = WiFUtils::WriteRegDword(apHkey, pConfigKey, WEPIndexKey,
                                 static_cast<DWORD>(wepKeys.m_KeyIndex));
    if (FAILED(hr))
        return hr;

    // Passphrase.
    hr = WiFUtils::WriteRegString(apHkey, pConfigKey, PassphraseKey, 
                                  GetPassphrase());
    if (FAILED(hr))
        return hr;

    // Broadcast-SSID mode.
    hr = WiFUtils::WriteRegDword(apHkey, pConfigKey, BroadcastSSIDKey,
                                 IsSsidBroadcast()? 1:0);
    if (FAILED(hr))
        return hr;
    
    return S_OK;
}

// ----------------------------------------------------------------------------
