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

    pslret.cpp

Abstract:

    Provide PSL return address translation for OsAccess.

Environment:

    OsaxsT0 / OsaxsH

--*/

#include "osaxs_p.h"


BOOL ThreadExists (DWORD dwThread)
{
    DWORD iProc;
    BOOL fResult = FALSE;
    PPROCESS pProcArray = FetchProcArray ();

    DEBUGGERMSG (OXZONE_PSL, (L"++ThreadExists: 0x%.08x\r\n", dwThread));
    for (iProc = 0; iProc < MAX_PROCESSES; iProc++)
    {
        if (pProcArray[iProc].dwVMBase)
        {
            PTHREAD pth = pProcArray[iProc].pTh;
            while (pth)
            {
                if (pth == (PTHREAD) dwThread)
                {
                    fResult = TRUE;
                    goto Done;
                }
                pth = pth->pNextInProc;
            }
        }
    }

Done:
    DEBUGGERMSG (OXZONE_PSL, (L"--ThreadExists: %d\r\n", fResult)); 
    return fResult;
}


HRESULT InitTranslateRA (
    PSLRETSTATE *pprs,
    DWORD dwWalkThread,
    DWORD dwExceptionCode
    )
{
    HRESULT hr = S_OK;

    DEBUGGERMSG (OXZONE_PSL, (L"++InitTranslateRA: Thread=0x%.08x\r\n", dwWalkThread));
    if (! ThreadExists (dwWalkThread))
        hr = E_FAIL;

    if (SUCCEEDED (hr))
    {
        PTHREAD pth;
        pprs->pWalkThread = (PTHREAD) dwWalkThread;
        pth = FetchThreadStruct (pprs->pWalkThread);
        if (pth)
        {
            pprs->cFrameInCallStack = 0;
            pprs->pStk = pth->pcstkTop;
            pprs->pLastProc = pth->pProc;
            pprs->dwExceptionCode = dwExceptionCode;
        }
        else
            hr = E_FAIL;
    }
    DEBUGGERMSG (OXZONE_PSL, (L"--InitTranslateRA: 0x%.08x\r\n", hr));
    return hr;
}


HRESULT TranslateRAForPSL (
    PSLRETSTATE *pprs,
    DWORD dwAddr,
    DWORD dwStackFrameAddr,
    DWORD *pdwTranslatedAddr,
    DWORD *pdwStackPtr,
    DWORD *pdwProcess
    )
{
    HRESULT hr = S_OK;
    ULONG ulNewRA;
    ULONG ulTemp;
    PROCESS *pProc;

    DEBUGGERMSG (OXZONE_PSL, (L"++TranslateRAForPSL (RA=%8.8lX, dwCurFrame=%8.8lX)\r\n", dwAddr, dwStackFrameAddr));
    DEBUGGERMSG (OXZONE_PSL, (L"  TranslateRAForPSL pStk=%8.8lX, pCThd=%8.8lX,"
                      L" pWlkThd=%8.8lX, pLstPrc=%8.8lX\r\n",
            pprs->pStk, pCurThread, pprs->pWalkThread, pprs->pLastProc));

    if (!pdwTranslatedAddr || !pdwStackPtr || !pdwProcess || !pprs)
    {
        hr = E_INVALIDARG;
    }

    if (SUCCEEDED (hr))
    {
        *pdwStackPtr = 0; // default is 0 (for "to be ignored")
        ulNewRA = dwAddr;
        pProc = FetchProcStruct (pprs->pLastProc);
        if (! pProc)
            hr = E_FAIL;
    }

    if (SUCCEEDED (hr))
    {
        *pdwProcess = (DWORD)pProc->hProc;
        if (! dwAddr)
        {
            DEBUGGERMSG(OXZONE_PSL, (L"  TranslateRAForPSL (in RA = NULL), no more frames\r\n"));
            goto Finished;
        }

        DEBUGGERMSG(OXZONE_PSL, (L"  TranslateRAForPSL (in RA = %8.8lX)\r\n", dwAddr));

        if ((1 == pprs->cFrameInCallStack) && (pprs->dwExceptionCode == STATUS_INVALID_SYSTEM_SERVICE))
        { // case we get a raised exception in ObjectCall - 2nd frame unwinding - see .NET 67278
            DEBUGGERMSG(OXZONE_PSL, (L"  TranslateRAForPSL RaiseException in ObjectCall\r\n"));

            ulTemp = (ULONG) pprs->pStk->retAddr;
            ulNewRA = ZeroPtr (ulTemp);
            *pdwStackPtr = dwStackFrameAddr + 0x24; // This works for ARM
        }

        __try
        {
            if (((SYSCALL_RETURN == dwAddr) || (DIRECT_RETURN  == dwAddr) || ((DWORD) MD_CBRtn == dwAddr)) &&
                pprs->pStk)
            { 
                // PSL IPC style: Check for mode not process
                DWORD dwOffsetSP = (pprs->pStk->dwPrcInfo & CST_CALLBACK) ? CALLEE_SAVED_REGS : 0;
                DWORD dwPrevSP = pprs->pStk->dwPrevSP ? (pprs->pStk->dwPrevSP + dwOffsetSP) : 0;

                DEBUGGERMSG(OXZONE_PSL, (L"  TranslateRAForPSL PSL\r\n"));
                DEBUGGERMSG(OXZONE_PSL, (L"  TranslateRAForPSL pprcLast=%8.8lX, retAddr=%8.8lX, pcstkNext=%8.8lX dwPrevSP=%8.8lX dwOffsetSP=%8.8lX\r\n",
                    pprs->pStk->pprcLast, pprs->pStk->retAddr, pprs->pStk->pcstkNext, pprs->pStk->dwPrevSP, dwOffsetSP));

                *pdwStackPtr = dwPrevSP;

                ulTemp = (ULONG)pprs->pStk->retAddr;
                ulNewRA = ZeroPtr (ulTemp);

                if (dwStackFrameAddr)
                {
                    if ((DWORD)pprs->pStk->pprcLast > 0x10000uL)
                        pprs->pLastProc = pprs->pStk->pprcLast;
                    pprs->pStk = pprs->pStk->pcstkNext;
                }
            }
            else if (dwStackFrameAddr &&
                     pprs->pStk && 
                     ((DWORD)pprs->pStk < dwStackFrameAddr) && // Stack Frame Base pointer is now above cstk var (on the stack) so we changed process
                     !pprs->pStk->retAddr // Extra check (optional) that ret is NULL
                    )
            {
                // New IPC style (SC_PerformCallBack4)
                DEBUGGERMSG(OXZONE_PSL, (L"  TranslateRAForPSL Callback4\r\n"));
                DEBUGGERMSG(OXZONE_PSL, (L"  TranslateRAForPSL pprcLast=%8.8lX, pcstkNext=%8.8lX\r\n", pprs->pStk->pprcLast, pprs->pStk->pcstkNext));

                pprs->pLastProc = pprs->pStk->pprcLast;
                ulTemp = dwAddr;
                ulNewRA = ZeroPtr (ulTemp);
                pprs->pStk = pprs->pStk->pcstkNext;        
            }
            else
            {
                // Normal case:
                DEBUGGERMSG(OXZONE_PSL, (L"  TranslateRAForPSL Normal (in RA = out RA)\r\n"));
            }

            DEBUGGERMSG(OXZONE_PSL, (L"  TranslateRAForPSL "));
            if ((ulNewRA > (1 << VA_SECTION)) || (ulNewRA < (ULONG)DllLoadBase))
            {
                // Address from a EXE: Slotize it to its proper process
                DEBUGGERMSG(OXZONE_PSL, (L"EXE: mapinproc\r\n"));
                if (pprs->pLastProc)
                    ulNewRA = (ULONG)MapPtrInProc ((void *)ulNewRA, pprs->pLastProc);
                else
                    DEBUGGERMSG(OXZONE_PSL, (L"  TranslateRAForPSL *** ERROR pLastProc is NULL\r\n"));
            }
            else
            {
                // Address from a DLL: Zero-slotize it because the debugger has only a zero-slot address of it
                DEBUGGERMSG(OXZONE_PSL, (L"DLL: zeroslotize\r\n"));
                ulNewRA = (ULONG)ZeroPtr (ulNewRA);
            }

            *pdwTranslatedAddr = ulNewRA;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            hr = E_FAIL;
        }
    }

Finished:
    if (SUCCEEDED (hr))
    {
        DEBUGGERMSG(OXZONE_PSL, (L"  TranslateRAForPSL Old=%8.8lX -> New=%8.8lX\r\n",
                dwAddr, ulNewRA));
        DEBUGGERMSG(OXZONE_PSL, (L"  TranslateRAForPSL pStk=%8.8lX, pCThd=%8.8lX, pWlkThd=%8.8lX, pLstPrc=%8.8lX\r\n",
            pprs->pStk, pCurThread, pprs->pWalkThread, pprs->pLastProc));
    }

    if (SUCCEEDED(hr))
        ++pprs->cFrameInCallStack;

    DEBUGGERMSG(OXZONE_PSL, (L"--TranslateRAForPSL (0x%.08x)\r\n", hr));
    return hr;
}
