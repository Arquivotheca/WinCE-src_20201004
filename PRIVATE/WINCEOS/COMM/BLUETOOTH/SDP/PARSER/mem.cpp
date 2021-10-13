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
#include "pch.h"

// Note: This code was taken from the kernel mode section of NT and not
// user mode because user mode was using CoMem functions.

PVOID SdpAllocatePool(SIZE_T Size)
{
    PVOID memory = ExAllocatePoolWithTag(PagedPool, Size, 'LpdS');

    if (memory) {
        RtlZeroMemory(memory, Size);
    }

    return memory;
}

VOID SdpFreePool(PVOID Memory)
{
     ExFreePool(Memory);
}

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
LONG SdpInterlockedDecrement(PLONG Addend)
{
    return InterlockedDecrement(Addend);
}
#endif // UNDER_CE
