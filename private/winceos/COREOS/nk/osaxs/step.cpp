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

#include "osaxs_p.h"
#include "step.h"

SingleStep SingleStep::stepStates[MAX_STEP];


HRESULT BoostThreadPriority (PTHREAD pth, BYTE *bprio, BYTE *cprio, DWORD *quantum)
{
    *bprio = pth->bBPrio;
    *cprio = pth->bCPrio;
    *quantum = pth->dwQuantum;
    
    KCall ((PKFN)SCHL_SetThreadBasePrio, pth, 0);
    KCall ((PKFN)SCHL_SetThreadQuantum, pth, 0);

    DEBUGGERMSG(OXZONE_STEP, (L"  BoostThreadPriority, thread %08X, bprio %d, cprio %d, quant %d\r\n", pth->dwId, *bprio, *cprio, *quantum));
    return S_OK;
}



HRESULT RestoreThreadPriority (PTHREAD pth, BYTE bprio, BYTE cprio, DWORD quantum)
{
    KCall ((PKFN)SCHL_SetThreadBasePrio, pth, bprio);
    KCall ((PKFN)SCHL_SetThreadQuantum, pth, quantum);
    SET_CPRIO(pth, cprio);
    DEBUGGERMSG(OXZONE_STEP, (L"  RestoreThreadPriority, thread %08X, bprio %d, cprio %d, quant %d\r\n",
            pth->dwId, pth->bBPrio, pth->bCPrio, pth->dwQuantum));
    return S_OK;
}



void SingleStep::RestoreCBP()
{
    if (restorebp)
    {
        DEBUGGERMSG(OXZONE_STEP, (L"  SS:RestoreCBP, enabling bp %08X\r\n", restorebp));
        DD_EnableVMBreakpoint(restorebp);
    }
}



SingleStep* SingleStep::Find (DWORD dwThreadID)
{
    SingleStep *pFound = 0;
    for (DWORD i = 0; i < MAX_STEP; ++i)
    {
        if (stepStates[i].inuse)
        {
            if (stepStates[i].thrid == dwThreadID)
            {
                // Make sure we only find one step per thread
                KDASSERT (pFound == 0);
                pFound = &stepStates[i];
            }
            else
            {
                // Make sure that some other step is not atomic.
                // (atomic guarantee was broken!)
                KDASSERT (!stepStates[i].atomic);
            }
        }
    }
    return pFound;
}



void SingleStep::DeleteAll ()
{
    for (DWORD i = 0; i < MAX_STEP; i++)
    {
        if (stepStates[i].inuse)
            stepStates[i].Delete ();
    }
}


void SingleStep::DumpState()
{
    DEBUGGERMSG(OXZONE_STEP, (L" SS %d on thr %08X\r\n",
                this - stepStates, thrid));
    DEBUGGERMSG(OXZONE_STEP, (L" bp=%08X, safety=%08X, restore=%08X\r\n",
                bp, safetybp, restorebp));
    DEBUGGERMSG(OXZONE_STEP, (L" quant=%d, bprio=%d, cprio=%d\r\n",
                              quantum, bprio, cprio));
    DEBUGGERMSG(OXZONE_STEP, (L" inuse=%u, atomic=%u, run=%u\r\n",
                              inuse, atomic, run));
    DEBUGGERMSG(OXZONE_STEP, (L" dwDBPHandle=0x%08x\r\n", dwDBPHandle));    
    DEBUGGERMSG(OXZONE_STEP, (L" pVM=0x%08x dwAddress=0x%08x\r\n", pVM, dwAddress));
}


BOOL SingleStep::Trap(EXCEPTION_RECORD *pExceptionRecord, CONTEXT *pContext,
        BOOL SecondChance, BOOL *pfIgnore)
{
    SingleStep *pstep;
    ULONG *pec;
    DWORD CurThrId;
    BOOL fHandled;
    BOOL fIsHdstubNotification = FALSE;

    pec = &pExceptionRecord->ExceptionCode;
    DEBUGGERMSG(OXZONE_STEP, (L"++SS:Trap, EX=%08X, PC=%08X\r\n",
                              *pec,
                              CONTEXT_TO_PROGRAM_COUNTER(pContext)));
    
    DD_PushExceptionState(pExceptionRecord, pContext, SecondChance);

    // (1) Single step trap handler doesn't care about 2nd chance exceptions
    if (SecondChance)
    {
        goto not_handled;
    }

    // (2) Single step is only interested in Single step events
    if (*pec != STATUS_SINGLE_STEP && *pec != STATUS_BREAKPOINT)
    {
        goto not_handled;
    }

    // (2.1) Make sure that the breakpoint event is not triggered by an
    // os notification.  If so, pass the event up but do not clear step
    // states.
    if (*pec == STATUS_BREAKPOINT &&
          g_pKData->pOsAxsDataBlock != NULL &&
          g_pKData->pOsAxsDataBlock->dwPtrHdstubNotifOffset != 0)
    {
        ULONG const pc = CONTEXT_TO_PROGRAM_COUNTER(pContext);
        if (g_pKData != NULL && 
            g_pKData->pOsAxsDataBlock != NULL &&
            g_pKData->pOsAxsDataBlock->dwPtrHdstubNotifOffset != 0)
        {
            ULONG HDNotifyOffset = *(ULONG *)g_pKData->pOsAxsDataBlock->dwPtrHdstubNotifOffset;
            ULONG HDNotifyEnd = HDNotifyOffset + HD_NOTIFY_MARGIN;

            DEBUGGERMSG(OXZONE_STEP, (L" SS:Trap pc=%08X, HD=%08X-%08X\r\n", pc, HDNotifyOffset, HDNotifyEnd));
            if (HDNotifyOffset <= pc && pc < HDNotifyEnd)
            {
                fIsHdstubNotification = TRUE;
                goto not_handled;
            }
        }
    }

    // (3) If an existing step record exists for current thread, erase the 
    // single step record and continue according to how the step was created.
    CurThrId = PcbGetCurThreadId();
    pstep = Find (CurThrId);
    if (pstep != NULL)
    {
        BOOL fRun;

        DEBUGGERMSG(OXZONE_STEP, (L" SS:Trap, found single step record\r\n"));
        *pec = (ULONG) STATUS_SINGLE_STEP;
        fRun = pstep->run;
        pstep->Delete();
        if (fRun)
        {
            goto handled;
        }
        goto not_handled;
    }

not_handled:
    // Single step does not handle this event and other parts of the debugger
    // need to handle this.  (KD, etc)
    if (!fIsHdstubNotification)
    {
        // Flush all running single steps, unless we're handling a hdstub
        // notification.
        DeleteAll();
    }
    fHandled = FALSE;
    goto exit;

exit:
    DD_PopExceptionState();
    DEBUGGERMSG(OXZONE_STEP, (L"--SS:Trap %d\r\n", fHandled));
    return fHandled;

handled:
    // Sink the event because either the event was a single step that should
    // result in a run or a single step that was used to move past a
    // breakpoint.
    *pfIgnore = TRUE;
    fHandled = TRUE;
    goto exit;
}


// Determine whether bp is in the module at (AddrStart - AddrEnd)
static BOOL BPInModule(Breakpoint *bp,
                       BOOL fIsDLL,
                       PPROCESS pprc,
                       DWORD AddrStart,
                       DWORD AddrEnd)
{
    if (bp == NULL)
    {
        return FALSE;
    }
    
    DEBUGGERMSG(OXZONE_STEP, (L" BPInModule, bp %08X::%08X to %08X, %08X-%08X, (dll %d)\r\n",
                    bp->Address.vm, bp->Address.addr, pprc, AddrStart, AddrEnd,
                    fIsDLL));
    if (AddrStart <= (DWORD)bp->Address.addr &&
        (DWORD)bp->Address.addr < AddrEnd)
    {
        // Address of the breakpoint overlaps with the address range of the module.
        // If the module happens to be a DLL, it's a match because on CE dll's are fixed up once
        // per system.  (All DLL addresses are identical for all processes).
        //
        // If the module is a process, then make sure the process pointer is the same as
        // the vm process that contains the breakpoint.
        if (fIsDLL || bp->Address.vm == pprc)
        { 
            DEBUGGERMSG(OXZONE_STEP, (L" BPInModule, match bp @%08X::%08X to range %08X-%08X, %08X\r\n",
                bp->Address.vm, bp->Address.addr, AddrStart, AddrEnd, pprc));
            return TRUE;
        }
    }

    return FALSE;
}


// Return the owner of the thread that is single stepping.
PPROCESS SingleStep::OwnerProc()
{
    PTHREAD pth = DD_HandleToThread((HANDLE) thrid);
    if (pth != NULL)
    {
        return pth->pprcOwner;
    }
    return NULL;
}


// Trap OS Module unload notifications and see if any pending single steps need
// to be cleaned up.
BOOL SingleStep::ModuleUnload(DWORD ModuleHandle)
{
    DD_FreezeSystem();
    DEBUGGERMSG(OXZONE_STEP, (L"+SS:ModUnload %08X\r\n", ModuleHandle));
    
    PPROCESS pprc = (PPROCESS) ModuleHandle;
    PMODULE  pmod = (MODULE *) ModuleHandle;
    PPROCESS pprcVM;
    DWORD    addrStart;
    DWORD    addrEnd;
    BOOL     fIsDLL;
    
    if (DD_IsValidProcPtr(pprc))
    {
        // Process is unloading
        pprcVM    = pprc;
        addrStart = (DWORD) pprc->BasePtr;
        addrEnd   = addrStart + pprc->e32.e32_vsize;
        fIsDLL    = FALSE;
        DEBUGGERMSG(OXZONE_STEP, (L" SS:ModUnload, process %08X, (%08X-%08X)\r\n",
            pprcVM, addrStart, addrEnd));
    }
    else if (OSAXSIsModuleValid(pmod))
    {
        // DLL is unloading
        pprcVM    = g_pprcNK;
        addrStart = (DWORD) pmod->BasePtr;
        addrEnd   = addrStart + pmod->e32.e32_vsize;
        fIsDLL    = TRUE;
        if (pmod->wRefCnt > 1)
        {
            // This dll is only going away from a specific process and will still
            // be loaded in NK.EXE after this notification.
            goto exit;
        }
        DEBUGGERMSG(OXZONE_STEP, (L" SS:ModUnload, dll (%08X-%08X) ref=%d\r\n", addrStart, addrEnd, pmod->wRefCnt));
        
    }
    else
    {
        // Should never ever get here.  Something weird in the kernel if this
        // is the case.
        DBGRETAILMSG(OXZONE_ALERT, (L" SS:ModUnload, unrecognized module id %08X\r\n", ModuleHandle));
        goto exit;
    }

    // Loop over all of the step states and clean up the ones that have breakpoints
    // in this address range.
    for (DWORD i = 0; i < MAX_STEP; ++i)
    {
        if (!stepStates[i].inuse)
        {
            // not in use, skip.
            continue;
        }

        // There are four reasons to delete a single step given a module unload:
        // 1. The breakpoint (bp) used to single step is in the module being unloaded.
        // 2. The safety breakpoint used to single step into a call is in the unloading module.
        // 3. The single step temporarily disabled a breakpoint that is in the unloading module.
        // 4. A process is unloading and the single step is in a thread owned by process.
        if (!BPInModule(stepStates[i].bp, fIsDLL, pprcVM, addrStart, addrEnd) &&    // (1)
            !BPInModule(stepStates[i].safetybp, fIsDLL, pprcVM, addrStart, addrEnd) && // (2)
            !BPInModule(stepStates[i].restorebp, fIsDLL, pprcVM, addrStart, addrEnd)) // (3)
        {
            if (fIsDLL || pprc != stepStates[i].OwnerProc()) // (4)
            {
                continue;
            }
        }

        DEBUGGERMSG(OXZONE_STEP, (L" SS:ModUnload, match step[%d] Owner = %08X\r\n", i,
                        stepStates[i].OwnerProc()));

        // [begin Diagnostic output
        if (stepStates[i].bp != NULL)
        {
            DEBUGGERMSG(OXZONE_STEP, (L" SS:ModUnload, bp %08X::%08X\r\n", 
                stepStates[i].bp->Address.vm,
                stepStates[i].bp->Address.addr));
        }
        if (stepStates[i].safetybp != NULL)
        {
            DEBUGGERMSG(OXZONE_STEP, (L" SS:ModUnload, safetybp %08X::%08X\r\n", 
                stepStates[i].safetybp->Address.vm,
                stepStates[i].safetybp->Address.addr));
        }
        if (stepStates[i].restorebp != NULL)
        {
            DEBUGGERMSG(OXZONE_STEP, (L" SS:ModUnload, restorebp %08X::%08X\r\n", 
                stepStates[i].restorebp->Address.vm,
                stepStates[i].restorebp->Address.addr));
        }
        // end Diagnostic output]

        stepStates[i].Delete();
    }

exit:
    // We never sink the module unload.
    DEBUGGERMSG(OXZONE_STEP, (L"-SS:ModUnload\r\n"));
    DD_ThawSystem();
    return FALSE;
}


BOOL SingleStep::IsPSLCall(ULONG Offset)
{
    BOOL IsCall;

    IsCall = ((ULONG)LAST_METHOD <= Offset && Offset <= (ULONG)MAX_METHOD_NUM);
    DEBUGGERMSG(OXZONE_STEP, (L"  SS:IsPSL, %d\r\n", IsCall));
    return IsCall;
}


//
// Helper functions to abstract away memory access
//
BOOL ReadMem(ULONG address, void *buf, ULONG length)
{
    MEMADDR Dst, Src;
    ULONG cb;

    SetVirtAddr(&Src, PcbGetVMProc(), (void *)address);
    SetDebugAddr(&Dst, buf);
    cb = DD_MoveMemory0(&Dst, &Src, length);
    BOOL fSuccess = cb == length;
    if (fSuccess)
    {
        DD_Sanitize(buf, &Src, length);
    }
    return fSuccess;
}

BOOL ReadByte(ULONG Addr, BYTE *pb)
{
    return ReadMem(Addr, pb, sizeof(*pb));
}

BOOL ReadWord(ULONG Addr, WORD *pw)
{
    return ReadMem(Addr, pw, sizeof(*pw));
}

BOOL ReadDword(ULONG Addr, DWORD *pdw)
{
    return ReadMem(Addr, pdw, sizeof(*pdw));
}

#pragma warning(disable:4189)  
// Ignore this "unref'ed variable" warning.  We only use the variable when DEBUGGERMSGs are enabled.
// Skip the warning rather than duplicate the DEBUGGERMSG conditionals here.

void SingleStep::RestoreDBP()
{
    DWORD dwCount = DD_EnableDataBreakpointsOnPage(pVM, dwAddress);
    (void)dwCount;
    DEBUGGERMSG(OXZONE_STEP, (L"  SS:DeleteDBP, enabled %u\r\n", dwCount));
}


void SingleStep::Delete()
{
    DEBUGGERMSG(OXZONE_STEP, (L"++SS:Delete\r\n"));

    if (bp)
    {
        DEBUGGERMSG(OXZONE_STEP, (L"  SS:Delete, deleting bp %08X\r\n", bp));
        KDVERIFY(SUCCEEDED(DD_DeleteVMBreakpoint(bp)));
    }
    if (safetybp)
    {
        DEBUGGERMSG(OXZONE_STEP, (L"  SS:Delete, deleting safety %08X\r\n", safetybp));
        KDVERIFY(SUCCEEDED(DD_DeleteVMBreakpoint(safetybp)));
    }

    if(restorebp)
    {
        RestoreCBP();
    }
    if(pVM && dwAddress)
    {
        RestoreDBP();
    }

    if (atomic)
    {
        DEBUGGERMSG(OXZONE_STEP, (L"  SS:DeleteDBP, releasing scheduler\r\n"));
        PTHREAD pth = DD_HandleToThread (reinterpret_cast<HANDLE> (thrid));
        if (pth)
        {
            RestoreThreadPriority (pth, bprio, cprio, quantum);
        }
    }

    thrid = 0;
    inuse = FALSE;
    DEBUGGERMSG(OXZONE_STEP, (L"--SS:DeleteDBP\r\n"));
}

#pragma warning(default:4189)


HRESULT SingleStep::AddCBP(BOOL fRun)
{
    DEBUGGERMSG(OXZONE_STEP, (L"++SS:AddCBP: fRun=%d\r\n", fRun));

    HRESULT hr = E_FAIL;

    if(!inuse)
    {
        hr = New(fRun);
        if(FAILED(hr))
        {
            goto Error;
        }
    }

    run = run && fRun;  //Halting takes precedence over running.

    ULONG pc = CONTEXT_TO_PROGRAM_COUNTER(DD_ExceptionState.context);
    PPROCESS pprcVM = DD_GetProperVM (PcbGetVMProc (), (void *)pc);
    MEMADDR Addr;
    SetVirtAddr(&Addr, pprcVM, (void*)pc);
    hr = DD_FindVMBreakpoint(&Addr, NULL, &restorebp);
    if (SUCCEEDED(hr) && restorebp != NULL)
    {
        if (bp != NULL)
        {
            // Handle the case where the next bp and the restorebp are identical
            // eg. jump to current instruction address.
            const DWORD restorepa = DD_AddrToPA (&restorebp->Address);
            const DWORD nextpa = DD_AddrToPA (&bp->Address);
            if (restorepa != INVALID_PHYSICAL_ADDRESS && restorepa == nextpa)
            {
                hr = S_OK;
                goto Exit;
            }
        }

        DEBUGGERMSG(OXZONE_STEP, (L"  SS:AddCBP, found bp %08X\r\n", restorebp));
        DEBUGGERMSG(OXZONE_STEP, (L"  SS:AddCBP, acquiring spinlock to avoid scheduling\r\n"));
        // Not correct.  Replace once we have kernel sanctioned means of running atomically.
        // DD_FreezeSystem();

        hr = BoostThreadPriority (PcbGetCurThread (), &bprio, &cprio, &quantum);
        if (FAILED (hr))
        {
            goto Error;
        }
        atomic = TRUE;
        hr = DD_DisableVMBreakpoint(restorebp);
        if (FAILED(hr))
        {
            goto Error;
        }
    }

Exit:
    DEBUGGERMSG(OXZONE_STEP || FAILED(hr), (L"--SS:AddCBP, hr=%08X\r\n", hr));
    return hr;
Error:
    Delete();
    goto Exit;
}


#pragma warning(disable:4189)  
// Ignore this "unref'ed variable" warning.  We only use the variable when DEBUGGERMSGs are enabled.
// Skip the warning rather than duplicate the DEBUGGERMSG conditionals here.

HRESULT SingleStep::AddDBP(BOOL fRun)
{
    HRESULT hr = E_FAIL;

    if(!inuse)
    {
        hr = New(fRun);
        if(FAILED(hr) && hr == KDBG_E_ROMBPKERNEL)
        {
            //Try to set a BP on PSL return addr.
            PCALLSTACK pcstk = PcbGetCurThread()->pcstkTop;
            if(pcstk)
            {
                //skip the first callstack because the first callstack holds the callstack info
                //for the call into the exception handler.
                pcstk = pcstk->pcstkNext;
                if(pcstk && pcstk->retAddr)
                {
                    //Try to set a BP on the return address of the PSL call.
                    hr = SetBP(pcstk->retAddr, FALSE, &bp);
                }
            }
        }
        if(FAILED(hr))
        {
            goto Error;
        }
    }

    run = run && fRun;  //Halting takes precedence over running.

    dwDBPHandle = INVALID_DBP_HANDLE;

    if((STATUS_ACCESS_VIOLATION == DD_ExceptionState.exception->ExceptionCode) && 
        (DD_ExceptionState.exception->NumberParameters >= AV_EXCEPTION_PARAMETERS))
    {   
        BOOL fWrite = DD_ExceptionState.exception->ExceptionInformation[AV_EXCEPTION_ACCESS];
        dwAddress = DD_ExceptionState.exception->ExceptionInformation[AV_EXCEPTION_ADDRESS];
        pVM = (dwAddress > VM_SHARED_HEAP_BASE ? g_pprcNK : GetPCB()->pVMPrc);                
        dwDBPHandle = DD_FindDataBreakpoint(pVM, dwAddress, fWrite);
        
        DWORD dwCount = DD_DisableDataBreakpointsOnPage(pVM, dwAddress);
        (void)dwCount;
        DEBUGGERMSG(OXZONE_STEP, (L"  SS:NewDBP, found dbp 0x%08X disabled:%u\r\n", dwDBPHandle, dwCount));
        DEBUGGERMSG(OXZONE_STEP, (L"  SS:NewDBP, acquiring spinlock to avoid scheduling\r\n"));

        // Not correct.  Replace once we have kernel sanctioned means of running atomically.
        // DD_FreezeSystem();

        hr = BoostThreadPriority (PcbGetCurThread (), &bprio, &cprio, &quantum);
        if (FAILED (hr))
            goto Error;
        
        atomic = TRUE;
    }

Exit:
    DEBUGGERMSG(OXZONE_STEP || FAILED(hr), (L"--SS:AddDBP, hr=%08X\r\n", hr));
    return hr;
Error:
    Delete();
    goto Exit;
}

#pragma warning(default:4189)


HRESULT SingleStep::New(BOOL fRun)
{
    DEBUGGERMSG(OXZONE_STEP, (L"++SS:New, Run=%d\r\n", fRun));
    HRESULT hr = S_OK;

    inuse = TRUE;
    run = fRun;     
    thrid = PcbGetCurThreadId();
    bp = NULL;
    safetybp = NULL;
    quantum = 0;
    bprio = 0;
    cprio = 0;
    atomic = FALSE;
    restorebp = NULL;
    dwDBPHandle = 0;
    dwAddress = 0;
    pVM = 0;

    DEBUGGERMSG(OXZONE_STEP, (L"  SS:New, thread=%08X, PC=%08X\r\n", thrid,
        CONTEXT_TO_PROGRAM_COUNTER(DD_ExceptionState.context)));

    hr = CpuSetup();

    DumpState();
        
    DEBUGGERMSG(OXZONE_STEP || FAILED(hr), (L"--SS:New, hr=%08X\r\n", hr));
    return hr;
    
}


HRESULT SingleStep::AllocSingleStep(DWORD *dwIndex)
{
    DEBUGGERMSG(OXZONE_STEP, (L"++SS:AllocSingleStep: 0x%08x\r\n", dwIndex));
    HRESULT hr = E_FAIL;

    //Search for an existing singel step.
    for (DWORD i = 0; i < MAX_STEP; ++i)
    {
        if ((stepStates[i].inuse) && (stepStates[i].thrid == PcbGetCurThreadId()))
        {
            *dwIndex = i;
            hr = S_OK;
            goto Exit;
        }
    }

    for (DWORD i = 0; i < MAX_STEP; ++i)
    {
        if (!stepStates[i].inuse)
        {
            memset(&stepStates[i], 0, sizeof(stepStates[i]));
            *dwIndex = i;
            hr = S_OK;
            goto Exit;
        }
    }

    //Failed to find an available step structure.

Exit:
    DEBUGGERMSG(OXZONE_STEP || FAILED(hr), (L"--SS:AllocSingleStep: hr=0x%08x idx:%u\r\n", hr, *dwIndex));
    return hr;
    
}

HRESULT SingleStep::StepThread()
{
    DEBUGGERMSG(OXZONE_STEP, (L"++SS:StepThread\r\n"));
    DWORD dwIndex = 0;
    HRESULT hr = AllocSingleStep(&dwIndex);
    if(SUCCEEDED(hr))
    {
        hr = stepStates[dwIndex].New(FALSE);
        if(FAILED(hr))
        {
            stepStates[dwIndex].Delete();
        }
    }    
    DEBUGGERMSG(OXZONE_STEP || FAILED(hr), (L"--SS:StepThread: hr=0x%08x\r\n", hr));
    return hr;    
}

HRESULT SingleStep::StepCBP(BOOL fRun)
{
    DEBUGGERMSG(OXZONE_STEP, (L"++SS:StepCBP\r\n"));
    DWORD dwIndex = 0;
    HRESULT hr = AllocSingleStep(&dwIndex);
    if(SUCCEEDED(hr))
    {
        stepStates[dwIndex].AddCBP(fRun);
    }        
    DEBUGGERMSG(OXZONE_STEP || FAILED(hr), (L"--SS:StepCBP: hr=0x%08x\r\n", hr));
    return hr;       
}

HRESULT SingleStep::StepDBP(BOOL fRun)
{
    DEBUGGERMSG(OXZONE_STEP, (L"++SS:StepDBP\r\n"));
    DWORD dwIndex = 0;
    HRESULT hr = AllocSingleStep(&dwIndex);
    if(SUCCEEDED(hr))
    {
        hr = stepStates[dwIndex].AddDBP(fRun);
    }        
    DEBUGGERMSG(OXZONE_STEP || FAILED(hr), (L"--SS:StepDBP: hr=0x%08x\r\n", hr));
    return hr;           
}


HRESULT SingleStep::SetBP(void *addr, BOOL f16Bit, Breakpoint **ppbp)
{
    PPROCESS pprcVM = PcbGetVMProc();
    PPROCESS pprcProperVM = DD_GetProperVM(pprcVM, addr);

    MEMADDR Addr;
    SetVirtAddr(&Addr, pprcProperVM, addr);

    DWORD hthCur = PcbGetCurThreadId();

    HRESULT hr = DD_AddVMBreakpoint(&Addr, hthCur, f16Bit, ppbp);

    return hr;
}

