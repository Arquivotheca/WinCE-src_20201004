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


Module Name: patcher.h

++*/

#ifndef PATCHER_H
#define PATCHER_H


/* Defines used to generate API calls to the ROM Patcher system handle */
#define PATCHER_CALL(type, api, args) \
    IMPLICIT_DECL(type, SH_PATCHER, api, args)

#define PatchExe PATCHER_CALL(BOOL, 2, (PPROCESS pproc, LPCWSTR lpwsName))
#define PatchDll PATCHER_CALL(BOOL, 3, (PPROCESS pproc, PMODULE pmod, LPCWSTR lpwsName))
#define FreeDllPatch PATCHER_CALL(void, 4, (PPROCESS pproc, PMODULE pmod))

#endif

