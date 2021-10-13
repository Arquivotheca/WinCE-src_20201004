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
// Implementation of the ManualAPConfigDevice_t class.
//
// ----------------------------------------------------------------------------

#include "ManualAPConfigDevice_t.hpp"

#include "APCUtils.hpp"

#include <assert.h>
#include <strsafe.h>
#include <winsock.h>

using namespace ce;
using namespace ce::qa;

// ----------------------------------------------------------------------------
//  
// Constructor.
//
ManualAPConfigDevice_t::
ManualAPConfigDevice_t(
    const TCHAR *pAPName,
    const TCHAR *pConfigKey,
    const TCHAR *pDeviceType,
    const TCHAR *pDeviceName)
    : APConfigDevice_t(pAPName, pConfigKey, pDeviceType, pDeviceName, NULL)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
ManualAPConfigDevice_t::
~ManualAPConfigDevice_t(void)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Generates a configurator-type parameter for use by CreateConfigurator.
//
HRESULT
ManualAPConfigDevice_t::
CreateConfiguratorType(ce::tstring *pConfigParam) const
{
    return pConfigParam->assign(GetDeviceType())? S_OK : E_OUTOFMEMORY;
}

// ----------------------------------------------------------------------------
//
// Gets the current Access Point configuration from the registry.
//
DWORD
ManualAPConfigDevice_t::
GetAccessPoint(
    AccessPointState_t *pResponse)
{
    ce::gate<ce::critical_section> locker(m_Locker);
   *pResponse = m_State;
    pResponse->SetFieldFlags(0);
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Tells the operator how the Access Point should be configured and
// updates the registry.
//
DWORD
ManualAPConfigDevice_t::
SetAccessPoint(
    const AccessPointState_t &NewState,
          AccessPointState_t *pResponse)
{
    ce::gate<ce::critical_section> locker(m_Locker);

    AccessPointState_t originalState = m_State;
    m_State = NewState;

    HRESULT hr = SaveConfiguration();
    if (FAILED(hr))
    {
        if (E_ABORT == hr)
        {
            LogError(TEXT("Operator cancelled AP configuration change"));
        }
        else
        {
            LogError(TEXT("Error saving new AP configuration settings: %s"),
                     HRESULTErrorText(hr));
        }
        m_State = originalState;
        return HRESULT_CODE(hr);
    }
   
   *pResponse = m_State;
    pResponse->SetFieldFlags(NewState.GetFieldFlags());
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Tells the operator how the Access Point should be configured:
//
HRESULT
ManualAPConfigDevice_t::
SaveConfiguration(void)
{
    static const TCHAR star   = TEXT('*');
    static const TCHAR space  = TEXT(' ');

    HRESULT hr;

    TCHAR buff[128];
    DWORD buffChars = COUNTOF(buff);

    TCHAR caps[128];
    DWORD capsChars = COUNTOF(caps);

    ce::tstring caption, msg;
    TCHAR changed;

    // Only do the rest if there has been a configuration change.
    int fieldFlags = m_State.GetFieldFlags();
    if (fieldFlags == 0)
        return ERROR_SUCCESS;
        
#define CKHR(expr) \
    hr = (expr); \
    if (FAILED(hr)) \
        return hr

    caption.assign(TEXT("Set AP '"));
    caption.append(GetAPName());
    caption.append(TEXT("' configuration"));

    // Vendor name and model number.
    CKHR(StringCchPrintf(buff, buffChars,
                         TEXT("  Vendor name: %s\n"),
                         GetVendorName()));
    msg += buff;

    CKHR(StringCchPrintf(buff, buffChars,
                         TEXT("  Model number: %s\n\n"),
                         GetModelNumber()));
    msg += buff;

    // Capabilities.
    if (fieldFlags & (int)AccessPointState_t::FieldMaskCapabilities)
         changed = star;
    else changed = space;

    CKHR(StringCchPrintf(buff, buffChars,
                         TEXT("%c Capabiliies:\n"), changed));
    msg += buff;

    CKHR(APCapabilities2String(GetCapabilitiesImplemented(), caps, capsChars));
    CKHR(StringCchPrintf(buff, buffChars,
                         TEXT("      implemented: %s\n"),
                         caps));
    msg += buff;

    CKHR(APCapabilities2String(GetCapabilitiesEnabled(), caps, capsChars));
    CKHR(StringCchPrintf(buff, buffChars,
                         TEXT("    %c enabled:        %s\n\n"), changed,
                         caps));
    msg += buff;

    // SSID.
    if (fieldFlags & (int)AccessPointState_t::FieldMaskSsid)
         changed = star;
    else changed = space;

    CKHR(StringCchPrintf(buff, buffChars,
                         TEXT("%c SSID: %s\n"), changed,
                         GetSsid()));    
    msg += buff;

    // BSSID.
    const MACAddr_t &bssid = GetBSsid();
    if (fieldFlags & (int)AccessPointState_t::FieldMaskBSsid)
         changed = star;
    else changed = space;

    if (changed == star || bssid.IsValid())
    {
        TCHAR hexbuff[sizeof(bssid) * 4];
        bssid.ToString(hexbuff, COUNTOF(hexbuff));
        CKHR(StringCchPrintf(buff, buffChars,
                             TEXT("%c BSSID: %s\n\n"), changed, hexbuff));
        msg += buff;
    }
    
    // Radio status.
    if (fieldFlags & (int)AccessPointState_t::FieldMaskRadioState)
         changed = star;
    else changed = space;

    CKHR(StringCchPrintf(buff, buffChars,
                         TEXT("%c Radio State: %hs\n\n"), changed,
                         (IsRadioOn()? "ON" : "OFF")));
    msg += buff;

    // Authentication and cipher.
    if (fieldFlags & (int)AccessPointState_t::FieldMaskSecurityMode)
         changed = star;
    else changed = space;

    CKHR(StringCchPrintf(buff, buffChars, 
                         TEXT("%c Security modes:\n"), changed));
    msg += buff;

    CKHR(StringCchPrintf(buff, buffChars, 
                         TEXT("    %c Authentication: %s\n"), changed,
                         APAuthMode2String(GetAuthMode())));
    msg += buff;

    CKHR(StringCchPrintf(buff, buffChars, 
                         TEXT("    %c Cipher:             %s\n\n"), changed,
                         APCipherMode2String(GetCipherMode())));
    msg += buff;

    // Radius server.
    if (fieldFlags & (int)AccessPointState_t::FieldMaskRadius)
         changed = star;
    else changed = space;

    if (changed == star || GetRadiusServer() != 0)
    {
        in_addr inaddr;
        inaddr.s_addr = GetRadiusServer();
        
        CKHR(StringCchPrintf(buff, buffChars, 
                             TEXT("%c Radius:\n"), changed));
        msg += buff;

        CKHR(StringCchPrintf(buff, buffChars, 
                             TEXT("    %c Server: %u.%u.%u.%u\n"), changed,
                             inaddr.s_net, inaddr.s_host,
                             inaddr.s_lh, inaddr.s_impno));
        msg += buff;

        CKHR(StringCchPrintf(buff, buffChars, 
                             TEXT("    %c Port:     %u\n"), changed,
                             GetRadiusPort()));
        msg += buff;

        CKHR(StringCchPrintf(buff, buffChars, 
                             TEXT("    %c Secret: '%s'\n\n"), changed,
                             GetRadiusSecret()));
        msg += buff;
    }

    // WEP keys.
    const WEPKeys_t &keys = GetWEPKeys();
    if (keys.IsValid())
    {
        if (fieldFlags & (int)AccessPointState_t::FieldMaskWEPKeys)
             changed = star;
        else changed = space;

        CKHR(StringCchPrintf(buff, buffChars,
                             TEXT("%c WEP Keys:\n"), changed));
        msg += buff;

        for (int kx = 0 ; kx < 4 ; ++kx)
        {
            char hexbuff[128];
            const char *hexresult = hexbuff;
            if (keys.ValidKeySize(keys.m_Size[kx]))
                 keys.ToString(kx, hexbuff, COUNTOF(hexbuff));
            else hexresult = "    INVALID";

            const char *keyActive;
            if (kx == static_cast<int>(keys.m_KeyIndex))
                 keyActive = " (selected)";
            else keyActive = "";

            CKHR(StringCchPrintf(buff, buffChars,
                                 TEXT("     %d > %hs%hs\n"), kx,
                                 hexresult, keyActive));
            msg += buff;
        }
        msg += TEXT("\n");
    }

    // Passphrase.
    if (fieldFlags & (int)AccessPointState_t::FieldMaskPassphrase)
         changed = star;
    else changed = space;

    if (changed == star || GetPassphrase()[0] != TEXT('\0'))
    {
        CKHR(StringCchPrintf(buff, buffChars,
                             TEXT("%c Passphrase: '%s'\n\n"), changed,
                             GetPassphrase()));
        msg += buff;
    }

    msg += TEXT("            * modified");
    
    // Pop up a message-box.
    if (MessageBox(NULL, msg, caption, MB_OKCANCEL) != IDOK)
        return E_ABORT;

    return S_OK;

#undef CKHR
}

// ----------------------------------------------------------------------------
