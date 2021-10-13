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
*locale.h - definitions/declarations for localization routines
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This file defines the structures, values, macros, and functions
*       used by the localization routines.
*
*       [Public]
*
*Revision History:
*       03-21-89  JCR   Module created.
*       03-11-89  JCR   Modified for 386.
*       04-06-89  JCR   Corrected lconv definition (don't use typedef)
*       04-18-89  JCR   Added _LCONV_DEFINED so locale.h can be included twice
*       05-03-89  JCR   Added _INTERNAL_IFSTRIP for relinc usage
*       08-04-89  GJF   Cleanup, now specific to OS/2 2.0 (i.e., 386 flat model)
*       10-30-89  GJF   Fixed copyright, removed dummy args from prototype
*       11-02-89  JCR   Changed "DLL" to "_DLL"
*       03-01-90  GJF   Added #ifndef _INC_LOCALE and #include <cruntime.h>
*                       stuff. Also, removed some (now) useless preprocessor
*                       directives.
*       03-15-90  GJF   Replaced _cdecl with _CALLTYPE1 in prototypes.
*       11-12-90  GJF   Changed NULL to (void *)0.
*       02-12-91  GJF   Only #define NULL if it isn't #define-d.
*       08-20-91  JCR   C++ and ANSI naming
*       08-05-92  GJF   Function calling type and variable type macros.
*       12-29-92  CFW   Added _lc_time_data definition and supporting #defines.
*       01-21-93  GJF   Removed support for C6-386's _cdecl.
*       02-01-93  CFW   Removed __c_lconvinit vars to locale.h.
*       02-08-93  CFW   Removed time definitions to setlocal.h.
*       04-06-93  SKS   Replace _CRTAPI1/2 with __cdecl, _CRTVAR1 with nothing
*       04-07-93  SKS   Add _CRTIMP keyword for CRT DLL model
*       04-13-93  CFW   Add _charmax (when compiled -J, _CHAR_UNSIGNED is defined,
*                       and _charmax yanks in module to allow lconv struct members
*                       to be set to UCHAR_MAX).
*       04-14-93  CFW   Change _charmax from short to int.
*       10-07-93  GJF   Merged Cuda and NT versions.
*       12-17-93  CFW   Add wide char version protos.
*       11-03-94  GJF   Ensure 8 byte alignment.
*       02-11-95  CFW   Add _CRTBLD to avoid users getting wrong headers.
*       02-14-95  CFW   Clean up Mac merge.
*       12-14-95  JWM   Add "#pragma once".
*       02-20-97  GJF   Cleaned out obsolete support for _CRTAPI* and _NTSDK.
*                       Also, detab-ed.
*       09-30-97  JWM   Restored not-so-obsolete _CRTAPI1 support.
*       05-13-99  PML   Remove _CRTAPI1
*       05-17-99  PML   Remove all Macintosh support.
*       03-24-04  MSL   Fixed declarations to work with /clr:pure
*                       VSW#257801
*       09-08-04  AC    Renamed __get_current_locale to _get_current_locale;
*                       renamed __create_locale to _create_locale;
*                       renamed __free_locale to _free_locale;
*                       the old versions will be removed in the next LKG.
*                       VSW#247045
*       09-28-04  AC    Removed __get_current_locale, __create_locale, __free_locale
*                       VSW#365372
*       10-10-04  ESC   Use _CRT_PACKING
*       11-18-04  ATC   SAL Annotations for CRT Headers
*       01-14-05  AC    Fix SAL annotations (using prefast espx plug-in)
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*                       Packing fix
*       04-14-05  MSL   Intellisense locale name cleanup
*       05-27-05  MSL   64 bit definition of NULL
*       06-03-05  MSL   Revert 64 bit definition of NULL due to calling-convention fix
*
****/

#pragma once

#ifndef _INC_LOCALE
#define _INC_LOCALE

#include <crtdefs.h>

/*
 * Currently, all MS C compilers for Win32 platforms default to 8 byte
 * alignment.
 */
#pragma pack(push,_CRT_PACKING)

#ifdef  __cplusplus
extern "C" {
#endif

/* define NULL pointer value */
#ifndef NULL
#ifdef __cplusplus
#define NULL    0
#else
#define NULL    ((void *)0)
#endif
#endif

/* Locale categories */

#define LC_ALL          0
#define LC_COLLATE      1
#define LC_CTYPE        2
#define LC_MONETARY     3
#define LC_NUMERIC      4
#define LC_TIME         5

#define LC_MIN          LC_ALL
#define LC_MAX          LC_TIME

/* Locale convention structure */

#ifndef _LCONV_DEFINED
struct lconv {
        char *decimal_point;
        char *thousands_sep;
        char *grouping;
        char *int_curr_symbol;
        char *currency_symbol;
        char *mon_decimal_point;
        char *mon_thousands_sep;
        char *mon_grouping;
        char *positive_sign;
        char *negative_sign;
        char int_frac_digits;
        char frac_digits;
        char p_cs_precedes;
        char p_sep_by_space;
        char n_cs_precedes;
        char n_sep_by_space;
        char p_sign_posn;
        char n_sign_posn;
        wchar_t *_W_decimal_point;
        wchar_t *_W_thousands_sep;
        wchar_t *_W_int_curr_symbol;
        wchar_t *_W_currency_symbol;
        wchar_t *_W_mon_decimal_point;
        wchar_t *_W_mon_thousands_sep;
        wchar_t *_W_positive_sign;
        wchar_t *_W_negative_sign;
        };
#define _LCONV_DEFINED
#endif

/* ANSI: char lconv members default is CHAR_MAX which is compile time
   dependent. Defining and using _charmax here causes CRT startup code
   to initialize lconv members properly */

#ifdef  _CHAR_UNSIGNED
extern int _charmax;
extern __inline int __dummy(void) { return _charmax; }
#endif

/* function prototypes */

#ifndef _CONFIG_LOCALE_SWT
#define _ENABLE_PER_THREAD_LOCALE           0x1
#define _DISABLE_PER_THREAD_LOCALE          0x2
#define _ENABLE_PER_THREAD_LOCALE_GLOBAL    0x10
#define _DISABLE_PER_THREAD_LOCALE_GLOBAL   0x20
#define _ENABLE_PER_THREAD_LOCALE_NEW       0x100
#define _DISABLE_PER_THREAD_LOCALE_NEW      0x200
#define _CONFIG_LOCALE_SWT
#endif

//#if _PTD_LOCALE_SUPPORT
_Check_return_opt_ _CRTIMP int __cdecl _configthreadlocale(_In_ int _Flag);
//#endif /* _PTD_LOCALE_SUPPORT */
_Check_return_opt_ _CRTIMP char * __cdecl setlocale(_In_ int _Category, _In_opt_z_ const char * _Locale);
_Check_return_opt_ _CRTIMP struct lconv * __cdecl localeconv(void);
_Check_return_opt_ _CRTIMP _locale_t __cdecl _get_current_locale(void);
_Check_return_opt_ _CRTIMP _locale_t __cdecl _create_locale(_In_ int _Category, _In_z_ const char * _Locale);
_CRTIMP void __cdecl _free_locale(_In_opt_ _locale_t _Locale);

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP
/* use _get_current_locale, _create_locale and _free_locale, instead of these functions with double leading underscore */
_Check_return_ _CRT_OBSOLETE(_get_current_locale) _CRTIMP _locale_t __cdecl __get_current_locale(void);
_Check_return_ _CRT_OBSOLETE(_create_locale) _CRTIMP _locale_t __cdecl __create_locale(_In_ int _Category, _In_z_ const char * _Locale);
_CRT_OBSOLETE(_free_locale) _CRTIMP void __cdecl __free_locale(_In_opt_ _locale_t _Locale);
#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

#ifndef _WLOCALE_DEFINED

/* wide function prototypes, also declared in wchar.h  */

_Check_return_opt_ _CRTIMP wchar_t * __cdecl _wsetlocale(_In_ int _Category, _In_opt_z_ const wchar_t * _Locale);
_Check_return_opt_ _CRTIMP _locale_t __cdecl _wcreate_locale(_In_ int _Category, _In_z_ const wchar_t * _Locale);

#define _WLOCALE_DEFINED
#endif

#ifdef  __cplusplus
}
#endif

#pragma pack(pop)

#endif  /* _INC_LOCALE */
