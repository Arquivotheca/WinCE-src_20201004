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
// Definitions and declarations for the WeinschelController_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WeinschelController_t_
#define _DEFINED_WeinschelController_t_
#pragma once

#include "AttenuatorController_t.hpp"
#include <RFAttenuatorState_t.hpp>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Specializes DeviceController_t for controlling Weinschel RF attenuators.
//
class CommPort_t;
class WeinschelController_t : public AttenuatorController_t
{
private:

    // Communication port handle:
    CommPort_t *m_pPort;

    // COM port number:
    int m_PortNumber;

    // Weinschel channel number:
    int m_ChannelNumber;

    // Cached attenuation values:
    RFAttenuatorState_t m_State;

    // Copy may only be used internally:
    WeinschelController_t(const WeinschelController_t &rhs);

    // Assignment is deliberately disabled:
    WeinschelController_t &operator = (const WeinschelController_t &rhs);

public:
    
    // Default COM-port connection settings:
    static const int DefaultBaudRate = CBR_38400;
    static const int DefaultByteSize = 8;
    static const int DefaultParity   = NOPARITY;
    static const int DefaultStopBits = ONESTOPBIT;
    
    // Constructor:
    //    DeviceType: usually "Weinschel".
    //    DeviceName:
    //        COM port number and Weinschel channel number:
    //            [COM]{port}-[CHAN]{channel}
    //        Examples:
    //            1-3
    //            COM2-CHAN1
    WeinschelController_t(
        const TCHAR *pDeviceType,
        const TCHAR *pDeviceName);

    // Destructor:
    __override virtual
   ~WeinschelController_t(void);

    // Gets the COM port or Weinschel channel number:
    int GetPortNumber(void) const { return m_PortNumber; }
    int GetChannelNumber(void) const { return m_ChannelNumber; }

    // Gets the current settings for the RF attenuator:
    __override virtual DWORD
    GetAttenuator(
        RFAttenuatorState_t *pResponse);

    // Sets the current settings for the RF attenuator:
    __override virtual DWORD
    SetAttenuator(
        const RFAttenuatorState_t &NewState,
              RFAttenuatorState_t *pResponse);

    // (Re)connects the Weinschel device:
    DWORD
    Reconnect(void);
};

};
};
#endif /* _DEFINED_WeinschelController_t_ */
// ----------------------------------------------------------------------------
