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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
/*++


Module Name:

    dbg.h

Abstract:

    dbg module header file

Environment:

    WinCE

--*/

#ifndef _DBGH_
#define _DBGH_


#include "kernel.h"


extern PROCESS *g_pFocusProcOverride;


void kdbgWtoA (LPCWSTR pWstr, LPCHAR pAstr);
void kdbgWtoAmax (LPCWSTR pWstr, LPCHAR pAstr, int max);
DWORD kdbgwcslen (LPCWSTR pWstr);


BOOL
ProcessAdvancedCmd(
    IN PSTRING pParamBuf
    );


void *
MapToDebuggeeCtxKernEquivIfAcc(
    PPROCESS pVM,
    BYTE * pbAddr,
    BOOL fProbeOnly
    );

ULONG
Kdstrtoul(
    const BYTE *lpStr,
    ULONG radix
    );


void
SetKernLoadAddress(
    void
    );

LPCHAR
GetWin32ExeName(
    PPROCESS pProc
    );

#endif // _DBGH_

