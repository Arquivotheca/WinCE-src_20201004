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
/*
-------------------------------------------------------------------------------


Module Name:

    globals.h

Description:

    Declares all global variables and test function prototypes, EXCEPT when
    included by globals.cpp; in that case, it DEFINES global variables,
    including the function table.

-------------------------------------------------------------------------------
*/

#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#ifdef UNDER_NT
#define DEF_PAR_PORT    TEXT("LPT1:")
#else
#define DEF_PAR_PORT    TEXT("LPT1:")
#endif

/*
-------------------------------------------------------------------------------
The following macros allow this header file to be included in all source files,
without the danger of multiply defining global variables. The global variables
will be defined once (when this file is included by globals.cpp); every other
time the variables will be declared as 'extern's.
-------------------------------------------------------------------------------
*/

// ----------------------------------------------------------------------------
// Local Macros
// ----------------------------------------------------------------------------

#ifdef __GLOBALS_CPP__

    #define GLOBAL
    #define INIT(x) = x

#else // __GLOBALS_CPP__

    #define GLOBAL  extern
    #define INIT(x)

#endif // __GLOBALS_CPP__

// ----------------------------------------------------------------------------
// Global Macros
// ----------------------------------------------------------------------------

#define countof(x) (sizeof(x)/sizeof(*(x)))

#define FAIL(x)        g_pKato->Log( LOG_FAIL, TEXT("FAIL in %s @ line %d: %s"), \
                                    __FILE_NAME__, __LINE__, TEXT(x) )                                

extern TCHAR g_driverName[];

// ----------------------------------------------------------------------------
// Global Function Prototypes
// ----------------------------------------------------------------------------

void Debug(LPCTSTR, ...);
SHELLPROCAPI ShellProc(UINT, SPPARAM);

// ----------------------------------------------------------------------------
// TUX Function Table
// ----------------------------------------------------------------------------

#include "ft.h"

// ----------------------------------------------------------------------------
// Globals
// ----------------------------------------------------------------------------

// Global CKato logging object. Set while processing SPM_LOAD_DLL message.
GLOBAL CKato* g_pKato INIT(NULL);

// Global shell info structure. Set while processing SPM_SHELL_INFO message.
GLOBAL SPS_SHELL_INFO* g_pShellInfo INIT(NULL);

// User Argument
class TuxArg {
public:
    TuxArg(LPCTSTR lpCmdLine);
    // attribute.
private:
};

extern class TuxArg * pUserArg;

#endif // __GLOBALS_H__
