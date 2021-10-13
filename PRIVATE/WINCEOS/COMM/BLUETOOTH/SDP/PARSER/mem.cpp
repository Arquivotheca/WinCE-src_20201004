//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
