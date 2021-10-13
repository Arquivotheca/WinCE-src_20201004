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
//  init.c
//
//  Copyright (c) 1991-2000 Microsoft Corporation
//
//  Description:
//
//
//  History:
//      11/15/92    cjp     [curtisp]
//
//------------------------------------------------------------------------------
#include "acmi.h"
#include "wavelib.h"

PACMGARB    pag;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL
acm_Terminate(VOID)
{
    FUNC("acm_Terminate");

    acmFreeDrivers();
    LocalFree(pag);
    pag = NULL;

    FUNC_EXIT();
    return (TRUE);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL
acm_Initialize(VOID)
{
    FUNC("acm_Initialize");

    pag = (PACMGARB) LocalAlloc(LPTR, sizeof(ACMGARB));

    pag->cUsage             = 1;
    pag->hinst              = g_hInstance;
    pag->fDriversBooted     = FALSE;

    FUNC_EXIT();
    return (TRUE);
}


