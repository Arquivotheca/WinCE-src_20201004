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
*ctype.h - character conversion macros and ctype macros
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines macros for character classification/conversion.
*       [ANSI/System V]
*
*       [Public]
*
*Revision History:
*       07-31-87  PHG   changed (unsigned char)(c) to (0xFF & (c)) to
*                       suppress -W2 warning
*       08-07-87  SKS   Removed (0xFF & (c)) -- is????() functions take an (int)
*       12-18-87  JCR   Added _FAR_ to declarations
*       01-19-87  JCR   DLL routines
*       02-10-88  JCR   Cleaned up white space
*       08-19-88  GJF   Modify to also work for the 386 (small model only)
*       12-08-88  JCR   DLL now access _ctype directly (removed DLL routines)
*       03-26-89  GJF   Brought into sync with CRT\H\CTYPE.H
*       05-03-89  JCR   Added _INTERNAL_IFSTRIP for relinc usage
*       07-28-89  GJF   Cleanup, now specific to OS/2 2.0 (i.e., 386 flat model)
*       10-30-89  GJF   Fixed copyright, removed dummy args from prototypes
*       11-02-89  JCR   Changed "DLL" to "_DLL"
*       02-28-90  GJF   Added #ifndef _INC_CTYPE and #include <cruntime.h>
*                       stuff. Also, removed #ifndef _CTYPE_DEFINED stuff and
*                       some other (now) useless preprocessor directives.
*       03-22-90  GJF   Replaced _cdecl with _CALLTYPE1 in prototypes and
*                       with _VARTYPE1 in variable declarations.
*       01-16-91  GJF   ANSI naming.
*       03-21-91  KRS   Added isleadbyte macro.
*       08-20-91  JCR   C++ and ANSI naming
*       09-28-91  JCR   ANSI names: DOSX32=prototypes, WIN32=#defines for now
*       10-11-91  ETC   All under _INTL: isleadbyte/isw* macros, prototypes;
*                       new is* macros; add wchar_t typedef; some intl defines.
*       12-17-91  ETC   ctype width now independent of _INTL, leave original
*                       short ctype table under _NEWCTYPETABLE.
*       01-22-92  GJF   Changed definition of _ctype for users of crtdll.dll.
*       04-06-92  KRS   Changes for new ISO proposal.
*       08-07-92  GJF   Function calling type and variable type macros.
*       10-26-92  GJF   Fixed _pctype and _pwctype for crtdll.
*       01-19-93  CFW   Move to _NEWCTYPETABLE, remove switch.
*       01-21-93  GJF   Removed support for C6-386's _cdecl.
*       02-17-93  CFW   Removed incorrect UNDONE comment and unused code.
*       02-18-93  CFW   Clean up common _WCTYPE_DEFINED section.
*       03-25-93  CFW   _toupper\_tolower now defined when _INTL.
*       04-06-93  SKS   Replace _CRTAPI1/2 with __cdecl, _CRTVAR1 with nothing
*       04-07-93  SKS   Add _CRTIMP keyword for CRT DLL model
*                       Use link-time aliases for old names, not #define's
*       04-12-93  CFW   Change is*, isw* macros to evaluate args only once.
*       04-14-93  CFW   Simplify MB_CUR_MAX def.
*       05-05-93  CFW   Change is_wctype to iswctype as per ISO.
*       09-01-93  GJF   Merged Cuda and NT SDK versions.
*       10-14-93  SRW   Add support for _CTYPE_DISABLE_MACROS symbol
*       11-11-93  GJF   Merged in change above (10-14-93).
*       11-22-93  CFW   Wide stuff must be under !__STDC__.
*       11-30-93  CFW   Change is_wctype from #define to proto.
*       12-07-93  CFW   Move wide defs outside __STDC__ check.
*       02-07-94  CFW   Move _isctype proto.
*       04-08-94  CFW   Optimize isleadbyte.
*       04-11-94  GJF   Made MB_CUR_MAX, _pctype and _pwctype into deferences
*                       of function returns for _DLL (for compatiblity with
*                       the Win32s version of msvcrt*.dll). Also,
*                       conditionally include win32s.h for DLL_FOR_WIN32S.
*       05-03-94  GJF   Made declarations of MB_CUR_MAX, _pctype and _pwctype
*                       for _DLL also conditional on _M_IX86.
*       10-18-94  GJF   Added prototypes and macros for _tolower_nolock,
*                       _toupper_nolock, _towlower_nolock and _towupper_nolock.
*       02-11-95  CFW   Add _CRTBLD to avoid users getting wrong headers.
*       02-14-95  CFW   Clean up Mac merge.
*       04-03-95  JCF   Remove #ifdef _WIN32 around wchar_t.
*       10-16-95  GJF   Define _to[w][lower|upper]_lk to be to[w][lower|upper]
*                       for DLL_FOR_WIN32S.
*       12-14-95  JWM   Add "#pragma once".
*       02-22-96  JWM   Merge in PlumHall mods.
*       01-21-97  GJF   Cleaned out obsolete support for Win32s, _NTSDK and
*                       _CRTAPI*. Fixed prototype for __p_pwctype(). Also,
*                       detab-ed.
*       08-14-97  GJF   Strip __p_* prototypes from release version.
*       09-30-97  JWM   Restored not-so-obsolete _CRTAPI1 support.
*       09-10-98  GJF   Added support for per-thread locale information.
*       05-13-99  PML   Remove _CRTAPI1
*       05-17-99  PML   Remove all Macintosh support.
*       10-25-99  GB    Added chvalidator for debug version. VS7#5695
*       11-08-99  PML   wctype_t is unsigned short, not wchar_t - it's a set
*                       of bitflags, not a wide char.
*       07-20-00  GB    typedefed wint_t to unsigned short
*       08-18-00  GB    changed MACRO __ascii_iswalpha to just work 'A'-'Z'
*                       and 'a' to 'z'.
*       09-06-00  GB    declared _ctype, _pwctype etc as const.
*       01-29-01  GB    Added __pctype_func, __pwctype_func, __mb_cur_max_func
*                       for use with STATIC_STDCPP stuff
*       11-12-01  GB    Added support for new locale implementation.
*       10-08-02  EAN   Declared towupper and towlower to take and return
*                       wint_t instead of wchar_t
*       08-13-03  AC    Added __iswcsym and __iswcsymf
*       02-16-04  SJ    VSW#243523 - Used INTERNAL_IFSTRIP to prevent stuff from
*                       getting stripped off the header in libw32\include
*       03-08-04  MSL   Fixed broken _isxxx_l which referred to undefined struct and 
*                       didn't tolerate NULL
*       03-22-04  AJS   Removed data exports when compiling /clr:pure
*       03-26-04  AC    Fixed isprint family
*       03-30-04  AC    Reverted the isprint fix
*       04-20-04  AC    Remove duplicated declaration
*                       VSW#287561
*       09-15-04  AGH   Changed _ST_MODEL to _CRT_DISABLE_PERFCRITICAL_THREADLOCKING
*       09-27-04  ESC   Added iswupper_l
*       10-01-04  AGH   Declared _iswupper_l
*       10-18-04  MSL   Removed iswupper_l (should be _iswupper_l)
*       11-08-04  JL    Added _CRT_NONSTDC_DEPRECATE to deprecate non-ANSI functions
*       11-18-04  ATC   SAL Annotations for CRT Headers
*       12-14-04  AC    Replaced _CRT_DISABLE_PERFCRITICAL_THREADLOCKING with _CRT_DISABLE_PERFCRIT_LOCKS
*                       VSW#300574
*       01-20-05  GR    Add _CRT_JIT_INTRINSIC annotations for JIT64 use
*       03-16-05  PAL   Add prototypes for _is*_l functions outside _CTYPE_DISABLE_MACROS.
*                       VSW 468378
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*                       Packing fix
*       04-14-05  MSL   Intellisense locale name cleanup
*                       Compile under MIDL
*       05-06-05  PML   add ___mb_cur_max_l_func
*
****/

#pragma once

#ifndef _INC_CTYPE
#define _INC_CTYPE

#include <crtdefs.h>

#ifdef  __cplusplus
extern "C" {
#endif

#ifndef WEOF
#define WEOF (wint_t)(0xFFFF)
#endif

#ifndef _CRT_CTYPEDATA_DEFINED
#define _CRT_CTYPEDATA_DEFINED
#ifndef _CTYPE_DISABLE_MACROS
#ifndef _INTERNAL_IFSTRIP_
extern const unsigned short __newctype[];
#if     defined(_DLL) && defined(_M_IX86)
/* Retained for compatibility with VC++ 5.0 and earlier versions */
_CRTIMP const unsigned short ** __cdecl __p__pctype(void);
#endif
#endif  /* _INTERNAL_IFSTRIP_ */

#ifndef __PCTYPE_FUNC
#if defined(_CRT_DISABLE_PERFCRIT_LOCKS) && !defined(_DLL)
#define __PCTYPE_FUNC  _pctype
#else
#define __PCTYPE_FUNC   __pctype_func()
#endif  
#endif  /* __PCTYPE_FUNC */

_CRTIMP const unsigned short * __cdecl __pctype_func(void);
#if !defined(_M_CEE_PURE)
_CRTIMP extern const unsigned short *_pctype;
#else
#define _pctype (__pctype_func())
#endif /* !defined(_M_CEE_PURE) */
#endif  /* _CTYPE_DISABLE_MACROS */
#endif

#ifndef _CRT_WCTYPEDATA_DEFINED
#define _CRT_WCTYPEDATA_DEFINED
#ifndef _CTYPE_DISABLE_MACROS
#if !defined(_M_CEE_PURE)
_CRTIMP extern const unsigned short _wctype[];
#endif /* !defined(_M_CEE_PURE) */
#ifndef _INTERNAL_IFSTRIP_
extern const unsigned short __newctype[];
#if     defined(_DLL) && defined(_M_IX86)
/* Retained for compatibility with VC++ 5.0 and earlier versions */
_CRTIMP const wctype_t ** __cdecl __p__pwctype(void);
#endif
#endif  /* _INTERNAL_IFSTRIP_ */

_CRTIMP const wctype_t * __cdecl __pwctype_func(void);
#if !defined(_M_CEE_PURE)
_CRTIMP extern const wctype_t *_pwctype;
#else
#define _pwctype (__pwctype_func())
#endif /* !defined(_M_CEE_PURE) */
#endif  /* _CTYPE_DISABLE_MACROS */
#endif

#ifndef _CTYPE_DISABLE_MACROS
#ifndef _INTERNAL_IFSTRIP_
extern const unsigned char __newclmap[];
extern const unsigned char __newcumap[];
#endif  /* _INTERNAL_IFSTRIP_ */
#endif  /* _CTYPE_DISABLE_MACROS */


#ifndef _INTERNAL_IFSTRIP_
extern pthreadlocinfo __ptlocinfo;
extern pthreadmbcinfo __ptmbcinfo;
extern int __locale_changed;
extern struct threadlocaleinfostruct __initiallocinfo;
extern _locale_tstruct __initiallocalestructinfo;
#if _PTD_LOCALE_SUPPORT
extern int __globallocalestatus;
pthreadlocinfo __cdecl __updatetlocinfo(void);
pthreadmbcinfo __cdecl __updatetmbcinfo(void);
#endif  /* _PTD_LOCALE_SUPPORT */
#endif  /* _INTERNAL_IFSTRIP_ */


/* set bit masks for the possible character types */

#define _UPPER          0x1     /* upper case letter */
#define _LOWER          0x2     /* lower case letter */
#define _DIGIT          0x4     /* digit[0-9] */
#define _SPACE          0x8     /* tab, carriage return, newline, */
                                /* vertical tab or form feed */
#define _PUNCT          0x10    /* punctuation character */
#define _CONTROL        0x20    /* control character */
#define _BLANK          0x40    /* space char */
#define _HEX            0x80    /* hexadecimal digit */

#define _LEADBYTE       0x8000                  /* multibyte leadbyte */
#define _ALPHA          (0x0100|_UPPER|_LOWER)  /* alphabetic character */


/* character classification function prototypes */

#ifndef _CTYPE_DEFINED

_Check_return_ _CRTIMP int __cdecl _isctype(_In_ int _C, _In_ int _Type);
_Check_return_ _CRTIMP int __cdecl _isctype_l(_In_ int _C, _In_ int _Type, _In_opt_ _locale_t _Locale);
_Check_return_ _CRT_JIT_INTRINSIC _CRTIMP int __cdecl isalpha(_In_ int _C);
_Check_return_ _CRTIMP int __cdecl _isalpha_l(_In_ int _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRT_JIT_INTRINSIC _CRTIMP int __cdecl isupper(_In_ int _C);
_Check_return_ _CRTIMP int __cdecl _isupper_l(_In_ int _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRT_JIT_INTRINSIC _CRTIMP int __cdecl islower(_In_ int _C);
_Check_return_ _CRTIMP int __cdecl _islower_l(_In_ int _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRT_JIT_INTRINSIC _CRTIMP int __cdecl isdigit(_In_ int _C);
_Check_return_ _CRTIMP int __cdecl _isdigit_l(_In_ int _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl isxdigit(_In_ int _C);
_Check_return_ _CRTIMP int __cdecl _isxdigit_l(_In_ int _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRT_JIT_INTRINSIC _CRTIMP int __cdecl isspace(_In_ int _C);
_Check_return_ _CRTIMP int __cdecl _isspace_l(_In_ int _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl ispunct(_In_ int _C);
_Check_return_ _CRTIMP int __cdecl _ispunct_l(_In_ int _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRT_JIT_INTRINSIC _CRTIMP int __cdecl isalnum(_In_ int _C);
_Check_return_ _CRTIMP int __cdecl _isalnum_l(_In_ int _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl isprint(_In_ int _C);
_Check_return_ _CRTIMP int __cdecl _isprint_l(_In_ int _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl isgraph(_In_ int _C);
_Check_return_ _CRTIMP int __cdecl _isgraph_l(_In_ int _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl iscntrl(_In_ int _C);
_Check_return_ _CRTIMP int __cdecl _iscntrl_l(_In_ int _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRT_JIT_INTRINSIC _CRTIMP int __cdecl toupper(_In_ int _C);
_Check_return_ _CRT_JIT_INTRINSIC _CRTIMP int __cdecl tolower(_In_ int _C);
_Check_return_ _CRT_JIT_INTRINSIC _CRTIMP int __cdecl _tolower(_In_ int _C);
_Check_return_ _CRTIMP int __cdecl _tolower_l(_In_ int _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRT_JIT_INTRINSIC _CRTIMP int __cdecl _toupper(_In_ int _C);
_Check_return_ _CRTIMP int __cdecl _toupper_l(_In_ int _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl __isascii(_In_ int _C);
_Check_return_ _CRTIMP int __cdecl __toascii(_In_ int _C);
_Check_return_ _CRTIMP int __cdecl __iscsymf(_In_ int _C);
_Check_return_ _CRTIMP int __cdecl __iscsym(_In_ int _C);
#define _CTYPE_DEFINED
#endif

#ifndef _WCTYPE_DEFINED

/* wide function prototypes, also declared in wchar.h  */

/* character classification function prototypes */

_Check_return_ _CRTIMP int __cdecl iswalpha(_In_ wint_t _C);
_Check_return_ _CRTIMP int __cdecl _iswalpha_l(_In_ wint_t _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl iswupper(_In_ wint_t _C);
_Check_return_ _CRTIMP int __cdecl _iswupper_l(_In_ wint_t _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl iswlower(_In_ wint_t _C);
_Check_return_ _CRTIMP int __cdecl _iswlower_l(_In_ wint_t _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl iswdigit(_In_ wint_t _C);
_Check_return_ _CRTIMP int __cdecl _iswdigit_l(_In_ wint_t _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl iswxdigit(_In_ wint_t _C);
_Check_return_ _CRTIMP int __cdecl _iswxdigit_l(_In_ wint_t _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl iswspace(_In_ wint_t _C);
_Check_return_ _CRTIMP int __cdecl _iswspace_l(_In_ wint_t _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl iswpunct(_In_ wint_t _C);
_Check_return_ _CRTIMP int __cdecl _iswpunct_l(_In_ wint_t _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl iswalnum(_In_ wint_t _C);
_Check_return_ _CRTIMP int __cdecl _iswalnum_l(_In_ wint_t _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl iswprint(_In_ wint_t _C);
_Check_return_ _CRTIMP int __cdecl _iswprint_l(_In_ wint_t _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl iswgraph(_In_ wint_t _C);
_Check_return_ _CRTIMP int __cdecl _iswgraph_l(_In_ wint_t _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl iswcntrl(_In_ wint_t _C);
_Check_return_ _CRTIMP int __cdecl _iswcntrl_l(_In_ wint_t _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl iswascii(_In_ wint_t _C);

_Check_return_ _CRTIMP wint_t __cdecl towupper(_In_ wint_t _C);
_Check_return_ _CRTIMP wint_t __cdecl _towupper_l(_In_ wint_t _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP wint_t __cdecl towlower(_In_ wint_t _C);
_Check_return_ _CRTIMP wint_t __cdecl _towlower_l(_In_ wint_t _C, _In_opt_ _locale_t _Locale); 
_Check_return_ _CRTIMP int __cdecl iswctype(_In_ wint_t _C, _In_ wctype_t _Type);
_Check_return_ _CRTIMP int __cdecl _iswctype_l(_In_ wint_t _C, _In_ wctype_t _Type, _In_opt_ _locale_t _Locale);

_Check_return_ _CRTIMP int __cdecl __iswcsymf(_In_ wint_t _C);
_Check_return_ _CRTIMP int __cdecl _iswcsymf_l(_In_ wint_t _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl __iswcsym(_In_ wint_t _C);
_Check_return_ _CRTIMP int __cdecl _iswcsym_l(_In_ wint_t _C, _In_opt_ _locale_t _Locale);

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP
_Check_return_ _CRTIMP int __cdecl isleadbyte(_In_ int _C);
_Check_return_ _CRTIMP int __cdecl _isleadbyte_l(_In_ int _C, _In_opt_ _locale_t _Locale);
_CRT_OBSOLETE(iswctype) _CRTIMP int __cdecl is_wctype(_In_ wint_t _C, _In_ wctype_t _Type);
#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

#define _WCTYPE_DEFINED
#endif

/* the character classification macro definitions */

#ifndef _CTYPE_DISABLE_MACROS

/*
 * Maximum number of bytes in multi-byte character in the current locale
 * (also defined in stdlib.h).
 */
#ifndef MB_CUR_MAX
#ifndef _INTERNAL_IFSTRIP_
#if     defined(_DLL) && defined(_M_IX86)
/* Retained for compatibility with VC++ 5.0 and earlier versions */
_CRTIMP int * __cdecl __p___mb_cur_max(void);
#endif
#define __MB_CUR_MAX(ptloci) (ptloci)->mb_cur_max
#endif  /* _INTERNAL_IFSTRIP_ */

#if defined(_CRT_DISABLE_PERFCRIT_LOCKS) && !defined(_DLL)
#define MB_CUR_MAX __mb_cur_max
#else
#define MB_CUR_MAX ___mb_cur_max_func()
#endif
#if !defined(_M_CEE_PURE)
/* No data exports in pure code */
_CRTIMP extern int __mb_cur_max;
#else
#define __mb_cur_max (___mb_cur_max_func())
#endif /* !defined(_M_CEE_PURE) */
_CRTIMP int __cdecl ___mb_cur_max_func(void);
_CRTIMP int __cdecl ___mb_cur_max_l_func(_locale_t);
#endif  /* MB_CUR_MAX */

/* Introduced to detect error when character testing functions are called
 * with illegal input of integer.
 */
#ifdef _DEBUG
_CRTIMP int __cdecl _chvalidator(_In_ int _Ch, _In_ int _Mask);
#define __chvalidchk(a,b)       _chvalidator(a,b)
#else
#define __chvalidchk(a,b)       (__PCTYPE_FUNC[(a)] & (b))
#endif


#ifndef _INTERNAL_IFSTRIP_
#ifdef _WCE_BOOTCRT
#define __ascii_isalpha(c)     ( ('A' <= (c) && (c) <= 'Z') || ( 'a' <= (c) && (c) <= 'z'))
#define __ascii_isdigit(c)     ( '0' <= (c) && (c) <= '9')
#else
#define __ascii_isalpha(c)      ( __chvalidchk(c, _ALPHA))
#define __ascii_isdigit(c)      ( __chvalidchk(c, _DIGIT))
#endif
#define __ascii_tolower(c)      ( (((c) >= 'A') && ((c) <= 'Z')) ? ((c) - 'A' + 'a') : (c) )
#define __ascii_toupper(c)      ( (((c) >= 'a') && ((c) <= 'z')) ? ((c) - 'a' + 'A') : (c) )
#define __ascii_iswalpha(c)     ( ('A' <= (c) && (c) <= 'Z') || ( 'a' <= (c) && (c) <= 'z'))
#define __ascii_iswdigit(c)     ( '0' <= (c) && (c) <= '9')
#define __ascii_towlower(c)     ( (((c) >= L'A') && ((c) <= L'Z')) ? ((c) - L'A' + L'a') : (c) )
#define __ascii_towupper(c)     ( (((c) >= L'a') && ((c) <= L'z')) ? ((c) - L'a' + L'A') : (c) )
#ifdef _WCE_BOOTCRT
static int __ascii_isspace(int c) {
    switch (c)
    {
        case ' ':
        case '\n':
        case '\t':
        case '\r':
        case '\f':
        case '\v':
            return 1;
        default:
            return 0;
    }
}
#endif // _WCE_BOOTCRT
#endif  /* _INTERNAL_IFSTRIP_ */

#if defined(_CRT_DISABLE_PERFCRIT_LOCKS) && !defined(_DLL)
#ifndef __cplusplus
#define isalpha(_c)     (MB_CUR_MAX > 1 ? _isctype(_c,_ALPHA) : __chvalidchk(_c, _ALPHA))
#define isupper(_c)     (MB_CUR_MAX > 1 ? _isctype(_c,_UPPER) : __chvalidchk(_c, _UPPER))
#define islower(_c)     (MB_CUR_MAX > 1 ? _isctype(_c,_LOWER) : __chvalidchk(_c, _LOWER))
#define isdigit(_c)     (MB_CUR_MAX > 1 ? _isctype(_c,_DIGIT) : __chvalidchk(_c, _DIGIT))
#define isxdigit(_c)    (MB_CUR_MAX > 1 ? _isctype(_c,_HEX)   : __chvalidchk(_c, _HEX))
#define isspace(_c)     (MB_CUR_MAX > 1 ? _isctype(_c,_SPACE) : __chvalidchk(_c, _SPACE))
#define ispunct(_c)     (MB_CUR_MAX > 1 ? _isctype(_c,_PUNCT) : __chvalidchk(_c, _PUNCT))
#define isalnum(_c)     (MB_CUR_MAX > 1 ? _isctype(_c,_ALPHA|_DIGIT) : __chvalidchk(_c, (_ALPHA|_DIGIT)))
#define isprint(_c)     (MB_CUR_MAX > 1 ? _isctype(_c,_BLANK|_PUNCT|_ALPHA|_DIGIT) : __chvalidchk(_c, (_BLANK|_PUNCT|_ALPHA|_DIGIT)))
#define isgraph(_c)     (MB_CUR_MAX > 1 ? _isctype(_c,_PUNCT|_ALPHA|_DIGIT) : __chvalidchk(_c, (_PUNCT|_ALPHA|_DIGIT)))
#define iscntrl(_c)     (MB_CUR_MAX > 1 ? _isctype(_c,_CONTROL) : __chvalidchk(_c, _CONTROL))
#elif   0         /* Pending ANSI C++ integration */
_Check_return_ inline int isalpha(_In_ int _C)
        {return (MB_CUR_MAX > 1 ? _isctype(_C,_ALPHA) : __chvalidchk(_C, _ALPHA)); }
_Check_return_ inline int isupper(_In_ int _C)
        {return (MB_CUR_MAX > 1 ? _isctype(_C,_UPPER) : __chvalidchk(_C, _UPPER)); }
_Check_return_ inline int islower(_In_ int _C)
        {return (MB_CUR_MAX > 1 ? _isctype(_C,_LOWER) : __chvalidchk(_C, _LOWER)); }
_Check_return_ inline int isdigit(_In_ int _C)
        {return (MB_CUR_MAX > 1 ? _isctype(_C,_DIGIT) : __chvalidchk(_C, _DIGIT)); }
_Check_return_ inline int isxdigit(_In_ int _C)
        {return (MB_CUR_MAX > 1 ? _isctype(_C,_HEX)   : __chvalidchk(_C, _HEX)); }
_Check_return_ inline int isspace(_In_ int _C)
        {return (MB_CUR_MAX > 1 ? _isctype(_C,_SPACE) : __chvalidchk(_C, _SPACE)); }
_Check_return_ inline int ispunct(_In_ int _C)
        {return (MB_CUR_MAX > 1 ? _isctype(_C,_PUNCT) : __chvalidchk(_C, _PUNCT)); }
_Check_return_ inline int isalnum(_In_ int _C)
        {return (MB_CUR_MAX > 1 ? _isctype(_C,_ALPHA|_DIGIT)
                : __chvalidchk(_C) , (_ALPHA|_DIGIT)); }
_Check_return_ inline int isprint(_In_ int _C)
        {return (MB_CUR_MAX > 1 ? _isctype(_C,_BLANK|_PUNCT|_ALPHA|_DIGIT)
                : __chvalidchk(_C , (_BLANK|_PUNCT|_ALPHA|_DIGIT))); }
_Check_return_ inline int isgraph(_In_ int _C)
        {return (MB_CUR_MAX > 1 ? _isctype(_C,_PUNCT|_ALPHA|_DIGIT)
                : __chvalidchk(_C , (_PUNCT|_ALPHA|_DIGIT))); }
_Check_return_ inline int iscntrl(_In_ int _C)
        {return (MB_CUR_MAX > 1 ? _isctype(_C,_CONTROL)
                : __chvalidchk(_C , _CONTROL)); }
#endif  /* __cplusplus */
#endif  

#ifdef _DEBUG
_CRTIMP int __cdecl _chvalidator_l(_In_opt_ _locale_t, _In_ int _Ch, _In_ int _Mask);
#define _chvalidchk_l(_Char, _Flag, _Locale)  _chvalidator_l(_Locale, _Char, _Flag)
#else
#define _chvalidchk_l(_Char, _Flag, _Locale)  (_Locale==NULL ? __chvalidchk(_Char, _Flag) : ((_locale_t)_Locale)->locinfo->pctype[_Char] & (_Flag))
#endif  /* DEBUG */

#ifndef _INTERNAL_IFSTRIP_
#define __ascii_isalpha_l(_Char, _Locale)      ( _chvalidchk_l(_Char, _ALPHA, _Locale))
#define __ascii_isdigit_l(_Char, _Locale)      ( _chvalidchk_l(_Char, _DIGIT, _Locale))
#endif

#define _ischartype_l(_Char, _Flag, _Locale)    ( ((_Locale)!=NULL && (((_locale_t)(_Locale))->locinfo->mb_cur_max) > 1) ? _isctype_l(_Char, (_Flag), _Locale) : _chvalidchk_l(_Char,_Flag,_Locale))
#define _isalpha_l(_Char, _Locale)      _ischartype_l(_Char, _ALPHA, _Locale)
#define _isupper_l(_Char, _Locale)      _ischartype_l(_Char, _UPPER, _Locale)
#define _islower_l(_Char, _Locale)      _ischartype_l(_Char, _LOWER, _Locale)
#define _isdigit_l(_Char, _Locale)      _ischartype_l(_Char, _DIGIT, _Locale)
#define _isxdigit_l(_Char, _Locale)     _ischartype_l(_Char, _HEX, _Locale)
#define _isspace_l(_Char, _Locale)      _ischartype_l(_Char, _SPACE, _Locale)
#define _ispunct_l(_Char, _Locale)      _ischartype_l(_Char, _PUNCT, _Locale)
#define _isalnum_l(_Char, _Locale)      _ischartype_l(_Char, _ALPHA|_DIGIT, _Locale)
#define _isprint_l(_Char, _Locale)      _ischartype_l(_Char, _BLANK|_PUNCT|_ALPHA|_DIGIT, _Locale)
#define _isgraph_l(_Char, _Locale)      _ischartype_l(_Char, _PUNCT|_ALPHA|_DIGIT, _Locale)
#define _iscntrl_l(_Char, _Locale)      _ischartype_l(_Char, _CONTROL, _Locale)

#define _tolower(_Char)    ( (_Char)-'A'+'a' )
#define _toupper(_Char)    ( (_Char)-'a'+'A' )

#define __isascii(_Char)   ( (unsigned)(_Char) < 0x80 )
#define __toascii(_Char)   ( (_Char) & 0x7f )

#ifndef _WCTYPE_INLINE_DEFINED

#ifdef _CRTBLD
#ifndef _INTERNAL_IFSTRIP_
#define _CRT_WCTYPE_NOINLINE
#else
#undef _CRT_WCTYPE_NOINLINE
#endif
#endif

#if !defined(__cplusplus) || defined(_M_CEE_PURE) || defined(MRTDLL) || defined(_CRT_WCTYPE_NOINLINE)
#define iswalpha(_c)    ( iswctype(_c,_ALPHA) )
#define iswupper(_c)    ( iswctype(_c,_UPPER) )
#define iswlower(_c)    ( iswctype(_c,_LOWER) )
#define iswdigit(_c)    ( iswctype(_c,_DIGIT) )
#define iswxdigit(_c)   ( iswctype(_c,_HEX) )
#define iswspace(_c)    ( iswctype(_c,_SPACE) )
#define iswpunct(_c)    ( iswctype(_c,_PUNCT) )
#define iswalnum(_c)    ( iswctype(_c,_ALPHA|_DIGIT) )
#define iswprint(_c)    ( iswctype(_c,_BLANK|_PUNCT|_ALPHA|_DIGIT) )
#define iswgraph(_c)    ( iswctype(_c,_PUNCT|_ALPHA|_DIGIT) )
#define iswcntrl(_c)    ( iswctype(_c,_CONTROL) )
#define iswascii(_c)    ( (unsigned)(_c) < 0x80 )

#define _iswalpha_l(_c,_p)    ( iswctype(_c,_ALPHA) )
#define _iswupper_l(_c,_p)    ( iswctype(_c,_UPPER) )
#define _iswlower_l(_c,_p)    ( iswctype(_c,_LOWER) )
#define _iswdigit_l(_c,_p)    ( iswctype(_c,_DIGIT) )
#define _iswxdigit_l(_c,_p)   ( iswctype(_c,_HEX) )
#define _iswspace_l(_c,_p)    ( iswctype(_c,_SPACE) )
#define _iswpunct_l(_c,_p)    ( iswctype(_c,_PUNCT) )
#define _iswalnum_l(_c,_p)    ( iswctype(_c,_ALPHA|_DIGIT) )
#define _iswprint_l(_c,_p)    ( iswctype(_c,_BLANK|_PUNCT|_ALPHA|_DIGIT) )
#define _iswgraph_l(_c,_p)    ( iswctype(_c,_PUNCT|_ALPHA|_DIGIT) )
#define _iswcntrl_l(_c,_p)    ( iswctype(_c,_CONTROL) )
#elif   0         /* __cplusplus */
_Check_return_ inline int iswalpha(_In_ wint_t _C) {return (iswctype(_C,_ALPHA)); }
_Check_return_ inline int iswupper(_In_ wint_t _C) {return (iswctype(_C,_UPPER)); }
_Check_return_ inline int iswlower(_In_ wint_t _C) {return (iswctype(_C,_LOWER)); }
_Check_return_ inline int iswdigit(_In_ wint_t _C) {return (iswctype(_C,_DIGIT)); }
_Check_return_ inline int iswxdigit(_In_ wint_t _C) {return (iswctype(_C,_HEX)); }
_Check_return_ inline int iswspace(_In_ wint_t _C) {return (iswctype(_C,_SPACE)); }
_Check_return_ inline int iswpunct(_In_ wint_t _C) {return (iswctype(_C,_PUNCT)); }
_Check_return_ inline int iswalnum(_In_ wint_t _C) {return (iswctype(_C,_ALPHA|_DIGIT)); }
_Check_return_ inline int iswprint(_In_ wint_t _C) {return (iswctype(_C,_BLANK|_PUNCT|_ALPHA|_DIGIT)); }
_Check_return_ inline int iswgraph(_In_ wint_t _C) {return (iswctype(_C,_PUNCT|_ALPHA|_DIGIT)); }
_Check_return_ inline int iswcntrl(_In_ wint_t _C) {return (iswctype(_C,_CONTROL)); }
_Check_return_ inline int iswascii(_In_ wint_t _C) {return ((unsigned)(_C) < 0x80); }

_Check_return_ inline int __CRTDECL _iswalpha_l(_In_ wint_t _C, _In_opt_ _locale_t) {return (iswctype(_C,_ALPHA)); }
_Check_return_ inline int __CRTDECL _iswupper_l(_In_ wint_t _C, _In_opt_ _locale_t) {return (iswctype(_C,_UPPER)); }
_Check_return_ inline int __CRTDECL _iswlower_l(_In_ wint_t _C, _In_opt_ _locale_t) {return (iswctype(_C,_LOWER)); }
_Check_return_ inline int __CRTDECL _iswdigit_l(_In_ wint_t _C, _In_opt_ _locale_t) {return (iswctype(_C,_DIGIT)); }
_Check_return_ inline int __CRTDECL _iswxdigit_l(_In_ wint_t _C, _In_opt_ _locale_t) {return(iswctype(_C,_HEX)); }
_Check_return_ inline int __CRTDECL _iswspace_l(_In_ wint_t _C, _In_opt_ _locale_t) {return (iswctype(_C,_SPACE)); }
_Check_return_ inline int __CRTDECL _iswpunct_l(_In_ wint_t _C, _In_opt_ _locale_t) {return (iswctype(_C,_PUNCT)); }
_Check_return_ inline int __CRTDECL _iswalnum_l(_In_ wint_t _C, _In_opt_ _locale_t) {return (iswctype(_C,_ALPHA|_DIGIT)); }
_Check_return_ inline int __CRTDECL _iswprint_l(_In_ wint_t _C, _In_opt_ _locale_t) {return (iswctype(_C,_BLANK|_PUNCT|_ALPHA|_DIGIT)); }
_Check_return_ inline int __CRTDECL _iswgraph_l(_In_ wint_t _C, _In_opt_ _locale_t) {return (iswctype(_C,_PUNCT|_ALPHA|_DIGIT)); }
_Check_return_ inline int __CRTDECL _iswcntrl_l(_In_ wint_t _C, _In_opt_ _locale_t) {return (iswctype(_C,_CONTROL)); }

_Check_return_ inline int isleadbyte(int _C) {return (__PCTYPE_FUNC[(unsigned char)(_C)] & _LEADBYTE); }
#endif  /* __cplusplus */
#define _WCTYPE_INLINE_DEFINED
#endif  /* _WCTYPE_INLINE_DEFINED */

/* MS C version 2.0 extended ctype macros */

#define __iscsymf(_c)   (isalpha(_c) || ((_c) == '_'))
#define __iscsym(_c)    (isalnum(_c) || ((_c) == '_'))
#define __iswcsymf(_c)  (iswalpha(_c) || ((_c) == '_'))
#define __iswcsym(_c)   (iswalnum(_c) || ((_c) == '_'))

#define _iscsymf_l(_c, _p)   (_isalpha_l(_c, _p) || ((_c) == '_'))
#define _iscsym_l(_c, _p)    (_isalnum_l(_c, _p) || ((_c) == '_'))
#define _iswcsymf_l(_c, _p)  (iswalpha(_c) || ((_c) == '_'))
#define _iswcsym_l(_c, _p)   (iswalnum(_c) || ((_c) == '_'))

#endif  /* _CTYPE_DISABLE_MACROS */


#if     !__STDC__

/* Non-ANSI names for compatibility */

#ifndef _CTYPE_DEFINED
_Check_return_ _CRT_NONSTDC_DEPRECATE(__isascii) _CRTIMP int __cdecl isascii(_In_ int _C);
_Check_return_ _CRT_NONSTDC_DEPRECATE(__toascii) _CRTIMP int __cdecl toascii(_In_ int _C);
_Check_return_ _CRT_NONSTDC_DEPRECATE(__iscsymf) _CRTIMP int __cdecl iscsymf(_In_ int _C);
_Check_return_ _CRT_NONSTDC_DEPRECATE(__iscsym) _CRTIMP int __cdecl iscsym(_In_ int _C);
#else
#define isascii __isascii
#define toascii __toascii
#define iscsymf __iscsymf
#define iscsym  __iscsym
#endif

#endif  /* __STDC__ */

#ifdef  __cplusplus
}
#endif

#endif  /* _INC_CTYPE */
