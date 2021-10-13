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
#include <windev.h>
#define _OBJBASE_H_
#define __objidl_h__
#include "imm.h"
#include "window.hpp"   // gwes
#include "dlgmgr.h"

#include <CePtr.hpp>
#include <GweApiSet1.hpp>

extern  GweApiSet1_t*    pGweApiSet1Entrypoints;

/*
    @comm WNDCLASS structure is restricted as follows:

    @comm The following fields are supported:
    @comm <p lpfnWndProc>
    @comm <p cbClsExtra> must be a multiple of 4
    @comm <p cbWndExtra> must be a multiple of 4
    @comm <p hInstance>
    @comm <p hbrBackground>
    @comm <p lpszClassName> must be a string.  Atoms are not supported.

    @comm The following fields are unsupported and must be NULL.
    @comm <p style>
    @comm <p hCursor>
    @comm <p lpszMenuName>

*/
// work around bug in c11 x86 compiler which generates bad code for this
// function call
#ifdef x86
#pragma optimize("", off)
#endif

extern "C"
ATOM
WINAPI
RegisterClassW(
    __in CONST   WNDCLASSW*  pwndcls
    )
{
    if ( !pwndcls )
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
        }

    WNDCLASSW   WndClass = *pwndcls;

    return pGweApiSet1Entrypoints->m_pRegisterClassW(&WndClass, sizeof(WNDCLASSW));
}

#ifdef x86
#pragma optimize("", on)
#endif

