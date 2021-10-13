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
    
    cpu.cpp

Module Description:


--*/

#include "osaxs_p.h"


HRESULT OsAxsGetCpuPCur (DWORD dwCpuNum,
        PPROCESS *pprcVM,
        PPROCESS *pprcActv,
        PTHREAD *pth)
{
    HRESULT hr;

    if (dwCpuNum == 0)
    {
        DEBUGGERMSG(OXZONE_CPU, (L"  OsAxsGetCpuPCur: VM=%08X, PRC=%08X, THD=%08X\r\n", pVMProc, pActvProc, pCurThread));
        *pprcVM = pVMProc;
        *pprcActv = pActvProc;
        *pth = pCurThread;

        hr = S_OK;
    }
    else if (g_ppcbs[dwCpuNum-1])
    {
        DEBUGGERMSG(OXZONE_CPU, (L"  OsAxsGetCpuPCur: CPU#%d: VM=%08X, PRC=%08X, THD=%08X\r\n", g_ppcbs[dwCpuNum - 1]->dwCpuId, 
                pVMProc, pActvProc, pCurThread));
        *pprcVM = g_ppcbs[dwCpuNum - 1]->pVMPrc;
        *pprcActv = g_ppcbs[dwCpuNum - 1]->pCurPrc;
        *pth = g_ppcbs[dwCpuNum - 1]->pCurThd;
        hr = S_OK;
    }
    else
    {
        DEBUGGERMSG(OXZONE_CPU, (L"  OsAxsGetCpuPCur: Invalid Cpu Id %d\r\n", dwCpuNum));
        *pprcVM = NULL;
        *pprcActv = NULL;
        *pth = NULL;
        hr = E_FAIL;
    }

    return hr;
}


HRESULT OsAxsGetCpuContext(DWORD dwCpuId, PCONTEXT pctx, size_t cbctx)
{
    HRESULT hr;
    
    DEBUGGERMSG(OXZONE_CPU, (L"++OsAxsGetCpuContext: CPU%d, pctx=%08X, Context Size %d\r\n", dwCpuId, pctx, cbctx));
    
    if ((dwCpuId == 0) || (dwCpuId == PcbGetCurCpu()))
    {
        DEBUGGERMSG(OXZONE_CPU, (L"  OsAxsGetCpuContext: Copying out exception record\r\n"));
        hr = OsAxsGetExceptionRecord(pctx, cbctx);
    }
    else if (g_ppcbs[dwCpuId - 1] && g_ppcbs[dwCpuId - 1]->pSavedContext)
    {
        OsAxsContextFromCpuContext(pctx, reinterpret_cast<CPUCONTEXT*>(g_ppcbs[dwCpuId-1]->pSavedContext));
        pctx->ContextFlags = CE_CONTEXT_INTEGER | CE_CONTEXT_CONTROL;
        hr = S_OK;
    }
    else
    {
        hr = E_FAIL;
    }

    DEBUGGERMSG(OXZONE_CPU, (L"--OsAxsGetCpuContext: %08X\r\n", hr));
    return hr;
}


HRESULT OsAxsSetCpuContext(DWORD dwCpuId, PCONTEXT pctx, size_t cbctx)
{
    HRESULT hr;

    DEBUGGERMSG(OXZONE_CPU, (L"++OsAxsSetCpuContext: CPU%d, pctx=%08X, Context Size %d\r\n", dwCpuId, pctx, cbctx));
    if ((dwCpuId == 0) || (dwCpuId == PcbGetCurCpu()))
    {
        hr = OsAxsSetExceptionRecord(pctx, cbctx);
    }
    else if (g_ppcbs[dwCpuId - 1] && g_ppcbs[dwCpuId - 1]->pSavedContext)
    {
        // TODO: Add support to access extended contexts on all of these CPUs.
        const DWORD kdwStandardCtxFlags = CE_CONTEXT_INTEGER | CE_CONTEXT_CONTROL;

        if ((pctx->ContextFlags & kdwStandardCtxFlags) == kdwStandardCtxFlags)
        {
            OsAxsCpuContextFromContext(reinterpret_cast<CPUCONTEXT*>(g_ppcbs[dwCpuId - 1]->pSavedContext), pctx);
        }
        hr = S_OK;
    }
    else
    {
        hr = E_FAIL;
    }
    DEBUGGERMSG(OXZONE_CPU, (L"--OsAxsSetCpuContext: %08X\r\n", hr));
    return hr;
}
