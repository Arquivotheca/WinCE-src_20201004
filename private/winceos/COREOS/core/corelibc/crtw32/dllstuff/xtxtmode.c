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
*xtxtmode.c - set global text mode flag
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Sets the global file mode to text.  This is the default.
*
*       This is a special version for the DLL model.  This object goes into
*       MSVCRT.LIB (and therefore into the client EXE) and not into the
*       CRTL in a DLL (MSVCRTXX.DLL).  It is identical to txtmode.obj
*       except that the latter has a DLL export definition in the DLL model.
*
*Revision History:
*       06-08-89  PHG   Module created, based on asm version.
*       04-04-90  GJF   Added #include <cruntime.h>. Also, fixed the copyright.
*       01-23-92  GJF   Added #include <stdlib.h> (contains decl of _fmode).
*       04-05-94  GJF   Added conditional so this definition doesn't make it
*                       into the msvcrt*.dll for Win32s.
*       05-15-95  SKS   This is a copy of txtmode.obj except for the
*                       DLL import semantics.
*       06-28-96  GJF   Removed DLL_FOR_WIN32S. Also, detab-ed.
*
*******************************************************************************/

#define SPECIAL_CRTEXE  /* turn off _CRTIMP for decl. of _fmode */

#include <cruntime.h>
#include <stdlib.h>

int _fmode = 0;                 /* set text mode */
