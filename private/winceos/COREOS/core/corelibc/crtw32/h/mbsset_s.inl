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
*mbsset_s.inl - implementation of _mbsset_s
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This file contains the algorithm for _mbsset_s.
*
*Revision History:
*       07-27-04  AC    Created
*       08-03-04  AC    Fixed small error (typo in macro _RETURN_DEST_NOT_NULL_TERMINATED)
*       08-04-04  AC    Added support for locale argument
*       09-27-04  ESC   Write 0 over invalid termination instead of ' '
*       10-30-04  AC    Fix according to the spec
*                       VSW#385890, 386380
*       05-27-05  MSL   Source processing
*
****/
 
_FUNC_PROLOGUE
#if _USE_LOCALE_ARG
errno_t __cdecl _FUNC_NAME(unsigned char *_DEST, size_t _SizeInBytes, unsigned int _Value, _LOCALE_ARG_DECL)
#else
errno_t __cdecl _FUNC_NAME(unsigned char *_DEST, size_t _SizeInBytes, unsigned int _Value)
#endif
{
    int mbcs_error = 0;
    unsigned char *p;
    size_t available;
    unsigned char highval, lowval;
 
    /* validation section */
    _VALIDATE_STRING(_DEST, _SizeInBytes);
 
#if _USE_LOCALE_ARG
    _LOCALE_UPDATE;
    if (_LOCALE_SHORTCUT_TEST)
    {
        return _SB_FUNC_NAME((char *)_DEST, _SizeInBytes, (int)_Value);
    }
#endif

    p = _DEST;
    available = _SizeInBytes;
    highval = (unsigned char)(_Value >> 8);
    lowval = (unsigned char)(_Value & 0x00ff);

    /* ensure _Value is a valid mbchar */
    if ((highval != 0 && (lowval == 0 || !_ISMBBLEAD(highval))) ||
        (highval == 0 && _ISMBBLEAD(lowval)))
    {
        _RESET_STRING(_DEST, _SizeInBytes);
        _RETURN_MBCS_ERROR;
    }

    if (highval != 0)
    {
        while (*p != 0 && --available > 0)
        {
            if (p[1] == 0)
            {
                /* do not orphan leadbyte */
                *p = 0;
                ++available;
                mbcs_error = 1;
                break;
            }
            *p++ = highval;
            if (--available == 0)
            {
                break;
            }
            *p++ = lowval;
        }
    }
    else
    {
        while (*p != 0 && --available > 0)
        {
            *p++ = lowval;
        }
    }
 
    if (available == 0)
    {
        _RESET_STRING(_DEST, _SizeInBytes);
        _RETURN_DEST_NOT_NULL_TERMINATED(_DEST, _SizeInBytes);
    }
    _FILL_STRING(_DEST, _SizeInBytes, _SizeInBytes - available + 1);
 
    if (mbcs_error)
    {
        _RETURN_MBCS_ERROR;
    }
    else
    {
        _RETURN_NO_ERROR;
    }
}
 
