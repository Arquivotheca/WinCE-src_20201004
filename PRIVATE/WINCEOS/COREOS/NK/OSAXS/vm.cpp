//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++

Module Name:
    
    vm.cpp

Abstract:
    
    This file contains code that allows OsAxsT1 to manipulate the virtual
    memory structure.

Environment:

    OsAxsT1
--*/

#include "osaxs_p.h"

HRESULT ManipulateVm (DWORD dwOperation, DWORD dwVirtualAddr,
    DWORD dwNewPhysicalAddr, DWORD *pdwPhysicalAddrRet)
{
    HRESULT hr = S_OK;

    // Break down dwVirtualAddr into the key parts
    const DWORD ixSection = (dwVirtualAddr >> VA_SECTION) & SECTION_MASK;
    const DWORD ixBlock   = (dwVirtualAddr >> VA_BLOCK) & BLOCK_MASK;
    const DWORD ixPage    = (dwVirtualAddr >> VA_PAGE) & PAGE_MASK;
    const DWORD ixOffset  = dwVirtualAddr & (PAGE_SIZE - 1);

    // Find the aPage entry that contains dwVirtualAddr.
    PSECTION pSection = KData.aSections[ixSection];
    if (pSection)
    {
        MEMBLOCK *pBlock = (*pSection)[ixBlock];
        if (pBlock)
        {
            const DWORD dwPhysPerms = pBlock->aPages[ixPage] & PG_PERMISSION_MASK;
            const DWORD dwPhysAddr  = PFN2PA (PFNfromEntry (pBlock->aPages[ixPage])) | ixOffset;

            switch (dwOperation)
            {
            case OSAXST1_VM_GET:
                if (*pdwPhysicalAddrRet)
                    *pdwPhysicalAddrRet = dwPhysAddr;
                hr = S_OK;
                break;

            case OSAXST1_VM_SET:
                if (*pdwPhysicalAddrRet)
                    *pdwPhysicalAddrRet = dwPhysAddr;

                // Remove the offset bits (align down to page boundary)
                dwNewPhysicalAddr &= ~(PAGE_SIZE - 1);

                // Keep the old permissions.
                pBlock->aPages[ixPage] = PA2PFN (dwNewPhysicalAddr) | dwPhysPerms;
                hr = S_OK;
                break;

            default:
                hr = E_FAIL;
                break;
            }
        }
        else
            hr = OSAXS_E_VANOTMAPPED;
    }
    else
        hr = OSAXS_E_VANOTMAPPED;

    return hr;
}
