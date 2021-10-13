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
// Definitions and declarations for the WiFiBase_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WiFiBase_t_
#define _DEFINED_WiFiBase_t_
#pragma once

#include "CmdArgList_t.hpp"
#include "WiFUtils.hpp"

namespace ce {
namespace qa {


// ----------------------------------------------------------------------------
//
// Provides the basic implementation for all the tests to be run by the
// Tux test-suites.
//
class WiFiBase_t
{
public:

    // Initializes or cleans up static resources:
    static void StartupInitialize(void);
    static void ShutdownCleanup(void);

    // Creates and/or retrieves the list of CmdArg objects used to
    // configure the static variables:
    static CmdArgList_t *
    GetCmdArgList(void);   

    // Constructor and destructor:
    WiFiBase_t(void);
    virtual
   ~WiFiBase_t(void);

    // Initializes this concrete test class:
    // Returns ERROR_CALL_NOT_IMPLEMENTED if the test is to be skipped.
    virtual DWORD
    Init(void) = 0;

    // Runs the test:
    // Returns ERROR_CALL_NOT_IMPLEMENTED if the test is to be skipped.
    virtual DWORD
    Run(void) = 0;

    // Cleans up after the test finishes:
    // Note that this could be called while the test is still processing
    // the Run method. In that case, the test is expected to stop the
    // test as quickly as possible.
    virtual DWORD
    Cleanup(void) = 0;

    // Runs the Init, Run and Cleanup and returns a Tux TPR result code:
    virtual int
    Execute(
        IN DWORD        TestId,
        IN const TCHAR *pTestName);


};

};
};
#endif /* _DEFINED_WiFiBase_t_ */
// ----------------------------------------------------------------------------
