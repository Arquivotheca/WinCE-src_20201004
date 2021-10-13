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
#define BUILDING_USER_STUBS

#include <windows.h>

#include <GweApiSet1.hpp>

extern  GweApiSet1_t*    pGweApiSet1Entrypoints;

extern "C"
BOOL
xxx_SystemParametersInfo_GWE(
    UINT    uiAction,
    UINT    uiParam,
    PVOID   pvParam,
    UINT    fWinIni
    )
{
    return pGweApiSet1Entrypoints->m_pSystemParametersInfoW(uiAction, uiParam, pvParam, fWinIni);
}

