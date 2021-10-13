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
#include <kernel.h>

// This table is NOT const data since it is edited to initialize the TSS entries.
static ULONGLONG g_aGlobalDescriptorTable[] = {
    0,
    0x00CF9A000000FFFF,         // Ring 0 code, Limit = 4G
    0x00CF92000000FFFF,         // Ring 0 data, Limit = 4G
    0x00CFBA000000FFFF,         // Ring 1 code, Limit = 4G
    0x00CFB2000000FFFF,         // Ring 1 data, Limit = 4G
    0x00CFDA000000FFFF,         // Ring 2 code, Limit = 4G
    0x00CFD2000000FFFF,         // Ring 2 data, Limit = 4G
    0x00CFFA000000FFFF,         // Ring 3 code, Limit = 4G
    0x00CFF2000000FFFF,         // Ring 3 data, Limit = 4G
    0,                          // Will be main TSS
    0,                          // Will be NMI TSS
    0,                          // Will be Double Fault TSS
    0x0040F20000000000+FS_LIMIT,    // PCR selector
    0x00CFBE000000FFFF,         // Ring 1 (conforming) code, Limit = 4G
};

const FWORDPtr g_KernelGDTBase = {sizeof(g_aGlobalDescriptorTable)-1, &g_aGlobalDescriptorTable };

extern ROMHDR *const volatile pTOC;     // Gets replaced by RomLoader with real address

extern void SetOsAxsDataBlockPointer(struct KDataStruct *);

void KernelRelocate (ROMHDR *const pTOC);
LPVOID FindKernelEntry (ROMHDR const *pTOC);

extern POEMGLOBAL OEMInitGlobals (PNKGLOBAL pNKGlob);
extern ADDRMAP OEMAddressTable[];

//------------------------------------------------------------------------------
// minimal x86 initialization before jumping to the entry point of kernel.dll
// returns the entry point to kernel.dll
//------------------------------------------------------------------------------
LPVOID x86Init (struct KDataStruct *pKData) 
{
    /* Initialize kernel globals */
    KernelRelocate (pTOC);

    /* The only argument passed to the entry point of kernel.dll is the address */
    /* of KData, we need to put everything we need to pass to in in KData. */
    pKData->pGDT          = g_aGlobalDescriptorTable;
    pKData->dwTOCAddr     = (DWORD) pTOC;
    pKData->dwOEMInitGlobalsAddr = (DWORD) OEMInitGlobals;
    pKData->pAddrMap      = OEMAddressTable;

    SetOsAxsDataBlockPointer(pKData);

    /* Switch to our GDT now that kernel relocation is done */
    __asm {
        lgdt    [g_KernelGDTBase]
        push    KGDT_R0_CODE
        push    offset OurCSIsLoadedNow
        retf
OurCSIsLoadedNow:
        mov     eax, KGDT_R0_DATA
        mov     ss, ax
        mov     eax, KGDT_R3_DATA
        mov     ds, ax
        mov     es, ax
    }

    return FindKernelEntry (pTOC);
}


