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
*wio.inl - inline definitions for low-level file handling and I/O functions
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
*       09-22-04  AGH   Guard header with #ifndef RC_INVOKED, so RC ignores it
*       11-10-06  PMB   Removed most _INTEGRAL_MAX_BITS #ifdefs
*                       DDB#11174
*
****/

#if     _MSC_VER > 1000 /*IFSTRIP=IGN*/
#pragma once
#endif

#include <crtdefs.h>

#ifndef _INC_WIO_INL
#define _INC_WIO_INL
#ifndef _SYSCRT
#ifndef RC_INVOKED

/* _STATIC_ASSERT is for enforcing boolean/integral conditions at compile time.
   Since it is purely a compile-time mechanism that generates no code, the check
   is left in even if _DEBUG is not defined. */

#ifndef _STATIC_ASSERT
#define _STATIC_ASSERT(expr) typedef char __static_assert_t[ (expr) ]
#endif

#ifdef _USE_32BIT_TIME_T
static __inline intptr_t __CRTDECL _wfindfirst(const wchar_t *_Filename, _wfinddata_t *_Finddata)
{
    _STATIC_ASSERT( sizeof(_wfinddata_t) == sizeof(struct _wfinddata32_t) );
    return _wfindfirst32(_Filename,(struct _wfinddata32_t *)_Finddata);
}
static __inline int __CRTDECL _wfindnext(intptr_t _Desc, _wfinddata_t *_Finddata)
{
    _STATIC_ASSERT( sizeof(_wfinddata_t) == sizeof(struct _wfinddata32_t) );
    return _wfindnext32(_Desc,(struct _wfinddata32_t *)_Finddata);
}
#else
static __inline intptr_t __CRTDECL _wfindfirst(const wchar_t *_Filename, _wfinddata_t *_Finddata)
{
    _STATIC_ASSERT( sizeof(_wfinddata_t) == sizeof(struct _wfinddata64i32_t) );
    return _wfindfirst64i32(_Filename,(struct _wfinddata64i32_t *)_Finddata);
}
static __inline int __CRTDECL _wfindnext(intptr_t _Desc, _wfinddata_t *_Finddata)
{
    _STATIC_ASSERT( sizeof(_wfinddata_t) == sizeof(struct _wfinddata64i32_t) );
    return _wfindnext64i32(_Desc,(struct _wfinddata64i32_t *)_Finddata);
}
#endif /* _USE_32BIT_TIME_T */

#ifdef _USE_32BIT_TIME_T
static __inline intptr_t __CRTDECL _wfindfirsti64(const wchar_t *_Filename, _wfinddatai64_t *_Finddata)
{
    _STATIC_ASSERT( sizeof(_wfinddatai64_t) == sizeof(struct _wfinddata32i64_t) );
    return _wfindfirst32i64(_Filename,(struct _wfinddata32i64_t *)_Finddata);
}
static __inline int __CRTDECL _wfindnexti64(intptr_t _Desc, _wfinddatai64_t *_Finddata)
{
    _STATIC_ASSERT( sizeof(_wfinddatai64_t) == sizeof(struct _wfinddata32i64_t) );
    return _wfindnext32i64(_Desc,(struct _wfinddata32i64_t *)_Finddata);
}
#else
static __inline intptr_t __CRTDECL _wfindfirsti64(const wchar_t *_Filename, _wfinddatai64_t *_Finddata)
{
    _STATIC_ASSERT( sizeof(_wfinddatai64_t) == sizeof(struct _wfinddata64_t) );
    return _wfindfirst64(_Filename,(struct _wfinddata64_t *)_Finddata);
}
static __inline int __CRTDECL _wfindnexti64(intptr_t _Desc, _wfinddatai64_t *_Finddata)
{
    _STATIC_ASSERT( sizeof(_wfinddatai64_t) == sizeof(struct _wfinddata64_t) );
    return _wfindnext64(_Desc,(struct _wfinddata64_t *)_Finddata);
}
#endif /* _USE_32BIT_TIME_T */

#endif /* RC_INVOKED */
#endif /* _SYSCRT */
#endif /* _INC_WIO_INL */
