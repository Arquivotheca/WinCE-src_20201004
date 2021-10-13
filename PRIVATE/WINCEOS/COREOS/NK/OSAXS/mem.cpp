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
/*++

Module Name:

    mem.cpp

Abstract:

    Memory Move operation for target-side.

Environment:

    OsaxsT0

--*/

#include "osaxs_p.h"
#include "dbg_common.h"


/*++

Routine Name:

    FlushCache

Routine Description:

    Flush cache so that memory changes will be properly propagated to all aliased virtual addresses.

--*/
void FlushCache ()
{
#if defined(MIPS)
    g_OsaxsData.pfnFlushCacheRange (0, 0, CACHE_SYNC_WRITEBACK | CACHE_SYNC_DISCARD);
    g_OsaxsData.pfnFlushCacheRange (0, 0, CACHE_SYNC_INSTRUCTIONS);
#elif defined(SHx)
    g_OsaxsData.pfnFlushCacheRange (0, 0, CACHE_SYNC_INSTRUCTIONS | CACHE_SYNC_WRITEBACK | CACHE_SYNC_DISCARD);
#elif defined(ARM)
    g_OsaxsData.pfnFlushCacheRange (0, 0, CACHE_SYNC_WRITEBACK | CACHE_SYNC_DISCARD);
    g_OsaxsData.pfnFlushCacheRange (0, 0, CACHE_SYNC_INSTRUCTIONS);
#endif
}


/*++

Routine Name:

    SafeDbgVerify

Routine Description:

    Safely call DbgVerify in the kernel by saving the last error and restoring it after
    calling dbgverify.

    This function should always be called from MapToDebuggeeCtxKernEquivIfAcc because 
    using this function incorrectly can result in a corrupted VM state for the device.

Arguments:

    pVM - pointer to process record
    pvAddr - virtual address.
    fProbeOnly - TRUE -> if page containing address is inaccessable, then fail. (non instrusive)
                 FALSE -> if page is inaccessable, force the page in. (intrusive)

Return Value:

    NULL -> Unmappable, unaccessable, invalid address.
    Otherwise -> KVA that corresponds to (pVM, pvAddr)

--*/
static void* SafeDbgVerify (PROCESS *pVM, void * pvAddr, BOOL fProbeOnly)
{
    DWORD dwLastError;
    void * pvRet;
    
    dwLastError = KGetLastError (pCurThread);
    pvRet = g_OsaxsData.pfnDbgVerify (pVM, pvAddr, fProbeOnly,   NULL);
    KSetLastError (pCurThread, dwLastError);
    return pvRet;
}


/*++

Routine Name:

    MapToDebuggeeCtxKernEquivIfAcc

Routine Description:

    Map an address to a Debuggee context (default or override), and converted to a potentially
    statically mapped kernel address equivalent, if accessible (NULL if not accessible - in case of
    non-Forced Paged-in).

    Debuggee process space is by default based on current process (or prior in chain for thread migration)
    or can be overriden if client debugger sets focus on different process than current
    If in EXE range in slot 0, remap to Debuggee slot
    If in DLL R/O range, remap to NK
    If in DLL R/W range, remap to debuggee process

    If Forced Page-in mode ON, the page will be paged-in if not already done
    If Forced Page-in mode OFF, returns NULL

Arguments:

    pFocusProc : Process whose VM table is going to be used to get a physical address.
    pvAddr : virtual address
    fProbeOnly : TRUE -> Don't page in an unpaged page.
                 FALSE -> Page in unpaged mempage so it'll be guaranteed accessable after calling this function.

Return Value:

    NULL -> on error / unaccessable / etc.
    Non NULL -> KVA address.

--*/
void *MapToDebuggeeCtxKernEquivIfAcc (PROCESS *pFocusProc, void *pvAddr, BOOL fProbeOnly)
{
    void *pvRet = NULL;
    PPROCESS pActualVM;

    DEBUGGERMSG(OXZONE_MEM, (L"++MapToDebuggeeCtxKernEquivIfAcc(pVM=%08X, Addr=%08X, Probe = %d)\r\n", pFocusProc, pvAddr, fProbeOnly));
    pActualVM = GetProperVM(pFocusProc, pvAddr);
    if (pActualVM)
    {
        pvRet = SafeDbgVerify (pActualVM, (void*)pvAddr, fProbeOnly);
        if (pvRet)
        {
            DEBUGGERMSG (OXZONE_MEM, (L"  MapToDebuggeeCtxKernEquivIfAcc %08X -> %08X \r\n", pvAddr, pvRet));
        }
        else if ((DWORD) pvAddr >= VM_SHARED_HEAP_BASE)
        {
            // It's still possible for DbgVerify to fail to map an address, especially if the corresponding physical address is
            // not mappable to KVA.
            // This should only really happen on X86 and ARM, both of which has a OEMAddressTable.

            // This block of code is limited to addresses that use the NK.EXE page table.  We should be able to
            // allow this code for any page table, but we need to make sure that before the actual memory operation
            // happens, the current VM of this thread NEEDS to switch to the appropriate one, otherwise the paging
            // may fail and bad things will happen.
            
            DWORD dwPFN;
            dwPFN = g_OsaxsData.pfnGetPFNOfProcess (pActualVM, (void *) PAGEALIGN_DOWN ((DWORD) pvAddr));
            if (dwPFN && dwPFN != INVALID_PHYSICAL_ADDRESS && Pfn2Virt (dwPFN) == NULL)
            {
                // The address does have a PA, and the kernel can't map it to a KVA.
                // We should just allow the debugger to access this address using the original address.
                DEBUGGERMSG (OXZONE_MEM, (L"  MapToDebuggeeCtxKernEquivIfAcc: %08X has a PFN entry value of %08X which can not be translated to KVA\r\n", pvAddr, dwPFN));
                pvRet = pvAddr;
            }
            else
            {
                // There is no PFN/PA for this address or the existing PFN can be mapped to KVA.
                // In the first case, the address is just bogus.
                // In the second case, the address should have been translated in the call to DbgVerify.
                // Either way, we probably don't want the debugger to access this address.
                DEBUGGERMSG (OXZONE_MEM, (L"  MapToDebuggeeCtxKernEquivIfAcc unable to map %08X.\r\n", pvAddr));
            }
        }
    }
    else
    {
        DEBUGGERMSG (OXZONE_MEM, (L"  MapToDebuggeeCtxKernEquivIfAcc unable to find VM.\r\n"));
    }

    DEBUGGERMSG (OXZONE_MEM, (L"--MapToDebuggeeCtxKernEquivIfAcc (ret=0x%08X)\r\n", pvRet));
    return pvRet;
}


void *OSAXSMapPtrWithSize (PPROCESS VMProc, void *Address, size_t Size, BOOL ProbeOnly)
{
    void *StartAddress = MapToDebuggeeCtxKernEquivIfAcc (VMProc, Address, ProbeOnly);
    if (StartAddress)
    {
        void *EndAddress = reinterpret_cast<BYTE*> (Address) + Size;
        void *CurrentAddress = reinterpret_cast<void*> (PAGEALIGN_UP(reinterpret_cast<DWORD> (Address)));

        while (CurrentAddress < EndAddress)
        {
            if (MapToDebuggeeCtxKernEquivIfAcc (VMProc, CurrentAddress, ProbeOnly))
            {
                CurrentAddress = reinterpret_cast<void*> (reinterpret_cast<DWORD> (CurrentAddress) + VM_PAGE_SIZE);
            }
            else
            {
                DEBUGGERMSG (OXZONE_MEM, (L"  MapPtrWithSize: Failed to map page 0X%08X", Address));
                return NULL;
            }
        }
    }
    return StartAddress;
}


BOOL kdpIsInaccessible(LPVOID pvAddr)
{
    BOOL fRet = FALSE;   // Assume FALSE in case IOCTL not implemented

    DEBUGGERMSG(OXZONE_MEM, (L"++kdpIsInaccessible: Enter, pvAddr=0x%08X\r\n", pvAddr));
    // First make sure that OEMKDIoControl is implemented
    if (pfnOEMKDIoControl)
    {
        // Now check if the memory is Inaccessible for read/write
        fRet = KCall ((PKFN)pfnOEMKDIoControl, KD_IOCTL_MEMORY_INACCESSIBLE, pvAddr, sizeof(pvAddr));
    }
    else
    {
        DEBUGGERMSG(OXZONE_MEM, (L"  kdpIsInaccessible: pfnOEMKDIoControl = NULL\r\n"));
    }

    DEBUGGERMSG(OXZONE_MEM, (L"--kdpIsInaccessible: Leave, fRet=%s\r\n", (fRet ? L"TRUE" : L"FALSE") ));
    return fRet;
}


DWORD OsAxsReadMemory (void *pDestination, PROCESS *pVM, void *pSource, DWORD Length)
{
    DEBUGGERMSG (OXZONE_MEM, (L"++OsAxsReadMemory: 0x%08x from 0x%08x(0x%08x) %d bytes\r\n", pDestination, pVM, pSource, Length));
    DWORD CopiedCount = 0;
    BYTE *pDest = reinterpret_cast<BYTE *>(pDestination);
    BYTE *pSrc = reinterpret_cast<BYTE *>(pSource);
    BOOL TerminateEarly = FALSE;

    while (!TerminateEarly && CopiedCount < Length) 
    {
        BYTE *pDest = reinterpret_cast<BYTE *>(pDestination) + CopiedCount;
        BYTE *pSrc = reinterpret_cast<BYTE *>(pSource) + CopiedCount;
        DWORD ToCopy = Length - CopiedCount;

        if (ToCopy > VM_PAGE_SIZE)
        {
            ToCopy = VM_PAGE_SIZE - (VM_PAGE_OFST_MASK & (DWORD)pSrc);
        }

        BYTE *pMappedSrc = reinterpret_cast<BYTE *> (MapToDebuggeeCtxKernEquivIfAcc (pVM, pSrc, TRUE));
        if (pMappedSrc)
        {
            __try
            {
                memcpy (pDest, pMappedSrc, ToCopy);
                CopiedCount += ToCopy;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                DEBUGGERMSG (OXZONE_ALERT, (L"  OsAxsReadMemory: Failing memcpy from Process 0x%08x VM Addr = 0x%08x, Mapped Addr = 0x%08x\r\n",
                             pVM, pSrc, pMappedSrc));
                TerminateEarly = TRUE;
            }
        }
        else
        {
            DEBUGGERMSG(OXZONE_MEM, (L"  OsAxsReadMemory: Trying to access unpaged in memory: pVM=%08X(%s), %08X\r\n",
                                       pVM, pVM? pVM->lpszProcName : L"(default)", pSrc));
            TerminateEarly = TRUE;
        }
    }
    DEBUGGERMSG (OXZONE_MEM, (L"--OsAxsReadMemory: read %d bytes\r\n", CopiedCount));

    return CopiedCount;
}


DWORD SafeMoveMemory (PROCESS *pFocusProc, void *pvDestination, void *pvSource, DWORD dwLength)
{
    DWORD dwTgtAddr;
    DWORD dwSrcAddr;
    ULONG ActualLength;
    BYTE *pbDestination = reinterpret_cast<BYTE *>(pvDestination);
    BYTE *pbSource = reinterpret_cast<BYTE *>(pvSource);

    DEBUGGERMSG (OXZONE_MEM, (L"++SafeMoveMemory: PROC: 0x%.08x, From 0x%.08x to 0x%.08x, %d bytes\r\n",
                             pFocusProc, pbSource, pbDestination, dwLength));
    
    // Move the source information to the destination address.
    FlushCache ();

    ActualLength = dwLength;
    
    //
    // This code is the most common cause of KD exceptions because the OS lies and says
    // we can safely access memory.  We use SEH as a last resort.
    //
    __try
    {
        // Get Debuggee context, potentially statically mapped kernel address equivalent if accessible
        dwTgtAddr = (DWORD) MapToDebuggeeCtxKernEquivIfAcc (pFocusProc, pbDestination, FALSE);
        dwSrcAddr = (DWORD) MapToDebuggeeCtxKernEquivIfAcc (pFocusProc, pbSource, FALSE);

        if (g_OsaxsData.pfnIsRom ((void *) dwTgtAddr, 1))
        {
            DEBUGGERMSG (OXZONE_MEM, (L"  SafeMoveMemory: target address is in ROM (Addr=0x%08X,KAddr=0x%08X). Abort Mem Copy.\r\n",
                                     pbDestination, dwTgtAddr));
            dwTgtAddr = 0; // Don't even try to write to ROM (will except on some platforms)
        }
        else if (kdpIsInaccessible ((void *) dwTgtAddr))
        {
            DEBUGGERMSG (OXZONE_MEM, (L"  SafeMoveMemory: target address is Inaccessible (Addr=0x%08X,KAddr=0x%08X). Abort Mem Copy.\r\n",
                                     pbDestination, dwTgtAddr));
            dwTgtAddr = 0; // Don't even try to write to Inaccessible memory (will except on some platforms)
        }
        else if (kdpIsInaccessible ((void *) dwSrcAddr))
        {
            DEBUGGERMSG (OXZONE_MEM, (L"  SafeMoveMemory: source address is Inaccessible (Addr=0x%08X,KAddr=0x%08X). Abort Mem Copy.\r\n",
                                     pbSource, dwSrcAddr));
            dwSrcAddr = 0; // Don't even try to read from Inaccessible memory (will except on some platforms)
        }
        else
        {
            DEBUGGERMSG (OXZONE_MEM, (L"  SafeMoveMemory: Dst 0x%08X (was 0x%08X) Src 0x%08X (was 0x%08X) length 0x%08X\r\n",
                                     dwTgtAddr, pbDestination, dwSrcAddr, pbSource, ActualLength));
        }

        while (dwLength && dwTgtAddr && dwSrcAddr) 
        {
            *(BYTE*) dwTgtAddr = *(BYTE*) dwSrcAddr; // Actual byte per byte copy
            ++pbDestination;
            ++pbSource;
            --dwLength;

            if (dwLength)
            {
                if (reinterpret_cast<DWORD>(pbDestination) & (VM_PAGE_SIZE-1))
                { // Did not cross page boundary
                    dwTgtAddr++;
                }
                else
                { // Crossed page boundary
                    dwTgtAddr = (DWORD) MapToDebuggeeCtxKernEquivIfAcc (pFocusProc, pbDestination, FALSE);
                    if (g_OsaxsData.pfnIsRom ((void *) dwTgtAddr, 1))
                    {
                        DEBUGGERMSG (OXZONE_MEM, (L"  SafeMoveMemory: target address is in ROM (Addr=0x%08X,KAddr=0x%08X). Abort Mem Copy.\r\n",
                                                 pbDestination, dwTgtAddr));
                        dwTgtAddr = 0; // Don't even try to write to ROM (will except on some platforms)
                    }
                    else if (kdpIsInaccessible ((void *) dwTgtAddr))
                    {
                        DEBUGGERMSG (OXZONE_MEM, (L"  SafeMoveMemory: target address is Inaccessible (Addr=0x%08X,KAddr=0x%08X). Abort Mem Copy.\r\n",
                                                 pbDestination, dwTgtAddr));
                        dwTgtAddr = 0; // Don't even try to write to Inaccessible memory (will except on some platforms)
                    }
                }
                
                if (reinterpret_cast<DWORD>(pbSource) & (VM_PAGE_SIZE - 1))
                { // Did not cross page boundary
                    dwSrcAddr++;
                }
                else
                { // Crossed page boundary
                    dwSrcAddr = (DWORD) MapToDebuggeeCtxKernEquivIfAcc (pFocusProc, pbSource, FALSE);
                    if (kdpIsInaccessible ((void *) dwSrcAddr))
                    {
                        DEBUGGERMSG (OXZONE_MEM, (L"  SafeMoveMemory: source address is Inaccessible (Addr=0x%08X,KAddr=0x%08X). Abort Mem Copy.\r\n",
                                                 pbSource, dwSrcAddr));
                        dwSrcAddr = 0; // Don't even try to read from Inaccessible memory (will except on some platforms)
                    }
                }
            }
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        DEBUGGERMSG(OXZONE_MEM, (L"  SafeMoveMemory: Exception encountered\r\n"));
        dwLength = ActualLength;
    }

    //
    // Flush the instruction cache in case the write was into the instruction
    // stream.
    //
    FlushCache ();

    DEBUGGERMSG (OXZONE_MEM, (L"--SafeMoveMemory\r\n"));
    return ActualLength - dwLength;
}


void * Pfn2Virt (unsigned long pfn)
{
    return g_OsaxsData.pfnPfn2Virt (pfn);
}
