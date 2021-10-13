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
*wdllargv.c - Dummy _wsetargv() routine for use with C Run-Time as a DLL (CRTDLL)
*             (wchar_t version)
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	This object goes into CRTDLL.LIB, which is linked with user programs
*	to use CRTDLL.DLL for C run-time library functions.  If the user
*	program links explicitly with WSETARGV.OBJ, this object will not be
*	linked in, and the _wsetargv() that does get called with set the flag
*	that will enable wildcard expansion.  If WSETARGV.OBJ is not linked
*	into the EXE, this object will get called by the CRT start-up stub
*	and the flag to enable wildcard expansion will not be set.
*
*Revision History:
*	11-24-93  CFW	Module created.
*	02-07-94  CFW	POSIXify.
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

#include "dllargv.c"

#endif /* _POSIX_ */
