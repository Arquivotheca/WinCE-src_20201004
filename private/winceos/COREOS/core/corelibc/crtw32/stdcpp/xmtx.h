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
/* xmtx.h internal header */
#pragma once
#ifndef _XMTX
#define _XMTX
#ifndef RC_INVOKED
#include <yvals.h>
#include <stdlib.h>

 #pragma pack(push,_CRT_PACKING)
 #pragma warning(push,3)
 #pragma push_macro("new")
 #undef new

#ifndef MRTDLL
 #ifndef _M_CEE_PURE
_C_LIB_DECL
 #endif /* _M_CEE_PURE */
#endif /* MRTDLL */

#ifdef MRTDLL
 #ifndef _M_CEE
/* We want to declare Enter/LeaveCriticalSection as p/invoke */
#define EnterCriticalSection _undefined_EnterCriticalSection
#define LeaveCriticalSection _undefined_LeaveCriticalSection
 #endif /* _M_CEE */
#endif /* MRTDLL */

#include <windows.h>

#ifdef MRTDLL
 #ifndef _M_CEE
#undef EnterCriticalSection
#undef LeaveCriticalSection

_RELIABILITY_CONTRACT
[System::Security::SuppressUnmanagedCodeSecurity]
[System::Runtime::InteropServices::DllImport("kernel32.dll")]
void EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection);

_RELIABILITY_CONTRACT
[System::Security::SuppressUnmanagedCodeSecurity]
[System::Runtime::InteropServices::DllImport("kernel32.dll")]
void LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
 #endif /* _M_CEE */
#endif /* MRTDLL */

typedef CRITICAL_SECTION _Rmtx;

#ifdef _M_CEE_PURE
void __clrcall _Mtxinit(_Rmtx *);
void __clrcall _Mtxdst(_Rmtx *);
void __clrcall _Mtxlock(_Rmtx *);
void __clrcall _Mtxunlock(_Rmtx *);

#else /* _M_CEE_PURE */
_MRTIMP2 void __cdecl _Mtxinit(_Rmtx *);
_MRTIMP2 void __cdecl _Mtxdst(_Rmtx *);
_MRTIMP2 void __cdecl _Mtxlock(_Rmtx *);
_MRTIMP2 void __cdecl _Mtxunlock(_Rmtx *);
#endif /* _M_CEE_PURE */

 #if !_MULTI_THREAD
  #define _Mtxinit(mtx)
  #define _Mtxdst(mtx)
  #define _Mtxlock(mtx)
  #define _Mtxunlock(mtx)

typedef char _Once_t;

  #define _Once(cntrl, func)	if (*(cntrl) == 0) (func)(), *(cntrl) = 2
  #define _ONCE_T_INIT	0

 #else /* !_MULTI_THREAD */

typedef long _Once_t;

  #ifdef _M_CEE_PURE
void __clrcall _Once(_Once_t *, void (__cdecl *)(void));

  #else /* _M_CEE_PURE */
_MRTIMP2 void __cdecl _Once(_Once_t *, void (__cdecl *)(void));
  #endif /* _M_CEE_PURE */

  #define _ONCE_T_INIT	0
 #endif /* !_MULTI_THREAD */

#ifndef MRTDLL
 #ifndef _M_CEE_PURE
_END_C_LIB_DECL
 #endif /* _M_CEE_PURE */
#endif /* MRTDLL */
 #pragma pop_macro("new")
 #pragma warning(pop)
 #pragma pack(pop)
#endif /* RC_INVOKED */
#endif /* _XMTX */

/*
 * Copyright (c) 1992-2012 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
V6.00:0009 */
