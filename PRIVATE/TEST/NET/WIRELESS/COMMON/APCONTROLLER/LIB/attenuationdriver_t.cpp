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
// Implementation of the AttenuationDriver_t class.
//
// ----------------------------------------------------------------------------

#include "AttenuationDriver_t.hpp"

using namespace ce;
using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Registry keys:
//
const TCHAR * const AttenuationDriver_t::AttenuatorKey = TEXT("Attenuator");

// ----------------------------------------------------------------------------
//
// Constructor.
//
AttenuationDriver_t::
AttenuationDriver_t(void)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
AttenuationDriver_t::
~AttenuationDriver_t(void)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// (Re)connects to the RF attenuation device and gets the current
// attenuation values. This default implementation does nothing.
//
HRESULT
AttenuationDriver_t::
Reconnect(void)
{
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Gets the current, minimum or maximum attenuation.
//
int
AttenuationDriver_t::
GetAttenuation(void)
{
    Reconnect();
    return m_State.GetCurrentAttenuation();
}
int
AttenuationDriver_t::
GetMinAttenuation(void)
{
    Reconnect();
    return m_State.GetMinimumAttenuation();
}
int
AttenuationDriver_t::
GetMaxAttenuation(void)
{
    Reconnect();
    return m_State.GetMaximumAttenuation();
}

// ----------------------------------------------------------------------------
//
// Caches the new attenuation settings until the next SaveAttenuation.
//
HRESULT
AttenuationDriver_t::
SetAttenuation(int CurrentAttenuation)
{
    HRESULT hr = Reconnect();
    if (FAILED(hr))
        return hr;

    if (CurrentAttenuation < m_State.GetMinimumAttenuation()
     || CurrentAttenuation > m_State.GetMaximumAttenuation())
    {
        return E_INVALIDARG;
    }
    else
    {
        m_State.SetCurrentAttenuation(CurrentAttenuation);
        return ERROR_SUCCESS;
    }
}

HRESULT
AttenuationDriver_t::
SetAttenuation(
    int CurrentAttenuation,
    int MinimumAttenuation,
    int MaximumAttenuation)
{
    HRESULT hr = Reconnect();
    if (FAILED(hr))
        return hr;

    if (CurrentAttenuation < MinimumAttenuation
     || CurrentAttenuation > MaximumAttenuation)
    {
        return E_INVALIDARG;
    }
    else
    {
        m_State.SetCurrentAttenuation(CurrentAttenuation);
        m_State.SetMinimumAttenuation(MinimumAttenuation);
        m_State.SetMaximumAttenuation(MaximumAttenuation);
        return ERROR_SUCCESS;
    }
}

// ----------------------------------------------------------------------------
//
// Schedules an attenutation adjustment within the specified range
// using the specified adjustment steps.
//
HRESULT
AttenuationDriver_t::
AdjustAttenuation(
    int  StartAttenuation, // Attenuation after first adjustment
    int  FinalAttenuation, // Attenuation after last adjustment
    int  AdjustTime,       // Time, in seconds, to perform adjustment
    long StepTimeMS)       // Time, in milliseconds, between each step
                           // (Limited to between 500 (1/2 second) and
                           // 3,600,000 (1 hour)
{
    HRESULT hr = Reconnect();
    if (FAILED(hr))
        return hr;

    if (StartAttenuation < m_State.GetMinimumAttenuation()
     || StartAttenuation > m_State.GetMaximumAttenuation()
     || FinalAttenuation < m_State.GetMinimumAttenuation()
     || FinalAttenuation > m_State.GetMaximumAttenuation())
    {
        return E_INVALIDARG;
    }
    else
    {
        m_State.AdjustAttenuation(StartAttenuation,
                                  FinalAttenuation,
                                  AdjustTime,
                                  StepTimeMS);
        return ERROR_SUCCESS;
    }
}

// ----------------------------------------------------------------------------
