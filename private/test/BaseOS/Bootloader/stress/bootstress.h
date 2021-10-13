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

#pragma once

#include <windows.h>
#include <pm.h>

#define FILE_TOTAL_RAN    L"\\release\\bootStressTotalRan.txt"
#define FILE_BOOTTEST      L"\\release\\bootStress.txt"
#define FILE_LASTTICK       L"\\release\\bootStressLastTick.txt"

// ----------------------------------------------------------------------------
// Reboot device
//-----------------------------------------------------------------------------
static inline void reboot()
{
    SetCleanRebootFlag ();

    if(!SetSystemPowerState(NULL, POWER_STATE_RESET, 0))
    {
       NKDbgPrintfW(L"Failed to reboot the device.\r\n");
    }
}

