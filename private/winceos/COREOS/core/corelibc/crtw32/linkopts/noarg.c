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
/***
*noarg.c - stub out CRT's processing of command line arguments
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Stub out the processing of the command line into argv[], normally
*       carried out at during startup. Note, the argc and argv arguments to
*       main are not meaningful if this object is used. Nor are __argc and
*       __argv.
*
*Revision History:
*       05-05-97  GJF   Created.
*       06-30-97  GJF   Added stubs for _[w]wincmdln().
*       03-27-01  PML   _[w]setargv now returns an int (vs7#231220)
*
*******************************************************************************/

#include <tchar.h>

#ifdef _M_CEE_PURE
#define _CALLING __clrcall
#else
#define _CALLING __cdecl
#endif

int _CALLING _setargv() { return 0; }

int _CALLING _wsetargv() { return 0; }

_TUCHAR * _CALLING _wincmdln() { return NULL; }

_TUCHAR * _CALLING _wwincmdln() { return NULL; }
