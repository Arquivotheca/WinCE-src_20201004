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
*search.h - declarations for searcing/sorting routines
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This file contains the declarations for the sorting and
*       searching routines.
*       [System V]
*
*       [Public]
*
*Revision History:
*       10/20/87  JCR   Removed "MSC40_ONLY" entries
*       12-11-87  JCR   Added "_loadds" functionality
*       12-18-87  JCR   Added _FAR_ to declarations
*       01-21-88  JCR   Removed _LOAD_DS from declarations
*       02-10-88  JCR   Cleaned up white space
*       08-22-88  GJF   Modified to also work for the 386 (small model only)
*       05-03-89  JCR   Added _INTERNAL_IFSTRIP for relinc usage
*       08-01-89  GJF   Cleanup, now specific to OS/2 2.0 (i.e., 386 flat model)
*       10-30-89  GJF   Fixed copyright
*       11-02-89  JCR   Changed "DLL" to "_DLL"
*       11-17-89  GJF   Changed arg types to be consistently "[const] void *"
*                       (same as 06-05-89 change to CRT version)
*       03-01-90  GJF   Added #ifndef _INC_SEARCH and #include <cruntime.h>
*                       stuff. Also, removed some (now) useless preprocessor
*                       directives.
*       03-21-90  GJF   Replaced _cdecl with _CALLTYPE1 in prototypes.
*       01-17-91  GJF   ANSI naming.
*       08-20-91  JCR   C++ and ANSI naming
*       09-28-91  JCR   ANSI names: DOSX32=prototypes, WIN32=#defines for now
*       08-05-92  GJF   Function calling type and variable type macros.
*       01-21-93  GJF   Removed support for C6-386's _cdecl.
*       04-06-93  SKS   Replace _CRTAPI1/2 with __cdecl, _CRTVAR1 with nothing
*       04-07-93  SKS   Add _CRTIMP keyword for CRT DLL model
*                       Use link-time aliases for old names, not #define's
*       10-11-93  GJF   Merged Cuda and NT versions.
*       12-28-94  JCF   Merged with mac header.
*       02-11-95  CFW   Add _CRTBLD to avoid users getting wrong headers.
*       02-14-95  CFW   Clean up Mac merge.
*       12-14-95  JWM   Add "#pragma once".
*       02-20-97  GJF   Cleaned out obsolete support for _CRTAPI* and _NTSDK.
*                       Also, detab-ed.
*       09-30-97  JWM   Restored not-so-obsolete _CRTAPI1 support.
*       12-15-98  GJF   Changes for 64-bit size_t.
*       05-13-99  PML   Remove _CRTAPI1
*       05-17-99  PML   Remove all Macintosh support.
*       10-06-99  PML   Add _W64 modifier to types which are 32 bits in Win32,
*                       64 bits in Win64.
*       08-11-03  AC    Added safe version (qsort_s, bsearch_s, lsearch_s, _lfind_s)
*       10-14-04  MAL   Undeprecate _lsearch_s, _lfind_s.
*       10-31-04  MSL   Add missing __clrcall versions of functions
*       11-08-04  JL    Added _CRT_NONSTDC_DEPRECATE to deprecate non-ANSI functions
*       11-18-04  ATC   SAL Annotations for CRT Headers
*       01-14-05  AC    Fix SAL annotations (using prefast espx plug-in)
*       02-10-05  AC    Fix _lfind_s and _lsearch_s prototypes in _M_CEE
*                       VSW#454149
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*                       Packing fix
*       04-14-05  MSL   Compile clean under MIDL
*
****/

#pragma once

#ifndef _INC_SEARCH
#define _INC_SEARCH

#include <crtdefs.h>
#include <stddef.h>

#ifdef  __cplusplus
extern "C" {
#endif

/* Function prototypes */

#ifndef _CRT_ALGO_DEFINED
#define _CRT_ALGO_DEFINED
#if __STDC_WANT_SECURE_LIB__
_Check_return_ _CRTIMP void * __cdecl bsearch_s(_In_ const void * _Key, _In_reads_bytes_(_NumOfElements * _SizeOfElements) const void * _Base, 
        _In_ rsize_t _NumOfElements, _In_ rsize_t _SizeOfElements,
        _In_ int (__cdecl * _PtFuncCompare)(void *, const void *, const void *), void * _Context);
#endif
_Check_return_ _CRTIMP void * __cdecl bsearch(_In_ const void * _Key, _In_reads_bytes_(_NumOfElements * _SizeOfElements) const void * _Base, 
        _In_ size_t _NumOfElements, _In_ size_t _SizeOfElements,
        _In_ int (__cdecl * _PtFuncCompare)(const void *, const void *));

#if __STDC_WANT_SECURE_LIB__
_CRTIMP void __cdecl qsort_s(_Inout_updates_bytes_(_NumOfElements* _SizeOfElements) void * _Base, 
        _In_ rsize_t _NumOfElements, _In_ rsize_t _SizeOfElements,
        _In_ int (__cdecl * _PtFuncCompare)(void *, const void *, const void *), void *_Context);
#endif
_CRTIMP void __cdecl qsort(_Inout_updates_bytes_(_NumOfElements * _SizeOfElements) void * _Base, 
	_In_ size_t _NumOfElements, _In_ size_t _SizeOfElements, 
        _In_ int (__cdecl * _PtFuncCompare)(const void *, const void *));
#endif

_Check_return_ _CRTIMP void * __cdecl _lfind_s(_In_ const void * _Key, _In_reads_bytes_((*_NumOfElements) * _SizeOfElements) const void * _Base,
        _Inout_ unsigned int * _NumOfElements, _In_ size_t _SizeOfElements, 
        _In_ int (__cdecl * _PtFuncCompare)(void *, const void *, const void *), void * _Context);
_Check_return_ _CRTIMP void * __cdecl _lfind(_In_ const void * _Key, _In_reads_bytes_((*_NumOfElements) * _SizeOfElements) const void * _Base, 
        _Inout_ unsigned int * _NumOfElements, _In_ unsigned int _SizeOfElements, 
	_In_ int (__cdecl * _PtFuncCompare)(const void *, const void *));

_Check_return_ _CRTIMP void * __cdecl _lsearch_s(_In_ const void * _Key, _Inout_updates_bytes_((*_NumOfElements ) * _SizeOfElements) void  * _Base, 
        _Inout_ unsigned int * _NumOfElements, _In_ size_t _SizeOfElements, 
	_In_ int (__cdecl * _PtFuncCompare)(void *, const void *, const void *), void * _Context);
_Check_return_ _CRTIMP void * __cdecl _lsearch(_In_ const void * _Key, _Inout_updates_bytes_((*_NumOfElements ) * _SizeOfElements) void  * _Base, 
        _Inout_ unsigned int * _NumOfElements, _In_ unsigned int _SizeOfElements, 
	_In_ int (__cdecl * _PtFuncCompare)(const void *, const void *));

#if defined(__cplusplus) && defined(_M_CEE) /*IFSTRIP=IGN*/
/*
 * Managed search routines. Note __cplusplus, this is because we only support
 * managed C++.
 */
extern "C++"
{

#if __STDC_WANT_SECURE_LIB__
_Check_return_ void * __clrcall bsearch_s(_In_ const void * _Key, _In_reads_bytes_(_NumOfElements*_SizeOfElements) const void * _Base, 
        _In_ rsize_t _NumOfElements, _In_ rsize_t _SizeOfElements, 
        _In_ int (__clrcall * _PtFuncCompare)(void *, const void *, const void *), void * _Context);
#endif
_Check_return_ void * __clrcall bsearch(_In_ const void * _Key, _In_reads_bytes_(_NumOfElements*_SizeOfElements) const void * _Base, 
        _In_ size_t _NumOfElements, _In_ size_t _SizeOfElements,
        _In_ int (__clrcall * _PtFuncCompare)(const void *, const void *));

_Check_return_ void * __clrcall _lfind_s(_In_ const void * _Key, _In_reads_bytes_(_NumOfElements*_SizeOfElements) const void * _Base, 
        _Inout_ unsigned int * _NumOfElements, _In_ size_t _SizeOfElements, 
        _In_ int (__clrcall * _PtFuncCompare)(void *, const void *, const void *), void * _Context);
_Check_return_ void * __clrcall _lfind(_In_ const void * _Key, _In_reads_bytes_((*_NumOfElements)*_SizeOfElements) const void * _Base, 
        _Inout_ unsigned int * _NumOfElements, _In_ unsigned int _SizeOfElements,
        _In_ int (__clrcall * _PtFuncCompare)(const void *, const void *));

_Check_return_ void * __clrcall _lsearch_s(_In_ const void * _Key, _In_reads_bytes_((*_NumOfElements)*_SizeOfElements) void * _Base, 
        _In_ unsigned int * _NumOfElements, _In_ size_t _SizeOfElements, 
        _In_ int (__clrcall * _PtFuncCompare)(void *, const void *, const void *), void * _Context);
_Check_return_ void * __clrcall _lsearch(_In_ const void * _Key, _Inout_updates_bytes_((*_NumOfElements)*_SizeOfElements) void * _Base, 
        _Inout_ unsigned int * _NumOfElements, _In_ unsigned int _SizeOfElements,
        _In_ int (__clrcall * _PtFuncCompare)(const void *, const void *));

#if __STDC_WANT_SECURE_LIB__
void __clrcall qsort_s(_Inout_updates_bytes_(_NumOfElements*_SizeOfElements) void * _Base, 
        _In_ rsize_t _NumOfElements, _In_ rsize_t _SizeOfElements, 
        _In_ int (__clrcall * _PtFuncCompare)(void *, const void *, const void *), void * _Context);
#endif
void __clrcall qsort(_Inout_updates_bytes_(_NumOfElements*_SizeOfElements) void * _Base, 
        _In_ size_t _NumOfElements, _In_ size_t _SizeOfElements, 
        _In_ int (__clrcall * _PtFuncCompare)(const void *, const void *));

}
#endif


#if     !__STDC__
/* Non-ANSI names for compatibility */

_Check_return_ _CRTIMP _CRT_NONSTDC_DEPRECATE(_lfind) void * __cdecl lfind(_In_ const void * _Key, _In_reads_bytes_((*_NumOfElements) * _SizeOfElements) const void * _Base,
        _Inout_ unsigned int * _NumOfElements, _In_ unsigned int _SizeOfElements, 
	_In_ int (__cdecl * _PtFuncCompare)(const void *, const void *));
_Check_return_ _CRTIMP _CRT_NONSTDC_DEPRECATE(_lsearch) void * __cdecl lsearch(_In_ const void * _Key, _Inout_updates_bytes_((*_NumOfElements) * _SizeOfElements) void  * _Base,
        _Inout_ unsigned int * _NumOfElements, _In_ unsigned int _SizeOfElements, 
	_In_ int (__cdecl * _PtFuncCompare)(const void *, const void *));

#endif  /* __STDC__ */


#ifdef  __cplusplus
}
#endif

#endif  /* _INC_SEARCH */
