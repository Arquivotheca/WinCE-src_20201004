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
*tcscat_s.inl - general implementation of _tcscpy_s
*
*Purpose:
*       This file contains the general algorithm for strcat_s and its variants.
*
****/
 
_FUNC_PROLOGUE
errno_t __cdecl _FUNC_NAME(_CHAR *_DEST, size_t _SIZE, const _CHAR *_SRC)
{
    _CHAR *p;
    size_t available;
 
    /* validation section */
    _VALIDATE_STRING(_DEST, _SIZE);
    _VALIDATE_POINTER_RESET_STRING(_SRC, _DEST, _SIZE);
 
    p = _DEST;
    available = _SIZE;
    while (available > 0 && *p != 0)
    {
        p++;
        available--;
    }
 
    if (available == 0)
    {
        _RESET_STRING(_DEST, _SIZE);
        _RETURN_DEST_NOT_NULL_TERMINATED(_DEST, _SIZE);
    }
 
    while ((*p++ = *_SRC++) != 0 && --available > 0)
    {
    }
 
    if (available == 0)
    {
        _RESET_STRING(_DEST, _SIZE);
        _RETURN_BUFFER_TOO_SMALL(_DEST, _SIZE);
    }
    _FILL_STRING(_DEST, _SIZE, _SIZE - available + 1);
    _RETURN_NO_ERROR;
}
 
