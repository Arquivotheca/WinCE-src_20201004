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
*rterr.h - runtime errors
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This file defines the C runtime errors
*
*       [Internal]
*
*Revision History:
*       06-01-90  GJF   Module created.
*       08-08-90  GJF   Added _RT_CONIO, redefined _RT_NONCONT and
*                       _RT_INVALDISP.
*       09-08-91  GJF   Added _RT_ONEXIT for Win32 (_WIN32_).
*       09-28-91  GJF   Fixed conflict with RTEs in 16-bit Win support. Also,
*                       added three math errors.
*       10-23-92  GJF   Added _RT_PUREVIRT.
*       02-23-93  SKS   Update copyright to 1993
*       02-14-95  CFW   Clean up Mac merge.
*       03-03-95  GJF   Added _RT_STDIOINIT.
*       03-29-95  CFW   Add error message to internal headers.
*       06-02-95  GJF   Added _RT_LOWIOINIT.
*       12-14-95  JWM   Add "#pragma once".
*       04-23-96  GJF   Added _RT_HEAPINIT.
*       02-24-97  GJF   Detab-ed.
*       09-30-02  GB    Added error message for unitialized crt calls.
*       05-10-04  GB    Added error message for conflict in order of
*                       initializtion.
*       10-09-04  MSL   Added error message for locale allocation
*       06-10-05  AC    Added error message for manifest check.
*       06-10-05  PML   Added _RT_COOKIE_INIT
*       06-17-08  GM    Removed Fusion.
*
****/

#pragma once

#ifndef _INC_RTERR
#define _INC_RTERR

#ifndef _CRTBLD
/*
 * This is an internal C runtime header file. It is used when building
 * the C runtimes only. It is not to be used as a public header file.
 */
#error ERROR: Use of C runtime library internal header file.
#endif  /* _CRTBLD */

#define _RT_STACK				0               /* stack overflow */
#define _RT_NULLPTR				1               /* null pointer assignment */
#define _RT_FLOAT				2               /* floating point not loaded */
#define _RT_INTDIV				3               /* integer divide by 0 */

/*
 * the following three errors must be in the given order!
 */
#define _RT_EXECMEM				5               /* not enough memory on exec */
#define _RT_EXECFORM			6               /* bad format on exec */
#define _RT_EXECENV				7               /* bad environment on exec */

#define _RT_SPACEARG			8               /* not enough space for arguments */
#define _RT_SPACEENV			9               /* not enough space for environment */
#define _RT_ABORT				10              /* Abnormal program termination */

#define _RT_NPTR				12              /* illegal near pointer use */
#define _RT_FPTR				13              /* illegal far pointer use */
#define _RT_BREAK				14              /* control-BREAK encountered */
#define _RT_INT					15              /* unexpected interrupt */
#define _RT_THREAD				16              /* not enough space for thread data */
#define _RT_LOCK				17              /* unexpected multi-thread lock error */
#define _RT_HEAP				18              /* unexpected heap error */
#define _RT_OPENCON				19              /* unable to open console device */

/*
 * _RT_QWIN and _RT_NOMAIN are used in 16-bit Windows support
 */
#define _RT_QWIN				20              /* unexpected QuickWin error */
#define _RT_NOMAIN				21              /* no main procedure */


#define _RT_NONCONT				22              /* non-continuable exception */
#define _RT_INVALDISP			23              /* invalid disposition of exception */


/*
 * _RT_ONEXIT is specific to Win32 and Dosx32 platforms
 */
#define _RT_ONEXIT				24              /* insufficient heap to allocate
                                             * initial table of funct. ptrs
                                             * used by _onexit()/atexit(). */

#define _RT_PUREVIRT			25              /* pure virtual function call attempted
                                             * (C++ error) */

#define _RT_STDIOINIT			26              /* not enough space for stdio initial-
                                             * ization */
#define _RT_LOWIOINIT			27              /* not enough space for lowio initial-
                                             * ization */
#define _RT_HEAPINIT			28              /* heap failed to initialize */
#define _RT_BADCLRVERSION		29              /* Application appdomain setting incompatible with CLR */

#define _RT_CRT_NOTINIT			30              /* CRT is not initialized */

#define _RT_CRT_INIT_CONFLICT	31              /* global initialization order conflict */

#define _RT_LOCALE				32              /* lack of space for locale */

#define _RT_CRT_INIT_MANAGED_CONFLICT     33      /* global initialization order conflict */

#define _RT_ONEXIT_VAR                    34      /* Onexit begin and end variables inconsistency */

/*
 * _RT_COOKIE_INIT is not valid for _NMSG_WRITE, _RT_COOKIE_INIT_TXT is passed
 * directly to FatalAppExit in __security_init_cookie.
 */
#define _RT_COOKIE_INIT         35              /* __security_init_cookie called too late */

/*
 * _RT_DOMAIN, _RT_SING and _RT_TLOSS are generated by the floating point
 * library.
 */
#define _RT_DOMAIN				120
#define _RT_SING				121
#define _RT_TLOSS				122

#define _RT_CRNL				252
#define _RT_BANNER				255

#endif  /* _INC_RTERR */
