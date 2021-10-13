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
*setlocal.c - Contains the setlocale function
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Contains the _wsetlocale() function.
*
*Revision History:
*       10-29-93  CFW   Module created.
*       01-03-94  CFW   Fix for NULL locale string.
*       02-07-94  CFW   POSIXify.
*       04-15-94  GJF   Made definition off outwlocale conditional on
*                       DLL_FOR_WIN32S.
*       07-26-94  CFW   Fix for bug #14663.
*       01-10-95  CFW   Debug CRT allocs.
*       01-06-99  GJF   Changes for 64-bit size_t.
*       05-13-99  PML   Remove Win32s
*       07-31-01  PML   Make thread-safer, not totally thread-safe (vs7#283330)
*       05-08-02  GB    Fix for VS7#514442, Fixed possiblity of BO.
*       03-25-05  PAL   Do not allow current thread locale to be updated after
*                       getting a string from setlocale(). Fixes read from freed
*                       memory: VSW 73865.
*       04-03-05  PAL   Remove VSW reference from comment in the last change.
*       04-01-05  MSL   Integer overflow protection
*       05-22-05  PML   Lock against race condition in _wsetlocale (vsw#73865)
*
*******************************************************************************/

#include <wchar.h>
#include <stdlib.h>
#include <setlocal.h>
#include <locale.h>
#include <dbgint.h>
#include <mtdll.h>
#include <internal.h>
#include <malloc.h>

#if !_PTD_LOCALE_SUPPORT
pthreadmbcinfo __cdecl __get_mbcinfo_addref(void);
void __cdecl __mbcinfo_release(pthreadmbcinfo* pptmbci);
pthreadlocinfo __cdecl __get_locinfo_addref(void);
void __cdecl __locinfo_release(pthreadlocinfo* pptloci);
#endif  /* _PTD_LOCALE_SUPPORT */

char * __cdecl setlocale (
        int _category,
        const char *_locale
        )
{
        size_t size = 0;
        wchar_t *inwlocale = NULL;
        wchar_t *outwlocale;
        pthreadlocinfo ptloci;
        int *refcount = NULL;
        char *outlocale = NULL;
        _locale_tstruct locale;
#if _PTD_LOCALE_SUPPORT
        _ptiddata ptd;
#endif  /* _PTD_LOCALE_SUPPORT */

        /* convert ASCII string into WCS string */
        if (_locale)
        {
            _ERRCHECK_EINVAL_ERANGE(mbstowcs_s(&size, NULL, 0, _locale, INT_MAX));
            if ((inwlocale = (wchar_t *)_calloc_crt(size, sizeof(wchar_t))) == NULL)
                return NULL;

            if (_ERRCHECK_EINVAL_ERANGE(mbstowcs_s(NULL, inwlocale, size, _locale, _TRUNCATE)) != 0)
            {
                _free_crt (inwlocale);
                return NULL;
            }
        }

        /* set the locale and get WCS return string */

        outwlocale = _wsetlocale(_category, inwlocale);
        _free_crt (inwlocale);

        if (NULL == outwlocale)
            return NULL;

        // We now have a locale string, but the global locale can be changed by
        // another thread. If we allow this thread's locale to be updated before we're done
        // with this string, it might be freed from under us.
        // Call versions of the wide-to-MB-char conversions that do not update the current thread's
        // locale.

#if _PTD_LOCALE_SUPPORT
        ptd = _getptd();
        locale.locinfo = ptd->ptlocinfo;
        locale.mbcinfo = ptd->ptmbcinfo;
#else
        locale.locinfo = __get_locinfo_addref();
        locale.mbcinfo = __get_mbcinfo_addref();
        __try
        {
#endif  /* _PTD_LOCALE_SUPPORT */

        /* get space for WCS return value, first call only */

        size = 0;
        if (_ERRCHECK_EINVAL_ERANGE(_wcstombs_s_l(&size, NULL, 0, outwlocale, 0, &locale)) != 0)
            return NULL;

        refcount = (int *)_malloc_crt(size + sizeof(int));
        if (!refcount)
            return NULL;

        outlocale = (char*)&refcount[1];

        /* convert return value to ASCII */
        if ( _ERRCHECK_EINVAL_ERANGE(_wcstombs_s_l(NULL, outlocale, size, outwlocale, _TRUNCATE, &locale)) != 0)
        {
            _free_crt(refcount);
            return NULL;
        }

        ptloci = locale.locinfo;
        _mlock(_SETLOCALE_LOCK);
        __try {
            _ASSERTE(((ptloci->lc_category[_category].locale != NULL) && (ptloci->lc_category[_category].refcount != NULL)) ||
                     ((ptloci->lc_category[_category].locale == NULL) && (ptloci->lc_category[_category].refcount == NULL)));
            if (ptloci->lc_category[_category].refcount != NULL &&
                InterlockedDecrement(ptloci->lc_category[_category].refcount) == 0) {
                _free_crt(ptloci->lc_category[_category].refcount);
            }

#if _PTD_LOCALE_SUPPORT
            if (!(ptd->_ownlocale & _PER_THREAD_LOCALE_BIT) &&
                        !(__globallocalestatus & _GLOBAL_LOCALE_BIT)) {
                        !(__globallocalestatus & _GLOBAL_LOCALE_BIT))
#else
            if (ptloci == __ptlocinfo)
#endif  /* _PTD_LOCALE_SUPPORT */
            {
                if (ptloci->lc_category[_category].refcount != NULL &&
                    InterlockedDecrement(ptloci->lc_category[_category].refcount) == 0) {
                    _free_crt(ptloci->lc_category[_category].refcount);
                }
            }

            /*
             * Note that we are using a risky trick here.  We are adding this
             * locale to an existing threadlocinfo struct, and thus starting
             * the locale's refcount with the same value as the whole struct.
             * That means all code which modifies both threadlocinfo::refcount
             * and threadlocinfo::lc_category[]::refcount in structs that are
             * potentially shared across threads must make those modifications
             * under _SETLOCALE_LOCK.  Otherwise, there's a race condition
             * for some other thread modifying threadlocinfo::refcount after
             * we load it but before we store it to refcount.
             */
#if 0
            /*
             * NOTES STRIPPED FROM SHIPPED SOURCE:
             * The alternative would be to always set the locale/refcount
             * fields whenever we also set the locale/refcount fields per
             * lc_category.  Or, instead of modifying the existing struct,
             * we could copy it into a new struct and modify the new struct.
             * But that would probably lead to poor cross-thread sharing, and
             * is a riskier fix to take late in Whidbey.
             */
#endif
            *refcount = ptloci->refcount;
            ptloci->lc_category[_category].refcount = refcount;
            ptloci->lc_category[_category].locale = outlocale;
        }
        __finally {
            _munlock(_SETLOCALE_LOCK);
        }
#if !_PTD_LOCALE_SUPPORT
        }
        __finally {
            __mbcinfo_release(&locale.mbcinfo);
            __locinfo_release(&locale.locinfo);
        }
#endif  /* _PTD_LOCALE_SUPPORT */

        return outlocale;
}
