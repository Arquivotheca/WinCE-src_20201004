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
    
    unwinder.cpp

Module Description:

    Stack unwinder

--*/
#include "osaxs_p.h"

ULONG GetCallStack(HANDLE hThrd, ULONG dwMaxFrames, LPVOID lpFrames, DWORD dwFlags, DWORD dwSkip)
{
    DWORD dwFrames;
    PTHREAD pth;
    CONTEXT ctx;

    if (!pfnDoThreadGetContext || !pfnNKGetThreadCallStack)
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  Unwinder!GetCallStack: pfnDoThreadGetContext or pfnNKGetThreadCallStack is NULL\r\n"));
        dwFrames = 0;
        goto Exit;
    }

    pth = HandleToThread (hThrd);
    if (!pth)
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  Unwinder!GetCallStack: Invalid thread handle, hThrd=0x%08X\r\n", hThrd));
        dwFrames = 0;
        goto Exit;
    }

    if (pth == pCurThread)
    {
        // If this is the current thread we need to start unwinding from the point of 
        // the exception, otherwise we get the HDSTUB + OSAXS frames included
        ctx = *g_pContextException;
    }
    else
    {
        // For other threads, get the threads context
        ctx.ContextFlags = CONTEXT_FULL;
        if (!pfnDoThreadGetContext(hThrd, &ctx))
        {
            DEBUGGERMSG(OXZONE_ALERT, (L"  Unwinder!GetCallStack: DoThreadGetContext failed, hThrd=0x%08X, Error=%u\r\n", 
                                      hThrd, pfnGetLastError()));
            dwFrames = 0;
            goto Exit;
        }
    }

    dwFrames = pfnNKGetThreadCallStack(pth, dwMaxFrames, lpFrames, dwFlags, dwSkip, &ctx);
    
Exit:

    return dwFrames;
}

#ifdef _TEST
#define MAX_NUM_FRAMES  10
VOID RunCallStackTest()
{
    DEBUGGERMSG (1, (TEXT("+++RunCallStackTest: \r\n")));
    HANDLE hThread = GetCurrentThread();
    CallSnapshotEx callstack[MAX_NUM_FRAMES] = {0};
    GetCallStack(hThread, MAX_NUM_FRAMES, (LPVOID)callstack, 2, 0, NULL);
    for (int i=0; i < MAX_NUM_FRAMES; i++)
    {
        DEBUGGERMSG(1, (TEXT("0x%.08x - "), callstack[i].dwReturnAddr));
    }
    DEBUGGERMSG (1, (TEXT("\r\n---RunCallStackTest: ")));
}
#endif

