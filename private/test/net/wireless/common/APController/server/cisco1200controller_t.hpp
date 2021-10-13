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
// Definitions and declarations for the Cisco1200Controller_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_Cisco1200Controller_t_
#define _DEFINED_Cisco1200Controller_t_
#pragma once

#include "CiscoAPController_t.hpp"


namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Specializes DeviceController_t for controlling Cisco 1200 Series
// Access Points.
//
class Cisco1200Controller_t : public CiscoAPTypeController_t
{
private:
    
    // Copy and assignment are deliberately disabled:
    Cisco1200Controller_t(const Cisco1200Controller_t &src);
    Cisco1200Controller_t &operator = (const Cisco1200Controller_t &src);

public:

    // Constructor and destructor:
    Cisco1200Controller_t(
        CiscoPort_t         *pCLI,        // Telnet/CLI interface
        int                  Intf,        // Radio intf or -1 if all intfs
        const CiscoAPInfo_t &APInfo,      // Basic configuration information
        AccessPointState_t  *pState);     // Initial device configuration  
  __override virtual
   ~Cisco1200Controller_t(void);

    // Initializes the device-controller:
  __override virtual DWORD
    InitializeDevice(void);

    // Gets the configuration settings from the device:
  __override virtual DWORD
    LoadSettings(void);

    // Updates the device's configuration settings:
  __override virtual DWORD
    UpdateSettings(const AccessPointState_t &NewState);
};

};
};
#endif /* _DEFINED_Cisco1200Controller_t_ */
// ----------------------------------------------------------------------------
