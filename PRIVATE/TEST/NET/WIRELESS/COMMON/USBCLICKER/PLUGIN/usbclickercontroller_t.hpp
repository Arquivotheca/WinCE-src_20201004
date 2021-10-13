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
// Definitions and declarations for the USBClickerController_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_USBClickerController_t_
#define _DEFINED_USBClickerController_t_
#pragma once

#include <DeviceController_t.hpp>
#include <inc/sync.hxx>

// Device-access synchronization mutex:
extern ce::mutex g_USBClickerMutex;

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Specializes DeviceController_t for controlling USB "clicker" devices.
//
class USBClicker_t;
class USBClickerController_t : public DeviceController_t
{
private:

    // Local SCB-device interface:
    USBClicker_t *m_pClicker;

    // Copy and assignment are deliberately disabled:
    USBClickerController_t(const USBClickerController_t &src);
    USBClickerController_t &operator = (const USBClickerController_t &src);

public:
    
    // Constructor:
    //    DeviceType: usually "USBClicker".
    //    DeviceName:
    //        COM port number and communication speed:
    //            [COM]{port}-[SPEED]{baud-rate}
    //        Examples:
    //            1-9600
    //            COM2-9600
    USBClickerController_t(
        IN const TCHAR *pDeviceType,
        IN const TCHAR *pDeviceName);

    // Destructor:
    virtual
   ~USBClickerController_t(void);

    // Performs the remote SCB command:
    virtual DWORD
    DoDeviceCommand(
        IN        DWORD        Command,
        IN  const BYTE       *pCommandData,
        IN        DWORD        CommandDataBytes,
        OUT       BYTE      **ppReturnData,
        OUT       DWORD       *pReturnDataBytes,
        OUT       ce::tstring *pErrorMessage);
};

};
};
#endif /* _DEFINED_USBClickerController_t_ */
// ----------------------------------------------------------------------------
