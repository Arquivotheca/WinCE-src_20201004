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
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// ----------------------------------------------------------------------------
//
// TuxMain definitions.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_TuxMain_
#define _DEFINED_TuxMain_
#pragma once

#ifndef DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <windows.h>
#include <katoex.h>
#include <tux.h>

// Global shell info structure:
extern SPS_SHELL_INFO *g_pShellInfo;

// Indicates the DLL has been run by the CETK in a "offline" state:
// Tells us we need to restart the CETK connection when we're finished.
extern bool g_fCETKOffline;
    
// Command line arguments:
//
extern int    argc;
extern TCHAR *argv[];

#endif /* _DEFINED_TuxMain_ */
// ----------------------------------------------------------------------------
