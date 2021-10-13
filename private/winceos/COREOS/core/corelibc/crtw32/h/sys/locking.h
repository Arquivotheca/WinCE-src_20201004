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
*sys/locking.h - flags for locking() function
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This file defines the flags for the locking() function.
*       [System V]
*
*       [Public]
*
*Revision History:
*       08-22-89  GJF   Fixed copyright
*       10-30-89  GJF   Fixed copyright (again)
*       03-21-90  GJF   Added #ifndef _INC_LOCKING stuff
*       01-21-91  GJF   ANSI naming.
*       09-16-92  SKS   Fix copyright, clean up backslash
*       02-23-93  SKS   Update copyright to 1993
*       12-28-94  JCF   Merged with mac header.
*       02-14-95  CFW   Clean up Mac merge, add _CRTBLD.
*       04-27-95  CFW   Add mac/win32 test.
*       12-14-95  JWM   Add "#pragma once".
*       05-17-99  PML   Remove all Macintosh support.
*
****/

#pragma once

#ifndef _INC_LOCKING
#define _INC_LOCKING

#if !defined(_WIN32)
#error ERROR: Only Win32 target supported!
#endif

#ifndef _CRTBLD
/* This version of the header files is NOT for user programs.
 * It is intended for use when building the C runtimes ONLY.
 * The version intended for public use will not have this message.
 */
#error ERROR: Use of C runtime library internal header file.
#endif /* _CRTBLD */

#define _LK_UNLCK       0       /* unlock the file region */
#define _LK_LOCK        1       /* lock the file region */
#define _LK_NBLCK       2       /* non-blocking lock */
#define _LK_RLCK        3       /* lock for writing */
#define _LK_NBRLCK      4       /* non-blocking lock for writing */

#if !__STDC__
/* Non-ANSI names for compatibility */
#define LK_UNLCK        _LK_UNLCK
#define LK_LOCK         _LK_LOCK
#define LK_NBLCK        _LK_NBLCK
#define LK_RLCK         _LK_RLCK
#define LK_NBRLCK       _LK_NBRLCK
#endif

#endif  /* _INC_LOCKING */
