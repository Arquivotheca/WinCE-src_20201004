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

#pragma once

// Common routines between OSAXS and KD

/*++

Routine Name:

    ModuleFromAddress

Routine Description:

    This function determines based on the address "dwAddr" which module it falls within, and returns
    the module record.

Arguments:

    dwAddr - VM Address

Return Value:

    NULL -> error, unable to find a module
    Non-Null -> pointer to module record.

--*/
static PMODULE ModuleFromAddress(DWORD dwAddr)
{
    PMODULELIST pModList;
    DWORD StartAddress;
    DWORD EndAddress;
    PMODULE pMod;

    pModList = (PMODULELIST)g_pprcNK->modList.pFwd;
    pMod = NULL;
    while (!pMod && pModList && pModList != (PMODULELIST)&g_pprcNK->modList)
    {
        if (pModList->pMod)
        {
            StartAddress = (DWORD) pModList->pMod->BasePtr;
            EndAddress = StartAddress + pModList->pMod->e32.e32_vsize;
            if (dwAddr >= StartAddress && dwAddr < EndAddress)
            {
                pMod = pModList->pMod;
            }
        }
        pModList = (PMODULELIST)pModList->link.pFwd;
    }
    return pMod;
}


/*++

Routine Name:

    ModuleGetSectionFlags

Routine Description:

    Given an address and a module record, determine which section of the module contains the address,
    and return the section flags.

Arguments:

    pMod - pointer to module record
    dwAddr - address within the module
    pdwFlags - returns the flags for the section

Return Value:

    TRUE -> success,
    FALSE -> failure.
    
--*/
static BOOL ModuleGetSectionFlags(PMODULE pMod, DWORD dwAddr, DWORD *pdwFlags)
{
    BOOL fResult;
    DWORD StartAddress;
    DWORD EndAddress;
    DWORD i;

    fResult = FALSE;
    // Address is within this DLL's virtual address range.  Examine O32_PTR.
    if (pMod->o32_ptr)
    {
        i = 0;
        while (!fResult && i < pMod->e32.e32_objcnt)
        {
            StartAddress = pMod->o32_ptr[i].o32_realaddr;
            EndAddress = StartAddress + pMod->o32_ptr[i].o32_vsize;

            if (dwAddr >= StartAddress && dwAddr < EndAddress)
            {
                *pdwFlags = pMod->o32_ptr[i].o32_flags;
                fResult = TRUE;
            }
            ++i;
        }
    }
    return fResult;
}


// Based on pvAddr, replace pVMGuess with NK.EXE if necessary.
static PPROCESS GetProperVM(PPROCESS pVMGuess, void *pvAddr)
{
    //DEBUGGERMSG(OXZONE_MEM, (L"++GetProperVM: %08X, %08X\r\n", pVMGuess, pvAddr));
    if (((DWORD) pvAddr) >= VM_DLL_BASE && ((DWORD) pvAddr) < VM_SHARED_HEAP_BASE)
    {
        PMODULE pmod = ModuleFromAddress ((DWORD) pvAddr);
        if (pmod)
        {
            DWORD dwModFlags;
            if (ModuleGetSectionFlags(pmod, (DWORD) pvAddr, &dwModFlags))
            {
                if ((dwModFlags & IMAGE_SCN_MEM_WRITE) && !(dwModFlags & IMAGE_SCN_MEM_SHARED))
                {
                    //DEBUGGERMSG(OXZONE_MEM, (L"  GetProperVM: %08X using %08X\r\n", pvAddr, pVMGuess));
                }
                else
                {
                    pVMGuess = g_pprcNK;
                    //DEBUGGERMSG(OXZONE_MEM, (L"  GetProperVM: %08X using NK\r\n", pvAddr));
                }
            }
        }
    }

    // If still null at this point, default to the current VM.
    if (!pVMGuess)
    {
        pVMGuess = pVMProc;
    }
    //DEBUGGERMSG(OXZONE_MEM, (L"--GetProperVM: %08X\r\n", pVMGuess));
    return pVMGuess;
}

