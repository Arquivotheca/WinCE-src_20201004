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

    PackData.cpp

Abstract:

    This is the device side implementation of PackData.  The point of this
    file is to take a process / thread / module structure and render it down
    into a nice structure that the caller can use.  These functions are also
    used to make porting to host side easier.

--*/

#include "osaxs_p.h"

#if defined(x86)

HRESULT GetExceptionRegistration(DWORD *pdwBuff)
{
    DEBUGGERMSG (OXZONE_PACKDATA, (L"++GetExceptionRegistration\r\n"));
    HRESULT hr = E_FAIL;

    // This is only valid for x86.  It is used by the debugger for unwinding through exception handlers.
    if (pdwBuff)
    {
        CALLSTACK* pStack = NULL;
        PVOID pRegistration = NULL;
        PVOID pvCallStack = pdwBuff + 1;

        memcpy(&pStack, pvCallStack, sizeof(VOID*));
        if (pStack)
        {
            // We are at a PSL boundary and need to look up the next registration
            // pointer -- don't trust the pointer we got from the host

            // TODO: This is gone in YAMAZAKI - fix it later
            // pRegistration = (VOID*)pStack->extra;
            
            pStack = pStack->pcstkNext;
        }
        else
        {
            // Request for the registration head pointer
            pRegistration = GetRegistrationHead();
            pStack = pCurThread->pcstkTop;
        }

        memcpy(pdwBuff, &pRegistration, sizeof(VOID*));
        memcpy(pvCallStack, &pStack, sizeof(VOID*));
        DEBUGGERMSG (OXZONE_PACKDATA, (L"  GetExceptionRegistration:  Registration Ptr: %8.8lx pCallStack: %8.8lx\r\n", (DWORD)pRegistration, (DWORD)pStack));
        hr = S_OK;
    }
    else
    {
        DEBUGGERMSG (OXZONE_PACKDATA, (L"  GetExceptionRegistration:  Invalid parameter\r\n", hr));
    }

    DEBUGGERMSG (OXZONE_PACKDATA, (L"--GetExceptionRegistration:  hr=0x%08x\r\n", hr));
    return hr;
}
#endif

