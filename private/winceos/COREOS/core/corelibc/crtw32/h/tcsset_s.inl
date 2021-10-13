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
*tcsset_s.inl - general implementation of _tcsset_s
*
*Purpose:
*       This file contains the general algorithm for _strset_s and its variants.
*
****/
 
_FUNC_PROLOGUE
errno_t __cdecl _FUNC_NAME(__inout_ecount_z(_SIZE) _CHAR *_DEST, __in size_t _SIZE, __in _CHAR_INT _Value)
{
    _CHAR *p;
    size_t available;
 
    /* validation section */
    _VALIDATE_STRING(_DEST, _SIZE);
    
    p = _DEST;
    available = _SIZE;
    while (*p != 0 && --available > 0)
    {
        *p++ = (_CHAR)_Value;
    }
 
    if (available == 0)
    {
        _RESET_STRING(_DEST, _SIZE);
        _RETURN_DEST_NOT_NULL_TERMINATED(_DEST, _SIZE);
    }
    _FILL_STRING(_DEST, _SIZE, _SIZE - available + 1);
    _RETURN_NO_ERROR;
}
 
