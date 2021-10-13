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
#include <inc/hash_map.hxx>

namespace ce {
namespace qa {

// Forward declarations:
class DeviceController_t;

// ----------------------------------------------------------------------------
//
// A hash-map for storing and returning name-value pairs:
//
typedef ce::hash_map<ce::string,ce::wstring> ValueMap;
typedef ValueMap::iterator                   ValueMapIter;
typedef ValueMap::const_iterator             ValueMapCIter;

// ----------------------------------------------------------------------------
//
// Prototypes for the functions to be provided by DeviceController
// plugins:
//

// Creates a new DeviceController_t object.
//
typedef
DWORD WINAPI
DeviceControllerCreator_t(
    IN  const TCHAR         *pDeviceType,
    IN  const TCHAR         *pDeviceName,
    OUT DeviceController_t **ppDevice,
    OUT ce::tstring         *pErrorMessage);

// ----------------------------------------------------------------------------
//
// Server's configuration parameters and utility funtions.
//
struct Utils
{
    // Initializes or cleans up static resources:
    static void StartupInitialize(void);
    static void ShutdownCleanup  (void);

  // Configuration parameters:

    // Configuration key in registry:
    static TCHAR DefaultRegistryKey[];
    static TCHAR      *pRegistryKey;

    // Server's host name or address:
    static TCHAR DefaultServerHost[];
    static TCHAR      *pServerHost;

    // Server's port number:
    static TCHAR DefaultServerPort[];
    static TCHAR      *pServerPort;

    // TclSh path name:
    static TCHAR DefaultShellExecName[];
    static TCHAR      *pShellExecName;

  // Utility functions:

    // Generates the specified type of device-controller:
    static DWORD
    CreateController(
        IN  const TCHAR         *pDeviceType,
        IN  const TCHAR         *pDeviceName,
        OUT DeviceController_t **ppDevice,
        OUT ce::tstring         *pErrorMessage);

    // Trims spaces from the front and back of the specified token:
    static TCHAR *
    TrimToken(
        OUT TCHAR *pToken);

    // Converts the specified token to an integer:
    // Returns the default value if the token is empty or invalid.
    static int
    GetIntToken(
        OUT TCHAR *pToken,
        IN  int DefaultValue);

    // Runs the specified command and waits for it to finish:
    static DWORD
    RunCommand(
        IN  const TCHAR *pProgram,
        IN  const TCHAR *pCommand,
        IN  DWORD        WaitTimeMS);
};

};
};
#endif /* _DEFINED_Utils_ */
// ----------------------------------------------------------------------------
