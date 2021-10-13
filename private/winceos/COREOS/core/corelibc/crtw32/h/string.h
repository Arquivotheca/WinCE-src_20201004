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
*string.h - declarations for string manipulation functions
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This file contains the function declarations for the string
*       manipulation functions.
*       [ANSI/System V]
*
*       [Public]
*
*Revision History:
*       10/20/87  JCR   Removed "MSC40_ONLY" entries
*       12-11-87  JCR   Added "_loadds" functionality
*       12-18-87  JCR   Added _FAR_ to declarations
*       02-10-88  JCR   Cleaned up white space
*       08-19-88  GJF   Modified to also work for the 386 (small model only)
*       03-22-88  JCR   Added strcoll and strxfrm
*       05-03-89  JCR   Added _INTERNAL_IFSTRIP for relinc usage
*       08-03-89  GJF   Cleanup, now specific to OS/2 2.0 (i.e., 386 flat model)
*       10-30-89  GJF   Fixed copyright, removed dummy args from strcoll and
*                       strxfrm
*       11-02-89  JCR   Changed "DLL" to "_DLL"
*       11-17-89  GJF   Added const to appropriate arg types for memccpy(),
*                       memicmp() and _strerror().
*       02-27-90  GJF   Added #ifndef _INC_STRING, #include <cruntime.h>
*                       and _CALLTYPE1 stuff. Also, some cleanup.
*       03-21-90  GJF   Got rid of movedata() prototype.
*       08-14-90  SBM   Added NULL definition for ANSI compliance
*       11-12-90  GJF   Changed NULL to (void *)0.
*       01-18-91  GJF   ANSI naming.
*       02-12-91  GJF   Only #define NULL if it isn't #define-d.
*       03-21-91  KRS   Added wchar_t type, also in stdlib.h and stddef.h.
*       08-20-91  JCR   C++ and ANSI naming
*       09-28-91  JCR   ANSI names: DOSX32=prototypes, WIN32=#defines for now
*       10-07-91  ETC   Prototypes for wcs functions and _stricoll under _INTL.
*       04-06-92  KRS   Rip out _INTL switches again.
*       06-23-92  GJF   double slash is non-ANSI comment limiter.
*       08-05-92  GJF   Function calling type and variable type macros.
*       08-18-92  KRS   Activate wcstok.
*       08-21-92  GJF   Merged last two changes.
*       01-21-93  GJF   Removed support for C6-386's _cdecl.
*       04-07-93  SKS   Add _CRTIMP keyword for CRT DLL model
*                       Use link-time aliases for old names, not #define's
*                       Intrinsic functions cannot use __declspec(dllimport)
*       10-06-93  GJF   Merged NT SDK and Cuda versions.
*       11-22-93  CFW   wcsxxx defines in SDK, prototypes in !SDK.
*       12-07-93  CFW   Move wide defs outside __STDC__ check.
*       01-14-94  CFW   Add _strnicoll, _wcsnicoll.
*       04-11-94  CFW   Add _NLSCMPERROR.
*       05-17-94  GJF   Compiler for DEC Alpha provides implementation of
*                       memmove as an intrinsic. Thus, Alpha version of
*                       prototype cannot be _CRTIMP.
*       05-26-94  CFW   Add _strncoll, _wcsncoll.
*       12-28-94  JCF   Merged with mac header.
*       02-11-95  CFW   Add _CRTBLD to avoid users getting wrong headers.
*       02-14-95  CFW   Clean up Mac merge.
*       03-01-95  SAH   add _CRTIMP to MIPS intrinsics
*       12-14-95  JWM   Add "#pragma once".
*       02-05-97  GJF   Cleaned out obsolete support for _CRTAPI* and _NTSDK.
*       09-30-97  JWM   Restored not-so-obsolete _CRTAPI1 support.
*       08-27-98  GJF   Added __ascii_memicmp, __ascii_stricmp and 
*                       __ascii_strnicmp.
*       12-15-98  GJF   Changes for 64-bit size_t.
*       05-13-99  PML   Remove _CRTAPI1
*       05-17-99  PML   Remove all Macintosh support.
*       10-06-99  PML   Add _W64 modifier to types which are 32 bits in Win32,
*                       64 bits in Win64.
*       10-27-99  PML   unsigned int -> size_t in memccpy, memicmp
*       11-30-99  GB    Add _wcserror and __wcserror
*       07-15-01  PML   Remove all ALPHA, MIPS, and PPC code
*       10-09-02  EAN   Added std C++ overloads for strchr, strpbrk, strrchr,
*                       strstr, memchr, wcschr, wcspbrk, wcsrchr, wcsstr
*       08-01-03  AC    Added support for mode _dbg functions (a la _malloc_dbg)
*       02-06-04  SJ    Fixed signature for memmove_s VSW#232378.
*       03-10-04  AC    Changed name from strlen_s to strnlen.
*       03-22-04  AJS   Removed inline /clr:pure versions of aliased functions now linker bug is fixed.
*       03-22-04  MSL   Double slash removal 
*       04-20-04  AC    Remove duplicated declaration
*                       VSW#287561
*       05-11-04  AC    [w]mem[cpy|move] are deprecated only if _CRT_SECURE_DEPRECATE_MEMORY is defined
*                       VSW#302444
*       07-19-04  AC    Added _CRT_ALTERNATIVE_* defines for safecrt.h
*       07-29-04  AC    Added more _CRT_ALTERNATIVE_* defines for safecrt.h
*       08-03-04  SJ    Removed redundant CONST_RETURN defn - it exists in crtdefs.h
*       08-05-04  AC    Added _strset_s family.
*                       Removed _CRT_ALTERNATIVE and introduced _CRTIMP_ALTERNATIVE.
*       10-05-04  AC    Removed obsolete strlen_s family.
*                       VSW#379264
*       10-31-04  MSL   Fix parameter names that violate standard
*       11-02-04  AC    Add cpp overloads for secure functions.
*                       VSW#313364
*       11-08-04  JL    Added _CRT_NONSTDC_DEPRECATE to deprecate non-ANSI functions
*       11-18-04  ATC   SAL Annotations for CRT Headers
*       01-14-05  AC    Fix SAL annotations (using prefast espx plug-in)
*       01-21-05  MSL   Add param names to overload macros
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*                       Packing fix
*       04-01-05  MSL   Correct SAL annotation for n functions
*       04-14-05  MSL   Intellisense locale name cleanup
*                       Compile clean under MIDL
*                       SAL cleanup for n functions
*       05-27-05  MSL   64 bit definition of NULL
*       06-03-05  MSL   Revert 64 bit definition of NULL due to calling-convention fix
*       12-05-05  MSL   Remove disabling of dud warning
*       12-07-05  MSL   SAL fixes for prefast
*       01-25-06  AC    Added/modified some macro for secure cpp overloads
*                       VSW#565201
*       07-18-07  ATC   Fixing OACR warnings.
*
****/

#pragma once

#ifndef _INC_STRING
#define _INC_STRING

#include <crtdefs.h>

#ifdef  __cplusplus
extern "C" {
#endif

#ifndef _NLSCMP_DEFINED
#define _NLSCMPERROR    2147483647  /* currently == INT_MAX */
#define _NLSCMP_DEFINED
#endif

/* Define NULL pointer value */
#ifndef NULL
#ifdef __cplusplus
#define NULL    0
#else
#define NULL    ((void *)0)
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

_Check_return_wat_ _CRTIMP_ALTERNATIVE errno_t __cdecl _strset_s(_Inout_updates_z_(_DstSize) char * _Dst, _In_ size_t _DstSize, _In_ int _Value);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_1(errno_t, _strset_s, _Prepost_z_ char, _Dest, _In_ int, _Value)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_1(char *, __RETURN_POLICY_DST, __EMPTY_DECLSPEC, _strset, _Inout_z_, char, _Dest, _In_ int, _Value)
#if __STDC_WANT_SECURE_LIB__
_Check_return_wat_ _CRTIMP_ALTERNATIVE errno_t __cdecl strcpy_s(_Out_writes_z_(_SizeInBytes) char * _Dst, _In_ rsize_t _SizeInBytes, _In_z_ const char * _Src);
#endif
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_1(errno_t, strcpy_s, _Post_z_ char, _Dest, _In_z_ const char *, _Source)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_1(char *, __RETURN_POLICY_DST, __EMPTY_DECLSPEC, strcpy, _Out_writes_z_(_String_length_(_Source) + 1), char, _Dest, _In_z_ const char *, _Source)
#if __STDC_WANT_SECURE_LIB__
_Check_return_wat_ _CRTIMP_ALTERNATIVE errno_t __cdecl strcat_s(_Inout_updates_z_(_SizeInBytes) char * _Dst, _In_ rsize_t _SizeInBytes, _In_z_ const char * _Src);
#endif
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_1(errno_t, strcat_s, char, _Dest, _In_z_ const char *, _Source)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_1(char *, __RETURN_POLICY_DST, __EMPTY_DECLSPEC, strcat, _Inout_updates_z_(_String_length_(_Dest) + _String_length_(_Source) + 1), char, _Dest, _In_z_ const char *, _Source)
        _Check_return_ int     __cdecl strcmp(_In_z_ const char * _Str1, _In_z_ const char * _Str2);
        _Check_return_ size_t  __cdecl strlen(_In_z_ const char * _Str);
_Check_return_ _CRTIMP
_When_(_MaxCount > _String_length_(_Str), _Post_satisfies_(return == _String_length_(_Str)))
_When_(_MaxCount <= _String_length_(_Str), _Post_satisfies_(return == _MaxCount))
size_t  __cdecl strnlen(_In_reads_or_z_(_MaxCount) const char * _Str, _In_ size_t _MaxCount);
#if __STDC_WANT_SECURE_LIB__ && !defined (__midl)
_Check_return_ static __inline
_When_(_MaxCount > _String_length_(_Str), _Post_satisfies_(return == _String_length_(_Str)))
_When_(_MaxCount <= _String_length_(_Str), _Post_satisfies_(return == _MaxCount))
size_t  __CRTDECL strnlen_s(_In_reads_or_z_(_MaxCount) const char * _Str, _In_ size_t _MaxCount)
{
    return (_Str==0) ? 0 : strnlen(_Str, _MaxCount);
}
#endif
#if __STDC_WANT_SECURE_LIB__
_Check_return_wat_ _CRTIMP errno_t __cdecl memmove_s(_Out_writes_bytes_to_opt_(_DstSize,_MaxCount) void * _Dst, _In_ rsize_t _DstSize, _In_reads_bytes_opt_(_MaxCount) const void * _Src, _In_ rsize_t _MaxCount);
#endif

_CRTIMP _CRT_INSECURE_DEPRECATE_MEMORY(memmove_s) void *  __cdecl memmove(_Out_writes_bytes_all_opt_(_Size) void * _Dst, _In_reads_bytes_opt_(_Size) const void * _Src, _In_ size_t _Size);

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#pragma push_macro("_strdup")
#undef _strdup
#endif

_Check_return_ _CRTIMP char *  __cdecl _strdup(_In_opt_z_ const char * _Src);

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#pragma pop_macro("_strdup")
#endif

_Check_return_ _CRTIMP _CONST_RETURN char *  __cdecl strchr(_In_z_ const char * _Str, _In_ int _Val);
_Check_return_ _CRTIMP int     __cdecl _stricmp(_In_z_  const char * _Str1, _In_z_  const char * _Str2);
_Check_return_ _CRTIMP int     __cdecl _strcmpi(_In_z_  const char * _Str1, _In_z_  const char * _Str2);
_Check_return_ _CRTIMP int     __cdecl _stricmp_l(_In_z_  const char * _Str1, _In_z_  const char * _Str2, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int     __cdecl strcoll(_In_z_  const char * _Str1, _In_z_  const  char * _Str2);
_Check_return_ _CRTIMP int     __cdecl _strcoll_l(_In_z_  const char * _Str1, _In_z_  const char * _Str2, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int     __cdecl _stricoll(_In_z_  const char * _Str1, _In_z_  const char * _Str2);
_Check_return_ _CRTIMP int     __cdecl _stricoll_l(_In_z_  const char * _Str1, _In_z_  const char * _Str2, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int     __cdecl _strncoll  (_In_reads_or_z_(_MaxCount) const char * _Str1, _In_reads_or_z_(_MaxCount) const char * _Str2, _In_ size_t _MaxCount);
_Check_return_ _CRTIMP int     __cdecl _strncoll_l(_In_reads_or_z_(_MaxCount) const char * _Str1, _In_reads_or_z_(_MaxCount) const char * _Str2, _In_ size_t _MaxCount, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int     __cdecl _strnicoll (_In_reads_or_z_(_MaxCount) const char * _Str1, _In_reads_or_z_(_MaxCount) const char * _Str2, _In_ size_t _MaxCount);
_Check_return_ _CRTIMP int     __cdecl _strnicoll_l(_In_reads_or_z_(_MaxCount) const char * _Str1, _In_reads_or_z_(_MaxCount) const char * _Str2, _In_ size_t _MaxCount, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP size_t  __cdecl strcspn(_In_z_  const char * _Str, _In_z_  const char * _Control);
_Check_return_ _CRT_INSECURE_DEPRECATE(_strerror_s) _CRTIMP char *  __cdecl _strerror(_In_opt_z_ const char * _ErrMsg);
_Check_return_wat_ _CRTIMP errno_t __cdecl _strerror_s(_Out_writes_z_(_SizeInBytes) char * _Buf, _In_ size_t _SizeInBytes, _In_opt_z_ const char * _ErrMsg);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_1(errno_t, _strerror_s, char, _Buffer, _In_opt_z_ const char *, _ErrorMessage)
_Check_return_ _CRT_INSECURE_DEPRECATE(strerror_s) _CRTIMP char *  __cdecl strerror(_In_ int);
#if __STDC_WANT_SECURE_LIB__
_Check_return_wat_ _CRTIMP errno_t __cdecl strerror_s(_Out_writes_z_(_SizeInBytes) char * _Buf, _In_ size_t _SizeInBytes, _In_ int _ErrNum);
#endif
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_1(errno_t, strerror_s, char, _Buffer, _In_ int, _ErrorMessage)
_Check_return_wat_ _CRTIMP errno_t __cdecl _strlwr_s(_Inout_updates_z_(_Size) char * _Str, _In_ size_t _Size);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_0(errno_t, _strlwr_s, _Prepost_z_ char, _String)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_0(char *, __RETURN_POLICY_DST, _CRTIMP, _strlwr, _Inout_z_, char, _String)
_Check_return_wat_ _CRTIMP errno_t __cdecl _strlwr_s_l(_Inout_updates_z_(_Size) char * _Str, _In_ size_t _Size, _In_opt_ _locale_t _Locale);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_1(errno_t, _strlwr_s_l, _Prepost_z_ char, _String, _In_opt_ _locale_t, _Locale)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_1_EX(char *, __RETURN_POLICY_DST, _CRTIMP, _strlwr_l, _strlwr_s_l, _Inout_updates_z_(_Size) char, _Inout_z_, char, _String, _In_opt_ _locale_t, _Locale)
#if __STDC_WANT_SECURE_LIB__
_Check_return_wat_ _CRTIMP_ALTERNATIVE errno_t __cdecl strncat_s(_Inout_updates_z_(_SizeInBytes) char * _Dst, _In_ rsize_t _SizeInBytes, _In_reads_or_z_(_MaxCount) const char * _Src, _In_ rsize_t _MaxCount);
#endif
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_2(errno_t, strncat_s, _Prepost_z_ char, _Dest, _In_reads_or_z_(_Count) const char *, _Source, _In_ size_t, _Count)
#pragma warning(push)
#pragma warning(disable:6059)
/* prefast noise VSW 489802 */
__DEFINE_CPP_OVERLOAD_STANDARD_NFUNC_0_2_EX(char *, __RETURN_POLICY_DST, _CRTIMP, strncat, strncat_s, _Inout_updates_z_(_Size) char, _Inout_updates_z_(_Count), char, _Dest, _In_reads_or_z_(_Count) const char *, _Source, _In_ size_t, _Count)
#pragma warning(pop)
_Check_return_ _CRTIMP int     __cdecl strncmp(_In_reads_or_z_(_MaxCount) const char * _Str1, _In_reads_or_z_(_MaxCount) const char * _Str2, _In_ size_t _MaxCount);
_Check_return_ _CRTIMP int     __cdecl _strnicmp(_In_reads_or_z_(_MaxCount) const char * _Str1, _In_reads_or_z_(_MaxCount) const char * _Str2, _In_ size_t _MaxCount);
_Check_return_ _CRTIMP int     __cdecl _strnicmp_l(_In_reads_or_z_(_MaxCount) const char * _Str1, _In_reads_or_z_(_MaxCount) const char * _Str2, _In_ size_t _MaxCount, _In_opt_ _locale_t _Locale);
#if __STDC_WANT_SECURE_LIB__
_Check_return_wat_ _CRTIMP_ALTERNATIVE errno_t __cdecl strncpy_s(_Out_writes_z_(_SizeInBytes) char * _Dst, _In_ rsize_t _SizeInBytes, _In_reads_or_z_(_MaxCount) const char * _Src, _In_ rsize_t _MaxCount);
#endif
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_2(errno_t, strncpy_s, char, _Dest, _In_reads_or_z_(_Count) const char *, _Source, _In_ size_t, _Count)
__DEFINE_CPP_OVERLOAD_STANDARD_NFUNC_0_2_EX(char *, __RETURN_POLICY_DST, _CRTIMP, strncpy, strncpy_s, _Out_writes_z_(_Size) char, _Out_writes_(_Count) _Post_maybez_, char, _Dest, _In_reads_or_z_(_Count) const char *, _Source, _In_ size_t, _Count)
_Check_return_wat_ _CRTIMP_ALTERNATIVE errno_t __cdecl _strnset_s(_Inout_updates_z_(_SizeInBytes) char * _Str, _In_ size_t _SizeInBytes, _In_ int _Val, _In_ size_t _MaxCount);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_2(errno_t, _strnset_s, _Prepost_z_ char, _Dest, _In_ int, _Val, _In_ size_t, _Count)
__DEFINE_CPP_OVERLOAD_STANDARD_NFUNC_0_2_EX(char *, __RETURN_POLICY_DST, _CRTIMP, _strnset, _strnset_s, _Inout_updates_z_(_Size) char, _Inout_updates_z_(_Count), char, _Dest, _In_ int, _Val, _In_ size_t, _Count)
_Check_return_ _CRTIMP _CONST_RETURN char *  __cdecl strpbrk(_In_z_ const char * _Str, _In_z_ const char * _Control);
_Check_return_ _CRTIMP _CONST_RETURN char *  __cdecl strrchr(_In_z_ const char * _Str, _In_ int _Ch);
_CRTIMP char *  __cdecl _strrev(_Inout_z_ char * _Str);
_Check_return_ _CRTIMP size_t  __cdecl strspn(_In_z_ const char * _Str, _In_z_ const char * _Control);
_Check_return_ _Ret_maybenull_ _CRTIMP _CONST_RETURN char *  __cdecl strstr(_In_z_ const char * _Str, _In_z_ const char * _SubStr);
_Check_return_ _CRT_INSECURE_DEPRECATE(strtok_s) _CRTIMP char *  __cdecl strtok(_Inout_opt_z_ char * _Str, _In_z_ const char * _Delim);
#if __STDC_WANT_SECURE_LIB__
_Check_return_ _CRTIMP_ALTERNATIVE char *  __cdecl strtok_s(_Inout_opt_z_ char * _Str, _In_z_ const char * _Delim, _Inout_ _Deref_prepost_opt_z_ char ** _Context);
#endif
_Check_return_wat_ _CRTIMP errno_t __cdecl _strupr_s(_Inout_updates_z_(_Size) char * _Str, _In_ size_t _Size);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_0(errno_t, _strupr_s, _Prepost_z_ char, _String)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_0(char *, __RETURN_POLICY_DST, _CRTIMP, _strupr, _Inout_z_, char, _String)
_Check_return_wat_ _CRTIMP errno_t __cdecl _strupr_s_l(_Inout_updates_z_(_Size) char * _Str, _In_ size_t _Size, _locale_t _Locale);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_1(errno_t, _strupr_s_l, _Prepost_z_ char, _String, _locale_t, _Locale)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_1_EX(char *, __RETURN_POLICY_DST, _CRTIMP, _strupr_l, _strupr_s_l, _Inout_updates_z_(_Size) char, _Inout_z_, char, _String, _In_opt_ _locale_t, _Locale)
_Check_return_opt_ _CRTIMP size_t  __cdecl strxfrm (_Out_writes_opt_(_MaxCount) _Post_maybez_ char * _Dst, _In_z_ const char * _Src, _In_ size_t _MaxCount);
_Check_return_opt_ _CRTIMP size_t  __cdecl _strxfrm_l(_Out_writes_opt_(_MaxCount) _Post_maybez_ char * _Dst, _In_z_ const char * _Src, _In_ size_t _MaxCount, _In_opt_ _locale_t _Locale);

#ifdef __cplusplus
extern "C++" {
#ifndef _CPP_NARROW_INLINES_DEFINED
#define _CPP_NARROW_INLINES_DEFINED
_Check_return_ inline char * __CRTDECL strchr(_In_z_ char * _Str, _In_ int _Ch)
	{ return (char*)strchr((const char*)_Str, _Ch); }
_Check_return_ inline char * __CRTDECL strpbrk(_In_z_ char * _Str, _In_z_ const char * _Control)
	{ return (char*)strpbrk((const char*)_Str, _Control); }
_Check_return_ inline char * __CRTDECL strrchr(_In_z_ char * _Str, _In_ int _Ch)
	{ return (char*)strrchr((const char*)_Str, _Ch); }
_Check_return_ _Ret_maybenull_ inline char * __CRTDECL strstr(_In_z_ char * _Str, _In_z_ const char * _SubStr)
	{ return (char*)strstr((const char*)_Str, _SubStr); }
#endif
#ifndef _CPP_MEMCHR_DEFINED
#define _CPP_MEMCHR_DEFINED
_Check_return_ inline void * __CRTDECL memchr(_In_reads_bytes_opt_(_N) void * _Pv, _In_ int _C, _In_ size_t _N)
	{ return (void*)memchr((const void*)_Pv, _C, _N); }
#endif
}
#endif

#if     !__STDC__

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#pragma push_macro("strdup")
#undef strdup
#endif

_Check_return_ _CRT_NONSTDC_DEPRECATE(_strdup) _CRTIMP char * __cdecl strdup(_In_opt_z_ const char * _Src);

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#pragma pop_macro("strdup")
#endif

/* prototypes for oldnames.lib functions */
_Check_return_ _CRT_NONSTDC_DEPRECATE(_strcmpi) _CRTIMP int __cdecl strcmpi(_In_z_ const char * _Str1, _In_z_ const char * _Str2);
_Check_return_ _CRT_NONSTDC_DEPRECATE(_stricmp) _CRTIMP int __cdecl stricmp(_In_z_ const char * _Str1, _In_z_ const char * _Str2);
_CRT_NONSTDC_DEPRECATE(_strlwr) _CRTIMP char * __cdecl strlwr(_Inout_z_ char * _Str);
_Check_return_ _CRT_NONSTDC_DEPRECATE(_strnicmp) _CRTIMP int __cdecl strnicmp(_In_reads_or_z_(_MaxCount) const char * _Str1, _In_reads_or_z_(_MaxCount) const char * _Str, _In_ size_t _MaxCount);
_CRT_NONSTDC_DEPRECATE(_strnset) _CRTIMP char * __cdecl strnset(_Inout_updates_z_(_MaxCount) char * _Str, _In_ int _Val, _In_ size_t _MaxCount);
_CRT_NONSTDC_DEPRECATE(_strrev) _CRTIMP char * __cdecl strrev(_Inout_z_ char * _Str);
_CRT_NONSTDC_DEPRECATE(_strset)         char * __cdecl strset(_Inout_z_ char * _Str, _In_ int _Val);
_CRT_NONSTDC_DEPRECATE(_strupr) _CRTIMP char * __cdecl strupr(_Inout_z_ char * _Str);

#endif  /* !__STDC__ */


#ifndef _WSTRING_DEFINED

/* wide function prototypes, also declared in wchar.h  */

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#pragma push_macro("_wcsdup")
#undef _wcsdup
#endif

_Check_return_ _CRTIMP wchar_t * __cdecl _wcsdup(_In_z_ const wchar_t * _Str);

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#pragma pop_macro("_wcsdup")
#endif

#if __STDC_WANT_SECURE_LIB__
_Check_return_wat_ _CRTIMP_ALTERNATIVE errno_t __cdecl wcscat_s(_Inout_updates_z_(_SizeInWords) wchar_t * _Dst, _In_ rsize_t _SizeInWords, _In_z_ const wchar_t * _Src);
#endif
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_1(errno_t, wcscat_s, wchar_t, _Dest, _In_z_ const wchar_t *, _Source)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_1(wchar_t *, __RETURN_POLICY_DST, _CRTIMP, wcscat, _Inout_updates_z_(_String_length_(_Dest) + _String_length_(_Source) + 1), wchar_t, _Dest, _In_z_ const wchar_t *, _Source)
_Check_return_
_When_(return != NULL, _Ret_range_(_Str, _Str+_String_length_(_Str)-1))
_CRTIMP _CONST_RETURN wchar_t * __cdecl wcschr(_In_z_ const wchar_t * _Str, wchar_t _Ch);
_Check_return_ _CRTIMP int __cdecl wcscmp(_In_z_ const wchar_t * _Str1, _In_z_ const wchar_t * _Str2);
#if __STDC_WANT_SECURE_LIB__
_Check_return_wat_ _CRTIMP_ALTERNATIVE errno_t __cdecl wcscpy_s(_Out_writes_z_(_SizeInWords) wchar_t * _Dst, _In_ rsize_t _SizeInWords, _In_z_ const wchar_t * _Src);
#endif
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_1(errno_t, wcscpy_s, wchar_t, _Dest, _In_z_ const wchar_t *, _Source)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_1(wchar_t *, __RETURN_POLICY_DST, _CRTIMP, wcscpy, _Out_writes_z_(_String_length_(_Source) + 1), wchar_t, _Dest, _In_z_ const wchar_t *, _Source)
_Check_return_ _CRTIMP size_t __cdecl wcscspn(_In_z_ const wchar_t * _Str, _In_z_ const wchar_t * _Control);
_Check_return_ _CRTIMP size_t __cdecl wcslen(_In_z_ const wchar_t * _Str);
_Check_return_ _CRTIMP
_When_(_MaxCount > _String_length_(_Src), _Post_satisfies_(return == _String_length_(_Src)))
_When_(_MaxCount <= _String_length_(_Src), _Post_satisfies_(return == _MaxCount))
size_t __cdecl wcsnlen(_In_reads_or_z_(_MaxCount) const wchar_t * _Src, _In_ size_t _MaxCount);
#if __STDC_WANT_SECURE_LIB__ && !defined (__midl)
_Check_return_ static __inline
_When_(_MaxCount > _String_length_(_Src), _Post_satisfies_(return == _String_length_(_Src)))
_When_(_MaxCount <= _String_length_(_Src), _Post_satisfies_(return == _MaxCount))
size_t __CRTDECL wcsnlen_s(_In_reads_or_z_(_MaxCount) const wchar_t * _Src, _In_ size_t _MaxCount)
{
    return (_Src == NULL) ? 0 : wcsnlen(_Src, _MaxCount);
}
#endif
#if __STDC_WANT_SECURE_LIB__
_Check_return_wat_ _CRTIMP_ALTERNATIVE errno_t __cdecl wcsncat_s(_Inout_updates_z_(_SizeInWords) wchar_t * _Dst, _In_ rsize_t _SizeInWords, _In_reads_or_z_(_MaxCount) const wchar_t * _Src, _In_ rsize_t _MaxCount);
#endif
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_2(errno_t, wcsncat_s, _Prepost_z_ wchar_t, _Dest, _In_reads_or_z_(_Count) const wchar_t *, _Source, _In_ size_t, _Count)
#pragma warning(push)
#pragma warning(disable:6059)
__DEFINE_CPP_OVERLOAD_STANDARD_NFUNC_0_2_EX(wchar_t *, __RETURN_POLICY_DST, _CRTIMP, wcsncat, wcsncat_s, _Inout_updates_z_(_Size) wchar_t, _Inout_updates_z_(_Count), wchar_t, _Dest, _In_reads_or_z_(_Count) const wchar_t *, _Source, _In_ size_t, _Count)
#pragma warning(pop)
_Check_return_ _CRTIMP int __cdecl wcsncmp(_In_reads_or_z_(_MaxCount) const wchar_t * _Str1, _In_reads_or_z_(_MaxCount) const wchar_t * _Str2, _In_ size_t _MaxCount);
#if __STDC_WANT_SECURE_LIB__
_Check_return_wat_ _CRTIMP_ALTERNATIVE errno_t __cdecl wcsncpy_s(_Out_writes_z_(_SizeInWords) wchar_t * _Dst, _In_ rsize_t _SizeInWords, _In_reads_or_z_(_MaxCount) const wchar_t * _Src, _In_ rsize_t _MaxCount);
#endif
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_2(errno_t, wcsncpy_s, wchar_t, _Dest, _In_reads_or_z_(_Count) const wchar_t *, _Source, _In_ size_t, _Count)
__DEFINE_CPP_OVERLOAD_STANDARD_NFUNC_0_2_EX(wchar_t *, __RETURN_POLICY_DST, _CRTIMP, wcsncpy, wcsncpy_s, _Pre_notnull_ _Post_maybez_ wchar_t, _Out_writes_(_Count) _Post_maybez_, wchar_t, _Dest, _In_reads_or_z_(_Count) const wchar_t *, _Source, _In_ size_t, _Count)
_Check_return_ _CRTIMP _CONST_RETURN wchar_t * __cdecl wcspbrk(_In_z_ const wchar_t * _Str, _In_z_ const wchar_t * _Control);
_Check_return_ _CRTIMP _CONST_RETURN wchar_t * __cdecl wcsrchr(_In_z_ const wchar_t * _Str, _In_ wchar_t _Ch);
_Check_return_ _CRTIMP size_t __cdecl wcsspn(_In_z_ const wchar_t * _Str, _In_z_ const wchar_t * _Control);
_Check_return_ _Ret_maybenull_
_When_(return != NULL, _Ret_range_(_Str, _Str+_String_length_(_Str)-1))
_CRTIMP _CONST_RETURN wchar_t * __cdecl wcsstr(_In_z_ const wchar_t * _Str, _In_z_ const wchar_t * _SubStr);
_Check_return_ _CRT_INSECURE_DEPRECATE(wcstok_s) _CRTIMP wchar_t * __cdecl wcstok(_Inout_opt_z_ wchar_t * _Str, _In_z_ const wchar_t * _Delim);
#if __STDC_WANT_SECURE_LIB__
_Check_return_ _CRTIMP_ALTERNATIVE wchar_t * __cdecl wcstok_s(_Inout_opt_z_ wchar_t * _Str, _In_z_ const wchar_t * _Delim, _Inout_ _Deref_prepost_opt_z_ wchar_t ** _Context);
#endif
_Check_return_ _CRT_INSECURE_DEPRECATE(_wcserror_s) _CRTIMP wchar_t * __cdecl _wcserror(_In_ int _ErrNum);
_Check_return_wat_ _CRTIMP errno_t __cdecl _wcserror_s(_Out_writes_opt_z_(_SizeInWords) wchar_t * _Buf, _In_ size_t _SizeInWords, _In_ int _ErrNum);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_1(errno_t, _wcserror_s, wchar_t, _Buffer, _In_ int, _Error)
_Check_return_ _CRT_INSECURE_DEPRECATE(__wcserror_s) _CRTIMP wchar_t * __cdecl __wcserror(_In_opt_z_ const wchar_t * _Str);
_Check_return_wat_ _CRTIMP errno_t __cdecl __wcserror_s(_Out_writes_opt_z_(_SizeInWords) wchar_t * _Buffer, _In_ size_t _SizeInWords, _In_z_ const wchar_t * _ErrMsg);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_1(errno_t, __wcserror_s, wchar_t, _Buffer, _In_z_ const wchar_t *, _ErrorMessage)

_Check_return_ _CRTIMP int __cdecl _wcsicmp(_In_z_ const wchar_t * _Str1, _In_z_ const wchar_t * _Str2);
_Check_return_ _CRTIMP int __cdecl _wcsicmp_l(_In_z_ const wchar_t * _Str1, _In_z_ const wchar_t * _Str2, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _wcsnicmp(_In_reads_or_z_(_MaxCount) const wchar_t * _Str1, _In_reads_or_z_(_MaxCount) const wchar_t * _Str2, _In_ size_t _MaxCount);
_Check_return_ _CRTIMP int __cdecl _wcsnicmp_l(_In_reads_or_z_(_MaxCount) const wchar_t * _Str1, _In_reads_or_z_(_MaxCount) const wchar_t * _Str2, _In_ size_t _MaxCount, _In_opt_ _locale_t _Locale);
_Check_return_wat_ _CRTIMP_ALTERNATIVE errno_t __cdecl _wcsnset_s(_Inout_updates_z_(_SizeInWords) wchar_t * _Dst, _In_ size_t _SizeInWords, _In_ wchar_t _Val, _In_ size_t _MaxCount);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_2(errno_t, _wcsnset_s, _Prepost_z_ wchar_t, _Dst, wchar_t, _Val, _In_ size_t, _MaxCount)
__DEFINE_CPP_OVERLOAD_STANDARD_NFUNC_0_2_EX(wchar_t *, __RETURN_POLICY_DST, _CRTIMP, _wcsnset, _wcsnset_s, _Inout_updates_z_(_Size) wchar_t, _Inout_updates_z_(_MaxCount), wchar_t, _Str, wchar_t, _Val, _In_ size_t, _MaxCount)
_CRTIMP wchar_t * __cdecl _wcsrev(_Inout_z_ wchar_t * _Str);
_Check_return_wat_ _CRTIMP_ALTERNATIVE errno_t __cdecl _wcsset_s(_Inout_updates_z_(_SizeInWords) wchar_t * _Dst, _In_ size_t _SizeInWords, _In_ wchar_t _Value);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_1(errno_t, _wcsset_s, _Prepost_z_ wchar_t, _Str, wchar_t, _Val)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_1_EX(wchar_t *, __RETURN_POLICY_DST, _CRTIMP, _wcsset, _wcsset_s, _Inout_updates_z_(_Size) wchar_t, _Inout_z_, wchar_t, _Str, wchar_t, _Val)

_Check_return_wat_ _CRTIMP errno_t __cdecl _wcslwr_s(_Inout_updates_z_(_SizeInWords) wchar_t * _Str, _In_ size_t _SizeInWords);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_0(errno_t, _wcslwr_s, _Prepost_z_ wchar_t, _String)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_0(wchar_t *, __RETURN_POLICY_DST, _CRTIMP, _wcslwr, _Inout_z_, wchar_t, _String)
_Check_return_wat_ _CRTIMP errno_t __cdecl _wcslwr_s_l(_Inout_updates_z_(_SizeInWords) wchar_t * _Str, _In_ size_t _SizeInWords, _In_opt_ _locale_t _Locale);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_1(errno_t, _wcslwr_s_l, _Prepost_z_ wchar_t, _String, _In_opt_ _locale_t, _Locale)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_1_EX(wchar_t *, __RETURN_POLICY_DST, _CRTIMP, _wcslwr_l, _wcslwr_s_l, _Inout_updates_z_(_Size) wchar_t, _Inout_z_, wchar_t, _String, _In_opt_ _locale_t, _Locale)
_Check_return_wat_ _CRTIMP errno_t __cdecl _wcsupr_s(_Inout_updates_z_(_Size) wchar_t * _Str, _In_ size_t _Size);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_0(errno_t, _wcsupr_s, _Prepost_z_ wchar_t, _String)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_0(wchar_t *, __RETURN_POLICY_DST, _CRTIMP, _wcsupr, _Inout_z_, wchar_t, _String)
_Check_return_wat_ _CRTIMP errno_t __cdecl _wcsupr_s_l(_Inout_updates_z_(_Size) wchar_t * _Str, _In_ size_t _Size, _In_opt_ _locale_t _Locale);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_1(errno_t, _wcsupr_s_l, _Prepost_z_ wchar_t, _String, _In_opt_ _locale_t, _Locale)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_1_EX(wchar_t *, __RETURN_POLICY_DST, _CRTIMP, _wcsupr_l, _wcsupr_s_l, _Inout_updates_z_(_Size) wchar_t, _Inout_z_, wchar_t, _String, _In_opt_ _locale_t, _Locale)
_Check_return_opt_ _CRTIMP size_t __cdecl wcsxfrm(_Out_writes_opt_(_MaxCount) _Post_maybez_ wchar_t * _Dst, _In_z_ const wchar_t * _Src, _In_ size_t _MaxCount);
_Check_return_opt_ _CRTIMP size_t __cdecl _wcsxfrm_l(_Out_writes_opt_(_MaxCount) _Post_maybez_ wchar_t * _Dst, _In_z_ const wchar_t *_Src, _In_ size_t _MaxCount, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl wcscoll(_In_z_ const wchar_t * _Str1, _In_z_ const wchar_t * _Str2);
_Check_return_ _CRTIMP int __cdecl _wcscoll_l(_In_z_ const wchar_t * _Str1, _In_z_ const wchar_t * _Str2, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _wcsicoll(_In_z_ const wchar_t * _Str1, _In_z_ const wchar_t * _Str2);
_Check_return_ _CRTIMP int __cdecl _wcsicoll_l(_In_z_ const wchar_t * _Str1, _In_z_ const wchar_t *_Str2, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _wcsncoll(_In_reads_or_z_(_MaxCount) const wchar_t * _Str1, _In_reads_or_z_(_MaxCount) const wchar_t * _Str2, _In_ size_t _MaxCount);
_Check_return_ _CRTIMP int __cdecl _wcsncoll_l(_In_reads_or_z_(_MaxCount) const wchar_t * _Str1, _In_reads_or_z_(_MaxCount) const wchar_t * _Str2, _In_ size_t _MaxCount, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _wcsnicoll(_In_reads_or_z_(_MaxCount) const wchar_t * _Str1, _In_reads_or_z_(_MaxCount) const wchar_t * _Str2, _In_ size_t _MaxCount);
_Check_return_ _CRTIMP int __cdecl _wcsnicoll_l(_In_reads_or_z_(_MaxCount) const wchar_t * _Str1, _In_reads_or_z_(_MaxCount) const wchar_t * _Str2, _In_ size_t _MaxCount, _In_opt_ _locale_t _Locale);

#ifdef  __cplusplus
#ifndef _CPP_WIDE_INLINES_DEFINED
#define _CPP_WIDE_INLINES_DEFINED
extern "C++" {
_Check_return_
_When_(return != NULL, _Ret_range_(_Str, _Str+_String_length_(_Str)-1))
        inline wchar_t * __CRTDECL wcschr(_In_z_ wchar_t *_Str, wchar_t _Ch)
        {return ((wchar_t *)wcschr((const wchar_t *)_Str, _Ch)); }
_Check_return_ inline wchar_t * __CRTDECL wcspbrk(_In_z_ wchar_t *_Str, _In_z_ const wchar_t *_Control)
        {return ((wchar_t *)wcspbrk((const wchar_t *)_Str, _Control)); }
_Check_return_ inline wchar_t * __CRTDECL wcsrchr(_In_z_ wchar_t *_Str, _In_ wchar_t _Ch)
        {return ((wchar_t *)wcsrchr((const wchar_t *)_Str, _Ch)); }
_Check_return_ _Ret_maybenull_
_When_(return != NULL, _Ret_range_(_Str, _Str+_String_length_(_Str)-1))
        inline wchar_t * __CRTDECL wcsstr(_In_z_ wchar_t *_Str, _In_z_ const wchar_t *_SubStr)
        {return ((wchar_t *)wcsstr((const wchar_t *)_Str, _SubStr)); }
}
#endif
#endif

#if     !__STDC__

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#pragma push_macro("wcsdup")
#undef wcsdup
#endif

_Check_return_ _CRT_NONSTDC_DEPRECATE(_wcsdup) _CRTIMP wchar_t * __cdecl wcsdup(_In_z_ const wchar_t * _Str);

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#pragma pop_macro("wcsdup")
#endif

/* old names */
#define wcswcs wcsstr

/* prototypes for oldnames.lib functions */
_Check_return_ _CRT_NONSTDC_DEPRECATE(_wcsicmp) _CRTIMP int __cdecl wcsicmp(_In_z_ const wchar_t * _Str1, _In_z_ const wchar_t * _Str2);
_Check_return_ _CRT_NONSTDC_DEPRECATE(_wcsnicmp) _CRTIMP int __cdecl wcsnicmp(_In_reads_or_z_(_MaxCount) const wchar_t * _Str1, _In_reads_or_z_(_MaxCount) const wchar_t * _Str2, _In_ size_t _MaxCount);
_CRT_NONSTDC_DEPRECATE(_wcsnset) _CRTIMP wchar_t * __cdecl wcsnset(_Inout_updates_z_(_MaxCount) wchar_t * _Str, _In_ wchar_t _Val, _In_ size_t _MaxCount);
_CRT_NONSTDC_DEPRECATE(_wcsrev) _CRTIMP wchar_t * __cdecl wcsrev(_Inout_z_ wchar_t * _Str);
_CRT_NONSTDC_DEPRECATE(_wcsset) _CRTIMP wchar_t * __cdecl wcsset(_Inout_z_ wchar_t * _Str, wchar_t _Val);
_CRT_NONSTDC_DEPRECATE(_wcslwr) _CRTIMP wchar_t * __cdecl wcslwr(_Inout_z_ wchar_t * _Str);
_CRT_NONSTDC_DEPRECATE(_wcsupr) _CRTIMP wchar_t * __cdecl wcsupr(_Inout_z_ wchar_t * _Str);
_Check_return_ _CRT_NONSTDC_DEPRECATE(_wcsicoll) _CRTIMP int __cdecl wcsicoll(_In_z_ const wchar_t * _Str1, _In_z_ const wchar_t * _Str2);

#endif  /* !__STDC__ */

#define _WSTRING_DEFINED
#endif

#ifndef _INTERNAL_IFSTRIP_
_Check_return_ int __cdecl __ascii_memicmp(_In_reads_bytes_opt_(_Size) const void * _Buf1, _In_reads_bytes_opt_(_Size) const void * _Buf2, _In_ size_t _Size);
_Check_return_ int __cdecl __ascii_stricmp(_In_z_ const char * _Str1, _In_z_ const char * _Str2);
_Check_return_ int __cdecl __ascii_strnicmp(_In_reads_or_z_(_MaxCount) const char * _Str1, _In_reads_or_z_(_MaxCount) const char * _Str2, _In_ size_t _MaxCount);
#endif  /* _INTERNAL_IFSTRIP_ */

#ifdef  __cplusplus
}
#endif

#endif  /* _INC_STRING */
