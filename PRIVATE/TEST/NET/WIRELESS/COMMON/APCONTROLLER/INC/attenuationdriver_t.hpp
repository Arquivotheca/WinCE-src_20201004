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
// Definitions and declarations for the AttenuationDriver_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_AttenuationDriver_t_
#define _DEFINED_AttenuationDriver_t_
#pragma once

#include "RFAttenuatorState_t.hpp"
#include <inc/string.hxx>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Provides the basic implementation for an RF Attenuator device-controller.
// This class must be extended to specialize it for a particular device.
//
class AttenuationDriver_t
{
private:

    // Copy and assignment are deliberately disabled:
    AttenuationDriver_t(const AttenuationDriver_t &src);
    AttenuationDriver_t &operator = (const AttenuationDriver_t &src);

protected:

    // Current attenuation state:
    RFAttenuatorState_t m_State;

public:

    // Data-store keys:
    static const TCHAR * const AttenuatorKey;

    // Constructor and destructor:
    AttenuationDriver_t(void);
    virtual
   ~AttenuationDriver_t(void);

    // Sends the saved attenuation value to the attenuator device and
    // stores the attenuator description in the specified parameter:
    virtual HRESULT
    SaveAttenuation(void) = 0;
    
    // (Re)connects to the RF attenuation device and gets the current
    // attenuation values:
    // The default implementation does nothing.
    virtual HRESULT
    Reconnect(void);

    // Gets the current, minimum or maximum attenuation:
    int
    GetAttenuation(void);
    int
    GetMinAttenuation(void);
    int
    GetMaxAttenuation(void);

    // Caches the new attenuation settings until the next SaveAttenuation:
    HRESULT
    SetAttenuation(
        int CurrentAttenuation);
    HRESULT
    SetAttenuation(
        int CurrentAttenuation,
        int MinimumAttenuation,
        int MaximumAttenuation);

    // Schedules an attenutation adjustment within the specified range
    // using the specified adjustment steps:
    HRESULT
    AdjustAttenuation(
        int  StartAttenuation,    // Attenuation after first adjustment
        int  FinalAttenuation,    // Attenuation after last adjustment
        int  AdjustTime = 30,     // Time, in seconds, to perform adjustment
        long StepTimeMS = 1000L); // Time, in milliseconds, between each step
                                  // (Limited to between 500 (1/2 second) and
                                  // 3,600,000 (1 hour)
};

};
};
#endif /* _DEFINED_AttenuationDriver_t_ */
// ----------------------------------------------------------------------------
