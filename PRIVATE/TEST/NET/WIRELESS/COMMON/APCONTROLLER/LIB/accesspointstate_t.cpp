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
// Implementation of the AccessPointState_t classes.
//
// ----------------------------------------------------------------------------

#include "AccessPointState_t.hpp"
#include "APControlClient_t.hpp"
#include "APControlData_t.hpp"
#include "WiFUtils.hpp"

#include <assert.h>

// Define this if you want LOTS of debug output:
//#define EXTRA_DEBUG 1

using namespace ce::qa;

// ----------------------------------------------------------------------------
//  
// Clears/initializes the state information.
//
void
AccessPointState_t::
Clear(void)
{
    memset(this, 0, sizeof(*this));
    m_StructSize = sizeof(*this);

    m_AuthMode   = APAuthOpen;
    m_CipherMode = APCipherClearText;

    m_BSsid.Clear();
    m_WEPKeys.Clear();
}

// ----------------------------------------------------------------------------
//  
// Gets or sets the vendor name.
//
HRESULT
AccessPointState_t::
GetVendorName(
  __out_ecount(BufferChars) char *pBuffer,
    int                            BufferChars,
    TranslationFailuresLogMethod_e LogFailures) const
{
    const char *pFieldName;
    if (LogFailures == TranslationFailuresNotLogged)
         pFieldName = NULL;
    else pFieldName = "Vendor Name";
    return WiFUtils::ConvertString(pBuffer, m_VendorName,
                                   BufferChars, pFieldName,
                                   COUNTOF(m_VendorName), CP_UTF8);
}

HRESULT
AccessPointState_t::
GetVendorName(
  __out_ecount(BufferChars) WCHAR *pBuffer,
    int                             BufferChars,
    TranslationFailuresLogMethod_e  LogFailures) const
{
    const char *pFieldName;
    if (LogFailures == TranslationFailuresNotLogged)
         pFieldName = NULL;
    else pFieldName = "Vendor Name";
    return WiFUtils::ConvertString(pBuffer, m_VendorName,
                                   BufferChars, pFieldName,
                                   COUNTOF(m_VendorName), CP_UTF8);
}

HRESULT
AccessPointState_t::
SetVendorName(
    const char *pValue,
    int          ValueChars,
    TranslationFailuresLogMethod_e LogFailures)
{
    const char *pFieldName;
    if (LogFailures == TranslationFailuresNotLogged)
         pFieldName = NULL;
    else pFieldName = "Vendor Name";
    WCHAR buffer[COUNTOF(m_VendorName)+1];
    HRESULT hr = WiFUtils::ConvertString(buffer,  pValue,
                                 COUNTOF(buffer), pFieldName,
                                                   ValueChars, CP_UTF8);
    if (SUCCEEDED(hr))
    {
        memcpy(m_VendorName, buffer, sizeof(m_VendorName));
    }
    return hr;
}

HRESULT
AccessPointState_t::
SetVendorName(
    const WCHAR *pValue,
    int           ValueChars,
    TranslationFailuresLogMethod_e LogFailures)
{
    const char *pFieldName;
    if (LogFailures == TranslationFailuresNotLogged)
         pFieldName = NULL;
    else pFieldName = "Vendor Name";
    WCHAR buffer[COUNTOF(m_VendorName)+1];
    HRESULT hr = WiFUtils::ConvertString(buffer,  pValue,
                                 COUNTOF(buffer), pFieldName,
                                                   ValueChars, CP_UTF8);
    if (SUCCEEDED(hr))
    {
        memcpy(m_VendorName, buffer, sizeof(m_VendorName));
    }
    return hr;
}

// ----------------------------------------------------------------------------
//  
// Gets or sets the model number.
//
HRESULT
AccessPointState_t::
GetModelNumber(
  __out_ecount(BufferChars) char *pBuffer,
    int                            BufferChars,
    TranslationFailuresLogMethod_e LogFailures) const
{
    const char *pFieldName;
    if (LogFailures == TranslationFailuresNotLogged)
         pFieldName = NULL;
    else pFieldName = "Model Number";
    return WiFUtils::ConvertString(pBuffer, m_ModelNumber,
                                   BufferChars, pFieldName,
                                   COUNTOF(m_ModelNumber), CP_UTF8);
}

HRESULT
AccessPointState_t::
GetModelNumber(
  __out_ecount(BufferChars) WCHAR *pBuffer,
    int                             BufferChars,
    TranslationFailuresLogMethod_e  LogFailures) const
{
    const char *pFieldName;
    if (LogFailures == TranslationFailuresNotLogged)
         pFieldName = NULL;
    else pFieldName = "Model Number";
    return WiFUtils::ConvertString(pBuffer, m_ModelNumber,
                                   BufferChars, pFieldName,
                                   COUNTOF(m_ModelNumber), CP_UTF8);
}

HRESULT
AccessPointState_t::
SetModelNumber(
    const char *pValue,
    int          ValueChars,
    TranslationFailuresLogMethod_e LogFailures)
{
    const char *pFieldName;
    if (LogFailures == TranslationFailuresNotLogged)
         pFieldName = NULL;
    else pFieldName = "Model Number";
    WCHAR buffer[COUNTOF(m_ModelNumber)+1];
    HRESULT hr = WiFUtils::ConvertString(buffer,  pValue,
                                 COUNTOF(buffer), pFieldName,
                                                   ValueChars, CP_UTF8);
    if (SUCCEEDED(hr))
    {
        memcpy(m_ModelNumber, buffer, sizeof(m_ModelNumber));
    }
    return hr;
}

HRESULT
AccessPointState_t::
SetModelNumber(
    const WCHAR *pValue,
    int           ValueChars,
    TranslationFailuresLogMethod_e LogFailures)
{
    const char *pFieldName;
    if (LogFailures == TranslationFailuresNotLogged)
         pFieldName = NULL;
    else pFieldName = "Model Number";
    WCHAR buffer[COUNTOF(m_ModelNumber)+1];
    HRESULT hr = WiFUtils::ConvertString(buffer,  pValue,
                                 COUNTOF(buffer), pFieldName,
                                                   ValueChars, CP_UTF8);
    if (SUCCEEDED(hr))
    {
        memcpy(m_ModelNumber, buffer, sizeof(m_ModelNumber));
    }
    return hr;
}

// ----------------------------------------------------------------------------
//  
// Gets or sets the SSID.
//
HRESULT
AccessPointState_t::
GetSsid(
  __out_ecount(BufferChars) char *pBuffer,
    int                            BufferChars,
    TranslationFailuresLogMethod_e LogFailures) const
{
    const char *pFieldName;
    if (LogFailures == TranslationFailuresNotLogged)
         pFieldName = NULL;
    else pFieldName = "SSID";
    return WiFUtils::ConvertString(pBuffer, m_Ssid,
                                   BufferChars, pFieldName,
                                   COUNTOF(m_Ssid), CP_UTF8);
}

HRESULT
AccessPointState_t::
GetSsid(
  __out_ecount(BufferChars) WCHAR *pBuffer,
    int                             BufferChars,
    TranslationFailuresLogMethod_e  LogFailures) const
{
    const char *pFieldName;
    if (LogFailures == TranslationFailuresNotLogged)
         pFieldName = NULL;
    else pFieldName = "SSID";
    return WiFUtils::ConvertString(pBuffer, m_Ssid,
                                   BufferChars, pFieldName,
                                   COUNTOF(m_Ssid), CP_UTF8);
}

HRESULT
AccessPointState_t::
SetSsid(
    const char *pValue,
    int          ValueChars,
    TranslationFailuresLogMethod_e LogFailures)
{
    const char *pFieldName;
    if (LogFailures == TranslationFailuresNotLogged)
         pFieldName = NULL;
    else pFieldName = "SSID";
    WCHAR buffer[COUNTOF(m_Ssid)+1];
    HRESULT hr = WiFUtils::ConvertString(buffer,  pValue,
                                 COUNTOF(buffer), pFieldName,
                                                   ValueChars, CP_UTF8);
    if (SUCCEEDED(hr))
    {
        m_FieldFlags |= FieldMaskSsid;
        memcpy(m_Ssid, buffer, sizeof(m_Ssid));
    }
    return hr;
}

HRESULT
AccessPointState_t::
SetSsid(
    const WCHAR *pValue,
    int           ValueChars,
    TranslationFailuresLogMethod_e LogFailures)
{
    const char *pFieldName;
    if (LogFailures == TranslationFailuresNotLogged)
         pFieldName = NULL;
    else pFieldName = "SSID";
    WCHAR buffer[COUNTOF(m_Ssid)+1];
    HRESULT hr = WiFUtils::ConvertString(buffer,  pValue,
                                 COUNTOF(buffer), pFieldName,
                                                   ValueChars, CP_UTF8);
    if (SUCCEEDED(hr))
    {
        m_FieldFlags |= FieldMaskSsid;
        memcpy(m_Ssid, buffer, sizeof(m_Ssid));
    }
    return hr;
}

// ----------------------------------------------------------------------------
//  
// Gets or sets the Radius secret key.
//
HRESULT
AccessPointState_t::
GetRadiusSecret(
  __out_ecount(BufferChars) char *pBuffer,
    int                            BufferChars,
    TranslationFailuresLogMethod_e LogFailures) const
{
    const char *pFieldName;
    if (LogFailures == TranslationFailuresNotLogged)
         pFieldName = NULL;
    else pFieldName = "Radius Secret Key";
    return WiFUtils::ConvertString(pBuffer, m_RadiusSecret,
                                   BufferChars, pFieldName,
                                   COUNTOF(m_RadiusSecret), CP_UTF8);
}

HRESULT
AccessPointState_t::
GetRadiusSecret(
  __out_ecount(BufferChars) WCHAR *pBuffer,
    int                             BufferChars,
    TranslationFailuresLogMethod_e  LogFailures) const
{
    const char *pFieldName;
    if (LogFailures == TranslationFailuresNotLogged)
         pFieldName = NULL;
    else pFieldName = "Radius Secret Key";
    return WiFUtils::ConvertString(pBuffer, m_RadiusSecret,
                                   BufferChars, pFieldName,
                                   COUNTOF(m_RadiusSecret), CP_UTF8);
}

HRESULT
AccessPointState_t::
SetRadiusSecret(
    const char *pValue,
    int          ValueChars,
    TranslationFailuresLogMethod_e LogFailures)
{
    const char *pFieldName;
    if (LogFailures == TranslationFailuresNotLogged)
         pFieldName = NULL;
    else pFieldName = "Radius Secret Key";
    WCHAR buffer[COUNTOF(m_RadiusSecret)+1];
    HRESULT hr = WiFUtils::ConvertString(buffer,  pValue,
                                 COUNTOF(buffer), pFieldName,
                                                   ValueChars, CP_UTF8);
    if (SUCCEEDED(hr))
    {
        m_FieldFlags |= FieldMaskRadius;
        memcpy(m_RadiusSecret, buffer, sizeof(m_RadiusSecret));
    }
    return hr;
}

HRESULT
AccessPointState_t::
SetRadiusSecret(
    const WCHAR *pValue,
    int           ValueChars,
    TranslationFailuresLogMethod_e LogFailures)
{
    const char *pFieldName;
    if (LogFailures == TranslationFailuresNotLogged)
         pFieldName = NULL;
    else pFieldName = "Radius Secret Key";
    WCHAR buffer[COUNTOF(m_RadiusSecret)+1];
    HRESULT hr = WiFUtils::ConvertString(buffer,  pValue,
                                 COUNTOF(buffer), pFieldName,
                                                   ValueChars, CP_UTF8);
    if (SUCCEEDED(hr))
    {
        m_FieldFlags |= FieldMaskRadius;
        memcpy(m_RadiusSecret, buffer, sizeof(m_RadiusSecret));
    }
    return hr;
}

// ----------------------------------------------------------------------------
//  
// Gets or sets the PSK passphase.
//
HRESULT
AccessPointState_t::
GetPassphrase(
  __out_ecount(BufferChars) char *pBuffer,
    int                            BufferChars,
    TranslationFailuresLogMethod_e LogFailures) const
{
    const char *pFieldName;
    if (LogFailures == TranslationFailuresNotLogged)
         pFieldName = NULL;
    else pFieldName = "PSK Passphrase";
    return WiFUtils::ConvertString(pBuffer, m_Passphrase,
                                   BufferChars, pFieldName,
                                   COUNTOF(m_Passphrase), CP_UTF8);
}

HRESULT
AccessPointState_t::
GetPassphrase(
  __out_ecount(BufferChars) WCHAR *pBuffer,
    int                             BufferChars,
    TranslationFailuresLogMethod_e  LogFailures) const
{
    const char *pFieldName;
    if (LogFailures == TranslationFailuresNotLogged)
         pFieldName = NULL;
    else pFieldName = "PSK Passphrase";
    return WiFUtils::ConvertString(pBuffer, m_Passphrase,
                                   BufferChars, pFieldName,
                                   COUNTOF(m_Passphrase), CP_UTF8);
}

HRESULT
AccessPointState_t::
SetPassphrase(
    const char *pValue,
    int          ValueChars,
    TranslationFailuresLogMethod_e LogFailures)
{
    const char *pFieldName;
    if (LogFailures == TranslationFailuresNotLogged)
         pFieldName = NULL;
    else pFieldName = "PSK Passphrase";
    WCHAR buffer[COUNTOF(m_Passphrase)+1];
    HRESULT hr = WiFUtils::ConvertString(buffer,  pValue,
                                 COUNTOF(buffer), pFieldName,
                                                   ValueChars, CP_UTF8);
    if (SUCCEEDED(hr))
    {
        m_FieldFlags |= FieldMaskPassphrase;
        memcpy(m_Passphrase, buffer, sizeof(m_Passphrase));
    }
    return hr;
}

HRESULT
AccessPointState_t::
SetPassphrase(
    const WCHAR *pValue,
    int           ValueChars,
    TranslationFailuresLogMethod_e LogFailures)
{
    const char *pFieldName;
    if (LogFailures == TranslationFailuresNotLogged)
         pFieldName = NULL;
    else pFieldName = "PSK Passphrase";
    WCHAR buffer[COUNTOF(m_Passphrase)+1];
    HRESULT hr = WiFUtils::ConvertString(buffer,  pValue,
                                 COUNTOF(buffer), pFieldName,
                                                   ValueChars, CP_UTF8);
    if (SUCCEEDED(hr))
    {
        m_FieldFlags |= FieldMaskPassphrase;
        memcpy(m_Passphrase, buffer, sizeof(m_Passphrase));
    }
    return hr;
}

// ----------------------------------------------------------------------------
//
// Compares this object to another; compares each of the flagged
// fields and updates the returns flags indicating which fields differ:
//
int
AccessPointState_t::
CompareFields(int Flags, const AccessPointState_t &peer) const
{
    if (Flags & (int)FieldMaskSsid)
    {
        if (_wcsnicmp(m_Ssid,
                 peer.m_Ssid, COUNTOF(m_Ssid)) == 0)
            Flags &= ~(int)FieldMaskSsid;
    }

    if (Flags & (int)FieldMaskBSsid)
    {
        if (memcmp(m_BSsid.m_Addr,
              peer.m_BSsid.m_Addr, sizeof(m_BSsid.m_Addr)) == 0)
            Flags &= ~(int)FieldMaskBSsid;
    }

    if (Flags & (int)FieldMaskCapabilities)
    {
        if (m_CapsImplemented == peer.m_CapsImplemented
         && m_CapsEnabled     == peer.m_CapsEnabled)
            Flags &= ~(int)FieldMaskCapabilities;
    }

    if (Flags & (int)FieldMaskRadioState)
    {
        if (m_fRadioState == peer.m_fRadioState)
            Flags &= ~(int)FieldMaskRadioState;
    }

    if (Flags & (int)FieldMaskSecurityMode)
    {
        if (m_AuthMode   == peer.m_AuthMode
         && m_CipherMode == peer.m_CipherMode)
            Flags &= ~(int)FieldMaskSecurityMode;
    }

    if (Flags & (int)FieldMaskRadius)
    {
        if (m_RadiusServer == peer.m_RadiusServer
         && m_RadiusPort   == peer.m_RadiusPort
         && wcsncmp(m_RadiusSecret,
               peer.m_RadiusSecret, COUNTOF(m_RadiusSecret)) == 0)
            Flags &= ~(int)FieldMaskRadius;
    }

    if ((Flags & (int)FieldMaskWEPKeys)
     && (m_WEPKeys.m_KeyIndex == peer.m_WEPKeys.m_KeyIndex))
    {
        Flags &= ~(int)FieldMaskWEPKeys;
        for (int kx = 0 ; 4 > kx ; ++kx)
        {
            int peerKeySize = static_cast<int>(peer.m_WEPKeys.m_Size[kx]);
            if (m_WEPKeys.ValidKeySize(peerKeySize))
            {
                int thisKeySize = static_cast<int>(m_WEPKeys.m_Size[kx]);
                if (thisKeySize == peerKeySize)
                {
                    if (memcmp(m_WEPKeys.m_Material[kx],
                          peer.m_WEPKeys.m_Material[kx], thisKeySize) == 0)
                        continue;
                }
                Flags |= (int)FieldMaskWEPKeys;
                break;
            }
        }
    }

    if (Flags & (int)FieldMaskPassphrase)
    {
        if (wcsncmp(m_Passphrase,
               peer.m_Passphrase, COUNTOF(m_Passphrase)) == 0)
            Flags &= ~(int)FieldMaskPassphrase;
    }

    return Flags;
}

// ----------------------------------------------------------------------------
//
// Extracts a state object from the specified message.
//
DWORD
AccessPointState_t::
DecodeMessage(
    DWORD        Result,
    const BYTE *pMessageData,
    DWORD        MessageDataBytes)
{
    int dataBytes = APControlData_t::CalculateDataBytes(MessageDataBytes);
    if (0 > dataBytes)
    {
        LogError(TEXT("Message type unknown: packet size only %d bytes"),
                 MessageDataBytes);
        return ERROR_INVALID_DATA;
    }
    
    const APControlData_t *pPacket = (const APControlData_t *)pMessageData;
    if (AccessPointStateDataType != pPacket->m_DataType)
    {
        LogError(TEXT("Unknown Access Point message type 0x%X"), 
                 (unsigned)pPacket->m_DataType);
        return ERROR_INVALID_DATA;
    }

    if (sizeof(*this) < dataBytes)
    {
        LogDebug(TEXT("%d-byte Access Point message truncated to %d bytes"),
                 dataBytes, sizeof(*this));
        dataBytes = sizeof(*this);
    }            

    Clear();
    memcpy(this, pPacket->m_Data, dataBytes);
    
    return Result;
}

// ----------------------------------------------------------------------------
//      
// Allocates a packet and initializes it to hold the current object.
//
DWORD
AccessPointState_t::
EncodeMessage(
    DWORD   Result,
    BYTE **ppMessageData,
    DWORD  *pMessageDataBytes) const
{
   *ppMessageData = NULL;
    *pMessageDataBytes = 0;

    int messageBytes = APControlData_t::CalculateMessageBytes(GetStructSize());

    APControlData_t *pPacket = (APControlData_t *) malloc(messageBytes);
    if (NULL == pPacket)
    {
        LogError(TEXT("Can't allocate %d bytes for Access Point message"),
                 messageBytes);
        return ERROR_OUTOFMEMORY;
    }
              
    pPacket->m_DataType = AccessPointStateDataType;
    memcpy(pPacket->m_Data, this, GetStructSize());

  *ppMessageData = (BYTE *)pPacket;
   *pMessageDataBytes = messageBytes;
       
    return Result;
}

// ----------------------------------------------------------------------------
//      
// Uses the specified client to send the message and await a response.
//
DWORD
AccessPointState_t::
SendMessage(
    DWORD CommandType,
    APControlClient_t  *pClient,
    AccessPointState_t *pResponse,
    ce::tstring        *pErrorResponse) const
{
    DWORD result = ERROR_SUCCESS;
    
    BYTE *pCommandData      = NULL;
    DWORD  commandDataBytes = 0;
    BYTE *pReturnData       = NULL;
    DWORD  returnDataBytes  = 0;
    
    pErrorResponse->clear();
    
#ifdef EXTRA_DEBUG
    LogDebug(TEXT("Sending access-point command %d to \"%s:%s\""),
             CommandType, pClient->GetServerHost(),
                          pClient->GetServerPort());
#endif

    // Packetize the command data (if any).
    result = EncodeMessage(result, &pCommandData,
                                    &commandDataBytes);
    if (ERROR_SUCCESS == result)
    {
        // Send the command and wait for a response.
        DWORD remoteResult;
        result = pClient->SendPacket(CommandType, 
                                    pCommandData, 
                                     commandDataBytes, &remoteResult,
                                                      &pReturnData, 
                                                       &returnDataBytes);
        if (ERROR_SUCCESS == result)
        {
            // Unpacketize the response message (if any).
            if (ERROR_SUCCESS == remoteResult)
            {
                result = pResponse->DecodeMessage(remoteResult,
                                                 pReturnData, 
                                                  returnDataBytes); 
            }
            else
            {
                result = APControlData_t::DecodeMessage(remoteResult,
                                                       pReturnData, 
                                                        returnDataBytes, 
                                                       pErrorResponse);
            }
        }
    }
    
    if (NULL != pReturnData)
    {
        free(pReturnData);
    }
    if (NULL != pCommandData)
    {
        free(pCommandData);
    }
    
    if (ERROR_SUCCESS != result && 0 == pErrorResponse->length())
    {
        pErrorResponse->assign(Win32ErrorText(result));
    }
    
    return result;
}

// ----------------------------------------------------------------------------
