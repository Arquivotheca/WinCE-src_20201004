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
// Definitions and declarations for the CiscoPort_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_CiscoPort_t_
#define _DEFINED_CiscoPort_t_
#pragma once

#include "TelnetPort_t.hpp"

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Extends TelnetPort to provide an interface for controlling Cisco devices.
//
class CiscoSsid_t;
class CiscoPort_t : public TelnetPort_t
{
public:

    // Maximum SSID length:
    static const int MaxSsidChars = 32;
    
    // Maximum Cisco radio-interfaces:
    static const int NumberRadioInterfaces = 4;
    
private:

    // Radio interface ID:
    int m_RadioIntf;

    // Radio interface type(s):
    DWORD m_RadioIntfType;

    // Current SSID:
    char m_Ssid[MaxSsidChars+1];

    // Copy and assignment are deliberately disabled:
    CiscoPort_t(const CiscoPort_t &src);
    CiscoPort_t &operator = (const CiscoPort_t &src);

public:

    // Command modes:
    enum CommandMode_e
    {
        CommandModeUserExec,   // User EXEC mode (read-only rights)
        CommandModePrivileged, // Privileged EXEC mode
        CommandModeConfig,     // Global config mode (configure entire device)
        CommandModeInterface,  // Interface config mode (configure radio interface)
        CommandModeRadius,     // RADIUS server config mode (configure RADIUS server group)
        CommandModeSsid        // SSID config (configure global SSID parameters)
    };

    // Radio interface types:
    static const DWORD InterfaceTypeRadioA = 0x01;
    static const DWORD InterfaceTypeRadioB = 0x02;
    static const DWORD InterfaceTypeRadioG = 0x04;
    
    // Contructor and destructor:
    CiscoPort_t(const TCHAR *pServerHost, DWORD ServerPort, int RadioIntf);
  __override virtual
   ~CiscoPort_t(void);

  // Accessors:
  
    // Gets the radio interface identifier:
    int
    GetRadioInterface(void) const { return m_RadioIntf; }

    // Gets or sets the ORed radio interface type(s):
    DWORD
    GetRadioInterfaceType(void) const { return m_RadioIntfType; }
    void
    SetRadioInterfaceType(DWORD NewType) { m_RadioIntfType = NewType; }

    // Gets or sets the current SSID:
    const char *
    GetSsid(void) const { return m_Ssid; }
    void
    SetSsid(const char *pNewSsid);

  // I/O Functions:

    // Gets or changes the current command mode:
    CommandMode_e
    GetCommandMode(void) const;
    DWORD
    SetCommandMode(CommandMode_e NewMode);

    // Retrieves the device's current running configuration:
    DWORD
    ReadRunningConfig(void);
    TelnetLines_t *
    GetRunningConfig(void); 

    // Sends the specified command and retrieves the response (if any):
    // If necessary, changes to the specified command-mode before sending
    // the command.
    DWORD
    SendCommand(
        const char                *pFormat, ...);
    DWORD
    SendCommand(
        CiscoPort_t::CommandMode_e CommandMode,
        const char                *pFormat, ...);
    DWORD
    SendCommandV(
        CiscoPort_t::CommandMode_e CommandMode,
        const char                *pFormat,
        va_list                    pArgList,
        long                       MaxWaitTimeMS = TelnetPort_t::DefaultReadTimeMS);
    TelnetLines_t *
    GetCommandResponse(void);

    // Sends the specified command checks the response for an error message:
    // (Any response is assumed to be an error message.)
    // If necessary, changes to the specified command-mode before sending
    // the command.
    // If appropriate, issues an error message containing the specified
    // operation name and the error message sent by the server.
    DWORD
    VerifyCommand(
        const char                *pOperation,
        const char                *pFormat, ...);
    DWORD
    VerifyCommand(
        CiscoPort_t::CommandMode_e CommandMode,
        const char                *pOperation,
        const char                *pFormat, ...);
    DWORD
    VerifyCommandV(
        CiscoPort_t::CommandMode_e CommandMode,
        const char                *pOperation,
        const char                *pFormat,
        va_list                    pArgList,
        long                       MaxWaitTimeMS = TelnetPort_t::DefaultReadTimeMS);

  // Global SSIDs:

    // Gets the count of global SSIDs:
    int
    GetNumberGlobalSsids(void) const;

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
    
};

// ----------------------------------------------------------------------------
//
// Represents the configuration of a single global SSID in a Cisco
// Access Point.
//
class CiscoHandle_t;
class CiscoSsid_t
{
private:

    // SSID name:
    char m_Ssid[CiscoPort_t::MaxSsidChars+1];

    // Radio interface(s) associated to the SSID:
    char m_Interface[CiscoPort_t::NumberRadioInterfaces];

    // CiscoHandle is a friend:
    friend class CiscoHandle_t;
    
public:

    // Constructor/Destructor:
    CiscoSsid_t(const char *pSsid);
   ~CiscoSsid_t(void) {;}

 // Accessors:

    // Gets the SSID name:
    const char *
    GetSsid(void) const { return m_Ssid; }

    // Associates or disassociates the global SSID to/from a radio interface:
    // The Disassociate method will optionally retrieve the count of radio
    // interfaces still associated with the global SSID.
    DWORD
    AssociateRadioInterface(
        int RadioIntf);
    DWORD
    DisassociateRadioInterface(
        int  RadioIntf,
        int *pNumberAssociations = NULL);

    // Determines whether the global SSID is associated with the specified
    // radio interface:
    bool
    IsAssociated(
        int RadioIntf) const;
};

};
};
#endif /* _DEFINED_CiscoPort_t_ */
// ----------------------------------------------------------------------------
