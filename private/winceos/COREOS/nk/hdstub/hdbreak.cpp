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

#include "hdstub_p.h"
#include "hdbreak.h"


ROMMap ROMMap::Entries[ROMMap::REMAP_PAGES];


void ROMMap::AddRef()
{
    ++RefCount;
}


void ROMMap::Release()
{
    --RefCount;
}


HRESULT ROMMap::PatchVM()
{
    DEBUGGERMSG(HDZONE_BP0, (L"++ROMMap::PatchVM 0x%08X::0x%08X\r\n", VM, VA));

    HRESULT hr = S_OK;
    PTENTRY *OriginalPTE;
    PTENTRY *CowPTE;

    if (!GetPageTableEntryPtr(VM, (DWORD)VA, &OriginalPTE))
    {
        hr = KDBG_E_ROMBPFAIL;
        goto Error;
    }

    if (!GetPageTableEntryPtr(g_pprcNK, (DWORD)MappedPageAddr, &CowPTE))
    {
        hr = KDBG_E_ROMBPFAIL;
        goto Error;
    }

    // Do the Copy on Write.
    MEMADDR dst, src;
    SetDebugAddr(&dst, MappedPageAddr);
    SetVirtAddr(&src, VM, VA);
    ULONG MoveSize = DD_MoveMemory0(&dst, &src, VM_PAGE_SIZE);
    if(MoveSize != VM_PAGE_SIZE)
    {
        hr = KDBG_E_ROMBPFAIL;
        goto Error;
    }
    
    // Patch up the original VM
    OriginalEntry = *OriginalPTE;
    *OriginalPTE = PFNfromEntry(*CowPTE) | (PG_PERMISSION_MASK & *OriginalPTE);

    // Now go and fix up potential other VMs
    PPROCESS Vm = g_pprcNK;
    PTENTRY const OldPFN = PFNfromEntry(OriginalEntry);
    PTENTRY const NewPFN = PFNfromEntry(*CowPTE);
    do
    {        
        PTENTRY *ProcPTE;
        if (GetPageTableEntryPtr(Vm, (DWORD)VA, &ProcPTE) &&
            PFNfromEntry(*ProcPTE) == OldPFN)
        {
            //Note: we don't have to worry about ref-counting the PFN.
            //The kernel won't free the physical page the module is XIP.
            *ProcPTE = NewPFN | (PG_PERMISSION_MASK & *ProcPTE);
        }
        Vm = (PPROCESS) Vm->prclist.pFwd;
    } while (Vm != g_pprcNK);

    // Make sure all of the changes to the page table above get reflected.
    NKCacheRangeFlush(NULL,0,CACHE_SYNC_ALL);

Exit:
    DEBUGGERMSG(HDZONE_BP0 || FAILED(hr), (L"--ROMMap::PatchVM hr=0x%08X for 0x%08X::0x%08X \r\n", hr, VM, VA));
    return hr;
Error:
    goto Exit;
}


HRESULT ROMMap::RestoreVM()
{
    DEBUGGERMSG(HDZONE_BP0, (L"++ROMMap::RestoreVM 0x%08X::0x%08X\r\n", VM, VA));    

    HRESULT hr = S_OK;
    PTENTRY *OriginalPTE;
    PTENTRY *CowPTE;
   
    if (!GetPageTableEntryPtr(VM, (DWORD)VA, &OriginalPTE))
    {
        hr = KDBG_E_ROMBPFAIL;
        goto Error;
    }

    if (!GetPageTableEntryPtr(g_pprcNK, (DWORD)MappedPageAddr, &CowPTE))
    {
        hr = KDBG_E_ROMBPFAIL;
        goto Error;
    }

    *OriginalPTE = OriginalEntry;

    // Now go and fix up potential other VMs
    PPROCESS Vm = g_pprcNK;
    PTENTRY const CowPFN = PFNfromEntry(*CowPTE);
    PTENTRY const OldPFN = PFNfromEntry(OriginalEntry);
    do
    {
        PTENTRY *ProcPTE;
        if (GetPageTableEntryPtr(Vm, (DWORD)VA, &ProcPTE) &&
            PFNfromEntry(*ProcPTE) == CowPFN)
        {            
            *ProcPTE = OldPFN | (PG_PERMISSION_MASK & *ProcPTE);
        }
        Vm = (PPROCESS) Vm->prclist.pFwd;
    } while (Vm != g_pprcNK);

    // Make sure all of the changes to the page table above get reflected.
    NKCacheRangeFlush(NULL,0,CACHE_SYNC_ALL);

Exit:
    DEBUGGERMSG(HDZONE_BP0 || FAILED(hr), (L"--ROMMap::RestoreVM hr=0x%08X for 0x%08X::0x%08X \r\n", hr, VM, VA));
    return hr;
Error:
    goto Exit;
}


HRESULT ROMMap::CopyPage(PPROCESS vm, void *va)
{
    DEBUGGERMSG(HDZONE_BP0, (L"++ROMMap::CopyPage 0x%08X::0x%08X\r\n", vm, va));    

    HRESULT hr = S_OK;
    void *vaPageAligned = (void *)PAGEALIGN_DOWN((DWORD)va);        
    
    if ((ULONG)vaPageAligned >= VM_SHARED_HEAP_BASE)
    {
        hr = KDBG_E_ROMBPKERNEL;
        goto Error;
    }

    //have we already mapped in this page? if so, just addref
    ROMMap *entry = FindPhysPage(vm, vaPageAligned);
    if (entry)
    {
        entry->AddRef();
        hr = S_OK;
        goto Exit;
    }

    DWORD dwPFN = GetPFNOfProcess(vm, vaPageAligned);
    if(dwPFN == INVALID_PHYSICAL_ADDRESS)
    {
        hr = KDBG_E_ROMBPFAIL;
        goto Error;
    }

    DEBUGGERMSG(HDZONE_BP0, (L"  ROMMap::CopyPage, 0x%08X::0x%08X-->0x%08x\r\n", vm, va, dwPFN));    
    entry = New(vm, vaPageAligned, dwPFN);
    if (entry == NULL)
    {
        hr = KDBG_E_ROMBPRESOURCE;
        goto Error;
    }

    hr = entry->PatchVM();
    if (FAILED(hr))
    {
        goto Error;
    }

Exit:
    DEBUGGERMSG(HDZONE_BP0 || FAILED(hr), (L"--ROMMap::CopyPage hr=0x%08X for 0x%08X::0x%08X \r\n", hr, vm, va));
    return hr;
Error:
    goto Exit;
}


HRESULT ROMMap::FreePage(PPROCESS vm, void *va)
{
    DEBUGGERMSG(HDZONE_BP0, (L"++ROMMap::FreePage 0x%08X::0x%08x\r\n", vm, va));    

    HRESULT hr = S_OK;
    void *vaPageAligned = (void *)PAGEALIGN_DOWN((DWORD)va);

    ROMMap *entry = FindPhysPage(vm, vaPageAligned);
    if (!entry)
    {
        hr = KDBG_E_ROMBPFAIL;
        goto Error;
    }

    entry->Release();
    if (entry->RefCount == 0)
    {
        hr = entry->RestoreVM();
        if (FAILED(hr))
            goto Error;
    }
    
Exit:
    DEBUGGERMSG(HDZONE_BP0 || FAILED(hr), (L"--ROMMap::FreePage hr=0x%08X for 0x%08X::0x%08x\r\n", hr, vm, va));    
    return hr;
Error:
    goto Exit;
}


ROMMap *ROMMap::FindPhysPage(PPROCESS vm, void *va)
{    
    DEBUGGERMSG(HDZONE_BP0, (L"++ROMMap::FindPhysPage 0x%08x::0x%08x\r\n", vm, va));

    void *vaPageAligned = (void *)PAGEALIGN_DOWN((DWORD)va);            
    DWORD dwPFN = GetPFNOfProcess(vm, vaPageAligned);
    if(dwPFN != INVALID_PHYSICAL_ADDRESS)
    {    
        for (ULONG i = 0; i < REMAP_PAGES; ++i)
        {
            if (Entries[i].RefCount && Entries[i].dwMappedPagePFN == dwPFN)
            {
                DEBUGGERMSG(HDZONE_BP0, (L"--ROMMap::FindPhysPage Entry Index: %d\r\n", i));
                return &Entries[i];
            }
        }
    }
    DEBUGGERMSG(HDZONE_BP0, (L"--ROMMap::FindPhysPage Page not found\r\n"));
    return NULL;
}


ROMMap *ROMMap::New(PPROCESS vm, void *va, DWORD dwPFN)
{
    DEBUGGERMSG(HDZONE_BP0, (L"++ROMMap::New mapping 0x%08x::0x%08x (pfn:0x%08x)\r\n", vm, va, dwPFN));
    for (ULONG i = 0; i < REMAP_PAGES; ++i)
    {
        if (Entries[i].RefCount == 0)
        {
            Entries[i].RefCount = 1;
            Entries[i].dwPFN = dwPFN;
            Entries[i].VM = vm;
            Entries[i].VA = va;
            Entries[i].OriginalEntry = 0;
            DEBUGGERMSG(HDZONE_BP0, (L"--ROMMap::New mapped 0x%08x::0x%08x (pfn:0x%08x) to 0x%08x (pfn:0x%08x)\r\n", vm, va, dwPFN, Entries[i].MappedPageAddr, Entries[i].dwMappedPagePFN));
            return &Entries[i];
        }
    }
    DEBUGGERMSG(HDZONE_BP0 || HDZONE_ALERT, (L"--ROMMap::New No Free Pages!\r\n"));
    return NULL;
}


BOOL ROMMap::Init()
{
    DEBUGGERMSG(HDZONE_BP0, (L"++ROM:Init\r\n"));

    BOOL fSuccess = TRUE;
    void *Mem = VMAlloc(g_pprcNK, NULL, VM_PAGE_SIZE * REMAP_PAGES, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (Mem)
    {
        for (ULONG i = 0; i < REMAP_PAGES && fSuccess; ++i)
        {
            Entries[i].MappedPageAddr = static_cast<BYTE*>(Mem) + i*VM_PAGE_SIZE;
            Entries[i].dwMappedPagePFN = GetPFNOfProcess(g_pprcNK, (void *)PAGEALIGN_DOWN((DWORD)Entries[i].MappedPageAddr));
            if(INVALID_PHYSICAL_ADDRESS == Entries[i].dwMappedPagePFN)
            {
                DEBUGGERMSG(HDZONE_BP0 ||HDZONE_ALERT, (L"  ROM:Init, failed to convert to PFN: 0x%08x\r\n", Entries[i].dwMappedPagePFN));
                fSuccess = FALSE;
            }            
        }        
    }
    else
    {
        DEBUGGERMSG(HDZONE_BP0 || HDZONE_ALERT, (L"  ROM:Init, failed to reserve pages for remap.\r\n"));        
        fSuccess = FALSE;
    }

    DEBUGGERMSG(HDZONE_BP0 || !fSuccess, (L"--ROM:Init fSuccess=%d\r\n", fSuccess));
    return fSuccess;
}


BYTE BreakpointImpl::RefCount[BPMax];
BreakpointImpl BreakpointImpl::Breakpoints[BPMax];
BreakpointImpl *BreakpointImpl::CurrentlyDisabledBreakpoint;
BOOL BreakpointImpl::fDisableAllOnHalt = TRUE;

HRESULT BreakpointImpl::CopyPageOnWrite()
{
    DEBUGGERMSG(HDZONE_BP0, (L"++BP::CopyPageOnWrite\r\n"));
    HRESULT hr = S_OK;

//TODO: DON"T SUBMIT THIS!!!
#if 0
    if (!fIntrusivePaging)
    {
        hr = KDBG_E_BPINTRUSIVE;
        goto Exit;
    }
#endif
    hr = ROMMap::CopyPage(Address.vm, Address.addr);
    if (SUCCEEDED(hr))
    {
        fRomBP = TRUE;
    }


//Exit:
    DEBUGGERMSG(HDZONE_BP0 || FAILED(hr), (L"--BP::CopyPageOnWrite: hr=0x%08x\r\n", hr));
    return hr;

}


HRESULT BreakpointImpl::FreeCopiedPage()
{
    DEBUGGERMSG(HDZONE_BP0, (L"++BP::FreeCopiedPage\r\n"));
    HRESULT hr = S_OK;

    if (fRomBP)
    {
        hr = ROMMap::FreePage(Address.vm, Address.addr);
        if (SUCCEEDED(hr))
        {
            fRomBP = FALSE;
        }
    }

    DEBUGGERMSG(HDZONE_BP0 || FAILED(hr), (L"--BP::FreeCopiedPage hr:0x%08X\r\n", hr));
    return hr;
}


//
// Determine whether the breakpoint is set at the moment by reading the address
// and comparing the read instruction against the known breakpoint instruction.
//
HRESULT BreakpointImpl::GetState(enum BreakpointState *st)
{
    DEBUGGERMSG(HDZONE_BP0, (L"+BP[%d]:GetState %08X::%08X\r\n", (this - Breakpoints), Address.vm, Address.addr));

    ULONG ReadBuffer = 0;
    MEMADDR LocalBuf;
    ULONG MoveSize;
    HRESULT hr;

    SetDebugAddr(&LocalBuf, &ReadBuffer);
    MoveSize = DD_MoveMemory0(&LocalBuf, &Address, InstrSize);
    if (MoveSize != InstrSize)
    {
        // Why did we fail to get read?
        PTENTRY *pte;
        BOOL fHasPTE = GetPageTableEntryPtrNoCheck(Address.vm, (DWORD) Address.addr, &pte);
        if (fHasPTE)
        {
            DWORD dwPager = (*pte & PG_PAGER_MASK) >> PG_PAGER_SHIFT;
            if (dwPager == VM_PAGER_LOADER)
            {
                // Code page hasn't been paged in yet.
                *st = BPS_PAGEDOUT;
                hr = S_OK;
                goto Exit;
            }
        }

        // Other reason.
        hr = KDBG_E_BPREAD;
        goto Error;
    }

    DEBUGGERMSG(HDZONE_BP0, (L" BP[%d]:GetState (%08x)::(%08X) -> %08X\r\n",
                             this - Breakpoints, Address.vm, Address.addr, ReadBuffer));

    if (memcmp(&ReadBuffer, &BreakInstr, InstrSize) == 0)
    {
        *st = BPS_ACTIVE;
    }
    else
    {
        *st = BPS_INACTIVE;
    }

    hr = S_OK;

Exit:
    DEBUGGERMSG(HDZONE_BP0, (L"-BP[%d]:GetState hr=0x%08X for %d\r\n", (this - Breakpoints), hr, *st));
    return hr;
Error:
    goto Exit;
}


//
// Attempt to intelligently set the state of the breakpoint.
// If the breakpoint is already in the state that the caller desires, NOP it.
//
HRESULT BreakpointImpl::SetState(enum BreakpointState newState)
{
    DEBUGGERMSG(HDZONE_BP0, (L"+BP[%d]:SetState %08X::%08X, %d\r\n",
        (this - Breakpoints), Address.vm, Address.addr, newState));
    
    HRESULT hr;
    enum BreakpointState curState;

    KDASSERT(newState != BPS_PAGEDOUT);

    hr = GetState(&curState);
    if (FAILED(hr))
    {
        goto Error;
    }

    if (curState == BPS_PAGEDOUT)
    {
        // Can't do anything to a breakpoint that's paged out.  This
        // should be ok because
        // 1. We can rewrite the bp on page in, if newStatus ==
        // BPS_ACTIVE
        // 2. When paging in, page in is from ROM, which won't have a
        // breakpoint set.  if newStatus == BPS_INACTIVE.
        DEBUGGERMSG(HDZONE_BP0, (L" BP[%d]:SetState %08X::%08X, currently paged out\r\n", (this - Breakpoints), Address.vm, Address.addr));
        hr = S_OK;
        goto Exit;
    }

    if (curState == BPS_INACTIVE && newState == BPS_ACTIVE)
    {        
        // SAFE to call ReadInstruction here because we just checked the status
        // of the breakpoint in GetState() above...
        hr = ReadInstruction();
        if (FAILED(hr))
        {
            goto Error;
        }

        if (!fDisabled)
        {
            hr = WriteInstruction((BYTE*)&LastInstr, (BYTE*)&BreakInstr);               
            if (FAILED(hr))
            {
                goto Error;
            }
            DEBUGGERMSG(HDZONE_BP0, (L" BP[%d]:SetState %08X::%08X, wrote bp\r\n", (this - Breakpoints), Address.vm, Address.addr));
            fWritten = TRUE;
        }
    }
    else if (curState == BPS_ACTIVE && newState == BPS_INACTIVE)
    {
        if (fWritten || fDisabled)
        {
            // fWritten or fDisabled = true if we've successfully read the original
            // instruction before.  Therefore we're in a position to write back
            // a valid LastInstr.
            hr = WriteInstruction((BYTE*)&BreakInstr, (BYTE*)&LastInstr);
            if (FAILED(hr))
            {
                DBGRETAILMSG(HDZONE_ALERT, (L" BP[%d]:SetState %08X::%08X, failed to writeback\r\n",
                    (this - Breakpoints), Address.vm, Address.addr));
            }
        }
        fWritten = FALSE;
    }

    hr = S_OK;

Exit:
    DEBUGGERMSG(HDZONE_BP0, (L"-BP[%d]:SetState hr=0x%08X\r\n", (this - Breakpoints), hr));
    return hr;

Error:
    DBGRETAILMSG(HDZONE_ALERT, (L" BP[%d]:SetState (%08X::%08X) failed, hr=%08X, newState:%d\r\n",
        (this - Breakpoints), Address.vm, Address.addr, hr, newState)); 
    goto Exit;
}


//
// This method updates LastInstr!  Make sure it never ever gets called when
// a breakpoint is already set at the address.
//
// Can result in the loss of the original instruction underneath, especially if
// it reads in an aliased breakpoint at the same location.
//
HRESULT BreakpointImpl::ReadInstruction()
{
    DEBUGGERMSG(HDZONE_BP0, (L"+BP[%d]:Read\r\n", (this - Breakpoints)));
    
    HRESULT hr = S_OK;    
    MEMADDR Dst;
    DWORD ReadBuffer = 0;
    SetDebugAddr(&Dst, &ReadBuffer);
    ULONG MovedSize = DD_MoveMemory0(&Dst, &Address, InstrSize);
    if (MovedSize != InstrSize)
    {
        hr = KDBG_E_BPREAD;
        goto Error;
    }
    Sanitize(&ReadBuffer, &Address, InstrSize);
    LastInstr = ReadBuffer;
    hr = S_OK;

Exit:
    DEBUGGERMSG(HDZONE_BP0, (L"-BP[%d]:Read hr=0x%08X for 0x%08x::0x%08x->0x%08x\r\n",
                             (this - Breakpoints), hr, Address.vm, Address.addr, LastInstr));
    return hr;
Error:
    DBGRETAILMSG(HDZONE_BP0, (L" BP[%d]:Read failed %08X::%08X, hr=%08X\r\n",
        (this - Breakpoints), Address.vm, Address.addr, hr));
    goto Exit;
}


//
// Intelligently write an instruction to the breakpoint's address.
// If OldInst is not at the location, fail because we don't know where we're writing.
// If NewIst is at the location, short cut success because we don't need to do work.
// If NewInst can't be read back, fail because we're probably trying to write to ROM.
//
HRESULT BreakpointImpl::WriteInstruction(BYTE *OldInst, BYTE *NewInst)
{
    DEBUGGERMSG(HDZONE_BP0, (L"+BP[%d]:Write 0x%08x::0x%08X\r\n",
                             (this - Breakpoints),
                             Address.vm,
                             Address.addr));
    
    HRESULT hr = S_OK;
    
    // Make sure the old instruction is still there.
    MEMADDR LocalBuf;
    ULONG ReadBuffer = 0;
    ULONG MoveSize;
    SetDebugAddr(&LocalBuf, &ReadBuffer);
    MoveSize = DD_MoveMemory0(&LocalBuf, &Address, InstrSize);
    if (MoveSize != InstrSize)
    {
        DEBUGGERMSG(HDZONE_ALERT, (L" BP[%d]:Write Failed to Read Instruction\r\n",
                                   (this - Breakpoints)));
        hr = KDBG_E_BPREAD;
        goto Error;
    }

    DEBUGGERMSG(HDZONE_BP0, (L" BP[%d]:Write Old Instruction at: 0x%08x::0x%08X=0x%08X\r\n",
                             (this - Breakpoints),
                             Address.vm,
                             Address.addr,
                             ReadBuffer));

    if (memcmp(&ReadBuffer, NewInst, InstrSize) == 0)
    {
        DEBUGGERMSG(HDZONE_BP0, (L" BP[%d]:Write BP already written, nop\r\n",
                                 (this - Breakpoints)));
        goto Exit;
    }

    if (memcmp(&ReadBuffer, OldInst, InstrSize))
    {
        DEBUGGERMSG(HDZONE_ALERT, (L" BP[%d]:Write Unexpected Old Instruction\r\n",
                                   (this - Breakpoints)));
        hr = KDBG_E_BPFAIL;
        goto Error;
    }

    // Write the new instruction
    SetDebugAddr(&LocalBuf, NewInst);
    MoveSize = DD_MoveMemory0(&Address, &LocalBuf, InstrSize);
    if (MoveSize != InstrSize)
    {
        DEBUGGERMSG(HDZONE_ALERT, (L" BP[%d]:Write Failed to Write new instruction\r\n",
                                   (this - Breakpoints)));
        hr = KDBG_E_BPFAIL;
        goto Error;
    }

    // Make sure the new instruction is there.
    SetDebugAddr(&LocalBuf, &ReadBuffer);
    ReadBuffer = 0;
    MoveSize = DD_MoveMemory0(&LocalBuf, &Address, InstrSize);
    if (MoveSize != InstrSize)
    {
        DEBUGGERMSG(HDZONE_ALERT, (L" BP[%d]:Write Failed to Read Back New Instruction\r\n",
                                   (this - Breakpoints)));
        hr = KDBG_E_BPFAIL;
        goto Error;
    }

    DEBUGGERMSG(HDZONE_BP0, (L" BP[%d]:Write  New Instruction at 0x%08x::0x%08X=0x%08X\r\n",
                             (this - Breakpoints),
                             Address.vm,
                             Address.addr,
                             ReadBuffer));

    if (memcmp(&ReadBuffer, NewInst, InstrSize) != 0)
    {
        DEBUGGERMSG(HDZONE_ALERT, (L" BP[%d]:Write Failed Readback instruction doesn't match new instruction\r\n",
                                   (this - Breakpoints)));
        hr = KDBG_E_BPWRITE;
        goto Exit;
    }

    hr = S_OK;
    
Exit:
    DEBUGGERMSG(HDZONE_BP0 || FAILED(hr), (L"-BP::Write  hr=0x%08X for 0x%08x::0x%08X\r\n",
                                           hr, Address.vm, Address.addr));
    return hr;
Error:
    goto Exit;
}


//
// Flag a breakpoint to be disabled.  When continuing, the debugger will
// make sure that the original instruction remains so it will
// not hit a breakpoint on that location.
//
// NOTE!  Only one breakpoint can be disabled at a time.
//
HRESULT BreakpointImpl::Disable()
{
    DEBUGGERMSG(HDZONE_BP0, (L"+BP[%d]:Disable 0x%08x::0x%08X thread=0x%08x\r\n",
                    (this - Breakpoints), Address.vm, Address.addr, hTh));
    HRESULT hr = S_OK;

    // Read the instruction so that we can guarantee that the original instruction
    // entry will be valid.  SAFE to call back because this method is called
    // when all of the breakpoints are disabled, and therefore all of the original
    // instructions are in place.
    hr = ReadInstruction();
    if (FAILED(hr))
    {
        // Failed to read the breakpoint.  Something really really bad happened.
        // At this point, we can't successfully keep this breakpoint disabled
        // at all.
        goto Error;
    }

    KDASSERT(BreakpointImpl::CurrentlyDisabledBreakpoint == NULL);
    BreakpointImpl::CurrentlyDisabledBreakpoint = this;
    fDisabled = TRUE;
    hr = S_OK;
Exit:
    DEBUGGERMSG(HDZONE_BP0 || FAILED(hr), (L"-BP[%d]:Disable hr=0x%08X for 0x%08x::0x%08X thread=0x%08x\r\n",
                                           (this - Breakpoints), hr, Address.vm, Address.addr, hTh));
    return hr;
Error:
    goto Exit;
}


HRESULT BreakpointImpl::Enable()
{
    DEBUGGERMSG(HDZONE_BP0, (L"++BP::Enable 0x%08x::0x%08X thread=0x%08x\r\n",
                             Address.vm, Address.addr, hTh));

    KDASSERT(BreakpointImpl::CurrentlyDisabledBreakpoint == this);
    BreakpointImpl::CurrentlyDisabledBreakpoint = NULL;
    fDisabled = FALSE;

    HRESULT hr = S_OK;
    if (!DisableAllOnHalt())
    {
        hr = SetState(BPS_ACTIVE);
    }

    DEBUGGERMSG(HDZONE_BP0, (L"--BP::Enable\r\n"));
    return hr;
}

BreakpointImpl *BreakpointImpl::Find(MEMADDR *Address, ULONG ThreadID, BOOL SixteenBit)
{
    DEBUGGERMSG(HDZONE_BP0, (L"++BP::Find 0x%08x::0x%08x thread:0x%08x SixteenBit:%d\r\n", Address->vm, Address->addr, ThreadID, SixteenBit));

    KDASSERT(AddrType(Address) == MF_TYPE_VIRTUAL);
    
    for (ULONG i = 0; i < BPMax; ++i)
    {
        if (RefCount[i] &&
              AddrEq(&Breakpoints[i].Address, Address) &&
              Breakpoints[i].hTh == ThreadID)
        {
            // Match.
            KDASSERT(!SixteenBit || (Breakpoints[i].InstrSize == CpuBreakSize16));
            DEBUGGERMSG(HDZONE_BP0, (L"--BP::Find. Index:%d\r\n", i));
            return &Breakpoints[i];
        }
    }

    DEBUGGERMSG(HDZONE_BP0, (L"--BP::Find. Not Found\r\n"));
    return NULL;
}


BreakpointImpl *BreakpointImpl::New(MEMADDR *Address, ULONG ThreadID, BOOL SixteenBit)
{
    DEBUGGERMSG(HDZONE_BP0, (L"++BP::New 0x%08x::0x%08x thread:0x%08x SixteenBit:%d\r\n", Address->vm, Address->addr, ThreadID, SixteenBit));

    // Search for a new breakpoint.
    for (ULONG i = 0; i < BPMax; ++i)
    {
        if (RefCount[i] == 0)
        {
            // Allocate the new breakpoint structure.
            BreakpointImpl *Bp = &Breakpoints[i];
            RefCount[i] = 1;
            memset(Bp, 0, sizeof (*Bp));
            Bp->Address = *Address;
            Bp->hTh = ThreadID;
            Bp->BreakInstr = SixteenBit? CpuBreak16 : CpuBreak;
            Bp->InstrSize = SixteenBit? CpuBreakSize16 : CpuBreakSize;
            DEBUGGERMSG(HDZONE_BP0, (L"--BP::New Index:%d\r\n", i));
            return Bp;
        }
    }
    
    DEBUGGERMSG(HDZONE_BP0 || HDZONE_ALERT, (L"--BP::New Out of Breakpoints!\r\n"));
    return NULL;
}


void BreakpointImpl::Reset()
{
    DEBUGGERMSG(HDZONE_BP0, (L"++BP::Reset\r\n"));

    // Disable all breakpoints and writeback all instructions.
    for (ULONG i = 0; i < BPMax; ++i)
    {
        if (RefCount[i])
        {
            Breakpoints[i].SetState(BPS_INACTIVE);
        }
    }

    // Zero the entire structure.
    memset(Breakpoints, 0, sizeof(Breakpoints));

    DEBUGGERMSG(HDZONE_BP0, (L"--BP::Reset\r\n"));
}


// Handle page in.
// Resolve each page in to a physical page and write breakpoints that match
// physical addresses.
BOOL BreakpointImpl::OnPageIn(DWORD PageAddr, DWORD NumPages, BOOL Writeable)
{
    // TODO: FreezeSystem may be too intrusive.  If this is the case, I think
    // TODO: a spinlock will work.  We will have to update FreezeSystem to
    // TODO: also temporarily grab the spinlock before calling StopAllOtherCPUs()
    DD_FreezeSystem();
    DEBUGGERMSG(HDZONE_BP0, (L"+BP:PageIn %08X, %d, %d\r\n", PageAddr, NumPages, Writeable));
    
    for (ULONG Page = 0; Page < NumPages; ++Page)
    {
        ULONG const CurPageAddr = PageAddr + (Page*VM_PAGE_SIZE);
        DWORD const PagePA = GetPhysicalAddress(pVMProc, (void *) CurPageAddr);
        if (PagePA == INVALID_PHYSICAL_ADDRESS)
        {
            // Something really bad happened.
            DBGRETAILMSG(HDZONE_ALERT, (L" BP:PageIn: %08X::%08X invalid\r\n",
                         pVMProc, CurPageAddr));
            goto Error;
        }
        
        DWORD const NextPagePA = PagePA + VM_PAGE_SIZE;
        if (NextPagePA < PagePA)
        {
            // Rolled over?
            DBGRETAILMSG(HDZONE_ALERT, (L" BP:PageIn: Succ(%08X) == %08X??\r\n",
                         PagePA, NextPagePA));
            goto Error;
        }
        
        for (ULONG i = 0; i < BPMax; ++i)
        {
            if (RefCount[i] == 0)
            {
                continue;
            }

            DWORD const BpPA = AddrToPA(&Breakpoints[i].Address);
            if (BpPA < PagePA || NextPagePA <= BpPA || 
                Breakpoints[i].fDisabled)
            {
                continue;
            }

            HRESULT hr = Breakpoints[i].SetState(BPS_ACTIVE);
            if (FAILED(hr))
            {
                DEBUGGERMSG(HDZONE_BP0, (L" BP[%d]:PageIn: %08X::%08X failed SetState %08X\r\n",
                    i, Breakpoints[i].Address.vm, Breakpoints[i].Address.addr, hr));
            }
        }
    }

    WriteBackDisabled();
    
Exit:
    DEBUGGERMSG(HDZONE_BP0, (L"-BP:PageIn\r\n"));
    DD_ThawSystem();

    // Never interrupt the page in notification.
    return FALSE;

Error:
    DBGRETAILMSG(HDZONE_ALERT, (L"!BP:PageIn failed\r\n"));
    goto Exit;
}


HRESULT BreakpointImpl::Add(MEMADDR *Address, ULONG ThreadID, BOOL SixteenBit, Breakpoint **Handle)
{
    DEBUGGERMSG(HDZONE_BP0, (L"+BP::Add 0x%08x::0x%08x threadID:0x%08x  16bit:%d\r\n", Address->vm, Address->addr, ThreadID, SixteenBit));
    HRESULT hr = E_FAIL;
    BreakpointImpl *bp = NULL;
    BOOL fRomBP = FALSE;

    if((AddrType(Address) != MF_TYPE_VIRTUAL || Address->vm == NULL))
    {
        // Only support Virtual address on breakpoints.
        // VM needs to be defined for an address by this point.
        DEBUGGERMSG(HDZONE_ALERT, (L" BP::Add 0x%08x::0x%08x threadID:0x%08x  16bit:%d\r\n", Address->vm, Address->addr, ThreadID, SixteenBit));
        hr = E_INVALIDARG;
        goto Error;
    }

    bp = Find(Address, ThreadID, SixteenBit);
    if (bp)
    {
        DEBUGGERMSG(HDZONE_BP0, (L" BP:Add, found existing BP %08X\r\n", bp));
        ++RefCount[bp - Breakpoints];
        *Handle = bp;
        hr = S_OK;
        goto Exit;
    }

    //
    // Allocating a completely new breakpoint
    //
    bp = New(Address, ThreadID, SixteenBit);
    if (bp == NULL)
    {
        hr = KDBG_E_BPRESOURCE;
        goto Error;
    }
    
    //Is it worth calling kdpIsROM?  We can still figure out if it's a ROM breakpoint because the write will fail.
    fRomBP = kdpIsROM(bp->Address.vm, bp->Address.addr, bp->InstrSize);
    if(!fRomBP)
    {
        //check to see if we already copied a ROM page into RAM.
        ROMMap *pTemp = ROMMap::FindPhysPage(bp->Address.vm, bp->Address.addr);
        if(pTemp != NULL)
        {
            fRomBP = TRUE;
        }
    }
    
    if(!fRomBP)
    {
        // Make sure we can set the breakpoint.
        hr = bp->SetState(BPS_ACTIVE);
        if (hr == KDBG_E_BPWRITE)
        {
            // Could not write breakpoint.  Try creating a rom bp and try setting bp.            
            fRomBP = TRUE;
        }
        else if (FAILED(hr))
        {
            goto Error;
        }
    }

    if(fRomBP)
    {
        // Set up the ROM page swap.
        hr = bp->CopyPageOnWrite();
        if(FAILED(hr))
        {
            goto Error;
        }

        // And see if the new code page works.
        hr = bp->SetState(BPS_ACTIVE);
        if(FAILED(hr))
        {
            goto Error;
        }
    }
    
    // Disable the breakpoint so it won't get hit by the debugger.
    hr = bp->SetState(BPS_INACTIVE);
    if (FAILED(hr))
    {
        // On failure, other than failure to read the breakpoint (Assumption
        // being that the page that the breakpoint is on is paged out...),
        // clean up, bail out.
        goto Error;
    }

    // Brand new breakpoint successfully set at this point
    *Handle = bp;
    hr = S_OK;

Exit:
    DEBUGGERMSG(HDZONE_BP0 || FAILED(hr), (L"--BP::Add hr=0x%08X handle=0x%08x RomBP:%d for 0x%08x::0x%08x threadID:0x%08x\r\n", hr, *Handle, fRomBP, Address->vm, Address->addr, ThreadID));
    return hr;
Error:
    if(bp != NULL)
    {
        Delete(bp);
    }
    *Handle = NULL;
    goto Exit;
}


HRESULT BreakpointImpl::Delete(Breakpoint *Handle)
{
    DEBUGGERMSG(HDZONE_BP0, (L"++BP::Del 0x%08X\r\n", Handle));

    HRESULT hr = S_OK;
    BreakpointImpl *bp = (BreakpointImpl *)Handle;

    DEBUGGERMSG(HDZONE_BP0, (L"  BP::Del 0x%08X::0x%08x thread:0x%08x\r\n", bp->Address.vm, bp->Address.addr, bp->hTh));
    DWORD ibp = bp - Breakpoints;
    if (bp < Breakpoints || ibp >= BPMax)
    {
        DBGRETAILMSG(HDZONE_ALERT, (L" BP:Del, invalid handle %08X\r\n", Handle));
        hr = E_HANDLE;
        goto error;
    }
          
    --RefCount[ibp];
    if (RefCount[ibp] == 0)
    {
        hr = bp->SetState(BPS_INACTIVE);
        hr = bp->FreeCopiedPage();
    }

    hr = S_OK;

exit:
    DEBUGGERMSG(HDZONE_BP0, (L"--BP::Del hr=0x%08X for 0x%08X::0x%08x thread:0x%08x\r\n",
                             hr, bp->Address.vm, bp->Address.addr, bp->hTh));
    return hr;

error:
    DBGRETAILMSG(HDZONE_ALERT, (L" BP:Del failed, %08X\r\n", hr));
    goto exit;
}


HRESULT BreakpointImpl::EnableAll()
{
    DEBUGGERMSG(HDZONE_BP0, (L"+BP:EnableAll\r\n"));
    HRESULT hr;

    // Turn on all of the breakpoints.
    for (ULONG i = 0; i < BPMax; ++i)
    {
        if (RefCount[i] > 0 && !Breakpoints[i].fDisabled)
        {
            hr = Breakpoints[i].SetState(BPS_ACTIVE);
            if (FAILED(hr))
            {
                DEBUGGERMSG(HDZONE_BP0 || FAILED(hr), (L" BP:EnableAll hr=0x%08X failed for bp %d hr:0x%08x\r\n", hr, i));
            }
        }
    }

    WriteBackDisabled();
    
    DEBUGGERMSG(HDZONE_BP0, (L"-BP:EnableAll\r\n"));
    return S_OK;
}


// Go back and disable the breakpoint that has been marked as disabled by
// writing back it's original value.
//
// The original value is read in in Disable().
HRESULT BreakpointImpl::WriteBackDisabled()
{
    HRESULT hr;

    DEBUGGERMSG(HDZONE_BP0, (L"+BP:WriteBackDisabled\r\n"));
    
    if (CurrentlyDisabledBreakpoint != NULL)
    {
        ULONG Idx = CurrentlyDisabledBreakpoint - Breakpoints;
        KDASSERT(Idx < BPMax);
        KDASSERT(RefCount[Idx] > 0);
        if (Idx < BPMax && RefCount[Idx] > 0)
        {
            DEBUGGERMSG(HDZONE_BP0, (L" BP[%d]:WriteBackDisabled, Writing back Last:%08X @0x%08x::0x%08x thread:0x%08x\r\n",
                            Idx,
                            Breakpoints[Idx].LastInstr,
                            Breakpoints[Idx].Address.vm,
                            Breakpoints[Idx].Address.addr,
                            Breakpoints[Idx].hTh));
            hr = Breakpoints[Idx].WriteInstruction((BYTE*)&Breakpoints[Idx].BreakInstr,
                                                 (BYTE*)&Breakpoints[Idx].LastInstr);
            if (FAILED(hr))
            {
                DEBUGGERMSG(HDZONE_ALERT, (L" BP[%d]:WriteBackDisabled hr=0x%08X failed to write back, 0x%08x::0x%08x thread:0x%08x\r\n",
                                           Idx,
                                           hr,
                                           Breakpoints[Idx].Address.vm,
                                           Breakpoints[Idx].Address.addr,
                                           Breakpoints[Idx].hTh));
                goto Error;
            }
        }
    }

    hr = S_OK;

Exit:
    DEBUGGERMSG(HDZONE_BP0, (L"-BP:WriteBackDisabled, hr=%08X\r\n", hr));
    return hr;

Error:
    DBGRETAILMSG(HDZONE_ALERT, (L" BP:WriteBackDisabled, hr=%08X\r\n", hr));
    goto Exit;
}


HRESULT BreakpointImpl::DisableAll()
{
    DEBUGGERMSG(HDZONE_BP0, (L"++BP::DisableAll\r\n"));
    HRESULT hr;

    for (ULONG i = 0; i < BPMax; ++i)
    {
        BreakpointImpl *bp = &Breakpoints[i];
        if (RefCount[i] && !bp->fDisabled)
        {
            hr = bp->SetState(BPS_INACTIVE);
            if (FAILED(hr))
            {
                DEBUGGERMSG(HDZONE_BP0 || FAILED(hr), (L"  BP::DisableAll hr=0x%08X for bp 0x%08x::0x%08x thread:0x%08x\r\n",
                    hr, bp->Address.vm, bp->Address.addr, bp->hTh));
            }
        }
    }
    DEBUGGERMSG(HDZONE_BP0, (L"--BP::DisableAll\r\n"));
    return S_OK;
}


// This function allows the caller to walk through the entire
// breakpoint list and only return breakpoint entries that match the
// address.  The address match is done based on physical address.
HRESULT BreakpointImpl::Find(MEMADDR *Address, Breakpoint *StartBp, Breakpoint **Bp)
{
    DEBUGGERMSG(HDZONE_BP0, (L"++BP::Find Address: 0x%08x::0x%08x StartBp:0x%08x Bpt: 0x%08x\r\n", Address->vm, Address->addr, StartBp, Bp));    
    KDASSERT(AddrType(Address) == MF_TYPE_VIRTUAL);

    HRESULT hr = KDBG_E_BPNOTFOUND; 
    *Bp = 0;

    ULONG IdxStart;
    if (StartBp)
    {
        IdxStart = (StartBp - Breakpoints) + 1;
    }
    else
    {
        IdxStart = 0;
    }

    DWORD physAddr = AddrToPA(Address);
    if (physAddr == INVALID_PHYSICAL_ADDRESS)
    {
        hr = KDBG_E_BPFAIL;
        goto Error;
    }    
    
    // We never want odd offsets in ARM/Thumb/Thumb2 to be used when
    // setting breakpoints
#if defined(ARM) || defined(MIPS)
    physAddr &= CLEAR_BIT0;
#endif

    if (IdxStart > BPMax)
    {
        hr = KDBG_E_BPFAIL;
        goto Error;
    }

    for (ULONG i = IdxStart; i < BPMax; ++i)
    {
        if (RefCount[i] > 0)
        {
            DWORD bpPA = AddrToPA(&Breakpoints[i].Address);
            if (physAddr == bpPA)
            {
                // Match.
                DEBUGGERMSG(HDZONE_BP0, (L"  BP::Find 0x%08x::0x%08x =? 0x%08x::0x%08x\r\n",
                                         Address->vm, Address->addr,
                                         Breakpoints[i].Address.vm, Breakpoints[i].Address.addr));
                *Bp = &Breakpoints[i];
                hr = S_OK;
                goto Exit;                    
            }
        }
    }

Exit:
    DEBUGGERMSG(HDZONE_BP0 || (FAILED(hr) && hr != KDBG_E_BPNOTFOUND), (L"--BP::Find Address: hr=0x%08X for 0x%08x::0x%08x StartBp:0x%08x Bpt: 0x%08x\r\n",
        hr, Address->vm, Address->addr, StartBp, Bp));    
    return hr;
Error:
    goto Exit;
}

HRESULT BreakpointImpl::Disable(Breakpoint *Bp)
{
    BreakpointImpl *BpI = (BreakpointImpl *)Bp;
    HRESULT hr = BpI->Disable(); 
    return hr;
}

HRESULT BreakpointImpl::Enable(Breakpoint *Bp)
{
    BreakpointImpl *BpI = (BreakpointImpl *)Bp;
    HRESULT hr = BpI->Enable();
    return hr;
}

BOOL OnEmbeddedBreakpoint()
{
    DEBUGGERMSG(HDZONE_BP0, (L"++BP::OnEmbeddedBreakpoint\r\n"));
    MEMADDR Dst, Src;
    ULONG CurrentInstruction;

    BOOL const SixteenBit = Currently16Bit();
    ULONG const Match = SixteenBit? BreakpointImpl::CpuBreak16 : 
                                    BreakpointImpl::CpuBreak;
    ULONG const Length = SixteenBit? BreakpointImpl::CpuBreakSize16 :
                                     BreakpointImpl::CpuBreakSize;

    SetDebugAddr(&Dst, &CurrentInstruction);


    ULONG ProgramCounter = (DWORD)CONTEXT_TO_PROGRAM_COUNTER(DD_ExceptionState.context);

    // We never want odd offsets in ARM/Thumb/Thumb2 to be used when
    // setting breakpoints
    if (SixteenBit)
    {
        // Make sure the LSB of the program counter isn't odd in the
        // case of 16-bit mode.
        ProgramCounter &= CLEAR_BIT0;
    }

    SetVirtAddr(&Src, PcbGetVMProc(), (void *)ProgramCounter);

    ULONG MoveSize = DD_MoveMemory0(&Dst, &Src, Length);
    BOOL fRet = FALSE;
    if(MoveSize == Length)
    {
        BreakpointImpl::Sanitize(&CurrentInstruction, &Src, Length);
        fRet = memcmp(&CurrentInstruction, &Match, Length) == 0;
    }
    DEBUGGERMSG(HDZONE_BP0, (L"--BP::OnEmbeddedBreakpoint %d\r\n", fRet));
    return fRet;
 }

#ifdef DEBUG
HRESULT BreakpointImpl::DumpBreakpoint(Breakpoint *Bp)
{
    DEBUGGERMSG(HDZONE_BP0, (L"Breakpoint 0x%08x\r\n", Bp));
    DEBUGGERMSG(HDZONE_BP0, (L"vm:0x%08x\r\n", Bp->Address.vm));
    DEBUGGERMSG(HDZONE_BP0, (L"hTh:0x%08x\r\n", Bp->hTh));
    DEBUGGERMSG(HDZONE_BP0, (L"Address:0x%08x\r\n", Bp->Address.addr));
    DEBUGGERMSG(HDZONE_BP0, (L"BreakInst:0x%08x\r\n", Bp->BreakInstr));
    DEBUGGERMSG(HDZONE_BP0, (L"LastInst:0x%08x\r\n", Bp->LastInstr));
    DEBUGGERMSG(HDZONE_BP0, (L"InstSize:%d\r\n", Bp->InstrSize));
    DEBUGGERMSG(HDZONE_BP0, (L"RomBP:%d fWritten:%d Disabled:%d\r\n", Bp->fRomBP, Bp->fWritten, Bp->fDisabled));
    return S_OK;
}
#endif


BOOL BreakpointImpl::DoSanitize(void *buffer, MEMADDR const *Address, ULONG nSize, BOOL fAlwaysCopy,
                                BOOL fExternal)
{
    DWORD_PTR PhysStart = AddrToPA(Address);
    DWORD_PTR PhysEnd   = PhysStart + nSize;
    BOOL fTouched   = FALSE;
    BYTE *pbBuffer  = reinterpret_cast<BYTE*>(buffer);

    KDASSERT(PAGEALIGN_DOWN(PhysStart) == PAGEALIGN_DOWN(PhysEnd - 1));

    DEBUGGERMSG(HDZONE_BP0,(L"+BP:Sanitize(%08X,%08X,%d)\r\n",buffer,Address->addr,nSize));

    if (fAlwaysCopy)
    {
        MEMADDR Dst;
        SetDebugAddr(&Dst, buffer);
        DD_MoveMemory0(&Dst, Address, nSize);
    }
    
    for (DWORD i = 0; i < BPMax; ++i)
    {
        BreakpointImpl *bp = &Breakpoints[i];
        if (RefCount[i] == 0 ||
              !(bp->fWritten || bp->fDisabled))
        {
            // Make sure the breakpoint record knows about the original instruction
            // If it's not allocated, ignore.
            // If neither fWritten or fDisabled is set, ignore.
            continue;
        }

        if (fExternal && bp->fDisabled)
        {
            // Skip disabled breakpoints for external callers.
            // fDisabled breakpoints are cleared once returning to the system.
            // See EnableAll
            continue;
        }

        DWORD BPStart = AddrToPA(&bp->Address);
        if (BPStart == INVALID_PHYSICAL_ADDRESS)
        {
            // Skip bad breakpoints.
            continue;
        }
        DWORD BPEnd = BPStart + bp->InstrSize;

        if (BPStart >= PhysEnd || BPEnd <= PhysStart)
        {
            // This breakpoint definitely does not overlap
            continue;
        }
        DEBUGGERMSG(HDZONE_BP0,(L" BP:Sanitize bp%d @%08X orig %08X\r\n",i,
            bp->Address.addr,bp->LastInstr));

        ULONG_PTR OverlapStart = max(PhysStart, BPStart);
        ULONG_PTR OverlapEnd   = min(PhysEnd, BPEnd);
        ULONG_PTR OverlapIdx   = OverlapStart - PhysStart; 
        ULONG_PTR BpIdx        = OverlapStart - BPStart;

        PREFAST_ASSUME (OverlapEnd >= OverlapStart);

        ULONG_PTR OverlapLen   = OverlapEnd - OverlapStart;
        KDASSERT(OverlapLen <= bp->InstrSize);
        BYTE *OrigInstr        = reinterpret_cast<BYTE*>(&bp->LastInstr);
        memcpy(&pbBuffer[OverlapIdx], &OrigInstr[BpIdx], OverlapLen);
        fTouched = TRUE;
    }
    DEBUGGERMSG(HDZONE_BP0,(L"-BP:Sanitize %d\r\n",fTouched));
    return fTouched;
}

void BreakpointImpl::SetDisableAllOnHalt(BOOL fDisable)
{
    fDisableAllOnHalt = fDisable;
}

BOOL BreakpointImpl::DisableAllOnHalt()
{
    return fDisableAllOnHalt;
}

HRESULT BreakpointImpl::SetState(Breakpoint *bp, enum BreakpointState state)
{
    BreakpointImpl *bpi = reinterpret_cast<BreakpointImpl *>(bp);
    return bpi->SetState(state);
}

HRESULT BreakpointImpl::GetState(Breakpoint *bp, enum BreakpointState *state)
{
    BreakpointImpl *bpi = reinterpret_cast<BreakpointImpl *>(bp);
    return bpi->GetState(state);
}

void BreakpointImpl::Sanitize(void *buffer, MEMADDR const *Addr, ULONG nSize)
{
    if (!DisableAllOnHalt())
    {
        DoSanitize(buffer, Addr, nSize, FALSE, FALSE);
    }
}

BOOL IsValidProcess(PPROCESS pprc)
{
    if (pprc == NULL)
    {
        return false;
    }
    DWORD prcID = pprc->dwId;
    return pprc == GetProcPtr(h2pHDATA(reinterpret_cast<HANDLE>(prcID),
                                       g_pprcNK->phndtbl));
}

