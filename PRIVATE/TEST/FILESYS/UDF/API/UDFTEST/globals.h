//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#include <windows.h>
#include <tchar.h>
#include <katoex.h>
#include <tux.h>
#include <pathtable.h>
#include "util.h"

#ifndef UNDER_CE
#include <stdio.h>
#define ASSERT(x) 
#endif

//******************************************************************************
//***** Windows CE specific definitions
//******************************************************************************
#ifdef UNDER_CE

#ifndef STARTF_USESIZE
#define STARTF_USESIZE     0x00000002
#endif

#ifndef STARTF_USEPOSITION
#define STARTF_USEPOSITION 0x00000004
#endif

#ifndef ZeroMemory
#define ZeroMemory(Destination,Length) memset(Destination, 0, Length)
#endif

#ifndef _vsntprintf
#define _vsntprintf(d,c,f,a) wvsprintf(d,f,a)
#endif

#endif // UNDER_CE

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
extern CKato *g_pKato;

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
extern SPS_SHELL_INFO *g_pShellInfo;

// Global critical section to be used by threaded tests if necessary.
extern CRITICAL_SECTION g_csProcess;

// Global command line option flags
extern BOOL g_fCheckData;

// Global path to disc
extern TCHAR g_szDiscPath[MAX_PATH];

// Global PathTable
extern PathTable *g_pPathTable;

#endif // __GLOBALS_H__
