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

#include <osaxs_p.h>

HRESULT OSAXSTranslateAddress(HANDLE hProc, void* pvAddr, BOOL fReturnKVA, void** ppvPhysOrKVA)
{
    DEBUGGERMSG(OXZONE_VM, (L"++OSAXSTranslateAddress hProc = 0x%08x, addr = 0x%08x\r\n", hProc, pvAddr));
    HRESULT hr;
    PPROCESS pProc = pVMProc;
    if (hProc)
    {
        pProc = OSAXSHandleToProcess(hProc);
    }

    if (pProc)
    {
        if (fReturnKVA)
        {
            // *ppvPhysOrKVA = g_OsaxsData.pfnGetKAddrOfProcess(pProc, pvAddr);
            *ppvPhysOrKVA = MapToDebuggeeCtxKernEquivIfAcc(pProc, pvAddr, FALSE);
            DEBUGGERMSG(OXZONE_VM, (L"  OSAXSTranslateAddress (0x%08x, 0x%08x) -> KVA 0x%08x\r\n", hProc, pvAddr, *ppvPhysOrKVA));
        }
        else
        {
            *ppvPhysOrKVA = reinterpret_cast<void*>(g_OsaxsData.pfnGetPFNOfProcess(pProc, pvAddr));
            DEBUGGERMSG(OXZONE_VM, (L"  OSAXSTranslateAddress (0x%08x, 0x%08x) -> PA 0x%08x\r\n", hProc, pvAddr, *ppvPhysOrKVA));
        }

        if (*ppvPhysOrKVA)
            hr = S_OK;
        else
            hr = E_FAIL;
    }
    else
    {
        DEBUGGERMSG(OXZONE_VM || OXZONE_ALERT, (L"  OSAXSTranslateAddress failed! Bad hProc? (%08X, %08X, %d)\r\n", hProc, pvAddr, fReturnKVA));
        hr = E_HANDLE;
    }

    DEBUGGERMSG(OXZONE_VM, (L"--OSAXSTranslateAddress returning HRESULT 0x%08x\r\n", hr));
    return hr;
}
