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
    
    unwinder.cpp

Module Description:

    Stack unwinder

--*/
#include "osaxs_p.h"

ULONG GetCallStack(PTHREAD pth, ULONG dwMaxFrames, LPVOID lpFrames, DWORD dwFlags, DWORD dwSkip)
{
    DWORD dwFrames = 0;
    static CONTEXT ctx;

    if (!pth)
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  Unwinder!GetCallStack: Invalid thread pointer, pTh=0x%08X\r\n", pth));
        dwFrames = 0;
        goto Exit;
    }
   
    if (pth == pCurThread)
    {
        ctx = *DD_ExceptionState.context;
    }
    else
    {
        // For other threads, get the threads context
        ctx.ContextFlags = CONTEXT_FULL;
        if (!SCHL_DoThreadGetContext (pth, &ctx)) // TODO: Fix parameter 1 for YAMAZAKI
        {
            DEBUGGERMSG(OXZONE_ALERT, (L"  Unwinder!GetCallStack: DoThreadGetContext failed, hThrd=0x%08X, Error=%u\r\n", 
                                      pth->dwId, GetLastError()));
            dwFrames = 0;
            goto Exit;
        }
    }
    dwFrames = DD_GetCallStack(pth, &ctx, dwMaxFrames, lpFrames, dwFlags, dwSkip);

Exit:
    return dwFrames;           
}

