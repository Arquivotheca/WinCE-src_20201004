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
*wctomb.c - Convert wide character to multibyte character.
*
*
*Purpose:
*       Convert a wide character into the equivalent multibyte character.
*
*******************************************************************************/

#include <cruntime.h>
#include <stdlib.h>
#include <errno.h>
#include <locale.h>
#include <internal.h>
#include <limits.h>
#include <mtdll.h>
#include <setlocal.h>

/***
*errno_t __internal_wctomb_s() - Convert wide character to multibyte character.
*
*Purpose:
*       Convert a wide character into the equivalent multi-byte character,
*       according to the ANSI Codepage.
*
*Entry:
*       int *retvalue = pointer to a useful return value:
*           if s == NULL && sizeInBytes == 0: number of bytes needed for the conversion
*           if s == NULL && sizeInBytes > 0: the state information
*           if s != NULL : number of bytes used for the conversion
*           The pointer can be null.
*       char *s = pointer to multibyte character
*       size_t sizeInBytes = size of the destination buffer
*       wchar_t wchar = source wide character
*
*Exit:
*       The error code.
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

int __cdecl __internal_wctomb_s (
        int *pRetValue,
        char *dst,
        size_t sizeInBytes,
        wchar_t wchar
        )
{
    if (dst == NULL && sizeInBytes > 0)
    {
        /* indicate do not have state-dependent encodings */
        if (pRetValue != NULL)
        {
            *pRetValue = 0;
        }
        return 0;
    }

    if (pRetValue != NULL)
    {
        *pRetValue = -1;
    }

    /* validation section */
    /* we need to cast sizeInBytes to int, so we make sure we are not going to truncate sizeInBytes */
    _VALIDATE_RETURN_ERRCODE(sizeInBytes <= INT_MAX, EINVAL);

    if (dst == NULL)
    {
        /* sizeInBytes == 0, i.e. we return the size of the buffer */
        if (pRetValue != NULL)
        {
            *pRetValue = MB_CUR_MAX;
        }
        return 0;
    }
    else
    {
        int size;
        BOOL defused = FALSE;
        BOOL* pDefused = g_fCrtLegacyInputValidation ? NULL : &defused;

        if (((size = WideCharToMultiByte(CP_ACP,
                                         0,
                                         &wchar, 1,
                                         dst, (int)sizeInBytes,
                                         NULL,
                                         pDefused)) == 0) ||
            defused)
        {
            if (size == 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
            {
                if (dst != NULL && sizeInBytes > 0)
                {
                    memset(dst, 0, sizeInBytes);
                }
                _VALIDATE_RETURN_ERRCODE(("Buffer too small", 0), ERANGE);
            }
            _SET_ERRNO(EILSEQ);
            return EILSEQ;
        }

        if (pRetValue != NULL)
        {
            *pRetValue = size;
        }
        return 0;
    }
}

