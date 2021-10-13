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
// Definitions and declarations for the APConfigDevice_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_APConfigDevice_t_
#define _DEFINED_APConfigDevice_t_
#pragma once

#include <APConfigurator_t.hpp>
#include "AccessPointController_t.hpp"

#include <sync.hxx>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Extends APConfigurator_t to retrieve configuration information from
// the registry and integrate an actual access-point device controller.
//
class APConfigDevice_t : public APConfigurator_t,
                         public AccessPointController_t
{
private:

    // Configuration key:
    ce::tstring m_ConfigKey;

    // Actual device controller:
    DeviceController_t *m_pDevice;

    // Copy and assignment are deliberately disabled:
    APConfigDevice_t(const APConfigDevice_t &src);
    APConfigDevice_t &operator = (const APConfigDevice_t &src);

protected:

    // Synchronization object;
    ce::critical_section m_Locker;

    // These objects are only constructed by CreateConfigurator:
    APConfigDevice_t(
        const TCHAR        *pAPName,
        const TCHAR        *pConfigKey,
        const TCHAR        *pDeviceType,
        const TCHAR        *pDeviceName,
        DeviceController_t *pDevice);

    // Loads the initial AP configuration from the registry:
    virtual HRESULT
    LoadConfiguration(HKEY apHkey);

    // Generates a configurator-type parameter for use by CreateConfigurator:
    virtual HRESULT
    CreateConfiguratorType(ce::tstring *pConfigParam) const;

public:

    // Destructor:
    __override virtual
   ~APConfigDevice_t(void);

    // Generates an object from the registry:
    static HRESULT
    CreateConfigurator(
        const TCHAR        *pRootKey,
        const TCHAR        *pAPName,
        APConfigDevice_t **ppConfig);

    // Gets the AP's configuration store key:
    const TCHAR *
    GetConfigKey(void) const { return m_ConfigKey; }

    // Gets the current configuration of an Access Point:
    __override virtual DWORD
    GetAccessPoint(
        AccessPointState_t *pResponse);

    // Updates the configuration of an Access Point:
    __override virtual DWORD
    SetAccessPoint(
        const AccessPointState_t &NewState,
              AccessPointState_t *pResponse);

    // Saves the updated configuration values to the registry:
    __override virtual HRESULT
    SaveConfiguration(void);
};

};
};
#endif /* _DEFINED_APConfigDevice_t_ */
// ----------------------------------------------------------------------------
