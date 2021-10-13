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
// Definitions and declarations for the WLANProfileOneX_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WLANProfileOneX_t_
#define _DEFINED_WLANProfileOneX_t_
#pragma once

#include "WLANTypes.hpp"
#if (WLAN_VERSION > 0)

#include <WLANProfileEapConfig_t.hpp>

#include <vector.hxx>
#include <wlanapi.h>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Represents an 802.1X element within a MSM-Security profile.
//
class WiFiAccount_t;
class WiFiConfig_t;
class WLANProfileOneX_t
{
public:

    // Suplicant modes:
    //
    enum SupplicantMode_e
    {
        InhibitTransmission = 1, // EAPOL-Start packets are not transmitted.
                                 // Valid for wired LAN profiles only
        IncludeLearning     = 2, // EAPOL packets are transmitted according to
                                 // policy "learned" by the machine.
                                 // Valid for wired LAN profiles only. 
        Compliant           = 3, // EAPOL packets are transmitted as specified
                                 // by 802.1X. Valid for both wired and
                                 // wireless LAN profiles. 
    };

    // Type of credentials used for authentication:
    //
    enum AuthMode_e
    {
        MachineOrUser = 1, // Use machine or user credentials.
                           // When a user is logged on, the user's 
                           // credentials are used for authentication.
                           // When no user is logged on, machine
                           // credentials are used. 
        Machine       = 2, // Use machine credentials only.  
        User          = 3, // Use user credentials only. 
        Guest         = 4, // Use guest (empty) credentials only. 

    };
    
private:

    // Is user credential data to be cached?
    //
    static const bool sm_DefCacheUserData = false;
    bool m_bCacheUserData;

    // Time (in seconds) to wait between failed authentication attempts:
    //
    static const int sm_MinHeldPeriod =    1;
    static const int sm_DefHeldPeriod =   60;
    static const int sm_MaxHeldPeriod = 3601;
    int m_HeldPeriod;

    // Time (in seconds) to await a response from the authenticator:
    //
    static const int sm_MinAuthPeriod =    1;
    static const int sm_DefAuthPeriod =   30;
    static const int sm_MaxAuthPeriod = 3601;
    int m_AuthPeriod;

    // Time (in seconds) to wait before an EAPOL-Start is sent:
    //
    static const int sm_MinStartPeriod =    1;
    static const int sm_DefStartPeriod =    5;
    static const int sm_MaxStartPeriod = 3601;
    int m_StartPeriod;

    // Maximum EAPOL-Start messages w/o response before assuming authicator
    // is not present:
    //
    static const int sm_MinMaxStart =   1;
    static const int sm_DefMaxStart =   3;
    static const int sm_MaxMaxStart = 101;
    int m_MaxStart;

    // Maximum authentication failures before assuming credentials are
    // incorrect:
    //
    static const int sm_MinMaxAuthFailures =   1;
    static const int sm_DefMaxAuthFailures =   3;
    static const int sm_MaxMaxAuthFailures = 101;
    int m_MaxAuthFailures;

    // Supplicant mode:
    //
    static const SupplicantMode_e sm_DefSupplicantMode = Compliant;
    SupplicantMode_e m_SupplicantMode;

    // Authentication mode:
    //
    static const AuthMode_e sm_DefAuthMode = User;
    AuthMode_e m_AuthMode;

    // EAP Authentication configuration(s):
    //
    vector<WLANProfileEapConfig_t, ce::allocator, ce::incremental_growth<0>> m_EapConfigs;

public:

    // Constructor / destructor:
    //
    WLANProfileOneX_t(void) { Clear(); }
    void
    Clear(void);

    // Initializes the profile from the specified network configuration:
    // If necessary, uses the optional WiFiAccount object to supply EAP
    // authenticaiton credentials.
    // Returns ERROR_INSUFFICIENT_BUFFER if it will require multiple objects
    // to represent the network data. In that case, the caller should copy
    // this instance of the object and re-call the method again.
    //
    DWORD
    Initialize(
        const WiFiConfig_t  &Network,
        const WiFiAccount_t *pEapCredentials = NULL);

    // Is user credential data to be cached?
    //
    bool
    IsCacheUserData(void) const { return m_bCacheUserData; }
    void
    SetCacheUserData(bool bCacheUserData) { m_bCacheUserData = bCacheUserData; }

    // Time (in seconds) to wait between failed authentication attempts:
    //
    int
    GetHeldPeriod(void) const { return m_HeldPeriod; }
    void
    SetHeldPeriod(int HeldPeriod) { m_HeldPeriod = HeldPeriod; }

    // Time (in seconds) to await a response from the authenticator:
    //
    int
    GetAuthPeriod(void) const { return m_AuthPeriod; }
    void
    SetAuthPeriod(int AuthPeriod) { m_AuthPeriod = AuthPeriod; }

    // Time (in seconds) to wait before an EAPOL-Start is sent:
    //
    int
    GetStartPeriod(void) const { return m_StartPeriod; }
    void
    SetStartPeriod(int StartPeriod) { m_StartPeriod = StartPeriod; }

    // Maximum EAPOL-Start messages w/o response before assuming authicator
    // is not present:
    //
    int
    GetMaxStart(void) const { return m_MaxStart; }
    void
    SetMaxStart(int MaxStart) { m_MaxStart = MaxStart; }

    // Maximum authentication failures before assuming credentials are
    // incorrect:
    //
    int
    GetMaxAuthFailures(void) const { return m_MaxAuthFailures; }
    void
    SetMaxAuthFailures(int MaxAuthFailures) { m_MaxAuthFailures = MaxAuthFailures; }

    // Supplicant mode:
    //
    SupplicantMode_e
    GetSupplicantMode(void) const { return m_SupplicantMode; }
    void
    SetSupplicantMode(SupplicantMode_e SupplicantMode) { m_SupplicantMode = SupplicantMode; }

    // Authentication mode:
    //
    AuthMode_e
    GetAuthMode(void) const { return m_AuthMode; }
    void
    SetAuthMode(AuthMode_e AuthMode) { m_AuthMode = AuthMode; }

    // EAP Authentication configuration:
    //
    int
    GetEapConfigCount(void) const { return m_EapConfigs.size(); }
    const WLANProfileEapConfig_t *
    GetEapConfig(int IX) const;
    DWORD
    AddEapConfig(const WLANProfileEapConfig_t &EapConfig);
    DWORD
    SetEapConfig(int IX, const WLANProfileEapConfig_t &EapConfig);
    DWORD
    RemoveEapConfig(int IX);

  // XML encoding/decoding:

    // XML tags:
    //
    static const WCHAR XmlTagName[];
    static const WCHAR XmlNamespace[];

    // Encodes the object into a DOM element:
    //
    HRESULT
    EncodeToXml(
        litexml::XmlElement_t **ppRoot,
        const TCHAR            *pNamespace = XmlNamespace) const;

    // Initializes the object from the specified DOM element:
    //
    HRESULT
    DecodeFromXml(
        const litexml::XmlElement_t &Root);
    
};

};
};

#endif /* if (WLAN_VERSION > 0) */
#endif /* _DEFINED_WLANProfileOneX_t_ */
// ----------------------------------------------------------------------------
