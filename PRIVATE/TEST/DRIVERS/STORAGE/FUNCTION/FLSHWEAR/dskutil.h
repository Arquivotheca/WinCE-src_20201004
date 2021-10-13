//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

