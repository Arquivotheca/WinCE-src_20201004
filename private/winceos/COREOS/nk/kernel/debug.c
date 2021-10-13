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
#include "kernel.h"

#ifdef DEBUG

DBGPARAM dpCurSettings = {
    TEXT("NK Kernel"), {
    TEXT("Schedule"),   TEXT("Memory"),    TEXT("ObjDisp"),   TEXT("Debugger"),
    TEXT("Security"), TEXT("Loader"),    TEXT("VirtMem"),   TEXT("Loader2"),
    TEXT("ThreadIDs"),  TEXT("MapFile"),   TEXT("PhysMem"),   TEXT("SEH"),
    TEXT("OpenExe"),    TEXT("Error"), TEXT("Paging"), TEXT("APIEntry") },
    0x2010         // Turn on bit 0 for schedule, 1 for memory, etc... (default: security and error)
//  0x51a0         // useful for VM debugging
//  0xdde6         // useful for when things don't even boot
//  0xc934         // useful for when things boot somewhat
//  0x4100
};

#endif

