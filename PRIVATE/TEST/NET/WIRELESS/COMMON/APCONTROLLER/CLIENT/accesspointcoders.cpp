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
// Implementation of the AccessPointCoders class.
//
// ----------------------------------------------------------------------------

#include "AccessPointCoders.hpp"

#include <APConfigurator_t.hpp>

#include <assert.h>
#include <inc/hash_map.hxx>
#include <tchar.h>
#include <strsafe.h>
#include <winsock.h>

using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Macros:
//
#define CKHR(expr)  if (FAILED(hr = (expr)))  return HRESULT_CODE(hr)
#define CKAPP(expr) if (!pArgs->append(expr)) return ERROR_OUTOFMEMORY

// ----------------------------------------------------------------------------
//
// Trims spaces from the front and back of the specified token.
//
static TCHAR *
TrimToken(TCHAR *pToken)
{
    for (; _istspace(*pToken) ; ++pToken) ;
    TCHAR *pEnd = pToken;
    for (; *pEnd != TEXT('\0') ; ++pEnd) ;
    while (pEnd-- != pToken)
        if (_istspace(*pEnd))
            *pEnd = TEXT('\0');
        else break;
    return pToken;
}

// ----------------------------------------------------------------------------
//
// Initializes an Access Point state object from a command-line argument.
//
DWORD
AccessPointCoders::
String2State(
    const TCHAR        *pArgs,
    AccessPointState_t *pState)
{
    HRESULT hr;

    pState->Clear();

    // Separate the params into a mapped list.
    ce::hash_map<ce::tstring,const TCHAR *> params;
    ce::hash_map<ce::tstring,const TCHAR *>::iterator found;
    ce::tstring lArgs(pArgs);
    for (TCHAR *name = _tcstok(lArgs.get_buffer(), TEXT("&")) ;
                name != NULL ;
                name = _tcstok(NULL, TEXT("&")))
    {
        const TCHAR *value = TEXT("");
        TCHAR *equal = _tcschr(name, TEXT('='));
        if (NULL != equal)
        {
            equal[0] = TEXT('\0');
            value = TrimToken(equal+1);
        }
        name = TrimToken(name);
        if (name[0] != TEXT('\0'))
        {
            params.insert(ce::tstring(name), value);
        }
    }

    found = params.find(ce::tstring(APConfigurator_t::VendorNameKey));
    if (params.end() != found)
    {
        CKHR(pState->SetVendorName(found->second));
        params.erase(found);
    }

    found = params.find(ce::tstring(APConfigurator_t::ModelNumberKey));
    if (params.end() != found)
    {
        CKHR(pState->SetModelNumber(found->second));
        params.erase(found);
    }

    found = params.find(ce::tstring(APConfigurator_t::SsidKey));
    if (params.end() != found)
    {
        CKHR(pState->SetSsid(found->second));
        params.erase(found);
    }

    found = params.find(ce::tstring(APConfigurator_t::BSsidKey));
    if (params.end() != found)
    {
        MACAddr_t bssid;
        hr = bssid.FromString(found->second);
        if (FAILED(hr))
        {
            LogError(TEXT("Bad %s value \"%s\""), 
                     APConfigurator_t::BSsidKey, &(found->second)[0]);
            return HRESULT_CODE(hr);
        }
        pState->SetBSsid(bssid);
        params.erase(found);
    }

    found = params.find(ce::tstring(APConfigurator_t::CapsImplementedKey));
    if (params.end() != found)
    {
        int caps = String2APCapabilities(found->second);
        if (caps >= (int)UnknownAPCapability)
        {
            LogError(TEXT("Bad %s value \"%s\""),
                     APConfigurator_t::CapsImplementedKey, &(found->second)[0]);
            return ERROR_INVALID_PARAMETER;
        }
        pState->SetCapabilitiesImplemented(caps);
        params.erase(found);
    }

    found = params.find(ce::tstring(APConfigurator_t::CapsEnabledKey));
    if (params.end() != found)
    {
        int caps = String2APCapabilities(found->second);
        if (caps >= (int)UnknownAPCapability)
        {
            LogError(TEXT("Bad %s value \"%s\""),
                     APConfigurator_t::CapsEnabledKey, &(found->second)[0]);
            return ERROR_INVALID_PARAMETER;
        }
        pState->SetCapabilitiesEnabled(caps);
        params.erase(found);
    }

    found = params.find(ce::tstring(APConfigurator_t::RadioStateKey));
    if (params.end() != found)
    {
        pState->SetRadioState(_tcstol(found->second, NULL, 10) != 0);
        params.erase(found);
    }

    found = params.find(ce::tstring(APConfigurator_t::AuthenticationKey));
    if (params.end() != found)
    {
        APAuthMode_e auth = String2APAuthMode(found->second);
        if ((int)auth >= (int)UnknownAPAuthMode)
        {
            LogError(TEXT("Bad %s value \"%s\""),
                     APConfigurator_t::AuthenticationKey, &(found->second)[0]);
            return ERROR_INVALID_PARAMETER;
        }
        pState->SetAuthMode(auth);
        params.erase(found);
    }

    found = params.find(ce::tstring(APConfigurator_t::CipherKey));
    if (params.end() != found)
    {
        APCipherMode_e auth = String2APCipherMode(found->second);
        if ((int)auth >= (int)UnknownAPCipherMode)
        {
            LogError(TEXT("Bad %s value \"%s\""),
                     APConfigurator_t::CipherKey, &(found->second)[0]);
            return ERROR_INVALID_PARAMETER;
        }
        pState->SetCipherMode(auth);
        params.erase(found);
    }

    found = params.find(ce::tstring(APConfigurator_t::RadiusServerKey));
    if (params.end() != found)
    {
        int addr[4] = { -1, -1, -1, -1 };
        const TCHAR *data = found->second;
        for (int ax = 0 ; ax < 4 && *data ; ++ax)
        {
            addr[ax] = 0;
            for (int nx = 0 ; nx < 4 ; ++nx)
            {
                int ch = *(data++);
                if (L'0' <= ch && ch <= L'9') ch -= L'0';
                else if (L'.' == ch) break;
                else goto outerLoop;
                addr[ax] *= 10;
                addr[ax] += ch;
            }
        }
outerLoop:
        if (  0 > addr[0] ||   0 > addr[1] ||   0 > addr[2] ||   0 > addr[3]
         || 255 < addr[0] || 255 < addr[1] || 255 < addr[2] || 255 < addr[3])
        {
            LogError(TEXT("Bad %s value \"%s\""),
                     APConfigurator_t::RadiusServerKey, &(found->second)[0]);
            return ERROR_INVALID_PARAMETER;
        }
        in_addr inaddr;
        inaddr.s_net   = addr[0];
        inaddr.s_host  = addr[1];
        inaddr.s_lh    = addr[2];
        inaddr.s_impno = addr[3];
        pState->SetRadiusServer(inaddr.s_addr);
        params.erase(found);
    }

    found = params.find(ce::tstring(APConfigurator_t::RadiusPortKey));
    if (params.end() != found)
    {
        pState->SetRadiusPort((int)_tcstoul(found->second, NULL, 10));
        params.erase(found);
    }

    found = params.find(ce::tstring(APConfigurator_t::RadiusSecretKey));
    if (params.end() != found)
    {
        CKHR(pState->SetRadiusSecret(found->second));
        params.erase(found);
    }

    WEPKeys_t keyData;
    TCHAR     keyName[32];
    bool foundWEPKeys = false;
    for (int kx = 0 ; kx < 4 ; ++kx)
    {
        CKHR(StringCchPrintf(keyName, COUNTOF(keyName),
                             TEXT("%s%d"), APConfigurator_t::WEPKeyKey, kx+1));
        found = params.find(ce::tstring(keyName));
        if (params.end() != found)
        {
            foundWEPKeys = true;
            hr = keyData.FromString(kx, found->second);
            if (FAILED(hr))
            {
                LogError(TEXT("Bad %s value \"%s\""), 
                         keyName, &(found->second)[0]);
                return HRESULT_CODE(hr);
            }
            params.erase(found);
        }
    }

    found = params.find(ce::tstring(APConfigurator_t::WEPIndexKey));
    if (params.end() != found)
    {
        keyData.m_KeyIndex = (int)_tcstoul(found->second, NULL, 10);
        if (!keyData.IsValid())
        {
            LogError(TEXT("Bad %s \"%s\" (or corresponding key)"),
                     APConfigurator_t::WEPIndexKey, &(found->second)[0]);
        }
        foundWEPKeys = true;
    }

    if (foundWEPKeys)
    {
        pState->SetWEPKeys(keyData);
    }

    found = params.find(ce::tstring(APConfigurator_t::PassphraseKey));
    if (params.end() != found)
    {
        CKHR(pState->SetPassphrase(found->second));
        params.erase(found);
    }

    if (params.size())
    {
        found = params.begin();
        LogError(TEXT("Parameter name \"%s=%s\" unrecognized"),
                &found->first[0], &(found->second)[0]);
        return ERROR_INVALID_PARAMETER;
    }

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Translates an Access Point state object to command-argument form.
//
DWORD
AccessPointCoders::
State2String(
    const AccessPointState_t &State,
    ce::tstring              *pArgs)
{
    HRESULT hr;

    TCHAR data[512];
    int   dataChars = COUNTOF(data);
    TCHAR buff1[256];
    int   buff1Chars = COUNTOF(buff1);
    TCHAR buff2[128];
    int   buff2Chars = COUNTOF(buff2);

    const TCHAR *prefix = TEXT("");
    
    pArgs->clear();

    CKHR(State.GetVendorName(data, dataChars));
    if (data[0])
    {
        CKHR(StringCchPrintf(buff1, buff1Chars, TEXT("%s=%s"),
                             APConfigurator_t::VendorNameKey,
                             data));
        CKAPP(buff1);
        prefix = TEXT("\n");
    }

    CKHR(State.GetModelNumber(data, dataChars));
    if (data[0])
    {
        CKHR(StringCchPrintf(buff1, buff1Chars, TEXT("%s%s=%s"), prefix,
                             APConfigurator_t::ModelNumberKey,
                             data));
        CKAPP(buff1);
        prefix = TEXT("\n");
    }

    CKHR(State.GetSsid(data, dataChars));
    if (data[0])
    {
        CKHR(StringCchPrintf(buff1, buff1Chars, TEXT("%s%s=%s"), prefix,
                             APConfigurator_t::SsidKey,
                             data));
        CKAPP(buff1);
        prefix = TEXT("\n");
    }

    const MACAddr_t &bssid = State.GetBSsid();
    if (bssid.IsValid())
    {
        bssid.ToString(buff2, buff2Chars);
        CKHR(StringCchPrintf(buff1, buff1Chars, TEXT("%s%s=%s"), prefix,
                             APConfigurator_t::BSsidKey, buff2));
        CKAPP(buff1);
        prefix = TEXT("\n");
    }

    int caps = (int)State.GetCapabilitiesImplemented(); 
    if (caps)
    {
        CKHR(APCapabilities2String(caps, buff2, buff2Chars));
        CKHR(StringCchPrintf(buff1, buff1Chars, TEXT("%s%s=%s"), prefix,
                             APConfigurator_t::CapsImplementedKey, buff2));
        CKAPP(buff1);
        prefix = TEXT("\n");
    }

    caps = (int)State.GetCapabilitiesEnabled(); 
    if (caps)
    {
        CKHR(APCapabilities2String(caps, buff2, buff2Chars));
        CKHR(StringCchPrintf(buff1, buff1Chars, TEXT("%s%s=%s"), prefix,
                             APConfigurator_t::CapsEnabledKey, buff2));
        CKAPP(buff1);
        prefix = TEXT("\n");
    }

    if (State.IsRadioOn())
    {
        CKHR(StringCchPrintf(buff1, buff1Chars, TEXT("%s%s=%d"), prefix,
                             APConfigurator_t::RadioStateKey,
                             State.IsRadioOn()?1:0));
        CKAPP(buff1);
        prefix = TEXT("\n");
    }

    APAuthMode_e auth = State.GetAuthMode();
    if (auth != APAuthOpen)
    {
        CKHR(StringCchPrintf(buff1, buff1Chars, TEXT("%s%s=%s"), prefix,
                             APConfigurator_t::AuthenticationKey,
                             APAuthMode2String(auth)));
        CKAPP(buff1);
        prefix = TEXT("\n");
    }

    APCipherMode_e cipher = State.GetCipherMode();
    if (cipher != APCipherClearText)
    {
        CKHR(StringCchPrintf(buff1, buff1Chars, TEXT("%s%s=%s"), prefix,
                             APConfigurator_t::CipherKey, 
                             APCipherMode2String(cipher)));
        CKAPP(buff1);
        prefix = TEXT("\n");
    }

    if (State.GetRadiusServer())
    {
        in_addr inaddr;
        inaddr.s_addr = State.GetRadiusServer();
        CKHR(StringCchPrintf(buff1, buff1Chars, TEXT("%s%s=%u.%u.%u.%u"), prefix,
                             APConfigurator_t::RadiusServerKey,
                             inaddr.s_net, inaddr.s_host,
                             inaddr.s_lh, inaddr.s_impno));
        CKAPP(buff1);
        prefix = TEXT("\n");
    }

    if (State.GetRadiusPort())
    {
        CKHR(StringCchPrintf(buff1, buff1Chars, TEXT("%s%s=%d"), prefix,
                             APConfigurator_t::RadiusPortKey, 
                             State.GetRadiusPort()));
        CKAPP(buff1);
        prefix = TEXT("\n");
    }

    CKHR(State.GetRadiusSecret(data, dataChars));
    if (data[0])
    {
        CKHR(StringCchPrintf(buff1, buff1Chars, TEXT("%s%s=%s"), prefix,
                             APConfigurator_t::RadiusSecretKey,
                             data));
        CKAPP(buff1);
        prefix = TEXT("\n");
    }

    const WEPKeys_t &keys = State.GetWEPKeys();
    if (keys.IsValid())
    {
        for (int kx = 0 ; kx < 4 ; ++kx)
        {
            if (keys.ValidKeySize(keys.m_Size[kx]))
            {
                keys.ToString(kx, buff2, buff2Chars);
                CKHR(StringCchPrintf(buff1, buff1Chars, TEXT("%s%s%d=%s"), prefix,
                                     APConfigurator_t::WEPKeyKey,
                                     kx+1, buff2));
                CKAPP(buff1);
                prefix = TEXT("\n");
            }
        }
    }

    CKHR(State.GetPassphrase(data, dataChars));
    if (data[0])
    {
        CKHR(StringCchPrintf(buff1, buff1Chars, TEXT("%s%s=%s"), prefix,
                             APConfigurator_t::PassphraseKey, 
                             data));
        CKAPP(buff1);
        prefix = TEXT("\n");
    }

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
