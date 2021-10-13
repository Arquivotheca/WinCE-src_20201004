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
*xmbtowc.c - Convert multibyte char to wide char.
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Convert a multibyte character into the equivalent wide character.
*
*Revision History:
*       12-XX-95  PJP   Created from mbtowc.c December 1995 by P.J. Plauger
*       04-17-96  GJF   Updated for current locale locking. Also, reformatted
*                       and made several cosmetic changes.
*       09-25-96  GJF   Made mbrlen, mbrtowc and mbsrtowcs multithread safe.
*       09-17-97  JWM   Added "return MB_CUR_MAX" to "if (*pst != 0)" branch.
*       05-17-99  PML   Remove all Macintosh support.
*       01-29-01  GB    Added _func function version of data variable used in msvcprt.lib
*                       to work with STATIC_CPPLIB
*       04-29-02  GB    Added try-finally arounds lock-unlock.
*       02-27-04  AC    Moved standard C functions to crt.
*       03-30-04  AC    Readded back the C functions so that msvcp80.dll is binary compatible.
*                       VSW#256950
*       08-08-04  AJS   Reremoved the C functions in msvcp80.dll for LKG9.
*                       VSW#339719
*       05-06-05  PML   Get locale from codepage instead of using global locale
*                       VSW#2495
*       05-23-05  PML   _lock_locale has been useless for years (vsw#497047)
*       07-23-10  PJP   reworked _Cvtvec to drop _locale_t, dropped hash table code
*
*******************************************************************************/

#include <cruntime.h>
#include <stdlib.h>
#include <mtdll.h>
#include <errno.h>
#include <dbgint.h>
#include <ctype.h>
#include <limits.h>              /* for INT_MAX */
#include <stdio.h>               /* for EOF */
#include <xlocinfo.h>            /* for _Cvtvec, _Mbrtowc */
#include <internal.h>
#include <locale.h>
#include <setlocal.h>
#include <mbctype.h>             /* for _ismbblead_l */
#include <sect_attribs.h>
#include <rterr.h>

/***
*int _Mbrtowc() - Convert multibyte char to wide character.
*
*Purpose:
*       Convert a multi-byte character into the equivalent wide character,
*       according to the specified LC_CTYPE category, or the current locale.
*       [ANSI].
*
*       NOTE:  Currently, the C libraries support the "C" locale only.
*              Non-C locale support now available under _INTL switch.
*Entry:
*       wchar_t  *pwc = pointer to destination wide character
*       const char *s = pointer to multibyte character
*       size_t      n = maximum length of multibyte character to consider
*       mbstate_t *pst      = pointer to state
*       const _Cvtvec *     = pointer to locale info
*
*Exit:
*       If s = NULL, returns 0, indicating we only use state-independent
*       character encodings.
*       If s != NULL, returns:  0 (if *s = null char)
*                               -1 (if the next n or fewer bytes not valid mbc)
*                               number of bytes comprising converted mbc
*
*Exceptions:
*
*******************************************************************************/

int _MRTIMP2 __cdecl _Mbrtowc(
        wchar_t  *pwc,
        const char *s,
        size_t n,
        mbstate_t *pst,
        const _Cvtvec *ploc
        )
{
        if ( !s || n == 0 )
            /* indicate do not have state-dependent encodings,
               handle zero length string */
            return 0;

        if ( !*s )
        {
            /* handle NULL char */
            if (pwc)
                *pwc = 0;
            return 0;
        }

        {   /* perform locale-dependent parse */
			_Cvtvec cvtvec;
            unsigned char ch;

            if (ploc == 0)
            {
                cvtvec = _Getcvt();
                ploc = &cvtvec;
            }

            if (ploc->_Isclocale)
            {
                if (pwc)
                    *pwc = (wchar_t)(unsigned char)*s;
                return sizeof(char);
            }

            _ASSERTE (ploc->_Mbcurmax == 1 ||
                      ploc->_Mbcurmax == 2);

            if (*pst != 0)
            {   /* complete two-byte multibyte character */
                ((char *)pst)[1] = *s;
                if (ploc->_Mbcurmax <= 1 ||
                    (MultiByteToWideChar(ploc->_Page,
                                         MB_PRECOMPOSED|MB_ERR_INVALID_CHARS,
                                         (char *)pst,
                                         2,
                                         pwc,
                                         (pwc) ? 1 : 0) == 0))
                {   /* translation failed */
                    *pst = 0;
                    errno = EILSEQ;
                    return -1;
                }
                *pst = 0;
                return ploc->_Mbcurmax;
            }

            ch = (unsigned char)*s;
            if ( ploc->_Isleadbyte[ch >> 3] & (1 << (ch & 7)) )
            {
                /* multi-byte char */
                if (n < (size_t)ploc->_Mbcurmax)
                {   /* save partial multibyte character */
                    ((char *)pst)[0] = *s;
                    return (-2);
                }
                else if ( ploc->_Mbcurmax <= 1 ||
                          (MultiByteToWideChar( ploc->_Page,
                                                MB_PRECOMPOSED |
                                                    MB_ERR_INVALID_CHARS,
                                                s,
                                                ploc->_Mbcurmax,
                                                pwc,
                                                (pwc) ? 1 : 0) == 0) )
                {
                    /* validate high byte of mbcs char */
                    if (!*(s+1))
                    {
                        *pst = 0;
                        errno = EILSEQ;
                        return -1;
                    }
/*                  else translation failed with no complaint? [pjp] */
                }
                return ploc->_Mbcurmax;
            }
            else {
                /* single byte char */

                if ( MultiByteToWideChar( ploc->_Page,
                                          MB_PRECOMPOSED|MB_ERR_INVALID_CHARS,
                                          s,
                                          1,
                                          pwc,
                                          (pwc) ? 1 : 0) == 0 )
                {
                    errno = EILSEQ;
                    return -1;
                }

                return sizeof(char);
            }
        }
}

#ifdef MRTDLL
int _MRTIMP2 __cdecl _Mbrtowc(
        unsigned short* pwc,
        const char *s,
        size_t n,
        mbstate_t *pst,
        const _Cvtvec *ploc
        )
    {
    return _Mbrtowc((wchar_t *)pwc, s, n, pst, ploc);
    }
#endif
