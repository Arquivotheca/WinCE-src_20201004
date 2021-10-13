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

    mem.cpp

Abstract:

    Memory Move operation for target-side.

Environment:

    OsaxsT0

--*/

#include "osaxs_p.h"

static void FlushCache ()
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


static LPMODULE ModuleFromAddress(DWORD dwAddr)
{
    DWORD dwAddrZeroed;  // Address zero-slotized
    LPMODULE pMod;

    // Zero slotize
    dwAddrZeroed = ZeroPtr(dwAddr);

    // Walk module list to find this address
    pMod = pModList;
    while (pMod)
    {
        if (
            // User dll
            ((dwAddrZeroed >= ZeroPtr ((DWORD)pMod->BasePtr)) &&
             (dwAddrZeroed <  ZeroPtr ((DWORD)pMod->BasePtr + pMod->e32.e32_vsize))) ||
            // Kernel dll
            (IsKernelVa (pMod->BasePtr) &&
             (dwAddr >= (DWORD)pMod->BasePtr) &&
             (dwAddr <  (DWORD)pMod->BasePtr + pMod->e32.e32_vsize)) ||
            // Additional relocated RW data section
            ((dwAddrZeroed >= ZeroPtr ((DWORD)pMod->rwLow)) &&
             (dwAddrZeroed < ZeroPtr ((DWORD)pMod->rwHigh)))
           )
        {
            DEBUGGERMSG (OXZONE_MEM, (L"  MapToDebuggeeCtxKernEquivIfAcc: Found in Dll (%s)\r\n", pMod->lpszModName));
            break;
        }
        
        pMod=pMod->pMod;
    }

    return pMod;
}



// Get statically mapped kernel address and attempt to read (and page-in if allowed)
void* SafeDbgVerify (void * pvAddr, BOOL fProbeOnly)
{
    DWORD dwLastError;
    void * pvRet;
    
    dwLastError = KGetLastError (pCurThread);
    pvRet = g_OsaxsData.pfnDbgVerify (pvAddr, fProbeOnly,   NULL);
    KSetLastError (pCurThread, dwLastError);
    return pvRet;
}



static void * MapToDebuggeeCtxKernEquivIfAcc (PROCESS *pFocusProc, void *pvAddr, BOOL fProbeOnly)
{
    void *pvRet = NULL;
    ULONG loop;

    DEBUGGERMSG (OXZONE_MEM, (L"++MapToDebuggeeCtxKernEquivIfAcc (0x%08X)\r\n", pvAddr));
    {
        PVOID Temp = 0;
        LPMODULE lpMod;
        PCALLSTACK  pCurStk;
        if (((ULONG) pvAddr >= (DWORD) DllLoadBase) && ((ULONG) pvAddr < (2 << VA_SECTION)))
        { // pvAddr is in a DLL
            DEBUGGERMSG (OXZONE_MEM, (L"  MapToDebuggeeCtxKernEquivIfAcc: found in dll\r\n"));
            lpMod = ModuleFromAddress ((ULONG) pvAddr);
            if (lpMod)
            {                    
                // We now have the module.  Loop through to find out what section
                // this address is in.  If it is in a read only section map it into
                // the zero slot.  Otherwise try to find a process space to map it into.
                for (loop=0; loop < lpMod->e32.e32_objcnt; loop++)
                {
                    if ((ZeroPtr(pvAddr) >= ZeroPtr(lpMod->o32_ptr[loop].o32_realaddr)) &&
                        (ZeroPtr(pvAddr) < ZeroPtr(lpMod->o32_ptr[loop].o32_realaddr)+lpMod->o32_ptr[loop].o32_vsize))
                    {
                        if ((lpMod->o32_ptr[loop].o32_flags & IMAGE_SCN_MEM_READ) &&
                             !(lpMod->o32_ptr[loop].o32_flags & IMAGE_SCN_MEM_WRITE))
                        { // This is the read only section, map the pointer in NK
                            DEBUGGERMSG (OXZONE_MEM, (L"  MapToDebuggeeCtxKernEquivIfAcc: read only section, map in NK (VM nk = %08X)\r\n",
                                g_OsaxsData.pProcArray->dwVMBase));
                            Temp = MapPtrProc(pvAddr,g_OsaxsData.pProcArray);
                        } 
                        else 
                        { // Read/Write section:
                            //now have to search for the process to map the pointer into.
                            DEBUGGERMSG (OXZONE_MEM, (L"  MapToDebuggeeCtxKernEquivIfAcc: read/write section, map in CurProc (VM=%08X)\r\n",
                                    pFocusProc->dwVMBase));
                            
                            if ((lpMod->inuse & pFocusProc->aky) // DLL Used by current process
                                    || !(pCurThread->pcstkTop)) 
                            { // Map in current process
                                Temp = (PVOID)((DWORD)pvAddr|pFocusProc->dwVMBase);
                                DEBUGGERMSG (OXZONE_MEM, (L"  MapToDebuggeeCtxKernEquivIfAcc: DLL used by process\r\n"));
                            } 
                            else 
                            { // RW section but not used by current process (PSL case):
                                DEBUGGERMSG (OXZONE_MEM, (L"  MapToDebuggeeCtxKernEquivIfAcc: DLL not used by process (walk back stack lists)\r\n"));
                                pCurStk=pCurThread->pcstkTop;
                                while (pCurStk) 
                                { // Walk back all Cross-process calls on current thread to find process using that dll
                                    if ((pCurStk->pprcLast != (PPROCESS)USER_MODE) && (pCurStk->pprcLast != (PPROCESS)KERNEL_MODE)) 
                                    {
                                        if (lpMod->inuse & pCurStk->pprcLast->aky)
                                        {
                                            Temp = MapPtrInProc (pvAddr, pCurStk->pprcLast);
                                            DEBUGGERMSG (OXZONE_MEM, (L"  MapToDebuggeeCtxKernEquivIfAcc: map in pCurStk->pprcLast (VM=%08X)\r\n",
                                                pCurStk->pprcLast->dwVMBase));
                                            break;
                                        }
                                    }
                                    pCurStk=pCurStk->pcstkNext;
                                }
                            }
                        }
                    }
                }
            }
            else
            { // Map in NK
                Temp = MapPtrInProc (pvAddr, g_OsaxsData.pProcArray);
                DEBUGGERMSG (OXZONE_MEM, (L"  MapToDebuggeeCtxKernEquivIfAcc: Unknown module, map in NK (VM=%08X)\r\n", g_OsaxsData.pProcArray->dwVMBase));
            }
        } 
        else if (((ULONG) pvAddr < (DWORD) DllLoadBase) && pFocusProc)
        { // pbAddr is in Slot0 Proc and override ON
            Temp = (PVOID)((DWORD)pvAddr | pFocusProc->dwVMBase);
            DEBUGGERMSG( OXZONE_MEM, ( L"  MapToDebuggeeCtxKernEquivIfAcc: Slot 0 process space, map in ProcOverride (VM-%.08X)\r\n",
                pFocusProc->dwVMBase ));
        }
        else
        { // In non slot 0 Proc
            DEBUGGERMSG (OXZONE_MEM, (L"  MapToDebuggeeCtxKernEquivIfAcc: found in fully qualified (non slot 0) user exe or kernel space\r\n"));
            Temp = pvAddr; // No changes
        }
        
        DEBUGGERMSG (OXZONE_MEM, (L"  MapToDebuggeeCtxKernEquivIfAcc: Temp = %08X\r\n", Temp));
        pvRet = SafeDbgVerify (Temp, fProbeOnly); // Get statically mapped kernel address and attempt to read (and page-in if allowed)
    }
    
    DEBUGGERMSG (OXZONE_MEM, (L"--MapToDebuggeeCtxKernEquivIfAcc (ret=0x%08X)\r\n", pvRet));
    return pvRet;
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
                if (reinterpret_cast<DWORD>(pbDestination) & (PAGE_SIZE-1))
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
                
                if (reinterpret_cast<DWORD>(pbSource) & (PAGE_SIZE - 1))
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
