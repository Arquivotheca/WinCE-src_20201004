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
// Definitions and declarations for the AttenuatorDevice_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_AttenuatorDevice_t_
#define _DEFINED_AttenuatorDevice_t_
#pragma once

#include <AttenuationDriver_t.hpp>
#include "AttenuatorController_t.hpp"

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Extends AttenuationDriver_t to retrieve configuration information from
// the registry and integrate an actual attenuator device-controller.
//
class AttenuatorDevice_t : public AttenuationDriver_t,
                           public AttenuatorController_t
{
private:

    // AP's name:
    ce::tstring m_APName;

    // Configuration key:
    ce::tstring m_ConfigKey;

    // Actual device controller:
    DeviceController_t *m_pDevice;

    // Copy and assignment are deliberately disabled:
    AttenuatorDevice_t(const AttenuatorDevice_t &src);
    AttenuatorDevice_t &operator = (const AttenuatorDevice_t &src);

protected:

    // Synchronization object;
    ce::critical_section m_Locker;

    // These objects are only contructed by CreateAttenuator:
    AttenuatorDevice_t(
        const TCHAR        *pAPName,
        const TCHAR        *pConfigKey,
        const TCHAR        *pDeviceType,
        const TCHAR        *pDeviceName,
        DeviceController_t *pDevice);
    
    // Loads the initial configuration from registry:
    virtual HRESULT
    LoadAttenuation(HKEY apHkey);

    // Generates an attenuator-type parameter for use by CreateAttenuator:
    virtual HRESULT
    CreateAttenuatorType(ce::tstring *pConfigParam) const;

public:

    // Destructor:
    __override virtual
   ~AttenuatorDevice_t(void);

    // Generates an object from the specified configuration parameter:
    static HRESULT
    CreateAttenuator(
        const TCHAR         *pRootKey,
        const TCHAR         *pAPName,
        AttenuatorDevice_t **ppAttenuator);

    // Gets the AP name or configuration key:
    const TCHAR *
    GetAPName(void) const { return m_APName; }
    const TCHAR *
    GetConfigKey(void) const { return m_ConfigKey; }

    // Gets the current settings for an RF attenuator:
    __override virtual DWORD
    GetAttenuator(
        RFAttenuatorState_t *pResponse);
                                                                                
    // Updates the attenuation settings of an RF attenuator:
    __override virtual DWORD
    SetAttenuator(
        const RFAttenuatorState_t &NewState,
              RFAttenuatorState_t *pResponse);

    // Saves the updated attenuation values to the registry:
    __override virtual HRESULT
    SaveAttenuation(void);
};

};
};
#endif /* _DEFINED_AttenuatorDevice_t_ */
// ----------------------------------------------------------------------------
