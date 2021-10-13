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

