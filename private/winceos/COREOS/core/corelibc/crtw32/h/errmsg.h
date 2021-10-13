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
*errmsg.h - defines error message numbers
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This file contains the constants for error message numbers.
*       Same as errmsg.inc
*
*       [Internal]
*
*Revision History:
*       08-03-89  GJF   Fixed copyright
*       10-30-89  GJF   Fixed copyright
*       02-28-90  GJF   Added #ifndef _INC_ERRMSG stuff
*       02-14-95  CFW   Clean up Mac merge.
*       03-29-95  CFW   Add error message to internal headers.
*       12-14-95  JWM   Add "#pragma once".
*       02-24-97  GJF   Detab-ed.
*
****/

#pragma once

#ifndef _INC_ERRMSG

#ifndef _CRTBLD
/*
 * This is an internal C runtime header file. It is used when building
 * the C runtimes only. It is not to be used as a public header file.
 */
#error ERROR: Use of C runtime library internal header file.
#endif  /* _CRTBLD */

#define STCKOVR 0
#define NULLERR 1
#define NOFP    2
#define DIVZR   3
#define BADVERS 4
#define NOMEM   5
#define BADFORM 6
#define BADENV  7
#define NOARGV  8
#define NOENVP  9
#define ABNORM  10
#define UNKNOWN 11

#define CRT_NERR 11

#define _INC_ERRMSG
#endif  /* _INC_ERRMSG */
