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
// Definitions and declarations for the AccessPointCoders class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_AccessPointCoders_
#define _DEFINED_AccessPointCoders_
#pragma once

#include <AccessPointState_t.hpp>
#include <WiFUtils.hpp>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Provides methods for translating Access Point state objects to and
// from command-argument form.
//
struct AccessPointCoders
{
    static DWORD
    String2State(
        const TCHAR        *pArgs,
        AccessPointState_t *pState);

    static DWORD
    State2String(
        const AccessPointState_t &State,
        ce::tstring              *pArgs);
};

};
};
#endif /* _DEFINED_AccessPointCoders_ */
// ----------------------------------------------------------------------------
