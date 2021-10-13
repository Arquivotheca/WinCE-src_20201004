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
/*++

Module Name:
    
    translatera.cpp

Module Description:

    thread return address translation

--*/
#include "osaxs_p.h"
#include "psyscall.h"


static BOOL IsSysCallReturn (DWORD dwRA)
{
#ifdef x86
    return dwRA == g_pKData->dwSyscallReturnTrap;
#else
    return (dwRA == g_pKData->dwSyscallReturnTrap) ||
        ((dwRA + INST_SIZE) == g_pKData->dwSyscallReturnTrap);
#endif
}

static BOOL IsDirectReturn (DWORD dwRA)
{
#ifdef x86
    return dwRA == (DWORD) g_pKData->pAPIReturn;
#else
    return (dwRA == (DWORD) g_pKData->pAPIReturn) ||
        ((dwRA + INST_SIZE) == g_pKData->pAPIReturn);
#endif
}

static PPROCESS GetStkProcess (PCALLSTACK pcstk)
{
    PPROCESS pprc;

    if (pcstk->pprcVM)
        pprc = pcstk->pprcVM;
    else if (pcstk->pprcLast)
        pprc = pcstk->pprcLast;
    else
        pprc = NULL;

    return pprc;
}

// TranslateReturnAddress functionality is placed here.
// The purpose of this is to decouple dependency on the layout of OS structure.

HRESULT OsAxsTranslateReturnAddress (HANDLE hThread,
        DWORD *pdwState,
        DWORD *pdwReturn,
        DWORD *pdwFrame,
        DWORD *pdwFrameCurProcHnd)
{
    HRESULT hr;
    PTHREAD pth;
    PCALLSTACK pcstkCur;
    DWORD i;

    DEBUGGERMSG (OXZONE_UNWINDER, (L"++OsAxsTranslateReturnAddress State=%d, hTh=%08X, RA=%08X, FP=%08X, hPrc=%08X\r\n",
                *pdwState, *pdwReturn, *pdwFrame, *pdwFrameCurProcHnd));

    // Thread handle -> thread pointer
    pth = OsAxsHandleToThread (hThread);
    if (!pth)
    {
        hr = E_HANDLE;
        goto Exit;
    }

    // Default frame VM process
    if (*pdwFrameCurProcHnd == 0)
    {
        if (pth->pprcVM != NULL)
        {
            *pdwFrameCurProcHnd = pth->pprcVM->dwId;
        }
        else if (pth->pprcActv != NULL)
        {
            *pdwFrameCurProcHnd = pth->pprcActv->dwId;
        }
        else
        {
            DEBUGGERMSG(OXZONE_ALERT, (L" OsAxsTranslateRA: pth %08X (%08X) has no VM or Actv process\r\n"));
        }
    }

    pcstkCur = pth->pcstkTop;

    // If this is the exception thread, need to bump the first one off.  The OS actually keeps the falsified
    // pcstk record that tracks OS capturing an exception.
    if ((pth == pCurThread) && pcstkCur && ((pcstkCur->dwPrcInfo & CST_EXCEPTION) != 0))
        pcstkCur = pcstkCur->pcstkNext;

    // Advance Stack frame to the current one.
    for (i = 0; (i < *pdwState) && pcstkCur; ++i)
        pcstkCur = pcstkCur->pcstkNext;

    if (pcstkCur && (IsSysCallReturn (*pdwReturn) || IsDirectReturn (*pdwReturn)))
    {
        *pdwReturn = (DWORD) pcstkCur->retAddr;
#ifndef x86
        *pdwFrame = pcstkCur->dwPrevSP;
#endif
        if (GetStkProcess (pcstkCur))
            *pdwFrameCurProcHnd = GetStkProcess (pcstkCur)->dwId;
        else
        {
            hr = E_FAIL;
            goto Exit;
        }

        // Since the stkframe was used, increment the unwinder state.
        ++(*pdwState);
    }
    else if (*pdwFrame && pcstkCur && ((DWORD) pcstkCur < *pdwFrame) && (pcstkCur->retAddr == 0))
    {
        // callback4 return
        if (GetStkProcess (pcstkCur))
            *pdwFrameCurProcHnd = GetStkProcess (pcstkCur)->dwId;
        else
        {
            hr = E_FAIL;
            goto Exit;
        }

        // Since the stkframe was used, increment the unwinder state.
        ++(*pdwState);
    }

    hr = S_OK;
    
Exit:
    DEBUGGERMSG (OXZONE_UNWINDER, (L"--OsAxsTranslateReturnAddress (%08X) State=%d, hTh=%08X, RA=%08X, FP=%08X, hPrc=%08X\r\n",
                hr, *pdwState, *pdwReturn, *pdwFrame, *pdwFrameCurProcHnd));
    return hr;
}


BOOL PSLCallHandleAndMethod(ULONG Offset, ULONG *Handle, LONG *Method)
{
    if (Offset < (ULONG)LAST_METHOD || Offset > (ULONG)MAX_METHOD_NUM)
    {
        DEBUGGERMSG(OXZONE_UNWINDER, (L"  PSLResolve, %08X is not an API offset\r\n", Offset));
        return FALSE;
    }
    
#ifdef x86
    if (Offset & 0x1)
        return FALSE;
    OsAxsReadMemory(Handle, PcbGetVMProc(), (void *)(DD_ExceptionState.context->Esp), sizeof(*Handle));
    *Method = (LONG) (Offset - FIRST_METHOD) >> 1;
#endif

#ifdef ARM
    if (Offset & 0x3)
        return FALSE;
    *Handle = DD_ExceptionState.context->R0;
    *Method = (LONG) (Offset - FIRST_METHOD) >> 2;
#endif

#ifdef MIPS
    if ((Offset & 0x3) != 2)
        return FALSE;
    *Handle = (ULONG) DD_ExceptionState.context->IntA0;
    *Method = (LONG) (Offset - FIRST_METHOD) >> 2;
#endif

#ifdef SH
    if (Offset & 0x1)
        return FALSE;
    *Handle = DD_ExceptionState.context->R4;
    *Method = (LONG) (Offset - FIRST_METHOD) >> 1;
#endif

    DEBUGGERMSG(OXZONE_UNWINDER, (L"  PSLResolve, h=%08X, m=%08X\r\n", *Handle, *Method));
    return TRUE;
}


// Translate Offset to the PSL destination.
ULONG OsAxsTranslateOffset(ULONG Offset)
{
    DEBUGGERMSG(OXZONE_UNWINDER, (L"+OsAxsTranslateOffset, %08X\r\n", Offset));
    ULONG Handle;
    LONG Method;
    ULONG NewOffset = Offset;
    
    if (IsSysCallReturn(Offset))
    {
        DEBUGGERMSG(OXZONE_UNWINDER, (L" OsAxsTranslateOffset, Syscall return\r\n"));
        PCALLSTACK pstk = pCurThread->pcstkTop;
        if (pstk)
            pstk->pcstkNext;
        
        NewOffset = (pstk)? (ULONG)pstk->retAddr : Offset;
    }
    else if (PSLCallHandleAndMethod(Offset, &Handle, &Method))
    {
        CINFO const *pci = NULL;
        LONG ApiIdx = NUM_API_SETS;
        if (Method <= 0)
        {
            // Implicit
            Method = -Method;
            ApiIdx = (Method >> APISET_SHIFT) & APISET_MASK;
            Method &= METHOD_MASK;
            pci = SystemAPISets[ApiIdx];
            ApiIdx = pci->type;
        }

        DEBUGGERMSG(OXZONE_UNWINDER, (L" OsAxsTranslateOffset, API=%d, Handle=%08X, Method=%08X\r\n",
                ApiIdx, Handle, Method));

        if (ApiIdx != 0)
        {
            PHDATA HandleData = h2pHDATA((HANDLE) Handle, pActvProc->phndtbl);
            if (HandleData != NULL && HandleData->pci != NULL)
            {
                DEBUGGERMSG(OXZONE_UNWINDER, (L"  OsAxsTranslateOffset, cMethods %d, disp %d\r\n",
                    HandleData->pci->cMethods, HandleData->pci->disp));

                pci = HandleData->pci;
            }
            else
            {
#ifdef x86
                // Second chance for x86.  retail code optimizes the
                // trap from a call to jmp...
                OsAxsReadMemory(&Handle, PcbGetVMProc(), (void *)(DD_ExceptionState.context->Esp + REGSIZE), sizeof(Handle));

                HandleData = h2pHDATA((HANDLE) Handle, pActvProc->phndtbl);
                if (HandleData != NULL && HandleData->pci != NULL)
                {
                    DEBUGGERMSG(OXZONE_UNWINDER, (L" OsAxsTranslateOffset, 2nd chance cMethods %d, disp %d\r\n",
                        HandleData->pci->cMethods, HandleData->pci->disp));
                    
                    pci = HandleData->pci;
                }
                else
                {
                    goto Exit;
                }
#else
                goto Exit;
#endif
            }
        }

        if(pci == NULL)
        {
            goto Exit;
        }
        DEBUGGERMSG(OXZONE_UNWINDER, (L" OsAxsTranslateOffset, %08X\r\n", pci->cMethods));
        if (Method < pci->cMethods)
        {
            switch (pci->disp)
            {
            case DISPATCH_KERNEL:
            case DISPATCH_I_KERNEL:
            case DISPATCH_KERNEL_PSL:
            case DISPATCH_I_KPSL:
                NewOffset = (ULONG) pci->ppfnExtMethods[Method];
            default:
                break;
            }
        }
    }

Exit:
    DEBUGGERMSG(OXZONE_UNWINDER, (L"-OsAxsTranslateOffset, %08X\r\n", NewOffset));
    return NewOffset;
}
