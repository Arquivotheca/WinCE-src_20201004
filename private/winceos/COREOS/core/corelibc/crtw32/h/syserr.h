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
*syserr.h - constants/macros for error message routines
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This file contains macros/constants for perror, strerror,
*       and _strerror.
*
*       [Internal]
*
*Revision History:
*       08-15-89  GJF   Fixed copyright
*       10-30-89  GJF   Fixed copyright (again)
*       03-02-90  GJF   Added #ifndef _INC_SYSERR stuff
*       01-22-91  GJF   ANSI naming.
*       01-23-92  GJF   Added support for crtdll.dll (have to redefine
*                       _sys_nerr).
*       10-01-92  GJF   Increased _SYS_MSGMAX.
*       02-23-93  SKS   Update copyright to 1993
*       04-04-93  SKS   Switch to _declspec(dllimport) for exported data/funcs
*       10-12-93  GJF   Merged NT and Cuda versions.
*       02-14-95  CFW   Clean up Mac merge.
*       03-29-95  CFW   Add error message to internal headers.
*       12-14-95  JWM   Add "#pragma once".
*       02-24-97  GJF   Cleaned out obsolete support for _NTSDK. Also,
*                       detab-ed.
*
****/

#pragma once

#ifndef _INC_SYSERR
#define _INC_SYSERR

#include <crtdefs.h>
#include <internal.h>

#ifndef _CRTBLD
/*
 * This is an internal C runtime header file. It is used when building
 * the C runtimes only. It is not to be used as a public header file.
 */
#error ERROR: Use of C runtime library internal header file.
#endif  /* _CRTBLD */

/* Macro for perror, strerror, and _strerror */

#define _sys_err_msg(m) _sys_errlist[(((m)<0)||((m)>=_sys_nerr)?_sys_nerr:(m))]

/* Maximum length of an error message.
   NOTE: This parameter value must be correspond to the length of the longest
   message in sys_errlist (source module syserr.c). */

#define _SYS_MSGMAX 38

__inline
const char *_get_sys_err_msg(int m)
{
_BEGIN_SECURE_CRT_DEPRECATION_DISABLE
    return _sys_err_msg(m);
_END_SECURE_CRT_DEPRECATION_DISABLE
}

#endif  /* _INC_SYSERR */
