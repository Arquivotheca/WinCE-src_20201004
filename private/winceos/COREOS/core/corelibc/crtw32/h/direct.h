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
*direct.h - function declarations for directory handling/creation
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This include file contains the function declarations for the library
*       functions related to directory handling and creation.
*
*       [Public]
*
*Revision History:
*       12/11/87  JCR   Added "_loadds" functionality
*       12-18-87  JCR   Added _FAR_ to declarations
*       02-10-88  JCR   Cleaned up white space
*       08-22-88  GJF   Modified to also work with the 386 (small model only)
*       01-31-89  JCR   Added _chdrive, _fullpath, _getdrive, _getdcwd
*       05-03-89  JCR   Added _INTERNAL_IFSTRIP for relinc usage
*       07-28-89  GJF   Cleanup, now specific to OS/2 2.0 (i.e., 386 flat model)
*       10-30-89  GJF   Fixed copyright
*       11-02-89  JCR   Changed "DLL" to "_DLL"
*       11-17-89  GJF   Moved _fullpath prototype to stdlib.h. Added const
*                       attrib. to arg types for chdir, mkdir, rmdir
*       02-28-90  GJF   Added #ifndef _INC_DIRECT and #include <cruntime.h>
*                       stuff. Also, removed some (now) useless preprocessor
*                       directives.
*       03-21-90  GJF   Replaced _cdecl with _CALLTYPE1 or _CALLTYPE2 in
*                       prototypes.
*       03-30-90  GJF   Now all are _CALLTYPE1.
*       01-17-91  GJF   ANSI naming.
*       08-20-91  JCR   C++ and ANSI naming
*       08-26-91  BWM   Added _diskfree_t, _getdiskfree, and
*       09-26-91  JCR   Non-ANSI alias is for getcwd, not getDcwd (oops)
*       09-28-91  JCR   ANSI names: DOSX32=prototypes, WIN32=#defines for now
*       04-29-92  GJF   Added _getdcwd_nolock for Win32.
*       08-07-92  GJF   Function calling type and variable type macros.
*       01-21-93  GJF   Removed support for C6-386's _cdecl.
*       04-06-93  SKS   Replace _CRTAPI1/2 with __cdecl, _CRTVAR1 with nothing
*       04-07-93  SKS   Add _CRTIMP keyword for CRT DLL model
*                       Use link-time aliases for old names, not #define's
*       04-08-93  SKS   Fix oldnames prototype for getcwd()
*       09-01-93  GJF   Merged Cuda and NT SDK versions.
*       12-07-93  CFW   Add wide char version protos.
*       11-03-94  GJF   Ensure 8 byte alignment.
*       12-15-94  XY    merged with mac header
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
*       08-01-03  AC    Added support for mode _dbg functions (a la _malloc_dbg)
*       10-10-04  ESC   Use _CRT_PACKING
*       11-08-04  JL    Added _CRT_NONSTDC_DEPRECATE to deprecate non-ANSI functions
*       11-18-04  ATC   SAL Annotations for CRT Headers
*       01-14-05  AC    Fix SAL annotations (using prefast espx plug-in)
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*                       Packing fix
*
****/

#pragma once

#ifndef _INC_DIRECT
#define _INC_DIRECT

#include <crtdefs.h>

/*
 * Currently, all MS C compilers for Win32 platforms default to 8 byte
 * alignment.
 */
#pragma pack(push,_CRT_PACKING)

#ifdef  __cplusplus
extern "C" {
#endif

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP

/* _getdiskfree structure for _getdiskfree() */
#ifndef _DISKFREE_T_DEFINED

struct _diskfree_t {
        unsigned total_clusters;
        unsigned avail_clusters;
        unsigned sectors_per_cluster;
        unsigned bytes_per_sector;
        };

#define _DISKFREE_T_DEFINED
#endif

#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

/* function prototypes */

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#pragma push_macro("_getcwd")
#pragma push_macro("_getdcwd")
#undef _getcwd
#undef _getdcwd
#endif

_Check_return_ _Ret_maybenull_z_ _CRTIMP char * __cdecl _getcwd(_Out_writes_opt_(_SizeInBytes) char * _DstBuf, _In_ int _SizeInBytes);
_Check_return_ _Ret_maybenull_z_ _CRTIMP char * __cdecl _getdcwd(_In_ int _Drive, _Out_writes_opt_(_SizeInBytes) char * _DstBuf, _In_ int _SizeInBytes);
#define  _getdcwd_nolock  _getdcwd

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#pragma pop_macro("_getcwd")
#pragma pop_macro("_getdcwd")
#endif

_Check_return_ _CRTIMP int __cdecl _chdir(_In_z_ const char * _Path);

#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

_Check_return_ _CRTIMP int __cdecl _mkdir(_In_z_ const char * _Path);
_Check_return_ _CRTIMP int __cdecl _rmdir(_In_z_ const char * _Path);

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP

_Check_return_ _CRTIMP int __cdecl _chdrive(_In_ int _Drive);
_Check_return_ _CRTIMP int __cdecl _getdrive(void);
_Check_return_ _CRTIMP unsigned long __cdecl _getdrives(void);

#ifndef _GETDISKFREE_DEFINED
_Check_return_ _CRTIMP unsigned __cdecl _getdiskfree(_In_ unsigned _Drive, _Out_ struct _diskfree_t * _DiskFree);
#define _GETDISKFREE_DEFINED
#endif

#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

#ifndef _WDIRECT_DEFINED

/* wide function prototypes, also declared in wchar.h  */

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#pragma push_macro("_wgetcwd")
#pragma push_macro("_wgetdcwd")
#undef _wgetcwd
#undef _wgetdcwd
#endif

_Check_return_ _Ret_maybenull_z_ _CRTIMP wchar_t * __cdecl _wgetcwd(_Out_writes_opt_(_SizeInWords) wchar_t * _DstBuf, _In_ int _SizeInWords);
_Check_return_ _Ret_maybenull_z_ _CRTIMP wchar_t * __cdecl _wgetdcwd(_In_ int _Drive, _Out_writes_opt_(_SizeInWords) wchar_t * _DstBuf, _In_ int _SizeInWords);
#define  _wgetdcwd_nolock  _wgetdcwd

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#pragma pop_macro("_wgetcwd")
#pragma pop_macro("_wgetdcwd")
#endif

_Check_return_ _CRTIMP int __cdecl _wchdir(_In_z_ const wchar_t * _Path);

#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

_Check_return_ _CRTIMP int __cdecl _wmkdir(_In_z_ const wchar_t * _Path);
_Check_return_ _CRTIMP int __cdecl _wrmdir(_In_z_ const wchar_t * _Path);

#define _WDIRECT_DEFINED
#endif

#if     !__STDC__

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP

/* Non-ANSI names for compatibility */
#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#pragma push_macro("getcwd")
#undef getcwd
#endif

_Check_return_ _Ret_maybenull_z_ _CRT_NONSTDC_DEPRECATE(_getcwd) _CRTIMP char * __cdecl getcwd(_Out_writes_opt_(_SizeInBytes) char * _DstBuf, _In_ int _SizeInBytes);

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#pragma pop_macro("getcwd")
#endif

_Check_return_ _CRT_NONSTDC_DEPRECATE(_chdir) _CRTIMP int __cdecl chdir(_In_z_ const char * _Path);

#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

_Check_return_ _CRT_NONSTDC_DEPRECATE(_mkdir) _CRTIMP int __cdecl mkdir(_In_z_ const char * _Path);
_Check_return_ _CRT_NONSTDC_DEPRECATE(_rmdir) _CRTIMP int __cdecl rmdir(_In_z_ const char * _Path);

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP
#define diskfree_t  _diskfree_t
#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

#endif  /* __STDC__ */

#ifdef  __cplusplus
}
#endif

#pragma pack(pop)

#endif  /* _INC_DIRECT */
