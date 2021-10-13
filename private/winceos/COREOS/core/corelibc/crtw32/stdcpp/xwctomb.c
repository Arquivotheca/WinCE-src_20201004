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
*xwctomb.c - Convert wide character to multibyte character, with locale.
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Convert a wide character into the equivalent multibyte character.
*
*Revision History:
*       12-XX-95  PJP   Created from wctomb.c December 1995 by P.J. Plauger
*       04-18-96  GJF   Updated for current locale locking. Also, reformatted
*                       and made several cosmetic changes.
*       09-26-96  GJF   Made _Getcvt() and wcsrtombs() multithread safe.
*       04-28-99  GJF   Changed dwFlags arg value to 0 in WideCharToMultiByte
*                       calls to avoid problems with codepage 1258 on NT 5.0.
*       05-17-99  PML   Remove all Macintosh support.
*       01-29-01  GB    Added _func function version of data variable used in msvcprt.lib
*                       to work with STATIC_CPPLIB
*       04-29-02  GB    Added try-finally arounds lock-unlock.
*       02-27-04  AC    Moved standard C functions to crt.
*       03-30-04  AC    Readded back the C functions so that msvcp80.dll is binary compatible.
*                       VSW#256950
*       08-08-04  AJS   Reremoved the C functions in msvcp80.dll for LKG9.
*                       VSW#339719
*       05-06-05  PML   Use passed in codepage, not __mb_cur_max_func (VSW#2495)
*       05-23-05  PML   _lock_locale has been useless for years (vsw#497047)
*       07-23-10  PJP   reworked _Cvtvec to drop _locale_t
*
*******************************************************************************/

#include <cruntime.h>
#include <stdlib.h>
#include <mtdll.h>
#include <errno.h>
#include <limits.h>             /* for MB_LEN_MAX */
#include <string.h>             /* for memcpy */
#include <stdio.h>              /* for EOF */
#include <xlocinfo.h>           /* for _Cvtvec, _Wcrtomb */
#include <locale.h>
#include <setlocal.h>
#include <internal.h>
#include <mbctype.h>

/***
*int _Wcrtomb() - Convert wide character to multibyte character.
*
*Purpose:
*       Convert a wide character into the equivalent multi-byte character,
*       according to the specified LC_CTYPE category, or the current locale.
*       [ANSI].
*
*       NOTE:  Currently, the C libraries support the "C" locale only.
*              Non-C locale support now available under _INTL switch.
*Entry:
*       char *s             = pointer to multibyte character
*       wchar_t wchar       = source wide character
*       mbstate_t *pst      = pointer to state (not used)
*       const _Cvtvec *ploc = pointer to locale info
*
*Exit:
*       Returns:
*      -1 (if error) or number of bytes comprising converted mbc
*
*Exceptions:
*
*******************************************************************************/

/* Retained for backward compatibility of DLL exports only */
_CRTIMP2_PURE int __CLRCALL_PURE_OR_CDECL __Wcrtomb_lk
        (
        char *s,
        wchar_t wchar,
        mbstate_t *pst,
        const _Cvtvec *ploc
        )
{
        return _Wcrtomb(s, wchar, pst, ploc);
}

_CRTIMP2_PURE int __CLRCALL_PURE_OR_CDECL _Wcrtomb
        (
        char *s,
        wchar_t wchar,
        mbstate_t *pst,
        const _Cvtvec *ploc
        )
{
        if (ploc->_Isclocale)
        {
            if ( wchar > 255 )  /* validate high byte */
            {
                errno = EILSEQ;
                return -1;
            }

            *s = (char) wchar;
            return sizeof(char);
        } else {
            int size;
            BOOL defused = 0;
			_Cvtvec cvtvec;

            if (ploc == 0)
            {
                cvtvec = _Getcvt();
                ploc = &cvtvec;
            }

            if ( ((size = WideCharToMultiByte(ploc->_Page,
                                              0,
                                              &wchar,
                                              1,
                                              s,
                                              ploc->_Mbcurmax,
                                              NULL,
                                              &defused)) == 0) ||
                 (defused) )
            {
                errno = EILSEQ;
                return -1;
            }

            return size;
        }
}

#ifdef MRTDLL
_CRTIMP2_PURE int __CLRCALL_PURE_OR_CDECL _Wcrtomb
        (
        char *s,
        unsigned short wchar,
        mbstate_t *pst,
        const _Cvtvec *ploc
        )
    {
    return _Wcrtomb(s,(wchar_t) wchar, pst, ploc);
    }
#endif /* MRTDLL */

/***
*_Cvtvec _Getcvt() - get conversion info for current locale
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

_CRTIMP2_PURE _Cvtvec __CLRCALL_PURE_OR_CDECL _Getcvt()
{
        _Cvtvec cvt = {0};
        int idx;

        cvt._Page = ___lc_codepage_func();
        cvt._Mbcurmax = ___mb_cur_max_func();
        cvt._Isclocale = ___lc_locale_name_func()[LC_CTYPE] == NULL;

        if (!cvt._Isclocale)
            for (idx = 0; idx < 256; ++idx)
                if (_ismbblead(idx))
                    cvt._Isleadbyte[idx >> 3] |= 1 << (idx & 7);
        return (cvt);
}
