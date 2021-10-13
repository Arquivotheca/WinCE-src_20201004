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
*mbccpy_s.inl - general implementation of _mbccpy_s
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This file contains the algorithm for _mbccpy_s.
*
*Revision History:
*       07-23-04  AC    Created
*       08-04-04  AC    Added support for locale argument
*       05-27-05  MSL   Source processing
*
****/
 
_FUNC_PROLOGUE
#if _USE_LOCALE_ARG
errno_t __cdecl _FUNC_NAME(unsigned char *_DEST, size_t _SizeInBytes, int *_PCopied, const unsigned char *_SRC, _LOCALE_ARG_DECL)
#else
errno_t __cdecl _FUNC_NAME(unsigned char *_DEST, size_t _SizeInBytes, int *_PCopied, const unsigned char *_SRC)
#endif
{
    /* validation section */
    _ASSIGN_IF_NOT_NULL(_PCopied, 0);
    _VALIDATE_STRING(_DEST, _SizeInBytes);
    if (_SRC == NULL)
    {
        *_DEST = '\0';
        _RETURN_EINVAL;
    }
 
#if _USE_LOCALE_ARG
    _LOCALE_UPDATE;
#endif

    /* copy */
    if (_ISMBBLEAD(*_SRC))
    {
        if (_SRC[1] == '\0')
        {
            /* the source string contained a lead byte followed by the null terminator:
               we copy only the null terminator and return EILSEQ to indicate the
               malformed char */
            *_DEST = '\0';
            _ASSIGN_IF_NOT_NULL(_PCopied, 1);
            _RETURN_MBCS_ERROR;
        }
        if (_SizeInBytes < 2)
        {
            *_DEST = '\0';
            _RETURN_BUFFER_TOO_SMALL(_DEST, _SizeInBytes);
        }
        *_DEST++ = *_SRC++;
        *_DEST = *_SRC;
        _ASSIGN_IF_NOT_NULL(_PCopied, 2);
    }
    else
    {
        *_DEST = *_SRC;
        _ASSIGN_IF_NOT_NULL(_PCopied, 1);
    }
 
    _RETURN_NO_ERROR;
}
