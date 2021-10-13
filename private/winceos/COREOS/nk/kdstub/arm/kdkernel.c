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

    kdkernel.c

Abstract:

    This module contains code that somewhat emulates KiDispatchException

--*/

#include "kdp.h"

void CpuContextToContext(CONTEXT *pCtx, CPUCONTEXT *pCpuCtx)
{
    pCtx->R0 = pCpuCtx->R0;
    pCtx->R1 = pCpuCtx->R1;
    pCtx->R2 = pCpuCtx->R2;
    pCtx->R3 = pCpuCtx->R3;
    pCtx->R4 = pCpuCtx->R4;
    pCtx->R5 = pCpuCtx->R5;
    pCtx->R6 = pCpuCtx->R6;
    pCtx->R7 = pCpuCtx->R7;
    pCtx->R8 = pCpuCtx->R8;
    pCtx->R9 = pCpuCtx->R9;
    pCtx->R10 = pCpuCtx->R10;
    pCtx->R11 = pCpuCtx->R11;
    pCtx->R12 = pCpuCtx->R12;
    pCtx->Sp = pCpuCtx->Sp;
    pCtx->Lr = pCpuCtx->Lr;
    pCtx->Pc = pCpuCtx->Pc;
    pCtx->Psr = pCpuCtx->Psr;
    pCtx->ContextFlags = CONTEXT_FULL;
}


// An escape mechanism to allow an user to override the autodetection below
BOOL g_fOverrideWMMXDetection = FALSE;
BOOL g_fWMMXPresent = FALSE;

BOOL HasWMMX()
{
    BOOL fRet;
    fRet = FALSE;
    
    if(g_fOverrideWMMXDetection)
        fRet = g_fWMMXPresent;
    else
        fRet = NKIsProcessorFeaturePresent(PF_ARM_INTEL_WMMX);

    DEBUGGERMSG(KDZONE_INTELWMMX, (L"--HasWMMX: %d\r\n", fRet));
    return fRet;
}
