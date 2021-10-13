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
*tcstok_s.inl - general implementation of _tcstok_s
*
*Purpose:
*       This file contains the general algorithm for strtok_s and its variants.
*
****/
 
_FUNC_PROLOGUE
_CHAR * __cdecl _FUNC_NAME(_CHAR *_String, const _CHAR *_Control, _CHAR **_Context)
{
    _CHAR *token;
    const _CHAR *ctl;
 
    /* validation section */
    _VALIDATE_POINTER_ERROR_RETURN(_Context, EINVAL, NULL);
    _VALIDATE_POINTER_ERROR_RETURN(_Control, EINVAL, NULL);
    _VALIDATE_CONDITION_ERROR_RETURN(_String != NULL || *_Context != NULL, EINVAL, NULL);
 
    /* If string==NULL, continue with previous string */
    if (!_String)
    {
        _String = *_Context;
    }
 
    /* Find beginning of token (skip over leading delimiters). Note that
    * there is no token iff this loop sets string to point to the terminal null. */
    for ( ; *_String != 0 ; _String++)
    {
        for (ctl = _Control; *ctl != 0 && *ctl != *_String; ctl++)
            ;
        if (*ctl == 0)
        {
            break;
        }
    }
 
    token = _String;
 
    /* Find the end of the token. If it is not the end of the string,
    * put a null there. */
    for ( ; *_String != 0 ; _String++)
    {
        for (ctl = _Control; *ctl != 0 && *ctl != *_String; ctl++)
            ;
        if (*ctl != 0)
        {
            *_String++ = 0;
            break;
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
