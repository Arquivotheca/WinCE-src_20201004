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
// Definitions and declarations for the APController_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_APController_t_
#define _DEFINED_APController_t_
#pragma once

#include <MACAddr_t.hpp>
#include <WEPKeys_t.hpp>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Bit-map describing the capabilities provided by an AP:
//
enum APCapability_e
{
    APCapsWEP_802_1X = 0x001 // Wired Equivalent Privacy with Radius
   ,APCapsWPA        = 0x010 // Wireless Protected Access with Radius
   ,APCapsWPA_PSK    = 0x020 // WPA with private shared key
   ,APCapsWPA_AES    = 0x040 // WPA using Advanced Encryption Standard
   ,APCapsWPA2       = 0x100 // Robust Security Network Associations (802.11i)
   ,APCapsWPA2_PSK   = 0x200 // RSNA with PSK
   ,AllAPCapabilities = 0xFFF
   ,UnknownAPCapability = 0x1000
};

const TCHAR *
APCapability2String(
    APCapability_e Capability);
APCapability_e
String2APCapability(
    const TCHAR *CapaString);
HRESULT
APCapabilities2String(
                            int    Capabilities, 
  __out_ecount(BufferChars) TCHAR *Buffer, 
                            int    BufferChars);
int
String2APCapabilities(
    const TCHAR *CapsString);

// ----------------------------------------------------------------------------
//
// STA <-> AP authentication modes:
//
enum APAuthMode_e
{
    APAuthOpen       = 0  // no authentication
   ,APAuthShared     = 1  // Wired Equivalent Privacy
   ,APAuthWEP_802_1X = 2  // "Dynamic" WEP with 802.1X (requires Radius)
   ,APAuthWPA        = 3  // Wireless Protected Access (requires Radius)
   ,APAuthWPA_PSK    = 4  // wireless Protected Access with private shared key
   ,APAuthWPA2       = 5  // RSNA (802.11i) (requires Radius)
   ,APAuthWPA2_PSK   = 6  // RSNA (802.11i) with private shared key
   ,NumberAPAuthModes = 7
   ,UnknownAPAuthMode = 99
};

const TCHAR *
APAuthMode2String(APAuthMode_e AuthMode);
APAuthMode_e
String2APAuthMode(const TCHAR *AuthString);

// ----------------------------------------------------------------------------
//
// Message-encryption ciphers:
//
enum APCipherMode_e
{
    APCipherClearText = 0 // no encryption
   ,APCipherWEP       = 1 // Wired Equivalent Privacy
   ,APCipherTKIP      = 2 // Temporal Key Integrity Protocol
   ,APCipherAES       = 3 // Advanced Encryption Standard
   ,NumberAPCipherModes = 4 
   ,UnknownAPCipherMode = 99
};

const TCHAR *
APCipherMode2String(APCipherMode_e CipherMode);
APCipherMode_e
String2APCipherMode(const TCHAR *CipherString); 

// ----------------------------------------------------------------------------
//
// EAP (Extensible Authentication Protocol) authentication ciphers:
//
enum APEapAuthMode_e
{
    APEapAuthTLS  = 0  // Transport Layer Security
   ,APEapAuthMD5  = 1  // Message Digest algorithm 5
   ,APEapAuthPEAP = 2  // Protected Extensible Authentication Protocol
   ,NumberAPEapAuthModes = 3
   ,UnknownAPEapAuthMode = 99
};

const TCHAR *
APEapAuthMode2String(APEapAuthMode_e EapAuthMode);
APEapAuthMode_e
String2APEapAuthMode(const TCHAR *EapAuthString); 

// ----------------------------------------------------------------------------
//
// Validate the specified combination of security modes given the AP's
// stated capabilities:
//
bool
ValidateSecurityModes(
    APAuthMode_e   AuthMode,
    APCipherMode_e CipherMode,
    int            Capabilities = (int)AllAPCapabilities);

// ----------------------------------------------------------------------------
//
// Provides the basic implementation for an AP (WiFi Access Point) device-
// controller.
//
// These objects can only be constructed by the static CreateAPControllers
// method. A list of these objects is usually created and individual objects
// referenced via the APPool_t class.
//
// In the standard usage model, the programmer has APPool_t create a list
// of these objects then selects the AP to be used for a particular test:
// 
//      using namespace ce::qa;
//
//      APPool_t apList;
//      HRESULT hr = apList.LoadAPControllers(TEXT("Software\\Microsoft\\CETest\\TEST27"));
//      if (FAILED(hr)
//          return hr;
// 
//      APController_t *ap = NULL;
//      for (int apx = apList.size() ; --apx >= 0 ;)
//      {
//          APController_t *thisAP = apList.GetAP(apx);
//          if (thisAP->GetAuthMode() == APAuthWPA)
//          {
//              ap = thisAp;
//              break;
//          }
//      }
//
// Then the selected AP can be interrogated and/or re-configured as needed
// for the test:
//
//      ap->SetSecurityMode(APAuthWPA, APCipherClearText):
//      ap->SetPassphrase(TEXT("0987654321qwerty"));
//      ap->SetAttenuation(57);
//      hr = ap->SaveConfiguration();
//      if (FAILED(hr))
//          return hr;
//
// Note that the configuration changes are not transmitted to the device
// until the SaveConfiguration method is called. This, effectively, caches 
// all configuration changes so they can be sent to the device as a group.
//
class APConfigurator_t;
class AttenuationDriver_t;
class APController_t
{
private:
    
    // Copy and assignment are deliberately disabled:
    APController_t(const APController_t &src);
    APController_t &operator = (const APController_t &src);

protected:

    // AP's configuration data:
    APConfigurator_t *m_pConfig;

    // Signal-strength attenuation:
    AttenuationDriver_t *m_pAttenuator;

public:

    // Constructor and destructor:
    APController_t( 
        APConfigurator_t    *pConfig,
        AttenuationDriver_t *pAttenuator);
    virtual
   ~APController_t(void);

    // Determines whether the object was successfully connected to an
    // Access Point and/or RF Attenuator:
    bool
    IsAccessPointValid(void);
    bool
    IsAttenuatorValid(void);
 
    // Sends the saved configuration updates to the AP and RF-attenuator
    // devices and stores them in the configuration store:
    virtual HRESULT
    SaveConfiguration(void);

    // Gets the AP's "friendly" name:
    const TCHAR *
    GetAPName(void) const;
    
    // Gets the AP vendor name or model number:
    const TCHAR *
    GetVendorName(void) const;
    const TCHAR *
    GetModelNumber(void) const;

    // Gets or sets the current RF-attenuation value:
    int
    GetAttenuation(void) const;
    HRESULT
    SetAttenuation(int Attenuation);
    int
    GetMinAttenuation(void) const;
    int
    GetMaxAttenuation(void) const;

    // Schedules an attenutation adjustment within the specified range
    // using the specified adjustment steps:
    HRESULT
    AdjustAttenuation(
        int  StartAttenuation,    // Attenuation after first adjustment
        int  FinalAttenuation,    // Attenuation after last adjustment
        int  AdjustTime = 30,     // Time, in seconds, to perform adjustment
        long StepTimeMS = 1000L); // Time, in milliseconds, between each step
                                  // (Limited to between 500 (1/2 second) and
                                  // 3,600,000 (1 hour)

    // Gets or sets the SSID (WLAN Service Set Identifier / Name):
    const TCHAR *
    GetSsid(void) const;
    HRESULT
    SetSsid(const TCHAR *pSsid);

    // Gets or sets the BSSID (Basic Service Set Identifier):
    const MACAddr_t &
    GetBSsid(void) const;
    HRESULT
    SetBSsid(const MACAddr_t &BSsid);

    // Gets or sets the bit-map describing the AP's capabilities:
    int
    GetCapabilitiesImplemented(void) const;
    int
    GetCapabilitiesEnabled(void) const;
    HRESULT
    SetCapabilitiesEnabled(int Capabilities);
    
    // Gets or sets the AP's radio on/off status:
    bool
    IsRadioOn(void) const;
    HRESULT
    SetRadioState(bool State);

    // Gets or sets the security modes:
    APAuthMode_e 
    GetAuthMode(void) const;
    const TCHAR *
    GetAuthName(void) const { APAuthMode2String(GetAuthMode()); }
    APCipherMode_e
    GetCipherMode(void) const;
    const TCHAR *
    GetCipherName(void) const { APCipherMode2String(GetCipherMode()); }
    HRESULT
    SetSecurityMode(
        APAuthMode_e   AuthMode,
        APCipherMode_e CipherMode);

    // Gets or sets the RADIUS Server information:
    DWORD
    GetRadiusServer(void) const;
    int
    GetRadiusPort(void) const;
    const TCHAR *
    GetRadiusSecret(void) const;
    HRESULT
    SetRadius(
        const TCHAR *pServer,
        int          Port,
        const TCHAR *pKey);

    // Gets or sets the WEP (Wired Equivalent Privacy) key:
    const WEPKeys_t &
    GetWEPKeys(void) const;
    HRESULT
    SetWEPKeys(const WEPKeys_t &KeyData);

    // Gets or sets the pass-phrase:
    const TCHAR *
    GetPassphrase(void) const;
    HRESULT
    SetPassphrase(const TCHAR *pPassphrase);
};

};
};
#endif /* _DEFINED_APController_t_ */
// ----------------------------------------------------------------------------
