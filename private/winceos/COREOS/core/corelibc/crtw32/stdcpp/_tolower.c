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
*_tolower.c - convert character to lower case
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines _Tolower().
*
*Revision History:
*       01-xx-96  PJP   Created from tolower.c, January 1996 by P.J. Plauger
*       04-16-96  GJF   Updated for current locale locking. Also, reformatted
*                       and made several cosmetic changes.
*       09-25-96  GJF   Added locale locking to _Getctype.
*       03-17-97  RDK   Added error flag to __crtLCMapStringA.
*       05-17-99  PML   Remove all Macintosh support.
*       01-29-01  GB    Added _func function version of data variable used in
*                       msvcprt.lib to work with STATIC_CPPLIB
*       03-12-01  PML   Use supplied locale to check case VS7#190902
*       04-03-01  PML   Reverse lead/trail bytes in composed char (vs7#232853)
*       04-26-02  GB    Fixed problem with operator precedence. problem was
*                       !ploc->_Table[c]&_UPPER
*       04-29-02  GB    Added try-finally arounds lock-unlock.
*       04-01-05  MSL   Integer overflow protection
*       05-06-05  PML   Use passed in codepage, not _cpp_leadbyte (VSW#2495)
*       05-22-05  AC    Moved some function in the import lib for clr:pure
*                       VSW#417363
*       05-23-05  PML   _lock_locale has been useless for years (vsw#497047)
*
*******************************************************************************/

#include <cruntime.h>
#include <ctype.h>
#include <stddef.h>
#include <xlocinfo.h>
#include <locale.h>
#include <mtdll.h>
#include <setlocal.h>
#include <awint.h>
#include <stdlib.h>
#include <dbgint.h>
#include <yvals.h>

/* remove macro definitions of _tolower() and tolower()
 */
#undef  _tolower
#undef  tolower

/***
*int _Tolower(c) - convert character to lower case
*
*Purpose:
*       _Tolower() is a version of tolower with a locale argument.
*
*Entry:
*       c - int value of character to be converted
*       const _Ctypevec * = pointer to locale info
*
*Exit:
*       returns int value of lower case representation of c
*
*Exceptions:
*
*******************************************************************************/

_CRTIMP2_PURE int __CLRCALL_PURE_OR_CDECL _Tolower (
        int c,
        const _Ctypevec *ploc
        )
{
        int size;
        unsigned char inbuffer[3];
        unsigned char outbuffer[3];

        UINT codepage;
        const wchar_t *locale_name;

        if (ploc == 0)
        {
            locale_name = ___lc_locale_name_func()[LC_CTYPE];
            codepage = ___lc_codepage_func();
        }
        else
        {
            locale_name = ploc->_LocaleName;
            codepage = ploc->_Page;
        }

        if (locale_name == NULL)
        {
            if ( (c >= 'A') && (c <= 'Z') )
                c = c + ('a' - 'A');
            return c;
        }

        /* if checking case of c does not require API call, do it */
        if ((unsigned)c < 256)
        {
            if (ploc == 0)
            {
                if (!isupper(c))
                {
                    return c;
                }
            }
            else
            {
                if (!(ploc->_Table[c] & _UPPER))
                {
                    return c;
                }
            }
        }

        /* convert int c to multibyte string */
        if (ploc == 0 ? _cpp_isleadbyte((c >> 8) & 0xff)
                      : (ploc->_Table[(c >> 8) & 0xff] & _LEADBYTE) != 0)
        {
            inbuffer[0] = (c >> 8 & 0xff);
            inbuffer[1] = (unsigned char)c;
            inbuffer[2] = 0;
            size = 2;
        } else {
            inbuffer[0] = (unsigned char)c;
            inbuffer[1] = 0;
            size = 1;
        }

        /* convert wide char to lowercase */
        if (0 == (size = __crtLCMapStringA(NULL, locale_name, LCMAP_LOWERCASE,
            (const char *)inbuffer, size, (char *)outbuffer, 3, codepage, TRUE)))
        {
            return c;
        }

        /* construct integer return value */
        if (size == 1)
            return ((int)outbuffer[0]);
        else
            return ((int)outbuffer[1] | ((int)outbuffer[0] << 8));

}


/***
*_Ctypevec _Getctype() - get ctype info for current locale
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

_CRTIMP2_PURE _Ctypevec __CLRCALL_PURE_OR_CDECL _Getctype()
{
        /* get ctype info for current locale */
        _Ctypevec ctype;

        ctype._Page = ___lc_codepage_func();
        ctype._Table = (const short *)_calloc_crt(256, sizeof (*__pctype_func()));
        if (ctype._Table != 0)
        {
            memcpy((void *)ctype._Table, __pctype_func(), 256 * sizeof (*__pctype_func()));
            ctype._Delfl = 1;
        }
        else
        {
            ctype._Table = (const short *)__pctype_func();
            ctype._Delfl = 0;
        }
        ctype._LocaleName = ___lc_locale_name_func()[LC_COLLATE];
        if (ctype._LocaleName)
            ctype._LocaleName = _wcsdup(ctype._LocaleName);

        return (ctype);
}
