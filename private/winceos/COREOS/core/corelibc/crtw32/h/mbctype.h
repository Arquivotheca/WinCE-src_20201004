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
*mbctype.h - MBCS character conversion macros
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Defines macros for MBCS character classification/conversion.
*
*       [Public]
*
*Revision History:
*       11-19-92  KRS   Ported from 16-bit sources.
*       02-23-93  SKS   Update copyright to 1993
*       07-09-93  KRS   Fix problems with _isxxxlead/trail macros, etc.
*       08-12-93  CFW   Change _mbctype type, fix ifstrip macro name.
*       09-29-93  CFW   Add _ismbbkprint, modify _ismbbkana.
*       10-08-93  GJF   Support NT SDK and Cuda.
*       10-13-93  GJF   Deleted obsolete COMBOINC check.
*       10-19-93  CFW   Remove _MBCS test.
*       10-27-93  CFW   _CRTIMP for __mbcodepage.
*       01-04-94  CFW   Add _setmbcp and _getmbcp.
*       04-14-94  CFW   Remove _mbcodepage and second _setmbcp parameter.
*       04-18-94  CFW   Use _ALPHA instead of _LOWER|_UPPER.
*       04-21-94  CFW   Remove _mbcodepage ref.
*       04-21-94  GJF   Made declaration of _mbctype conditional on _DLL
*                       (for compatibility with the Win32s version of
*                       msvcrt*.dll). Made safe for repeated inclusions.
*                       Also, conditionally included win32s.h.
*       05-03-94  GJF   Made declaration of _mbctype for _DLL conditional on
*                       _M_IX86 also.
*       02-11-95  CFW   Add _CRTBLD to avoid users getting wrong headers.
*       02-14-95  CFW   Clean up Mac merge.
*       03-22-95  CFW   Add _MB_CP_LOCALE.
*       12-14-95  JWM   Add "#pragma once".
*       03-17-97  RDK   Add external _mbcasemap.
*       03-26-97  GJF   Cleaned out obsolete support for Win32s, _NTSDK and
*                       _CRTAPI*.
*       08-13-97  GJF   Strip __p_* prototypes from release version.
*       09-30-97  JWM   Restored not-so-obsolete _CRTAPI1 support.
*       04-24-98  GJF   Added support for per-thread mbc information.
*       05-13-99  PML   Remove _CRTAPI1
*       05-17-99  PML   Remove all Macintosh support.
*       06-08-00  PML   Remove threadmbcinfo.{pprev,pnext}.  Rename
*                       THREADMBCINFO to _THREADMBCINFO.
*       05-28-02  GB    replaced _ctype with _pctype which makes more sense.
*       10-08-02  EAN   removed _CRTIMP on __ismbslead_mt
*       03-22-04  AJS   Removed data exports when compiling /clr:pure
*       03-26-04  AC    Fixed isprint family
*       03-30-04  AC    Reverted the isprint fix
*       04-07-04  MSL   Double slash removal
*       09-15-04  AGH   Changed _ST_MODEL to _CRT_DISABLE_PERFCRITICAL_THREADLOCKING
*       11-18-04  ATC   SAL Annotations for CRT Headers
*       12-14-04  AC    Replaced _CRT_DISABLE_PERFCRITICAL_THREADLOCKING with _CRT_DISABLE_PERFCRIT_LOCKS
*                       VSW#300574
*       01-14-05  AC    Fix SAL annotations (using prefast espx plug-in)
*       03-15-05  PAL   Resolve data export to function call under /clr:pure
*                       VSW 242824
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*                       Packing fix
*       04-14-05  MSL   Intellisense locale name cleanup
*
****/

#pragma once

#ifndef _INC_MBCTYPE
#define _INC_MBCTYPE

#include <crtdefs.h>
#include <ctype.h>

#ifdef  __cplusplus
extern "C" {
#endif


/*
 * MBCS - Multi-Byte Character Set
 */

/*
 * This declaration allows the user access the _mbctype[] look-up array.
 */
#ifndef _INTERNAL_IFSTRIP_
#if     defined(_DLL) && defined(_M_IX86)
/* Retained for compatibility with VC++ 5.0 and earlier versions */
_Check_return_ _CRTIMP unsigned char * __cdecl __p__mbctype(void);
_Check_return_ _CRTIMP unsigned char * __cdecl __p__mbcasemap(void);
#endif
#endif  /* _INTERNAL_IFSTRIP_ */
#if !defined(_M_CEE_PURE)
/* No data exports in pure code */
_CRTIMP extern unsigned char _mbctype[];
_CRTIMP extern unsigned char _mbcasemap[];
#else
_Check_return_ _CRTIMP unsigned char * __cdecl __p__mbctype(void);
_Check_return_ _CRTIMP unsigned char * __cdecl __p__mbcasemap(void);
#define _mbctype   (__p__mbctype())
#define _mbcasemap (__p__mbcasemap())
#endif /* !defined(_M_CEE_PURE) */

#ifndef _INTERNAL_IFSTRIP_
extern pthreadmbcinfo __ptmbcinfo;
extern int __locale_changed;
extern struct threadmbcinfostruct __initialmbcinfo;
#if _PTD_LOCALE_SUPPORT
extern int __globallocalestatus;
pthreadmbcinfo __cdecl __updatetmbcinfo(void);
#endif  /* _PTD_LOCALE_SUPPORT */
#endif  /* _INTERNAL_IFSTRIP_ */

/* bit masks for MBCS character types */

#define _MS     0x01    /* MBCS single-byte symbol */
#define _MP     0x02    /* MBCS punct */
#define _M1     0x04    /* MBCS 1st (lead) byte */
#define _M2     0x08    /* MBCS 2nd byte*/

#define _SBUP   0x10    /* SBCS upper char */
#define _SBLOW  0x20    /* SBCS lower char */

/* byte types  */

#define _MBC_SINGLE     0       /* valid single byte char */
#define _MBC_LEAD       1       /* lead byte */
#define _MBC_TRAIL      2       /* trailing byte */
#define _MBC_ILLEGAL    (-1)    /* illegal byte */

#define _KANJI_CP   932

/* _setmbcp parameter defines */
#define _MB_CP_SBCS     0
#define _MB_CP_OEM      -2
#define _MB_CP_ANSI     -3
#define _MB_CP_LOCALE   -4


#ifndef _MBCTYPE_DEFINED

/* MB control routines */

_CRTIMP int __cdecl _setmbcp(_In_ int _CodePage);
_CRTIMP int __cdecl _getmbcp(void);


/* MBCS character classification function prototypes */

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP

/* byte routines */
_Check_return_ _CRTIMP int __cdecl _ismbbkalnum( _In_ unsigned int _C );
_Check_return_ _CRTIMP int __cdecl _ismbbkalnum_l(_In_ unsigned int _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _ismbbkana( _In_ unsigned int _C );
_Check_return_ _CRTIMP int __cdecl _ismbbkana_l(_In_ unsigned int _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _ismbbkpunct( _In_ unsigned int _C );
_Check_return_ _CRTIMP int __cdecl _ismbbkpunct_l(_In_ unsigned int _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _ismbbkprint( _In_ unsigned int _C );
_Check_return_ _CRTIMP int __cdecl _ismbbkprint_l(_In_ unsigned int _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _ismbbalpha( _In_ unsigned int _C );
_Check_return_ _CRTIMP int __cdecl _ismbbalpha_l(_In_ unsigned int _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _ismbbpunct( _In_ unsigned int _C );
_Check_return_ _CRTIMP int __cdecl _ismbbpunct_l(_In_ unsigned int _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _ismbbalnum( _In_ unsigned int _C );
_Check_return_ _CRTIMP int __cdecl _ismbbalnum_l(_In_ unsigned int _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _ismbbprint( _In_ unsigned int _C );
_Check_return_ _CRTIMP int __cdecl _ismbbprint_l(_In_ unsigned int _C, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _ismbbgraph( _In_ unsigned int _C );
_Check_return_ _CRTIMP int __cdecl _ismbbgraph_l(_In_ unsigned int _C, _In_opt_ _locale_t _Locale);

#ifndef _MBLEADTRAIL_DEFINED
_Check_return_ _CRTIMP int __cdecl _ismbblead( _In_ unsigned int _C);
_Check_return_ _CRTIMP int __cdecl _ismbblead_l(_In_ unsigned int _C, _In_opt_ _locale_t _Locale );
_Check_return_ _CRTIMP int __cdecl _ismbbtrail( _In_ unsigned int _C);
_Check_return_ _CRTIMP int __cdecl _ismbbtrail_l(_In_ unsigned int _C, _In_opt_ _locale_t _Locale );
_Check_return_ _CRTIMP int __cdecl _ismbslead(_In_reads_z_(_Pos - _Str+1) const unsigned char * _Str, _In_z_ const unsigned char * _Pos);
_Check_return_ _CRTIMP int __cdecl _ismbslead_l(_In_reads_z_(_Pos - _Str+1) const unsigned char * _Str, _In_z_ const unsigned char * _Pos, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _ismbstrail(_In_reads_z_(_Pos - _Str+1) const unsigned char * _Str, _In_z_ const unsigned char * _Pos);
_Check_return_ _CRTIMP int __cdecl _ismbstrail_l(_In_reads_z_(_Pos - _Str+1) const unsigned char * _Str, _In_z_ const unsigned char * _Pos, _In_opt_ _locale_t _Locale);

#define _MBLEADTRAIL_DEFINED
#endif

#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

#define _MBCTYPE_DEFINED
#endif

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP

/*
 * char byte classification macros
 */

#if defined(_CRT_DISABLE_PERFCRIT_LOCKS) && !defined(_DLL)
#define _ismbbkalnum(_c)    ((_mbctype+1)[(unsigned char)(_c)] & _MS)
#define _ismbbkprint(_c)    ((_mbctype+1)[(unsigned char)(_c)] & (_MS|_MP))
#define _ismbbkpunct(_c)    ((_mbctype+1)[(unsigned char)(_c)] & _MP)

#define _ismbbalnum(_c) (((_pctype)[(unsigned char)(_c)] & (_ALPHA|_DIGIT))||_ismbbkalnum(_c))
#define _ismbbalpha(_c) (((_pctype)[(unsigned char)(_c)] & (_ALPHA))||_ismbbkalnum(_c))
#define _ismbbgraph(_c) (((_pctype)[(unsigned char)(_c)] & (_PUNCT|_ALPHA|_DIGIT))||_ismbbkprint(_c))
#define _ismbbprint(_c) (((_pctype)[(unsigned char)(_c)] & (_BLANK|_PUNCT|_ALPHA|_DIGIT))||_ismbbkprint(_c))
#define _ismbbpunct(_c) (((_pctype)[(unsigned char)(_c)] & _PUNCT)||_ismbbkpunct(_c))

#define _ismbblead(_c)  ((_mbctype+1)[(unsigned char)(_c)] & _M1)
#define _ismbbtrail(_c) ((_mbctype+1)[(unsigned char)(_c)] & _M2)

#define _ismbbkana(_c)  ((_mbctype+1)[(unsigned char)(_c)] & (_MS|_MP))
#endif

#ifndef _INTERNAL_IFSTRIP_
#define _ismbbalnum_l(_c, pt)  ((((pt)->locinfo->pctype)[(unsigned char)(_c)] & \
                                (_ALPHA|_DIGIT)) || \
                                (((pt)->mbcinfo->mbctype+1)[(unsigned char)(_c)] & _MS))
#define _ismbbalpha_l(_c, pt)  ((((pt)->locinfo->pctype)[(unsigned char)(_c)] & \
                            (_ALPHA)) || \
                            (((pt)->mbcinfo->mbctype+1)[(unsigned char)(_c)] & _MS))
#define _ismbbgraph_l(_c, pt)  ((((pt)->locinfo->pctype)[(unsigned char)(_c)] & \
                            (_PUNCT|_ALPHA|_DIGIT)) || \
                            (((pt)->mbcinfo->mbctype+1)[(unsigned char)(_c)] & (_MS|_MP)))
#define _ismbbprint_l(_c, pt)  ((((pt)->locinfo->pctype)[(unsigned char)(_c)] & \
                            (_BLANK|_PUNCT|_ALPHA|_DIGIT)) || \
                            (((pt)->mbcinfo->mbctype + 1)[(unsigned char)(_c)] & (_MS|_MP)))
#define _ismbbpunct_l(_c, pt)  ((((pt)->locinfo->pctype)[(unsigned char)(_c)] & _PUNCT) || \
                                (((pt)->mbcinfo->mbctype+1)[(unsigned char)(_c)] & _MP))
#define _ismbblead_l(_c, p)   ((p->mbcinfo->mbctype + 1)[(unsigned char)(_c)] & _M1)
#define _ismbbtrail_l(_c, p)  ((p->mbcinfo->mbctype + 1)[(unsigned char)(_c)] & _M2)
#endif  /* _INTERNAL_IFSTRIP_ */

#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

#ifdef  __cplusplus
}
#endif

#endif  /* _INC_MBCTYPE */
