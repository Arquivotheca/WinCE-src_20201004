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

    kdkernel.c

Abstract:

    This module contains code that somewhat emulates KiDispatchException

--*/

#include "kdp.h"


void CpuContextToContext(CONTEXT *pCtx, CPUCONTEXT *pCpuCtx)
{
    memset(pCtx, 0, sizeof(CONTEXT));

    pCtx->IntZero = 0;
    pCtx->IntAt = pCpuCtx->IntAt;
    pCtx->IntV0 = pCpuCtx->IntV0;
    pCtx->IntV1 = pCpuCtx->IntV1;
    pCtx->IntA0 = pCpuCtx->IntA0;
    pCtx->IntA1 = pCpuCtx->IntA1;
    pCtx->IntA2 = pCpuCtx->IntA2;
    pCtx->IntA3 = pCpuCtx->IntA3;
    pCtx->IntT0 = pCpuCtx->IntT0;
    pCtx->IntT1 = pCpuCtx->IntT1;
    pCtx->IntT2 = pCpuCtx->IntT2;
    pCtx->IntT3 = pCpuCtx->IntT3;
    pCtx->IntT4 = pCpuCtx->IntT4;
    pCtx->IntT5 = pCpuCtx->IntT5;
    pCtx->IntT6 = pCpuCtx->IntT6;
    pCtx->IntT7 = pCpuCtx->IntT7;
    pCtx->IntS0 = pCpuCtx->IntS0;
    pCtx->IntS1 = pCpuCtx->IntS1;
    pCtx->IntS2 = pCpuCtx->IntS2;
    pCtx->IntS3 = pCpuCtx->IntS3;
    pCtx->IntS4 = pCpuCtx->IntS4;
    pCtx->IntS5 = pCpuCtx->IntS5;
    pCtx->IntS6 = pCpuCtx->IntS6;
    pCtx->IntS7 = pCpuCtx->IntS7;
    pCtx->IntT8 = pCpuCtx->IntT8;
    pCtx->IntT9 = pCpuCtx->IntT9;
    pCtx->IntK0 = pCpuCtx->IntK0;
    pCtx->IntK1 = pCpuCtx->IntK1;
    pCtx->IntGp = pCpuCtx->IntGp;
    pCtx->IntSp = pCpuCtx->IntSp;
    pCtx->IntS8 = pCpuCtx->IntS8;
    pCtx->IntRa = pCpuCtx->IntRa;
    pCtx->IntLo = pCpuCtx->IntLo;
    pCtx->IntHi = pCpuCtx->IntHi;

    pCtx->Fsr = pCpuCtx->Fsr;

    pCtx->Fir = pCpuCtx->Fir;
    pCtx->Psr = pCpuCtx->Psr;


    pCtx->ContextFlags = pCpuCtx->ContextFlags;

}

