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
// Implementation of the BuffaloBroadcomController_t class.
//
// ----------------------------------------------------------------------------

#include "BuffaloBroadcomController_t.hpp"
#include "AccessPointState_t.hpp"
#include "HttpPort_t.hpp"

#include <assert.h>
#include <strsafe.h>

// Define this if you want LOTS of debug output:
#define EXTRA_DEBUG 1

using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Wireless page field names and values:
//
static const WCHAR WirelessPage[] = L"/wireless.asp";
static long        WirelessRestart = 2000L; // update restart time

// Radio state:
static const char WiFiRadioStateName[] = "wl_radio";

// SSid:
static const char WiFiSsidName[] = "wl_ssid";

// SSid:
static const char WiFiSsidBroadcastName[] = "wl_closed";

// ----------------------------------------------------------------------------
//
// Security page field names and values:
//
static const WCHAR SecurityPage[] = L"/security.asp";
static long        SecurityRestart = 3000L; // update restart time

// 802.11 authentication:
static const char  SecAuthModeName [] = "wl_auth";
static const WCHAR SecAuthModeTrue [] = L"1";
static const WCHAR SecAuthModeFalse[] = L"0";

// 802.1x (radius):
static const char  SecRadiusModeName [] = "wl_auth_mode";
static const WCHAR SecRadiusModeTrue [] = L"radius";
static const WCHAR SecRadiusModeFalse[] = L"none";

// WPA:
static const char  SecWpaModeName [] = "wl_akm_wpa";
static const WCHAR SecWpaModeTrue [] = L"enabled";
static const WCHAR SecWpaModeFalse[] = L"disabled";

// WPA-PSK:
static const char  SecWpaPskModeName [] = "wl_akm_psk";
static const WCHAR SecWpaPskModeTrue [] = L"enabled";
static const WCHAR SecWpaPskModeFalse[] = L"disabled";

// WPA2:
static const char  SecWpa2ModeName [] = "wl_akm_wpa2";
static const WCHAR SecWpa2ModeTrue [] = L"enabled";
static const WCHAR SecWpa2ModeFalse[] = L"disabled";

// WPA2-PSK:
static const char  SecWpa2PskModeName [] = "wl_akm_psk2";
static const WCHAR SecWpa2PskModeTrue [] = L"enabled";
static const WCHAR SecWpa2PskModeFalse[] = L"disabled";

// WEP encryption:
static const char  SecWepCipherName [] = "wl_wep";
static const WCHAR SecWepCipherTrue [] = L"enabled";
static const WCHAR SecWepCipherFalse[] = L"disabled";

// WPA encryption:
static const char  SecWpaCipherName   [] = "wl_crypto";
static const WCHAR SecWpaCipherTkip   [] = L"tkip";
static const WCHAR SecWpaCipherAes    [] = L"aes";
static const WCHAR SecWpaCipherTkipAes[] = L"tkip+aes";

// Radius information:
static const char SecRadiusServerName[] = "wl_radius_ipaddr";
static const char SecRadiusPortName  [] = "wl_radius_port";
static const char SecRadiusSecretName[] = "wl_radius_key";

// WEP keys:
static const char SecWEPKeyName[] = "wl_key";

// Passphrase:
static const char SecPassphraseName[] = "wl_wpa_psk";

// ----------------------------------------------------------------------------
//
// Destructor.
//
BuffaloBroadcomController_t::
~BuffaloBroadcomController_t(void)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Loads settings from the WirelessPage.
//
static DWORD
LoadWirelessPage(
    HttpPort_t         *pHttp,
    AccessPointState_t *pState,
    ce::string         *pWebPage,
    HtmlForms_t        *pPageForms)
{
    HRESULT hr;
    DWORD result;
    ValueMapCIter it;

    // Fill in the model's capabilities.
    pState->SetVendorName(TEXT("Buffalo"));
    pState->SetModelNumber(TEXT("G54s (Broadcom)"));
    int caps = (int)(APCapsWEP_802_1X
                   | APCapsWPA  | APCapsWPA_PSK | APCapsWPA_AES
                   | APCapsWPA2 | APCapsWPA2_PSK | APCapsWPA2_TKIP);
    pState->SetCapabilitiesImplemented(caps);
    pState->SetCapabilitiesEnabled(caps);

    // Load and parse the wireless-lan page.
    result = HtmlAPTypeController_t::ParsePageRequest(pHttp, WirelessPage, 100L,
                                                      pWebPage, pPageForms, 1);
    if (ERROR_SUCCESS != result)
        return result;
    const ValueMap &params = pPageForms->GetFields(0);

    // Get the radio state.
    it = HtmlUtils::LookupParam(params, WiFiRadioStateName,
                                "Radio status", WirelessPage);
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;
    pState->SetRadioState(wcstol(it->second, NULL, 10) != 0);

    // Get the SSid.
    it = HtmlUtils::LookupParam(params, WiFiSsidName,
                                "SSID", WirelessPage);
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;
    hr = pState->SetSsid(it->second, it->second.length());
    if (FAILED(hr))
    {
        LogError(TEXT("Bad SSID value \"%ls\" in %ls"),
               &(it->second)[0], WirelessPage);
        return HRESULT_CODE(hr);
    }

    // Get the broadcast-SSID flag.
    it = HtmlUtils::LookupParam(params, WiFiSsidBroadcastName,
                                "broadcast-SSID", WirelessPage);
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;
    pState->SetSsidBroadcast(wcstol(it->second, NULL, 10) == 0);
    
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Loads settings from the SecurityPage.
//
static DWORD
LoadSecurityPage(
    HttpPort_t         *pHttp,
    AccessPointState_t *pState,
    ce::string         *pWebPage,
    HtmlForms_t        *pPageForms)
{
    HRESULT hr;
    DWORD result;
    ValueMapCIter it;

    // Load and parse the wireless-lan security page.
    result = HtmlAPTypeController_t::ParsePageRequest(pHttp, SecurityPage, 100L,
                                                      pWebPage, pPageForms, 1);
    if (ERROR_SUCCESS != result)
        return result;
    const ValueMap &params = pPageForms->GetFields(0);

    // Get the authentication mode.
    it = HtmlUtils::LookupParam(params, SecAuthModeName,
                                "Authentication mode", SecurityPage);
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;
    if (wcscmp(it->second, SecAuthModeTrue) == 0)
        pState->SetAuthMode(APAuthShared);
    else
    {
        it = HtmlUtils::LookupParam(params, SecWpaModeName,
                                    "WPA mode", SecurityPage);
        if (it == params.end())
            return ERROR_INVALID_PARAMETER;
        if (_wcsicmp(it->second, SecWpaModeTrue) == 0)
            pState->SetAuthMode(APAuthWPA);
        else
        {
            it = HtmlUtils::LookupParam(params, SecWpaPskModeName,
                                        "WPA-PSK mode", SecurityPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            if (_wcsicmp(it->second, SecWpaPskModeTrue) == 0)
                pState->SetAuthMode(APAuthWPA_PSK);
            else
            {
                it = HtmlUtils::LookupParam(params, SecWpa2ModeName,
                                            "WPA2 mode", SecurityPage);
                if (it == params.end())
                    return ERROR_INVALID_PARAMETER;
                if (_wcsicmp(it->second, SecWpa2ModeTrue) == 0)
                    pState->SetAuthMode(APAuthWPA2);
                else
                {
                    it = HtmlUtils::LookupParam(params, SecWpa2PskModeName,
                                                "WPA2-PSK mode", SecurityPage);
                    if (it == params.end())
                        return ERROR_INVALID_PARAMETER;
                    if (_wcsicmp(it->second, SecWpa2PskModeTrue) == 0)
                        pState->SetAuthMode(APAuthWPA2_PSK);
                    else
                    {
                        it = HtmlUtils::LookupParam(params, SecRadiusModeName,
                                                    "802.1x mode", SecurityPage);
                        if (it == params.end())
                            return ERROR_INVALID_PARAMETER;
                        if (_wcsicmp(it->second, SecRadiusModeTrue) == 0)
                            pState->SetAuthMode(APAuthWEP_802_1X);
                        else
                            pState->SetAuthMode(APAuthOpen);
                    }
                }
            }
        }
    }

    // Get the cipher mode.
    switch (pState->GetAuthMode())
    {
        case APAuthWPA:  case APAuthWPA_PSK:
        case APAuthWPA2: case APAuthWPA2_PSK:
            it = HtmlUtils::LookupParam(params, SecWpaCipherName,
                                        "Cipher mode", SecurityPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            if (_wcsicmp(it->second, SecWpaCipherAes) == 0
             || _wcsicmp(it->second, SecWpaCipherTkipAes) == 0)
            {
                pState->SetCipherMode(APCipherAES);
            }
            else
            {
                pState->SetCipherMode(APCipherTKIP);
            }
            break;

        default:
            it = HtmlUtils::LookupParam(params, SecWepCipherName,
                                        "WEP mode", SecurityPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            if (_wcsicmp(it->second, SecWepCipherTrue) == 0)
                 pState->SetCipherMode(APCipherWEP);
            else pState->SetCipherMode(APCipherClearText);
            break;
    }

    // Get the Radius server information.
    it = HtmlUtils::LookupParam(params, SecRadiusServerName,
                                "Radius server", SecurityPage);
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;
    else
    {
        ce::string value;
        hr = WiFUtils::ConvertString(&value, it->second, "radius server",
                                             it->second.length(), CP_UTF8);
        if (FAILED(hr))
            return HRESULT_CODE(hr);

        DWORD server = inet_addr(value);
        if (server == INADDR_NONE)
        {
            LogError(TEXT("Bad radius-server address \"%ls\""),
                    &it->second[0]);
            return ERROR_INVALID_PARAMETER;
        }
        pState->SetRadiusServer(server);
    }

    it = HtmlUtils::LookupParam(params, SecRadiusPortName,
                                "Radius port", SecurityPage);
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;
    else
    {
        long port = wcstol(it->second, NULL, 10);
        if (0 > port || port >= 32*1024)
        {
            LogError(TEXT("Invalid Radius port \"%s\""),
                    &it->second[0]);
            return ERROR_INVALID_PARAMETER;
        }
        pState->SetRadiusPort(port);
    }

    it = HtmlUtils::LookupParam(params, SecRadiusSecretName,
                                "Radius key", SecurityPage);
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;
    else
    {
        hr = pState->SetRadiusSecret(it->second, it->second.length());
        if (FAILED(hr))
            return HRESULT_CODE(hr);
    }

    // Get the WEP keys.
    WEPKeys_t keys = pState->GetWEPKeys();
    for (int kx = 0 ; 4 > kx ; ++kx)
    {
        char name[20];
        hr = StringCchPrintfA(name, sizeof(name), "%s%d", SecWEPKeyName, kx+1);
        if (FAILED(hr))
            return HRESULT_CODE(hr);

        it = HtmlUtils::LookupParam(params, name,
                                    "WEP key", SecurityPage);
        if (it == params.end())
            return ERROR_INVALID_PARAMETER;
        const ce::wstring &value = it->second;

        memset(keys.m_Material[kx], 0, sizeof(keys.m_Material[kx]));
        if (0 == value.length())
        {
            keys.m_Size[kx] = 0;
            continue;
        }

        bool isAscii;
        if (keys.ValidKeySize(value.length()))
        {
            isAscii = true;
            keys.m_Size[kx] = static_cast<BYTE>(value.length());
        }
        else
        {
            isAscii = false;
            keys.m_Size[kx] = static_cast<BYTE>(value.length() / 2);
        }

        if (!keys.ValidKeySize(keys.m_Size[kx]))
        {
            LogError(TEXT("Invalid %hs WEP key \"%hs\" = \"%ls\" in \"%ls\""),
                     isAscii? "ASCII" : "HEX",
                     name, &value[0], SecurityPage);
            return ERROR_INVALID_PARAMETER;
        }

        if (isAscii)
        {
            ce::string mbValue;
            hr = WiFUtils::ConvertString(&mbValue, value, "WEP key",
                                                   value.length(), CP_UTF8);
            if (FAILED(hr))
                return HRESULT_CODE(hr);
            memcpy(keys.m_Material[kx], &mbValue[0], value.length());
        }
        else
        {
            hr = keys.FromString(kx, value);
            if (FAILED(hr))
            {
                LogError(TEXT("Invalid HEX WEP key \"%hs\" = \"%ls\""),
                         name, &value[0]);
                return HRESULT_CODE(hr);
            }
        }
    }

    // Get the WEP key index.
    it = HtmlUtils::LookupParam(params, SecWEPKeyName,
                                "WEP Key Index", SecurityPage);
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;
    else
    {
        long keyIndex = wcstol(it->second, NULL, 10);
        if (0 >= keyIndex || keyIndex > 4)
        {
            LogError(TEXT("Invalid WEP-key index \"%ls\""),
                     &it->second[0]);
            return ERROR_INVALID_PARAMETER;
        }
        keys.m_KeyIndex = static_cast<BYTE>(keyIndex-1);
    }

    pState->SetWEPKeys(keys);

    // Get the passphrase.
    it = HtmlUtils::LookupParam(params, SecPassphraseName,
                                "Passphrase", SecurityPage);
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;
    else
    {
        hr = pState->SetPassphrase(it->second, it->second.length());
        if (FAILED(hr))
            return HRESULT_CODE(hr);
    }

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Updates the WirelessPage configuration settings.
//
static DWORD
UpdateWirelessPage(
    HttpPort_t              *pHttp,
    AccessPointState_t      *pState,
    const AccessPointState_t &NewState)
{
    HRESULT hr;
    DWORD result;
    ValueMap params;
    ValueMapIter it;
    HtmlForms_t forms;
    ce::string webPage;
    ce::wstring wstr;

    int fieldFlags = NewState.GetFieldFlags()
                     & ((int)pState->FieldMaskRadioState
                       |(int)pState->FieldMaskSsid
                       |(int)pState->FieldMaskSsidBroadcast);

    for (int tries = 0 ;; ++tries)
    {
        // Parse the web-page's form fields.
        result = LoadWirelessPage(pHttp, pState, &webPage, &forms);
        if (ERROR_SUCCESS != result)
            return result;

        // Check which elements still need to be updated.
        // If they're all done, break out of the loop.
        fieldFlags = pState->CompareFields(fieldFlags, NewState);
        if (fieldFlags == 0)
            break;
        LogDebug(TEXT("[AC] Updates remaining at pass %d = 0x%X"),
                 tries+1, fieldFlags);
        params = forms.GetFields(0);

        // Radio-state:
        if (fieldFlags & (int)pState->FieldMaskRadioState)
        {
            if (1 <= tries)
            {
                LogError(TEXT("Unable to %hs radio in page \"%ls\""),
                         NewState.IsRadioOn()? "enable" : "disable",
                         WirelessPage);
                return ERROR_INVALID_PARAMETER;
            }
    
            // Update the parameters.
            it = HtmlUtils::LookupParam(params, WiFiRadioStateName,
                                        "Radio status", WirelessPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            params.erase(it);
            params.insert(WiFiRadioStateName,
                          NewState.IsRadioOn()? L"1" : L"0");
        }
    
        // SSID:
        if (fieldFlags & (int)pState->FieldMaskSsid)
        {
            WCHAR ssid[NewState.MaxSsidChars+1];
            hr = NewState.GetSsid(ssid, COUNTOF(ssid));
            if (FAILED(hr))
                return HRESULT_CODE(hr);
    
            if (1 <= tries)
            {
                LogError(TEXT("Unable to update SSID")
                         TEXT(" to \"%ls\" in page \"%ls\""),
                         ssid, WirelessPage);
                return ERROR_INVALID_PARAMETER;
            }
    
            // Update the SSID.
            it = HtmlUtils::LookupParam(params, WiFiSsidName,
                                        "SSID", WirelessPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;

            params.erase(it);
            params.insert(WiFiSsidName, ssid);
        }
    
        // Broadcast-SSID:
        if (fieldFlags & (int)pState->FieldMaskSsidBroadcast)
        {
            if (1 <= tries)
            {
                LogError(TEXT("Unable to %hs broadcast-SSID in page \"%ls\""),
                         NewState.IsSsidBroadcast()? "enable" : "disable",
                         WirelessPage);
                return ERROR_INVALID_PARAMETER;
            }
    
            // Update the parameters.
            it = HtmlUtils::LookupParam(params, WiFiSsidBroadcastName,
                                        "broadcast-SSID", WirelessPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            params.erase(it);
            params.insert(WiFiSsidBroadcastName,
                          NewState.IsSsidBroadcast()? L"0" : L"1");
        }
        
        // Update the configuration.
        result = pHttp->SendUpdateRequest(WirelessPage,
                                          forms.GetMethod(0),
                                          forms.GetAction(0),
                                          params,
                                          WirelessRestart,
                                         &webPage,
                                         "Committing values");
        if (ERROR_SUCCESS != result)
            return result;
    }

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Updates the SecurityPage configuration settings.
//
static DWORD
UpdateSecurityPage(
    HttpPort_t              *pHttp,
    AccessPointState_t      *pState,
    const AccessPointState_t &NewState)
{
    HRESULT hr;
    DWORD result;
    ValueMap params;
    ValueMapIter it;
    HtmlForms_t forms;
    ce::string webPage;
    ce::wstring wstr;

    WCHAR buff[256];
    int   buffChars = COUNTOF(buff);

    int fieldFlags = NewState.GetFieldFlags()
                     & ((int)pState->FieldMaskSecurityMode
                       |(int)pState->FieldMaskRadius
                       |(int)pState->FieldMaskWEPKeys
                       |(int)pState->FieldMaskPassphrase);

    // The security modes must be set properly to change items like the
    // WEP keys. Unfortunately, that means it takes multiple passes to
    // set some of the values; At each pass we set the modes required
    // to modify one or more values, update those, then try to set the
    // modes back the way they belong.
    //
    for (int tries = 0 ;; ++tries)
    {
        // Parse the web-page's form fields.
        result = LoadSecurityPage(pHttp, pState, &webPage, &forms);
        if (ERROR_SUCCESS != result)
            return result;

        // Check which elements still need to be updated.
        // If they're all done, break out of the loop.
        fieldFlags = pState->CompareFields(fieldFlags, NewState);
        if (fieldFlags == 0)
            break;
        LogDebug(TEXT("[AC] Updates remaining at pass %d = 0x%X"),
                 tries+1, fieldFlags);
        params = forms.GetFields(0);

        // Extract and validate the default security-mode settings.        
        APAuthMode_e   authMode;
        APCipherMode_e cipherMode;
        if (fieldFlags & (int)pState->FieldMaskSecurityMode)
        {
            authMode   = NewState.GetAuthMode();
            cipherMode = NewState.GetCipherMode();
            if (fieldFlags & (int)pState->FieldMaskSecurityMode)
            {
                if (!ValidateSecurityModes(authMode, cipherMode,
                                           pState->GetCapabilitiesEnabled()))
                {
                    TCHAR vendorName [pState->MaxVendorNameChars +1];
                    TCHAR modelNumber[pState->MaxModelNumberChars+1];
                    pState->GetVendorName (vendorName,  COUNTOF(vendorName));
                    pState->GetModelNumber(modelNumber, COUNTOF(modelNumber));
                    LogError(TEXT("%s %s APs cannot support")
                             TEXT(" authentication \"%s\" and/or")
                             TEXT(" cipher \"%s\""),
                             vendorName,
                             modelNumber,
                             NewState.GetAuthName(),
                             NewState.GetCipherName());
                    return ERROR_INVALID_PARAMETER;
                }
            }
        }
        else
        {
            authMode   = pState->GetAuthMode();
            cipherMode = pState->GetCipherMode();
        }

        // WEP keys:
        if (fieldFlags & (int)pState->FieldMaskWEPKeys)
        {
            if (3 <= tries)
            {
                LogError(TEXT("Unable to update WEP keys in page \"%ls\""),
                         SecurityPage);
                return ERROR_INVALID_PARAMETER;
            }
    
            // If necessary, change to WEP cipher-mode.
            if ((APAuthOpen != authMode && APAuthShared != authMode)
             || (APCipherWEP != cipherMode))
            {
                if (APAuthShared != authMode)
                    authMode = APAuthOpen;
                cipherMode = APCipherWEP;
                fieldFlags |= (int)pState->FieldMaskSecurityMode;
            }
    
            // Update the WEP keys.
            const WEPKeys_t &keys = NewState.GetWEPKeys();
            for (int kx = 0 ; 4 > kx ; ++kx)
            {
                int keySize = static_cast<int>(keys.m_Size[kx]);
                if (!keys.ValidKeySize(keySize))
                    continue;
    
                char name[32];
                int  nameChars = COUNTOF(name);
    
                static const WCHAR *hexmap = L"0123456789ABCDEF";
                const BYTE *keymat = &keys.m_Material[kx][0];
                const BYTE *keyend = &keys.m_Material[kx][keySize];
                WCHAR *pBuffer = buff;
                for (; keymat < keyend ; ++keymat)
                {
                    *(pBuffer++) = hexmap[(*keymat >> 4) & 0xf];
                    *(pBuffer++) = hexmap[ *keymat       & 0xf];
                }
                *pBuffer = L'\0';
    
                hr = StringCchPrintfA(name, nameChars,
                                      "%s%d", SecWEPKeyName, kx+1);
                if (FAILED(hr))
                    return HRESULT_CODE(hr);
    
                it = HtmlUtils::LookupParam(params, name,
                                            "WEP key", SecurityPage);
                if (it == params.end())
                    return ERROR_INVALID_PARAMETER;
                params.erase(it);
                params.insert(name, buff);
            }
        
            // Update the WEP key index.
            hr = StringCchPrintfW(buff, buffChars, L"%d",
                                  static_cast<int>(keys.m_KeyIndex)+1);
            if (FAILED(hr))
                return HRESULT_CODE(hr);
            it = HtmlUtils::LookupParam(params, SecWEPKeyName,
                                        "WEP Key index", SecurityPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            params.erase(it);
            params.insert(SecWEPKeyName, buff);
        
        }
    
        // Passphrase:
        if (fieldFlags & (int)pState->FieldMaskPassphrase)
        {
            WCHAR passphrase[NewState.MaxPassphraseChars+1];
            hr = NewState.GetPassphrase(passphrase, COUNTOF(passphrase));
            if (FAILED(hr))
                return HRESULT_CODE(hr);
            
            if (2 <= tries)
            {
                LogError(TEXT("Unable to update Passphrase")
                         TEXT(" to \"%ls\" in page \"%ls\""),
                         passphrase, WirelessPage);
                return ERROR_INVALID_PARAMETER;
            }
    
            // If necessary, change to PSK authentication-mode.
            if (authMode != APAuthWPA_PSK && authMode != APAuthWPA2_PSK)
            {
                authMode = APAuthWPA_PSK;
                fieldFlags |= (int)pState->FieldMaskSecurityMode;
            }

            // Update the PSK.
            it = HtmlUtils::LookupParam(params, SecPassphraseName,
                                        "Passphrase", SecurityPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;

            params.erase(it);
            params.insert(SecPassphraseName, passphrase);
        }
    
        // Radius server:
        if (fieldFlags & (int)pState->FieldMaskRadius)
        {
            WCHAR secret[NewState.MaxRadiusSecretChars+1];
            hr = NewState.GetRadiusSecret(secret, COUNTOF(secret));
            if (FAILED(hr))
                return HRESULT_CODE(hr);
            
            in_addr inaddr;
            inaddr.s_addr = NewState.GetRadiusServer();
            hr = StringCchPrintfW(buff, buffChars, 
                                  L"%u.%u.%u.%u",
                                  inaddr.s_net, inaddr.s_host,
                                  inaddr.s_lh, inaddr.s_impno);
            if (FAILED(hr))
                return HRESULT_CODE(hr);

            if (1 <= tries)
            {
                LogError(TEXT("Unable to update Radius info to")
                         TEXT(" \"%ls,%d,%ls\" in page \"%ls\""),
                         buff, NewState.GetRadiusPort(), secret,
                         SecurityPage);
                return ERROR_INVALID_PARAMETER;
            }
    
            // If necessary, enable EAP authentication.
            if (authMode != APAuthWEP_802_1X)
            {
                authMode = APAuthWEP_802_1X;
                fieldFlags |= (int)pState->FieldMaskSecurityMode;
            }
    
            // Update the server.
            it = HtmlUtils::LookupParam(params, SecRadiusServerName,
                                        "Radius server", SecurityPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            params.erase(it);
            params.insert(SecRadiusServerName, buff);
        
            // Update the port.
            hr = StringCchPrintfW(buff, buffChars,
                                  L"%d", NewState.GetRadiusPort());
            if (FAILED(hr))
                return HRESULT_CODE(hr);
            it = HtmlUtils::LookupParam(params, SecRadiusPortName,
                                        "Radius server port", SecurityPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            params.erase(it);
            params.insert(SecRadiusPortName, buff);
        
            // Update the secret key.
            it = HtmlUtils::LookupParam(params, SecRadiusSecretName,
                                        "Radius secret key", SecurityPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;

            params.erase(it);
            params.insert(SecRadiusSecretName, secret);
        }

        // Security modes:
        if (fieldFlags & (int)pState->FieldMaskSecurityMode)
        {
            if (4 <= tries)
            {
                LogError(TEXT("Unable to update security modes")
                         TEXT(" to auth=%s and cipher=%s")
                         TEXT(" in page \"%ls\""),
                         NewState.GetAuthName(),
                         NewState.GetCipherName(),
                         SecurityPage);
                return ERROR_INVALID_PARAMETER;
            }
    
            const WCHAR *pAuthModeValue    = SecAuthModeFalse;
            const WCHAR *pRadiusModeValue  = SecRadiusModeFalse;
            const WCHAR *pWpaModeValue     = SecWpaModeFalse;
            const WCHAR *pWpaPskModeValue  = SecWpaPskModeFalse;
            const WCHAR *pWpa2ModeValue    = SecWpa2ModeFalse;
            const WCHAR *pWpa2PskModeValue = SecWpa2PskModeFalse;
            const WCHAR *pWepCipherValue   = SecWepCipherFalse;
            const WCHAR *pWpaCipherValue   = SecWpaCipherTkip;

            // Set the authentication mode.
            switch (authMode)
            {
                case APAuthShared:
                    pAuthModeValue = SecAuthModeTrue;
                    break;
                case APAuthWEP_802_1X:
                    pRadiusModeValue = SecRadiusModeTrue;
                    break;
                case APAuthWPA:
                    pWpaModeValue = SecWpaModeTrue;
                    break;
                case APAuthWPA_PSK:
                    pWpaPskModeValue = SecWpaPskModeTrue;
                    break;
                case APAuthWPA2:
                    pWpa2ModeValue = SecWpa2ModeTrue;
                    break;
                case APAuthWPA2_PSK:
                    pWpa2PskModeValue = SecWpa2PskModeTrue;
                    break;
            }

            // Set the cipher mode.
            switch (authMode)
            {
                case APAuthWPA:  case APAuthWPA_PSK:
                case APAuthWPA2: case APAuthWPA2_PSK:
                    if (cipherMode == APCipherAES)
                    {
                        pWpaCipherValue = SecWpaCipherAes; 
                    }
                    break;

                default:
                    if (cipherMode == APCipherWEP)
                    {
                        pWepCipherValue = SecWepCipherTrue;
                    }
                    break;
            }

            // Set the web-page's form parameters.
            it = HtmlUtils::LookupParam(params, SecAuthModeName,
                                        "Authentication mode", SecurityPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            params.erase(it);
            params.insert(SecAuthModeName, pAuthModeValue);
        
            it = HtmlUtils::LookupParam(params, SecRadiusModeName,
                                        "802.1x mode", SecurityPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            params.erase(it);
            params.insert(SecRadiusModeName, pRadiusModeValue);
        
            it = HtmlUtils::LookupParam(params, SecWpaModeName,
                                        "WPA mode", SecurityPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            params.erase(it);
            params.insert(SecWpaModeName, pWpaModeValue);
        
            it = HtmlUtils::LookupParam(params, SecWpaPskModeName,
                                        "WPA-PSK mode", SecurityPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            params.erase(it);
            params.insert(SecWpaPskModeName, pWpaPskModeValue);
        
            it = HtmlUtils::LookupParam(params, SecWpa2ModeName,
                                        "WPA2 mode", SecurityPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            params.erase(it);
            params.insert(SecWpa2ModeName, pWpa2ModeValue);
        
            it = HtmlUtils::LookupParam(params, SecWpa2PskModeName,
                                        "WPA2-PSK mode", SecurityPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            params.erase(it);
            params.insert(SecWpa2PskModeName, pWpa2PskModeValue);
        
            it = HtmlUtils::LookupParam(params, SecWepCipherName,
                                        "WEP mode", SecurityPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            params.erase(it);
            params.insert(SecWepCipherName, pWepCipherValue);
        
            it = HtmlUtils::LookupParam(params, SecWpaCipherName,
                                        "Cipher mode", SecurityPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            params.erase(it);
            params.insert(SecWpaCipherName, pWpaCipherValue);
        }
    
        // Update the configuration.
        result = pHttp->SendUpdateRequest(SecurityPage,
                                          forms.GetMethod(0),
                                          forms.GetAction(0),
                                          params,
                                          SecurityRestart,
                                         &webPage,
                                          "Committing values");
        if (ERROR_SUCCESS != result)
            return result;
    }

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Initializes the device-controller.
//
DWORD
BuffaloBroadcomController_t::
InitializeDevice(void)
{
    HRESULT hr;
    DWORD result;
    ce::string webPage;
    HtmlForms_t forms;
    ce::string  mbValue;
    ce::wstring wcValue;
    const char *pValue,
               *pValueEnd;

    // Load and parse the wireless-lan page.
    result = LoadWirelessPage(m_pHttp, m_pState, &webPage, &forms);
    if (ERROR_SUCCESS != result)
        return result;

    // Find the BSSID (MAC Address), convert it to Unicode and load it.
    HtmlUtils::FindString(strstr(webPage.get_buffer(), "<body"),
                          "\"wl_unit\"", "selected", "</option>", &mbValue);
    pValue = strrchr(mbValue, '(');
    if (pValue)
    {
        pValue++;
        pValueEnd = strchr(pValue, ')');
    }
    else
    {
        pValue    = &mbValue[0];
        pValueEnd = &mbValue[mbValue.length()];
    }

    if (NULL == pValueEnd || pValueEnd <= pValue)
    {
        LogError(TEXT("MAC Address missing from \"%ls\""),
                 WirelessPage);
        return ERROR_INVALID_PARAMETER;
    }

    hr = WiFUtils::ConvertString(&wcValue, pValue, "BSSID",
                                 pValueEnd - pValue, CP_UTF8);
    if (FAILED(hr))
        return HRESULT_CODE(hr);

    MACAddr_t bssid;
    hr = bssid.FromString(wcValue);
    if (FAILED(hr))
    {
        LogError(TEXT("Bad BSSID value \"%ls\" in %ls"),
                 &wcValue[0], WirelessPage);
        return ERROR_INVALID_PARAMETER;
    }
    m_pState->SetBSsid(bssid);

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Gets the configuration settings from the device.
//
DWORD
BuffaloBroadcomController_t::
LoadSettings(void)
{
    DWORD result;
    ce::string webPage;
    HtmlForms_t forms;

    // Load and parse the wireless-lan page.
    result = LoadWirelessPage(m_pHttp, m_pState, &webPage, &forms);
    if (ERROR_SUCCESS != result)
        return result;

    // Load and parse the wireless-lan security page.
    result = LoadSecurityPage(m_pHttp, m_pState, &webPage, &forms);
    if (ERROR_SUCCESS != result)
        return result;

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Updates the device's configuration settings.
//
DWORD
BuffaloBroadcomController_t::
UpdateSettings(
    const AccessPointState_t &NewState)
{
    DWORD result;

    // Update the wireless-lan page.
    result = UpdateWirelessPage(m_pHttp, m_pState, NewState);
    if (ERROR_SUCCESS != result)
        return result;

    // Update the wireless-lan security page.
    result = UpdateSecurityPage(m_pHttp, m_pState, NewState);
    if (ERROR_SUCCESS != result)
        return result;

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
