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
// Implementation of the APController_t class.
//
// ----------------------------------------------------------------------------

#include <APController_t.hpp>
#include <APConfigurator_t.hpp>
#include <AttenuationDriver_t.hpp>

#include <assert.h>
#include <tchar.h>

using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Provides a generic mechanism for translating between enumerations
// and string values.
//
struct EnumMap
{
    int          code;
    const TCHAR *name;
};

template <class T> const TCHAR *
EnumMap2String(
    EnumMap      Map[], 
    int          MapSize,
    T            Code, 
    const TCHAR *Unknown)
{
    int comp = (int)Code;
    for (int mx = 0 ; mx < MapSize ; ++mx)
    {
        if (Map[mx].code == comp)
            return Map[mx].name;
    }
    return Unknown;
}

template <class T> T
String2EnumMap(
    EnumMap      Map[], 
    int          MapSize, 
    const TCHAR *Name, 
    T            Unknown)
{
    for (int mx = 0 ; mx < MapSize ; ++mx)
    {
        if (_tcsicmp(Map[mx].name, Name) == 0)
            return (T)Map[mx].code;
    }
    return Unknown;
}

// ----------------------------------------------------------------------------
//
// AP Capabilities:
//
static EnumMap s_Capabilities[] =
{
    { APCapsWEP_802_1X, TEXT("WEP_802_1X") }
   ,{ APCapsWPA,        TEXT("WPA") }
   ,{ APCapsWPA_PSK,    TEXT("WPA_PSK") }
   ,{ APCapsWPA_AES,    TEXT("WPA_AES") }
   ,{ APCapsWPA2,       TEXT("WPA2") }
   ,{ APCapsWPA2_PSK,   TEXT("WPA2_PSK") }
   // Aliases:
   ,{ APCapsWPA_PSK,    TEXT("WPA-PSK") }
   ,{ APCapsWPA_AES,    TEXT("WPA-AES") }
   ,{ APCapsWPA2_PSK,   TEXT("WPA2-PSK") }
};
static const int s_CapabilitiesSize = COUNTOF(s_Capabilities);

const TCHAR *
ce::qa::
APCapability2String(
    APCapability_e Capability)
{
    return EnumMap2String(s_Capabilities, s_CapabilitiesSize, 
                          Capability, TEXT("Unknown AP Capability code"));
}

APCapability_e
ce::qa::
String2APCapability(
    const TCHAR *CapaString)
{
    return String2EnumMap(s_Capabilities, s_CapabilitiesSize,
                          CapaString, UnknownAPCapability);
}

HRESULT
ce::qa::
APCapabilities2String(
                            int    Capabilities,
  __out_ecount(BufferChars) TCHAR *Buffer,
                            int    BufferChars)
{
    TCHAR *bufptr = Buffer;
    for (int mx = 0 ; Capabilities != 0 && mx < s_CapabilitiesSize ; ++mx)
    {
        int code = s_Capabilities[mx].code;
        if ((Capabilities & code) == code)
        {
            if (bufptr != Buffer && 2 < BufferChars)
                *(bufptr++) = TEXT(',');
            const TCHAR *caps = APCapability2String((APCapability_e)code);
            int length = _tcslen(caps);
            if (length + 2 > BufferChars)
                return ERROR_INSUFFICIENT_BUFFER;
            memcpy(bufptr, caps, length * sizeof(TCHAR));
            bufptr += length;
            BufferChars -= length;
            Capabilities &= ~code;
        }
    }
    *bufptr = TEXT('\0');
    return S_OK;
}

int
ce::qa::
String2APCapabilities(
    const TCHAR *CapsString)
{
    static const TCHAR seps[] = TEXT(";:, \t");
    int result = 0;
    ce::tstring lstring(CapsString);
    for (TCHAR *token = _tcstok(lstring.get_buffer(), seps) ;
                token != NULL ;
                token = _tcstok(NULL, seps))
    {
        ce::tstring caps(token);
        caps.trim(seps);
        if (caps.length() != 0)
        {
            int code = (int)String2APCapability(caps);
            if (code >= (int)UnknownAPCapability)
            {
                return code;
            }
            result |= code;
        }
    }
    return result;
}

// ----------------------------------------------------------------------------
//
// STA <-> AP authentication modes:
//
static EnumMap s_AuthModes[] =
{
    { APAuthOpen,       TEXT("Open") }
   ,{ APAuthShared,     TEXT("Shared") }
   ,{ APAuthWEP_802_1X, TEXT("WEP_802_1X") }
   ,{ APAuthWPA,        TEXT("WPA") }
   ,{ APAuthWPA_PSK,    TEXT("WPA_PSK") }
   ,{ APAuthWPA2,       TEXT("WPA2") }
   ,{ APAuthWPA2_PSK,   TEXT("WPA2_PSK") }
   // Aliases:
   ,{ APAuthOpen,       TEXT("none") }
   ,{ APAuthWEP_802_1X, TEXT("WEP_802_1X") }
   ,{ APAuthWEP_802_1X, TEXT("WEP-802-1X") }
   ,{ APAuthWPA_PSK,    TEXT("WPA-PSK") }
   ,{ APAuthWPA2_PSK,   TEXT("WPA2-PSK") }
};
static const int s_AuthModesSize = COUNTOF(s_AuthModes);

const TCHAR *
ce::qa::
APAuthMode2String(
    APAuthMode_e AuthMode)
{
    return EnumMap2String(s_AuthModes, s_AuthModesSize,
                          AuthMode, TEXT("Unknown authentication mode"));
}

APAuthMode_e
ce::qa::
String2APAuthMode(
    const TCHAR *AuthString)
{
    return String2EnumMap(s_AuthModes, s_AuthModesSize,
                          AuthString, UnknownAPAuthMode);
}

// ----------------------------------------------------------------------------
//
// Message-encryption ciphers:
//
static struct EnumMap s_CipherModes[] =
{
    { APCipherClearText, TEXT("ClearText") }
   ,{ APCipherWEP,       TEXT("WEP") }
   ,{ APCipherTKIP,      TEXT("TKIP") }
   ,{ APCipherAES,       TEXT("AES") }
   // Aliases:
   ,{ APCipherClearText, TEXT("none") }
};
static const int s_CipherModesSize = COUNTOF(s_CipherModes);

const TCHAR *
ce::qa::
APCipherMode2String(
    APCipherMode_e CipherMode)
{
    return EnumMap2String(s_CipherModes, s_CipherModesSize, 
                          CipherMode, TEXT("Unknown cipher mode"));
}

APCipherMode_e
ce::qa::
String2APCipherMode(
    const TCHAR *CipherString)
{
    return String2EnumMap(s_CipherModes, s_CipherModesSize,
                          CipherString, UnknownAPCipherMode);
}

// ----------------------------------------------------------------------------
//
// EAP (Extensible Authentication Protocol) authentication ciphers:
//
static struct EnumMap s_EapAuthModes[] =
{
    { APEapAuthTLS,  TEXT("TLS") }
   ,{ APEapAuthMD5,  TEXT("MD5") }
   ,{ APEapAuthPEAP, TEXT("PEAP") }
   // Aliases:
};
static const int s_EapAuthModesSize = COUNTOF(s_EapAuthModes);

const TCHAR *
ce::qa::
APEapAuthMode2String(
    APEapAuthMode_e EapAuthMode)
{
    return EnumMap2String(s_EapAuthModes, s_EapAuthModesSize, 
                          EapAuthMode, TEXT("Unknown EAP auth mode"));
}

APEapAuthMode_e
ce::qa::
String2APEapAuthMode(
    const TCHAR *EapAuthString)
{
    return String2EnumMap(s_EapAuthModes, s_EapAuthModesSize,
                          EapAuthString, UnknownAPEapAuthMode);
}

// ----------------------------------------------------------------------------
//  
// Validate the specified combination of security modes given the AP's
// stated capabilities.
//
bool
ce::qa::
ValidateSecurityModes(
    APAuthMode_e   AuthMode,
    APCipherMode_e CipherMode,
    int            Capabilties)
{
    if ((int)Capabilties >= (int)UnknownAPCapability)
        return false;

    switch (AuthMode)
    {
        case APAuthOpen:
        case APAuthShared:
            break;
        case APAuthWEP_802_1X:
            if ((Capabilties & APCapsWEP_802_1X) == 0)
                return false;
            break;
        case APAuthWPA:
            if ((Capabilties & APCapsWPA) == 0)
                return false;
            break;
        case APAuthWPA_PSK:
            if ((Capabilties & APCapsWPA_PSK) == 0)
                return false;
            break;
        case APAuthWPA2:
            if ((Capabilties & APCapsWPA2) == 0)
                return false;
            break;
        case APAuthWPA2_PSK:
            if ((Capabilties & APCapsWPA2_PSK) == 0)
                return false;
            break;
        default:
            return false;
    }

    switch (CipherMode)
    {
        case APCipherClearText:
        case APCipherWEP:
            break;
        case APCipherTKIP:
            if ((Capabilties & (APCapsWPA
                              | APCapsWPA_PSK
                              | APCapsWPA_AES
                              | APCapsWPA2
                              | APCapsWPA2_PSK)) == 0)
                return false;
            break;
        case APCipherAES:
            if ((Capabilties & (APCapsWPA_AES
                              | APCapsWPA2
                              | APCapsWPA2_PSK)) == 0)
                return false;
            break;
        default:
            return false;
    }

    assert(NumberAPAuthModes   == 7);
    assert(NumberAPCipherModes == 4);
    static bool LegalModes[NumberAPAuthModes][NumberAPCipherModes] =
    {// Cipher:                             Authentication:
     //   ClearText  WEP    TKIP     AES 
         {  true,   true,   false,  false } // AuthOpen
        ,{  false,  true,   false,  false } // AuthShared
        ,{  false,  true,   false,  false } // AuthWEP_802_1X
        ,{  false,  false,  true,   true  } // AuthWPA
        ,{  false,  false,  true,   true  } // AuthWPA_PSK
        ,{  false,  false,  true,   true  } // AuthWPA2
        ,{  false,  false,  true,   true  } // AuthWPA2_PSK
    };

    return LegalModes[(int)AuthMode   % NumberAPAuthModes]
                     [(int)CipherMode % NumberAPCipherModes];
}

/* ============================ APController_t ============================= */

// ----------------------------------------------------------------------------
//  
// Constructor.
//
APController_t::
APController_t(
    APConfigurator_t    *pConfig,
    AttenuationDriver_t *pAttenuator)
    : m_pConfig(pConfig),
      m_pAttenuator(pAttenuator)
{
    assert(NULL != pConfig);
    assert(NULL != pAttenuator);
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
APController_t::
~APController_t(void)
{
    if (m_pConfig != NULL)
    {
        delete m_pConfig;
        m_pConfig = NULL;
    }
    if (m_pAttenuator != NULL)
    {
        delete m_pAttenuator;
        m_pAttenuator = NULL;
    }
}

// ----------------------------------------------------------------------------
//   
// Determines whether the object was successfully connected to an
// Access Point and/or RF Attenuator.
//
bool
APController_t::
IsAccessPointValid(void)
{
    if (NULL == m_pConfig)
        return false;
    else
    {
        HRESULT hr = m_pConfig->Reconnect();
        return SUCCEEDED(hr);
    }
}

bool
APController_t::
IsAttenuatorValid(void)
{
    if (NULL == m_pAttenuator)
        return false;
    else
    {
        HRESULT hr = m_pAttenuator->Reconnect();
        return SUCCEEDED(hr);
    }
}

// ----------------------------------------------------------------------------
//   
// Sends the saved configuration updates to the AP and RF-attenuator
// devices and stores them in the configuration store.
//
HRESULT
APController_t::
SaveConfiguration(void)
{
    HRESULT hr;

    // Update the configuration.
    hr = m_pConfig->SaveConfiguration();

    // Update the attenuation.
    if (SUCCEEDED(hr))
    {
        hr = m_pAttenuator->SaveAttenuation();
    }

    return hr;
}

// ----------------------------------------------------------------------------
//
// Gets the AP's "friendly" name.
//
const TCHAR *
APController_t::
GetAPName(void) const 
{
    return m_pConfig->GetAPName();
}

// ----------------------------------------------------------------------------
//
// Gets the AP vendor name or model number.
//
const TCHAR *
APController_t::
GetVendorName(void) const 
{
    return m_pConfig->GetVendorName();
}

const TCHAR *
APController_t::
GetModelNumber(void) const 
{
    return m_pConfig->GetModelNumber();
}

// ----------------------------------------------------------------------------
//
// Gets or sets the RF-attenuation value.
//
int
APController_t::
GetAttenuation(void) const
{
    return m_pAttenuator->GetAttenuation();
}

HRESULT
APController_t::
SetAttenuation(int Attenuation)
{
    return m_pAttenuator->SetAttenuation(Attenuation);
}

int
APController_t::
GetMinAttenuation(void) const
{
    return m_pAttenuator->GetMinAttenuation();
}

int
APController_t::
GetMaxAttenuation(void) const
{
    return m_pAttenuator->GetMaxAttenuation();
}

// ----------------------------------------------------------------------------
//
// Schedules an attenutation adjustment within the specified range
// using the specified adjustment steps.
//
HRESULT
APController_t::
AdjustAttenuation(
    int  StartAttenuation, // Attenuation after first adjustment
    int  FinalAttenuation, // Attenuation after last adjustment
    int  AdjustmentStep,   // Size, in dBs, of each adjustment
    long StepTimeMS)       // Time, in milliseconds, between each step
                           // (Limited to between 500 (1/2 second) and
                           // 3,600,000 (1 hour)
{
    return m_pAttenuator->AdjustAttenuation(StartAttenuation,
                                            FinalAttenuation,
                                            AdjustmentStep,
                                            StepTimeMS);
}

// ----------------------------------------------------------------------------
//
// Gets or sets the SSID (WLAN Service Set Identifier / Name):
//
const TCHAR *
APController_t::
GetSsid(void) const 
{
    return m_pConfig->GetSsid();
}

HRESULT
APController_t::
SetSsid(const TCHAR *pSsid)
{
    return m_pConfig->SetSsid(pSsid);
}

// ----------------------------------------------------------------------------
//
// Gets or sets the BSSID (Basic Service Set Identifier):
//
const MACAddr_t &
APController_t::
GetBSsid(void) const 
{
    return m_pConfig->GetBSsid();
}

HRESULT
APController_t::
SetBSsid(const MACAddr_t &BSsid)
{
    return m_pConfig->SetBSsid(BSsid);
}

// ----------------------------------------------------------------------------
//
// Gets or sets the bit-map describing the AP's capabilities:
//
int
APController_t::
GetCapabilitiesImplemented(void) const 
{
    return m_pConfig->GetCapabilitiesImplemented();
}

int
APController_t::
GetCapabilitiesEnabled(void) const 
{
    return m_pConfig->GetCapabilitiesEnabled();
}

HRESULT
APController_t::
SetCapabilitiesEnabled(int Capabilities)
{
    return m_pConfig->SetCapabilitiesEnabled(Capabilities);
}

// ----------------------------------------------------------------------------
//
// Gets or sets the AP's radio on/off status:
//
bool
APController_t::
IsRadioOn(void) const 
{
    return m_pConfig->IsRadioOn();
}

HRESULT
APController_t::
SetRadioState(bool State)
{ 
    return m_pConfig->SetRadioState(State);
}

// ----------------------------------------------------------------------------
//
// Gets or sets the security modes:
//
APAuthMode_e 
APController_t::
GetAuthMode(void) const 
{
    return m_pConfig->GetAuthMode();
}

APCipherMode_e
APController_t::
GetCipherMode(void) const 
{
    return m_pConfig->GetCipherMode();
}

HRESULT
APController_t::
SetSecurityMode(
    APAuthMode_e   AuthMode,
    APCipherMode_e CipherMode)
{
    return m_pConfig->SetSecurityMode(AuthMode, CipherMode);
}

// ----------------------------------------------------------------------------
//
// Gets or sets the RADIUS Server information:
//
DWORD
APController_t::
GetRadiusServer(void) const 
{
    return m_pConfig->GetRadiusServer();
}

int
APController_t::
GetRadiusPort(void) const
{
    return m_pConfig->GetRadiusPort();
}

const TCHAR *
APController_t::
GetRadiusSecret(void) const 
{
    return m_pConfig->GetRadiusSecret();
}

HRESULT
APController_t::
SetRadius(
    const TCHAR *pServer,
    int          Port,
    const TCHAR *pKey)
{
    return m_pConfig->SetRadius(pServer, Port, pKey);
}

// ----------------------------------------------------------------------------
//
// Gets or sets the WEP (Wired Equivalent Privacy) key.
//
const WEPKeys_t &
APController_t::
GetWEPKeys(void) const
{
    return m_pConfig->GetWEPKeys();
}

HRESULT
APController_t::
SetWEPKeys(const WEPKeys_t &KeyData)
{
    return m_pConfig->SetWEPKeys(KeyData);
}

// ----------------------------------------------------------------------------
//
// Gets or sets the pass-phrase.
//
const TCHAR *
APController_t::
GetPassphrase(void) const
{
    return m_pConfig->GetPassphrase();
}

HRESULT
APController_t::
SetPassphrase(const TCHAR *Passphrase)
{ 
    return m_pConfig->SetPassphrase(Passphrase);
}

// ----------------------------------------------------------------------------
