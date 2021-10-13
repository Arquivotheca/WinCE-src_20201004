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
// Definitions and declarations for the DeviceController_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_DeviceController_t_
#define _DEFINED_DeviceController_t_
#pragma once

#include <WiFUtils.hpp>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Provides a base class for deriving controllers for specific device types.
//
class MemBuffer_t;
class AccessPointState_t;
class RFAttenuatorState_t;
class DeviceController_t
{
private:

    // Device type and name:
    ce::tstring m_DeviceType;
    ce::tstring m_DeviceName;

    // Copy and assignment are deliberately disabled:
    DeviceController_t(const DeviceController_t &src);
    DeviceController_t &operator = (const DeviceController_t &src);

protected:

    // Constructs the object for controlling the specified device:
    DeviceController_t(
        IN const TCHAR *pDeviceType,
        IN const TCHAR *pDeviceName);
    
public:
    
    // Destructor:
    virtual
   ~DeviceController_t(void);

    // Gets the device type or name:
    const TCHAR *
    GetDeviceType(void) const { return m_DeviceType; }
    const TCHAR *
    GetDeviceName(void) const { return m_DeviceName; }

    // The default implementations for the following methods just return
    // a "not implemented" indication. Hence, derived classes need only
    // implement those methods they provide.

    // Gets the current configuration of an Access Point:
    virtual DWORD
    GetAccessPoint(
        OUT AccessPointState_t *pResponse,
        OUT ce::tstring        *pErrorMessage);

    // Updates the configuration of an Access Point:
    virtual DWORD
    SetAccessPoint(
        IN const AccessPointState_t &NewState,
        OUT      AccessPointState_t *pResponse,
        OUT      ce::tstring        *pErrorMessage);

    // Gets the current settings for an RF attenuator:
    virtual DWORD
    GetAttenuator(
        OUT RFAttenuatorState_t *pResponse,
        OUT ce::tstring         *pErrorMessage);

    // Updates the attenuation settings of an RF attenuator:
    virtual DWORD
    SetAttenuator(
        IN const RFAttenuatorState_t &NewState,
        OUT      RFAttenuatorState_t *pResponse,
        OUT      ce::tstring         *pErrorMessage);

    // Performs a generic device-control command:
    virtual DWORD
    DoDeviceCommand(
        IN       DWORD        Command,
        IN const BYTE       *pCommandData,
        IN       DWORD        CommandDataBytes,
        OUT      MemBuffer_t *pReturnData,
        OUT      MemBuffer_t *pErrorMessage);
};

};
};
#endif /* _DEFINED_DeviceController_t_ */
// ----------------------------------------------------------------------------
