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
#include <kernel.h>

// This table is NOT const data since it is edited to initialize the TSS entries.
static ULONGLONG BootGDT[] = {
    0,
    0x00CF9A000000FFFF,         // Ring 0 code, Limit = 4G
    0x00CF92000000FFFF,         // Ring 0 data, Limit = 4G
};

const FWORDPtr BootGDTBase = {sizeof(BootGDT)-1, &BootGDT };

extern ROMHDR *const volatile pTOC;     // Gets replaced by RomLoader with real address

extern void SetOsAxsDataBlockPointer(struct KDataStruct *);

void KernelRelocate (ROMHDR *const pTOC);
LPVOID FindKernelEntry (ROMHDR const *pTOC);

extern POEMGLOBAL OEMInitGlobals (PNKGLOBAL pNKGlob);

//------------------------------------------------------------------------------
// minimal x86 initialization before jumping to the entry point of kernel.dll
// returns the entry point to kernel.dll
//------------------------------------------------------------------------------
#pragma warning ( push )
#pragma warning ( disable : 4740 )
// C4740: flow in or out of inline asm code suppresses global optimizations
LPVOID x86Init (struct KDataStruct *pKData) 
{
    /* Initialize kernel globals */
    KernelRelocate (pTOC);

    /* The only argument passed to the entry point of kernel.dll is the address */
    /* of KData, we need to put everything we need to pass to in in KData. */
    pKData->dwTOCAddr               = (DWORD) pTOC;
    pKData->dwOEMInitGlobalsAddr    = (DWORD) OEMInitGlobals;

    SetOsAxsDataBlockPointer(pKData);

    /* Switch to our GDT now that kernel relocation is done */
    __asm {
        lgdt    [BootGDTBase]
        mov     eax, KGDT_R0_DATA
        mov     ss, ax
        mov     ds, ax
        mov     es, ax
        push    KGDT_R0_CODE
        push    offset OurCSIsLoadedNow
        retf
OurCSIsLoadedNow:
    }

    return FindKernelEntry (pTOC);
}
#pragma warning ( pop )

