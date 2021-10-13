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

    PackData.cpp

Abstract:

    This is the device side implementation of PackData.  The point of this
    file is to take a process / thread / module structure and render it down
    into a nice structure that the caller can use.  These functions are also
    used to make porting to host side easier.

--*/

#include "osaxs_p.h"


HRESULT GetModuleO32Data (DWORD dwAddrModule, DWORD *pdwNumO32Data, void *pvResult, DWORD *pcbResult)
{
    HRESULT hr = S_OK;
    o32_lite *o32_ptr = NULL;
    DWORD co32 = 0;

    DEBUGGERMSG (OXZONE_PACKDATA, (L"++GetModuleO32Data: 0x%08x\r\n", dwAddrModule));
    __try
    {
        PMODULE pmod = reinterpret_cast<PMODULE> (dwAddrModule);
        if (pmod && pmod->lpSelf == pmod)
        {
            if (pmod->wRefCnt)
            {
                co32 = pmod->e32.e32_objcnt;
                o32_ptr = pmod->o32_ptr;

                DEBUGGERMSG (OXZONE_PACKDATA, (L"  GetModuleO32Data: Module data.\r\n"));
            }
            else
            {
                DBGRETAILMSG (1, (L"  GetModuleO32Data: Module is not being used, inuse=0\r\n"));
                hr = E_FAIL;
            }
        }
        else if (pmod)
        {
            PROCESS *pProcess = reinterpret_cast <PROCESS *> (dwAddrModule);
#if 0  // TODO: Fix this for YAMA
            PMODULE pCoreDll = (PMODULE)hCoreDll;
            DWORD dwInUse = (1 << pProcess->bASID);

            // Make sure process is not busy starting or dying (Check if using CoreDll.dll)
            // Except for Nk.exe, we always allow it
            if ((!pCoreDll || !(dwInUse & pCoreDll->inuse)) && (0 != pProcess->bASID))
            {
                DBGRETAILMSG (1, (L"  GetModuleO32Data: Bad process struct @0x%08x\r\n", pProcess));
                hr = E_FAIL;
            }
            else
#endif
            {
                co32 = pProcess->e32.e32_objcnt;
                o32_ptr = pProcess->o32_ptr;
                DEBUGGERMSG (OXZONE_PACKDATA, (L"  GetModuleO32Data: Process data.\r\n"));
            }
        }
        else
        {
            DEBUGGERMSG (OXZONE_PACKDATA, (L"  GetModuleO32Data: NULL address.\r\n"));
            hr = E_INVALIDARG;
        }

        if (SUCCEEDED (hr))
        {
            // copy out the o32_ptr data.
            size_t cbO32Data = sizeof (o32_lite) * co32;

            DEBUGGERMSG (OXZONE_PACKDATA, (L"  GetModuleO32Data: num o32_lite=%d, addr o32_lite=0x%08x\r\n", co32, o32_ptr));

            *pdwNumO32Data = co32;
            if (cbO32Data)
            {
                if (*pcbResult >= cbO32Data)
                {
                    if (o32_ptr)
                    {
                        SafeMoveMemory (g_pKData->pCurPrc, pvResult, o32_ptr, cbO32Data);
                    }
                    else
                    {
                        DEBUGGERMSG (OXZONE_PACKDATA, (L"  GetModuleO32Data: o32_ptr is null?\r\n"));
                        cbO32Data = 0;
                        *pdwNumO32Data = 0;
                    }
                }
                else
                {
                    DBGRETAILMSG (1, (L"  GetModuleO32Data: Not enough room in the result buffer!\r\n"));
                    hr = E_OUTOFMEMORY;
                }
            }

            if (SUCCEEDED (hr))
            {
                *pcbResult = cbO32Data;
            }
            else
            {
                *pcbResult = 0;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DBGRETAILMSG (1, (L"  GetModuleO32Data: Unexpected exception.\r\n"));
        hr = E_FAIL;
    }
    DBGRETAILMSG (OXZONE_PACKDATA || FAILED (hr), (L"--GetModuleO32Data: hr=0x%08x, pmod=0x%08x\r\n", hr, dwAddrModule));
    return hr;
}


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

