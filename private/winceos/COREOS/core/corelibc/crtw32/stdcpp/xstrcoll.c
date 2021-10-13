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
*xstrcoll.c - Collate locale strings
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Compare two strings using the locale LC_COLLATE information.
*
*Revision History:
*       01-XX-96  PJP   Created from strcoll.c January 1996 by P.J. Plauger
*       04-17-96  GJF   Updated for current locale locking. Also, reformatted
*                       and made several cosmetic changes.
*       05-14-96  JWM   Update to _Strcoll(): error path failed to unlock.
*       09-26-96  GJF   Made _GetColl() multithread safe.
*       12-02-97  GJF   Removed bogus codepage determination.
*       01-12-98  GJF   Use _lc_collate_cp codepage.
*       01-05-99  GJF   Changes for 64-bit size_t.
*       05-11-99  PML   Win64 fix: cast ptr diff to int
*       05-17-99  PML   Remove all Macintosh support.
*       01-29-01  GB    Added _func function version of data variable used in msvcprt.lib
*                       to work with STATIC_CPPLIB
*       04-29-02  GB    Added try-finally arounds lock-unlock.
*       03-13-04  MSL   Avoid returing from __try for prefast
*       05-06-05  PML   Use passed in codepage, not __lc_collate_cp_func (VSW#2495)
*       05-22-05  AC    Moved some function in the import lib for clr:pure
*                       VSW#417363
*       05-23-05  PML   _lock_locale has been useless for years (vsw#497047)
*
*******************************************************************************/

#include <cruntime.h>
#include <string.h>
#include <xlocinfo.h>   /* for _Collvec, _Strcoll */
#include <windows.h>
#include <stdlib.h>
#include <malloc.h>
#include <locale.h>
#include <mtdll.h>
#include <setlocal.h>
#include <errno.h>
#include <awint.h>

/***
*int _Strcoll() - Collate locale strings
*
*Purpose:
*       Compare two strings using the locale LC_COLLATE information.
*       [ANSI].
*
*       Non-C locale support available under _INTL switch.
*       In the C locale, strcoll() simply resolves to strcmp().
*Entry:
*       const char *s1b = pointer to beginning of the first string
*       const char *s1e = pointer past end of the first string
*       const char *s2b = pointer to beginning of the second string
*       const char *s1e = pointer past end of the second string
*       const _Collvec *ploc = pointer to locale info
*
*Exit:
*       Less than 0    = first string less than second string
*       0              = strings are equal
*       Greater than 0 = first string greater than second string
*
*Exceptions:
*       _NLSCMPERROR    = error
*       errno = EINVAL
*
*******************************************************************************/

_CRTIMP2_PURE int __CLRCALL_PURE_OR_CDECL _Strcoll (
        const char *_string1,
        const char *_end1,
        const char *_string2,
        const char *_end2,
        const _Collvec *ploc
        )
{
        int ret=0;
        UINT codepage;
        int n1 = (int)(_end1 - _string1);
        int n2 = (int)(_end2 - _string2);
        const wchar_t *locale_name;

        if (ploc == 0)
        {
            locale_name = ___lc_locale_name_func()[LC_COLLATE];
            codepage = ___lc_collate_cp_func();
        }
        else
        {
            locale_name = ploc->_LocaleName;
            codepage = ploc->_Page;
        }

        if (locale_name == NULL)
        {
            int ans;
            ans = memcmp(_string1, _string2, n1 < n2 ? n1 : n2);
            ret=(ans != 0 || n1 == n2 ? ans : n1 < n2 ? -1 : +1);
        }
        else
        {
            if ( 0 == (ret = __crtCompareStringA( NULL,
                                                  locale_name,
                                                  SORT_STRINGSORT,
                                                  _string1,
                                                  n1,
                                                  _string2,
                                                  n2,
                                                  codepage )) )
            {
                errno=EINVAL;
                ret=_NLSCMPERROR;
            }
            else
            {
                ret-=2;
            }
        }

        return ret;
}


/***
*_Collvec _Getcoll() - get collation info for current locale
*
*Purpose:
*
*Entry:
*
*Exit:
*
*Exceptions:
*
*******************************************************************************/

_CRTIMP2_PURE _Collvec __CLRCALL_PURE_OR_CDECL _Getcoll()
{
        _Collvec coll;

        coll._Page = ___lc_collate_cp_func();
        coll._LocaleName = ___lc_locale_name_func()[LC_COLLATE];
        if (coll._LocaleName)
            coll._LocaleName = _wcsdup(coll._LocaleName);

        return (coll);
}
