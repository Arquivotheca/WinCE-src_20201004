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
*rtti.h - prototypes of CRT entry points for run-time type information routines.
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       RTTI header.
*
*       [Internal]
*
*Revision History:
*       09-26-94  JWM   Module created (prototypes only).
*       10-03-94  JWM   Made all prototypes 'extern "C"'
*       02-14-95  CFW   Clean up Mac merge.
*       03-29-95  CFW   Add error message to internal headers.
*       12-14-95  JWM   Add "#pragma once".
*       02-24-97  GJF   Detab-ed.
*       04-21-00  PML   Add throw(...) exception specifications.
*       04-07-04  MSL   Double slash removal
*
****/

#pragma once

#ifndef _INC_RTTI
#define _INC_RTTI

#ifndef _CRTBLD
/*
 * This is an internal C runtime header file. It is used when building
 * the C runtimes only. It is not to be used as a public header file.
 */
#error ERROR: Use of C runtime library internal header file.
#endif  /* _CRTBLD */

#define _RTTI 1         /* needed by ehdata.h */

#include <ehdata.h>
#include <rttidata.h>

typedef TypeDescriptor _RTTITypeDescriptor;

#pragma warning(disable: 4290)

#ifdef  _ACTUAL_PARAMS
extern "C" PVOID __CLRCALL_OR_CDECL __RTDynamicCast (
    PVOID *,                /* ptr to src object */
    LONG,                   /* offset of vfptr in src object */
    _RTTITypeDescriptor *,  /* src type */
    _RTTITypeDescriptor *,  /* target type */
    BOOL) throw(...);       /* isReference */


extern "C" _RTTITypeDescriptor * __CLRCALL_OR_CDECL __RTtypeid (PVOID *) throw(...);  /* ptr to src object */


extern "C" PVOID __CLRCALL_OR_CDECL __RTCastToVoid (PVOID *) throw(...);   /* ptr to src object */

#else

extern "C" PVOID __CLRCALL_OR_CDECL __RTDynamicCast (
    PVOID,                  /* ptr to vfptr */
    LONG,                   /* offset of vftable */
    PVOID,                  /* src type */
    PVOID,                  /* target type */
    BOOL) throw(...);       /* isReference */

extern "C" PVOID __CLRCALL_OR_CDECL __RTtypeid (PVOID) throw(...);     /* ptr to vfptr */

extern "C" PVOID __CLRCALL_OR_CDECL __RTCastToVoid (PVOID) throw(...); /* ptr to vfptr */


#endif

#define TYPEIDS_EQ(pID1, pID2)  ((pID1 == pID2) || !strcmp(pID1->name, pID2->name))

#endif  /* _INC_RTTI */

