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
--*/

#include "kdp.h"


void ThreadStopFunc(void)
{
    _asm int 2
}


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
