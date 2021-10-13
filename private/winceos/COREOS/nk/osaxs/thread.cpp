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
    
    thread.cpp

Module Description:

--*/

#include <osaxs_p.h>


void OsAxsContextFromCpuContext(PCONTEXT pctx, CPUCONTEXT *pcpuctx)
{
#if defined(x86)
    pctx->SegGs = pcpuctx->TcxGs;
    pctx->SegFs = pcpuctx->TcxFs;
    pctx->SegEs = pcpuctx->TcxEs;
    pctx->SegDs = pcpuctx->TcxDs;
    pctx->Edi = pcpuctx->TcxEdi;
    pctx->Esi = pcpuctx->TcxEsi;
    pctx->Ebx = pcpuctx->TcxEbx;
    pctx->Edx = pcpuctx->TcxEdx;
    pctx->Ecx = pcpuctx->TcxEcx;
    pctx->Eax = pcpuctx->TcxEax;
    pctx->Ebp = pcpuctx->TcxEbp;
    pctx->Eip = pcpuctx->TcxEip;
    pctx->SegCs = pcpuctx->TcxCs;
    pctx->EFlags = pcpuctx->TcxEFlags;
    pctx->Esp = pcpuctx->TcxEsp;
    pctx->SegSs = pcpuctx->TcxSs;
#elif defined (SH4)
    pctx->PR = pcpuctx->PR;
    pctx->MACH = pcpuctx->MACH;
    pctx->MACL = pcpuctx->MACL;
    pctx->GBR = pcpuctx->GBR;
    pctx->R0 = pcpuctx->R0;
    pctx->R1 = pcpuctx->R1;
    pctx->R2 = pcpuctx->R2;
    pctx->R3 = pcpuctx->R3;
    pctx->R4 = pcpuctx->R4;
    pctx->R5 = pcpuctx->R5;
    pctx->R6 = pcpuctx->R6;
    pctx->R7 = pcpuctx->R7;
    pctx->R8 = pcpuctx->R8;
    pctx->R9 = pcpuctx->R9;
    pctx->R10 = pcpuctx->R10;
    pctx->R11 = pcpuctx->R11;
    pctx->R12 = pcpuctx->R12;
    pctx->R13 = pcpuctx->R13;
    pctx->R14 = pcpuctx->R14;
    pctx->R15 = pcpuctx->R15;
    pctx->Fir = pcpuctx->Fir;
    pctx->Psr = pcpuctx->Psr;
#elif defined (ARM)
    pctx->Psr = pcpuctx->Psr;
    pctx->R0 = pcpuctx->R0;
    pctx->R1 = pcpuctx->R1;
    pctx->R2 = pcpuctx->R2;
    pctx->R3 = pcpuctx->R3;
    pctx->R4 = pcpuctx->R4;
    pctx->R5 = pcpuctx->R5;
    pctx->R6 = pcpuctx->R6;
    pctx->R7 = pcpuctx->R7;
    pctx->R8 = pcpuctx->R8;
    pctx->R9 = pcpuctx->R9;
    pctx->R10 = pcpuctx->R10;
    pctx->R11 = pcpuctx->R11;
    pctx->R12 = pcpuctx->R12;
    pctx->Sp = pcpuctx->Sp;
    pctx->Lr = pcpuctx->Lr;
    pctx->Pc = pcpuctx->Pc;
#elif defined (MIPS)
    pctx->IntAt = pcpuctx->IntAt;
    pctx->IntV0 = pcpuctx->IntV0;
    pctx->IntV1 = pcpuctx->IntV1;
    pctx->IntA0 = pcpuctx->IntA0;
    pctx->IntA1 = pcpuctx->IntA1;
    pctx->IntA2 = pcpuctx->IntA2;
    pctx->IntA3 = pcpuctx->IntA3;
    pctx->IntT0 = pcpuctx->IntT0;
    pctx->IntT1 = pcpuctx->IntT1;
    pctx->IntT2 = pcpuctx->IntT2;
    pctx->IntT3 = pcpuctx->IntT3;
    pctx->IntT4 = pcpuctx->IntT4;
    pctx->IntT5 = pcpuctx->IntT5;
    pctx->IntT6 = pcpuctx->IntT6;
    pctx->IntT7 = pcpuctx->IntT7;
    pctx->IntS0 = pcpuctx->IntS0;
    pctx->IntS1 = pcpuctx->IntS1;
    pctx->IntS2 = pcpuctx->IntS2;
    pctx->IntS3 = pcpuctx->IntS3;
    pctx->IntS4 = pcpuctx->IntS4;
    pctx->IntS5 = pcpuctx->IntS5;
    pctx->IntS6 = pcpuctx->IntS6;
    pctx->IntS7 = pcpuctx->IntS7;
    pctx->IntT8 = pcpuctx->IntT8;
    pctx->IntT9 = pcpuctx->IntT9;
    pctx->IntK0 = pcpuctx->IntK0;
    pctx->IntK1 = pcpuctx->IntK1;
    pctx->IntGp = pcpuctx->IntGp;
    pctx->IntSp = pcpuctx->IntSp;
    pctx->IntS8 = pcpuctx->IntS8;
    pctx->IntRa = pcpuctx->IntRa;
    pctx->IntLo = pcpuctx->IntLo;
    pctx->IntHi = pcpuctx->IntHi;
    pctx->Fir = pcpuctx->Fir;
    pctx->Psr = pcpuctx->Psr;
#endif /*#if defined (MIPS) */
}

void OsAxsCpuContextFromContext(CPUCONTEXT *pcpuctx, PCONTEXT pctx)
{
#if defined(x86)
    pcpuctx->TcxGs = pctx->SegGs;
    pcpuctx->TcxFs = pctx->SegFs;
    pcpuctx->TcxEs = pctx->SegEs;
    pcpuctx->TcxDs = pctx->SegDs;
    pcpuctx->TcxEdi = pctx->Edi;
    pcpuctx->TcxEsi = pctx->Esi;
    pcpuctx->TcxEbx = pctx->Ebx;
    pcpuctx->TcxEdx = pctx->Edx;
    pcpuctx->TcxEcx = pctx->Ecx;
    pcpuctx->TcxEax = pctx->Eax;
    pcpuctx->TcxEbp = pctx->Ebp;
    pcpuctx->TcxEip = pctx->Eip;
    pcpuctx->TcxCs = pctx->SegCs;
    pcpuctx->TcxEFlags = pctx->EFlags;
    pcpuctx->TcxEsp = pctx->Esp;
    pcpuctx->TcxSs = pctx->SegSs;
#elif defined (SH4)
    pcpuctx->PR = pctx->PR;
    pcpuctx->MACH = pctx->MACH;
    pcpuctx->MACL = pctx->MACL;
    pcpuctx->GBR = pctx->GBR;
    pcpuctx->R0 = pctx->R0;
    pcpuctx->R1 = pctx->R1;
    pcpuctx->R2 = pctx->R2;
    pcpuctx->R3 = pctx->R3;
    pcpuctx->R4 = pctx->R4;
    pcpuctx->R5 = pctx->R5;
    pcpuctx->R6 = pctx->R6;
    pcpuctx->R7 = pctx->R7;
    pcpuctx->R8 = pctx->R8;
    pcpuctx->R9 = pctx->R9;
    pcpuctx->R10 = pctx->R10;
    pcpuctx->R11 = pctx->R11;
    pcpuctx->R12 = pctx->R12;
    pcpuctx->R13 = pctx->R13;
    pcpuctx->R14 = pctx->R14;
    pcpuctx->R15 = pctx->R15;
    pcpuctx->Fir = pctx->Fir;
    pcpuctx->Psr = pctx->Psr;
#elif defined (ARM)
    pcpuctx->Psr = pctx->Psr;
    pcpuctx->R0 = pctx->R0;
    pcpuctx->R1 = pctx->R1;
    pcpuctx->R2 = pctx->R2;
    pcpuctx->R3 = pctx->R3;
    pcpuctx->R4 = pctx->R4;
    pcpuctx->R5 = pctx->R5;
    pcpuctx->R6 = pctx->R6;
    pcpuctx->R7 = pctx->R7;
    pcpuctx->R8 = pctx->R8;
    pcpuctx->R9 = pctx->R9;
    pcpuctx->R10 = pctx->R10;
    pcpuctx->R11 = pctx->R11;
    pcpuctx->R12 = pctx->R12;
    pcpuctx->Sp = pctx->Sp;
    pcpuctx->Lr = pctx->Lr;
    pcpuctx->Pc = pctx->Pc;
#elif defined (MIPS)
    pcpuctx->IntAt = pctx->IntAt;
    pcpuctx->IntV0 = pctx->IntV0;
    pcpuctx->IntV1 = pctx->IntV1;
    pcpuctx->IntA0 = pctx->IntA0;
    pcpuctx->IntA1 = pctx->IntA1;
    pcpuctx->IntA2 = pctx->IntA2;
    pcpuctx->IntA3 = pctx->IntA3;
    pcpuctx->IntT0 = pctx->IntT0;
    pcpuctx->IntT1 = pctx->IntT1;
    pcpuctx->IntT2 = pctx->IntT2;
    pcpuctx->IntT3 = pctx->IntT3;
    pcpuctx->IntT4 = pctx->IntT4;
    pcpuctx->IntT5 = pctx->IntT5;
    pcpuctx->IntT6 = pctx->IntT6;
    pcpuctx->IntT7 = pctx->IntT7;
    pcpuctx->IntS0 = pctx->IntS0;
    pcpuctx->IntS1 = pctx->IntS1;
    pcpuctx->IntS2 = pctx->IntS2;
    pcpuctx->IntS3 = pctx->IntS3;
    pcpuctx->IntS4 = pctx->IntS4;
    pcpuctx->IntS5 = pctx->IntS5;
    pcpuctx->IntS6 = pctx->IntS6;
    pcpuctx->IntS7 = pctx->IntS7;
    pcpuctx->IntT8 = pctx->IntT8;
    pcpuctx->IntT9 = pctx->IntT9;
    pcpuctx->IntK0 = pctx->IntK0;
    pcpuctx->IntK1 = pctx->IntK1;
    pcpuctx->IntGp = pctx->IntGp;
    pcpuctx->IntSp = pctx->IntSp;
    pcpuctx->IntS8 = pctx->IntS8;
    pcpuctx->IntRa = pctx->IntRa;
    pcpuctx->IntLo = pctx->IntLo;
    pcpuctx->IntHi = pctx->IntHi;
    pcpuctx->Fir = pctx->Fir;
    pcpuctx->Psr = pctx->Psr;
#endif /*#if defined (MIPS) */
}

bool OsAxsIsExceptionThread(PTHREAD pth)
{
    return pth == pCurThread;
}

bool OsAxsIsThreadRunning(PTHREAD pth, DWORD *pdwCpuId)
{
    for (DWORD i = 0; i < MAX_CPU; ++i)
    {
        if (g_ppcbs[i] && g_ppcbs[i]->pCurThd == pth)
        {
            if (pdwCpuId)
                *pdwCpuId = g_ppcbs[i]->dwCpuId;
            return true;
        }
    }
    return false;
}


HRESULT OsAxsGetThreadContext(PTHREAD pth, PCONTEXT pctx, size_t cbctx)
{
    HRESULT hr;
    DWORD dwCpuId;

    DEBUGGERMSG(OXZONE_THREAD, (L"++OsAxsGetThreadContext pth%08X, pctx%08X cb%d\r\n", pth, pctx, cbctx));
    if (OsAxsIsThreadRunning(pth, &dwCpuId))
    {
        hr = OsAxsGetCpuContext(dwCpuId, pctx, cbctx);
    }
    else
    {
        // Copy the context out of the thread.
        OsAxsContextFromCpuContext(pctx, &pth->ctx);
        // Non-running threads don't have floating point context available.
        pctx->ContextFlags = CE_CONTEXT_CONTROL | CE_CONTEXT_INTEGER;
        hr = S_OK;
    }
    DEBUGGERMSG(OXZONE_THREAD, (L"--OsAxsGetThreadContext hr=%08x\r\n", hr));
    return hr;
}


HRESULT OsAxsSetThreadContext(PTHREAD pth, PCONTEXT pctx, size_t cbctx)
{
    HRESULT hr;
    DWORD dwCpuId;

    if (OsAxsIsThreadRunning(pth, &dwCpuId))
    {
        hr = OsAxsSetCpuContext(dwCpuId, pctx, cbctx);
    }
    else
    {
        if (pctx->ContextFlags & (CE_CONTEXT_FLOATING_POINT|CE_CONTEXT_DSP))
        {
            DEBUGGERMSG(OXZONE_ALERT, (L"  OsAxsSetThreadContext: Setting a context that has FP and DSP registers?\r\n"));
        }
        // Copy the context into the thread.
        OsAxsCpuContextFromContext(&pth->ctx, pctx);
        hr = S_OK;
    }
    return hr;
}
