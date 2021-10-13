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
// Implementation of the DeviceController_t class.
//
// ----------------------------------------------------------------------------

#include "DeviceController_t.hpp"

#include <assert.h>
#include <MemBuffer_t.hpp>

using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Constructs the object for controlling the specific device.
//
DeviceController_t::
DeviceController_t(
    IN const TCHAR *pDeviceType,
    IN const TCHAR *pDeviceName)
    : m_DeviceType(pDeviceType),
      m_DeviceName(pDeviceName)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
DeviceController_t::
~DeviceController_t(void)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Gets the current configuration from an Access Point.
//
DWORD
DeviceController_t::
GetAccessPoint(
    OUT AccessPointState_t *pResponse,
    OUT ce::tstring        *pErrorMessage)
{
    WiFUtils::FmtMessage(pErrorMessage,
             TEXT("Device %s-%s does not support GetAccessPoint command"),
             GetDeviceType(), GetDeviceName());
    return ERROR_CALL_NOT_IMPLEMENTED;
}

// ----------------------------------------------------------------------------
//
// Updates the configuration of an Access Point.
//
DWORD
DeviceController_t::
SetAccessPoint(
    IN const AccessPointState_t &NewState,
    OUT      AccessPointState_t *pResponse,
    OUT      ce::tstring        *pErrorMessage)
{
    WiFUtils::FmtMessage(pErrorMessage,
             TEXT("Device %s-%s does not support SetAccessPoint command"),
             GetDeviceType(), GetDeviceName());
    return ERROR_CALL_NOT_IMPLEMENTED;
}

// ----------------------------------------------------------------------------
//
// Gets the current settings from an RF attenuation.
//
DWORD
DeviceController_t::
GetAttenuator(
    OUT RFAttenuatorState_t *pResponse,
    OUT ce::tstring         *pErrorMessage)
{
    WiFUtils::FmtMessage(pErrorMessage,
             TEXT("Device %s-%s does not support GetAttenuator command"),
             GetDeviceType(), GetDeviceName());
    return ERROR_CALL_NOT_IMPLEMENTED;
}

// ----------------------------------------------------------------------------
//
// Updates the settings of an RF attenuator.
//
DWORD
DeviceController_t::
SetAttenuator(
    IN const RFAttenuatorState_t &NewState,
    OUT      RFAttenuatorState_t *pResponse,
    OUT      ce::tstring         *pErrorMessage)
{
    WiFUtils::FmtMessage(pErrorMessage,
             TEXT("Device %s-%s does not support SetAttenuator command"),
             GetDeviceType(), GetDeviceName());
    return ERROR_CALL_NOT_IMPLEMENTED;
}

// ----------------------------------------------------------------------------
//
// Performs a remote SCB command.
//
DWORD
DeviceController_t::
DoDeviceCommand(
    IN       DWORD        Command,
    IN const BYTE       *pCommandData,
    IN       DWORD        CommandDataBytes,
    OUT      MemBuffer_t *pReturnData,
    OUT      MemBuffer_t *pErrorMessage)
{
    ce::tstring message;
    WiFUtils::FmtMessage(&message,
             TEXT("Device %s-%s does not support DoDeviceCommand command"),
             GetDeviceType(), GetDeviceName());
    pErrorMessage->Assign(message);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

// ----------------------------------------------------------------------------
