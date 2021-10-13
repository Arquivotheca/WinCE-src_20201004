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
// Definitions and declarations for the AccessPointState_t classes.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_AccessPointState_t_
#define _DEFINED_AccessPointState_t_
#pragma once

#include <APController_t.hpp>
#include <WiFUtils.hpp>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Current and/or requested state of an Access-Point device.
//
class APControlClient_t;
class AccessPointState_t
{
protected:
    INT16 m_StructSize;

public:

    // Character-array sizes:
    enum { MaxVendorNameChars   =  32 };
    enum { MaxModelNumberChars  =  32 };
    enum { MaxSsidChars         =  32 };
    enum { MaxRadiusSecretChars = 256 };
    enum { MaxPassphraseChars   =  64 };

    // Indicates whether the string-access methods should show errors
    // if the character-translation or string-copy fails:
    enum TranslationFailuresLogMethod_e { 
         TranslationFailuresNotLogged = 0,
         TranslationFailuresAreLogged = 1
    };
    
private:

    // AP vendor name and model number:
    WCHAR m_VendorName [MaxVendorNameChars];
    WCHAR m_ModelNumber[MaxModelNumberChars];

    // SSID (WLAN Service Set Identifer / Name):
    WCHAR m_Ssid[MaxSsidChars];

    // BSSID (Basic Service Set Identifier):
    MACAddr_t m_BSsid;
    
    // AP capability flags:
    UINT16 m_CapsImplemented;
    UINT16 m_CapsEnabled;

    // Radio's on/off state:
    BYTE m_fRadioState;
    
    // Authentication and encryption modes:
    INT8 m_AuthMode;
    INT8 m_CipherMode;

    // RADIUS Server information:
    UINT32 m_RadiusServer;
    INT16  m_RadiusPort;
    WCHAR  m_RadiusSecret[MaxRadiusSecretChars];

    // WEP (Wired Equivalent Privacy) keys:
    WEPKeys_t m_WEPKeys;

    // Passphrase:
    WCHAR m_Passphrase[MaxPassphraseChars];

    // Flags indicating which which values have been set or modified:
    UINT16 m_FieldFlags;

public:

    AccessPointState_t(void) { Clear(); }
    
    // Clears/initializes the state information:
    void
    Clear(void);
    
    // AP vendor name:
    HRESULT
    GetVendorName(
      __out_ecount(BufferChars) char *pBuffer,
        int                            BufferChars,
        TranslationFailuresLogMethod_e LogFailures = TranslationFailuresAreLogged) const;
    HRESULT
    GetVendorName(
      __out_ecount(BufferChars) WCHAR *pBuffer,
        int                             BufferChars,
        TranslationFailuresLogMethod_e  LogFailures = TranslationFailuresAreLogged) const;
    HRESULT
    SetVendorName(
        const char *pValue, 
        int          ValueChars = -1,
        TranslationFailuresLogMethod_e LogFailures = TranslationFailuresNotLogged);
    HRESULT
    SetVendorName(
        const WCHAR *pValue, 
        int           ValueChars = -1,
        TranslationFailuresLogMethod_e LogFailures = TranslationFailuresNotLogged);

    // AP Model number:
    HRESULT
    GetModelNumber(
      __out_ecount(BufferChars) char *pBuffer,
        int                            BufferChars,
        TranslationFailuresLogMethod_e LogFailures = TranslationFailuresAreLogged) const;
    HRESULT
    GetModelNumber(
      __out_ecount(BufferChars) WCHAR *pBuffer,
        int                             BufferChars,
        TranslationFailuresLogMethod_e  LogFailures = TranslationFailuresAreLogged) const;
    HRESULT
    SetModelNumber(
        const char *pValue, 
        int          ValueChars = -1,
        TranslationFailuresLogMethod_e LogFailures = TranslationFailuresNotLogged);
    HRESULT
    SetModelNumber(
        const WCHAR *pValue, 
        int           ValueChars = -1,
        TranslationFailuresLogMethod_e LogFailures = TranslationFailuresNotLogged);
    
    // SSID (WLAN Service Set Identifer / Name):
    HRESULT
    GetSsid(
      __out_ecount(BufferChars) char *pBuffer,
        int                            BufferChars,
        TranslationFailuresLogMethod_e LogFailures = TranslationFailuresAreLogged) const;
    HRESULT
    GetSsid(
      __out_ecount(BufferChars) WCHAR *pBuffer,
        int                             BufferChars,
        TranslationFailuresLogMethod_e  LogFailures = TranslationFailuresAreLogged) const;
    HRESULT
    SetSsid(
        const char *pValue,
        int          ValueChars = -1,
        TranslationFailuresLogMethod_e LogFailures = TranslationFailuresNotLogged);
    HRESULT
    SetSsid(
        const WCHAR *pValue, 
        int          ValueChars = -1,
        TranslationFailuresLogMethod_e LogFailures = TranslationFailuresNotLogged);
            
    // BSSID (Basic Service Set Identifier):
    const MACAddr_t &
    GetBSsid(void) const { return m_BSsid; }
    void
    SetBSsid(const MACAddr_t &BSsid)
    {
        m_BSsid = BSsid;
        m_FieldFlags |= FieldMaskBSsid;
    }
    
    // AP capability flags:
    int
    GetCapabilitiesImplemented(void) const { return (int)m_CapsImplemented; }
    void
    SetCapabilitiesImplemented(int Value)
    {
        m_CapsImplemented = (UINT16)Value;
        m_FieldFlags |= FieldMaskCapabilities;
    }
    int
    GetCapabilitiesEnabled(void) const { return (int)m_CapsEnabled; }
    void
    SetCapabilitiesEnabled(int Value)
    {
        m_CapsEnabled = (UINT16)Value;
        m_FieldFlags |= FieldMaskCapabilities;
    }

    // Radio's on/off state:
    bool
    IsRadioOn(void) const { return m_fRadioState != 0; }
    void
    SetRadioState(bool Value)
    {
        m_fRadioState = Value? 1:0;
        m_FieldFlags |= FieldMaskRadioState;
    }
    
    // Authentication and encryption modes:
    APAuthMode_e
    GetAuthMode(void) const { return (APAuthMode_e)m_AuthMode; }
    const TCHAR *
    GetAuthName(void) const { return APAuthMode2String(GetAuthMode()); }
    APCipherMode_e
    GetCipherMode(void) const { return (APCipherMode_e)m_CipherMode; }
    const TCHAR *
    GetCipherName(void) const { return APCipherMode2String(GetCipherMode()); }
    void
    SetAuthMode(APAuthMode_e Value)
    {
        m_AuthMode = (INT8)Value;
        m_FieldFlags |= FieldMaskSecurityMode;
    }
    void
    SetCipherMode(APCipherMode_e Value)
    {
        m_CipherMode = (INT8)Value;
        m_FieldFlags |= FieldMaskSecurityMode;
    }
    
    // RADIUS Server information:
    DWORD
    GetRadiusServer(void) const { return (DWORD)m_RadiusServer; }
    void
    SetRadiusServer(DWORD Value)
    {
        m_RadiusServer = (UINT32)Value;
        m_FieldFlags |= FieldMaskRadius;
    }
    
    int
    GetRadiusPort(void) const { return (int)m_RadiusPort; }
    void
    SetRadiusPort(int Value)
    {
        m_RadiusPort = (UINT16)Value;
        m_FieldFlags |= FieldMaskRadius;
    }

    HRESULT
    GetRadiusSecret(
      __out_ecount(BufferChars) char *pBuffer,
        int                            BufferChars,
        TranslationFailuresLogMethod_e LogFailures = TranslationFailuresAreLogged) const;
    HRESULT
    GetRadiusSecret(
      __out_ecount(BufferChars) WCHAR *pBuffer,
        int                             BufferChars,
        TranslationFailuresLogMethod_e  LogFailures = TranslationFailuresAreLogged) const;
    HRESULT
    SetRadiusSecret(
        const char *pValue, 
        int          ValueChars = -1,
        TranslationFailuresLogMethod_e LogFailures = TranslationFailuresNotLogged);
    HRESULT
    SetRadiusSecret(
        const WCHAR *pValue, 
        int           ValueChars = -1,
        TranslationFailuresLogMethod_e LogFailures = TranslationFailuresNotLogged);
    
    // WEP (Wired Equivalent Privacy) keys:
    const WEPKeys_t &
    GetWEPKeys(void) const { return m_WEPKeys; }
    void
    SetWEPKeys(
    const WEPKeys_t &Value)
    {
        m_WEPKeys = Value;
        m_FieldFlags |= FieldMaskWEPKeys;
    }
    
    // Passphrase:
    HRESULT
    GetPassphrase(
      __out_ecount(BufferChars) char *pBuffer,
        int                            BufferChars,
        TranslationFailuresLogMethod_e LogFailures = TranslationFailuresAreLogged) const;
    HRESULT
    GetPassphrase(
      __out_ecount(BufferChars) WCHAR *pBuffer,
        int                             BufferChars,
        TranslationFailuresLogMethod_e  LogFailures = TranslationFailuresAreLogged) const;
    HRESULT
    SetPassphrase(
        const char *pValue, 
        int          ValueChars = -1,
        TranslationFailuresLogMethod_e LogFailures = TranslationFailuresNotLogged);
    HRESULT
    SetPassphrase(
        const WCHAR *pValue, 
        int           ValueChars = -1,
        TranslationFailuresLogMethod_e LogFailures = TranslationFailuresNotLogged);

    // Flags indicating which which values have been set or modified:
    enum FieldMask_e {
        FieldMaskSsid         = 0x0001
       ,FieldMaskBSsid        = 0x0002
       ,FieldMaskCapabilities = 0x0004
       ,FieldMaskRadioState   = 0x0008
       ,FieldMaskSecurityMode = 0x0010
       ,FieldMaskRadius       = 0x0020
       ,FieldMaskWEPKeys      = 0x0040
       ,FieldMaskPassphrase   = 0x0080
       ,FieldMaskAll          = 0xFFFF
    };
    int  GetFieldFlags(void) const { return m_FieldFlags; }
    void SetFieldFlags(int Flags) { m_FieldFlags = Flags; }

    // Compares this object to another; compares each of the flagged
    // fields and updates the returns flags indicating which fields differ:
    int CompareFields(int Flags, const AccessPointState_t &peer) const;

    // Gets the size (in bytes) of the structure.
    int GetStructSize(void) const { return m_StructSize; }

  // APControl message-formatting and communication:

    // Message-data type:
    enum { AccessPointStateDataType = 0xAC };

    // Extracts a state object from the specified message.
    DWORD
    DecodeMessage(
        DWORD        Result,
        const BYTE *pMessageData,
        DWORD        MessageDataBytes);
    
    // Allocates a packet and initializes it to hold the current object:
    DWORD
    EncodeMessage(
        DWORD Result,
        BYTE  **ppMessageData,
        DWORD   *pMessageDataBytes) const;

    // Uses the specified client to send the message and await a response:
    DWORD
    SendMessage(
        DWORD CommandType,
        APControlClient_t  *pClient,
        AccessPointState_t *pResponse,
        ce::tstring        *pErrorResponse) const;
};

};
};
#endif /* _DEFINED_AccessPointState_t_ */
// ----------------------------------------------------------------------------
