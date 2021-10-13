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
*wcstombs.c - Convert wide char string to multibyte char string.
*
*Purpose:
*       Convert a wide char string into the equivalent multibyte char string.
*
*******************************************************************************/

#include <cruntime.h>
#include <stdlib.h>
#include <limits.h>
#include <internal.h>
#include <internal_securecrt.h>
#include <dbgint.h>
#include <errno.h>
#include <locale.h>
#include <mtdll.h>
#include <setlocal.h>

/***
*size_t wcstombs() - Convert wide char string to multibyte char string.
*
*Purpose:
*       Convert a wide char string into the equivalent multibyte char string,
*       according to the LC_CTYPE category of the current locale.
*       [ANSI].
*
*       The destination string is null terminated only if the null terminator
*       is copied from the source string.
*
*Entry:
*       char *s            = pointer to destination multibyte char string
*       const wchar_t *pwc = pointer to source wide character string
*       size_t           n = maximum number of bytes to store in s
*
*Exit:
*       If s != NULL, returns    (size_t)-1 (if a wchar cannot be converted)
*       Otherwise:       Number of bytes modified (<=n), not including
*                    the terminating NUL, if any.
*
*Exceptions:
*       Returns (size_t)-1 if an error is encountered.
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

static errno_t __cdecl _wcstombs_helper (
    size_t* pConverted,
    char * s,
    const wchar_t * pwcs,
    size_t n
    )
{
    size_t count = 0;
    int i, retval;
    char buffer[MB_LEN_MAX];
    BOOL defused = FALSE;
    BOOL* pDefused = g_fCrtLegacyInputValidation ? NULL : &defused;

    _ASSERTE(pConverted != NULL);
    *pConverted = 0;

    if (s && n == 0)
        /* dest string exists, but 0 bytes converted */
        return 0;

    /* validation section */
    _VALIDATE_RETURN_ERRCODE((pwcs != NULL), EINVAL);

    /* if destination string exists, fill it in */
    if (s)
    {
        /* Assume that usually the buffer is large enough */
        if (((count = WideCharToMultiByte(CP_ACP,
                                          0,
                                          pwcs,
                                          -1,
                                          s,
                                          (int)n,
                                          NULL,
                                          pDefused )) != 0) &&
            (!defused))
        {
            *pConverted = count - 1; /* don't count NUL */
            return 0;
        }

        if (defused || GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        {
            _SET_ERRNO(EILSEQ);
            return EILSEQ;
        }

        /* buffer not large enough, must do char by char */
        while (count < n)
        {
            if (((retval = WideCharToMultiByte(CP_ACP,
                                               0,
                                               pwcs,
                                               1,
                                               buffer,
                                               MB_LEN_MAX,
                                               NULL,
                                               pDefused)) == 0)
                 || defused)
            {
                _SET_ERRNO(EILSEQ);
                return EILSEQ;
            }

            /* enforce this for prefast */
            if (retval < 0 ||
                retval > _countof(buffer))
            {
                _SET_ERRNO(EILSEQ);
                return EILSEQ;
            }

            if (count + retval > n) {
                *pConverted = count;
                return 0;
            }

            for (i = 0; i < retval; i++, count++) /* store character */
                if((s[count] = buffer[i])=='\0') {
                    *pConverted = count;
                    return 0;
                }

            pwcs++;
        }

        *pConverted = count;
        return 0;
    }
    else
    {
        /* s == NULL, get size only, pwcs must be NUL-terminated */
        if (((count = WideCharToMultiByte(CP_ACP,
                                          0,
                                          pwcs,
                                          -1,
                                          NULL,
                                          0,
                                          NULL,
                                          pDefused )) == 0) ||
             (defused) )
        {
            _SET_ERRNO(EILSEQ);
            return EILSEQ;
        }

        *pConverted = count - 1;
        return 0;
    }
}

size_t __cdecl wcstombs (
    char * s,
    const wchar_t * pwcs,
    size_t n
    )
{
    size_t converted;
    if (_wcstombs_helper(&converted, s, pwcs, n) != 0) {
        return (size_t)-1;
    }
    return converted;
}

/***
*errno_t wcstombs_s() - Convert wide char string to multibyte char string.
*
*Purpose:
*       Convert a wide char string into the equivalent multibyte char string,
*       according to the LC_CTYPE category of the current locale.
*
*       The destination string is always null terminated.
*
*Entry:
*       size_t *pConvertedChars = Number of bytes modified including the terminating NULL
*                                 This pointer can be NULL.
*       char *dst = pointer to destination multibyte char string
*       size_t sizeInBytes = size of the destination buffer
*       const wchar_t *src = pointer to source wide character string
*       size_t n = maximum number of bytes to store in s (not including the terminating NULL)
*
*Exit:
*       The error code.
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

errno_t __cdecl wcstombs_s (
    size_t *pConvertedChars,
    char * dst,
    size_t sizeInBytes,
    const wchar_t * src,
    size_t n
    )
{
    size_t retsize;
    size_t bufferSize;
    errno_t retvalue = 0;

    /* validation section */
    _VALIDATE_RETURN_ERRCODE((dst != NULL && sizeInBytes > 0) || (dst == NULL && sizeInBytes == 0), EINVAL);
    if (dst != NULL)
    {
        _RESET_STRING(dst, sizeInBytes);
    }

    if (pConvertedChars != NULL)
    {
        *pConvertedChars = 0;
    }

    bufferSize = (n > sizeInBytes) ? sizeInBytes : n;
    _VALIDATE_RETURN_ERRCODE((bufferSize <= INT_MAX), EINVAL);

    retvalue = _wcstombs_helper(&retsize, dst, src, bufferSize);

    if (retvalue != 0)
    {
        if (dst != NULL)
        {
            _RESET_STRING(dst, sizeInBytes);
        }
        return retvalue;
    }

    /* count the null terminator */
    retsize++;

    if (dst != NULL)
    {
        /* return error if the string does not fit, unless n == _TRUNCATE */
        if (retsize > sizeInBytes)
        {
            if (n != _TRUNCATE)
            {
                _RESET_STRING(dst, sizeInBytes);
                _VALIDATE_RETURN_ERRCODE(sizeInBytes > retsize, ERANGE);
            }
            retsize = sizeInBytes;
            retvalue = STRUNCATE;
        }

        /* ensure the string is null terminated */
        dst[retsize - 1] = '\0';
    }

    if (pConvertedChars != NULL)
    {
        *pConvertedChars = retsize;
    }

    return retvalue;
}

