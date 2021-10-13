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
*tsnprintf_s.inl - general implementation of snprintf_s and vsnprintf_s
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This file contains the general algorithm for snprintf_s and its variants.
*
*Revision History:
*       07-14-04  AC    Created
*       08-04-04  AC    Fixed return value in error case (always -1)
*
****/
 
#pragma warning(push)
#pragma warning(disable : 4793)
_FUNC_PROLOGUE
int __cdecl _FUNC_NAME(_CHAR *_DEST, size_t _SIZE, size_t _Count, const _CHAR *_Format, ...)
{
    va_list arglist;
    va_start(arglist, _Format);
    return _VFUNC_NAME(_DEST, _SIZE, _Count, _Format, arglist);
}
#pragma warning(pop)
 
_FUNC_PROLOGUE
int __cdecl _VFUNC_NAME(_CHAR *_DEST, size_t _SIZE, size_t _Count, const _CHAR *_Format, va_list _ArgList)
{
    int written = -1;
 
    /* validation section */
    _VALIDATE_POINTER_ERROR(_Format, -1);
    if (_Count == 0 && _DEST == NULL && _SIZE == 0)
    {
        /* this case is allowed; nothing to do */
        return 0;
    }
    _VALIDATE_STRING_ERROR(_DEST, _SIZE, -1);
 
    if (_SIZE > _Count)
    {
        written = _OUTPUT_FUNC(_DEST, _Count + 1, _Format, _ArgList);
        if (written == -2)
        {
            /* the string has been truncated, return -1 */
            _FILL_STRING(_DEST, _SIZE, _Count + 1);
            return -1;
        }
    }
    else
    {
        _ASSERT_EXPR((!_CrtGetCheckCount() || _COUNT < _SIZE), L"Buffer is too small");

        written = _OUTPUT_FUNC(_DEST, _SIZE, _Format, _ArgList);
        /* we allow truncation if _Count == _TRUNCATE */
        if (written == -2 && _Count == _TRUNCATE)
        {
            return -1;
        }
    }

    if (written < 0)
    {
        _RESET_STRING(_DEST, _SIZE);
        if (written == -2)
        {
            _RETURN_BUFFER_TOO_SMALL_ERROR(_DEST, _SIZE, -1);
        }
        return -1;
    }
    _FILL_STRING(_DEST, _SIZE, written + 1);
    return written;
}
