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
#include <windows.h>

// --------------------------------------------------------------------
// @func begin enumeration of devices matching the specified GUID
HANDLE DEV_DetectFirstDevice (
        IN const GUID *devclass, // @param GUID class of devices to detect
        OUT WCHAR *pszDevName, // @param outpt name of the device enumerated
        DWORD cchDevName // @param length, in chars, of pszDevName
        );
// @rdesc returns a handle to a device enumeration object if a device
// was enumerated, NULL if no devices match the specified GUID
// --------------------------------------------------------------------

// --------------------------------------------------------------------
// @func continue enumeration of devices started with DEV_DetectFirstDevice
BOOL DEV_DetectNextDevice (
        IN HANDLE hDetect, // @param handle returned from DEV_DetectFirstDevice
        OUT WCHAR *pszDevName, // @param output name of the device enumerated
        IN DWORD cchDevName // @param lengh, in chars, of pszDevName
        );
// @rdesc returns TRUE if a device was enumerated, FALSE if there are
// no more devices matching the GUID
// --------------------------------------------------------------------

// --------------------------------------------------------------------
// @func cleanup a device enumeration object returned by DEV_DetectFirstDevice
void DEV_DetectClose (
        HANDLE hDetect // @param handle returned from DEV_DetectFirstDevice
        );
// @rdesc n/a
// --------------------------------------------------------------------


