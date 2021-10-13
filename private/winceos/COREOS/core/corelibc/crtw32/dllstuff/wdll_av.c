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
*wdll_av.c - __wsetargv() routine for use with C Run-Time as a DLL (CRTDLL)
*            (wchar_t version)
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	This object is part of the start-up code for EXE's linked with
*	CRTDLL.LIB/MSVCRT.LIB.  This object will be linked into the user
*	EXE if and only if the user explicitly links with WSETARGV.OBJ.
*	The code in this object sets the flag that is passed to the
*	C Run-Time DLL to enable wildcard expansion of the argv[] vector.
*
*Revision History:
*       08-27-99  PML   Module created.
*
*******************************************************************************/

#ifndef _POSIX_

#define WPRFLAG 1

#ifndef _UNICODE   /* CRT flag */
#define _UNICODE 1
#endif

#ifndef UNICODE	   /* NT flag */
#define UNICODE 1
#endif

#undef _MBCS /* UNICODE not _MBCS */

#include "dll_argv.c"

#endif /* _POSIX_ */
