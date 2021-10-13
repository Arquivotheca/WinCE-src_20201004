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
// Definitions and declarations for the APController_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_APController_t_
#define _DEFINED_APController_t_
#pragma once

#include <APCTypes.hpp>
#include <MACAddr_t.hpp>
#include <WEPKeys_t.hpp>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Defines an attenuation step in an automated series:
//
struct RFAttenuationStep_t
{
    int StepAttenuation; // Attenuation in db
    int StepSeconds;     // Seconds to remain at this attenuation step
                         // (Limited to between 1 and 2^8-1 (255) seconds)
};

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
class  APConfigurator_t;
class  AttenuationDriver_t;
struct RFAttenuationStep_t;
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

    // Minimum and maximum time for asynchronous state change (in seconds):
    static const int MinimumAsyncStateChangeTime = 15;      
    static const int MaximumAsyncStateChangeTime = 15*60;
    
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

    // Gets or sets the bit-map describing the AP's capabilities:
    int
    GetCapabilitiesImplemented(void) const;
    int
    GetCapabilitiesEnabled(void) const;
    HRESULT
    SetCapabilitiesEnabled(int Capabilities);

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

    // Gets or sets the attenuation steps:
    HRESULT
    GetAttenuationSteps(
        RFAttenuationStep_t *pAttenuationSteps,
        int                 *pNumberAttenuationSteps) const;
    HRESULT
    SetAttenuationSteps(
        const RFAttenuationStep_t *pAttenuationSteps,
        int                         NumberAttenuationSteps);

    // Gets or sets the number of attenuation steps which must be completed
    // before reporting successful completion to the client. If this is
    // non-zero, any following steps will be completed by the server after
    // reporting results to the client:
    int     GetSynchronousSteps(void) const;
    HRESULT SetSynchronousSteps(int Steps);

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

    // Gets or sets the AP's broadcast-SSID mode:
    bool
    IsSsidBroadcast(void) const;
    HRESULT
    SetSsidBroadcast(bool Mode);

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

    // Delays (in seconds) before toggling access point state and
    // changing it back:
    int
    GetAsyncStateChangeDelay(void) const;
    int
    GetAsyncStateChangeReset(void) const;
    HRESULT
    SetAsyncStateChange(int DelayTimeSec, int ResetTimeSec);
    
  // XML encoding/decoding:

    // Encode the object into a DOM element:
    HRESULT
    EncodeToXml(
        litexml::XmlElement_t **ppRoot,
        int AccessPointFieldFlags = 0x7FFFFFFF,
        int AttenuatorFieldFlage  = 0x7FFFFFFF) const;

    // Initialize the object from the specified DOM element:
    HRESULT
    DecodeFromXml(
        const litexml::XmlElement_t &Root);
    
};

};
};
#endif /* _DEFINED_APController_t_ */
// ----------------------------------------------------------------------------
