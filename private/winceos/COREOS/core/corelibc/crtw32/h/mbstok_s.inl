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
*tcstok_s.inl - implementation of _mbstok_s
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This file contains the algorithm for _mbstok_s.
*
*Revision History:
*       07-27-04  AC    Created
*       08-03-04  AC    Added support for locale argument
*       09-27-04  ESC   Update to handle lead byte of (lead,null) as string terminator.
*       05-27-05  MSL   Source processing
*
****/
 
_FUNC_PROLOGUE
#if _USE_LOCALE_ARG
unsigned char * __cdecl _FUNC_NAME(unsigned char *_String, const unsigned char *_Control, unsigned char **_Context, _LOCALE_ARG_DECL)
#else
unsigned char * __cdecl _FUNC_NAME(unsigned char *_String, const unsigned char *_Control, unsigned char **_Context)
#endif
{
    unsigned char *token;
    const unsigned char *ctl;
    int dbc;
 
    /* validation section */
    _VALIDATE_POINTER_ERROR_RETURN(_Context, EINVAL, NULL);
    _VALIDATE_POINTER_ERROR_RETURN(_Control, EINVAL, NULL);
    _VALIDATE_CONDITION_ERROR_RETURN(_String != NULL || *_Context != NULL, EINVAL, NULL);
 
#if _USE_LOCALE_ARG
    _LOCALE_UPDATE;
    if (_LOCALE_SHORTCUT_TEST)
    {
        return (unsigned char*)_SB_FUNC_NAME((char *)_String, (const char *)_Control, (char **)_Context);
    }
#endif

    /* If string==NULL, continue with previous string */
    if (!_String)
    {
        _String = *_Context;
    }

    /* Find beginning of token (skip over leading delimiters). Note that
    * there is no token iff this loop sets string to point to the terminal null. */
    for ( ; *_String != 0; _String++)
    {
        for (ctl = _Control; *ctl != 0; ctl++)
        {
            if (_ISMBBLEAD(*ctl))
            {
                if (ctl[1] == 0)
                {
                    ctl++;
                    _SET_MBCS_ERROR;
                    break;
                }
                if (*ctl == *_String && ctl[1] == _String[1])
                {
                    break;
                }
                ctl++;
            }
            else
            {
                if (*ctl == *_String)
                {
                    break;
                }
            }
        }
        if (*ctl == 0)
        {
            break;
        }
        if (_ISMBBLEAD(*_String))
        {
            _String++;
            if (*_String == 0)
            {
                _SET_MBCS_ERROR;
                break;
            }
        }
    }
 
    token = _String;
 
    /* Find the end of the token. If it is not the end of the string,
    * put a null there. */
    for ( ; *_String != 0; _String++)
    {
        for (ctl = _Control, dbc = 0; *ctl != 0; ctl++)
        {
            if (_ISMBBLEAD(*ctl))
            {
                if (ctl[1] == 0)
                {
                    ctl++;
                    break;
                }
                if (ctl[0] == _String[0] && ctl[1] == _String[1])
                {
                    dbc = 1;
                    break;
                }
                ctl++;
            }
            else
            {
                if (*ctl == *_String)
                {
                    break;
                }
            }
        }
        if (*ctl != 0)
        {
            *_String++ = 0;
            if (dbc)
            {
                *_String++ = 0;
            }
            break;
        }
        if (_ISMBBLEAD(_String[0]))
        {
            if (_String[1] == 0)
            {
                *_String = 0;
                break;
            }
            _String++;
        }
    }
 
    /* Update the context */
    *_Context = _String;
 
    /* Determine if a token has been found. */
    if (token == _String)
    {
        return NULL;
    }
    else
    {
        return token;
    }
}
