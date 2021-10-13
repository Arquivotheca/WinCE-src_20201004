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
*tcsnset_s.inl - general implementation of _tcsnset_s
*
*Purpose:
*       This file contains the general algorithm for _strnset_s and its variants.
*
****/
 
_FUNC_PROLOGUE
errno_t __cdecl _FUNC_NAME(_CHAR *_DEST, size_t _SIZE, _CHAR_INT _Value, size_t _COUNT)
{
    _CHAR *p;
    size_t available;
 
    /* validation section */
    if (_COUNT == 0 && _DEST == NULL && _SIZE == 0)
    {
        /* this case is allowed; nothing to do */
        _RETURN_NO_ERROR;
    }
    _VALIDATE_STRING(_DEST, _SIZE);

    _ASSERT_EXPR((_COUNT < _SIZE), L"Buffer is too small");

    p = _DEST;
    available = _SIZE;
    while (*p != 0 && _COUNT > 0 && --available > 0)
    {
        *p++ = (_CHAR)_Value;
        --_COUNT;
    }
 
    if (_COUNT == 0)
    {
        /* ensure the string is null-terminated */
        while (*p != 0 && --available > 0)
        {
            ++p;
        }
    }
 
    if (available == 0)
    {
        _RESET_STRING(_DEST, _SIZE);
        _RETURN_DEST_NOT_NULL_TERMINATED(_DEST, _SIZE);
    }
    _FILL_STRING(_DEST, _SIZE, _SIZE - available + 1);
    _RETURN_NO_ERROR;
}
 
