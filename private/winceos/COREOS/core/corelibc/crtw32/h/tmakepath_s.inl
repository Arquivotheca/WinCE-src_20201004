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
*tmakepath_s.inl - general implementation of _tmakepath_s
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This file contains the general algorithm for _makepath_s and its variants.
*
*Revision History:
*       06-24-04  AC    Created
*       07-16-04  AC    Fixed __cdecl
*       08-05-04  AC    Fixed mbs support
*       09-24-04  MSL   Added return to silence flow analysis
*       01-21-05  MSL   SAL Annotation
*
*******************************************************************************/
 
_FUNC_PROLOGUE
errno_t __cdecl _FUNC_NAME(_CHAR *_DEST, size_t _SIZE, const _CHAR *_Drive, const _CHAR *_Dir, const _CHAR *_Filename, const _CHAR *_Ext)
{
    size_t written;
    const _CHAR *p;
    _CHAR *d;
 
    /* validation section */
    _VALIDATE_STRING(_DEST, _SIZE);
 
    /* copy drive */
    written = 0;
    d = _DEST;
    if (_Drive != NULL && *_Drive != 0)
    {
        written += 2;
        if(written >= _SIZE)
        {
            goto error_return;
        }
        *d++ = *_Drive;
        *d++ = _T(':');
    }
 
    /* copy dir */
    p = _Dir;
    if (p != NULL && *p != 0)
    {
        do {
            if(++written >= _SIZE)
            {
                goto error_return;
            }
            *d++ = *p++;
        } while (*p != 0);
 
#if _MBS_SUPPORT
        p = _MBSDEC(_Dir, p);
#else
        p = p - 1;
#endif
        if (*p != _T('/') && *p != _T('\\'))
        {
            if(++written >= _SIZE)
            {
                goto error_return;
            }
            *d++ = _T('\\');
        }
    }
 
    /* copy fname */
    p = _Filename;
    if (p != NULL)
    {
        while (*p != 0) 
        {
            if(++written >= _SIZE)
            {
                goto error_return;
            }
            *d++ = *p++;
        }
    }
 
    /* copy extension; check to see if a '.' needs to be inserted */
    p = _Ext;
    if (p != NULL)
    {
        if (*p != 0 && *p != _T('.'))
        {
            if(++written >= _SIZE)
            {
                goto error_return;
            }
            *d++ = _T('.');
        }
        while (*p != 0)
        {
            if(++written >= _SIZE)
            {
                goto error_return;
            }
            *d++ = *p++;
        }
    }
 
    if(++written > _SIZE)
    {
        goto error_return;
    }
    *d = 0;
    _FILL_STRING(_DEST, _SIZE, written);
    _RETURN_NO_ERROR;
 
error_return:
    _RESET_STRING(_DEST, _SIZE);
    _RETURN_BUFFER_TOO_SMALL(_DEST, _SIZE);

    /* should never happen, but compiler can't tell */
    return EINVAL;
}
