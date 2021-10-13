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
*xstrxfrm.c - Transform a string using locale information
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Transform a string using the locale information as set by
*       LC_COLLATE.
*
*Revision History:
*       01-XX-96  PJP   Created from strxfrm.c January 1996 by P.J. Plauger
*       04-18-96  GJF   Updated for current locale locking. Also, reformatted
*                       and made several cosmetic changes.
*       03-17-97  RDK   Added error flag to __crtLCMapStringA.
*       12-02-97  GJF   Removed bogus codepage determination.
*       01-12-98  GJF   Use _lc_collate_cp codepage.
*       01-05-99  GJF   Changes for 64-bit size_t.
*       05-17-99  PML   Remove all Macintosh support.
*       01-29-01  GB    Added _func function version of data variable used in msvcprt.lib
*                       to work with STATIC_CPPLIB
*       04-29-02  GB    Added try-finally arounds lock-unlock.
*       03-13-04  MSL   Avoid returing from __try for prefast
*       05-22-05  AC    Moved some function in the import lib for clr:pure
*                       VSW#417363
*       05-23-05  PML   _lock_locale has been useless for years (vsw#497047)
*
*******************************************************************************/

#include <cruntime.h>
#include <string.h>
#include <xlocinfo.h>   /* for _Collvec */
#include <windows.h>
#include <stdlib.h>
#include <limits.h>
#include <malloc.h>
#include <locale.h>
#include <mtdll.h>
#include <setlocal.h>
#include <awint.h>

/***
*size_t _Strxfrm() - Transform a string using locale information
*
*Purpose:
*       Transform the string pointer to by _string2 and place the
*       resulting string into the array pointer to by _string1.
*       No more than _end1 - _string1 characters are place into the
*       resulting string (including the null).
*
*       The transformation is such that if strcmp() is applied to
*       the two transformed strings, the return value is equal to
*       the result of strcoll() applied to the two original strings.
*       Thus, the conversion must take the locale LC_COLLATE info
*       into account.
*       [ANSI]
*
*       The value of the following expression is the size of the array
*       needed to hold the transformation of the source string:
*
*               1 + strxfrm(NULL,string,0)
*
*       NOTE:  Currently, the C libraries support the "C" locale only.
*       Thus, _Strxfrm() simply resolves to strncpy()/strlen().
*
*Entry:
*       char *_string1       = pointer to beginning of result string
*       char *_end1          = pointer past end of result string
*       const char *_string2 = pointer to beginning of source string
*       const char *_end2    = pointer past end of source string
*       const _Collvec *ploc = pointer to locale info
*
*Exit:
*       Length of the transformed string.
*       If the value returned is too big, the contents of the
*       _string1 array are indeterminate.
*
*Exceptions:
*       Non-standard: if OM/API error, return INT_MAX.
*
*******************************************************************************/

_CRTIMP2_PURE size_t __CLRCALL_PURE_OR_CDECL _Strxfrm (
        char *_string1,
        char *_end1,
        const char *_string2,
        const char *_end2,
        const _Collvec *ploc
        )
{
        size_t _n1 = _end1 - _string1;
        size_t _n2 = _end2 - _string2;
        int dstlen;
        size_t retval = (size_t)-1;   /* NON-ANSI: default if OM or API error */
        UINT codepage;
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

        if ((locale_name == NULL) &&
            (codepage == _CLOCALECP))
        {
            if (_n2 <= _n1)
            {
                memcpy(_string1, _string2, _n2);
            }
            retval=_n2;
        }
        else
        {
            /* Inquire size of dst string in BYTES */
            if (0 != (dstlen = __crtLCMapStringA(NULL,
                                                 locale_name,
                                                 LCMAP_SORTKEY,
                                                 _string2,
                                                 (int)_n2,
                                                 NULL,
                                                 0,
                                                 codepage,
                                                 TRUE)))
            {
                retval = dstlen;

                /* if not enough room, return amount needed */
                if (dstlen <= (int)(_n1))
                {
                    /* Map src string to dst string */
                    __crtLCMapStringA(NULL,
                                      locale_name,
                                      LCMAP_SORTKEY,
                                      _string2,
                                      (int)_n2,
                                      _string1,
                                      (int)_n1,
                                      codepage,
                                      TRUE);
                }
            }
        }

        return (size_t)retval;
}
