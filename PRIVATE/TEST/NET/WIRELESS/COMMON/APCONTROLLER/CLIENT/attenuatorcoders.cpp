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
// Implementation of the AttenuatorCoders class.
//
// ----------------------------------------------------------------------------

#include "AttenuatorCoders.hpp"

#include <assert.h>
#include <tchar.h>
#include <strsafe.h>

using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Initializes an RF Attenuator state object from a command-line argument.
//
DWORD
AttenuatorCoders::
String2State(
    const TCHAR         *pArgs,
    RFAttenuatorState_t *pState)
{
    size_t argsLength;
    if (FAILED(StringCchLength(pArgs, 10, &argsLength))
     || 0 == argsLength)
    {
        LogError(TEXT("Missing attenuation level"));
        return ERROR_INVALID_PARAMETER;
    }

    TCHAR *argend;
    unsigned long newVal = _tcstoul(pArgs, &argend, 10);
    if (NULL == argend || TEXT('\0') != *argend || 150 < newVal)
    {
        LogError(TEXT("Attenuator level \"%s\" is invalid"), pArgs);
        return ERROR_INVALID_PARAMETER;
    }

    pState->Clear();
    pState->SetCurrentAttenuation(newVal);
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Translates an RF Attenuator state object to command-argument form.
//
DWORD
AttenuatorCoders::
State2String(
    const RFAttenuatorState_t &State,
    ce::tstring               *pArgs)
{
    pArgs->clear();
    pArgs->reserve(64);
    HRESULT hr = StringCchPrintf(pArgs->get_buffer(), pArgs->capacity(), 
                                 TEXT("%d/%d/%d"), State.GetCurrentAttenuation(),
                                                   State.GetMinimumAttenuation(),
                                                   State.GetMaximumAttenuation());
    return HRESULT_CODE(hr);
}

// ----------------------------------------------------------------------------
