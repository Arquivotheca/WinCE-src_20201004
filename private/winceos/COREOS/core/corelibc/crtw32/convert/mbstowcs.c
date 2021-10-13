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
*mbstowcs.c - Convert multibyte char string to wide char string.
*
*Purpose:
*       Convert a multibyte char string into the equivalent wide char string.
*
*******************************************************************************/

#include <internal.h>
#include <internal_securecrt.h>
#include <locale.h>
#include <errno.h>
#include <cruntime.h>
#include <stdlib.h>
#include <string.h>
#include <dbgint.h>
#include <stdio.h>
#include <mtdll.h>
#include <setlocal.h>

/***
*size_t mbstowcs() - Convert multibyte char string to wide char string.
*
*Purpose:
*       Convert a multi-byte char string into the equivalent wide char string,
*       according to the LC_CTYPE category of the current locale.
*       [ANSI].
*
*Entry:
*       wchar_t *pwcs = pointer to destination wide character string buffer
*       const char *s = pointer to source multibyte character string
*       size_t      n = maximum number of wide characters to store
*
*Exit:
*       If pwcs != NULL returns the number of words modified (<=n): note that
*       if the return value == n, then no destination string is not 0 terminated.
*       If pwcs == NULL returns the length (not size) needed for the destination buffer.
*
*Exceptions:
*       Returns (size_t)-1 if s is NULL or invalid mbcs character encountered
*       and errno is set to EILSEQ.
*
*******************************************************************************/

static errno_t __cdecl _mbstowcs_helper (
        size_t *pConverted,
        wchar_t  *pwcs,
        const char *s,
        size_t n
        )
{
    size_t count = 0;
    DWORD dwErrInvalidCharsFlag = g_fCrtLegacyInputValidation ? 0 : MB_ERR_INVALID_CHARS;

    _ASSERTE(pConverted != NULL);
    *pConverted = 0;

    if (pwcs && n == 0)
        /* dest string exists, but 0 bytes converted */
        return 0;

    if (pwcs && n > 0)
    {
        *pwcs = '\0';
    }

    /* validation section */
    _VALIDATE_RETURN_ERRCODE((s != NULL), EINVAL);

    /* if destination string exists, fill it in */
    if (pwcs)
    {
        int bytecnt, charcnt;
        unsigned char *p;

        /* Assume that the buffer is large enough */
        if ((count = MultiByteToWideChar(CP_ACP,
                                         MB_PRECOMPOSED | dwErrInvalidCharsFlag,
                                         s,
                                         -1,
                                         pwcs,
                                         (int)n)) != 0 ) {
            *pConverted = count - 1; /* don't count NUL */
            return 0;
        }

        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        {
            _SET_ERRNO(EILSEQ);
            *pwcs = '\0';
            return EILSEQ;
        }

        /* User-supplied buffer not large enough. */

        /* How many bytes are in n characters of the string? */
        charcnt = (int)n;
        for (p = (unsigned char *)s; (charcnt-- && *p); p++)
        {
            if (isleadbyte(*p))
            {
                if(p[1]=='\0')
                {
                    /*  this is a leadbyte followed by EOS -- a dud MBCS string
                        We choose not to assert here because this
                        function is defined to deal with dud strings on
                        input and return a known value
                    */
                    _SET_ERRNO(EILSEQ);
                    *pwcs = '\0';
                    return EILSEQ;
                }
                else
                {
                    p++;
                }
            }
        }
        bytecnt = ((int) ((char *)p - (char *)s));

        if ((count = MultiByteToWideChar(CP_ACP,
                                         MB_PRECOMPOSED,
                                         s,
                                         bytecnt,
                                         pwcs,
                                         (int)n)) == 0)
        {
            _SET_ERRNO(EILSEQ);
            *pwcs = '\0';
            return EILSEQ;
        }

        *pConverted = count; /* no NUL in string */
        return 0;
    }
    else
    {
        /* pwcs == NULL, get size only, s must be NUL-terminated */
        if ((count = MultiByteToWideChar(CP_ACP,
                                         MB_PRECOMPOSED | dwErrInvalidCharsFlag,
                                         s,
                                         -1,
                                         NULL,
                                         0)) == 0) {
            _SET_ERRNO(EILSEQ);
            return EILSEQ;
        } else {
            *pConverted = count - 1;
            return 0;
        }
    }
}

size_t __cdecl mbstowcs (
        wchar_t  *pwcs,
        const char *s,
        size_t n
        )
{
    size_t converted;
    if (_mbstowcs_helper(&converted, pwcs, s, n) != 0) {
        return (size_t)-1;
    }
    return converted;
}

/***
*errno_t mbstowcs_s() - Convert multibyte char string to wide char string.
*
*Purpose:
*       Convert a multi-byte char string into the equivalent wide char string,
*       according to the LC_CTYPE category of the current locale.
*       Same as mbstowcs(), but the destination is ensured to be null terminated.
*       If there's not enough space, we return EINVAL.
*
*Entry:
*       size_t *pConvertedChars = Number of bytes modified including the terminating NULL
*                                 This pointer can be NULL.
*       wchar_t *pwcs = pointer to destination wide character string buffer
*       size_t sizeInWords = size of the destination buffer
*       const char *s = pointer to source multibyte character string
*       size_t n = maximum number of wide characters to store (not including the terminating NULL)
*
*Exit:
*       The error code.
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

errno_t __cdecl mbstowcs_s (
        size_t *pConvertedChars,
        wchar_t  *pwcs,
        size_t sizeInWords,
        const char *s,
        size_t n
        )
{
    size_t retsize;
    size_t bufferSize;
    errno_t retvalue = 0;

    /* validation section */
    _VALIDATE_RETURN_ERRCODE((pwcs == NULL && sizeInWords == 0) || (pwcs != NULL && sizeInWords > 0), EINVAL);

    if (pwcs != NULL)
    {
        _RESET_STRING(pwcs, sizeInWords);
    }

    if (pConvertedChars != NULL)
    {
        *pConvertedChars = 0;
    }

    bufferSize = (n > sizeInWords) ? sizeInWords : n;
    /* n must fit into an int for MultiByteToWideChar */
    _VALIDATE_RETURN_ERRCODE((bufferSize <= INT_MAX), EINVAL);

    /* Call a non-deprecated helper to do the work. */

    retvalue = _mbstowcs_helper(&retsize, pwcs, s, bufferSize);

    if (retvalue != 0)
    {
        if (pwcs != NULL)
        {
            _RESET_STRING(pwcs, sizeInWords);
        }
        return retvalue;
    }

    /* count the null terminator */
    retsize++;

    if (pwcs != NULL)
    {
        /* return error if the string does not fit, unless n == _TRUNCATE */
        if (retsize > sizeInWords)
        {
            if (n != _TRUNCATE)
            {
                _RESET_STRING(pwcs, sizeInWords);
                _VALIDATE_RETURN_ERRCODE(retsize <= sizeInWords, ERANGE);
            }
            retsize = sizeInWords;
            retvalue = STRUNCATE;
        }

        /* ensure the string is null terminated */
        pwcs[retsize - 1] = '\0';
    }

    if (pConvertedChars != NULL)
    {
        *pConvertedChars = retsize;
    }

    return retvalue;
}

