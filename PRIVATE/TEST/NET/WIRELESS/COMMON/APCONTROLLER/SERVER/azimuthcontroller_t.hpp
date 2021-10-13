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
// Definitions and declarations for the AzimuthController_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_AzimuthController_t_
#define _DEFINED_AzimuthController_t_
#pragma once

#include "DeviceController_t.hpp"
#include "Utils.hpp"

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Specializes DeviceController_t for controlling Azimuth devices.
//
class AzimuthController_t : public DeviceController_t
{
private:

    // Copy and assignment are deliberately disabled:
    AzimuthController_t(const AzimuthController_t &src);
    AzimuthController_t &operator = (const AzimuthController_t &src);

public:
    
    // Constructor:
    //    DeviceType: usually "Azimuth".
    //    DeviceName: an Azimuth device-identifier - usually
    //        C{chassis-number}-M{module-number}-{slot-number}
    //        Examples: C1-M1-1A, C2-M3-2B
    AzimuthController_t(
        const TCHAR *pDeviceType,
        const TCHAR *pDeviceName);

    // Destructor:
    virtual
   ~AzimuthController_t(void);

    // Gets the current, minimum and maximum RF attenuation:
    virtual DWORD
    GetAttenuator(
        RFAttenuatorState_t *pResponse,
        ce::tstring         *pErrorMessage);

    // Sets the current attenuation for the RF attenuator:
    virtual DWORD
    SetAttenuator(
        const RFAttenuatorState_t &NewState,
              RFAttenuatorState_t *pResponse,
              ce::tstring         *pErrorMessage);
};

};
};
#endif /* _DEFINED_AzimuthController_t_ */
// ----------------------------------------------------------------------------
