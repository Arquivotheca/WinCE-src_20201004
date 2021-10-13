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
*setlocal.h - internal definitions used by locale-dependent functions.
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Contains internal definitions/declarations for locale-dependent
*       functions, in particular those required by setlocale().
*
*       [Internal]
*
*Revision History:
*       10-16-91  ETC   32-bit version created from 16-bit setlocal.c
*       12-20-91  ETC   Removed GetLocaleInfo structure definitions.
*       08-18-92  KRS   Make _CLOCALEHANDLE == LANGNEUTRAL HANDLE = 0.
*       12-17-92  CFW   Added LC_ID, LCSTRINGS, and GetQualifiedLocale
*       12-17-92  KRS   Change value of NLSCMPERROR from 0 to INT_MAX.
*       01-08-93  CFW   Added LC_*_TYPE and _getlocaleinfo (wrapper) prototype.
*       01-13-93  KRS   Change LCSTRINGS back to LC_STRINGS for consistency.
*                       Change _getlocaleinfo prototype again.
*       02-08-93  CFW   Added time defintions from locale.h, added 'const' to
*                       GetQualifiedLocale prototype, added _lconv_static_*.
*       02-16-93  CFW   Changed time defs to long and short.
*       03-17-93  CFW   Add language and country info definitions.
*       03-23-93  CFW   Add _ to GetQualifiedLocale prototype.
*       03-24-93  CFW   Change to _get_qualified_locale.
*       04-06-93  SKS   Replace _CRTAPI1/2/3 with __cdecl, _CRTVAR1 with nothing
*       04-07-93  CFW   Added extern struct lconv definition.
*       09-14-93  CFW   Add internal use __aw_* function prototypes.
*       09-15-93  CFW   Use ANSI conformant "__" names.
*       09-27-93  CFW   Fix function prototypes.
*       11-09-93  CFW   Allow user to pass in code page to __crtxxx().
*       02-04-94  CFW   Remove unused first param from get_qualified_locale.
*       03-30-93  CFW   Move internal use __aw_* function prototypes to awint.h.
*       04-07-94  GJF   Added declaration for __lconv. Made declarations of
*                       __lconv_c, __lconv, __lc_codepage, __lc_handle
*                       conditional on ndef DLL_FOR_WIN32S. Conditionally
*                       included win32s.h.
*       04-11-94  CFW   Remove NLSCMPERROR.
*       04-13-94  GJF   Protected def of tagLC_ID, and typedefs to it, since
*                       they are duplicated in win32s.h.
*       04-15-94  GJF   Added prototypes for locale category initialization
*                       functions. Added declaration for __clocalestr.
*       02-14-95  CFW   Clean up Mac merge.
*       03-29-95  CFW   Add error message to internal headers.
*       04-11-95  CFW   Make country/language strings pointers.
*       12-14-95  JWM   Add "#pragma once".
*       06-05-96  GJF   Made __lc_handle and __lc_codepage _CRTIMP. Also, 
*                       cleaned up the formatting a bit.
*       01-31-97  RDK   Changed MAX_CP_LENGTH to 8 from 5 to accomodate up to
*                       7-digit codepages, e.g., 5-digit ISO codepages.
*       02-05-97  GJF   Cleaned out obsolete support for DLL_FOR_WIN32S and 
*                       _NTSDK.
*       01-12-98  GJF   Added __lc_collate_cp.
*       09-10-98  GJF   Added support for per-thread locale information.
*       11-06-98  GJF   Doubled MAX_CP_LEN (8 was a little small).
*       03-25-99  GJF   More reference counters for threadlocinfo stuff
*       04-24-99  PML   Added lconv_intl_refcount to threadlocinfo.
*       26-01-00  GB    Added __lc_clike.
*       06-08-00  PML   Rename THREADLOCALEINFO to _THREADLOCALEINFO.
*       09-06-00  GB    deleted _wctype and _pwctype from threadlocinfo.
*       01-29-01  GB    Added _func function version of data variable used in
*                       msvcprt.lib to work with STATIC_CPPLIB
*       03-25-01  PML   Add ww_caltype & ww_lcid to __lc_time_data (vs7#196892)
*       11-12-01  GB    Implemented new locale with true per thread locale
*                       capablity.
*       01-30-02  GB    Added per thread data for setlocale stuff that were
*                       static in functions. Added setloc_struct
*       04-25-02  GB    Added _COFFSET for use in setloc.c and initctype.c
*       03-08-04  MSL   Converted locale struct to use native not Win32 types to allow 
*                       external exposure
*       03-24-04  MSL   Fixed char type in mbc info to be unsigned, resolves
*                       Lots of subtle bugs
*                       VSW#251689
*       04-01-04  SJ    Design Changes in perthread locale - VSW#141214
*       04-07-04  MSL   Double slash removal
*       06-04-04  SJ    VSW#314764 - Adding a _locale_t param to __getlocaleinfo
*       10-08-04  ESC   Protect against -Zp with pragma pack
*       11-18-04  ATC   SAL Annotations for CRT Headers
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*                       Packing fix
*       04-14-05  MSL   Intellisense locale name cleanup
*
****/

#pragma once

#ifndef _INC_SETLOCAL
#define _INC_SETLOCAL

#ifndef _CRTBLD
/*
 * This is an internal C runtime header file. It is used when building
 * the C runtimes only. It is not to be used as a public header file.
 */
#error ERROR: Use of C runtime library internal header file.
#endif  /* _CRTBLD */

#ifdef  __cplusplus
extern "C" {
#endif

#include <crtdefs.h>
#include <cruntime.h>
#include <oscalls.h>
#include <limits.h>

#pragma pack(push,_CRT_PACKING)

/* Define _CRTIMP */

#ifndef _CRTIMP
#ifdef  CRTDLL
#define _CRTIMP __declspec(dllexport)
#else   /* ndef CRTDLL */
#ifdef  _DLL
#define _CRTIMP __declspec(dllimport)
#else   /* ndef _DLL */
#define _CRTIMP
#endif  /* _DLL */
#endif  /* CRTDLL */
#endif  /* _CRTIMP */

#define _CLOCALECP      CP_ACP  /* "C" locale Code page */
#define _COFFSET    127     /* offset to where ctype will point,
                               look in initctype on how it is being
                               used
                               introduced so that pctype can work with unsigned
                               char types and EOF, used only in initctype and
                               setloc.c
                               */

#define _PER_THREAD_LOCALE_BIT  0x2
#define _GLOBAL_LOCALE_BIT      0x1


/* LC_TIME localization structure */

#ifndef __LC_TIME_DATA
struct __lc_time_data {
        char *wday_abbr[7];
        char *wday[7];
        char *month_abbr[12];
        char *month[12];
        char *ampm[2];
        char *ww_sdatefmt;
        char *ww_ldatefmt;
        char *ww_timefmt;
        int  ww_caltype;
        int  refcount;
        wchar_t *_W_wday_abbr[7];
        wchar_t *_W_wday[7];
        wchar_t *_W_month_abbr[12];
        wchar_t *_W_month[12];
        wchar_t *_W_ampm[2];
        wchar_t *_W_ww_sdatefmt;
        wchar_t *_W_ww_ldatefmt;
        wchar_t *_W_ww_timefmt;
        wchar_t* _W_ww_locale_name;
};
#define __LC_TIME_DATA
#endif


#define MAX_LANG_LEN        64  /* max language name length */
#define MAX_CTRY_LEN        64  /* max country name length */
#define MAX_MODIFIER_LEN    0   /* max modifier name length - n/a */
#define MAX_LC_LEN          (MAX_LANG_LEN+MAX_CTRY_LEN+MAX_MODIFIER_LEN+3)
                                /* max entire locale string length */
#define MAX_CP_LEN          16  /* max code page name length */
#define CATNAMES_LEN        57  /* "LC_COLLATE=;LC_CTYPE=;..." length */

#define LC_INT_TYPE         0
#define LC_STR_TYPE         1
#define LC_WSTR_TYPE        2

#ifndef _SETLOC_STRUCT_DEFINED
struct _is_ctype_compatible {
        unsigned long id;
        int is_clike;
};

typedef struct setloc_struct {
    /* getqloc static variables */
    wchar_t *pchLanguage;
    wchar_t *pchCountry;
    int iLocState;
    int iPrimaryLen;
    BOOL bAbbrevLanguage;
    BOOL bAbbrevCountry;
    UINT        _cachecp;
    wchar_t     _cachein[MAX_LC_LEN];
    wchar_t     _cacheout[MAX_LC_LEN];
    /* _setlocale_set_cat (LC_CTYPE) static variable */
    struct _is_ctype_compatible _Loc_c[5];
    wchar_t _cacheLocaleName[LOCALE_NAME_MAX_LENGTH];
} _setloc_struct, *_psetloc_struct;
#define _SETLOC_STRUCT_DEFINED
#endif

#ifndef _THREADLOCALEINFO
typedef struct localerefcount {
        char *locale;
        wchar_t *wlocale;
        int *refcount;
        int *wrefcount;
} locrefcount;

typedef struct threadlocaleinfostruct {
        int refcount;
        unsigned int lc_codepage;
        unsigned int lc_collate_cp;
        unsigned int lc_time_cp;
        locrefcount lc_category[6];
        int lc_clike;
        int mb_cur_max;
        int * lconv_intl_refcount;
        int * lconv_num_refcount;
        int * lconv_mon_refcount;
        struct lconv * lconv;
        int * ctype1_refcount;
        unsigned short * ctype1;
        const unsigned short * pctype;
        const unsigned char * pclmap;
        const unsigned char * pcumap;
        struct __lc_time_data * lc_time_curr;
        wchar_t * locale_name[6];
} threadlocinfo;
#define _THREADLOCALEINFO
#endif

#ifndef _THREADMBCINFO
typedef struct threadmbcinfostruct {
        int refcount;
        int mbcodepage;
        int ismbcodepage;
        unsigned short mbulinfo[6];
        unsigned char mbctype[257];
        unsigned char mbcasemap[256];
        const wchar_t *mblocalename;
} threadmbcinfo;
#define _THREADMBCINFO
#endif

typedef struct tagLC_STRINGS {
        wchar_t szLanguage[MAX_LANG_LEN];
        wchar_t szCountry[MAX_CTRY_LEN];
        wchar_t szCodePage[MAX_CP_LEN];
        wchar_t szLocaleName[LOCALE_NAME_MAX_LENGTH];
} LC_STRINGS, *LPLC_STRINGS;

extern pthreadlocinfo __ptlocinfo;
pthreadlocinfo __cdecl __updatetlocinfo(void);
void __cdecl _setptlocale(int);

/* We have these as globals only for single threaded model. to improve performance */
#ifndef _M_CEE_PURE
_CRTIMP extern struct lconv * __lconv;  /* pointer to current lconv structure */
#endif

#define __LC_HANDLE(ptloci)     (ptloci)->lc_handle
#define __LC_CODEPAGE(ptloci)   (ptloci)->lc_codepage
#define __LC_COLLATE_CP(ptloci) (ptloci)->lc_collate_cp
#define __LC_CLIKE(ptloci)      (ptloci)->lc_clike
#define __LC_TIME_CURR(ptloci)  (ptloci)->lc_time_curr
#define __LCONV(ptloci)         (ptloci)->lconv

/* These functions are for enabling STATIC_CPPLIB functionality */
_CRTIMP wchar_t** __cdecl ___lc_locale_name_func(void);
_CRTIMP UINT __cdecl ___lc_codepage_func(void);
_CRTIMP UINT __cdecl ___lc_collate_cp_func(void);

BOOL __cdecl __get_qualified_locale(_In_opt_ const LPLC_STRINGS _LpInStr, _Out_opt_ UINT* _LpCodePage, _Out_opt_ LPLC_STRINGS _LpOutStr);

int __cdecl __getlocaleinfo (_In_opt_ _locale_t _Locale, _In_ int _Lc_type, LPCWSTR _LocaleName, LCTYPE _FieldType, _Out_ void * _Address);

/* lconv structure for the "C" locale */
extern struct lconv __lconv_c;

/* Initialization functions for locale categories */

int __cdecl __init_collate(_In_opt_ threadlocinfo *);
int __cdecl __init_ctype(_Inout_ threadlocinfo *_LocInfo);
int __cdecl __init_monetary(_Inout_ threadlocinfo *_LocInfo);
int __cdecl __init_numeric(_Inout_ threadlocinfo *_LocInfo);
int __cdecl __init_time(_Inout_ threadlocinfo *_LocInfo);
int __cdecl __init_dummy(_In_opt_ threadlocinfo *_LocInfo);

#ifdef  __cplusplus
}
#endif

#ifdef __cplusplus

#if !_PTD_LOCALE_SUPPORT
extern "C" pthreadmbcinfo __cdecl __get_mbcinfo_addref(void);
extern "C" void __cdecl __mbcinfo_release(pthreadmbcinfo* pptmbci);
extern "C" pthreadlocinfo __cdecl __get_locinfo_addref(void);
extern "C" void __cdecl __locinfo_release(pthreadlocinfo* pptloci);
#endif  /* _PTD_LOCALE_SUPPORT */

#if defined(_WIN32_WCE) && !defined(_WCE_FULLCRT)
extern "C" threadmbcinfo __initialmbcinfo;
#endif

class _LocaleUpdate
{
    _locale_tstruct localeinfo;
    _ptiddata ptd;
    bool updated;
    public:
    _LocaleUpdate(_locale_t plocinfo)
        : updated(false)
    {
        if (plocinfo == NULL)
        {
#if _PTD_LOCALE_SUPPORT
            ptd = _getptd();
            localeinfo.locinfo = ptd->ptlocinfo;
            localeinfo.mbcinfo = _GET_PTD_MBCINFO(ptd);

            __UPDATE_LOCALE(ptd, localeinfo.locinfo);
            __UPDATE_MBCP(ptd, localeinfo.mbcinfo);
            if (!(ptd->_ownlocale & _PER_THREAD_LOCALE_BIT))
            {
                ptd->_ownlocale |= _PER_THREAD_LOCALE_BIT;
                updated = true;
            }
#else
#ifdef _WCE_FULLCRT
            /* No per-thread locale support.  Use the global locales. */
            localeinfo.locinfo = __get_locinfo_addref();
            localeinfo.mbcinfo = __get_mbcinfo_addref();
            updated = true;
#else
            localeinfo.locinfo = &__initiallocinfo;
            localeinfo.mbcinfo = &__initialmbcinfo;
#endif

#endif  /* _PTD_LOCALE_SUPPORT */
        }
        else
        {
            localeinfo=*plocinfo;
        }
    }
    ~_LocaleUpdate()
    {
#if _PTD_LOCALE_SUPPORT
        if (updated)
            ptd->_ownlocale = ptd->_ownlocale & ~_PER_THREAD_LOCALE_BIT;
#else
#ifdef _WCE_FULLCRT
        if (updated)
        {
            __mbcinfo_release(&localeinfo.mbcinfo);
            __locinfo_release(&localeinfo.locinfo);
        }
#endif
#endif  /* _PTD_LOCALE_SUPPORT */
    }
    _locale_t GetLocaleT()
    {
        return &localeinfo;
    }
};
#endif

#if defined(_SAFECRT_IMPL)
#define _LOCALE_DATA dec_point
#elif defined (_WCE_BOOTCRT)
#define _LOCALE_DATA (NULL)
extern int __locale_changed;
#else
#define _LOCALE_DATA _loc_update.GetLocaleT()
#endif

#pragma pack(pop)

#endif  /* _INC_SETLOCAL */
