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
*wtime.inl - inline definitions for wctime()
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This file contains the definition of wctime().
*
*       [Public]
*
*Revision History:
*       02-04-03  EAN   Created
*       03-08-04  MSL   Fixed inlines to work with secure deprecation
*       09-22-04  AGH   Guard header with #ifndef RC_INVOKED, so RC ignores it
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

#ifndef _INC_WTIME_INL
#define _INC_WTIME_INL
#ifndef RC_INVOKED

#pragma warning(push)
#pragma warning(disable:4996)

#ifdef _USE_32BIT_TIME_T
static __inline wchar_t * __CRTDECL _wctime(const time_t * _Time)
{
#pragma warning( push )
#pragma warning( disable : 4996 )
    return _wctime32(_Time);
#pragma warning( pop )
}

static __inline errno_t __CRTDECL _wctime_s(wchar_t *_Buffer, size_t _SizeInWords, const time_t * _Time)
{
    return _wctime32_s(_Buffer, _SizeInWords, _Time);
}
#else
static __inline wchar_t * __CRTDECL _wctime(const time_t * _Time)
{
#pragma warning( push )
#pragma warning( disable : 4996 )
    return _wctime64(_Time);
#pragma warning( pop )
}

static __inline errno_t __CRTDECL _wctime_s(wchar_t *_Buffer, size_t _SizeInWords, const time_t * _Time)
{
    return _wctime64_s(_Buffer, _SizeInWords, _Time);
}
#endif /* _USE_32BIT_TIME_T */

#pragma warning(pop)

#endif /* RC_INVOKED */
#endif /* _INC_WTIME_INL */
