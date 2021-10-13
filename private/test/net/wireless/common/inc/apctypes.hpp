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
// Definitions and declarations for the Access Point Controller data-types.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_APCTypes_h_
#define _DEFINED_APCTypes_h_
#pragma once

#ifndef DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <windows.h>

// Maximum SSID-name length (ASCII charcters):
//
#define MAX_SSID_CHARS  32

namespace litexml
{
    class XmlBaseElement_t;
    class XmlElement_t;
};

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Bit-map describing the capabilities provided by an AP:
//
enum APCapability_e
{
    APCapsWEP_802_1X    = 0x0001 // Wired Equivalent Privacy with Radius
   ,APCapsWPA           = 0x0010 // Wireless Protected Access with Radius
   ,APCapsWPA_PSK       = 0x0020 // WPA with private shared key
   ,APCapsWPA_AES       = 0x0040 // WPA using Advanced Encryption Standard
   ,APCapsWPA2          = 0x0100 // Robust Security Network Associations (802.11i)
   ,APCapsWPA2_PSK      = 0x0200 // RSNA with PSK
   ,APCapsWPA2_TKIP     = 0x0400 // WPA2 using TKIP encryption
   ,APCapsWMM           = 0x1000 // Wireless Multi-Media (QoS)
   ,AllAPCapabilities   = 0x1FFF
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

HRESULT
AddXmlCapabilitiesElement(
    litexml::XmlElement_t  *pRoot,
    const WCHAR            *pName,
    int                     Value,
    litexml::XmlElement_t **ppElem = NULL);

HRESULT
AddXmlCapabilitiesAttribute(
    litexml::XmlElement_t *pElem,
    const WCHAR           *pName,
    int                    Value);

HRESULT
GetXmlCapabilitiesElement(
    const litexml::XmlElement_t  &Root,
    const WCHAR                  *pName,
    int                          *pValue,
    const litexml::XmlElement_t **ppElem = NULL);

HRESULT
GetXmlCapabilitiesAttribute(
    const litexml::XmlBaseElement_t &Elem,
    const WCHAR                     *pName,
    int                             *pValue);

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
};

const TCHAR *
APAuthMode2String(
    APAuthMode_e AuthMode);

APAuthMode_e
String2APAuthMode(
    const TCHAR *AuthString);

HRESULT
AddXmlAuthModeElement(
    litexml::XmlElement_t  *pRoot,
    const WCHAR            *pName,
    APAuthMode_e            Value,
    litexml::XmlElement_t **ppElem = NULL);

HRESULT
AddXmlAuthModeAttribute(
    litexml::XmlElement_t *pElem,
    const WCHAR           *pName,
    APAuthMode_e           Value);

HRESULT
GetXmlAuthModeElement(
    const litexml::XmlElement_t  &Root,
    const WCHAR                  *pName,
    APAuthMode_e                 *pValue,
    const litexml::XmlElement_t **ppElem = NULL);

HRESULT
GetXmlAuthModeAttribute(
    const litexml::XmlBaseElement_t &Elem,
    const WCHAR                     *pName,
    APAuthMode_e                    *pValue);

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
};

const TCHAR *
APCipherMode2String(
    APCipherMode_e CipherMode);

APCipherMode_e
String2APCipherMode(
    const TCHAR *CipherString);

HRESULT
AddXmlCipherModeElement(
    litexml::XmlElement_t  *pRoot,
    const WCHAR            *pName,
    APCipherMode_e          Value,
    litexml::XmlElement_t **ppElem = NULL);

HRESULT
AddXmlCipherModeAttribute(
    litexml::XmlElement_t *pElem,
    const WCHAR           *pName,
    APCipherMode_e         Value);

HRESULT
GetXmlCipherModeElement(
    const litexml::XmlElement_t  &Root,
    const WCHAR                  *pName,
    APCipherMode_e               *pValue,
    const litexml::XmlElement_t **ppElem = NULL);

HRESULT
GetXmlCipherModeAttribute(
    const litexml::XmlBaseElement_t &Elem,
    const WCHAR                     *pName,
    APCipherMode_e                  *pValue);

// ----------------------------------------------------------------------------
//
// EAP (Extensible Authentication Protocol) authentication ciphers:
//
enum APEapAuthMode_e
{
    APEapAuthTLS  = 0  // Transport Layer Security
   ,APEapAuthMD5  = 1  // Message Digest algorithm 5
   ,APEapAuthPEAP = 2  // Protected Extensible Authentication Protocol
   ,APEapAuthEAPSIM =3 // EapSim protocol
   ,NumberAPEapAuthModes = 4
};

const TCHAR *
APEapAuthMode2String(
    APEapAuthMode_e EapAuthMode);

APEapAuthMode_e
String2APEapAuthMode(
    const TCHAR *EapAuthString);

HRESULT
AddXmlEapAuthModeElement(
    litexml::XmlElement_t  *pRoot,
    const WCHAR            *pName,
    APEapAuthMode_e         Value,
    litexml::XmlElement_t **ppElem = NULL);

HRESULT
AddXmlEapAuthModeAttribute(
    litexml::XmlElement_t *pElem,
    const WCHAR           *pName,
    APEapAuthMode_e        Value);

HRESULT
GetXmlEapAuthModeElement(
    const litexml::XmlElement_t  &Root,
    const WCHAR                  *pName,
    APEapAuthMode_e              *pValue,
    const litexml::XmlElement_t **ppElem = NULL);

HRESULT
GetXmlEapAuthModeAttribute(
    const litexml::XmlBaseElement_t &Elem,
    const WCHAR                     *pName,
    APEapAuthMode_e                 *pValue);

// ----------------------------------------------------------------------------
//
// Validate the specified combination of security modes given an AP's
// stated capabilities:
//
bool
ValidateSecurityModes(
    APAuthMode_e   AuthMode,
    APCipherMode_e CipherMode,
    int            Capabilities = (int)AllAPCapabilities);

};
};
#endif /* _DEFINED_APCTypes_h_ */
// ----------------------------------------------------------------------------
