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
// Definitions and declarations for the CiscoHandle_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_CiscoHandle_t_
#define _DEFINED_CiscoHandle_t_
#pragma once

#include "TelnetHandle_t.hpp"
#include "CiscoPort_t.hpp"

#include <MemBuffer_t.hpp>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Extends TelnetHandle to customize it for communicating with Cisco devices.
//
class CiscoSsid_t;
class CiscoHandle_t : public TelnetHandle_t
{
private:

    // Current command mode:
    CiscoPort_t::CommandMode_e m_CommandMode;

    // Current radio interface ID:
    int m_RadioIntf;

    // Current SSID:
    char m_Ssid[CiscoPort_t::MaxSsidChars+1];

    // List of global SSIDs:
    MemBuffer_t m_GlobalSsids;
    int   m_NumberGlobalSsids;

    // Response from last SendCommand:
    TelnetLines_t m_Response;

    // Response from last running-config report:
    TelnetLines_t m_RunningConfig;

public:

    // Constructor/Destructor:
    CiscoHandle_t(const TCHAR *pServerHost, DWORD ServerPort);
  __override virtual
   ~CiscoHandle_t(void);

    // Connects to the telnet-server:
  __override virtual DWORD
    Connect(long MaxWaitTimeMS = TelnetPort_t::DefaultConnectTimeMS);

    // Closes the existing connection:
  __override virtual void
    Disconnect(void);

  // I/O Functions:

    // Gets or changes the current command mode:
    CiscoPort_t::CommandMode_e
    GetCommandMode(void) const { return m_CommandMode; }
    DWORD
    SetCommandMode(
        CiscoPort_t::CommandMode_e NewMode,
        int                        RadioIntf,
        const char                *pSsid);

    // Retrieves the device's current running configuration:
    DWORD
    ReadRunningConfig(void);
    TelnetLines_t *
    GetRunningConfig(void) { return &m_RunningConfig; } 
    
    // Sends the specified command and retrieves the response (if any):
    // If necessary, changes to the specified command-mode before sending
    // the command.
    DWORD
    SendCommandV(
        CiscoPort_t::CommandMode_e CommandMode,
        int                        RadioIntf,
        const char                *pSsid,
        const char                *pFormat,
        va_list                    pArgList,
        long                       MaxWaitTimeMS = TelnetPort_t::DefaultReadTimeMS);
    TelnetLines_t *
    GetCommandResponse(void) { return &m_Response; }

  // Global SSIDs:

    // Gets the count of global SSIDs:
    int
    GetNumberGlobalSsids(void) const { return m_NumberGlobalSsids; }

    // Adds or deletes the specified global SSID:
    DWORD
    AddGlobalSsid(const char *pSsid);
    DWORD
    DeleteGlobalSsid(const char *pSsid);

    // Looks up the specified global SSID by name or number:
    CiscoSsid_t *
    GetGlobalSsid(int IX);
    const CiscoSsid_t *
    GetGlobalSsid(int IX) const;
    CiscoSsid_t *
    GetGlobalSsid(const char *pSsid);
    const CiscoSsid_t *
    GetGlobalSsid(const char *pSsid) const;

private:
    
    // Sends the specified command and retrieves the response (if any):
    // If necessary, changes to the specified command-mode before sending
    // the command.
    DWORD
    SendCommand(
        CiscoPort_t::CommandMode_e CommandMode,
        int                        RadioIntf,
        const char                *pSsid,
        const char                *pCommand,
        TelnetLines_t             *pResponse,
        long                       MaxWaitTimeMS);
    
};

};
};
#endif /* _DEFINED_CiscoHandle_t_ */
// ----------------------------------------------------------------------------
