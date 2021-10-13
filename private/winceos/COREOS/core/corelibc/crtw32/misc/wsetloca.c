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
*wsetloca.c - Contains the wsetlocale function
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Contains the wsetlocale() function.
*
*Revision History:
*       03-21-89  JCR   Module created.
*       09-25-89  GJF   Fixed copyright. Checked for compatibility with Win 3.0
*       09-25-90  KRS   Major rewrite--support more than "C" locale if _INTL.
*       11-05-91  ETC   Get 09-25-90 working for C and "" locales; separate
*                       setlocal.h; add Init functions.
*       12-05-91  ETC   Separate nlsdata.c; add mt support; remove calls to
*                       itself.
*       12-20-91  ETC   Added _getlocaleinfo api interface function.
*       09-25-92  KRS   Fix for latest NLSAPI changes, etc.
*       01-25-93  KRS   Fix for latest changes, clean up code, etc.
*       02-02-93  CFW   Many modifications and bug fixes (all under _INTL).
*       02-08-93  CFW   Bug fixes and casts to avoid warnings (all under _INTL).
*       02-17-93  CFW   Removed re-call of init() functions in case of failure.
*       03-01-93  CFW   Check GetQualifiedLocale return value.
*       03-02-93  CFW   Added POSIX conformance, check environment variables.
*       03-09-93  CFW   Set CP to CP_ACP when changing to C locale.
*       03-17-93  CFW   Change expand to expandlocale, prepend _ to internal
*                       functions, lots of POSIX fixup.
*       03-23-93  CFW   Add _ to GetQualifiedLocale call.
*       03-24-93  CFW   Change to _get_qualified_locale, support ".codepage".
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       05-10-93  CFW   Disallow setlocale(LC_*, ".").
*       05-24-93  CFW   Clean up file (brief is evil).
*       09-15-93  CFW   Use ANSI conformant "__" names.
*       02-04-94  CFW   Remove unused param.
*       04-15-94  GJF   Moved prototypes for locale category initialization
*                       functions to setlocal.h. Made definitions for
*                       __lc_category, cacheid, cachecp, cachein and cacheout
*                       conditional on DLL_FOR_WIN32S. Made _clocalestr into
*                       a global for DLL_FOR_WIN32S so that crtlib.c may use
*                       it.
*       09-06-94  CFW   Remove _INTL switch.
*       09-06-94  CFW   Merge non-Win32.
*       01-10-95  CFW   Debug CRT allocs.
*       09-25-95  GJF   New locking scheme for functions which set or 
*                       reference locale information.
*       05-02-96  SKS   Variables _setlc_active and __unguarded_readlc_active
*                       are used by MSVCP42*.DLL and so must be _CRTIMP.
*       07-09-97  GJF   Made __lc_category selectany. Also, removed obsolete
*                       DLL_FOR_WIN32S support.
*       01-12-98  GJF   Use _lc_collate_cp codepage.
*       02-27-98  RKP   Add 64 bit support.
*       09-10-98  GJF   Revised multithread support based on threadlocinfo
*                       struct.
*       09-21-98  GJF   No need to lock or update threadlocinfo for setlocale
*                       calls which only read info.
*       11-06-98  GJF   In __lc_wcstolc, make sure you don't overflow
*                       names->szCodePage.
*       12-08-98  GJF   Fixed __updatetlocinfo (several errors).
*       01-04-99  GJF   Changes for 64-bit size_t.
*       01-18-99  GJF   In MT models, setlocale needs to check if the old 
*                       __ptlocinfo needs to be freed up. Also, unrelated,
*                       have _wsetlocale_get_all return NULL if malloc fails.
*       03-02-99  KRS   Partially back out previous fix for now. (per BryanT)
*       03-20-99  GJF   Added more reference counters (restoring fix)
*       04-24-99  PML   Added __lconv_intl_refcount
*       01-26-00  GB    Modified _setlocale_cat. Added _first_127char,
*                       _ctype_loc_style and __lc_clike
*       08-18-00  GB    Fixed problems with __lc_clike stuff.
*       09-06-00  GB    Made pwctype independent of locale.
*       10-12-00  GB    Compared requested locale to current locale for
*                       requested category in setlocale-set_cat. Performance
*                       enhancement.
*       11-05-00  PML   Fixed double-free of __lconv_mon_refcount and
*                       lconv_num_refcount (vs7#181380)
*       01-29-01  GB    Added _func function version of data variable used in
*                       msvcprt.lib to work with STATIC_CPPLIB
*       07-31-01  PML   setlocale(...,NULL) needs to be under lock (vs7#283330)
*       11-06-01  GB    Added _locterm for debug build to minimize leaks
*       11-12-01  GB    Implemented new locale with true per thread locale
*                       capablity.
*       01-30-02  GB    Replaced all the static global variables with per thread
*                       variables inside setloc_struct
*       04-04-02  GB    Fixed Bufferoverrun problem in __lc_wcstolc(VS7#342716).
*       04-25-02  GB    Increased the size of _ctype perthread variable and pointed
*                       it at begin+_COFFSET position so that isxxx macro would work
*                       same for signed char, usigned char and EOF.
*       05-06-02  MSL   Fix buffer overrun in setlocale for malicious input 
*                       VS7 526723
*       05-28-02  GB    Removed the _ctype and replaced it with __newctype.
*       06-27-02  GB    Fixed problems with __lc_clike cachae and checking.
*       06-27-03  GB    Added __set_global_locale function.
*       11-02-03  AC    Added validation.
*       12-23-03  AJS   Fixed caching code so that input string not truncated
*                       VSWhidbey#188934
*       03-08-04  MSL   Removed some unnecessary casts
*       03-13-04  MSL   Assorted prefast fixes
*       04-01-04  SJ    VSW#247900 - returning current status from 
*                       _configthreadlocale
*       09-08-04  AC    Renamed __get_current_locale to _get_current_locale;
*                       renamed __create_locale to _create_locale;
*                       renamed __free_locale to _free_locale;
*                       the old versions will be removed in the next LKG.
*                       VSW#247045
*       10-09-04  MSL   Prefast abort on locale failure
*       09-28-04  AC    Removed __get_current_locale, __create_locale, __free_locale
*                       VSW#365372
*       11-07-04  MSL   Add ability to set global locale status from startup code
*       02-11-05  AC    Remove redundant #pragma section
*                       VSW#445138
*       05-13-05  MSL   Make locale table const to avoid func ptr attack
*       05-22-05  PML   Lock against race condition in _wsetlocale (vsw#73865)
*                       Also eliminate _wsetlocale memory leak (vsw#498525)
*       05-23-05  PML   _lock_locale has been useless for years (vsw#497047)
*
****/

#include <locale.h>
#include <internal.h>
#include <cruntime.h>
#include <setlocal.h>
#include <mtdll.h>
#include <malloc.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h> /* for strtol */
#include <dbgint.h>
#include <ctype.h>
#include <awint.h>
#include <mbctype.h>
#include <rterr.h>

void * __cdecl __removelocaleref(pthreadlocinfo);
void __cdecl __addlocaleref(pthreadlocinfo);
void __cdecl __freetlocinfo(pthreadlocinfo);
LPWSTR __cdecl __copy_locale_name(LPCWSTR localeName);

/* "C" locale */
extern wchar_t __wclocalestr[];

#include <sect_attribs.h>
void __cdecl _locterm(void);

_CRTALLOC(".CRT$XPX") static _PVFV pterm = _locterm;

__declspec(selectany) struct {
        const wchar_t * catname;
        wchar_t * locale;
        int (* init)(threadlocinfo *);
} const __lc_category[LC_MAX-LC_MIN+1] = {
        /* code assumes locale initialization is "__wclocalestr" */
        { L"LC_ALL",     NULL,             __init_dummy /* never called */ },
        { L"LC_COLLATE", __wclocalestr,    __init_collate  },
        { L"LC_CTYPE",   __wclocalestr,    __init_ctype    },
        { L"LC_MONETARY",__wclocalestr,    __init_monetary },
        { L"LC_NUMERIC", __wclocalestr,    __init_numeric  },
        { L"LC_TIME",    __wclocalestr,    __init_time }
};

static const char _first_127char[] = {
        1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17,
        18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34,
        35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68,
        69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85,
        86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100,101,102,
        103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,
        120,121,122,123,124,125,126,127
};

extern threadlocinfo __initiallocinfo;
extern const unsigned short _wctype[];
static const short *_ctype_loc_style = _wctype+2;
int __locale_changed=0;

/*
 * Flag indicating whether or not setlocale() is active. Its value is the 
 * number of setlocale() calls currently active.
 *
 * WARNING - This flag and query function are obsolete, and are here only to
 * preserve existing exports for binary compatibility.
 */
_CRTIMP int __setlc_active;
_CRTIMP int __cdecl ___setlc_active_func(void)
{
    return __setlc_active;
}

/*
 * Flag indicating whether or not a function which references the locale
 * without having locked it is active. Its value is the number of such
 * functions.
 *
 * WARNING - This flag and query function are obsolete, and are here only to
 * preserve existing exports for binary compatibility.
 */
_CRTIMP int __unguarded_readlc_active;
_CRTIMP int * __cdecl ___unguarded_readlc_active_add_func(void)
{
    return &__unguarded_readlc_active;
}

/* helper function prototypes */
wchar_t * _expandlocale(const wchar_t *, wchar_t *, size_t, wchar_t *, size_t, UINT *);
void _wcscats(wchar_t *, size_t, int, ...);
void __lc_lctowcs(wchar_t *, size_t, const LC_STRINGS *);
int __lc_wcstolc(LC_STRINGS *, const wchar_t *);
static wchar_t * __cdecl _wsetlocale_set_cat(pthreadlocinfo, int, const wchar_t *);
static wchar_t * __cdecl _wsetlocale_get_all(pthreadlocinfo);
static pthreadlocinfo __cdecl _updatetlocinfo_nolock(void);
static wchar_t * __cdecl _wsetlocale_nolock(pthreadlocinfo, int, const wchar_t *);
int __cdecl _setmbcp_nolock(int, pthreadmbcinfo);
pthreadlocinfo __cdecl _updatetlocinfoEx_nolock(pthreadlocinfo *, pthreadlocinfo);

/***
*
* _copytlocinfo_nolock(pthreadlocinfo ptlocid, pthreadlocinfo ptlocis)
*
* Purpose:
*       Copy the contents of ptlocis to ptlocid and increase the refcount of all the
*       elements in ptlocid after copy.
*
******************************************************************************/
static void __cdecl _copytlocinfo_nolock(
    pthreadlocinfo ptlocid,
    pthreadlocinfo ptlocis)
{
    if (ptlocis != NULL && ptlocid != NULL && ptlocid != ptlocis) {
        *ptlocid = *ptlocis;
        ptlocid->refcount = 0;
        __addlocaleref(ptlocid);
    }
}

/***
* _configthreadlocale(int i)
*
* Purpose:
*       To set _ownlocale flag on threadinfo sturct. If this flag is set, this thread
*       is going own it's threadlocinfo struct. Setlocale call on other thread will have
*       no effect on this thread's locale. If 0 is passed then nothing is changed, but
*       current status is returned.
* Exit   :
*       Returns the current status - i.e. per thread locale is enabled or not.
*
*******************************************************************************/
int __cdecl _configthreadlocale(int i)
{
#if _PTD_LOCALE_SUPPORT
    /*
     * ownlocale flag struct:
     * bits: 000000000000000000000000000000P1 
     * P is set when _ENABLE_PER_THREAD_LOCALE is called for this thread
     * or _ENABLE_PER_THREAD_LOCALE_NEW was set when this thread was created.
     *
     * __globallocalestatus structure:
     * bits: 11111111111111111111111111111N1G
     * G is set if _ENABLE_PER_THREAD_LOCALE_GLOBAL is set.
     * G is 0 if _ENABLE_PER_THREAD_LOCALE_GLOBAL is not set.
     * N is set if _ENABLE_PER_THREAD_LOCALE_NEW is set.
     * N is 0 if _ENABLE_PER_THREAD_LOCALE_NEW is not set.
     */
    _ptiddata ptd = _getptd();
    int retval = (ptd->_ownlocale & _PER_THREAD_LOCALE_BIT)==0 ? _DISABLE_PER_THREAD_LOCALE:_ENABLE_PER_THREAD_LOCALE;
   
    switch(i)
    {
        case _ENABLE_PER_THREAD_LOCALE :
            ptd->_ownlocale = ptd->_ownlocale | _PER_THREAD_LOCALE_BIT;
            break;

        case _DISABLE_PER_THREAD_LOCALE : 
            ptd->_ownlocale = ptd->_ownlocale & ~_PER_THREAD_LOCALE_BIT;
            break;
            
        case 0 :
            break;

        /* used only during dll startup for linkopt */
        case -1 : 
            __globallocalestatus=-1;
            break;

        default :
            _VALIDATE_RETURN(("Invalid parameter for _configthreadlocale",0),EINVAL,-1);
            break;
    }

    return retval;
#else
    return _DISABLE_PER_THREAD_LOCALE;
#endif  /* _PTD_LOCALE_SUPPORT */
    
}


void __cdecl _locterm(void)
{
    if (__ptlocinfo != &__initiallocinfo) {
        _mlock(_SETLOCALE_LOCK);
        __try 
        {
            __ptlocinfo = _updatetlocinfoEx_nolock(&__ptlocinfo, &__initiallocinfo);
        }
        __finally
        {
            _munlock(_SETLOCALE_LOCK);
        }
    }
}

/***
* void sync_legacy_variables_lk()
*
* Purpose:
*   Syncs all the legacy locale specific variables to the global locale.
*
*******************************************************************************/
static __inline void sync_legacy_variables_lk()
{
    __lconv = __ptlocinfo->lconv;
    _pctype = __ptlocinfo->pctype;
    __mb_cur_max = __ptlocinfo->mb_cur_max;
}
/***
*_free_locale() - free threadlocinfo
*
*Purpose:
*       Free up the per-thread locale info structure specified by the passed
*       pointer.
*
*Entry:
*       pthreadlocinfo ptloci
*
*Exit:
*
*Exceptions:
*
*******************************************************************************/

void __cdecl _free_locale(
        _locale_t plocinfo
        )
{
    if (plocinfo != NULL)
    {
        _mlock(_MB_CP_LOCK);
        __try
        {
            if (plocinfo->mbcinfo != NULL &&
                    InterlockedDecrement(&(plocinfo->mbcinfo->refcount)) == 0 &&
                    plocinfo->mbcinfo != &__initialmbcinfo )
            {
                _free_crt(plocinfo->mbcinfo);
            }
        }
        __finally
        {
            _munlock(_MB_CP_LOCK);
        }

        if (plocinfo->locinfo != NULL)
        {
            /*
             * this portion has to be in locale lock as there may be case when
             * not this thread but some other thread is still holding to this
             * locale and is also trying to free this locale. In this case
             * we may end up leaking memory.
             */

            _mlock(_SETLOCALE_LOCK);
            __try
            {
                __removelocaleref(plocinfo->locinfo);
                if ( (plocinfo->locinfo != NULL) &&
                     (plocinfo->locinfo->refcount == 0) &&
                     (plocinfo->locinfo != &__initiallocinfo) )
                    __freetlocinfo(plocinfo->locinfo);
            }
            __finally
            {
                _munlock(_SETLOCALE_LOCK);
            }
        }

        _free_crt(plocinfo);
    }
}

/* __free_locale will be removed in the next LKG */
void __cdecl __free_locale(
        _locale_t plocinfo
        )
{
    _free_locale(plocinfo);
}

/***
* _locale_t _wcreate_locale(int category, char *wlocale) -
*    Set one or all locale categories of global locale structure
*
* Purpose:
*       The _wcreate_locale() routine allows the user to create a _locale_t
*       object that can be used with other locale functions.
*
* Entry:
*       int category = One of the locale categories defined in locale.h
*       char *locale = String identifying a specific locale.
*
* Exit:
*       If supplied locale pointer != NULL:
*
*           If locale string is '\0', set locale to default.
*
*           If desired setting can be honored, return a pointer to the
*           locale string for the appropriate category.
*
*           If desired setting can NOT be honored, return NULL.
*
* Exceptions:
*       Compound locale strings of the form "LC_COLLATE=xxx;LC_CTYPE=xxx;..."
*       are allowed for the LC_ALL category.  This is to support the ability
*       to restore locales with the returned string, as specified by ANSI.
*       Setting the locale with a compound locale string will succeed unless
*       *all* categories failed.  The returned string will reflect the current
*       locale.  For example, if LC_CTYPE fails in the above string, setlocale
*       will return "LC_COLLATE=xxx;LC_CTYPE=yyy;..." where yyy is the
*       previous locale (or the C locale if restoring the previous locale
*       also failed).  Unrecognized LC_* categories are ignored.
*
*******************************************************************************/

_locale_t __cdecl _wcreate_locale(
        int _category,
        const wchar_t *_wlocale
        )
{
    _locale_t retval = NULL;

    /* Validate input */
    if ( (_category < LC_MIN) || (_category > LC_MAX) || _wlocale == NULL)
        return NULL;

    if ((retval = _calloc_crt(sizeof(_locale_tstruct), 1)) == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }
    if ((retval->locinfo = _calloc_crt(sizeof(threadlocinfo), 1)) == NULL)
    {
        _free_crt(retval);
        errno = ENOMEM;
        return NULL;
    }
    if ((retval->mbcinfo = _calloc_crt(sizeof(threadmbcinfo), 1)) == NULL)
    {
        _free_crt(retval->locinfo);
        _free_crt(retval);
        errno = ENOMEM;
        return NULL;
    }
    _copytlocinfo_nolock(retval->locinfo, &__initiallocinfo);

    if (_wsetlocale_nolock(retval->locinfo, _category, _wlocale) == NULL)
    {
        __removelocaleref(retval->locinfo);
        __freetlocinfo(retval->locinfo);

        _free_crt(retval);
        retval = NULL;
    }
    else
    {
        if (_setmbcp_nolock(retval->locinfo->lc_codepage, retval->mbcinfo) != 0)
        {
            _free_crt(retval->mbcinfo);
            __removelocaleref(retval->locinfo);
            __freetlocinfo(retval->locinfo);
            _free_crt(retval);
            retval = NULL;
        }
        else
        {
            retval->mbcinfo->refcount = 1;
        }
    }

    return retval;
}

/***
* _locale_t _create_locale(int category, char *locale) -
*    Set one or all locale categories of global locale structure
****/
_locale_t __cdecl _create_locale(
        int _category,
        const char *_locale
        )
{
    wchar_t _wlocale[MAX_LC_LEN];

    /* Early input validation */
    if ( (_category < LC_MIN) || (_category > LC_MAX) || _locale == NULL)
        return NULL;

    if ( MultiByteToWideChar(CP_ACP, 0, _locale, -1, _wlocale, _countof(_wlocale)) == 0 )
    { // conversion to wide char failed
        return NULL;
    }

    return _wcreate_locale(_category, _wlocale);
}

/* __create_locale will be removed in the next LKG */
_locale_t __cdecl __create_locale(
        int _category,
        const char *_locale
        )
{
    return _create_locale(_category, _locale);
}

/*
* _locale_t _get_current_locale() -
*    Gets the current locale setting.
*
* Purpose:
*       Gets the current locale setting for this thread. Returns locale
*       in form of _locale_t, which then can be used with other locale
*       aware string funcitons.
*
*/

_locale_t __cdecl _get_current_locale(void)
{
    _locale_t retval = NULL;
#if _PTD_LOCALE_SUPPORT
    _ptiddata ptd = _getptd();
#endif  /* _PTD_LOCALE_SUPPORT */

    if ((retval = _calloc_crt(sizeof(_locale_tstruct), 1)) == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

#if _PTD_LOCALE_SUPPORT
    __updatetlocinfo();
    __updatetmbcinfo();
    /*
     * No one can free the data pointed to by ptlocinfo while we're copying
     * it, since we're copying this thread's ptlocinfo, which won't be updated
     * during the copy.  So there are no worries about it being freed from
     * under us.  We still need a lock while adding a reference for the new
     * copy, though, because of the race condition found in _wsetlocale.
     */
    retval->locinfo = ptd->ptlocinfo;
    retval->mbcinfo = ptd->ptmbcinfo;
#endif  /* _PTD_LOCALE_SUPPORT */
    _mlock(_SETLOCALE_LOCK);
    __try {
#if !_PTD_LOCALE_SUPPORT
        retval->locinfo = __ptlocinfo;
#endif  /* _PTD_LOCALE_SUPPORT */
        __addlocaleref(retval->locinfo);
    }
    __finally {
        _munlock(_SETLOCALE_LOCK);
    }
    _mlock(_MB_CP_LOCK);
    __try
    {
#if !_PTD_LOCALE_SUPPORT
        retval->mbcinfo = __ptmbcinfo;
#endif  /* _PTD_LOCALE_SUPPORT */
        InterlockedIncrement(&(retval->mbcinfo->refcount));
    }
    __finally
    {
        _munlock(_MB_CP_LOCK);
    }

    return retval;
}

/* __get_current_locale will be removed in the next LKG */
_locale_t __cdecl __get_current_locale(void)
{
    return _get_current_locale();
}

/*
*char * setlocale(int category, char *locale) - Set one or all locale categories
*
*Purpose:
*       The setlocale() routine allows the user to set one or more of
*       the locale categories to the specific locale selected by the
*       user.  [ANSI]
*
*       NOTE: Under !_INTL, the C libraries only support the "C" locale.
*       Attempts to change the locale will fail.
*
*Entry:
*       int category = One of the locale categories defined in locale.h
*       char *locale = String identifying a specific locale or NULL to
*                  query the current locale.
*
*Exit:
*       If supplied locale pointer == NULL:
*
*           Return pointer to current locale string and do NOT change
*           the current locale.
*
*       If supplied locale pointer != NULL:
*
*           If locale string is '\0', set locale to default.
*
*           If desired setting can be honored, return a pointer to the
*           locale string for the appropriate category.
*
*           If desired setting can NOT be honored, return NULL.
*
*Exceptions:
*       Compound locale strings of the form "LC_COLLATE=xxx;LC_CTYPE=xxx;..."
*       are allowed for the LC_ALL category.  This is to support the ability
*       to restore locales with the returned string, as specified by ANSI.
*       Setting the locale with a compound locale string will succeed unless
*       *all* categories failed.  The returned string will reflect the current
*       locale.  For example, if LC_CTYPE fails in the above string, setlocale
*       will return "LC_COLLATE=xxx;LC_CTYPE=yyy;..." where yyy is the
*       previous locale (or the C locale if restoring the previous locale
*       also failed).  Unrecognized LC_* categories are ignored.
*
*/

wchar_t * __cdecl _wsetlocale (
        int _category,
        const wchar_t *_wlocale
        )
{
    wchar_t * retval=NULL;
    pthreadlocinfo ptloci = NULL;
#if _PTD_LOCALE_SUPPORT
    _ptiddata ptd;
#endif  /* _PTD_LOCALE_SUPPORT */

    /* Validate category */
    _VALIDATE_RETURN(LC_MIN <= _category && _category <= LC_MAX, EINVAL, NULL);

#if _PTD_LOCALE_SUPPORT
    ptd = _getptd();

    __updatetlocinfo();
    // Note here that we increment the _ownlocale for this thread. We need this
    // to make sure that the locale is not updated to some other locale by call to
    // stricmp().
    // Don't set any flag that aligns with N, P or G
    ptd->_ownlocale |= 0x10;
#endif  /* _PTD_LOCALE_SUPPORT */
    __try {
        if ((ptloci = _calloc_crt(sizeof(threadlocinfo), 1)) != NULL)
        {
            /*
             * No one can free the data pointed to by ptlocinfo while we're
             * copying it, since we're copying this thread's ptlocinfo, which
             * won't be updated during the copy.  So there are no worries
             * about it being freed from under us.  We still need a lock while
             * making the copy, though, because of the race condition found in
             * _wsetlocale.
             */
            _mlock(_SETLOCALE_LOCK);
            __try {
#if _PTD_LOCALE_SUPPORT
                _copytlocinfo_nolock(ptloci, ptd->ptlocinfo);
#else
                _copytlocinfo_nolock(ptloci, __ptlocinfo);
#endif  /* _PTD_LOCALE_SUPPORT */
            }
            __finally {
                _munlock(_SETLOCALE_LOCK);
            }

            if (( ptloci != NULL) && (retval = _wsetlocale_nolock(ptloci, _category, _wlocale))) {
                /*
                * What we are trying here is that if no call has been made to
                * setlocale to change locale from "C" locale to some other locale
                * we keep __locale_changed = 0. Other funcitons depending on locale
                * use this variable to optimize performance for C locale which is
                * normally the case in 90% of the applications.
                */
                if (_wlocale != NULL && wcscmp(_wlocale, __wclocalestr) != 0)
                {
                    __locale_changed = 1;
                }

                _mlock(_SETLOCALE_LOCK);
                __try {
#if _PTD_LOCALE_SUPPORT
                    (void)_updatetlocinfoEx_nolock(&ptd->ptlocinfo, ptloci);
                    __removelocaleref(ptloci);
                    // Note that after incrementing _ownlocale, if this thread doesn't
                    // have it's own locale, _ownlocale variable should be 1.
                    if (!(ptd->_ownlocale & _PER_THREAD_LOCALE_BIT) &&
                        !(__globallocalestatus & _GLOBAL_LOCALE_BIT)) {
                        (void)_updatetlocinfoEx_nolock(&__ptlocinfo, ptd->ptlocinfo);
                        sync_legacy_variables_lk();
                    }
#else
                    (void)_updatetlocinfoEx_nolock(&__ptlocinfo, ptloci);
                    __removelocaleref(ptloci);
                    sync_legacy_variables_lk();
#endif  /* _PTD_LOCALE_SUPPORT */
                } __finally {
                    _munlock(_SETLOCALE_LOCK);
                }
            } else {
                __removelocaleref(ptloci);
                __freetlocinfo(ptloci);
            }
        }
    } __finally {
#if _PTD_LOCALE_SUPPORT
        // Undo the previous action.
        ptd->_ownlocale &= ~0x10;
#endif  /* _PTD_LOCALE_SUPPORT */
    }

    return retval;
}

static wchar_t * __cdecl _wsetlocale_nolock(
        pthreadlocinfo ploci,
        int _category,
        const wchar_t *_wlocale
        )
{
    wchar_t * retval;
    /* Interpret locale */

    if (_category != LC_ALL)
    {
        retval = (_wlocale) ? _wsetlocale_set_cat(ploci, _category,_wlocale) :
            ploci->lc_category[_category].wlocale;

    } else { /* LC_ALL */
        wchar_t lctemp[MAX_LC_LEN];
        int i;
        int same = 1;
        int fLocaleSet = 0; /* flag to indicate if anything successfully set */

        if (_wlocale != NULL)
        {
            if ( (_wlocale[0]==L'L') && (_wlocale[1]==L'C') && (_wlocale[2]==L'_') )
            {
                /* parse compound locale string */
                size_t len;
                const wchar_t * p = _wlocale;  /* start of string to parse */
                const wchar_t * s;

                do {
                    s = wcspbrk(p, L"=;");

                    if ((s== NULL) || (!(len=(size_t)(s-p))) || (*s==L';'))
                        return NULL;  /* syntax error */

                    /* match with known LC_ strings, if possible, else ignore */
                    for (i=LC_ALL+1; i<=LC_MAX; i++)
                    {
                        if ((!wcsncmp(__lc_category[i].catname,p,len))
                            && (len==wcslen(__lc_category[i].catname)))
                        {
                            break;  /* matched i */
                        }
                    } /* no match if (i>LC_MAX) -- just ignore */

                    if ((!(len = wcscspn(++s, L";"))) && (*s != L';'))
                        return NULL;  /* syntax error */

                    if (i<=LC_MAX)
                    {
                        _ERRCHECK(wcsncpy_s(lctemp, _countof(lctemp), s, len));
                        lctemp[len]=L'\0';   /* null terminate string */

                        /* don't fail unless all categories fail */
                        if (_wsetlocale_set_cat(ploci, i,lctemp))
                            fLocaleSet++;       /* record a success */
                    }
                    if (*(p = s+len)!=L'\0')
                        p++;  /* skip ';', if present */

                } while (*p);

                retval = (fLocaleSet) ? _wsetlocale_get_all(ploci) : NULL;

            } else { /* simple LC_ALL locale string */

                wchar_t localeNameTemp[LOCALE_NAME_MAX_LENGTH];
                /* confirm locale is supported, get expanded locale */
                if (retval = _expandlocale(_wlocale, lctemp, _countof(lctemp), localeNameTemp, _countof(localeNameTemp), NULL))
                {
                    for (i=LC_MIN; i<=LC_MAX; i++)
                    {
                        if (i!=LC_ALL)
                        {
                            if (wcscmp(lctemp, ploci->lc_category[i].wlocale) != 0)
                            { // does not match the LC_ALL locale
                                if (_wsetlocale_set_cat(ploci, i, lctemp))
                                {
                                    fLocaleSet++;   /* record a success */
                                }
                                else
                                {
                                    same = 0;       /* record a failure */
                                }
                            }
                            else
                                fLocaleSet++;   /* trivial succcess */
                        }
                    }
                    if (same) /* needn't call setlocale_get_all() if all the same */
                    {
                        retval = _wsetlocale_get_all(ploci);
                        /* retval set above */
                    }
                    else
                        retval = (fLocaleSet) ? _wsetlocale_get_all(ploci) : NULL;
                }
            }
        } else { /* LC_ALL & NULL */
            retval = _wsetlocale_get_all (ploci);
        }
    }

    /* common exit point */
    return retval;
} /* setlocale */


static wchar_t * __cdecl _wsetlocale_set_cat (
        pthreadlocinfo ploci,
        int category,
        const wchar_t * wlocale
        )
{
    wchar_t * oldlocale;
    wchar_t * oldlocalename;
    UINT oldcodepage;

    UINT cptemp;
    wchar_t lctemp[MAX_LC_LEN];
    wchar_t localeNameString[LOCALE_NAME_MAX_LENGTH];
    wchar_t * pch = NULL;
    wchar_t * pch_cat_locale = NULL;
    size_t cch = 0;
    short out[sizeof(_first_127char)];
    int i;
    _ptiddata _ptd = _getptd();
    struct _is_ctype_compatible *_Loc_c = _ptd->_setloc_data._Loc_c; // __setloc_data._Loc_c is array
    int _LOC_CCACHE = _countof(_ptd->_setloc_data._Loc_c);
    struct _is_ctype_compatible buf1, buf2;

    if (!_expandlocale(wlocale, lctemp, _countof(lctemp), localeNameString, _countof(localeNameString), &cptemp))
        return NULL;

    // if this cateogory's locale hadn't changed
    if (wcscmp(lctemp, ploci->lc_category[category].wlocale) == 0)
    {
        return ploci->lc_category[category].wlocale;
    }

    cch = wcslen(lctemp) + 1;
    if ((pch = (wchar_t *)_malloc_crt(sizeof(int) + (cch * sizeof(wchar_t)))) == NULL)
        return NULL;  /* error if malloc fails */

    pch_cat_locale = pch + (sizeof(int) / sizeof(wchar_t));

     /* save for possible restore */
    oldlocale = ploci->lc_category[category].wlocale;
    oldlocalename = ploci->locale_name[category];
    oldcodepage = ploci->lc_codepage;

    /* update locale string */
    _ERRCHECK(wcscpy_s(pch_cat_locale, cch, lctemp));
    ploci->lc_category[category].wlocale = pch_cat_locale;

    /* Copy locale name */
    if (lctemp[0] == L'C' && lctemp[1] == L'\x0') // if "C" locale
        ploci->locale_name[category] = NULL;
    else
        ploci->locale_name[category] = __copy_locale_name(localeNameString);

    /* To speedup locale based comparisions, we identify if the current
     * local has first 127 character set same as CLOCALE. If yes then
     * ploci->lc_clike = TRUE. 
     */

    if (category==LC_CTYPE)
    {
        ploci->lc_codepage = cptemp;
        buf1 = _Loc_c[_LOC_CCACHE -1];
        /* brings the recently used codepage to the top. or else shifts
         * every thing down by one so that new _Loc_c can be placed at
         * the top.
         */
        for ( i = 0; i < _LOC_CCACHE; i++)
        {
            if (ploci->lc_codepage == _Loc_c[i].id)
            {
                /* We don't really want to swap cache around in case what we are looking
                 *  for is the first element of the cache
                 */
                if (i!=0)
                {
                    _Loc_c[0] = _Loc_c[i];
                    _Loc_c[i] = buf1;
                }
                break;
            }
            else
            {
                buf2 = _Loc_c[i];
                _Loc_c[i] = buf1;
                buf1 = buf2;
            }
        }
        if ( i == _LOC_CCACHE)
        {
            if ( __crtGetStringTypeA(NULL, CT_CTYPE1,
                                      _first_127char,
                                      sizeof(_first_127char),
                                      out,
                                      ploci->lc_codepage,
                                      TRUE ))
            {
                int j;
                for ( j = 0; j < sizeof(_first_127char); j++)
                    out[j] = out[j]&
                            (_UPPER|_LOWER|_DIGIT|_SPACE|_PUNCT|_CONTROL|_BLANK|_HEX|_ALPHA);
                if ( !memcmp(out, _ctype_loc_style, (sizeof(_first_127char)/sizeof(char))*sizeof(short)))
                {
                    _Loc_c[0].is_clike = TRUE;
                }
                else
                {
                    _Loc_c[0].is_clike = FALSE;
                }
            }
            else
                _Loc_c[0].is_clike = FALSE;
            _Loc_c[0].id = ploci->lc_codepage;
        }
        ploci->lc_clike = _Loc_c[0].is_clike;
    } /* category==LC_CTYPE */
    else if ( category == LC_COLLATE )
        ploci->lc_collate_cp = cptemp;
    else if ( category == LC_TIME )
        ploci->lc_time_cp = cptemp;

    if (__lc_category[category].init(ploci) != 0)
    {
        /* restore previous state */
        ploci->lc_category[category].wlocale = oldlocale;
        _free_crt(ploci->locale_name[category]);
        ploci->locale_name[category] = oldlocalename;
        _free_crt(pch);
        ploci->lc_codepage = oldcodepage;

        return NULL; /* error if non-zero return */
    }

    /* locale set up successfully */
    /* Cleanup */
    if ((oldlocale != __wclocalestr) &&
        (InterlockedDecrement(ploci->lc_category[category].wrefcount) == 0)
        )
    {
        _ASSERT(0);
        _free_crt(ploci->lc_category[category].wrefcount);
        _free_crt(ploci->lc_category[category].refcount);
        _free_crt(ploci->locale_name[category]);
        ploci->lc_category[category].wlocale = NULL;
        ploci->locale_name[category] = NULL;
    }
    if (pch) {
        *(int *)pch  = 1;
    }
    ploci->lc_category[category].wrefcount = (int *)pch;

    return ploci->lc_category[category].wlocale;
} /* _wsetlocale_set_cat */



static wchar_t * __cdecl _wsetlocale_get_all ( pthreadlocinfo ploci)
{
    int i;
    int same = 1;
    wchar_t *pch = NULL;
    size_t cch = 0;
    int *refcount = NULL;
    size_t refcountSize = 0;
    
    /* allocate memory if necessary */
    refcountSize = sizeof(int) + (sizeof(wchar_t) * (MAX_LC_LEN+1) * (LC_MAX-LC_MIN+1)) + (sizeof(wchar_t) * CATNAMES_LEN);
    if ( (refcount = _malloc_crt(refcountSize)) == NULL)
        return NULL;

    pch = ((wchar_t*)refcount) + (sizeof(int) / sizeof(wchar_t));
    cch = (refcountSize - sizeof(int)) / sizeof(wchar_t);
    *pch = L'\0';
    *refcount = 1;
    for (i=LC_MIN+1; ; i++)
    {
        _wcscats(pch, cch, 3, __lc_category[i].catname, L"=", ploci->lc_category[i].wlocale);
        if (i<LC_MAX)
        {
            _ERRCHECK(wcscat_s(pch, cch, L";"));
            if (wcscmp(ploci->lc_category[i].wlocale, ploci->lc_category[i+1].wlocale))
                same=0;
        }
        else
        {
            if (!same) {
                if (ploci->lc_category[LC_ALL].wrefcount != NULL &&
                    InterlockedDecrement(ploci->lc_category[LC_ALL].wrefcount) == 0) {
                    _ASSERT(0);
                    _free_crt(ploci->lc_category[LC_ALL].wrefcount);
                }
                if (ploci->lc_category[LC_ALL].refcount != NULL &&
                    InterlockedDecrement(ploci->lc_category[LC_ALL].refcount) == 0) {
                    _ASSERT(0);
                    _free_crt(ploci->lc_category[LC_ALL].refcount);
                }
                ploci->lc_category[LC_ALL].refcount = NULL;
                ploci->lc_category[LC_ALL].locale = NULL;
                ploci->lc_category[LC_ALL].wrefcount = refcount;
                return ploci->lc_category[LC_ALL].wlocale = pch;
            } else {
                _free_crt(refcount);
                if (ploci->lc_category[LC_ALL].wrefcount != NULL &&
                    InterlockedDecrement(ploci->lc_category[LC_ALL].wrefcount) == 0) {
                    _ASSERT(0);
                    _free_crt(ploci->lc_category[LC_ALL].wrefcount);
                }
                if (ploci->lc_category[LC_ALL].refcount != NULL &&
                    InterlockedDecrement(ploci->lc_category[LC_ALL].refcount) == 0) {
                    _ASSERT(0);
                    _free_crt(ploci->lc_category[LC_ALL].refcount);
                }
                ploci->lc_category[LC_ALL].refcount = NULL;
                ploci->lc_category[LC_ALL].locale = NULL;
                ploci->lc_category[LC_ALL].wrefcount = NULL;
                ploci->lc_category[LC_ALL].wlocale = NULL;
                return ploci->lc_category[LC_CTYPE].wlocale;
            }
        }
    }
} /* _wsetlocale_get_all */


wchar_t * _expandlocale (
        const wchar_t *expr,
        wchar_t * output,
        size_t sizeInChars,
        wchar_t * localeNameOutput,
        size_t localeNameSizeInChars,
        UINT * cp
        )
{
    _psetloc_struct _psetloc_data = &_getptd()->_setloc_data;
    UINT *pcachecp = &_psetloc_data->_cachecp;
    wchar_t *cachein = _psetloc_data->_cachein;
    size_t cacheinLen = _countof(_psetloc_data->_cachein);
    wchar_t *cacheout = _psetloc_data->_cacheout;
    size_t cacheoutLen = _countof(_psetloc_data->_cacheout);
    size_t charactersInExpression = 0;
    int  iCodePage = 0;

    if (!expr)
        return NULL; /* error if no input */

    // Store the last successfully obtained locale name
    _ERRCHECK(wcsncpy_s(localeNameOutput, localeNameSizeInChars,_psetloc_data->_cacheLocaleName, _countof(_psetloc_data->_cacheLocaleName)));

    // if "C" locale
    if (((*expr==L'C') && (!expr[1])))
    {
        
        _ERRCHECK(wcscpy_s(output, sizeInChars, L"C"));
        if (cp)
        {
            *cp = CP_ACP; /* return to ANSI code page */
        }
        return output; /* "C" */
    }

    /* first, make sure we didn't just do this one */
    charactersInExpression = wcslen(expr);
    if (charactersInExpression >= MAX_LC_LEN ||       /* we would never have cached this */
        (wcscmp(cacheout,expr) && wcscmp(cachein,expr)))
    {
        /* do some real work */
        LC_STRINGS names;
        const wchar_t *source = L"";
        size_t charactersInSource = 0;

        /* begin: cache atomic section */

        if (__lc_wcstolc(&names, expr) == 0 &&                 /* no syntax error */
            __get_qualified_locale(&names, pcachecp , &names))  /* locale recognized */
        {
            __lc_lctowcs(cacheout, cacheoutLen, &names);
            if (localeNameOutput)
                _ERRCHECK(wcsncpy_s(localeNameOutput, localeNameSizeInChars, names.szLocaleName, wcslen(names.szLocaleName) + 1));
        }
        else if (__crtIsValidLocaleName(expr))
        {
            // Get default ANSI code page - don't fail if we can't get the value
            if (__crtGetLocaleInfoEx(expr, LOCALE_IDEFAULTANSICODEPAGE | LOCALE_RETURN_NUMBER,
                                        (LPWSTR) &iCodePage, sizeof(iCodePage) / sizeof(wchar_t)) == 0
                    || iCodePage == 0) // for locales have no assoicated ANSI codepage
            {
                iCodePage = GetACP();
            }

            // Copy code page
            *pcachecp = (WORD) iCodePage;

            /* Copy the locale name to the output */
            _ERRCHECK(wcsncpy_s(cacheout, cacheoutLen, expr, charactersInExpression + 1));

            // Also store to locale name output string
            _ERRCHECK(wcsncpy_s(localeNameOutput, localeNameSizeInChars, expr, charactersInExpression + 1));

            // Finally, make sure the locale name is cached for subsequent use
            _ERRCHECK(wcsncpy_s(_psetloc_data->_cacheLocaleName, _countof(_psetloc_data->_cacheLocaleName), expr, charactersInExpression + 1));
        }
        else
        {
            // restore locale name to cache
            _ERRCHECK(wcsncpy_s(_psetloc_data->_cacheLocaleName, _countof(_psetloc_data->_cacheLocaleName), localeNameOutput, wcslen(localeNameOutput) + 1));
            return NULL;  /* input unrecognized as locale name */
        }

        if (*expr && charactersInExpression < MAX_LC_LEN)
            _ERRCHECK(wcsncpy_s(cachein, cacheinLen, expr, charactersInExpression + 1));
        else
            *cachein = L'\x0';

        /* end: cache atomic section */
    }

    if (cp)
        memcpy((void *)cp, (void *)pcachecp, sizeof(*pcachecp));   /* possibly return cp */

    _ERRCHECK(wcscpy_s(output, sizeInChars, cacheout));
    return cacheout; /* return fully expanded locale string or locale name */
}

/* helpers */

int __cdecl __init_dummy(pthreadlocinfo ploci)  /* default routine for locale initializer */
{
        return 0;
}

void _wcscats ( wchar_t *outstr, size_t numberOfElements, int n, ...)
{
    int i;
    va_list substr;

    va_start (substr, n);

    for (i =0; i<n; i++)
    {
        _ERRCHECK(wcscat_s(outstr, numberOfElements, va_arg(substr, wchar_t *)));
    }
    va_end(substr);
}

int __lc_wcstolc ( LC_STRINGS *names, const wchar_t *wlocale)
{
    int i;
    size_t len;
    wchar_t wch;

    memset((void *)names, '\0', sizeof(LC_STRINGS));  /* clear out result */

    if (*wlocale==L'\0')
        return 0; /* trivial case */

    /* only code page is given */
    if (wlocale[0] == L'.' && wlocale[1] != L'\0')
    {
        _ERRCHECK(wcsncpy_s(names->szCodePage, _countof(names->szCodePage), &wlocale[1], MAX_CP_LEN-1));
        /* Make sure to null terminate the string in case wlocale is > MAX_CP_LEN */
        names->szCodePage[ MAX_CP_LEN -1] = 0;
        return 0;
    }

    for (i=0; ; i++)
    {
        if (!(len=wcscspn(wlocale,L"_.,")))
            return -1;  /* syntax error */

        wch = wlocale[len];

        if ((i==0) && (len<MAX_LANG_LEN) && (wch!=L'.'))
            _ERRCHECK(wcsncpy_s(names->szLanguage, _countof(names->szLanguage), wlocale, len));

        else if ((i==1) && (len<MAX_CTRY_LEN) && (wch!=L'_'))
            _ERRCHECK(wcsncpy_s(names->szCountry, _countof(names->szCountry), wlocale, len));

        else if ((i==2) && (len<MAX_CP_LEN) && (wch==L'\0' || wch==L','))
            _ERRCHECK(wcsncpy_s(names->szCodePage, _countof(names->szCodePage), wlocale, len));

        else
            return -1;  /* error parsing wlocale string */

        if (wch==L',')
        {
            /* modifier not used in current implementation, but it
               must be parsed to for POSIX/XOpen conformance */
            /*  wcsncpy(names->szModifier, wlocale, MAX_MODIFIER_LEN-1); */
            break;
        }

        if (!wch)
            break;
        wlocale+=(len+1);
    }
    return 0;
}
// Append locale name pieces together in the form of "Language_country.CodePage"
void __lc_lctowcs ( wchar_t *locale, size_t numberOfElements, const LC_STRINGS *names)
{
    _ERRCHECK(wcscpy_s(locale, numberOfElements, names->szLanguage));
    if (*(names->szCountry))
        _wcscats(locale, numberOfElements, 2, L"_", names->szCountry);
    if (*(names->szCodePage))
        _wcscats(locale, numberOfElements, 2, L".", names->szCodePage);
}

// Create a copy of a locale name string
LPWSTR __cdecl __copy_locale_name(LPCWSTR localeName)
{
    size_t cch;
    wchar_t* localeNameCopy;

    if (!localeName) // Input cannot be NULL
        return NULL;
    
    cch = wcsnlen(localeName, LOCALE_NAME_MAX_LENGTH);
    if (cch >= LOCALE_NAME_MAX_LENGTH) // locale name length must be <= LOCALE_NAME_MAX_LENGTH
        return NULL;

    if ((localeNameCopy = (wchar_t *) _malloc_crt((cch + 1) * sizeof(wchar_t))) == NULL)
        return NULL; 

    _ERRCHECK(wcsncpy_s(localeNameCopy, cch+1, localeName, cch+1));
    return localeNameCopy;
}
