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
#include "ThreadAffinity.h"

// Filter the breakpoint based on thread affinity.  Return true if a breakpoint
// matches.  Setup bp soar and return false if no breakpoint should fire at
// the moment.

BOOL ThreadAffinity::HandleBreakpoint ()
{
    MEMADDR Addr;
    Breakpoint *pbpLast;
    Breakpoint *pbp;
    PPROCESS pprcVm = PcbGetVMProc();
    DWORD hthCur = PcbGetCurThreadId();
    DWORD& pc = CONTEXT_TO_PROGRAM_COUNTER(DD_ExceptionState.context);
    ULONG pcNext;
    HRESULT hr;
    BOOL fHandled;

    DEBUGGERMSG (OXZONE_TA, (L"++TA:HandleBp\r\n"));

    SetVirtAddr(&Addr, pprcVm, (void *)pc);
    
    pbp = NULL;
    for (;;)
    {
        pbpLast = pbp;
        hr = DD_FindVMBreakpoint(&Addr, pbpLast, &pbp);
        if (hr == KDBG_E_BPNOTFOUND)
        {
            // Not quite an error.  Basically hit the end of the list.
            pbp = NULL;
            hr = S_OK;
        }
        if (FAILED(hr))
        {
            // Something bad happened.  Just halt into the debugger
            fHandled = FALSE;
            goto error;
        }
        else if (pbp != NULL)
        {
            // Found a breakpoint on this offset.  Check for thread affinity.
            DEBUGGERMSG(OXZONE_TA,
                    (L"  TA:HandleBp: BP @%08X:%08X\r\n",
                    pbp->Address.vm, pbp->Address.addr));
            DEBUGGERMSG(OXZONE_TA,
                    (L"  TA:HandleBp: BP hTh = %08X\r\n", pbp->hTh));

            if (pbp->hTh != 0 && pbp->hTh != hthCur)
            {
                // Not wildcard and does not match
                continue;
            }

            // Found a breakpoint that is wild card or matches.
            // Halt into debugger.
            DEBUGGERMSG(OXZONE_TA,
                    (L"  TA:HandleBp, match found, bp %08X thr %08X\r\n",
                     pbp, pbp->hTh));
            fHandled = FALSE;
            goto exit;
        }
        else if (pbpLast == NULL)
        {
            // We found no breakpoint at all (first time through loop)
            fHandled = !DD_OnEmbeddedBreakpoint();
            DEBUGGERMSG(OXZONE_TA, (L"--TA:HandleBp, no bp found, Handled %d\r\n", fHandled));
            goto exit;
        }
        else
        {
            // Hit the end of the breakpoint list.
            // We need to continue the thread because at this point no
            // breakpoint exists that matches this (program offset, thread id).
            // Set up the thread to continue;
            DEBUGGERMSG(OXZONE_TA,
                    (L"  TA:Trap, attempting to ignore breakpoint\r\n"));

            hr = DD_SingleStepCBP(TRUE);
            if (FAILED(hr))
            {
                fHandled = TRUE;
                goto error;
            }

            if (DD_OnEmbeddedBreakpoint())
            {
                // Manually advance the thread past the embedded breakpoint.
                DEBUGGERMSG(OXZONE_TA,
                        (L"  TA:Trap, Continuing on Embedded BP\r\n"));

                // Since a debugbreak is an invalid instruction exception,
                // we never actually execute it.  Since we don't execute the
                // instruction, we may miss out on certain updates to the processor
                // state that go along with instruction executions.
                //
                // Here we perform any architecture-specific updates needed
                // to simulate having executed the debug break instruction.
                DD_SimulateDebugBreakExecution();

                hr = DD_GetNextPC(&pcNext);
                if (SUCCEEDED(hr))
                {
                    DEBUGGERMSG(OXZONE_TA,
                            (L"  TA:Trap, %08X <- PC <- %08X\r\n", 
                             pc, pcNext));
                    pc = pcNext;
                }
            }
            fHandled = TRUE;
            goto exit;
        }
    }

exit:
    DEBUGGERMSG(OXZONE_TA, (L"--TA:HandleBp, fHandled=%d\r\n", fHandled));
    return fHandled;

error:
    DBGRETAILMSG(OXZONE_ALERT, (L" TA:HandleBp, error %08X\r\n", hr));
    goto exit;
}


BOOL ThreadAffinity::Trap(EXCEPTION_RECORD *pExceptionRecord, CONTEXT *pContext,
        BOOL SecondChance, BOOL *pfIgnore)
{
    ULONG *pec;
    BOOL fHandled;

    pec = &pExceptionRecord->ExceptionCode;
    DEBUGGERMSG(OXZONE_TA, (L"++TA:Trap, EX=%08X, PC=%08X\r\n",
                              *pec,
                              CONTEXT_TO_PROGRAM_COUNTER(pContext)));
    
    DD_PushExceptionState(pExceptionRecord, pContext, SecondChance);

    // (1) 2nd chance exceptions go to the debugger
    if (SecondChance)
    {
        goto pass;
    }

    // (2) We are only interested in Breakpoint events
    if (*pec != STATUS_BREAKPOINT)
    {
        goto pass;
    }

    fHandled = HandleBreakpoint();
    if (fHandled)
    {
        goto handled;
    }


pass:
    // Send the event to kd so the user can debug it.  Since the device is
    // doing an interactive halt, purge all single steps.
    fHandled = FALSE;
    goto exit;

exit:
    DD_PopExceptionState();
    DEBUGGERMSG(OXZONE_TA, (L"--TA:Trap %d\r\n", fHandled));
    return fHandled;

handled:
    *pfIgnore = TRUE;
    fHandled = TRUE;
    goto exit;
}



