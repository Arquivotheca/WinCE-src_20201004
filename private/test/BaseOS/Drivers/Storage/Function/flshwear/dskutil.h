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
#include "main.h"
#include "globals.h"

#include "dskioctl.h"

BOOL Util_FillDiskLinear(HANDLE hDisk, DWORD percentToFill);
BOOL Util_FillDiskRandom(HANDLE hDisk, DWORD percentToFill);
BOOL Util_FillDiskMaxFragment(HANDLE hDisk, DWORD percentToFill);

BOOL Util_FreeDiskLinear(HANDLE hDisk, DWORD percentToFree);
BOOL Util_FreeDiskRandom(HANDLE hDisk, DWORD percentToFree);
BOOL Util_FreeDiskMaxFragment(HANDLE hDisk, DWORD percentToFree);

