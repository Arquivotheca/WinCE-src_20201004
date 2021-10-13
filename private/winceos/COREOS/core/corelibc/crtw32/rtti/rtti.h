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
*
*Purpose:
*       RTTI header.
*
*       [Internal]
*
****/

#if     _MSC_VER > 1000
#pragma once
#endif

#ifndef _INC_RTTI
#define _INC_RTTI

#define _RTTI 1         // needed by ehdata.h

#include <ehdata.h>
#include <rttidata.h>

typedef TypeDescriptor _RTTITypeDescriptor;

#ifdef  _ACTUAL_PARAMS
extern "C" PVOID __cdecl __RTDynamicCast (PVOID *,                                      // ptr to src object
                                                                LONG,                                   // offset of vfptr in src object
                                                                _RTTITypeDescriptor *,  // src type
                                                                _RTTITypeDescriptor *,  // target type
                                                                BOOL);                                  // isReference


extern "C" _RTTITypeDescriptor * __cdecl __RTtypeid (PVOID *);          // ptr to src object


extern "C" PVOID __cdecl __RTCastToVoid (PVOID *);                                      // ptr to src object

#else

extern "C" PVOID __cdecl __RTDynamicCast (
                                                                PVOID,                          // ptr to vfptr
                                                                LONG,                           // offset of vftable
                                                                PVOID,                          // src type
                                                                PVOID,                          // target type
                                                                BOOL);                          // isReference

extern "C" PVOID __cdecl __RTtypeid (PVOID);            // ptr to vfptr

extern "C" PVOID __cdecl __RTCastToVoid (PVOID);        // ptr to vfptr


#endif

#define TYPEIDS_EQ(pID1, pID2)  ((pID1 == pID2) || !strcmp(pID1->name, pID2->name))

#endif  /* _INC_RTTI */

