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
// Implementation of the WZCConfigItem_t class.
//
// ----------------------------------------------------------------------------

#include "WZCConfigItem_t.hpp"

#include "WZCService_t.hpp"

#include <assert.h>
#include <WEPKeys_t.hpp>

using namespace ce;
using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Copies or deallocates the specified RAW_DATA structure.
//
DWORD
WZCConfigItem_t::
CopyRawData(RAW_DATA *pDest, const RAW_DATA &Source)
{
    assert(NULL != pDest);

   *pDest = Source;
    pDest->pData = NULL;
    pDest->dwDataLen = 0;

    if (Source.pData && Source.dwDataLen)
    {
        pDest->pData = (BYTE *) LocalAlloc(LMEM_FIXED, Source.dwDataLen);

        if (NULL == pDest->pData)
        {
            LogError(TEXT("Can't allocate %u bytes for WZC RAW_DATA"),
                     Source.dwDataLen);
            return ERROR_OUTOFMEMORY;
        }

        memcpy(pDest->pData, Source.pData, Source.dwDataLen);
        pDest->dwDataLen = Source.dwDataLen;
    }

    return ERROR_SUCCESS;
}

void
WZCConfigItem_t::
FreeRawData(RAW_DATA *pData)
{
    assert(NULL != pData);

    if (pData->pData && pData->dwDataLen)
        LocalFree(pData->pData);

    pData->pData = NULL;
    pData->dwDataLen = 0;
}

#ifdef UNDER_CE
// ----------------------------------------------------------------------------
//
// Copies or deallocates the specified WZC_EAPOL_PARAMS structure.
//
static DWORD
CopyEapolParams(WZC_EAPOL_PARAMS *pDest, const WZC_EAPOL_PARAMS &Source)
{
    assert(NULL != pDest);

   *pDest = Source;
    pDest->pbAuthData = NULL;
    pDest->dwAuthDataLen = 0;

    if (Source.pbAuthData && Source.dwAuthDataLen)
    {
        pDest->pbAuthData = (BYTE *) LocalAlloc(LMEM_FIXED, Source.dwAuthDataLen);

        if (NULL == pDest->pbAuthData)
        {
            LogError(TEXT("Can't allocate %u bytes for WZC_EAPOL_PARAMS"),
                     Source.dwAuthDataLen);
            return ERROR_OUTOFMEMORY;
        }

        memcpy(pDest->pbAuthData, Source.pbAuthData, Source.dwAuthDataLen);
        pDest->dwAuthDataLen = Source.dwAuthDataLen;
    }

    return ERROR_SUCCESS;
}

static void
FreeEapolParams(WZC_EAPOL_PARAMS *pData)
{
    assert(NULL != pData);

    if (pData->pbAuthData && pData->dwAuthDataLen)
        LocalFree(pData->pbAuthData);

    pData->pbAuthData = NULL;
    pData->dwAuthDataLen = 0;
}
#endif /* UNDER_CE */

// ----------------------------------------------------------------------------
//
// Copies or deallocates the specified WZC_WLAN_CONFIG structure.
//
DWORD
WZCConfigItem_t::
CopyConfigItem(WZC_WLAN_CONFIG *pDest, const WZC_WLAN_CONFIG &Source)
{
    assert(NULL != pDest);

   *pDest = Source;

    DWORD result = CopyRawData(&pDest->rdUserData,
                                Source.rdUserData);

#ifdef UNDER_CE
    if (ERROR_SUCCESS == result)
    {
        result = CopyEapolParams(&pDest->EapolParams,
                                  Source.EapolParams);

        if (ERROR_SUCCESS != result)
        {
            FreeRawData(&pDest->rdUserData);
        }
    }
#endif

    return result;
}

void
WZCConfigItem_t::
FreeConfigItem(WZC_WLAN_CONFIG *pData)
{
    assert(NULL != pData);

    FreeRawData(&pData->rdUserData);
#ifdef UNDER_CE
    FreeEapolParams(&pData->EapolParams);
#endif
}

// ----------------------------------------------------------------------------
//
// Constructor.
//
WZCConfigItem_t::
WZCConfigItem_t(void)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
WZCConfigItem_t::
~WZCConfigItem_t(void)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Copy constructors.
//
WZCConfigItem_t::
WZCConfigItem_t(const WZCConfigItem_t &Source)
    : m_pData(Source.m_pData)
{
    // nothing to do
}

WZCConfigItem_t::
WZCConfigItem_t(const WZC_WLAN_CONFIG &Source)
    : m_pData(Source)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Assignment operators.
//
WZCConfigItem_t &
WZCConfigItem_t::
operator = (const WZCConfigItem_t &Source)
{
    if (&Source != this)
    {
        m_pData = Source.m_pData;
    }
    return *this;
}

WZCConfigItem_t &
WZCConfigItem_t::
operator = (const WZC_WLAN_CONFIG &Source)
{
    if (&Source != m_pData)
    {
        m_pData = Source;
    }
    return *this;
}

// ----------------------------------------------------------------------------
//
// Makes sure this object contains a private copy of the WZC data.
//
DWORD
WZCConfigItem_t::
Privatize(void)
{
    DWORD result = m_pData.Privatize();

    if (ERROR_SUCCESS != result)
    {
        LogError(TEXT("Can't allocate memory to copy WZC_WLAN_CONFIG"));
    }

    return result;
}

// ----------------------------------------------------------------------------
//
// Encrypt WEP key material.
// Note: this is simply to protect from memory scanning.
//
static void
EncryptWEPKeyMaterial(
    WZC_WLAN_CONFIG *pConfig)
{
    static const BYTE fakeKeyMaterial[] = {
        0x56, 0x09, 0x08, 0x98, 0x4D,
        0x08, 0x11, 0x66, 0x42, 0x03,
        0x01, 0x67, 0x66
    };

    for (int kx = 0 ; kx < WZCCTL_MAX_WEPK_MATERIAL ; ++kx)
    {
        pConfig->KeyMaterial[kx] ^= fakeKeyMaterial[(7 * kx) % 13];
    }
}

// ----------------------------------------------------------------------------
//
// Initializes a wireless config object to connect to an infrastructure
// network as indicated.
// Returns two special error codes to signal errors which could be
// caused by the normal negative WZC API tests:
//     ERROR_INVALID_FLAGS - indicates security-modes are invalid
//     ERROR_INVALID_DATA -- indicates the key-data is invalid
//
DWORD
WZCConfigItem_t::
InitInfrastructure(
    const TCHAR                    *pSsidName,
    NDIS_802_11_AUTHENTICATION_MODE AuthMode,
    ULONG                           CipherMode,
    DWORD                           EAPType,
    int                             KeyIndex,
    const TCHAR                    *pKey)       // NULL for 802.1X (EAP)
{
    HRESULT hr;
    DWORD result;

    // Get a private copy of the structure and clear it.
    result = Privatize();
    if (ERROR_SUCCESS != result)
        return result;

    WZC_WLAN_CONFIG *pConfig = m_pData;
    FreeConfigItem(pConfig);

    memset(pConfig, 0, sizeof(WZC_WLAN_CONFIG));
    pConfig->Length = sizeof(WZC_WLAN_CONFIG);

    // Infrastructure:
    pConfig->InfrastructureMode = Ndis802_11Infrastructure;

    // SSID:
    if (pSsidName && pSsidName[0])
    {
        // Convert SSID from wide to (possibly multi-) byte characters.
        char ssidBuffer[MAX_SSID_LEN * 5];
        hr = WiFUtils::ConvertString(ssidBuffer, pSsidName,
                             COUNTOF(ssidBuffer), "SSID");
        if (FAILED(hr))
            return HRESULT_CODE(hr);

        size_t ssidChars = strlen(ssidBuffer);
        if (ssidChars > MAX_SSID_LEN)
            ssidChars = MAX_SSID_LEN;

        pConfig->Ssid.SsidLength = ssidChars;
        memcpy(pConfig->Ssid.Ssid, ssidBuffer, ssidChars);
    }

    // Security modes:
    pConfig->AuthenticationMode = AuthMode;
    pConfig->Privacy            = CipherMode;

    // WEP key:
    bool needEAPInfo = false;
    if (CipherMode == Ndis802_11WEPEnabled)
    {
        if (NULL == pKey || TEXT('\0') == pKey[0])
            needEAPInfo = true;
        else
        {
            assert(0 <= KeyIndex && KeyIndex < 4);
            KeyIndex = abs(KeyIndex) % 4;
            pConfig->KeyIndex = KeyIndex;

            WEPKeys_t wepKeys;
            hr = wepKeys.FromString(KeyIndex, pKey);
            if (FAILED(hr))
            {
                LogError(TEXT("Bad WEP key #%d \"%s\""), KeyIndex+1, pKey);
                return ERROR_INVALID_DATA;
            }

            size_t keySize = static_cast<size_t>(wepKeys.m_Size[KeyIndex]);
            if (keySize > sizeof(pConfig->KeyMaterial))
                keySize = sizeof(pConfig->KeyMaterial);

            pConfig->KeyLength = keySize;
            memcpy(pConfig->KeyMaterial,
                     wepKeys.m_Material[KeyIndex], keySize);

            EncryptWEPKeyMaterial(pConfig);
            pConfig->dwCtlFlags |= WZCCTL_WEPK_PRESENT;
        }
    }

    // Pre-shared key:
    else
    if (CipherMode == Ndis802_11Encryption2Enabled
     || CipherMode == Ndis802_11Encryption3Enabled)
    {
        if (NULL == pKey || TEXT('\0') == pKey[0])
            needEAPInfo = true;
        else
        {
            char keyBuffer[MAX_PATH];
            memset(keyBuffer, 0, sizeof(keyBuffer));

            hr = WiFUtils::ConvertString(keyBuffer, pKey,
                                 COUNTOF(keyBuffer), "PSK");
            if (FAILED(hr))
                return HRESULT_CODE(hr);

            WZCPassword2Key(pConfig, keyBuffer);

            EncryptWEPKeyMaterial(pConfig);
            pConfig->dwCtlFlags |= WZCCTL_WEPK_XFORMAT
                                |  WZCCTL_WEPK_PRESENT
                                |  WZCCTL_ONEX_ENABLED;
        }

        // Fill in default EAPOL parameters.
        pConfig->EapolParams.dwEapType    = DEFAULT_EAP_TYPE;
        pConfig->EapolParams.dwEapFlags   = EAPOL_ENABLED;
        pConfig->EapolParams.bEnable8021x = TRUE;
        pConfig->WPAMCastCipher = Ndis802_11Encryption2Enabled;
    }

    // If necessary, set up EAP (802.1X) authentication.
    if (needEAPInfo)
    {
        pConfig->EapolParams.dwEapType     = EAPType;
        pConfig->EapolParams.dwEapFlags    = EAPOL_ENABLED;
        pConfig->EapolParams.bEnable8021x  = TRUE;
        pConfig->EapolParams.dwAuthDataLen = 0;
        pConfig->EapolParams.pbAuthData    = 0;
    }

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Convert the specified supported rate to text.
//
static void
SupportedRate2Text(
                            BYTE RawValue,
  __out_ecount(BufferChars) TCHAR *pBuffer,
                            size_t  BufferChars)
{
    double fRate = ((double)(RawValue & 0x7F)) * 0.5;
    StringCchPrintf(pBuffer, BufferChars, TEXT("%.1f"), fRate);
}

// ----------------------------------------------------------------------------
//
// Logs the network configuration.
//
void
WZCConfigItem_t::
Log(void (*LogFunc)(const TCHAR *pFormat, ...),
    const TCHAR *pLinePrefix,   // insert at front of each line
    bool         Verbose) const // display all available info
{
    assert(NULL != LogFunc);
    if (NULL == pLinePrefix)
        pLinePrefix = TEXT("");

    const WZC_WLAN_CONFIG *pConfig = m_pData;

    TCHAR buff[MAX_PATH];
    int   buffChars = COUNTOF(buff);

    const TCHAR *desc;

    if (Verbose)
    {
        LogFunc(TEXT("%sLength                  = %d bytes."),
                pLinePrefix, pConfig->Length);
    }

    LogFunc(TEXT("%sdwCtlFlags              = 0x%08X"),
            pLinePrefix, pConfig->dwCtlFlags);

    if (Verbose)
    {
        MACAddr_t macAddr;
        macAddr.Assign((BYTE *)pConfig->MacAddress,
                        sizeof(pConfig->MacAddress));
        macAddr.ToString(buff, buffChars);
        LogFunc(TEXT("%sMacAddress              = %s"), pLinePrefix, buff);
    }

    WZCService_t::SSID2Text(pConfig->Ssid.Ssid,
                            pConfig->Ssid.SsidLength, buff, buffChars);
    if (TEXT('\0') == buff[0])
        SafeCopy(buff, TEXT("<NULL>"), buffChars);
    LogFunc(TEXT("%sSSID                    = %s"), pLinePrefix, buff);

    if (1 == pConfig->Privacy)
    {
        SafeCopy(buff,
                 TEXT("Privacy disabled (wireless data not encrypted)"),
                 buffChars);
    }
    else
    {
        desc = WZCService_t::PrivacyMode2Text(pConfig->Privacy);
        StringCchPrintf(buff, buffChars,
                        TEXT("Privacy enabled (encrypted with [%s])"),
                        desc);
    }
    LogFunc(TEXT("%sPrivacy                 = %d  %s"),
            pLinePrefix, pConfig->Privacy, buff);

    if (Verbose)
    {
        LogFunc(TEXT("%sRSSI                    = %d dBm")
                TEXT(" (0=excellent, -100=weak signal)"),
                pLinePrefix, pConfig->Rssi);

        desc = WZCService_t::NetworkType2Text(pConfig->NetworkTypeInUse);
        LogFunc(TEXT("%sNetworkTypeInUse        = %d %s"),
                pLinePrefix, pConfig->NetworkTypeInUse, desc);

        LogFunc(TEXT("%sConfiguration:"), pLinePrefix);
        LogFunc(TEXT("%s  Struct Length         = %d"),
                pLinePrefix, pConfig->Configuration.Length);
        LogFunc(TEXT("%s  BeaconPeriod          = %d kusec"),
                pLinePrefix, pConfig->Configuration.BeaconPeriod);
        LogFunc(TEXT("%s  ATIMWindow            = %d kusec"),
                pLinePrefix, pConfig->Configuration.ATIMWindow);

        int chan = WZCService_t::ChannelNumber2Int(pConfig->Configuration.DSConfig);
        LogFunc(TEXT("%s  DSConfig              = %d kHz (ch-%d)"),
                pLinePrefix, pConfig->Configuration.DSConfig, chan);

        LogFunc(TEXT("%s  FHConfig:\n"), pLinePrefix);
        LogFunc(TEXT("%s    Struct Length       = %d"),
                pLinePrefix, pConfig->Configuration.FHConfig.Length);
        LogFunc(TEXT("%s    HopPattern          = %d"),
                pLinePrefix, pConfig->Configuration.FHConfig.HopPattern);
        LogFunc(TEXT("%s    HopSet              = %d"),
                pLinePrefix, pConfig->Configuration.FHConfig.HopSet);
        LogFunc(TEXT("%s    DwellTime           = %d"),
                pLinePrefix, pConfig->Configuration.FHConfig.DwellTime);
    }

    desc = WZCService_t::InfrastructureMode2Text(pConfig->InfrastructureMode);
    LogFunc(TEXT("%sInfrastructure          = %d %s"),
            pLinePrefix, pConfig->InfrastructureMode, desc);

    if (Verbose)
    {
        buff[0] = 0;
        for (int rx = 0 ; rx < COUNTOF(pConfig->SupportedRates) ; ++rx)
        {
            if (0 != pConfig->SupportedRates[rx])
            {
                if (buff[0]) SafeAppend(buff, TEXT(", "), buffChars);
                TCHAR rateBuff[20];
                SupportedRate2Text(pConfig->SupportedRates[rx], rateBuff, 
                                                        COUNTOF(rateBuff));
                SafeAppend(buff, rateBuff, buffChars);
            }
        }
        LogFunc(TEXT("%sSupportedRates          = %s (Mbit/s)"),
                pLinePrefix, buff);
    }

#if 0
    if (Verbose)
    {
        LogFunc(TEXT("%sKeyIndex                = <not available>")
                TEXT(" (beaconing packets don't have this info)"), pLinePrefix);
        LogFunc(TEXT("%sKeyLength               = <not available>")
                TEXT(" (beaconing packets don't have this info)"), pLinePrefix);
        LogFunc(TEXT("%sKeyMaterial             = <not available>")
                TEXT(" (beaconing packets don't have this info)"), pLinePrefix);
    }
#endif

    desc = WZCService_t::AuthenticationMode2Text(pConfig->AuthenticationMode);
    LogFunc(TEXT("%sAuthentication          = %d %s"),
            pLinePrefix, pConfig->AuthenticationMode, desc);

    if (Verbose)
    {
        LogFunc(TEXT("%srdUserData length       = %d bytes"), pLinePrefix,
                pConfig->rdUserData.dwDataLen);
    }
}

// ----------------------------------------------------------------------------
