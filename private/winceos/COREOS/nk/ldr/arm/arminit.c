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

extern ROMHDR *const volatile pTOC;     // Gets replaced by RomLoader with real address
extern POEMGLOBAL OEMInitGlobals (PNKGLOBAL pNKGlob);
extern void SetOsAxsDataBlockPointer(struct KDataStruct *);

void KernelRelocate (ROMHDR *const pTOC);
LPVOID FindKernelEntry (ROMHDR const *pTOC);

//------------------------------------------------------------------------------
// minimal initialization before jumping to the entry point of kernel.dll
// returns the entry point to kernel.dll
//------------------------------------------------------------------------------
LPVOID ARMInit (struct KDataStruct *pKData) 
{
    /* Initialize kernel globals */
    KernelRelocate (pTOC);

    /* The only argument passed to the entry point of kernel.dll is the address */
    /* of KData, we need to put everything we need to pass to in in KData. */
    pKData->dwTOCAddr     = (DWORD) pTOC;
    pKData->dwOEMInitGlobalsAddr = (DWORD) OEMInitGlobals;

    SetOsAxsDataBlockPointer(pKData);

    return FindKernelEntry (pTOC);
}

