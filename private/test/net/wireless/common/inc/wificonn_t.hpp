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
// Definitions and declarations for the WiFiConn_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WiFiConn_t_
#define _DEFINED_WiFiConn_t_
#pragma once

#include <MACAddr_t.hpp>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// This class contains all the information we know about an active WiFi
// connection.
//
class WiFiConn_t
{
private:

    // Interface name:
    TCHAR m_InterfaceName[128];

    // Interface MAC address:
    MACAddr_t m_InterfaceMACAddr;
    TCHAR     m_InterfaceMAC[sizeof(MACAddr_t) * 4];

    // Access Point SSID:
    TCHAR m_APSsid[64];

    // Access Point MAC address:
    MACAddr_t m_APMACAddr;
    TCHAR     m_APMAC[sizeof(MACAddr_t) * 4];

public:
    
    // Constructor and destructor:
    WiFiConn_t(void);
   ~WiFiConn_t(void);

    // Gets all the current connection information for the specified
    // WiFi interface:
    // Does not modify the existing connection information if retrieval
    // of the new status fails.
    HRESULT
    UpdateInterfaceInfo(
        IN const TCHAR *pInterfaceName,
        IN bool         fSilentMode = false);
    
    // Gets or sets the interface name:
    const TCHAR *
    GetInterfaceName(void) const { return m_InterfaceName; }
    void
    SetInterfaceName(
        IN const TCHAR *pInterfaceName);

    // Gets or sets the interface MAC address:
    const TCHAR *
    GetInterfaceMAC(void) const { return m_InterfaceMAC; }
    const MACAddr_t &
    GetInterfaceMACAddr(void) const { return m_InterfaceMACAddr; }
    void
    SetInterfaceMACAddr(
        IN const MACAddr_t &MACAddr);
    void
    SetInterfaceMACAddr(
        IN const BYTE *pData,
        IN const DWORD  DataBytes);
    
    // Gets or sets the Access Point SSID:
    const TCHAR *
    GetAPSsid(void) const { return m_APSsid; }
    void
    SetAPSsid(
        IN const TCHAR *pAPSsid);
    void
    SetAPSsid(
        IN const BYTE *pData,
        IN const DWORD  DataBytes);

    // Gets or sets the Access Point MAC address:
    const TCHAR *
    GetAPMAC(void) const { return m_APMAC; }
    const MACAddr_t &
    GetAPMACAddr(void) const { return m_APMACAddr; }
    void
    SetAPMACAddr(
        IN const MACAddr_t &MACAddr);
    void
    SetAPMACAddr(
        IN const BYTE *pData,
        IN const DWORD  DataBytes);

};

};
};
#endif /* _DEFINED_WiFiConn_t_ */
// ----------------------------------------------------------------------------
