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
// Definitions and declarations for the CiscoAPController_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_CiscoAPController_t_
#define _DEFINED_CiscoAPController_t_
#pragma once

#include "AccessPointController_t.hpp"
#include "CiscoPort_t.hpp"

#include <AccessPointState_t.hpp>

namespace ce {
namespace qa {

// Device information:
//
struct CiscoAPInfo_t
{
    // Model and firmware version info:
    static const int MaxModelVersionNameChars = 64;
    char   ModelName[MaxModelVersionNameChars];
    DWORD  ModelNumber;
    char   IosVersionName[MaxModelVersionNameChars];
    double IosVersionNumber;
};
    
// ----------------------------------------------------------------------------
//
// Specializes DeviceController_t for controlling Cisco Access Points
// using their CLI interface.
//
class CiscoAPTypeController_t;
class CiscoAPController_t : public AccessPointController_t
{
private:

    // Primary Telnet/CLI interface:
    CiscoPort_t *m_pCLI;

    // Radio inteface ID or -1 if controlling all interfaces:
    int m_Intf;

    // Current AP configuration:
    AccessPointState_t m_State;

    // Type-specific device interface:
    CiscoAPTypeController_t *m_pType;
    
    // Copy and assignment are deliberately disabled:
    CiscoAPController_t(const CiscoAPController_t &src);
    CiscoAPController_t &operator = (const CiscoAPController_t &src);

public:
    
    // Constructor:
    //    DeviceType: "Cisco".
    //    DeviceName:
    //        Device IP address, user-name and password: 
    //            {IP} [: {user-name} [: {password} ] ]
    //            user-name defaults to "Cisco"
    //            password default to "Cisco"
    //        Examples:
    //            10.10.0.9                 <== 10.10.0.9:Cisco:Cisco
    //            10.10.0.9 : Cisco         <== 10.10.0.9:Cisco:Cisco
    //            10.10.0.9 : admin : Cisco <== 10.10.0.9:admin:Cisco
    //            192.168.47.11 : Mallard : Philmore     
    CiscoAPController_t(
        const TCHAR *pDeviceType,
        const TCHAR *pDeviceName);

    // Destructor:
  __override virtual
   ~CiscoAPController_t(void);
    
    // Provides an object for initializing the device's state:
  __override virtual void
    SetInitialState(
        IN const AccessPointState_t &InitialState);

    // Gets the current Access Point configuration settings:
  __override virtual DWORD
    GetAccessPoint(
        AccessPointState_t *pResponse);

    // Updates the Access Point configuration settings:
  __override virtual DWORD
    SetAccessPoint(
        const AccessPointState_t &NewState,
              AccessPointState_t *pResponse);
private:

    // Ensure the specific derived type controller object has been created:
    DWORD
    CreateTypeController(CiscoAPTypeController_t **ppType);
};

// ----------------------------------------------------------------------------
//
// Base for a series of model-specific device-interface classes.
//
class CiscoAPTypeController_t
{
protected:

    // Telnet/CLI controllers:
    CiscoPort_t *m_pCLI;
    CiscoPort_t *m_pIntfCLIs[CiscoPort_t::NumberRadioInterfaces];
    
    // Device information:
    CiscoAPInfo_t m_APInfo;

    // Current AP configuration:
    AccessPointState_t *m_pState;
    
public:

    // Constructor and destructor:
    explicit
    CiscoAPTypeController_t(
        CiscoPort_t         *pCLI,        // Primary Telnet/CLI interface
        int                  Intf,        // Radio intf or -1 if all intfs
        const CiscoAPInfo_t &APInfo,      // Basic configuration information
        AccessPointState_t  *pState);     // Initial device configuration
    virtual
   ~CiscoAPTypeController_t(void);

    // Gets the basic Access Point configuration settings and uses them
    // to generate the appropriate type of CiscoAPTypeController object
    // for this device model.
    static DWORD
    GenerateTypeController(
        const TCHAR              *pDeviceType, // vendor name
        const TCHAR              *pDeviceName, // IP address, user and password
        CiscoPort_t              *pCLI,        // Primary Telnet (CLI) controller
        int                       Intf,        // Radio intf or -1 if all intfs
        AccessPointState_t       *pState,      // initial device configuration
        CiscoAPTypeController_t **ppType);     // generated controller object


    // Create a Cisco 1200-series controller object:
    static DWORD
    Generate1200Controller(
        CiscoPort_t              *pCLI,        // Primary Telnet/CLI interface
        int                       Intf,        // Radio intf or -1 if all intfs
        const CiscoAPInfo_t      &APInfo,      // Basic configuration information
        AccessPointState_t       *pState,      // Initial device configuration  
        CiscoAPTypeController_t **ppType);     // Output pointer

    // Initializes the device-controller:
    virtual DWORD
    InitializeDevice(void) = 0;

    // Resets the device once it gets into an unknown state:
    virtual DWORD
    ResetDevice(void);

    // Gets the configuration settings from the device:
    virtual DWORD
    LoadSettings(void) = 0;

    // Updates the device's configuration settings:
    virtual DWORD
    UpdateSettings(const AccessPointState_t &NewState) = 0;

  // Utility methods:

    // Sends the specified command and stores the response in m_Response:
    // If necessary, changes to the specified command-mode before sending
    // the command.
    // If the command is being directed to the radio interface, sends it
    // to each the Access Point's interfaces in turn.
    DWORD
    SendCommand(
        CiscoPort_t::CommandMode_e CommandMode,
        const char                *pFormat, ...);
    DWORD
    SendCommandV(
        CiscoPort_t::CommandMode_e CommandMode,
        const char                *pFormat,
        va_list                    pArgList);

    // Sends the specified command, stores the response in m_Response and
    // and checks it for an error message:
    // (Any response is assumed to be an error message.)
    // If necessary, changes to the specified command-mode before sending
    // the command.
    // If the command is being directed to the radio interface, verifies
    // it on each the Access Point's interfaces in turn.
    // If appropriate, issues an error message containing the specified
    // operation name and the error message sent by the server.
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
        va_list                    pArgList);
    
};

};
};
#endif /* _DEFINED_CiscoAPController_t_ */
// ----------------------------------------------------------------------------
