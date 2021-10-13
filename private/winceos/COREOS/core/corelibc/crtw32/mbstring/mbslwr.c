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
*mbslwr.c - Convert string lower case (MBCS)
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Convert string lower case (MBCS)
*
*Revision History:
*       11-19-92  KRS   Ported from 16-bit sources.
*       09-29-93  CFW   Merge _KANJI and _MBCS_OS
*       10-05-93  GJF   Replaced _CRTAPI1 with __cdecl.
*       04-12-94  CFW   Make function generic.
*       04-15-93  CFW   Add _MB_CP_LOCK.
*       05-16-94  CFW   Use _mbbtolower/upper.
*       05-17-94  CFW   Enable non-Win32.
*       03-13-95  JCF   Add (unsigned char) in _MB* compare with *(cp+1).
*       05-31-95  CFW   Fix horrible Mac bug.
*       03-17-97  RDK   Added error flag to __crtLCMapStringA.
*       09-26-97  BWT   Fix POSIX
*       04-07-98  GJF   Revised multithread support based on threadmbcinfo
*                       structs
*       05-17-99  PML   Remove all Macintosh support.
*       10-08-03  AC    Added validation.
*       03-10-04  AC    Added secure version.
*       03-23-04  AC    Fixed problem in the non _s version.
*       04-25-05  AC    Added debug filling
*                       VSW#2459
*
*******************************************************************************/

#ifdef  _MBCS

#include <awint.h>
#include <mtdll.h>
#include <cruntime.h>
#include <ctype.h>
#include <mbdata.h>
#include <mbstring.h>
#include <mbctype.h>
#include <internal.h>
#include <internal_securecrt.h>
#include <locale.h>
#include <setlocal.h>
#include <string.h>


/***
* _mbslwr - Convert string lower case (MBCS)
*
*Purpose:
*       Convrts all the upper case characters in a string
*       to lower case in place.  MBCS chars are handled
*       correctly.
*
*Entry:
*       unsigned char *string = pointer to string
*
*Exit:
*       Returns a pointer to the input string.
*       Returns NULL on error.
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

errno_t __cdecl _mbslwr_s_l(
        unsigned char *string,
        size_t sizeInBytes,
        _locale_t plocinfo
        )
{
        size_t stringlen;

        /* validation section */
        _VALIDATE_RETURN_ERRCODE((string != NULL && sizeInBytes > 0) || (string == NULL && sizeInBytes == 0), EINVAL);

        if (string == NULL)
        {
            /* nothing to do */
            return 0;
        }

        stringlen = strnlen((char *)string, sizeInBytes);
        if (stringlen >= sizeInBytes)
        {
            _RESET_STRING(string, sizeInBytes);
            _RETURN_DEST_NOT_NULL_TERMINATED(string, sizeInBytes);
        }
        _FILL_STRING(string, sizeInBytes, stringlen + 1);

        unsigned char *cp, *dst;
        _LocaleUpdate _loc_update(plocinfo);

        for (cp = string, dst = string; *cp != '\0'; ++cp)
        {
            if (_ismbblead_l(*cp, _loc_update.GetLocaleT()))
            {

#if     !defined(_POSIX_)

                int retval;
                unsigned char ret[4];
                if ((retval = __crtLCMapStringA(
                                _loc_update.GetLocaleT(),
                                _loc_update.GetLocaleT()->mbcinfo->mblocalename,
                                LCMAP_LOWERCASE,
                                (const char *)cp,
                                2,
                                (char *)ret,
                                2,
                                _loc_update.GetLocaleT()->mbcinfo->mbcodepage,
                                TRUE )) == 0 )
                {
                    errno = EILSEQ;
                    _RESET_STRING(string, sizeInBytes);
                    return errno;
                }

                *(dst++) = ret[0];
                ++cp;
                if (retval > 1)
                {
                    *(dst++) = ret[1];
                }

#else   /* !_POSIX_ */

                int mbval = ((*cp) << 8) + *(cp+1);

                cp++;
                if (     mbval >= _MBUPPERLOW1
                    &&   mbval <= _MBUPPERHIGH1 )
                    *cp += _MBCASEDIFF1;

                else if (mbval >= _MBUPPERLOW2
                    &&   mbval <= _MBUPPERHIGH2 )
                    *cp += _MBCASEDIFF2;
#endif  /* !_POSIX_ */

            }
            else
            {
                /* single byte, macro version */
                *(dst++) = (unsigned char) _mbbtolower_l(*cp, _loc_update.GetLocaleT());
            }
        }
        /* null terminate the string */
        *dst = '\0';

        return 0;
}

errno_t (__cdecl _mbslwr_s)(
        unsigned char *string,
        size_t sizeInBytes
        )
{
    return _mbslwr_s_l(string, sizeInBytes, NULL);
}

unsigned char * (__cdecl _mbslwr_l)(
        unsigned char *string,
        _locale_t plocinfo
        )
{
    return (_mbslwr_s_l(string, (string == NULL ? 0 : (size_t)-1), plocinfo) == 0 ? string : NULL);
}

unsigned char * (__cdecl _mbslwr)(
        unsigned char *string
        )
{
    return (_mbslwr_s_l(string, (string == NULL ? 0 : (size_t)-1), NULL) == 0 ? string : NULL);
}

#endif  /* _MBCS */
