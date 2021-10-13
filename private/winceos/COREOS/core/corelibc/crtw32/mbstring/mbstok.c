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
*mbstok.c - Break string into tokens (MBCS)
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Break string into tokens (MBCS)
*
*Revision History:
*       11-19-92  KRS   Ported from 16-bit sources.
*       12-04-92  KRS   Added MTHREAD support.
*       02-17-93  GJF   Changed for new _getptd().
*       07-14-93  KRS   Fix: all references should be to _mtoken, not _token.
*       09-27-93  CFW   Remove Cruiser support.
*       10-06-93  GJF   Replaced _CRTAPI1 with __cdecl, MTHREAD with _MT.
*       04-15-93  CFW   Add _MB_CP_LOCK.
*       05-09-94  CFW   Optimize for SBCS.
*       05-19-94  CFW   Enable non-Win32.
*       09-11-97  GJF   Replaced __mbcodepage == 0 with _ISNOTMBCP.
*       04-21-98  GJF   Revised multithread support based on threadmbcinfo
*                       structs
*       05-09-02  MSL   Fixed to cope correctly with leadbyte, EOS VS7 340555
*       10-09-03  AC    Added secure version.
*       08-04-04  AC    Moved secure version into mbstok_s.inl and mbstok_s.c
*
*******************************************************************************/

#ifdef  _MBCS

#include <mtdll.h>
#include <cruntime.h>
#include <string.h>
#include <mbdata.h>
#include <mbctype.h>
#include <mbstring.h>
#include <stddef.h>
#include <internal.h>
#include <locale.h>
#include <setlocal.h>

/***
* _mbstok - Break string into tokens (MBCS)
*
*Purpose:
*       strtok considers the string to consist of a sequence of zero or more
*       text tokens separated by spans of one or more control chars. the first
*       call, with string specified, returns a pointer to the first char of the
*       first token, and will write a null char into string immediately
*       following the returned token. subsequent calls with zero for the first
*       argument (string) will work thru the string until no tokens remain. the
*       control string may be different from call to call. when no tokens remain
*       in string a NULL pointer is returned. remember the control chars with a
*       bit map, one bit per ascii char. the null char is always a control char.
*
*       MBCS chars supported correctly.
*
*Entry:
*       char *string = string to break into tokens.
*       char *sepset = set of characters to use as seperators
*
*Exit:
*       returns pointer to token, or NULL if no more tokens
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

extern "C" unsigned char * __cdecl _mbstok_l(
        unsigned char * string,
        const unsigned char * sepset,
        _locale_t plocinfo
        )
{
        _ptiddata_persist ptd = _getptd_persist();
        return _mbstok_s_l(string, sepset, &ptd->_mtoken, plocinfo);
}

extern "C" unsigned char * __cdecl _mbstok(
        unsigned char * string,
        const unsigned char * sepset
        )
{
    /* We call the deprecated _mbstok_l (and not _mbstok_s_l) so that we keep one 
     * single nextoken in the single thread case, i.e. the nextoken declared as static 
     * inside _mbstok_l
     */
    _BEGIN_SECURE_CRT_DEPRECATION_DISABLE
    return _mbstok_l(string, sepset, NULL);
    _END_SECURE_CRT_DEPRECATION_DISABLE
}

#endif  /* _MBCS */
