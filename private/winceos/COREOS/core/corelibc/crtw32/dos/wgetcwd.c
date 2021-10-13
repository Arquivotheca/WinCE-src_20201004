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
*wgetcwd.c - get current working directory (wchar_t version)
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*
*	contains functions _getcwd, _getdcwd and _getcdrv for getting the
*	current working directory.  getcwd gets the c.w.d. for the default disk
*	drive, whereas _getdcwd allows one to get the c.w.d. for whatever disk
*	drive is specified. _getcdrv gets the current drive.
*
*Revision History:
*	10-29-93  CFW	Module created.
*       02-07-94  CFW   POSIXify.
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

#include "getcwd.c"

#endif /* _POSIX_ */
