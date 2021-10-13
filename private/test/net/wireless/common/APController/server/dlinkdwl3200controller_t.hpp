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
// Definitions and declarations for the DLinkDWL3200Controller_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_DLinkDWL3200Controller_t_
#define _DEFINED_DLinkDWL3200Controller_t_
#pragma once

#include <APController_t.hpp>
#include "HtmlAPController_t.hpp"

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Customized interface methods for the D-Link DL-524 Access Point.
//
class DLinkDWL3200Controller_t : public HtmlAPTypeController_t
{
private:

    // Firmware version number:
    float m_FirmwareVersion;

public:

    // Constructor and destructor:
    DLinkDWL3200Controller_t(
        HttpPort_t         *pHttp,
        AccessPointState_t *pState,
        const ce::string   &WebPage);
  __override virtual
   ~DLinkDWL3200Controller_t(void);

    // Initializes the device-controller:
  __override virtual DWORD
    InitializeDevice(void);

    // Resets the device once it gets into an unknown state:
  __override virtual DWORD
    ResetDevice(void);

    // Gets the configuration settings from the device:
  __override virtual DWORD
    LoadSettings(void);

    // Updates the device's configuration settings:
  __override virtual DWORD
    UpdateSettings(const AccessPointState_t &NewState);
};

};
};
#endif /* _DEFINED_DLinkDWL3200Controller_t_ */
// ----------------------------------------------------------------------------
