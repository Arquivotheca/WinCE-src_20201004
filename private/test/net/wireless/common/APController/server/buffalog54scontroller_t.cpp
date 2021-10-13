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
// Implementation of the BuffaloG54sController_t class.
//
// ----------------------------------------------------------------------------

#include "BuffaloG54sController_t.hpp"

#include <assert.h>
#include <strsafe.h>

#include "AccessPointState_t.hpp"
#include "HttpPort_t.hpp"

// Define this if you want LOTS of debug output:
#define EXTRA_DEBUG 1

using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Web-page names:
//
static const WCHAR AdminPage[]    = L"/advance/ad-admin-system.htm";
static const WCHAR WirelessPage[] = L"/advance/ad-lan-wireless_g.htm";
static const WCHAR SecurityPage[] = L"/advance/ad-lan-wireless_sec_g.htm";

// ----------------------------------------------------------------------------
//
// Destructor.
//
BuffaloG54sController_t::
~BuffaloG54sController_t(void)
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

    WCHAR buff[64];
    int   buffChars = COUNTOF(buff);

    // Fill in the model's capabilities.
    int caps = (int)(APCapsWPA | APCapsWPA_PSK | APCapsWPA_AES);
    pState->SetCapabilitiesImplemented(caps);
    pState->SetCapabilitiesEnabled(caps);

    // Load and parse the wireless-lan page.
    result = HtmlAPTypeController_t::ParsePageRequest(pHttp, WirelessPage, 0L,
                                                      pWebPage, pPageForms, 1);
    if (ERROR_SUCCESS != result)
        return result;
    const ValueMap &params = pPageForms->GetFields(0);

    // Get the radio state.
    it = HtmlUtils::LookupParam(params, "wl_radio", "Radio status",
                                WirelessPage);
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;
    pState->SetRadioState(wcstol(it->second, NULL, 10) != 0);

    // Get the SSid.
    it = HtmlUtils::LookupParam(params, "wl_ssid_mac", "SSID",
                                WirelessPage);
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;
    if (it->second[0] == L'1')
    {
        // use the BSSID...
        pState->GetBSsid().ToString(buff, buffChars);
    }
    else
    {
        // use the form field...
        it = HtmlUtils::LookupParam(params, "wl_ssid_input", "SSID",
                                    WirelessPage);
        if (it == params.end())
            return ERROR_INVALID_PARAMETER;
        WiFUtils::ConvertString(buff, it->second, buffChars);
    }
    hr = pState->SetSsid(buff);
    if (FAILED(hr))
    {
        LogError(TEXT("Bad SSID value \"%ls\" in %ls"),
                 buff, WirelessPage);
        return HRESULT_CODE(hr);
    }

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
    result = HtmlAPTypeController_t::ParsePageRequest(pHttp, SecurityPage, 0L,
                                                      pWebPage, pPageForms, 1);
    if (ERROR_SUCCESS != result)
        return result;
    const ValueMap &params = pPageForms->GetFields(0);

    // Get the cipher-type.
    it = HtmlUtils::LookupParam(params, "wl_wep",
                                "Cipher mode",
                                SecurityPage);
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;
    else
    {
        if (0 == _wcsnicmp(it->second, L"off", 3))
        {
            pState->SetAuthMode(APAuthOpen);
            pState->SetCipherMode(APCipherClearText);
        }
        else
        if (0 == _wcsnicmp(it->second, L"rest", 4))
        {
            pState->SetAuthMode(APAuthShared);
            pState->SetCipherMode(APCipherWEP);
        }
        else
        if (0 == _wcsnicmp(it->second, L"tkip", 4))
        {
            pState->SetAuthMode(APAuthWPA_PSK);
            pState->SetCipherMode(APCipherTKIP);
        }
        else
        if (0 == _wcsnicmp(it->second, L"aes", 3))
        {
            pState->SetAuthMode(APAuthWPA_PSK);
            pState->SetCipherMode(APCipherAES);
        }
        else
        {
            LogError(TEXT("Cipher-mode element \"%hs\"")
                     TEXT(" value \"%ls\" unrecognized"),
                    &it->first[0], &it->second[0]);
            return ERROR_INVALID_PARAMETER;
        }
    }

    // Get the EAP-authentication flag.
    it = HtmlUtils::LookupParam(params, "wl_auth_select", "EAP flag",
                                SecurityPage);
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;
    else
    if (0 == _wcsnicmp(it->second, L"on", 2))
    {
        switch (pState->GetAuthMode())
        {
            case APAuthShared:  pState->SetAuthMode(APAuthWEP_802_1X); break;
            case APAuthWPA_PSK: pState->SetAuthMode(APAuthWPA);        break;
        }
    }

    // Get the Radius server information.
    it = HtmlUtils::LookupParam(params, "wl_radius_ipaddr",
                                "Radius server",
                                SecurityPage);
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

    it = HtmlUtils::LookupParam(params, "wl_radius_port",
                                "Radius port",
                                SecurityPage);
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;
    else
    {
        long port = wcstol(it->second, NULL, 10);
        if (0 > port || port >= 32*1024)
        {
            LogError(TEXT("Invalid Radius port \"%ls\""),
                    &it->second[0]);
            return ERROR_INVALID_PARAMETER;
        }
        pState->SetRadiusPort(port);
    }

    it = HtmlUtils::LookupParam(params, "wl_radius_key", 
                                "Radius key",
                                SecurityPage);
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;
    else
    {
        hr = pState->SetRadiusSecret(it->second, it->second.length());
        if (FAILED(hr))
        {
            LogError(TEXT("Invalid Radius secret \"%ls\""),
                    &it->second[0]);
            return HRESULT_CODE(hr);
        }
    }

    // Get the selected WEP key.
    WEPKeys_t keys = pState->GetWEPKeys();
    it = HtmlUtils::LookupParam(params, "wl_key", 
                                "selected WEP key",
                                SecurityPage);
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;
    int wepKeyIndex = 0;
    switch (it->second[0])
    {
        case L'2': wepKeyIndex = 1; break;
        case L'3': wepKeyIndex = 2; break;
        case L'4': wepKeyIndex = 3; break;
    }

    // Get the WEP keys.
    for (int kx = 0 ; 4 > kx ; ++kx)
    {
        char key[20];
        hr = StringCchPrintfA(key, sizeof(key), "wep_ascii_select%d", kx+1);
        if (FAILED(hr))
            return HRESULT_CODE(hr);

        it = HtmlUtils::LookupParam(params, key,
                                    "WEP key ASCII flag",
                                    SecurityPage);
        if (it == params.end())
            return ERROR_INVALID_PARAMETER;
        bool isAscii = wcstol(it->second, NULL, 10) != 0;

        hr = StringCchPrintfA(key, sizeof(key), "wl_key%d", kx+1);
        if (FAILED(hr))
            return HRESULT_CODE(hr);

        it = HtmlUtils::LookupParam(params, key, 
                                    "WEP key",
                                    SecurityPage);
        if (it == params.end())
            return ERROR_INVALID_PARAMETER;

        const ce::wstring &value = it->second;
        memset(keys.m_Material[kx], 0, sizeof(keys.m_Material[kx]));
        if (0 == value.length())
        {
            keys.m_Size[kx] = 0;
            continue;
        }

        // Can not overflow, as the string value is fro 0~9
        keys.m_Size[kx] = (BYTE)(isAscii? value.length() : value.length() / 2);
        if (!keys.ValidKeySize(keys.m_Size[kx]))
        {
            LogError(TEXT("Invalid %hs WEP key \"%hs\" = \"%ls\" in \"%ls\""),
                     isAscii? "ASCII" : "HEX",
                     key, &value[0], SecurityPage);
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
                         key, &value[0]);
                return HRESULT_CODE(hr);
            }
        }
    }
    keys.m_KeyIndex = (BYTE)wepKeyIndex;
    pState->SetWEPKeys(keys);

    // Get the passphrase.
    it = HtmlUtils::LookupParam(params, "wl_wpa_psk", 
                                "Passphrase",
                                SecurityPage);
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

    int fieldFlags = NewState.GetFieldFlags()
                     & ((int)pState->FieldMaskRadioState
                       |(int)pState->FieldMaskSsid);

    // The radio must be on before we can change the SSID wo we always
    // turn it on. Later, if the radio is supposed to be off, we change
    // it back.
    //
    // We do this by running through the process multiple times. The
    // first time we change the radio and update the SSID. The second
    // time we turn the radio back. Note that both passes are only done
    // if required, so usually this will only take one pass.
    //
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
            if (2 <= tries)
            {
                LogError(TEXT("Unable to %hs the radio in page \"%ls\""),
                         NewState.IsRadioOn()? "enable" : "disable",
                         WirelessPage);
                return ERROR_INVALID_PARAMETER;
            }
    
            // Update the parameters.
            it = HtmlUtils::LookupParam(params, "wl_radio", 
                                        "Radio status",
                                        WirelessPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            params.erase(it);
            params.insert("wl_radio", NewState.IsRadioOn()? L"1" : L"0");
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
    
            // Turn on the radio.
            it = HtmlUtils::LookupParam(params, "wl_radio", 
                                        "Radio status",
                                        WirelessPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            params.erase(it);
            params.insert("wl_radio", L"1");

            // Make sure the radio state gets put back the right way.
            fieldFlags |= (int)pState->FieldMaskRadioState;
    
            // Set the MAC/Custom SSID selector to Custom.
            it = HtmlUtils::LookupParam(params, "wl_ssid_mac",
                                        "MAC/custom SSID flag",
                                        WirelessPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            params.erase(it);
            params.insert("wl_ssid_mac", L"0");
    
            // Update the SSID.
            it = HtmlUtils::LookupParam(params, "wl_ssid_input", 
                                        "SSID",
                                        WirelessPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;

            params.erase(it);
            params.insert("wl_ssid_input", ssid);
        }
    
        // Update the configuration.
        result = pHttp->SendUpdateRequest(WirelessPage,
                                          forms.GetMethod(0),
                                          forms.GetAction(0),
                                          params,
                                          1000L,
                                         &webPage,
                                         "Applying new settings");
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

    WCHAR buff[256];
    int   buffChars = COUNTOF(buff);

    int fieldFlags = NewState.GetFieldFlags()
                     & ((int)pState->FieldMaskSecurityMode
                       |(int)pState->FieldMaskRadius
                       |(int)pState->FieldMaskWEPKeys
                       |(int)pState->FieldMaskPassphrase);

    // Get the requested EAP mode.
    const WCHAR *pRequestedEapModeValue = L"off";
    switch (NewState.GetAuthMode())
    {
        case APAuthWEP_802_1X:
        case APAuthWPA:
        case APAuthWPA2:
            pRequestedEapModeValue = L"on";
            break;
    }

    // The cipher mode must be set to WEP before we can change the WEP
    // keys. Conversely, it must be in TKIP or AES to change the
    // passphrase. Finally, the EAP authentication must be enabled
    // before we can set the Radius information.
    //
    // We handle all this this by running through the process multiple
    // times. Each time through we change as much as we can with one
    // set of cipher or authentcation settings. Eventually all the
    // settings will be correct.
    //
    for (int tries = 0 ;; ++tries)
    {
        // Parse the web-page's form fields.
        result = LoadSecurityPage(pHttp, pState, &webPage, &forms);
        if (ERROR_SUCCESS != result)
            return result;
        const ValueMap &constParams = forms.GetFields(0);

        // Check which elements still need to be updated.
        fieldFlags = pState->CompareFields(fieldFlags, NewState);

        // These APs can't do Open/WEP, so if they ask for that mode we
        // convert the request to Shared/WEP. Unfortunately, the requested
        // and retrieved modes won't match in that case. We get around this
        // by equating them here.
        APAuthMode_e   requestedAuthMode   = NewState.GetAuthMode();
        APCipherMode_e requestedCipherMode = NewState.GetCipherMode();
        if (fieldFlags & (int)pState->FieldMaskSecurityMode
         && APAuthOpen  == requestedAuthMode
         && APCipherWEP == requestedCipherMode)
        {
            if (APAuthShared == pState->GetAuthMode()
             && APCipherWEP  == pState->GetCipherMode())
            {
                fieldFlags &= ~(int)pState->FieldMaskSecurityMode;
            }
        }

        // If they're all done, break out of the loop.
        if (fieldFlags == 0)
            break;
        LogDebug(TEXT("[AC] Updates remaining at pass %d = 0x%X"),
                 tries+1, fieldFlags);
        params = constParams;
    
        // EAP-authentication mode:
        it = HtmlUtils::LookupParam(params, "wl_auth_select", 
                                    "EAP flag",
                                    SecurityPage);
        if (it == params.end())
            return ERROR_INVALID_PARAMETER;
        if (0 != _wcsicmp(it->second, pRequestedEapModeValue))
        {
            if (4 <= tries)
            {
                LogError(TEXT("Unable to set EAP")
                         TEXT(" authentication in page \"%ls\""),
                         SecurityPage);
                return ERROR_INVALID_PARAMETER;
            }
    
            params.erase(it);
            params.insert("wl_auth_select", pRequestedEapModeValue);
        }
    
        // Cipher mode:
        if (fieldFlags & (int)pState->FieldMaskSecurityMode)
        {
            if (4 <= tries)
            {
                LogError(TEXT("Unable to update security")
                         TEXT(" modes to auth=%s and cipher=%s")
                         TEXT(" in page \"%ls\""),
                         NewState.GetAuthName(),
                         NewState.GetCipherName(),
                         SecurityPage);
                return ERROR_INVALID_PARAMETER;
            }
    
            // Update the cipher-mode.
            it = HtmlUtils::LookupParam(params, "wl_wep", 
                                        "Cipher mode",
                                        SecurityPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;

            const WCHAR *newMode;
            switch (NewState.GetCipherMode())
            {
                case APCipherWEP:  newMode = L"restricted"; break;
                case APCipherTKIP: newMode = L"tkip"; break;
                case APCipherAES:  newMode = L"aes"; break;
                default:           newMode = L"off"; break;
            }
            params.erase(it);
            params.insert("wl_wep", newMode);
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
    
            // Change to WEP cipher-mode.
            it = HtmlUtils::LookupParam(params, "wl_wep",
                                        "Cipher mode",
                                        SecurityPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;

            if (_wcsicmp(it->second, L"restricted") != 0)
            {
                params.erase(it);
                params.insert("wl_wep", L"restricted");
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
    
                hr = StringCchPrintfA(name, nameChars,
                                     "wep_ascii_select%d", kx+1);
                if (FAILED(hr))
                    return HRESULT_CODE(hr);
    
                it = HtmlUtils::LookupParam(params, name,
                                            "WEP key ASCII/HEX flag",
                                            SecurityPage);
                if (it == params.end())
                    return ERROR_INVALID_PARAMETER;
                params.erase(it);
                params.insert(name, L"0");
    
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
    
                hr = StringCchPrintfA(name, nameChars, "wl_key%d", kx+1);
                if (FAILED(hr))
                    return HRESULT_CODE(hr);
    
                it = HtmlUtils::LookupParam(params, name,
                                            "WEP key",
                                            SecurityPage);
                if (it == params.end())
                    return ERROR_INVALID_PARAMETER;
                params.erase(it);
                params.insert(name, buff);
            }
    
            // Update the WEP key index.
            it = HtmlUtils::LookupParam(params, "wl_key",
                                        "selected WEP key",
                                        SecurityPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;

            buff[0] = static_cast<WCHAR>(keys.m_KeyIndex + L'1');
            buff[1] = L'\0';
            params.erase(it);
            params.insert("wl_key", buff);
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
                         TEXT(" to \"%s\" in page \"%ls\""),
                         passphrase, WirelessPage);
                return ERROR_INVALID_PARAMETER;
            }
    
            // If necessary, change to TKIP or AES cipher-mode.
            it = HtmlUtils::LookupParam(params, "wl_wep", 
                                        "Cipher mode",
                                        SecurityPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            if (_wcsicmp(it->second, L"tkip") != 0
             && _wcsicmp(it->second, L"aes")  != 0)
            {
                params.erase(it);
                params.insert("wl_wep", L"tkip");
                fieldFlags |= (int)pState->FieldMaskSecurityMode;
            }
    
            // If necessary, disable EAP authentication.
            it = HtmlUtils::LookupParam(params, "wl_auth_select", 
                                        "EAP flag",
                                        SecurityPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            if (0 != _wcsicmp(it->second, L"off"))
            {
                params.erase(it);
                params.insert("wl_auth_select", L"off");
            }
    
            // Update the PSK.
            it = HtmlUtils::LookupParam(params, "wl_wpa_psk", 
                                        "Passphrase",
                                        SecurityPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;

            params.erase(it);
            params.insert("wl_wpa_psk", passphrase);
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
                LogError(TEXT("Unable to update Radius info")
                         TEXT(" to \"%ls,%d,%s\" in page \"%ls\""),
                         buff, NewState.GetRadiusPort(), secret,
                         SecurityPage);
                return ERROR_INVALID_PARAMETER;
            }
    
            // If necessary, enable EAP authentication.
            it = HtmlUtils::LookupParam(params, "wl_auth_select", 
                                        "EAP flag",
                                        SecurityPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            if (0 != _wcsicmp(it->second, L"on"))
            {
                params.erase(it);
                params.insert("wl_auth_select", L"on");
            }
    
            // Update the server.
            it = HtmlUtils::LookupParam(params, "wl_radius_ipaddr",
                                        "Radius server",
                                        SecurityPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            params.erase(it);
            params.insert("wl_radius_ipaddr", buff);
        
            // Update the port.
            hr = StringCchPrintfW(buff, buffChars,
                                  L"%d", NewState.GetRadiusPort());
            if (FAILED(hr))
                return HRESULT_CODE(hr);

            it = HtmlUtils::LookupParam(params, "wl_radius_port",
                                        "Radius server port",
                                        SecurityPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            params.erase(it);
            params.insert("wl_radius_port", buff);
        
            // Update the secret key.
            it = HtmlUtils::LookupParam(params, "wl_radius_key",
                                        "Radius secret key",
                                        SecurityPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;

            params.erase(it);
            params.insert("wl_radius_key", secret);
        }
    
        // Update the configuration.
        result = pHttp->SendUpdateRequest(SecurityPage,
                                          forms.GetMethod(0),
                                          forms.GetAction(0),
                                          params,
                                          1000L,
                                         &webPage,
                                         "Applying new settings");
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
BuffaloG54sController_t::
InitializeDevice(void)
{
    HRESULT hr;
    DWORD result;
    ce::string webPage;
    ce::string  mbValue;
    ce::wstring wcValue;
    const char *pCursor,
             *oldCursor;

    // Load the system-information page.
    result = m_pHttp->SendPageRequest(AdminPage, 0L, &webPage);
    if (ERROR_SUCCESS != result)
        return result;
    pCursor = strstr(webPage.get_buffer(), "<body");

    // Vendor name and model number:
    m_pState->SetVendorName(TEXT("Buffalo"));
    oldCursor = pCursor;
    pCursor = HtmlUtils::FindString(oldCursor, "<strong>Model</strong>",
                                    "<strong>", "</strong>", &mbValue);
    if (oldCursor == pCursor)
    {
        LogError(TEXT("Model number missing from \"%ls\""),
                 AdminPage);
        return ERROR_INVALID_PARAMETER;
    }

    hr = WiFUtils::ConvertString(&wcValue, mbValue, "model number",
                                           mbValue.length(), CP_UTF8);
    if (FAILED(hr))
        return HRESULT_CODE(hr);

    hr = m_pState->SetModelNumber(wcValue, wcValue.length());
    if (FAILED(hr))
    {
        LogError(TEXT("Bad Model NUmber value \"%ls\" in %ls"),
                 &wcValue[0], AdminPage);
        return HRESULT_CODE(hr);
    }

    // BSSID (MAC Address):
    oldCursor = pCursor;
    pCursor = HtmlUtils::FindString(oldCursor,
                                    "<b>MAC Address</b>",   // skip first
                                    "<b>", "</b>", &mbValue);
    pCursor = HtmlUtils::FindString(pCursor,
                                    "<b>MAC Address</b>",
                                    "<b>", "</b>", &mbValue);
    if (oldCursor == pCursor)
    {
        LogError(TEXT("MAC Address missing from \"%ls\""),
                 AdminPage);
        return ERROR_INVALID_PARAMETER;
    }

    hr = WiFUtils::ConvertString(&wcValue, mbValue, "BSSID",
                                           mbValue.length(), CP_UTF8);
    if (FAILED(hr))
        return HRESULT_CODE(hr);

    MACAddr_t bssid;
    hr = bssid.FromString(wcValue);
    if (FAILED(hr))
    {
        LogError(TEXT("Bad BSSID value \"%ls\" in %ls"),
                 &wcValue[0], AdminPage);
        return HRESULT_CODE(hr);
    }
    m_pState->SetBSsid(bssid);

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Gets the configuration settings from the device.
//
DWORD
BuffaloG54sController_t::
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
BuffaloG54sController_t::
UpdateSettings(const AccessPointState_t &NewState)
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
