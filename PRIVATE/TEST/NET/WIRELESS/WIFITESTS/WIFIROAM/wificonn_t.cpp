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
// Implementation of the WiFiConn_t class.
//
// ----------------------------------------------------------------------------

#include "WiFiConn_t.hpp"

#include <Utils.hpp>

#include <assert.h>
#include <CUIOAdapter.h>
#include <inc/sync.hxx>

using namespace ce::qa;

// Syncronization object:
static ce::critical_section s_WiFiConnLocker;

// Cached interface information:
static WiFiConn_t s_WiFiInterfaceInfo;

// ----------------------------------------------------------------------------
//
// Constructor.
//
WiFiConn_t::
WiFiConn_t(void)
{
    m_InterfaceName[0] = TEXT('\0');
    m_InterfaceMAC[0] = TEXT('\0');
    m_APSsid[0] = TEXT('\0');
    m_APMAC[0] = TEXT('\0');
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
WiFiConn_t::
~WiFiConn_t(void)
{
    // Nothing to do
}

// ----------------------------------------------------------------------------
//
// Gets all the current connection information for the specified
// WiFi interface.
// Does not modify the existing connection information if retrieval
// of the new status fails.
//
HRESULT
WiFiConn_t::
UpdateInterfaceInfo(
    IN const TCHAR *pInterfaceName)
{
    HRESULT hr = S_OK;
    DWORD result;

    static const MaxQueryBufferBytes = 512;
    DWORD queryBuffer[MaxQueryBufferBytes/sizeof(DWORD)];
    BYTE *pQuerybuffer = (BYTE *)queryBuffer;
    DWORD bufferSize, bytesReturned;
        
    // Clear out the old information.
    WiFiConn_t info;
    info.SetInterfaceName(pInterfaceName);

    // Open the interface.
    CUIOAdapter oUIO;
    if (!oUIO.Open(info.m_InterfaceName))
    {
        if (!oUIO.Open(info.m_InterfaceName, GENERIC_WRITE))
        {
            LogError(TEXT("Failed to associate %s adapter with NDISUIO"),
                     info.GetInterfaceName());
            return E_FAIL;
        }
    }

    // Cache the interface MAC information.
    ce::gate<ce::critical_section> interfaceLocker(s_WiFiConnLocker);
    if (_tcscmp(s_WiFiInterfaceInfo.GetInterfaceName(),
                               info.GetInterfaceName()) == 0)
    {
        info = s_WiFiInterfaceInfo;
    }
    else
    {
        bufferSize = MaxQueryBufferBytes;
        bytesReturned = 0;
        if (oUIO.QueryOID(OID_802_3_CURRENT_ADDRESS,
                          pQuerybuffer,
                          bufferSize,
                          bytesReturned,
                          info.m_InterfaceName))
        {
            info.SetInterfaceMACAddr(pQuerybuffer, bytesReturned);
            if (info.m_InterfaceMACAddr.IsValid())
            {
                LogDebug(TEXT("Adapter %s MAC: %s, bytesReturned=%u"),
                         info.GetInterfaceName(), 
                         info.GetInterfaceMAC(),
                         bytesReturned);
                s_WiFiInterfaceInfo = info;
            }
            else
            {
                LogError(TEXT("Got no MAC address for WiFi NIC \"%s\", bytesReturned=%u"),
                         info.GetInterfaceName(), bytesReturned);
                hr = E_FAIL;
            }
        }
        else
        {
            result = GetLastError();
            LogError(TEXT("Can't get MAC address for WiFi NIC \"%s\": %s"),
                     info.GetInterfaceName(), Win32ErrorText(result));
            hr = HRESULT_FROM_WIN32(result);
        }
    }
    interfaceLocker.leave();

    // Get the SSID of the associated Access Point.
    bufferSize = MaxQueryBufferBytes;
    bytesReturned = 0;
    if (oUIO.QueryOID(OID_802_11_SSID,
                      pQuerybuffer,
                      bufferSize,
                      bytesReturned,
                      info.m_InterfaceName))
    {
        NDIS_802_11_SSID *psSSID = (NDIS_802_11_SSID *)pQuerybuffer;
        if (psSSID->Ssid[0] && psSSID->SsidLength)
        {
            info.SetAPSsid(psSSID->Ssid, psSSID->SsidLength);
            LogDebug(TEXT("Current SSID: %s, bytesReturned=%u"),
                     info.GetAPSsid(), bytesReturned);
        }
        else
        {
            LogError(TEXT("Got no SSID for WiFi NIC \"%s\", bytesReturned=%u"),
                     info.GetInterfaceName(), bytesReturned);
            hr = E_FAIL;
        }
    }
    else
    {
        result = GetLastError();
        LogError(TEXT("Can't get SSID for WiFi NIC \"%s\": %s"),
                 info.GetInterfaceName(), Win32ErrorText(result));
        hr = HRESULT_FROM_WIN32(result);
    }

    // Get the MAC address of the associated Access Point.
    bufferSize = MaxQueryBufferBytes;
    bytesReturned = 0;
    if (oUIO.QueryOID(OID_802_11_BSSID,
                      pQuerybuffer,
                      bufferSize,
                      bytesReturned,
                      info.m_InterfaceName))
    {
        info.SetInterfaceMACAddr(pQuerybuffer, bytesReturned);
        if (info.m_InterfaceMACAddr.IsValid())
        {
            LogDebug(TEXT("Current BSSID: %s, bytesReturned=%u"),
                     info.GetAPMAC(), bytesReturned);
        }
        else
        {
            LogError(TEXT("Got no BSSID for WiFi NIC \"%s\", bytesReturned=%u"),
                     info.GetInterfaceName(), bytesReturned);
            hr = E_FAIL;
        }
    }
    else
    {
        result = GetLastError();
        LogError(TEXT("Can't get BSSID for WiFi NIC \"%s\": %s"),
                 info.GetInterfaceName(), Win32ErrorText(result));
        hr = HRESULT_FROM_WIN32(result);
    }

    // Save the updated information.
    if (SUCCEEDED(hr))
    {
        new (this) WiFiConn_t(info);
    }
    
    return hr;
}

// ----------------------------------------------------------------------------
//
// Sets the interface name.
//
void
WiFiConn_t::
SetInterfaceName(
    IN const TCHAR *pInterfaceName)
{
    SafeCopy(m_InterfaceName, pInterfaceName, COUNTOF(m_InterfaceName));
}

// ----------------------------------------------------------------------------
//
// Sets the interface MAC address.
//
void
WiFiConn_t::
SetInterfaceMACAddr(
    IN const MACAddr_t &MACAddr)
{
    m_InterfaceMAC[0] = TEXT('\0');
    m_InterfaceMACAddr = MACAddr;
    if (m_InterfaceMACAddr.IsValid())
        m_InterfaceMACAddr.ToString(m_InterfaceMAC, COUNTOF(m_InterfaceMAC));
}

void
WiFiConn_t::
SetInterfaceMACAddr(
    IN const BYTE *pData,
    IN const DWORD  DataBytes)
{
    MACAddr_t mac;
    mac.Assign(pData, DataBytes);
    SetInterfaceMACAddr(mac);
}

// ----------------------------------------------------------------------------
//
// Sets the Access Point SSID.
//
void
WiFiConn_t::
SetAPSsid(
    IN const TCHAR *pAPSsid)
{
    SafeCopy(m_APSsid, pAPSsid, COUNTOF(m_APSsid));
}

void
WiFiConn_t::
SetAPSsid(
    IN const BYTE *pData,
    IN const DWORD  DataBytes)
{
    WiFUtils::ConvertString(m_APSsid, (const char *)pData,
                    COUNTOF(m_APSsid), "AP SSID", DataBytes);
}

// ----------------------------------------------------------------------------
//
// Sets the Access Point MAC address.
//
void
WiFiConn_t::
SetAPMACAddr(
    IN const MACAddr_t &MACAddr)
{
    m_APMAC[0] = TEXT('\0');
    m_APMACAddr = MACAddr;
    if (m_APMACAddr.IsValid())
        m_APMACAddr.ToString(m_APMAC, COUNTOF(m_APMAC));
}

void
WiFiConn_t::
SetAPMACAddr(
    IN const BYTE *pData,
    IN const DWORD  DataBytes)
{
    MACAddr_t mac;
    mac.Assign(pData, DataBytes);
    SetAPMACAddr(mac);
}

// ----------------------------------------------------------------------------
