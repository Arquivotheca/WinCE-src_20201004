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
--*/

#include "kdp.h"



void CpuContextToContext(CONTEXT *pCtx, CPUCONTEXT *pCpuCtx)
{
    
    *pCtx = *pCpuCtx;
    pCtx->ContextFlags = CONTEXT_FULL;
}

