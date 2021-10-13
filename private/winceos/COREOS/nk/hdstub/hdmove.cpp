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

#include <hdstub_p.h>

//NOTE: We currently only need to use 2 pages.  So we don't need to manage any 
//freelists.  If we ever have to use more than 2 pages, we will have to add a free list manager.
//Element 0 is for reading. Element 1 is for writing.
#define MAX_PAGES 2

struct hdMemMap
{
    void *mempage;
    void *origAddress;
    BOOL fInUse;
};

static hdMemMap s_memmap[MAX_PAGES];  // 0 = ro, 1 = rw
static int s_iPageCount = 0;

BOOL HdstubInitMemAccess (void)
{
    DWORD i;
    BOOL success = TRUE;

    if (!s_memmap[0].mempage)
    {
        s_memmap[0].fInUse = FALSE;
        s_memmap[0].origAddress = NULL;
        // Allocate all of the pages in one call to VirtualAlloc.  If the debugger needs more pages in the future,
        // try to keep all of the page allocations in one call.  By design, VMAlloc will allocate on in 64K chunks.
        s_memmap[0].mempage = VMAlloc (g_pprcNK, NULL, VM_PAGE_SIZE * _countof (s_memmap), MEM_RESERVE, PAGE_READWRITE);
        success = (s_memmap[0].mempage != NULL);
        if (success)
        {
            for (i = 1; i < _countof (s_memmap); i++)
            {
                s_memmap[i].fInUse = FALSE;
                s_memmap[i].origAddress = NULL;
                s_memmap[i].mempage = ((BYTE *) s_memmap[0].mempage) + (i *VM_PAGE_SIZE);
            }
            
            for (i = 0; i < _countof (s_memmap); i++)
            {
                DEBUGGERMSG(HDZONE_MOVE, (L"MemAccess: mem addr:0x%08x orig:0x%08x inuse:%d\r\n", &s_memmap[i].mempage, s_memmap[i].origAddress, s_memmap[i].fInUse));
            }
        }
    }

    if (!success)                       // Clear memory pages on error.
    {
        HdstubDeInitMemAccess ();
    }

    return success;
}

void HdstubDeInitMemAccess (void)
{
    if (s_memmap[0].mempage)
    {
        VMFreeAndRelease (g_pprcNK, s_memmap[0].mempage, VM_PAGE_SIZE * _countof (s_memmap));
        s_memmap[0].mempage = NULL;
    }
}


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
PMODULE ModuleFromAddress(DWORD dwAddr)
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
BOOL ModuleGetSectionFlags(PMODULE pMod, DWORD dwAddr, DWORD *pdwFlags)
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
PPROCESS GetProperVM(PPROCESS pVMGuess, void *pvAddr)
{
    DEBUGGERMSG(HDZONE_VM, (L"++GetProperVM: %08X, %08X\r\n", pVMGuess, pvAddr));    

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
                    //in a dll R/W section (most likely data section. Use vm guess or vm current.
                }
                else
                {
                    //in a dll code section.  Use NK.
                    pVMGuess = g_pprcNK;
                }
                DEBUGGERMSG(HDZONE_VM, (L"  GetProperVM: address 0x%08X with flags 0x%08x using %08X\r\n", pvAddr, dwModFlags, pVMGuess));
            }
        }
    }
    else if(((DWORD) pvAddr) >= VM_KMODE_BASE)
    {
        pVMGuess = g_pprcNK;        
    }

    // If still null at this point, default to the current VM.
    if (!pVMGuess)
    {
        pVMGuess = pVMProc;
    }
    DEBUGGERMSG(HDZONE_VM, (L"--GetProperVM: %08X\r\n", pVMGuess));    
    return pVMGuess;
}


DWORD GetPhysicalAddress(PPROCESS Process, void *Address)
{
    if (Process != NULL && !IsValidProcess(Process))
    {
        // Don't bother calling kernel API with an obviously bad
        // process.
        return INVALID_PHYSICAL_ADDRESS;
    }
    void *PageAddress = reinterpret_cast<void*>(
            PAGEALIGN_DOWN(reinterpret_cast<DWORD>(Address)));
    DWORD PageOffset = reinterpret_cast<DWORD>(Address) & VM_PAGE_OFST_MASK;
    DWORD pfn = GetPFNOfProcess(Process, PageAddress);
    if (pfn == INVALID_PHYSICAL_ADDRESS)
    {
        return pfn;
    }
    return PFN2PA(pfn) | PageOffset;
}


DWORD AddrToPA(MEMADDR const *pma)
{
    DWORD addrType = AddrType(pma);
    PPROCESS pprcVM = NULL;
    if (addrType == MF_TYPE_PHYSICAL)
    {
        return (DWORD) pma->addr;
    }
    else if (addrType == MF_TYPE_DEBUG)
    {
        pprcVM = PcbGetVMProc();
    }
    else if (addrType == MF_TYPE_VIRTUAL)
    {
        pprcVM = pma->vm;
    }
    else
    {
        // Bad!
        return INVALID_PHYSICAL_ADDRESS;
    }

    return GetPhysicalAddress(pprcVM, pma->addr);
}


static BOOL AddrIsInaccessible(LPVOID pvAddr)
{
    BOOL fRet = FALSE;   // Assume FALSE in case IOCTL not implemented

    DEBUGGERMSG(HDZONE_MOVE, (L"++AddrIsInaccessible, pvAddr=0x%08X\r\n", pvAddr));
    // Check if the memory is Inaccessible for read/write
    fRet = HdstubIoControl (KD_IOCTL_MEMORY_INACCESSIBLE, pvAddr, sizeof(pvAddr));
    DEBUGGERMSG(HDZONE_MOVE, (L"--AddrIsInaccessible, fRet=%s\r\n", (fRet ? L"TRUE" : L"FALSE") ));
    return fRet;
}


void *HdstubMapAddr(MEMADDR const *addr)
{
    DEBUGGERMSG(HDZONE_MOVE, (L"++HdstubMapAddr: 0x%08x::0x%08x flags:0x%08x\r\n", addr->vm, addr->addr, addr->flags));
    void *pRet = NULL;
    
    switch (AddrType(addr))
    {
    case MF_TYPE_DEBUG:
        pRet = addr->addr;
        break;

    case MF_TYPE_VIRTUAL:
    case MF_TYPE_PHYSICAL:
    {        
        DWORD dwPFN = INVALID_PHYSICAL_ADDRESS;
        ULONG ac = !!(addr->flags & MF_WRITE);                        // 0 = ro, 1 = rw        

        if(AddrType(addr) == MF_TYPE_VIRTUAL)
        {
            if (!IsValidProcess(addr->vm))
            {
                DEBUGGERMSG(HDZONE_MOVE, (L"  HdstubMapAddr: %08X is not a valid process\r\n",
                    addr->vm));
                break;
            }
            
            if(((addr->flags & MF_WRITE) && kdpIsROM(addr->vm, addr->addr, 1)) ||
                AddrIsInaccessible(addr->addr))
            {
                DEBUGGERMSG(HDZONE_MOVE, (L"  HdstubMapAddr: 0x%08x is not mappable\r\n", addr->addr));
                break;
            }

            dwPFN = GetPFNOfProcess(addr->vm, (void *) PAGEALIGN_DOWN ((DWORD) addr->addr));                            
            if(dwPFN == INVALID_PHYSICAL_ADDRESS)
            {
                DEBUGGERMSG(HDZONE_MOVE, (L"  HdstubMapAddr: 0x%08x::0x%08x Could not get PFN\r\n", addr->vm, addr->addr));
                break;

            }

            //NOTE: GetPFNOfProcess ensures that the page is committed

            if((((DWORD)addr->addr < VM_KMODE_BASE) && (addr->vm != pVMProc)) || 
                ((addr->flags & MF_WRITE) && !IsPageWritable(dwPFN)))
            {
            }
            else
            {   
                pRet = addr->addr;
                break;                
            }
            
        }
        else
        {
            dwPFN = PA2PFN((DWORD)addr->addr & ~VM_PAGE_OFST_MASK);
        }

        if(dwPFN != INVALID_PHYSICAL_ADDRESS)
        {
            dwPFN &= ~PG_PERMISSION_MASK; 
            NKCacheRangeFlush (0, 0, CACHE_SYNC_ALL);          // Clear caches and TLB.
            if(VMCreateKernelPageMapping(s_memmap[ac].mempage, dwPFN))
            {
                NKCacheRangeFlush (0, 0, CACHE_SYNC_ALL);          // Clear caches and TLB.                 
                s_iPageCount++;
                if(s_iPageCount > 2)
                {   
                    DEBUGGERMSG(HDZONE_MOVE, (L"  HdStubMapAddr: Too many mapped pages %d\r\n", s_iPageCount));
                }
                s_memmap[ac].origAddress = addr->addr;
                s_memmap[ac].fInUse = TRUE;
                pRet = (void *)((DWORD)s_memmap[ac].mempage | (((DWORD)addr->addr) & VM_PAGE_OFST_MASK)); 
                DEBUGGERMSG(HDZONE_MOVE, (L" Mapped: 0x%08x::0x%08x (pfn:0x%08x)-->0x%08x\r\n", addr->vm, addr->addr, dwPFN, pRet));
            }
            else
            {
                DEBUGGERMSG(HDZONE_ALERT, (L"  HdStubMapAddr: Failed to create mapping\r\n"));
            }
        }

        break;
    }

    case MF_TYPE_IO:
    default:                
        DEBUGGERMSG(HDZONE_ALERT, (L"  HdStubMapAddr: Unkown type: 0x%08x\r\n", addr->flags));
    }

    DEBUGGERMSG(HDZONE_MOVE, (L"--HdstubMapAddr: 0x%08x\r\n", pRet));
    return pRet;
}


void HdstubUnMapAddr(MEMADDR const *addr)
{
    DEBUGGERMSG(HDZONE_MOVE, (L"++HdstubUnMapAddr: 0x%08x\r\n", addr->addr));

    switch (AddrType(addr))
    {
    case MF_TYPE_DEBUG:
        break;
        
    case MF_TYPE_VIRTUAL:
    case MF_TYPE_PHYSICAL:
    {
        ULONG ac = !!(addr->flags & MF_WRITE);                        // 0 = ro, 1 = rw
    
        if(s_memmap[ac].fInUse)
        {
            if(addr->addr == s_memmap[ac].origAddress)
            {
                DEBUGGERMSG(HDZONE_MOVE, (L"  HdStubUnMapAddr: Unmapping 0x%08x from 0x%08x\r\n", addr->addr, s_memmap[ac].mempage));

                s_iPageCount--;
                s_memmap[ac].fInUse = FALSE;
                s_memmap[ac].origAddress = NULL;            
                NKCacheRangeFlush (0, 0, CACHE_SYNC_ALL);          // Clear caches and TLB.
                VMRemoveKernelPageMapping(s_memmap[ac].mempage);
                NKCacheRangeFlush (0, 0, CACHE_SYNC_ALL);          // Clear caches and TLB.
            }
            else
            {
                DEBUGGERMSG(HDZONE_ALERT, (L" Unmatched Free Address: 0x%08x ac:%d\r\n", addr->addr, ac));
            }
        }
        break;
    }
    
    case MF_TYPE_IO:
    default:
        DEBUGGERMSG(HDZONE_ALERT, (L"  HdStubUnMapAddr: Unkown type: 0x%08x\r\n", addr->flags));
    }

    DEBUGGERMSG(HDZONE_MOVE, (L"--HdstubUnMapAddr\r\n"));
}


//TODO: NOTE A physical source/dest can't cross a page boundary.
//Will users know to split up physical page reads.
ULONG
MoveMemory0(
    MEMADDR const *Dest,
    MEMADDR const *Source,
    ULONG Length)
{
    DEBUGGERMSG(HDZONE_MOVE, (L"++MoveMemory0: 0x%08x::0x%08x (%08x), 0x%08x::0x%08x (0x%08x), %d\r\n", Dest->vm, Dest->addr, Dest->flags, Source->vm, Source->addr, Source->flags, Length));

    //TODO: Parameter validation? 
    //Disallow NULL VM? 
    //disallow physical page of more than 1 page.   See above.
    //disallow rollover (although, I think code will work).
              
    MEMADDR Dest2;
    MEMADDR Source2;
    BYTE *DstAddr = NULL;
    BYTE *SrcAddr = NULL;
    ULONG srcBytesOnPage, dstBytesOnPage, bytesToCopy, bytesLeft=Length;

    // Move the source information to the destination address.

    memcpy(&Dest2, Dest, sizeof(Dest2));
    Dest2.flags |= MF_WRITE;
    memcpy(&Source2, Source, sizeof(Source2));

    //
    // This code is the most common cause of KD exceptions because the OS lies and says
    // we can safely access memory.  We use SEH as a last resort.
    //
    __try
    {
        DstAddr = static_cast<BYTE*>(HdstubMapAddr(&Dest2));
        DEBUGGERMSG(HDZONE_MOVE, (L"  MoveMemory0: Map Dest Addr(0x%08x::0x%08X) -> %08X\r\n", Dest2.vm, Dest2.addr, DstAddr));
        SrcAddr = static_cast<BYTE*>(HdstubMapAddr(&Source2));
        DEBUGGERMSG(HDZONE_MOVE, (L"  MoveMemory0: Map Source Addr(0x%08X::0x%08x) -> %08X\r\n", Source2.vm, Source2.addr, SrcAddr));
        while (bytesLeft && DstAddr && SrcAddr) 
        {
            //NOTE: rollover will work, but do we want to allow it?
            srcBytesOnPage = PAGEALIGN_DOWN((DWORD)SrcAddr + VM_PAGE_SIZE) - (DWORD)SrcAddr;
            dstBytesOnPage = PAGEALIGN_DOWN((DWORD)DstAddr + VM_PAGE_SIZE) - (DWORD)DstAddr;
            bytesToCopy = min(srcBytesOnPage, dstBytesOnPage);
            bytesToCopy = min(bytesToCopy, bytesLeft);
            memcpy(DstAddr, SrcAddr, bytesToCopy);

            bytesLeft -= bytesToCopy;

            SrcAddr = SrcAddr + bytesToCopy;
            if(bytesLeft && IsPageAligned(SrcAddr))
            {
                HdstubUnMapAddr(&Source2);
                Source2.addr = (void *)PAGEALIGN_DOWN((DWORD)Source2.addr + VM_PAGE_SIZE);
                SrcAddr = static_cast<BYTE*>(HdstubMapAddr(&Source2));
                DEBUGGERMSG(HDZONE_MOVE, (L"  MoveMemory0: Map Source Addr(%08X) -> %08X\r\n", Source2.addr, SrcAddr));
            }

            DstAddr = DstAddr + bytesToCopy;                        
            if(bytesLeft && IsPageAligned(DstAddr))
            {
                HdstubUnMapAddr(&Dest2);
                Dest2.addr = (void *)PAGEALIGN_DOWN((DWORD)Dest2.addr + VM_PAGE_SIZE);
                DstAddr = static_cast<BYTE*>(HdstubMapAddr(&Dest2));
                DEBUGGERMSG(HDZONE_MOVE, (L"  MoveMemory0: Map Dest Addr(%08X) -> %08X\r\n", Dest2.addr, DstAddr));

            }
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        DEBUGGERMSG(HDZONE_ALERT, (L"  HdstubMoveMemory0: Exception encountered\r\n"));
        bytesLeft = Length;
    }
    
    if (DstAddr)
    {
        HdstubUnMapAddr(&Dest2);
    }
    
    if (SrcAddr)
    {
        HdstubUnMapAddr(&Source2);
    }

    //
    // Flush the instruction cache in case the write was into the instruction
    // stream.
    //

    DEBUGGERMSG(HDZONE_MOVE, (L"--MoveMemory0: %d bytes\r\n", Length - bytesLeft));
    return Length - bytesLeft;
}


//------------------------------------------------------------------------------
// Get VM Page Table Entry for a VA of given ProcVM
//------------------------------------------------------------------------------
BOOL GetPageTableEntryPtr (PPROCESS pVM, DWORD dwAddr, DWORD** ppPageTableEntry)
{
    BOOL fRet = FALSE;

    if (GetPageTableEntryPtrNoCheck (pVM, dwAddr, ppPageTableEntry) &&
            IsPageCommitted (**ppPageTableEntry))
    {
        fRet = TRUE;
    }
    return fRet;
}


BOOL GetPageTableEntryPtrNoCheck (PPROCESS pVM, DWORD dwAddr, DWORD **ppPageTableEntry)
{
    BOOL fRet = FALSE;
    DWORD idxdir = VA2PDIDX (dwAddr);   // 1st-level PT index
    DWORD idx2nd = VA2PT2ND (dwAddr);   // 2nd-level PT index
    PPAGETABLE pptbl = GetPageTable(pVM->ppdir, idxdir);
    if (ppPageTableEntry) *ppPageTableEntry = NULL; // Init
    DEBUGGERMSG (HDZONE_MOVE, (TEXT("++KdpGetPageTableEntryPtr: va=%08X in Proc %08X(%s))\r\n"), 
                 dwAddr, pVM, pVM ? pVM->lpszProcName : L"null"));
    if (pptbl)
    {
        DEBUGGERMSG (HDZONE_MOVE, (TEXT("  KdpGetPageTableEntryPtr: pptbl=%08X\r\n"), pptbl));
        if (ppPageTableEntry)
        {
            *ppPageTableEntry = &(pptbl->pte[idx2nd]);
            DEBUGGERMSG (HDZONE_MOVE, (TEXT("  KdpGetPageTableEntryPtr: pPageTableEntry=%08X\r\n"), *ppPageTableEntry));
            fRet = TRUE;
        }
    }
    DEBUGGERMSG (HDZONE_MOVE, (TEXT("--KdpGetPageTableEntryPtr: ret=%i\r\n"), fRet));
    return fRet;
}

