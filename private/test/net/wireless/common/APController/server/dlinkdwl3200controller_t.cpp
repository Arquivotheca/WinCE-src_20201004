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
// Implementation of the DLinkDWL3200Controller_t class.
//
// ----------------------------------------------------------------------------

#include "DLinkDWL3200Controller_t.hpp"

#include <assert.h>
#include <strsafe.h>

#include "AccessPointState_t.hpp"
#include "HttpPort_t.hpp"

// Define this if you want LOTS of debug output:
#define EXTRA_DEBUG 1

using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Web-page definitions:
//
struct WebPageDesc_t
{
    APAuthMode_e mode;  // page's security settings
    WCHAR       *name;  // page URL
};
static const WebPageDesc_t s_WebPages[] =
{
    // Performance page:
    {   NumberAPAuthModes,
        L"/html/CfgWLanParam.html?1"
    },

    // WEP settings page:
    {   APAuthShared,
        L"/html/Wireless.html?1,0,0"
    },

    // WPA (Radius) settings page:
    {   APAuthWPA,
        L"/html/Wireless.html?1,0,3"
    },

    // PSK settings page:
    {   APAuthWPA_PSK,
        L"/html/Wireless.html?1,0,4"
    }
};
enum { s_NumberWebPages = sizeof(s_WebPages) / sizeof(s_WebPages[0]) };

// ----------------------------------------------------------------------------
//
// Web-page parameters:
//

// Radio state:
static const char  s_RadioStateName[] = "radio_wave";
static const WCHAR s_RadioStateOn  [] = L"1";
static const WCHAR s_RadioStateOff [] = L"0";

// WMM capability:
static const char  s_WMMCapName    [] = "WMM";
static const WCHAR s_WMMCapEnabled [] = L"1";
static const WCHAR s_WMMCapDisabled[] = L"0";

// SSID:
static const char s_SsidParamName[] = "Ssid";

// SSID-broadcask flag:
static const char  s_SsidBroadcastName[] = "SsidBroadcast";
static const WCHAR s_SsidBroadcastOn  [] = L"Enable";
static const WCHAR s_SsidBroadcastOff [] = L"Disable";

// WEP encryption mode:
static const char  s_WepCipherName[] = "WepType";
static const WCHAR s_WepCipherOn  [] = L"1";
static const WCHAR s_WepCipherOff [] = L"0";

// WEP keys:
static const char   s_WepKeyName        [] = "Key";
static const char   s_WepKeyLengthName  [] = "KS";
static const WCHAR  s_WepKeyLength40Bit [] = L"0";
static const WCHAR  s_WepKeyLength104Bit[] = L"1";
static const WCHAR  s_WepKeyLength128Bit[] = L"2";
static const char   s_WepKeyTypeName    [] = "Wep_Key_Type";
static const WCHAR  s_WepKeyTypeHex     [] = L"0";
static const WCHAR  s_WepKeyTypeAscii   [] = L"1";
static const char   s_WepKeyIndexName   [] = "ValidIndex";

static const WCHAR *s_WepKeyIndexValueV1  [] = { L"0", L"1", L"2", L"3" };
static const WCHAR *s_WepKeyIndexValueV2_1[] = { L"First", L"Second",
                                                 L"Third", L"Fourth" };
// Cipher-type:
static const char  s_WpaCipherName[] = "Cipher";
static const WCHAR s_WpaCipherAuto[] = L"AUTO";
static const WCHAR s_WpaCipherTkip[] = L"TKIP";
static const WCHAR s_WpaCipherAes [] = L"AES";

// Radius information:
static const char s_RadiusServerName[] = "radiusserver";
static const char s_RadiusPortName  [] = "radiusport";
static const char s_RadiusSecretName[] = "radiussecret";

// Passphrase:
static const char s_PassphraseName[] = "passphrase";

// Authentication modes:
static const char s_AuthModeIndex[] = "authstatus";
static const char s_AuthModeName[] = "AuthMenu";
enum {
    AuthModeIndexOpen = 0
   ,AuthModeIndexShared
   ,AuthModeIndexOpenShared
   ,AuthModeIndexWpaEap
   ,AuthModeIndexWpaPsk
   ,AuthModeIndexWpa2Eap
   ,AuthModeIndexWpa2Psk
   ,AuthModeIndexWpaAutoEap
   ,AuthModeIndexWpaAutoPsk
   ,NumberAuthModeIndexes
};
static const WCHAR * const s_AuthModeValuesV1[NumberAuthModeIndexes] = {
     L"Open System"
    ,L"Shared Key"
    ,L"Open System/Shared Key"
    ,L"WPA-EAP"
    ,L"WPA-PSK"
    ,L"WPA2-EAP"
    ,L"WPA2-PSK"
    ,L"WPA-Auto-EAP"
    ,L"WPA-Auto-PSK"
};
static const WCHAR * const s_AuthModeValuesV2_1[NumberAuthModeIndexes] = {
     L"Open System"
    ,L"Shared Key"
    ,L"Open System/Shared Key"
    ,L"WPA-Enterprise"
    ,L"WPA-Personal"
    ,L"WPA2-Enterprise"
    ,L"WPA2-Personal"
    ,L"WPA-Auto-Enterprise"
    ,L"WPA-Auto-Personal"
};

// ----------------------------------------------------------------------------
//
// Miscellaneous:
//

// Device-information page:
static const WCHAR s_DeviceInfoPage[] = L"/html/DeviceInfo.html";

// Device-reset page:
static const WCHAR s_ResetPage[] = L"/html/MntRestartSystem.html";
static const WCHAR s_ResetFormName[] = L"RESET";

// Device-restart time:
static long s_PageRestartTimeV1   = 15000L;
static long s_PageRestartTimeV2_1 = 28000L;

// Successful-update semaphore:
static const char s_UpdateSucceeded[] = "device is restarting";

// ----------------------------------------------------------------------------
//
// Searches the specified web-page looking for JavaScript
// "document.cookie" settings.
//
static DWORD
GetWebPageCookies(
    HttpPort_t       *pHttp,
    const ce::string &WebPage)
{
    DWORD result;
    const char *pCursor, *oldCursor = &WebPage[0];

    ce::string script;
    for (; oldCursor && oldCursor[0] ; oldCursor = pCursor)
    {
        pCursor = HtmlUtils::FindString(oldCursor, "<script", ">",
                                                  "</script>", &script);
        if (oldCursor == pCursor)
            break;

        for (char *pCookie = &script[0] ;;)
        {
            static const char CookieAssignment[] = "document.cookie";
            static size_t     CookieAssignmentChars = strlen(CookieAssignment);
            pCookie = strstr(pCookie, CookieAssignment);
            if (!pCookie || !pCookie[0])
                break;
            pCookie += CookieAssignmentChars;

            while (pCookie[0] && (isspace((unsigned char)pCookie[0])
                                                      || pCookie[0] == '='))
                   pCookie++;
            char quote = pCookie[0];
            if (quote != '"' && quote != '\'')
                continue;
            const char *pName = ++pCookie;

            char *pValue = pCookie;
            while (pValue[0] && pValue[0] != '=' && pValue[0] != quote)
                   pValue++;
            if (pValue[0] != '=')
                continue;

            pCookie = pValue;
            while (pCookie[0] && pCookie[0] != quote)
                   pCookie++;
            if (pCookie[0] != quote)
                continue;
            pCookie[0] = '\0';

            pValue[0] = ' ';
            while (--pValue > pName   && isspace((unsigned char)pValue[0])) ;
            while (++pValue < pCookie && isspace((unsigned char)pValue[0]))
                     pValue[0] = '\0';
            pCookie++;
#ifdef EXTRA_DEBUG
            LogDebug(L"[AC] Found cookie %hs = \"%hs\"", pName, pValue);
#endif

            result = pHttp->SetCookie(pName, pValue);
            if (ERROR_SUCCESS != result)
                return result;
        }
    }

    return ERROR_SUCCESS;
}

/* ========================= DLinkDWL3200Request_t ========================= */

// ----------------------------------------------------------------------------
//
// DLinkDWL3200Request_t encapsulates all the data and methods required to
// process a single DWL-3200 interaction.
//
class DLinkDWL3200Request_t
{
private:

    // Device interface controller:
    HttpPort_t *m_pHttp;

    // Device's current configuration:
    AccessPointState_t *m_pState;

    // Firmware version number:
    float m_FirmwareVersion;

    // Firmware-specific settings:
    const WCHAR * const *m_AuthModeValues;   // WEP cipher param values
    const WCHAR * const *m_WepKeyIndexValue; // WEP key index number param
    long                 m_PageRestartTime;  // Time to restart after update

    // Web-page buffer:
    ce::string m_WebPage;

    // Parsed web-page forms:
    HtmlForms_t m_PageForms;

    // Page currently being processed:
    const WCHAR *m_pPageName;
    APAuthMode_e m_PageMode; // Shared, WPA or WPA_PSK

    // Bit-maps indicating which fields need to be updated:
    int m_UpdatesRequested; // Changes requested by the user
    int m_UpdatesRemaining; // Changes remaining in this page

    // Page's current and initial security modes:
    APAuthMode_e   m_AuthMode,   m_RequestedAuthMode;
    APCipherMode_e m_CipherMode, m_RequestedCipherMode;

public:

    // Constructor and destructor:
    DLinkDWL3200Request_t(
        HttpPort_t         *pHttp,
        AccessPointState_t *pState,
        float               Version);
   ~DLinkDWL3200Request_t(void)
    { }

    // Loads settings from specified web-page:
    DWORD LoadPerformancePage(void);
    DWORD LoadWepSettingsPage(void);
    DWORD LoadWpaSettingsPage(void);
    DWORD LoadPskSettingsPage(void);

    // Updates all the settings:
    DWORD UpdateSettings(const AccessPointState_t &NewState);

private:

    // Loads or updates the radio status:
    DWORD
    LoadRadioStatus(const ValueMap &params);
    DWORD
    UpdateRadioStatus(
        ValueMap                 *pParams,
        const AccessPointState_t &NewState,
        bool                      Stalled);

    // Loads or updates the WMM capability:
    DWORD
    LoadWMMCapability(const ValueMap &params);
    DWORD
    UpdateWMMCapability(
        ValueMap                 *pParams,
        const AccessPointState_t &NewState,
        bool                      Stalled);

    // Loads or updates the SSID:
    DWORD
    LoadSSID(const ValueMap &params);
    DWORD
    UpdateSSID(
        ValueMap                 *pParams,
        const AccessPointState_t &NewState,
        bool                      Stalled);

    // Loads or updates the SSID-broadcast flag:
    DWORD
    LoadSsidBroadcast(const ValueMap &params);
    DWORD
    UpdateSsidBroadcast(
        ValueMap                 *pParams,
        const AccessPointState_t &NewState,
        bool                      Stalled);
    
    // Loads or updates the WEP keys:
    DWORD
    LoadWEPKeys(const ValueMap &params);
    DWORD
    UpdateWEPKeys(
        ValueMap                 *pParams,
        const AccessPointState_t &NewState,
        bool                      Stalled);

    // Loads or updates the Radius information:
    DWORD
    LoadRadiusInfo(const ValueMap &params);
    DWORD
    UpdateRadiusInfo(
        ValueMap                 *pParams,
        const AccessPointState_t &NewState,
        bool                      Stalled);

    // Loads or updates the passphrase:
    DWORD
    LoadPassphrase(const ValueMap &params);
    DWORD
    UpdatePassphrase(
        ValueMap                 *pParams,
        const AccessPointState_t &NewState,
        bool                      Stalled);

    // Loads or updates the security-mode settings:
    DWORD
    LoadSecurityModes(
        const ValueMap &Params);
    DWORD
    UpdateSecurityModes(
        ValueMap *pParams,
        bool      Stalled);

    // Requests the current web-page and parses form information:
    // Also filters WEP keys to remove spaces.
    DWORD
    ParsePageRequest(
        long SleepMillis,    // wait time before reading response
        int  FormsExpected); // number forms expected (or zero)

    // Looks up the specified form-parameter and complains if it
    // can't be found:
    ValueMapIter
    LookupParam(
        ValueMap   &Parameters,
        const char *pParamName,
        const char *pParamDesc = NULL)
    {
        return HtmlUtils::LookupParam(Parameters, pParamName, pParamDesc,
                                      m_pPageName);
    }
    ValueMapCIter
    LookupParam(
        const ValueMap &Parameters,
        const char     *pParamName,
        const char     *pParamDesc = NULL)
    {
        return HtmlUtils::LookupParam(Parameters, pParamName, pParamDesc,
                                      m_pPageName);
    }
};

// ----------------------------------------------------------------------------
//
// Constructor.
//
DLinkDWL3200Request_t::
DLinkDWL3200Request_t(
    HttpPort_t         *pHttp,
    AccessPointState_t *pState,
    float               Version)
    : m_pHttp(pHttp),
      m_pState(pState),
      m_FirmwareVersion(Version),
      m_pPageName(L""),
      m_PageMode(NumberAPAuthModes),
      m_UpdatesRequested(0),
      m_UpdatesRemaining(0),
      m_AuthMode(NumberAPAuthModes),
      m_RequestedAuthMode(NumberAPAuthModes),
      m_CipherMode(NumberAPCipherModes),
      m_RequestedCipherMode(NumberAPCipherModes)
{
    // Adjust for the firmware version.
    if (m_FirmwareVersion > 2.09)
    {
        m_AuthModeValues   = &s_AuthModeValuesV2_1[0];
        m_WepKeyIndexValue = &s_WepKeyIndexValueV2_1[0];
        m_PageRestartTime  = s_PageRestartTimeV2_1;
    }
    else
    {
        m_AuthModeValues   = &s_AuthModeValuesV1[0];
        m_WepKeyIndexValue = &s_WepKeyIndexValueV1[0];
        m_PageRestartTime  = s_PageRestartTimeV1;
    }
}

// ----------------------------------------------------------------------------
//
// Loads or updates the radio status.
//
DWORD
DLinkDWL3200Request_t::
LoadRadioStatus(
    const ValueMap &params)
{
    ValueMapCIter it;

    it = LookupParam(params, s_RadioStateName, "Radio status");
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;
    m_pState->SetRadioState(_wcsicmp(it->second, s_RadioStateOn) == 0);

    return ERROR_SUCCESS;
}

DWORD
DLinkDWL3200Request_t::
UpdateRadioStatus(
    ValueMap                 *pParams,
    const AccessPointState_t &NewState,
    bool                      Stalled)
{
    ValueMapIter it;

    if (Stalled)
    {
        LogError(TEXT("Unable to %hs the radio in page \"%ls\""),
                 NewState.IsRadioOn()? "enable" : "disable",
                 m_pPageName);
        return ERROR_INVALID_PARAMETER;
    }

    it = LookupParam(*pParams, s_RadioStateName, "Radio status");
    if (it == pParams->end())
        return ERROR_INVALID_PARAMETER;

    pParams->erase(it);
    pParams->insert(s_RadioStateName,
                    NewState.IsRadioOn()? s_RadioStateOn
                                        : s_RadioStateOff);

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Loads or updates the radio status.
//
DWORD
DLinkDWL3200Request_t::
LoadWMMCapability(
    const ValueMap &params)
{
    ValueMapCIter it;

    it = LookupParam(params, s_WMMCapName, "WMM Capability");
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;
    if (_wcsicmp(it->second, s_WMMCapEnabled) == 0)
    {
        m_pState->EnableCapability(APCapsWMM);
    }
    return ERROR_SUCCESS;
}

DWORD
DLinkDWL3200Request_t::
UpdateWMMCapability(
    ValueMap                 *pParams,
    const AccessPointState_t &NewState,
    bool                      Stalled)
{
    // Enforce turning ON only.
    assert(NewState.IsCapabilityEnabled(APCapsWMM));

    ValueMapIter it;

    if (Stalled)
    {
        LogError(TEXT("Unable to %hs WMM in page \"%ls\""),
                 NewState.IsCapabilityEnabled(APCapsWMM)? "enable" : "disable",
                 m_pPageName);
        return ERROR_INVALID_PARAMETER;
    }

    it = LookupParam(*pParams, s_WMMCapName, "WMM Capability");
    if (it == pParams->end())
        return ERROR_INVALID_PARAMETER;

    pParams->erase(it);
    pParams->insert(s_WMMCapName,
                    NewState.IsCapabilityEnabled(APCapsWMM) ? s_WMMCapEnabled
                                                            : s_WMMCapDisabled);

    return ERROR_SUCCESS;
}
// ----------------------------------------------------------------------------
//
// Loads or updates the SSID.
//
DWORD
DLinkDWL3200Request_t::
LoadSSID(const ValueMap &params)
{
    ValueMapCIter it;
    HRESULT hr;

    it = LookupParam(params, s_SsidParamName, "SSID");
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;

    hr = m_pState->SetSsid(it->second, it->second.length());
    if (FAILED(hr))
    {
        LogError(TEXT("Bad SSID value \"%ls\" in %ls"),
                &(it->second)[0], m_pPageName);
        return HRESULT_CODE(hr);
    }   

    return ERROR_SUCCESS;
}

DWORD
DLinkDWL3200Request_t::
UpdateSSID(
    ValueMap                 *pParams,
    const AccessPointState_t &NewState,
    bool                      Stalled)
{
    ValueMapIter it;

    WCHAR ssid[NewState.MaxSsidChars+1];
    HRESULT hr = NewState.GetSsid(ssid, COUNTOF(ssid));
    if (FAILED(hr))
        return HRESULT_CODE(hr);
    
    if (Stalled)
    {
        LogError(TEXT("Unable to update SSID to \"%ls\" in page \"%ls\""),
                 ssid, m_pPageName);
        return ERROR_INVALID_PARAMETER;
    }

    it = LookupParam(*pParams, s_SsidParamName, "SSID");
    if (it == pParams->end())
        return ERROR_INVALID_PARAMETER;

    pParams->erase(it);
    pParams->insert(s_SsidParamName, ssid);


    // Make sure we always updated security after updating ssid   
    m_UpdatesRemaining |= (int)m_pState->FieldMaskSecurityMode;    

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Loads or updates the SSID-broadcast flag.
//
DWORD
DLinkDWL3200Request_t::
LoadSsidBroadcast(
    const ValueMap &params)
{
    ValueMapCIter it;

    it = LookupParam(params, s_SsidBroadcastName, "Broadcast-SSID");
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;
    m_pState->SetSsidBroadcast(_wcsicmp(it->second, s_SsidBroadcastOn) == 0);

    return ERROR_SUCCESS;
}

DWORD
DLinkDWL3200Request_t::
UpdateSsidBroadcast(
    ValueMap                 *pParams,
    const AccessPointState_t &NewState,
    bool                      Stalled)
{
    ValueMapIter it;

    if (Stalled)
    {
        LogError(TEXT("Unable to %hs broadcast-SSID in page \"%ls\""),
                 NewState.IsSsidBroadcast()? "enable" : "disable",
                 m_pPageName);
        return ERROR_INVALID_PARAMETER;
    }

    it = LookupParam(*pParams, s_SsidBroadcastName, "Broadcast-SSID");
    if (it == pParams->end())
        return ERROR_INVALID_PARAMETER;

    pParams->erase(it);
    pParams->insert(s_SsidBroadcastName,
                    NewState.IsSsidBroadcast()? s_SsidBroadcastOn
                                              : s_SsidBroadcastOff);

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Loads or updates the WEP keys.
//
DWORD
DLinkDWL3200Request_t::
LoadWEPKeys(const ValueMap &params)
{
    ValueMapCIter it;
    HRESULT hr;

    WEPKeys_t keys = m_pState->GetWEPKeys();
    BYTE      keyIndex = keys.m_KeyIndex;

    it = LookupParam(params, s_WepKeyIndexName, "WEP key index");
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;
    for (int kx = 0 ;; ++kx)
    {
        if (kx < 4)
        {
            if (_wcsicmp(it->second, m_WepKeyIndexValue[kx]) == 0)
            {
                keyIndex = static_cast<BYTE>(kx);
                break;
            }
        }
        else
        {
            LogError(TEXT("Invalid WEP key index %hs = \"%ls\" in \"%ls\""),
                     s_WepKeyIndexName, &(it->second)[0], m_pPageName);
            return ERROR_INVALID_PARAMETER;
        }
    }

    it = LookupParam(params, s_WepKeyTypeName, "WEP key type");
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;
    bool wepKeyIsAscii = (_wcsicmp(it->second, s_WepKeyTypeAscii) == 0);

    for (int kx = 0 ; 4 > kx ; ++kx)
    {
        char name[20];
        hr = StringCchPrintfA(name, sizeof(name), "%s%d", s_WepKeyName, kx+1);
        if (FAILED(hr))
            return HRESULT_CODE(hr);

        it = LookupParam(params, name, "WEP key");
        if (it == params.end())
            return ERROR_INVALID_PARAMETER;
        const ce::wstring &value = &(it->second)[0];

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
            LogError(TEXT("Invalid %hs WEP key %hs = \"%ls\" in \"%ls\""),
                     wepKeyIsAscii? "ASCII" : "HEX",
                     name, &value[0], m_pPageName);
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
                LogError(TEXT("Invalid HEX WEP key %hs = \"%ls\""),
                         name, &value[0]);
                return HRESULT_CODE(hr);
            }
        }
    }

    keys.m_KeyIndex = keyIndex;
    m_pState->SetWEPKeys(keys);
    return ERROR_SUCCESS;
}

DWORD
DLinkDWL3200Request_t::
UpdateWEPKeys(
    ValueMap                 *pParams,
    const AccessPointState_t &NewState,
    bool                      Stalled)
{
    const WEPKeys_t &oldKeys = m_pState->GetWEPKeys();
    const WEPKeys_t &newKeys = NewState.GetWEPKeys();

    char nameBuff[32];
    int  nameChars = COUNTOF(nameBuff);

    WCHAR dataBuff[256];
    int   dataChars = COUNTOF(dataBuff);

    ValueMapIter it;
    HRESULT hr;

    int keyIndex = static_cast<int>(newKeys.m_KeyIndex);
    if (Stalled)
    {
        newKeys.ToString(keyIndex, dataBuff, dataChars);
        LogError(TEXT("Unable to update WEP key #%d in page \"%ls\""),
                 keyIndex+1, m_pPageName);
        return ERROR_INVALID_PARAMETER;
    }

    // Initialize the key index.
    if (0 <= keyIndex && keyIndex < 4)
    {
        it = LookupParam(*pParams, s_WepKeyIndexName, "WEP key index");
        if (it == pParams->end())
            return ERROR_INVALID_PARAMETER;
        pParams->erase(it);
        pParams->insert(s_WepKeyIndexName, m_WepKeyIndexValue[keyIndex]);
    }

    for (int kx = 0 ; 4 > kx ; ++kx)
    {
        // Ignore empty, invalid or identical keys.
        int keySize = static_cast<int>(newKeys.m_Size[kx]);
        if (!newKeys.ValidKeySize(keySize)
         || 0 == memcmp(newKeys.m_Material[kx],
                        oldKeys.m_Material[kx], keySize))
            continue;

        // If necessary, turn on WEP encryption.
        // The 2.10 firmware version returns inconsistent security
        // modes. Both Open and Shared are selected. Therefore, we
        // and the web-server can differ on their interpretation.
        // To avoid that we always send the correct modes, even
        // if they're not required.
        //if (APAuthShared != m_AuthMode || APCipherWEP != m_CipherMode)
        //{
            m_AuthMode = APAuthShared;
            m_CipherMode = APCipherWEP;
            m_UpdatesRequested |= (int)m_pState->FieldMaskSecurityMode;
            m_UpdatesRemaining |= (int)m_pState->FieldMaskSecurityMode;
        //}

        // Update the key-encoding param.
        it = LookupParam(*pParams, s_WepKeyTypeName, "WEP key type");
        if (it == pParams->end())
            return ERROR_INVALID_PARAMETER;
        pParams->erase(it);
        pParams->insert(s_WepKeyTypeName, s_WepKeyTypeHex);

        // Update the key-size param.
        const WCHAR *keySizeParam;
        switch (keySize)
        {
            case 5:  keySizeParam = s_WepKeyLength40Bit;  break;
            case 13: keySizeParam = s_WepKeyLength104Bit; break;
            case 16: keySizeParam = s_WepKeyLength128Bit; break;
            default:
                LogError(TEXT("Can't update %d-bit WEP keys")
                         TEXT(" - only 40, 104 and 128"),
                         keySize * 8);
                return ERROR_INVALID_PARAMETER;
        }

        it = LookupParam(*pParams, s_WepKeyLengthName, "WEP key length");
        if (it == pParams->end())
            return ERROR_INVALID_PARAMETER;
        pParams->erase(it);
        pParams->insert(s_WepKeyLengthName, keySizeParam);

        // Update the key index.
        it = LookupParam(*pParams, s_WepKeyIndexName, "WEP key index");
        if (it == pParams->end())
            return ERROR_INVALID_PARAMETER;
        pParams->erase(it);
        pParams->insert(s_WepKeyIndexName, m_WepKeyIndexValue[kx]);

        // Update the key itself.
        static const WCHAR *hexmap = L"0123456789abcdef";
        const BYTE *keymat = &newKeys.m_Material[kx][0];
        const BYTE *keyend = &newKeys.m_Material[kx][keySize];
        WCHAR *pBuffer = dataBuff;
        for (; keymat < keyend ; ++keymat)
        {
            *(pBuffer++) = hexmap[(*keymat >> 4) & 0xf];
            *(pBuffer++) = hexmap[ *keymat       & 0xf];
        }
        *pBuffer = L'\0';

        hr = StringCchPrintfA(nameBuff, nameChars, "%s%d", s_WepKeyName, kx+1);
        if (FAILED(hr))
            return HRESULT_CODE(hr);

        it = LookupParam(*pParams, nameBuff, "WEP key");
        if (it == pParams->end())
            return ERROR_INVALID_PARAMETER;
        pParams->erase(it);
        pParams->insert(nameBuff, dataBuff);

        // We can only update one key per pass.
        break;
    }

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Loads or updates the Radius information.
//
DWORD
DLinkDWL3200Request_t::
LoadRadiusInfo(const ValueMap &params)
{
    ValueMapCIter it;
    HRESULT hr;

    it = LookupParam(params, s_RadiusServerName, "Radius server");
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;

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

    m_pState->SetRadiusServer(server);

    it = LookupParam(params, s_RadiusPortName, "Radius port");
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;

    long port = wcstol(it->second, NULL, 10);
    if (0 > port || port >= 32*1024)
    {
        LogError(TEXT("Invalid Radius port \"%ls\""),
                &it->second[0]);
        return ERROR_INVALID_PARAMETER;
    }

    m_pState->SetRadiusPort(port);

    it = LookupParam(params, s_RadiusSecretName, "Radius key");
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;

    hr = m_pState->SetRadiusSecret(it->second, it->second.length());
    if (FAILED(hr))
    {
        LogError(TEXT("Bad radius-key value \"%ls\" in %ls"),
                &(it->second)[0], m_pPageName);
        return HRESULT_CODE(hr);
    }

    return ERROR_SUCCESS;
}

DWORD
DLinkDWL3200Request_t::
UpdateRadiusInfo(
    ValueMap                 *pParams,
    const AccessPointState_t &NewState,
    bool                      Stalled)
{
    HRESULT hr;
    ValueMapIter it;
    WCHAR server[24];
    WCHAR port  [20];
    WCHAR secret[NewState.MaxRadiusSecretChars+1];
    
    in_addr inaddr;
    inaddr.s_addr = NewState.GetRadiusServer();
    hr = StringCchPrintfW(server, COUNTOF(server), L"%u.%u.%u.%u",
                          inaddr.s_net, inaddr.s_host,
                          inaddr.s_lh, inaddr.s_impno);
    if (FAILED(hr))
        return HRESULT_CODE(hr);
    
    hr = StringCchPrintfW(port, COUNTOF(port), L"%d",
                          NewState.GetRadiusPort());
    if (FAILED(hr))
        return HRESULT_CODE(hr);

    hr = NewState.GetRadiusSecret(secret, COUNTOF(secret));
    if (FAILED(hr))
        return HRESULT_CODE(hr);

    if (Stalled)
    {
        LogError(TEXT("Unable to update Radius to")
                 TEXT(" \"%ls,%ls,%ls\" in page \"%ls\""),
                 server, port, secret,
                 m_pPageName);
        return ERROR_INVALID_PARAMETER;
    }

    // If necessary, turn on EAP encryption.
    if (APAuthWEP_802_1X != m_AuthMode
     && APAuthWPA        != m_AuthMode
     && APAuthWPA2       != m_AuthMode)
    {
        m_AuthMode = APAuthWPA;
        if (m_CipherMode != APCipherAES)
            m_CipherMode = APCipherTKIP;
        m_UpdatesRequested |= (int)m_pState->FieldMaskSecurityMode;
        m_UpdatesRemaining |= (int)m_pState->FieldMaskSecurityMode;
    }

    it = LookupParam(*pParams, s_RadiusServerName, "Radius server");
    if (it == pParams->end())
        return ERROR_INVALID_PARAMETER;
    pParams->erase(it);
    pParams->insert(s_RadiusServerName, server);

    it = LookupParam(*pParams, s_RadiusPortName, "Radius server port");
    if (it == pParams->end())
        return ERROR_INVALID_PARAMETER;
    pParams->erase(it);
    pParams->insert(s_RadiusPortName, port);

    it = LookupParam(*pParams, s_RadiusSecretName, "Radius secret key");
    if (it == pParams->end())
        return ERROR_INVALID_PARAMETER;
    pParams->erase(it);
    pParams->insert(s_RadiusSecretName, secret);

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Loads or updates the passphrase.
//
DWORD
DLinkDWL3200Request_t::
LoadPassphrase(const ValueMap &params)
{
    ValueMapCIter it;
    HRESULT hr;

    it = LookupParam(params, s_PassphraseName, "Passphrase");
    if (it == params.end())
        return ERROR_INVALID_PARAMETER;

    hr = m_pState->SetPassphrase(it->second, it->second.length());
    if (FAILED(hr))
    {
        LogError(TEXT("Bad passphrase value \"%ls\" in %ls"),
                &(it->second)[0], m_pPageName);
        return HRESULT_CODE(hr);
    }

    return ERROR_SUCCESS;
}

DWORD
DLinkDWL3200Request_t::
UpdatePassphrase(
    ValueMap                 *pParams,
    const AccessPointState_t &NewState,
    bool                      Stalled)
{
    ValueMapIter it;
    
    WCHAR passphrase[NewState.MaxPassphraseChars+1];
    HRESULT hr = NewState.GetPassphrase(passphrase, COUNTOF(passphrase));
    if (FAILED(hr))
        return HRESULT_CODE(hr);
    
    if (Stalled)
    {
        LogError(TEXT("Unable to update Passphrase to \"%ls\" in page \"%ls\""),
                 passphrase, m_pPageName);
        return ERROR_INVALID_PARAMETER;
    }

    // If necessary, turn on PSK authentication.
    if (APAuthWPA_PSK  != m_AuthMode
     && APAuthWPA2_PSK != m_AuthMode)
    {
        m_AuthMode = APAuthWPA_PSK;
        if (m_CipherMode != APCipherAES)
            m_CipherMode = APCipherTKIP;
        m_UpdatesRequested |= (int)m_pState->FieldMaskSecurityMode;
        m_UpdatesRemaining |= (int)m_pState->FieldMaskSecurityMode;
    }

    it = LookupParam(*pParams, s_PassphraseName, "Passphrase");
    if (it == pParams->end())
        return ERROR_INVALID_PARAMETER;

    pParams->erase(it);
    pParams->insert(s_PassphraseName, passphrase);

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Loads or updates the security-mode settings.
//
DWORD
DLinkDWL3200Request_t::
LoadSecurityModes(
    const ValueMap &Params)
{
    ValueMapCIter it;

    // Get the authentication mode.
    APAuthMode_e authMode = m_pState->GetAuthMode();
    ce::string statusNumber;
    const char *oldCursor, *pCursor;
    oldCursor = m_WebPage.get_buffer();
      pCursor = HtmlUtils::FindString(oldCursor, s_AuthModeIndex,
                                      "=", ";", &statusNumber);
    if (oldCursor == pCursor || 0 == statusNumber.length())
    {
        LogError(TEXT("Can't find \"%hs\" security-mode index in %ls"),
                 s_AuthModeIndex, m_pPageName);
        return ERROR_INVALID_PARAMETER;
    }

    char *valuEnd;
    int authIndex = -1;
    unsigned long lvalu = strtoul(statusNumber.get_buffer(), &valuEnd, 10);
    if (NULL != valuEnd && '\0' == valuEnd[0] && NumberAuthModeIndexes > lvalu)
    {
        authIndex = (int)lvalu;
    }
    else
    {
        LogError(TEXT("Bad \"%hs\" security-mode index \"%hs\" in %ls"),
                 s_AuthModeIndex, &statusNumber[0], m_pPageName);
        return ERROR_INVALID_PARAMETER;
    }

    switch (authIndex)
    {
        default:
            authMode = APAuthOpen;
            break;
        case (int)AuthModeIndexShared:
            authMode = APAuthShared;
            break;
        case (int)AuthModeIndexWpaEap:
        case (int)AuthModeIndexWpaAutoEap:
            authMode = APAuthWPA;
            break;
        case (int)AuthModeIndexWpaPsk:
        case (int)AuthModeIndexWpaAutoPsk:
            authMode = APAuthWPA_PSK;
            break;
        case (int)AuthModeIndexWpa2Eap:
            authMode = APAuthWPA2;
            break;
        case (int)AuthModeIndexWpa2Psk:
            authMode = APAuthWPA2_PSK;
            break;
    }

    // Get the cipher mode.
    APCipherMode_e cipherMode = m_pState->GetCipherMode();
    if (APAuthOpen == authMode || APAuthShared == authMode)
    {
        if (APAuthOpen == m_PageMode || APAuthShared == m_PageMode)
        {
            it = LookupParam(Params, s_WepCipherName, "WEP encryption mode");
            if (it == Params.end())
                return ERROR_INVALID_PARAMETER;
            if (0 == _wcsicmp(it->second, s_WepCipherOn))
                 cipherMode = APCipherWEP;
            else cipherMode = APCipherClearText;
        }
    }
    else
    {
        if (APAuthOpen != m_PageMode && APAuthShared != m_PageMode)
        {
            cipherMode = APCipherTKIP;
            it = LookupParam(Params, s_WpaCipherName, "WPA encryption mode");
            if (it != Params.end())
            {
                if (0 == _wcsicmp(it->second, s_WpaCipherAes))
                    cipherMode = APCipherAES;
            }
        }
    }

    m_pState->SetAuthMode(authMode);
    m_pState->SetCipherMode(cipherMode);
    return ERROR_SUCCESS;
}

DWORD
DLinkDWL3200Request_t::
UpdateSecurityModes(
    ValueMap *pParams,
    bool      Stalled)
{
    ValueMapIter it;

    if (Stalled)
    {
        LogError(TEXT("Unable to update security modes to")
                 TEXT(" auth=%s and cipher=%s in page \"%ls\""),
                 APAuthMode2String(m_AuthMode),
                 APCipherMode2String(m_CipherMode),
                 m_pPageName);
        return ERROR_INVALID_PARAMETER;
    }

    // Validate the new security modes.
    if (!ValidateSecurityModes(m_AuthMode, m_CipherMode,
                               m_pState->GetCapabilitiesEnabled()))
    {
        TCHAR vendorName [m_pState->MaxVendorNameChars +1];
        TCHAR modelNumber[m_pState->MaxModelNumberChars+1];
        m_pState->GetVendorName (vendorName,  COUNTOF(vendorName));
        m_pState->GetModelNumber(modelNumber, COUNTOF(modelNumber));
        LogError(TEXT("%s %s APs cannot support")
                 TEXT(" authentication \"%s\" and/or cipher \"%s\""),
                 vendorName,
                 modelNumber,
                 APAuthMode2String(m_AuthMode),
                 APCipherMode2String(m_CipherMode));
        return ERROR_INVALID_PARAMETER;
    }

    const WCHAR *pAuthModeValue  = NULL;
    const WCHAR *pWepCipherValue = NULL;
    const WCHAR *pWpaCipherValue = NULL;
    
    pWpaCipherValue = (APCipherTKIP == m_CipherMode)? s_WpaCipherTkip
                    : (APCipherAES  == m_CipherMode)? s_WpaCipherAes
                    :                                 s_WpaCipherAuto;

    switch (m_AuthMode)
    {
        default:
            pAuthModeValue  = m_AuthModeValues[AuthModeIndexOpen];
            pWepCipherValue = (APCipherWEP == m_CipherMode)? s_WepCipherOn
                                                           : s_WepCipherOff;
            pWpaCipherValue = NULL;
            break;

        case APAuthShared:
            pAuthModeValue  = m_AuthModeValues[AuthModeIndexShared];
            pWepCipherValue = (APCipherWEP == m_CipherMode)? s_WepCipherOn
                                                           : s_WepCipherOff;
            pWpaCipherValue = NULL;
            break;

        case APAuthWPA:
            pAuthModeValue = m_AuthModeValues[AuthModeIndexWpaEap];
            break;

        case APAuthWPA2:
            pAuthModeValue = m_AuthModeValues[AuthModeIndexWpa2Eap];
            break;

        case APAuthWPA_PSK:
            pAuthModeValue = m_AuthModeValues[AuthModeIndexWpaPsk];
            break;

        case APAuthWPA2_PSK:
            pAuthModeValue = m_AuthModeValues[AuthModeIndexWpa2Psk];
            break;
    }

    if (pAuthModeValue)
    {
        it = LookupParam(*pParams, s_AuthModeName, "Authentication mode");
        if (it != pParams->end())
            pParams->erase(it);
        pParams->insert(s_AuthModeName, pAuthModeValue);
    }

    if (pWepCipherValue)
    {
        it = LookupParam(*pParams, s_WepCipherName, "WEP encryption mode");
        if (it != pParams->end())
            pParams->erase(it);
        pParams->insert(s_WepCipherName, pWepCipherValue);
    }

    if (pWpaCipherValue)
    {
        it = LookupParam(*pParams, s_WpaCipherName, "WPA encryption mode");
        if (it != pParams->end())
            pParams->erase(it);
        pParams->insert(s_WpaCipherName, pWpaCipherValue);
    }

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Requests the current web-page and parses form information.
// Also filters WEP keys to remove spaces.
// This is required because the AP firmware formats each byte of the WEP
// key individualy. Hence, a key of "23.01.23..." shows up in the web-page
// form as "23 123". When we encode that to send an update it changes to
// "23%20123" which isn't interpretted properly by the AP.
//
DWORD
DLinkDWL3200Request_t::
ParsePageRequest(
    long SleepMillis,   // wait time before reading response
    int  FormsExpected) // number forms expected (or zero)
{
    HRESULT hr;
    DWORD result = HtmlAPTypeController_t::ParsePageRequest(m_pHttp,
                                                            m_pPageName,
                                                            SleepMillis,
                                                           &m_WebPage,
                                                           &m_PageForms,
                                                            FormsExpected);
    while (ERROR_SUCCESS == result)
    {
        ValueMap &params = m_PageForms.GetFields(0);
        ValueMapIter it;

        // Only do this if the key type is HEX.
        it = LookupParam(params, s_WepKeyTypeName);
        if (it == params.end())
            break;
        if (_wcsicmp(it->second, s_WepKeyTypeAscii) == 0)
            break;

        for (int kx = 0 ; kx < 4 ; ++kx)
        {
            char key[20];
            hr = StringCchPrintfA(key, sizeof(key), "%s%d", s_WepKeyName, kx+1);
            if (FAILED(hr))
                continue;

            it = LookupParam(params, key);
            if (it == params.end())
                continue;

            ce::wstring value = it->second;
                  WCHAR *pValuPtr = &value[0];
            const WCHAR *pValuEnd = &value[value.length()];
            for (; pValuPtr < pValuEnd ; ++pValuPtr)
            {
                if (pValuPtr[0] == L' ')
                    pValuPtr[0] =  L'0';
            }

            params.erase(it);
            params.insert(key, value);
        }
        break;
    }
    return result;
}

// ----------------------------------------------------------------------------
//
// Loads settings from the PerformancePage.
//
DWORD
DLinkDWL3200Request_t::
LoadPerformancePage(void)
{
    DWORD result;

    // Load and parse the wireless-lan performance page.
    m_pPageName = s_WebPages[0].name;
    m_PageMode  = s_WebPages[0].mode;
    result = ParsePageRequest(0, 1);
    if (ERROR_SUCCESS != result)
        return result;

    // Extract cookies.
    GetWebPageCookies(m_pHttp, m_WebPage);

    // Get the WMM capability.
    result = LoadWMMCapability(m_PageForms.GetFields(0));
    if (ERROR_SUCCESS != result)
        return result;

    // Get the radio state.
    if (m_FirmwareVersion <= 2.09)
    {
        result = LoadRadioStatus(m_PageForms.GetFields(0));
    }
    return result;
}

// ----------------------------------------------------------------------------
//
// Loads settings from the WepSettingsPage.
//
DWORD
DLinkDWL3200Request_t::
LoadWepSettingsPage(void)
{
    DWORD result;

    // Load and parse the wireless-lan page.
    m_pPageName = s_WebPages[1].name;
    m_PageMode  = s_WebPages[1].mode;
    result = ParsePageRequest(0, 1);
    if (ERROR_SUCCESS != result)
        return result;
    const ValueMap &params = m_PageForms.GetFields(0);

    // Extract cookies.
    GetWebPageCookies(m_pHttp, m_WebPage);

    // Get the radio state.
    if (m_FirmwareVersion > 2.09)
    {
        result = LoadRadioStatus(params);
        if (ERROR_SUCCESS != result)
            return result;
    }

    // Get the security modes.
    result = LoadSecurityModes(params);
    if (ERROR_SUCCESS != result)
        return result;

    // Get the SSID.
    result = LoadSSID(params);
    if (ERROR_SUCCESS != result)
        return result;

    // Get the broadcast-SSID flag.
    result = LoadSsidBroadcast(params);
    if (ERROR_SUCCESS != result)
        return result;
        
    // Get the WEP keys.
    result = LoadWEPKeys(params);
    if (ERROR_SUCCESS != result)
        return result;

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Loads settings from the WpaSettingsPage.
//
DWORD
DLinkDWL3200Request_t::
LoadWpaSettingsPage(void)
{
    DWORD result;

    // Load and parse the WPA-settings page.
    m_pPageName = s_WebPages[2].name;
    m_PageMode  = s_WebPages[2].mode;
    result = ParsePageRequest(0, 1);
    if (ERROR_SUCCESS != result)
        return result;
    const ValueMap &params = m_PageForms.GetFields(0);

    // Extract cookies.
    GetWebPageCookies(m_pHttp, m_WebPage);

    // Get the security modes.
    result = LoadSecurityModes(params);
    if (ERROR_SUCCESS != result)
        return result;

    // Get the Radius information.
    result = LoadRadiusInfo(params);
    if (ERROR_SUCCESS != result)
        return result;

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Loads settings from the PskSettingsPage.
//
DWORD
DLinkDWL3200Request_t::
LoadPskSettingsPage(void)
{
    DWORD result;

    // Load and parse the wireless-lan page.
    m_pPageName = s_WebPages[3].name;
    m_PageMode  = s_WebPages[3].mode;
    result = ParsePageRequest(0, 1);
    if (ERROR_SUCCESS != result)
        return result;
    const ValueMap &params = m_PageForms.GetFields(0);

    // Extract cookies.
    GetWebPageCookies(m_pHttp, m_WebPage);

    // Get the security modes.
    result = LoadSecurityModes(params);
    if (ERROR_SUCCESS != result)
        return result;

    // Get the passphrase.
    result = LoadPassphrase(params);
    if (ERROR_SUCCESS != result)
        return result;

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Updates the configuration settings:
//
DWORD
DLinkDWL3200Request_t::
UpdateSettings(const AccessPointState_t &NewState)
{
    DWORD result = NO_ERROR;

    const int OldStatesToCompare = 3;
    AccessPointState_t oldStates[s_NumberWebPages][OldStatesToCompare];

    int firstPageIX = 0;

    // Initialize the fields-to-be-changed flags.
    m_UpdatesRequested = NewState.GetFieldFlags();

    // Store the requested or current values of the fields which may need
    // to be altered in the process of changing other fields so we can put
    // them back the way they belong.
    if (m_UpdatesRequested & (int)m_pState->FieldMaskSecurityMode)
    {
        m_RequestedAuthMode   = NewState.GetAuthMode();
        m_RequestedCipherMode = NewState.GetCipherMode();
    }
    else
    {
        m_RequestedAuthMode   = m_pState->GetAuthMode();
        m_RequestedCipherMode = m_pState->GetCipherMode();
    }

    bool updated = true;
    for (int tries = 0 ; updated ; ++tries)
    {
        updated = false;

        // Reset the possibly-modified fields.
        m_AuthMode   = m_RequestedAuthMode;
        m_CipherMode = m_RequestedCipherMode;

        // For each web-page...
        for (int pageIX = firstPageIX ; pageIX < s_NumberWebPages ; ++pageIX)
        {
            // Load the page's current settings and get the bit-map
            // of data-elements in the page.
            int pageFields;
            switch (pageIX)
            {
                // Performance page:
                case 0:
                    result = LoadPerformancePage();
                    pageFields = (int)m_pState->FieldMaskCapabilities;
                    if (m_FirmwareVersion <= 2.09)
                        pageFields |= (int)m_pState->FieldMaskRadioState;
                    break;

                // WEP settings page:
                case 1:
                    result = LoadWepSettingsPage();
                    pageFields = ((int)m_pState->FieldMaskSecurityMode
                                 |(int)m_pState->FieldMaskSsid
                                 |(int)m_pState->FieldMaskSsidBroadcast
                                 |(int)m_pState->FieldMaskWEPKeys);
                    if (m_FirmwareVersion > 2.09)
                        pageFields |= (int)m_pState->FieldMaskRadioState;
                    break;

                // WPA (Radius) settings page:
                case 2:
                    result = LoadWpaSettingsPage();
                    pageFields = ((int)m_pState->FieldMaskSecurityMode
                                 |(int)m_pState->FieldMaskRadius);
                    break;

                // PSK settings page:
                case 3:
                    result = LoadPskSettingsPage();
                    pageFields = ((int)m_pState->FieldMaskSecurityMode
                                 |(int)m_pState->FieldMaskPassphrase);
                    break;
            }
            if (ERROR_SUCCESS != result)
                return result;

            // Store the page contents for checking for stalled updates.
            oldStates[pageIX][tries % OldStatesToCompare] = *m_pState;

            // Determine which of this page's fields still need to be updated.
            // If they're all done, go on to the next page.
            pageFields &= m_UpdatesRequested;
            m_UpdatesRemaining = m_pState->CompareFields(pageFields, NewState);

            if (0 == m_UpdatesRemaining)
                continue;

            // If the security-modes are the only remaining changes and
            // this page can't be used to set the new modes, just go on
            // to the next page.
            if (m_UpdatesRemaining == (int)m_pState->FieldMaskSecurityMode)
            {
                switch (m_PageMode)
                {
                    default: continue;
                    case APAuthShared:
                        if (APAuthOpen   != m_AuthMode
                         && APAuthShared != m_AuthMode)
                            continue;
                        break;
                    case APAuthWPA:
                        if (APAuthWPA  != m_AuthMode
                         && APAuthWPA2 != m_AuthMode)
                            continue;
                        break;
                    case APAuthWPA_PSK:
                        if (APAuthWPA_PSK  != m_AuthMode
                         && APAuthWPA2_PSK != m_AuthMode)
                            continue;
                        break;
                }
            }

            ValueMap params = m_PageForms.GetFields(0);

            LogDebug(TEXT("[AC] Updates remaining")
                     TEXT(" at pass %d in page %s = 0x%X"),
                     tries+1, m_pPageName, m_UpdatesRemaining);

            LogDebug(TEXT("Form (before updates):"));
            LogDebug(TEXT("  method = %ls"), m_PageForms.GetMethod(0));
            LogDebug(TEXT("  action = %ls"), m_PageForms.GetAction(0));
            LogDebug(TEXT("  %d form elements:"), params.size());
            for (ValueMapIter it = params.begin() ; it != params.end() ; ++it)
            {
                LogDebug(TEXT("    %hs = \"%ls\""), &(it->first)[0],
                                                    &(it->second)[0]);
            }

            // Make sure we're not endlessly looping with no progress.
            // We do this by comparing the current AP configuration against
            // the AP's state after the last few page-updates. It would seem
            // we could do as well by just comparing against the last state,
            // but that wouldn't detect "cycles" where, for example, changing
            // WEP key #1 causes key #2 to revert to a previous value and
            // going back to fix key #2 causes key #1 to revert and so on.
            // (Believe it or not, this actually happens.)
            bool stalled = false;
            if (tries > 0)
            {
                int first = tries % OldStatesToCompare;
                int prev = first;
                for (int tx = 0 ; tx < tries ; ++tx)
                {
                    if (prev == 0)
                         prev = OldStatesToCompare - 1;
                    else prev--;
                    if (prev == first)
                        break;
                    if (memcmp(m_pState, &(oldStates[pageIX][prev]),
                               sizeof(AccessPointState_t)) == 0)
                    {
                        stalled = true;
                        break;
                    }
                }
            }

            // Radio-state:
            if (m_UpdatesRemaining & (int)m_pState->FieldMaskRadioState)
            {
                result = UpdateRadioStatus(&params, NewState, stalled);
                if (ERROR_SUCCESS != result)
                    return result;
            }

            // WMM Capability:
            if (m_UpdatesRemaining & (int)m_pState->FieldMaskCapabilities)
            {
                result = UpdateWMMCapability(&params, NewState, stalled);
                if (ERROR_SUCCESS != result)
                    return result;
            }

            // SSID:
            if (m_UpdatesRemaining & (int)m_pState->FieldMaskSsid)
            {
                result = UpdateSSID(&params, NewState, stalled);
                if (ERROR_SUCCESS != result)
                    return result;
            }

            // Broadcast-SSID flag:
            if (m_UpdatesRemaining & (int)m_pState->FieldMaskSsidBroadcast)
            {
                result = UpdateSsidBroadcast(&params, NewState, stalled);
                if (ERROR_SUCCESS != result)
                    return result;
            }
            
            // WEP keys:
            if (m_UpdatesRemaining & (int)m_pState->FieldMaskWEPKeys)
            {
                result = UpdateWEPKeys(&params, NewState, stalled);
                if (ERROR_SUCCESS != result)
                    return result;
            }

            // Radius server information:
            if (m_UpdatesRemaining & (int)m_pState->FieldMaskRadius)
            {
                result = UpdateRadiusInfo(&params, NewState, stalled);
                if (ERROR_SUCCESS != result)
                    return result;
            }

            // Passphrase:
            if (m_UpdatesRemaining & (int)m_pState->FieldMaskPassphrase)
            {
                result = UpdatePassphrase(&params, NewState, stalled);
                if (ERROR_SUCCESS != result)
                    return result;
            }

            // Security modes:
            if (m_UpdatesRemaining & (int)m_pState->FieldMaskSecurityMode)
            {
                result = UpdateSecurityModes(&params, stalled);
                if (ERROR_SUCCESS != result)
                    return result;
            }

            // Update the configuration.
            result = m_pHttp->SendUpdateRequest(m_pPageName,
                                                m_PageForms.GetMethod(0),
                                                m_PageForms.GetAction(0),
                                                params,
                                                m_PageRestartTime,
                                               &m_WebPage,
                                                s_UpdateSucceeded);
            if (ERROR_SUCCESS != result)
                return result;

            updated = true;
        }
    }

    return ERROR_SUCCESS;
}

/* ======================== DLinkDWL3200Controller_t ======================= */

// ----------------------------------------------------------------------------
//
// Constructor.
//
DLinkDWL3200Controller_t::
DLinkDWL3200Controller_t(
    HttpPort_t         *pHttp,
    AccessPointState_t *pState,
    const ce::string   &WebPage)
    : HtmlAPTypeController_t(pHttp, pState),
      m_FirmwareVersion(2.1f) // Assume at least Version 2.10 firmware
{
    // Extract cookies from the web-page.
    GetWebPageCookies(pHttp, WebPage);
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
DLinkDWL3200Controller_t::
~DLinkDWL3200Controller_t(void)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Initializes the device-controller.
//
DWORD
DLinkDWL3200Controller_t::
InitializeDevice(void)
{
    ce::string webPage;
    const char *pValue;

    // Load the device-info page.
    DWORD result = m_pHttp->SendPageRequest(s_DeviceInfoPage, 0L, &webPage);
    if (ERROR_SUCCESS != result)
        return result;

    // Extract cookies from the web-page.
    GetWebPageCookies(m_pHttp, webPage);

    // Load the firmware version.
    static const char FirmwareHeader[] = "Firmware Version:";
    static size_t     FirmwareHeaderChars = strlen(FirmwareHeader);
    pValue = strstr(webPage.get_buffer(), FirmwareHeader);
    if (pValue)
    {
        pValue += FirmwareHeaderChars;
        while (pValue[0] && !isdigit(static_cast<unsigned char>(pValue[0])))
               pValue++;
        if (isdigit(static_cast<unsigned char>(pValue[0])))
        {
            char *valEnd;
            double value = strtod(pValue, &valEnd);
            if (value <= 0 || valEnd == NULL)
            {
                LogWarn(TEXT("[AC] Can't find firmware version in %s/%ls"),
                        m_pHttp->GetServerHost(), s_DeviceInfoPage);
                LogWarn(TEXT("[AC] Defaulting to version %0.2f"),
                        m_FirmwareVersion);
            }
            else
            {
                m_FirmwareVersion = (float)value;
                LogDebug(TEXT("[AC] Firmware version = %0.2f"),
                         m_FirmwareVersion);
            }
        }
    }

    // Find and load the BSSID (MAC Address).
    MACAddr_t mac; bool macFound = false;
    static const char MACAddressHeader[] = "MAC Address:";
    static size_t     MACAddressHeaderChars = strlen(MACAddressHeader);
    pValue = strstr(webPage.get_buffer(), MACAddressHeader);
    if (pValue)
    {
        pValue += MACAddressHeaderChars;
        for (; pValue[0] ; ++pValue)
        {
            if (isxdigit(static_cast<unsigned char>(pValue[0])))
            {
                HRESULT hr = mac.FromString(pValue);
                if (SUCCEEDED(hr))
                {
                    macFound = true;
                    break;
                }
            }
        }
    }
    if (!macFound)
    {
        LogError(TEXT("MAC Address missing from \"%ls\""),
                 s_DeviceInfoPage);
        return ERROR_INVALID_PARAMETER;
    }
    m_pState->SetBSsid(mac);

    // Fill in the model's capabilities.
    m_pState->SetVendorName(L"D-Link");
    m_pState->SetModelNumber(L"DWL-3200");
    int caps = (int)(APCapsWPA | APCapsWPA_PSK | APCapsWPA_AES);
    // Assume all firmware revs support WMM.
    if (m_FirmwareVersion > 2.09)
        caps |= (int)(APCapsWPA2 | APCapsWPA2_PSK | APCapsWPA2_TKIP);

    int dynamicCaps = int(APCapsWMM);
    m_pState->SetCapabilitiesImplemented(caps | dynamicCaps);
    m_pState->SetCapabilitiesEnabled(caps);

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Resets the device once it gets into an unknown state.
//
DWORD
DLinkDWL3200Controller_t::
ResetDevice(void)
{
    // Load the reset/restore page.
    ce::string webPage;
    HtmlForms_t pageForms;
    DWORD result = ParsePageRequest(m_pHttp, s_ResetPage, 0,
                                   &webPage, &pageForms, 0);
    if (ERROR_SUCCESS != result)
        return result;

    // Extract cookies from the web-page.
    GetWebPageCookies(m_pHttp, webPage);

    // Find the reset form.
    int formIx;
    for (formIx = 0 ; formIx < pageForms.Size() ; ++formIx)
        if (wcsstr(pageForms.GetAction(formIx), s_ResetFormName))
            break;
    if (formIx >= pageForms.Size())
    {
        LogError(TEXT("Reset form missing from \"%ls\""), s_ResetPage);
        return ERROR_INVALID_PARAMETER;
    }

    // Send the reset form.
    result = m_pHttp->SendUpdateRequest(s_ResetPage,
                                        pageForms.GetMethod(formIx),
                                        pageForms.GetAction(formIx),
                                        pageForms.GetFields(formIx),
                                      ((m_FirmwareVersion > 2.09)
                                          ? s_PageRestartTimeV2_1
                                          : s_PageRestartTimeV1),
                                       &webPage,
                                        s_UpdateSucceeded);
    if (ERROR_SUCCESS != result)
        return result;

    GetWebPageCookies(m_pHttp, webPage);
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Gets the configuration settings from the device.
//
DWORD
DLinkDWL3200Controller_t::
LoadSettings(void)
{
    DWORD result;
    DLinkDWL3200Request_t request(m_pHttp, m_pState, m_FirmwareVersion);

    m_pState->SetAuthMode(NumberAPAuthModes);
    m_pState->SetCipherMode(NumberAPCipherModes);

    result = request.LoadPerformancePage();
    if (ERROR_SUCCESS == result)
        result = request.LoadWepSettingsPage();
    if (ERROR_SUCCESS == result)
        result = request.LoadWpaSettingsPage();
    if (ERROR_SUCCESS == result)
        result = request.LoadPskSettingsPage();

    return result;
}

// ----------------------------------------------------------------------------
//
// Updates the device's configuration settings.
//
DWORD
DLinkDWL3200Controller_t::
UpdateSettings(const AccessPointState_t &NewState)
{
    DLinkDWL3200Request_t request(m_pHttp, m_pState, m_FirmwareVersion);

    return request.UpdateSettings(NewState);
}

// ----------------------------------------------------------------------------
