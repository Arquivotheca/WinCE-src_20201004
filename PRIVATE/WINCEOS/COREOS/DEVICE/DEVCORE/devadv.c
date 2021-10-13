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
#define WINCEMACRO
#define BUILDING_FS_STUBS

#include <windows.h>
#include <guiddef.h>

//
// AdvertiseInterfaceInternal take extra parameter dwId which AdvertiseInterface does not take.
// We need this dwID for tracking the driver directly by its content.
BOOL AdvertiseInterfaceInternal(const GUID* devclass,LPCWSTR name,DWORD dwId, BOOL fAdd)
{
    return AdvertiseInterface_Trap(devclass,name, dwId, fAdd);
}

