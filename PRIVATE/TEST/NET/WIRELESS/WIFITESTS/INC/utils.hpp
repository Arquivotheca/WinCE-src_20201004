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
// Definitions and declarations for the Utils class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_Utils_
#define _DEFINED_Utils_
#pragma once

#include <WiFUtils.hpp>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Utility funtions:
//
struct Utils
{
    // Initializes or cleans up static resources:
    static void StartupInitialize(void);
    static void ShutdownCleanup  (void);

    // Inserts a time-duration into the specified buffer:
    static void
    FormatRunTime(
        IN                          long    StartTime,
        OUT __out_ecount(BuffChars) TCHAR *pBuffer,
        IN                          int     BuffChars);
    
    // Converts the specified token to an integer:
    // Returns the default value if the token is empty or invalid.
    static int
    GetIntToken(
        IN TCHAR *pToken,
        IN int DefaultValue);

    // Gets the current system time in milliseconds since Jan 1, 1601:
    static _int64
    GetWallClockTime(void);
    
    // Trims spaces from the front and back of the specified token:
    static TCHAR *
    TrimToken(
        OUT TCHAR *pToken);
    
    // Runs the specified command and waits for it to finish:
    static DWORD
    RunCommand(
        IN const TCHAR *pProgram,
        IN const TCHAR *pCommand,
        IN DWORD        WaitTimeMS);
};

};
};
#endif /* _DEFINED_Utils_ */
// ----------------------------------------------------------------------------
