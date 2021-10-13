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
//------------------------------------------------------------------------------
//
//  waveapicstub.cpp
//
//  Copyright (c) 1991-2000 Microsoft Corporation
//
//  Description:
//      This file provides various stubs needed by waveapi that might not be available
//
//------------------------------------------------------------------------------

#include <waveproxy.h>

// Various stub functions which need to be available even if ACM not sysgen'd in.
CWaveProxy *CreateWaveProxyACM(
    DWORD dwCallback,
    DWORD dwInstance,
    DWORD fdwOpen,
    BOOL bIsOutput,
    CWaveLib *pCWaveLib
    )
{
    return NULL;
}

BOOL acm_Initialize(VOID)
{
    return TRUE;
}

BOOL acm_Terminate(VOID)
{
    return TRUE;
}

