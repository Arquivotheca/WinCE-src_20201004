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
// Implementation of the DLinkDI524Controller_t class.
//
// ----------------------------------------------------------------------------

#include "DLinkDI524Controller_t.hpp"

#include <assert.h>
#include <strsafe.h>

#include "AccessPointState_t.hpp"
#include "HttpPort_t.hpp"

// Define this if you want LOTS of debug output:
#define EXTRA_DEBUG 1

using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Device-information page:
//
static const WCHAR DeviceInfoPage[] = L"/st_device.html";

// ----------------------------------------------------------------------------
//
// Wireless page field names and values:
//
static const WCHAR WirelessPage[] = L"/h_wireless.html";
static long        WirelessRestart = 4000L; // update-restart time

// Radio state:
static const char WiFiRadioStateName[] = "enable";

// SSid:
static const char WiFiSsidName[] = "ssid";

// Security mode:
static const char  WiFiSecurityModeName  [] = "wep_type";
static const WCHAR WiFiSecurityModeNone  [] = L"0";
static const WCHAR WiFiSecurityModeWep   [] = L"1";
static const WCHAR WiFiSecurityModeWpa   [] = L"2";
static const WCHAR WiFiSecurityModeWpaPsk[] = L"3";

// WEP authentication mode:
static const char  WiFiWepAuthModeName  [] = "auth_type";
static const WCHAR WiFiWepAuthModeOpen  [] = L"0";
static const WCHAR WiFiWepAuthModeShared[] = L"1";

// WEP keys:
static const char  WiFiWepKeyName        [] = "key";
static const char  WiFiWepKeyLengthName  [] = "wep_key_len";
static const WCHAR WiFiWepKeyLength40Bit [] = L"5";
static const WCHAR WiFiWepKeyLength104Bit[] = L"13";
static const char  WiFiWepKeyTypeName    [] = "wep_key_type";
static const WCHAR WiFiWepKeyTypeHex     [] = L"0";
static const WCHAR WiFiWepKeyTypeAscii   [] = L"1";

// Radius information:
static const char WiFiRadiusServerName[] = "radius_ip1";
static const char WiFiRadiusPortName  [] = "radius_port1";
static const char WiFiRadiusSecretName[] = "radius_pass1";

// Passphrase:
static const char WiFiPassphraseName1[] = "wpapsk1";
static const char WiFiPassphraseName2[] = "wpapsk2"; // confirmation value

// Apply flag:
static const char  WiFiApplyFlagName [] = "apply";
static const WCHAR WiFiApplyFlagTrue [] = L"1";
static const WCHAR WiFiApplyFlagFalse[] = L"0";

// ----------------------------------------------------------------------------
//
// Destructor.
//
DLinkDI524Controller_t::
~DLinkDI524Controller_t(void)
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
    pState->SetVendorName(TEXT("D-Link"));
    pState->SetModelNumber(TEXT("DI-524"));
    int caps = (int)(APCapsWPA | APCapsWPA_PSK);
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
                                "Radio status",
                                WirelessPage);
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;
    pState->SetRadioState(wcstol(it->second, NULL, 10) != 0);

    // Get the SSid.
    it = HtmlUtils::LookupParam(params, WiFiSsidName,
                                "SSID",
                                WirelessPage);
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;
    hr = pState->SetSsid(it->second, it->second.length());
    if (FAILED(hr))
    {
        LogError(TEXT("Bad SSID value \"%ls\" in %ls"),
                 &(it->second)[0], WirelessPage);
        return HRESULT_CODE(hr);
    }

    // Get the security mode.
    it = HtmlUtils::LookupParam(params, WiFiSecurityModeName,
                                "Security mode",
                                WirelessPage);
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;
    if (wcscmp(it->second, WiFiSecurityModeWep) == 0)
    {
        pState->SetCipherMode(APCipherWEP);
        it = HtmlUtils::LookupParam(params, WiFiWepAuthModeName,
                                    "WEP authentication",
                                    WirelessPage);
        if (it == params.end())
            return ERROR_INVALID_PARAMETER;
        if (wcscmp(it->second, WiFiWepAuthModeShared) == 0)
             pState->SetAuthMode(APAuthShared);
        else pState->SetAuthMode(APAuthOpen);
    }
    else
    if (wcscmp(it->second, WiFiSecurityModeWpa) == 0)
    {
        pState->SetCipherMode(APCipherTKIP);
        pState->SetAuthMode(APAuthWPA);
    }
    else
    if (wcscmp(it->second, WiFiSecurityModeWpaPsk) == 0)
    {
        pState->SetCipherMode(APCipherTKIP);
        pState->SetAuthMode(APAuthWPA_PSK);
    }
    else
    {
        pState->SetCipherMode(APCipherClearText);
        pState->SetAuthMode(APAuthOpen);
    }

    // Get the Radius server information.
    it = HtmlUtils::LookupParam(params, WiFiRadiusServerName,
                                "Radius server",
                                WirelessPage);
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

    it = HtmlUtils::LookupParam(params, WiFiRadiusPortName,
                                "Radius port",
                                WirelessPage);
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

    it = HtmlUtils::LookupParam(params, WiFiRadiusSecretName,
                                "Radius key",
                                WirelessPage);
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;
    else
    {
        hr = pState->SetRadiusSecret(it->second, it->second.length());
        if (FAILED(hr))
        {
            LogError(TEXT("Bad radius secret key \"%ls\""),
                     &it->second[0]);
            return HRESULT_CODE(hr);
        }
    }

    // Get the WEP keys.
    it = HtmlUtils::LookupParam(params, WiFiWepKeyTypeName,
                                "WEP key type",
                                WirelessPage);
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;
    bool wepKeyIsAscii = (_wcsicmp(it->second, WiFiWepKeyTypeAscii) == 0);

    WEPKeys_t keys = pState->GetWEPKeys();
    for (int kx = 0 ; 4 > kx ; ++kx)
    {
        char key[20];
        hr = StringCchPrintfA(key, sizeof(key), "%s%d", WiFiWepKeyName, kx+1);
        if (FAILED(hr))
            return HRESULT_CODE(hr);

        it = HtmlUtils::LookupParam(params, key,
                                    "WEP key",
                                    WirelessPage);
        if (it == params.end())
            return ERROR_INVALID_PARAMETER;
        const ce::wstring &value = it->second;

        memset(keys.m_Material[kx], 0, sizeof(keys.m_Material[kx]));
        if (0 == value.length())
        {
            keys.m_Size[kx] = 0;
            continue;
        }

        if (wepKeyIsAscii)
             keys.m_Size[kx] = static_cast<BYTE>(value.length());
        else keys.m_Size[kx] = static_cast<BYTE>(value.length() / 2);

        if (!keys.ValidKeySize(keys.m_Size[kx]))
        {
            LogError(TEXT("Invalid %hs WEP key \"%hs\" = \"%s\" in \"%ls\""),
                     wepKeyIsAscii? "ASCII" : "HEX",
                     key, &value[0], WirelessPage);
            return ERROR_INVALID_PARAMETER;
        }

        if (wepKeyIsAscii)
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
    pState->SetWEPKeys(keys);

    // Get the passphrase.
    it = HtmlUtils::LookupParam(params, WiFiPassphraseName1,
                                "Passphrase",
                                WirelessPage);
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;
    else
    {
        hr = pState->SetPassphrase(it->second, it->second.length());
        if (FAILED(hr))
        {
            LogError(TEXT("Bad passphrase \"%ls\" in page %ls"),
                     &it->second[0], WirelessPage);
            return HRESULT_CODE(hr);
        }
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

    WCHAR buff[256];
    int   buffSize = COUNTOF(buff);

    int fieldFlags = NewState.GetFieldFlags()
                     & ((int)pState->FieldMaskRadioState
                       |(int)pState->FieldMaskSsid
                       |(int)pState->FieldMaskSecurityMode
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
                    LogError(TEXT("%ls %ls APs cannot support")
                             TEXT(" authentication \"%s\"")
                             TEXT(" and/or cipher \"%s\""),
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

        // Radio-state:
        // (The radio must be enabled for any other changes to be
        // accepted.)
        bool enableRadio = NewState.IsRadioOn();
        if (!enableRadio && fieldFlags != (int)pState->FieldMaskRadioState)
        {
            enableRadio = true;
            fieldFlags |= (int)pState->FieldMaskRadioState;
        }
        if (fieldFlags & (int)pState->FieldMaskRadioState)
        {
            if (5 <= tries)
            {
                LogError(TEXT("Unable to %hs the radio in page \"%ls\""),
                         enableRadio? "enable" : "disable",
                         WirelessPage);
                return ERROR_INVALID_PARAMETER;
            }
    
            // Update the parameters.
            it = HtmlUtils::LookupParam(params, WiFiRadioStateName,
                                        "Radio status",
                                        WirelessPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            params.erase(it);
            params.insert(WiFiRadioStateName, enableRadio? L"1" : L"0");
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
                                        "SSID",
                                        WirelessPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            params.erase(it);
            params.insert(WiFiSsidName, ssid);
        }

        // WEP keys:
        if (fieldFlags & (int)pState->FieldMaskWEPKeys)
        {
            const WEPKeys_t &keys = NewState.GetWEPKeys();

            if (3 <= tries)
            {
                LogError(TEXT("Unable to update WEP keys in page \"%ls\""),
                         WirelessPage);
                return ERROR_INVALID_PARAMETER;
            }
    
            // Change to WEP cipher-mode.
            cipherMode = APCipherWEP;
            fieldFlags |= (int)pState->FieldMaskSecurityMode;

            // Determine the size of the new WEP key(s).
            int wepKeySize = -1;
            for (int kx = 0 ; 4 > kx ; ++kx)
            {
                int keySize = static_cast<int>(keys.m_Size[kx]);
                if (keys.ValidKeySize(keySize))
                {
                    wepKeySize = keySize;
                    break;
                }
            }
            if (5 != wepKeySize && 13 != wepKeySize)
            {
                LogError(TEXT("Can't update %d-bit")
                         TEXT(" WEP keys - only 40 and 104"),
                         wepKeySize * 8);
                return ERROR_INVALID_PARAMETER;
            }

            // If necessary, change the key-size and -encoding.
            bool wepKeySpecsChanged = false;
            it = HtmlUtils::LookupParam(params, WiFiWepKeyTypeName,
                                        "WEP key type",
                                        WirelessPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            if (0 == _wcsicmp(it->second, WiFiWepKeyTypeAscii))
            {
                params.erase(it);
                params.insert(WiFiWepKeyTypeName, WiFiWepKeyTypeHex);
                wepKeySpecsChanged = true;
            }
    
            it = HtmlUtils::LookupParam(params, WiFiWepKeyLengthName,
                                        "WEP key length",
                                        WirelessPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            if (( 5 == wepKeySize
               && 0 == _wcsicmp(it->second, WiFiWepKeyLength104Bit))
             || (13 == wepKeySize
               && 0 == _wcsicmp(it->second, WiFiWepKeyLength40Bit)))
            {
                params.erase(it);
                params.insert(WiFiWepKeyLengthName,
                             (5 == wepKeySize)? WiFiWepKeyLength40Bit
                                              : WiFiWepKeyLength104Bit);
                wepKeySpecsChanged = true;
            }
    
            // If the key-size or -encoding hasn't changed, update the
            // WEP keys.
            if (!wepKeySpecsChanged)
            {
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
                                          "%s%d", WiFiWepKeyName, kx+1);
                    if (FAILED(hr))
                        return HRESULT_CODE(hr);
    
                    it = HtmlUtils::LookupParam(params, name,
                                                "WEP key",
                                                WirelessPage);
                    if (it == params.end())
                        return ERROR_INVALID_PARAMETER;
                    params.erase(it);
                    params.insert(name, buff);
                }
            }
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
            it = HtmlUtils::LookupParam(params, WiFiPassphraseName1,
                                        "Passphrase",
                                        WirelessPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            params.erase(it);
            params.insert(WiFiPassphraseName1, passphrase);

            it = HtmlUtils::LookupParam(params, WiFiPassphraseName2,
                                        "Passphrase confirm",
                                        WirelessPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            params.erase(it);
            params.insert(WiFiPassphraseName2, passphrase);
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
            hr = StringCchPrintf(buff, buffSize, 
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
                         WirelessPage);
                return ERROR_INVALID_PARAMETER;
            }
    
            // If necessary, enable EAP authentication.
            if (authMode != APAuthWPA && authMode != APAuthWPA2)
            {
                authMode = APAuthWPA;
                fieldFlags |= (int)pState->FieldMaskSecurityMode;
            }
    
            // Update the server.
            it = HtmlUtils::LookupParam(params, WiFiRadiusServerName,
                                        "Radius server",
                                        WirelessPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            params.erase(it);
            params.insert(WiFiRadiusServerName, buff);
        
            // Update the port.
            hr = StringCchPrintf(buff, buffSize,
                                 L"%d", NewState.GetRadiusPort());
            if (FAILED(hr))
                return HRESULT_CODE(hr);
            it = HtmlUtils::LookupParam(params, WiFiRadiusPortName,
                                        "Radius server port",
                                        WirelessPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            params.erase(it);
            params.insert(WiFiRadiusPortName, buff);
        
            // Update the secret key.
            it = HtmlUtils::LookupParam(params, WiFiRadiusSecretName,
                                        "Radius secret key",
                                        WirelessPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            params.erase(it);
            params.insert(WiFiRadiusSecretName, secret);
        }

        // Security modes:
        if (fieldFlags & (int)pState->FieldMaskSecurityMode)
        {
            if (4 <= tries)
            {
                LogError(TEXT("Unable to update security modes")
                         TEXT(" to auth=%s")
                         TEXT(" and cipher=%s in page \"%ls\""),
                         NewState.GetAuthName(),
                         NewState.GetCipherName(),
                         WirelessPage);
                return ERROR_INVALID_PARAMETER;
            }
    
            const WCHAR *pSecurityModeValue = WiFiSecurityModeNone;
            const WCHAR *pWepAuthModeValue  = WiFiWepAuthModeOpen;

            switch (cipherMode)
            {
                case APCipherWEP:
                    pSecurityModeValue = WiFiSecurityModeWep;
                    if (authMode == APAuthShared)
                        pWepAuthModeValue = WiFiWepAuthModeShared;
                    break;
        
                case APCipherTKIP:
                    if (authMode == APAuthWPA_PSK)
                         pSecurityModeValue = WiFiSecurityModeWpaPsk;
                    else pSecurityModeValue = WiFiSecurityModeWpa;
                    break;
        
            }
        
            it = HtmlUtils::LookupParam(params, WiFiSecurityModeName,
                                        "Security mode",
                                        WirelessPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            params.erase(it);
            params.insert(WiFiSecurityModeName, pSecurityModeValue);
        
            it = HtmlUtils::LookupParam(params, WiFiWepAuthModeName,
                                        "WEP auth mode",
                                        WirelessPage);
            if (it == params.end())
                return ERROR_INVALID_PARAMETER;
            params.erase(it);
            params.insert(WiFiWepAuthModeName, pWepAuthModeValue);
        }

        // Turn on the "apply" flag.
        it = HtmlUtils::LookupParam(params, WiFiApplyFlagName,
                                    "apply-changes flag",
                                    WirelessPage);
        if (it == params.end())
            return ERROR_INVALID_PARAMETER;
        params.erase(it);
        params.insert(WiFiApplyFlagName, WiFiApplyFlagTrue);
    
        // Update the configuration.
        result = pHttp->SendUpdateRequest(WirelessPage,
                                          forms.GetMethod(0),
                                          forms.GetAction(0),
                                          params,
                                          WirelessRestart,
                                         &webPage,
                                         "device is restarting");
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
DLinkDI524Controller_t::
InitializeDevice(void)
{
    ce::string webPage;
    HtmlForms_t forms;

    // Load the device-info page.
    DWORD result = m_pHttp->SendPageRequest(DeviceInfoPage, 0L, &webPage);
    if (ERROR_SUCCESS != result)
        return result;

    // Find and load the BSSID (MAC Address).
    MACAddr_t bssid;
    const char *pValue = strstr(webPage.get_buffer(), "MAC"); // skip LAN MAC
    if (pValue) pValue = strstr(pValue, "MAC");
    if (pValue)
    {
        for (; pValue[0] ; ++pValue)
        {
            if (isxdigit((unsigned char)pValue[0]))
            {
                HRESULT hr = bssid.FromString(pValue);
                if (SUCCEEDED(hr))
                    break;
            }
        }
    }
    if (!pValue || !pValue[0])
    {
        LogError(TEXT("MAC Address missing from \"%ls\""),
                 DeviceInfoPage);
        return ERROR_INVALID_PARAMETER;
    }
    m_pState->SetBSsid(bssid);

    // Load and parse the wireless-lan page.
    return LoadWirelessPage(m_pHttp, m_pState, &webPage, &forms);
}

// ----------------------------------------------------------------------------
//
// Gets the configuration settings from the device.
//
DWORD
DLinkDI524Controller_t::
LoadSettings(void)
{
    ce::string webPage;
    HtmlForms_t forms;
    return LoadWirelessPage(m_pHttp, m_pState, &webPage, &forms);
}

// ----------------------------------------------------------------------------
//
// Updates the device's configuration settings.
//
DWORD
DLinkDI524Controller_t::
UpdateSettings(const AccessPointState_t &NewState)
{
    return UpdateWirelessPage(m_pHttp, m_pState, NewState);
}

// ----------------------------------------------------------------------------
