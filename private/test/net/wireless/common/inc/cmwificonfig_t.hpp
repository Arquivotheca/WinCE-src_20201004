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
// Definitions and declarations for the CMWiFiConfig_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_CMWiFiConfig_t_
#define _DEFINED_CMWiFiConfig_t_
#pragma once

#include "CMWiFiTypes.hpp"
#if (CMWIFI_VERSION > 0)

#include <RmNet.h>
#include <CMNet.h>
#include <CmCspWiFi.h>
#include <MemBuffer_t.hpp>

#include <WLANProfile_t.hpp>

namespace ce {
namespace qa {

#ifdef UNDER_CE
// Profile for Eap User Data
typedef struct _PM_EAP_USER_CRED_PROFILE {
    EAP_METHOD_TYPE eapMethodType;
    DWORD cbCredBlobLength;
    BYTE  CredBlob[1];
} PM_EAP_USER_CRED_PROFILE, *PPM_EAP_USER_CRED_PROFILE;
#endif 

// ----------------------------------------------------------------------------
//
// CMWiFiConfig_t contains the Connection Manager and CSP WiFi connection
// structures and a WLAN Profile as one object. Any modifications to one
// will automatically be represented in the others.
//
class CMWiFiConfig_t
{
public:

    // Construction and destruction:
    //
    CMWiFiConfig_t(void) { m_EapCredStr = NULL; Clear(); }

    ~CMWiFiConfig_t(void) ;
    void
    Clear(void);

    // Copy and assignment:
    //
    CMWiFiConfig_t(const CMWiFiConfig_t &rhs);
    CMWiFiConfig_t &operator = (const CMWiFiConfig_t &rhs);

    // Determines whether the object contains a valid config:
    //
    bool
    IsValid(void) const { return m_Connection.Length() != 0; }

  // Accessors:

    // Gets or sets the connection name:
    //
    const WCHAR *
    GetName(void) const;
    DWORD
    SetName(const WCHAR *pConnName);

    // Gets or sets the CM connection:
    //
    const CM_CONNECTION_CONFIG &
    GetConnection(void) const;
    DWORD
    SetConnection(const CM_CONNECTION_CONFIG &Connection);

    // Gets or sets the specified connection characteristic:
    //
    DWORD
    GetCharacteristic(CM_CHARACTERISTIC Character) const;
    DWORD
    SetCharacteristic(CM_CHARACTERISTIC Character, DWORD NewValue);

    // Gets or sets the CSP WiFi type-specific info structure:
    //
    const CspWiFiSpecificInfo &
    GetCspWiFiInfo(void) const;
    DWORD
    SetCspWiFiInfo(const CspWiFiSpecificInfo &CspWiFiInfo);

    // Gets or sets the CSP-WiFi "interface specific" flag and GUID:
    //    
    bool
    IsInterfaceSpecific(void) const;
    GUID
    GetInterfaceGuid(void) const;
    DWORD
    SetInterfaceGuid(
        BOOL InterfaceSpecific,        
        GUID InterfaceGuid);

    // Gets or sets the "on-demand" flag:
    //
    BOOL
    GetOnDemand(void) const;
    DWORD
    SetOnDemand(BOOL NewValue);

    // Gets or sets the WLAN profile:
    //
    const WLANProfile_t &
    GetProfile(void) const;
    DWORD
    SetProfile(const WLANProfile_t &Profile);

    LPWSTR 
    GetEapCred(void) { return m_EapCredStr; };

    DWORD
    SetEapCred(LPWSTR EapCredStr) { m_EapCredStr = EapCredStr; return NO_ERROR; };

    // Logs the connection information:
    //
    void
    Log(
        void       (*LogFunc)(const TCHAR *pFormat, ...),
        bool         Verbose     = true,  // logs all available info
        const WCHAR *pLinePrefix = NULL);

  // Input/Output:

    // Adds the new connection config to Connection Manafer:
    //
    DWORD
    WriteToCM(void) const;

    // Reads the names connection config from Connection Manager:
    //
    DWORD
    ReadFromCM(const WCHAR *pConnName);

    // Writes the updated connection config back into Connection Manager:
    //
    DWORD
    UpdateToCM(CM_CONFIG_OPTION UpdateImmediacy = CMCO_IMMEDIATE_APPLY) const;

private:

    // CM Connection:
    //
    MemBuffer_t m_Connection;

    // WLAN profile:
    //
    WLANProfile_t m_Profile;

    LPWSTR m_EapCredStr;
    
};

};
};

#endif /* if (CMWIFI_VERSION > 0) */
#endif /* _DEFINED_CMWiFiConfig_t_ */
// ----------------------------------------------------------------------------
