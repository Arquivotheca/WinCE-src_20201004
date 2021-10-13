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
*swprintf.inl - inline definitions for (v)swprintf
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This file contains the function definitions for (v)swprintf
*
*       [Public]
*
*Revision History:
*       11-01-02  SJ   Created. This contains the inline versions
*                      of (v)swprintf. This just calls the _(v)snwprintf
*                      versions in the library.
*       08-21-03  SJ    VSWhidbey#46431 - swprintf error return fix
*       09-22-04  AGH   Guard header with #ifndef RC_INVOKED, so RC ignores it
*       10-14-04  MAL   VSWhidbey#226980 - remove static from (v)swprintf.
*       11-05-04  AGH   [v]swprintf is unmanaged to make sure programs link
*                       with the right version
*       02-01-05  AC    Added sal annotation to C++ version of swprintf and vswprintf
*                       VSW#448588
*       02-07-05  AC    Disable benign warning 4412
*                       VSW#448092
*       03-08-05  PAL   Add non-conforming overloads of _swprintf_l, _vswprintf_l (VSW 452537).
*       04-01-05  MSL   Correct SAL annotation for n functions
*
****/

#pragma once

#if !defined(__CRTDECL)
#if defined(_M_CEE_PURE) || defined(MRTDLL)
#define __CRTDECL   __clrcall
#else
#define __CRTDECL   __cdecl
#endif
#endif


#ifndef _INC_SWPRINTF_INL_
#define _INC_SWPRINTF_INL_
#ifndef RC_INVOKED

#include <vadefs.h>

#if defined(_M_CEE_MIXED)
#pragma managed(push, off)
#endif

#pragma warning( push )
#pragma warning( disable : 4793 4412 )
static __inline int swprintf(wchar_t * _String, size_t _Count, const wchar_t * _Format, ...)
{
    va_list _Arglist;
    int _Ret;
    _crt_va_start(_Arglist, _Format);
    _Ret = _vswprintf_c_l(_String, _Count, _Format, NULL, _Arglist);
    _crt_va_end(_Arglist);
    return _Ret;
}
#pragma warning( pop )

#pragma warning( push )
#pragma warning( disable : 4412 )
static __inline int __CRTDECL vswprintf(wchar_t * _String, size_t _Count, const wchar_t * _Format, va_list _Ap)
{
    return _vswprintf_c_l(_String, _Count, _Format, NULL, _Ap);
}
#pragma warning( pop )
#if defined(_M_CEE_MIXED)
#pragma managed(pop)
#endif

#pragma warning( push )
#pragma warning( disable : 4793 4412 )
static __inline int _swprintf_l(wchar_t * _String, size_t _Count, const wchar_t * _Format, _locale_t _Plocinfo, ...)
{
    va_list _Arglist;
    int _Ret;
    _crt_va_start(_Arglist, _Plocinfo);
    _Ret = _vswprintf_c_l(_String, _Count, _Format, _Plocinfo, _Arglist);
    _crt_va_end(_Arglist);
    return _Ret;
}
#pragma warning( pop )

#pragma warning( push )
#pragma warning( disable : 4412 )
static __inline int __CRTDECL _vswprintf_l(wchar_t * _String, size_t _Count, const wchar_t * _Format, _locale_t _Plocinfo, va_list _Ap)
{
    return _vswprintf_c_l(_String, _Count, _Format, _Plocinfo, _Ap);
}
#pragma warning( pop )

#ifdef __cplusplus /*IFSTRIP=IGN*/
#pragma warning( push )
#pragma warning( disable : 4996 )

#pragma warning( push )
#pragma warning( disable : 4793 4141 25025)
extern "C++" _SWPRINTFS_DEPRECATED _CRT_INSECURE_DEPRECATE(swprintf_s) __inline int swprintf(_Pre_notnull_ _Post_z_ wchar_t * _String, _In_z_ _Printf_format_string_ const wchar_t * _Format, ...)
{
    va_list _Arglist;
    _crt_va_start(_Arglist, _Format);
    int _Ret = _vswprintf(_String, _Format, _Arglist);
    _crt_va_end(_Arglist);
    return _Ret;
}
#pragma warning( pop )

#pragma warning( push )
#pragma warning( disable : 4141 25025)
extern "C++" _SWPRINTFS_DEPRECATED _CRT_INSECURE_DEPRECATE(vswprintf_s) __inline int __CRTDECL vswprintf(_Pre_notnull_ _Post_z_ wchar_t * _String, _In_z_ _Printf_format_string_ const wchar_t * _Format, va_list _Ap)
{
    return _vswprintf(_String, _Format, _Ap);
}
#pragma warning( pop )

#pragma warning( push )
#pragma warning( disable : 4793 4141 )
extern "C++" _SWPRINTFS_DEPRECATED _CRT_INSECURE_DEPRECATE(_swprintf_s_l) __inline int _swprintf_l(_Pre_notnull_ _Post_z_ wchar_t * _String, _In_z_ _Printf_format_string_params_(0) const wchar_t * _Format, _locale_t _Plocinfo, ...)
{
    va_list _Arglist;
    _crt_va_start(_Arglist, _Plocinfo);
    int _Ret = __vswprintf_l(_String, _Format, _Plocinfo, _Arglist);
    _crt_va_end(_Arglist);
    return _Ret;
}
#pragma warning( pop )

#pragma warning( push )
#pragma warning( disable : 4141 )
extern "C++" _SWPRINTFS_DEPRECATED _CRT_INSECURE_DEPRECATE(_vswprintf_s_l) __inline int __CRTDECL _vswprintf_l(_Pre_notnull_ _Post_z_ wchar_t * _String, _In_z_ _Printf_format_string_params_(2) const wchar_t * _Format, _locale_t _Plocinfo, va_list _Ap)
{
    return __vswprintf_l(_String, _Format, _Plocinfo, _Ap);
}
#pragma warning( pop )

#pragma warning( pop )

#endif  /* __cplusplus */

#endif /* RC_INVOKED */
#endif /* _INC_SWPRINTF_INL_ */

