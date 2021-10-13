//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

