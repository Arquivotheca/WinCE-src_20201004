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

#include <osaxs_p.h>

HRESULT OsAxsTranslateAddress(HANDLE hProc, void* pvAddr, BOOL fReturnKVA, void** ppvPhysOrKVA)
{
    DEBUGGERMSG(OXZONE_VM, (L"+OsAxsTranslateAddress 0x%08x::0x%08x KVA=%d\r\n", hProc, pvAddr, fReturnKVA));
    HRESULT hr;
    PPROCESS pProc = pVMProc;
    
    if (hProc != NULL)
    {
        pProc = OsAxsHandleToProcess(hProc);
        if (pProc == NULL)
        {
            // invalid proc.
            DEBUGGERMSG(OXZONE_VM, (L" OsAxsTranslateAddress, hProc %08x, invalid\r\n", hProc));
            hr = E_HANDLE;
            goto error;
        }
    }

    *ppvPhysOrKVA = NULL;

    DWORD aligned = ((DWORD) pvAddr) & ~VM_PAGE_OFST_MASK;
    DWORD dwPFN = GetPFNOfProcess(pProc, (void *) aligned);
    if (dwPFN == INVALID_PHYSICAL_ADDRESS)
    {
        // no physical page.
        DEBUGGERMSG(OXZONE_VM, (L" OsAxsTranslateAddress, %08X::%08X, no pfn\r\n", pProc, pvAddr));
        hr = E_FAIL;
        goto error;
    }

    if (fReturnKVA)
    {
        PVOID pvVirt = Pfn2Virt(dwPFN);
        if (pvVirt == NULL)
        {
            DEBUGGERMSG(OXZONE_VM, (L" OsAxsTranslateAddress, pfn %08X, no VA\r\n", dwPFN));
            hr = E_FAIL;
            goto error;
        }

        *ppvPhysOrKVA = (void *)((DWORD)pvVirt | ((DWORD)pvAddr & VM_PAGE_OFST_MASK));
    }
    else
    {
        *ppvPhysOrKVA = (void *)(PFN2PA(dwPFN) | ((DWORD)pvAddr & VM_PAGE_OFST_MASK));
    }


    hr = S_OK;
    
exit:
    DEBUGGERMSG(OXZONE_VM,
        (L"-OsAxsTranslateAddress, KVA:%d, addr:%08x, hr:%08x\r\n",
         fReturnKVA, *ppvPhysOrKVA, hr));
    return hr;

error:
    DEBUGGERMSG(OXZONE_VM, 
        (L" OsAxsTranslateAddress 0x%08x::0x%08x KVA=%d, failed hr=%08x\r\n",
         hProc, pvAddr, fReturnKVA, hr));
    goto exit;
}
