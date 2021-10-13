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
// Definitions and declarations for the APControlClient_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_APControlClient_t_
#define _DEFINED_APControlClient_t_
#pragma once

#include <WiFUtils.hpp>
#include <winsock.h>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Provides a simple client-side interface for accessing an APControl server.
//
class APControlClient_t
{
private:

    // Server host-name and port-number:
    ce::tstring m_ServerHost;
    ce::tstring m_ServerPort;

    // Device-type and -name:
    ce::tstring m_DeviceType;
    ce::tstring m_DeviceName;
    
    // Backchannel socket:
    SOCKET m_Socket;

    // Copy and assignment are deliberately disabled:
    APControlClient_t(const APControlClient_t &src);
    APControlClient_t &operator = (const APControlClient_t &src);
    
public:

    // Constructor and destructor:
    APControlClient_t(
        const TCHAR *pServerHost, 
        const TCHAR *pServerPort,
        const TCHAR *pDeviceType = NULL,
        const TCHAR *pDeviceName = NULL);
    virtual
   ~APControlClient_t(void);

    // Gets the server host-name or port-number:
    const TCHAR *
    GetServerHost(void) const { return m_ServerHost; }
    const TCHAR *
    GetServerPort(void) const { return m_ServerPort; }

    // Gets the device-type or -name:
    const TCHAR *
    GetDeviceType(void) const { return m_DeviceType; }
    const TCHAR *
    GetDeviceName(void) const { return m_DeviceName; }

    // Connects to the server:
    HRESULT
    Connect(void);

    // Disconnects from the server:
    HRESULT
    Disconnect(void);

    // Determines whether there is an active connection:
    bool
    IsConnected(void) const { return INVALID_SOCKET != m_Socket; }

    // Sends the specified Unicode-string command:
    DWORD
    SendStringCommand(
        DWORD CommandType,
        const ce::tstring &Command,
              ce::tstring *pResponse);

    // Sends the specified packet and waits for a response:
    DWORD
    SendPacket(
        DWORD  CommandType,
        BYTE *pCommandData,
        DWORD  CommandDataBytes,
        DWORD *pRemoteResult,
        BYTE **ppReturnData,
        DWORD  *pReturnDataBytes);
};

};
};
#endif /* _DEFINED_APControlClient_t_ */
// ----------------------------------------------------------------------------
