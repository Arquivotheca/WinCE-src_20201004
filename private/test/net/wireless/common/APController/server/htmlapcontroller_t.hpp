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
// Definitions and declarations for the HtmlAPController_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_HtmlAPController_t_
#define _DEFINED_HtmlAPController_t_
#pragma once

#include "AccessPointController_t.hpp"

namespace ce {
namespace qa {
    
// ----------------------------------------------------------------------------
//
// Specializes DeviceController_t for controlling Access Points configured
// using an HTML web-server.
//
class HtmlAPControllerImp_t;
class HtmlAPController_t : public AccessPointController_t
{
private:

    // Implementation:
    HtmlAPControllerImp_t *m_pImp;
    
    // Copy and assignment are deliberately disabled:
    HtmlAPController_t(const HtmlAPController_t &src);
    HtmlAPController_t &operator = (const HtmlAPController_t &src);

public:
    
    // Constructor:
    //    DeviceType: "Buffalo", "DLink", "LinkSys", etc.
    //    DeviceName:
    //        Device IP address, user-name and password: 
    //            {IP} [: {user-name} [: {password} ] ]
    //            user-name defaults to "root" or "admin"
    //            password default to ""
    //        Examples:
    //            10.10.0.9
    //            10.10.0.9:root
    //            192.168.47.11 : Mallard : Philmore     
    HtmlAPController_t(
        const TCHAR *pDeviceType,
        const TCHAR *pDeviceName);

    // Destructor:
  __override virtual
   ~HtmlAPController_t(void);

    // Gets the current Access Point configuration settings:
  __override virtual DWORD
    GetAccessPoint(
        AccessPointState_t *pResponse);

    // Updates the Access Point configuration settings:
  __override virtual DWORD
    SetAccessPoint(
        const AccessPointState_t &NewState,
              AccessPointState_t *pResponse);
};

// ----------------------------------------------------------------------------
//
// Base for a series of model-specific device-interface classes.
//
class HttpPort_t;
class HtmlForms_t;
class HtmlAPTypeController_t
{
protected:

    // HTTP Controller:
    HttpPort_t *m_pHttp;

    // Current AP configuration:
    AccessPointState_t *m_pState;

public:

    // Constructor and destructor:
    explicit
    HtmlAPTypeController_t(
        HttpPort_t         *pHttp,
        AccessPointState_t *pState)
        : m_pHttp(pHttp),
          m_pState(pState)
    { }
    virtual
   ~HtmlAPTypeController_t(void);

    // Gets the basic Access Point configuration settings and uses them
    // to generate the appropriate type of HtmlAPTypeController object
    // for this device model.
    static DWORD
    GenerateTypeController(
        const TCHAR             *pDeviceType, // vendor name
        const TCHAR             *pDeviceName, // IP address, user and password
        HttpPort_t              *pHttp,       // HTTP controller
        AccessPointState_t      *pState,      // initial device configuration
        HtmlAPTypeController_t **ppType);     // generated controller object

    // Create a Buffalo controller object:
    static DWORD
    GenerateBuffaloController(
        HttpPort_t              *pHttp,
        AccessPointState_t      *pState,
        HtmlAPTypeController_t** ppType);

    // Create a Dlink controller object:
    static DWORD
    GenerateDlinkController(
        HttpPort_t              *pHttp,
        AccessPointState_t      *pState,
        HtmlAPTypeController_t** ppType);

    // Initializes the device-controller:
    virtual DWORD
    InitializeDevice(void) = 0;

    // Resets the device once it gets into an unknown state:
    // (Since not all devices support this, the default implementation
    // for this method does nothing.)
    virtual DWORD
    ResetDevice(void);

    // Gets the configuration settings from the device:
    virtual DWORD
    LoadSettings(void) = 0;

    // Updates the device's configuration settings:
    virtual DWORD
    UpdateSettings(const AccessPointState_t &NewState) = 0;

  // Utility methods:

    // Uses the specified HTTP port to perform a web-page request and
    // parses form information out of the response:
    static DWORD
    ParsePageRequest(
        HttpPort_t  *pHttp,          // HTTP controller
        const WCHAR *pPageName,      // page to be loaded
        long         SleepMillis,    // wait time before reading response
        ce::string  *pWebPage,       // response from device
        HtmlForms_t *pPageForms,     // HTML forms parser
        int          FormsExpected); // number forms expected (or zero)
        
};

};
};
#endif /* _DEFINED_HtmlAPController_t_ */
// ----------------------------------------------------------------------------
