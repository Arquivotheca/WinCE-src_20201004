//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

