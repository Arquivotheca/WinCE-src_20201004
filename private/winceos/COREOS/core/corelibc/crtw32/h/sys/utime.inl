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
*utime.inl - inline definitions for time handling functions
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This file contains the definition of the _utime() function.
*
*       [Public]
*
*Revision History:
*       02/04/03  EAN   Created
*       11-11-04  AGH   Don't use utime32 and struct utimbuf
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

#ifndef _INC_UTIME_INL
#define _INC_UTIME_INL

/* _STATIC_ASSERT is for enforcing boolean/integral conditions at compile time.
   Since it is purely a compile-time mechanism that generates no code, the check
   is left in even if _DEBUG is not defined. */

#ifndef _STATIC_ASSERT
#define _STATIC_ASSERT(expr) typedef char __static_assert_t[ (expr) ]
#endif

#ifdef _USE_32BIT_TIME_T
static __inline int __CRTDECL _utime(const char * _Filename, struct _utimbuf * _Utimbuf)
{
    _STATIC_ASSERT( sizeof(struct _utimbuf) == sizeof(struct __utimbuf32) );
    return _utime32(_Filename,(struct __utimbuf32 *)_Utimbuf);
}
static __inline int __CRTDECL _futime(int _Desc, struct _utimbuf * _Utimbuf)
{
    _STATIC_ASSERT( sizeof(struct _utimbuf) == sizeof(struct __utimbuf32) );
    return _futime32(_Desc,(struct __utimbuf32 *)_Utimbuf);
}
static __inline int __CRTDECL _wutime(const wchar_t * _Filename, struct _utimbuf * _Utimbuf)
{
    _STATIC_ASSERT( sizeof(struct _utimbuf) == sizeof(struct __utimbuf32) );
    return _wutime32(_Filename,(struct __utimbuf32 *)_Utimbuf);
}
#else
static __inline int __CRTDECL _utime(const char * _Filename, struct _utimbuf * _Utimbuf)
{
    _STATIC_ASSERT( sizeof(struct _utimbuf) == sizeof(struct __utimbuf64) );
    return _utime64(_Filename,(struct __utimbuf64 *)_Utimbuf);
}
static __inline int __CRTDECL _futime(int _Desc, struct _utimbuf * _Utimbuf)
{
    _STATIC_ASSERT( sizeof(struct _utimbuf) == sizeof(struct __utimbuf64) );
    return _futime64(_Desc,(struct __utimbuf64 *)_Utimbuf);
}
static __inline int __CRTDECL _wutime(const wchar_t * _Filename, struct _utimbuf * _Utimbuf)
{
    _STATIC_ASSERT( sizeof(struct _utimbuf) == sizeof(struct __utimbuf64) );
    return _wutime64(_Filename,(struct __utimbuf64 *)_Utimbuf);
}
#endif /* _USE_32BIT_TIME_T */


#if     !__STDC__

/* Non-ANSI name for compatibility */

#ifdef _USE_32BIT_TIME_T
static __inline int __CRTDECL utime(const char * _Filename, struct utimbuf * _Utimbuf)
{
    _STATIC_ASSERT( sizeof(struct utimbuf) == sizeof(struct __utimbuf32) );
    return _utime32(_Filename,(struct __utimbuf32 *)_Utimbuf);
}
#else
static __inline int __CRTDECL utime(const char * _Filename, struct utimbuf * _Utimbuf)
{
    _STATIC_ASSERT( sizeof(struct utimbuf) == sizeof(struct __utimbuf64) );
    return _utime64(_Filename,(struct __utimbuf64 *)_Utimbuf);
}
#endif

#endif /* !__STDC__ */

#endif
