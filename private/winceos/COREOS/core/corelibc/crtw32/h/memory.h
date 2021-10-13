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
*memory.h - declarations for buffer (memory) manipulation routines
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This include file contains the function declarations for the
*       buffer (memory) manipulation routines.
*       [System V]
*
*       [Public]
*
*Revision History:
*       10/20/87  JCR   Removed "MSC40_ONLY" entries
*       12-11-87  JCR   Added "_loadds" functionality
*       12-18-87  JCR   Added _FAR_ to declarations
*       02-10-88  JCR   Cleaned up white space
*       08-22-88  GJF   Modified to also work for the 386 (small model only)
*       05-03-89  JCR   Added _INTERNAL_IFSTRIP for relinc usage
*       08-03-89  GJF   Cleanup, now specific to OS/2 2.0 (i.e., 386 flat model)
*       10-30-89  GJF   Fixed copyright
*       11-02-89  JCR   Changed "DLL" to "_DLL"
*       11-17-89  GJF   Added const to appropriate arg types for memccpy() and
*                       memicmp().
*       03-01-90  GJF   Added #ifndef _INC_MEMORY and #include <cruntime.h>
*                       stuff. Replace _cdecl with _CALLTYPE1 in prototypes.
*                       Also, removed some (now) useless preprocessor
*                       directives.
*       03-21-90  GJF   Replaced _cdecl with _CALLTYPE1 in prototypes. Also,
*                       got rid of movedata() prototype.
*       01-17-91  GJF   ANSI naming.
*       08-20-91  JCR   C++ and ANSI naming
*       09-28-91  JCR   ANSI names: DOSX32=prototypes, WIN32=#defines for now
*       08-05-92  GJF   Function calling type and variable type macros.
*       01-21-93  GJF   Removed support for C6-386's _cdecl.
*       04-06-93  SKS   Replace _CRTAPI1/2 with __cdecl, _CRTVAR1 with nothing
*       04-07-93  SKS   Add _CRTIMP keyword for CRT DLL model
*                       Use link-time aliases for old names, not #define's
*                       Intrinsic functions cannot use __declspec(dllimport)
*       10-11-93  GJF   Merged Cuda and NT versions.
*       02-11-95  CFW   Add _CRTBLD to avoid users getting wrong headers.
*       02-14-95  CFW   Clean up Mac merge.
*       03-10-95  BWT   add _CRTIMP to MIPS intrinsics
*       12-14-95  JWM   Add "#pragma once".
*       02-05-97  GJF   Deleted obsolete support for _CRTAPI* and _NTSDK. 
*       09-30-97  JWM   Restored not-so-obsolete _CRTAPI1 support.
*       12-15-98  GJF   Changes for 64-bit size_t.
*       05-13-99  PML   Remove _CRTAPI1
*       05-17-99  PML   Remove all Macintosh support.
*       10-06-99  PML   Add _W64 modifier to types which are 32 bits in Win32,
*                       64 bits in Win64.
*       10-27-99  PML   unsigned int -> size_t in memccpy, memicmp
*       07-15-01  PML   Remove all ALPHA, MIPS, and PPC code
*       10-09-02  EAN   Added std C++ overload for memchr
*       05-11-04  AC    [w]mem[cpy|move] are deprecated only if _CRT_SECURE_DEPRECATE_MEMORY is defined
*                       VSW#302444
*       11-08-04  JL    Added _CRT_NONSTDC_DEPRECATE to deprecate non-ANSI functions
*       11-18-04  ATC   SAL Annotations for CRT Headers
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*                       Packing fix
*       04-14-05  MSL   Compile under MIDL
*                       SAL cleanup for n functions
*
****/

#pragma once

#ifndef _INC_MEMORY
#define _INC_MEMORY

#include <crtdefs.h>

#ifdef  __cplusplus
extern "C" {
#endif

#ifndef _CONST_RETURN
#ifdef  __cplusplus
#define _CONST_RETURN  const
#define _CRT_CONST_CORRECT_OVERLOADS
#else
#define _CONST_RETURN
#endif
#endif

/* For backwards compatibility */
#define _WConst_return _CONST_RETURN

/* Function prototypes */
#ifndef _CRT_MEMORY_DEFINED
#define _CRT_MEMORY_DEFINED
_CRTIMP void *  __cdecl _memccpy( _Out_writes_bytes_opt_(_MaxCount) void * _Dst, _In_ const void * _Src, _In_ int _Val, _In_ size_t _MaxCount);
_Check_return_ _CRTIMP _CONST_RETURN void *  __cdecl memchr( _In_reads_bytes_opt_(_MaxCount) const void * _Buf , _In_ int _Val, _In_ size_t _MaxCount);
_Check_return_ _CRTIMP int     __cdecl _memicmp(_In_reads_bytes_opt_(_Size) const void * _Buf1, _In_reads_bytes_opt_(_Size) const void * _Buf2, _In_ size_t _Size);
_Check_return_ _CRTIMP int     __cdecl _memicmp_l(_In_reads_bytes_opt_(_Size) const void * _Buf1, _In_reads_bytes_opt_(_Size) const void * _Buf2, _In_ size_t _Size, _In_opt_ _locale_t _Locale);
_Check_return_ int     __cdecl memcmp(_In_reads_bytes_(_Size) const void * _Buf1, _In_reads_bytes_(_Size) const void * _Buf2, _In_ size_t _Size);
_CRT_INSECURE_DEPRECATE_MEMORY(memcpy_s)
_Post_equal_to_(_Dst)
_At_buffer_((unsigned char*)_Dst, _Iter_, _Size, _Post_satisfies_(((unsigned char*)_Dst)[_Iter_] == ((unsigned char*)_Src)[_Iter_]))
void *  __cdecl memcpy(_Out_writes_bytes_all_(_Size) void * _Dst, _In_reads_bytes_(_Size) const void * _Src, _In_ size_t _Size);
#if __STDC_WANT_SECURE_LIB__
_CRTIMP errno_t  __cdecl memcpy_s(_Out_writes_bytes_to_opt_(_DstSize, _MaxCount) void * _Dst, _In_ rsize_t _DstSize, _In_reads_bytes_opt_(_MaxCount) const void * _Src, _In_ rsize_t _MaxCount);
#if defined(__cplusplus) && _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_MEMORY
extern "C++"
{
 #ifndef _CRT_ENABLE_IF_DEFINED
  #define _CRT_ENABLE_IF_DEFINED
    template<bool _Enable, typename _Ty>
    struct _CrtEnableIf;

    template<typename _Ty>
    struct _CrtEnableIf<true, _Ty>
    {
        typedef _Ty _Type;
    };
 #endif
    template <size_t _Size, typename _DstType>
    inline
    typename _CrtEnableIf<(_Size > 1), void *>::_Type __cdecl memcpy(_DstType (&_Dst)[_Size], _In_reads_bytes_opt_(_SrcSize) const void *_Src, _In_ size_t _SrcSize) _CRT_SECURE_CPP_NOTHROW
    {
        return memcpy_s(_Dst, _Size * sizeof(_DstType), _Src, _SrcSize) == 0 ? _Dst : 0;
    }
}
#endif
#if defined(__cplusplus) && _CRT_SECURE_CPP_OVERLOAD_SECURE_NAMES_MEMORY
extern "C++"
{
    template <size_t _Size, typename _DstType>
    inline
    errno_t __CRTDECL memcpy_s(_DstType (&_Dst)[_Size], _In_reads_bytes_opt_(_SrcSize) const void * _Src, _In_ rsize_t _SrcSize) _CRT_SECURE_CPP_NOTHROW
    {
        return memcpy_s(_Dst, _Size * sizeof(_DstType), _Src, _SrcSize);
    }
}
#endif
#endif
        _Post_equal_to_(_Dst)
        _At_buffer_((unsigned char*)_Dst, _Iter_, _Size, _Post_satisfies_(((unsigned char*)_Dst)[_Iter_] == _Val))
        void *  __cdecl memset(_Out_writes_bytes_all_(_Size) void * _Dst, _In_ int _Val, _In_ size_t _Size);

#if     !__STDC__
/* Non-ANSI names for compatibility */
_CRT_NONSTDC_DEPRECATE(_memccpy) _CRTIMP void * __cdecl memccpy(_Out_writes_bytes_opt_(_Size) void * _Dst, _In_reads_bytes_opt_(_Size) const void * _Src, _In_ int _Val, _In_ size_t _Size);
_Check_return_ _CRT_NONSTDC_DEPRECATE(_memicmp) _CRTIMP int __cdecl memicmp(_In_reads_bytes_opt_(_Size) const void * _Buf1, _In_reads_bytes_opt_(_Size) const void * _Buf2, _In_ size_t _Size);
#endif  /* __STDC__ */

#endif

#ifdef  __cplusplus
#ifndef _CPP_MEMCHR_DEFINED
#define _CPP_MEMCHR_DEFINED
extern "C++" _Check_return_ inline void * __CRTDECL memchr( _In_reads_bytes_opt_(_N) void * _Pv , _In_ int _C, _In_ size_t _N)
	{ return (void*)memchr((const void*)_Pv, _C, _N); }
#endif
#endif

#ifdef  __cplusplus
}
#endif

#endif  /* _INC_MEMORY */
