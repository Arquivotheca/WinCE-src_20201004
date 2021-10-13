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
    
    unwinder.cpp

Module Description:

    Stack unwinder

--*/
#include "osaxs_p.h"

ULONG GetCallStack(PTHREAD pth, ULONG dwMaxFrames, LPVOID lpFrames, DWORD dwFlags, DWORD dwSkip)
{
    DWORD dwFrames;
    static CONTEXT ctx;
    BOOL RestorePrcInfo = FALSE;
    DWORD originalPrcInfo = 0;

    if (!pfnDoThreadGetContext || !pfnNKGetThreadCallStack)
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  Unwinder!GetCallStack: pfnDoThreadGetContext or pfnNKGetThreadCallStack is NULL\r\n"));
        dwFrames = 0;
        goto Exit;
    }

    if (!pth)
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  Unwinder!GetCallStack: Invalid thread pointer, pTh=0x%08X\r\n", pth));
        dwFrames = 0;
        goto Exit;
    }

    if (pth == pCurThread)
    {
        // If this is the current thread we need to start unwinding from the point of 
        // the exception, otherwise we get the HDSTUB + OSAXS frames included
        ctx = *g_pexceptionInfo->ExceptionPointers.ContextRecord;

        if (pth->pcstkTop != NULL && (pth->pcstkTop->dwPrcInfo & CST_EXCEPTION) == 0)
        {
            // Special case for the current thread.
            //
            // If the current thread has a pcstkTop record (it better!) and the dwProcInfo field
            // is not flagged as an exception field, we made need to fake an exception here.
            // (This is for the RaiseException()/CaptureDumpFile() case.  The kernel does not
            // set CST_EXCEPTION on calls to RaiseException.)
            //
            // Check whether the stackpointer from the exception record will fit within the thread's
            // secure stack range. If it does, then do nothing. If it does not, temporarily or
            // CST_EXCEPTION to CALLSTACK.dwPrcInfo.
            //
            // (When dwPrcInfo has the CST_EXCEPTION bit set, the kernel unwinder will automatically
            // pop off the topmost CALLSTACK record before doing any stack walking.  This should
            // synchronize the pcstkTop pointer with the exception context.)
            const DWORD StackPointer = static_cast<DWORD>(CONTEXT_TO_STACK_POINTER(&ctx));
            const DWORD SecureStackLo = pth->tlsSecure[PRETLS_STACKBASE];
            const DWORD SecureStackHi = SecureStackLo + pth->tlsSecure[PRETLS_STACKSIZE] - (4 * REG_SIZE);

            if (StackPointer < SecureStackLo || SecureStackHi <= StackPointer)
            {
                DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  Unwinder!GetCallStack: Thread %08X Stackpointer %08X is not in Secure Stack Range: %08X-%08X\r\n",
                                             pth, StackPointer, SecureStackLo, SecureStackHi));
                originalPrcInfo = pth->pcstkTop->dwPrcInfo;
                pth->pcstkTop->dwPrcInfo |= CST_EXCEPTION;
                RestorePrcInfo = TRUE;
                DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  Unwinder!GetCallStack: Thread %08X dwPrcInfo was %08X, is %08X\r\n", pth, originalPrcInfo,
                                             pth->pcstkTop->dwPrcInfo));
            }
        }
    }
    else
    {
        // For other threads, get the threads context
        ctx.ContextFlags = CONTEXT_FULL;
        if (!pfnDoThreadGetContext (pth, &ctx)) // TODO: Fix parameter 1 for YAMAZAKI
        {
            DEBUGGERMSG(OXZONE_ALERT, (L"  Unwinder!GetCallStack: DoThreadGetContext failed, hThrd=0x%08X, Error=%u\r\n", 
                                      pth->dwId, GetLastError()));
            dwFrames = 0;
            goto Exit;
        }
    }
    dwFrames = pfnNKGetThreadCallStack(pth, dwMaxFrames, lpFrames, dwFlags, dwSkip, &ctx);

Exit:
    if (RestorePrcInfo)
    {
        // Remember to restore the CALLSTACK.dwPrcInfo.  (Currently, the kernel doesn't really
        // check this flag anywhere else but in the unwinder.)
        if (pth->pcstkTop != NULL)
        {
            pth->pcstkTop->dwPrcInfo = originalPrcInfo;
            DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  Unwinder!GetCallStack: Thread %08X restore dwPrcInfo to %08X\r\n", pth, pth->pcstkTop->dwPrcInfo));
        }
    }

    return dwFrames;
}


#ifdef OSAXST1_BUILD
const DWORD MAX_FRAMES_PER_ITER = 20;

HRESULT OSAXSGetCallstack(HANDLE hThread, DWORD FrameStart, DWORD FramesToRead, DWORD *pFramesReturned, void *FrameBuffer, 
    DWORD *pFrameBufferLength)
{
    HRESULT hr = S_OK;
    THREAD *pthd = OSAXSHandleToThread(hThread);
    if (pthd)
    {
        const DWORD MAX_FRAMES_IN_BUF = *pFrameBufferLength / sizeof(OSAXS_CALLSTACK_FRAME);
        if (FramesToRead > MAX_FRAMES_IN_BUF)
            FramesToRead = MAX_FRAMES_IN_BUF;

        *pFramesReturned = GetCallStack(pthd, FramesToRead, FrameBuffer, STACKSNAP_EXTENDED_INFO | STACKSNAP_RETURN_FRAMES_ON_ERROR, FrameStart);
        if (*pFramesReturned)
        {
            *pFrameBufferLength = *pFramesReturned * sizeof(OSAXS_CALLSTACK_FRAME);
            hr = S_OK;
        }
        else
        {
            hr = E_FAIL;
        }
    }
    else
    {
        hr = E_HANDLE;
    }
    return hr;
}
#endif


#ifdef _TEST
#define MAX_NUM_FRAMES  10
VOID RunCallStackTest()
{
    DEBUGGERMSG (1, (TEXT("+++RunCallStackTest: \r\n")));
    CallSnapshotEx callstack[MAX_NUM_FRAMES] = {0};
    GetCallStack(g_pKData->pCurThd, MAX_NUM_FRAMES, (LPVOID)callstack, 2, 0, NULL);
    for (int i=0; i < MAX_NUM_FRAMES; i++)
    {
        DEBUGGERMSG(1, (TEXT("0x%.08x - "), callstack[i].dwReturnAddr));
    }
    DEBUGGERMSG (1, (TEXT("\r\n---RunCallStackTest: ")));
}
#endif

