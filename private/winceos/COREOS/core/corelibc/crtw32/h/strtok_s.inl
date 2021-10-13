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
*tcstok_s.inl - implementation of strtok_s
*
*
*Purpose:
*       This file contains the algorithm for strtok_s.
*
****/
 
_FUNC_PROLOGUE
char * __cdecl _FUNC_NAME(char *_String, const char *_Control, char **_Context)
{
    unsigned char *str;
    const unsigned char *ctl = (const unsigned char *)_Control;
    unsigned char map[32];
    int count;
 
    /* validation section */
    _VALIDATE_POINTER_ERROR_RETURN(_Context, EINVAL, NULL);
    _VALIDATE_POINTER_ERROR_RETURN(_Control, EINVAL, NULL);
    _VALIDATE_CONDITION_ERROR_RETURN(_String != NULL || *_Context != NULL, EINVAL, NULL);
 
    /* Clear control map */
    for (count = 0; count < 32; count++)
    {
        map[count] = 0;
    }
 
    /* Set bits in delimiter table */
    do {
        map[*ctl >> 3] |= (1 << (*ctl & 7));
    } while (*ctl++);
 
    /* If string is NULL, set str to the saved
    * pointer (i.e., continue breaking tokens out of the string
    * from the last strtok call) */
    if (_String != NULL)
    {
        str = (unsigned char *)_String;
    }
    else
    {
        str = (unsigned char *)*_Context;
    }
 
    /* Find beginning of token (skip over leading delimiters). Note that
    * there is no token iff this loop sets str to point to the terminal
    * null (*str == 0) */
    while ((map[*str >> 3] & (1 << (*str & 7))) && *str != 0)
    {
        str++;
    }
 
    _String = (char *)str;
 
    /* Find the end of the token. If it is not the end of the string,
    * put a null there. */
    for ( ; *str != 0 ; str++ )
    {
        if (map[*str >> 3] & (1 << (*str & 7)))
        {
            *str++ = 0;
            break;
        }
    }
 
    /* Update context */
    *_Context = (char *)str;
 
    /* Determine if a token has been found. */
    if (_String == (char *)str)
    {
        return NULL;
    }
    else
    {
        return _String;
    }
}
