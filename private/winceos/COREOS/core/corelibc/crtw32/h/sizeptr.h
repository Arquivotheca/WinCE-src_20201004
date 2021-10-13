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
*sizeptr.h - defines constants based on memory model
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       This file defines the constants SIZEC, SIZED, DIST, BDIST based
*       on the current memory model.
*       SIZEC is for far code models (medium, large).
*       SIZED is for large data models (compact, large).
*
*       [Internal]
*
*Revision History:
*       08-15-89  GJF   Fixed copyright, changed far to _far, near to _near
*       10-30-89  GJF   Fixed copyright (again)
*       02-14-95  CFW   Clean up Mac merge.
*       03-29-95  CFW   Add error message to internal headers.
*       12-14-95  JWM   Add "#pragma once".
*       02-24-97  GJF   Detab-ed.
*
****/

#pragma once

#ifndef _INC_SIZEPTR
#define _INC_SIZEPTR

#ifndef _CRTBLD
/*
 * This is an internal C runtime header file. It is used when building
 * the C runtimes only. It is not to be used as a public header file.
 */
#error ERROR: Use of C runtime library internal header file.
#endif /* _CRTBLD */

#ifdef  M_I86MM
#undef  SIZED
#define SIZEC
#endif

#ifdef  M_I86CM
#undef  SIZEC
#define SIZED
#endif

#ifdef  M_I86LM
#define SIZEC
#define SIZED
#endif

#ifdef  SS_NE_DS
#define SIZED
#endif

#ifdef  SIZED
#define DIST _far
#define BDIST _near     /*bizzare distance*/
#else
#define DIST _near
#define BDIST _far      /*bizzare distance*/
#endif

#endif  /* _INC_SIZEPTR */
