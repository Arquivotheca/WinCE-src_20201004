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
*stat.inl - inline definitions for low-level file handling and I/O functions
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This file contains the function definitions for the low-level
*       file handling and I/O functions.
*
*       [Public]
*
*Revision History:
*       02/04/03  EAN   Created
*
****/

#pragma once

#if !defined(__CRTDECL)
#if defined(_M_CEE_PURE)
#define __CRTDECL
#else
#define __CRTDECL   __cdecl
#endif
#endif

#ifndef _INC_STAT_INL
#define _INC_STAT_INL

/* _STATIC_ASSERT is for enforcing boolean/integral conditions at compile time.
   Since it is purely a compile-time mechanism that generates no code, the check
   is left in even if _DEBUG is not defined. */

#ifndef _STATIC_ASSERT
#define _STATIC_ASSERT(expr) typedef char __static_assert_t[ (expr) ]
#endif

#if     !__STDC__

/* Non-ANSI names for compatibility */

#ifdef _USE_32BIT_TIME_T
static __inline int __CRTDECL fstat(int _Desc, struct stat * _Stat)
{
    _STATIC_ASSERT( sizeof(struct stat) == sizeof(struct _stat32) );
    return _fstat32(_Desc,(struct _stat32 *)_Stat);
}
static __inline int __CRTDECL stat(const char * _Filename, struct stat * _Stat)
{
    _STATIC_ASSERT( sizeof(struct stat) == sizeof(struct _stat32) );
    return _stat32(_Filename,(struct _stat32 *)_Stat);
}
#else
static __inline int __CRTDECL fstat(int _Desc, struct stat * _Stat)
{
    _STATIC_ASSERT( sizeof(struct stat) == sizeof(struct _stat64i32) );
    return _fstat64i32(_Desc,(struct _stat64i32 *)_Stat);
}
static __inline int __CRTDECL stat(const char * _Filename, struct stat * _Stat)
{
    _STATIC_ASSERT( sizeof(struct stat) == sizeof(struct _stat64i32) );
    return _stat64i32(_Filename,(struct _stat64i32 *)_Stat);
}
#endif /* _USE_32BIT_TIME_T */
#endif /* !__STDC__ */

#endif
