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
--*/

#include "kdp.h"


void CpuContextToContext(CONTEXT *pCtx, CPUCONTEXT *pCpuCtx)
{
    pCtx->SegGs     =pCpuCtx->TcxGs;
    pCtx->SegFs     =pCpuCtx->TcxFs;
    pCtx->SegEs     =pCpuCtx->TcxEs;
    pCtx->SegDs     =pCpuCtx->TcxDs;
    pCtx->Edi       =pCpuCtx->TcxEdi;
    pCtx->Esi       =pCpuCtx->TcxEsi;
    pCtx->Ebp       =pCpuCtx->TcxEbp;
//  pCtx->Esp       =pCpuCtx->TcxNotEsp;    ???
    pCtx->Ebx       =pCpuCtx->TcxEbx;
    pCtx->Edx       =pCpuCtx->TcxEdx;
    pCtx->Ecx       =pCpuCtx->TcxEcx;
    pCtx->Eax       =pCpuCtx->TcxEax;
    pCtx->Eip       =pCpuCtx->TcxEip;
    pCtx->SegCs     =pCpuCtx->TcxCs;
    pCtx->EFlags    =pCpuCtx->TcxEFlags;
    pCtx->Esp       =pCpuCtx->TcxEsp;
    pCtx->SegSs     =pCpuCtx->TcxSs;
//  FLOATING_SAVE_AREA TcxFPU;
}   
