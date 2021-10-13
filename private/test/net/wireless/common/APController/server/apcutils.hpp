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
// Definitions and declarations for the APCUtils class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_APCUtils_
#define _DEFINED_APCUtils_
#pragma once

#include <WiFUtils.hpp>
#include <hash_map.hxx>

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
// Server's configuration parameters and utility funtions.
//
struct APCUtils
{
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
        const TCHAR         *pDeviceType,
        const TCHAR         *pDeviceName,
        DeviceController_t **ppDevice);

};

};
};
#endif /* _DEFINED_APCUtils_ */
// ----------------------------------------------------------------------------
