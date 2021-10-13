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

#ifndef _APPVERIFIER_H_
#define _APPVERIFIER_H_

#include <windows.h>
#include "Loader.h"

void
AppVerifierInit (
    LPVOID      pfn,
    LPCWSTR     pszProgName,
    LPCWSTR     pszCmdLine,
    HINSTANCE   hCoreDll
    );
void
AppVerifierDoLoadLibrary (
    PUSERMODULELIST pMod,
    LPCWSTR pszFileName
    );
BOOL
AppVerifierDoImports (
    PUSERMODULELIST pModCalling,
    DWORD  dwBaseAddr,
    PCInfo pImpInfo,
    BOOL fRecursive
    );
void
AppVerifierUnDoDepends (
    PUSERMODULELIST pMod
    );
void
AppVerifierDoNotify (
    PUSERMODULELIST pMod,
    DWORD dwReason
    );

void
AppVerifierProcessDetach (
    void
    );
void
AppVerifierUnlockLoader (
    void
    );

#endif // _APPVERIFIER_H_

