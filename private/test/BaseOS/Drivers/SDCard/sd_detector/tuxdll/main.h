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
#pragma once

#pragma once

#include <windows.h>
#include <tchar.h>
#include <katoex.h>
#include <tux.h>
#include "sddetect.h"

/*
    Prototypes:
            Prototypes for each of the individual unit tests
*/

TESTPROCAPI DetectSD(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI DetectSDHC(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI DetectMMC(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI DetectMMCHC(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI DetectSDIO(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

/*
    assorted random info below.
    probably no need to touch this for most cases.
*/

// NT ASSERT macro
#ifdef UNDER_NT
#include <crtdbg.h>
#define ASSERT _ASSERT
#define HFILE FILE*
#define Debug
#endif

// define macro
#define IGNORE_NONEXECUTE_CMDS(uMsg)  { if( (uMsg) != TPM_EXECUTE) return TPR_NOT_HANDLED; }

#define MODULE_NAME _T("sd_detector")

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
extern CKato *g_pKato;

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
extern SPS_SHELL_INFO *g_pShellInfo;

