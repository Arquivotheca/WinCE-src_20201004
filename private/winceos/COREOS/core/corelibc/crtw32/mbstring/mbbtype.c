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
*mbbtype.c - Return type of byte based on previous byte (MBCS)
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Return type of byte based on previous byte (MBCS)
*
*Revision History:
*       11-19-92  KRS   Ported from 16-bit sources.
*       10-05-93  GJF   Replace _CRTAPI1 with __cdecl.
*       04-03-98  GJF   Revised multithread support based on threadmbcinfo
*                       structs
*
*******************************************************************************/

#ifdef  _MBCS

#include <mtdll.h>
#include <cruntime.h>
#include <mbdata.h>
#include <mbctype.h>
#include <mbstring.h>
#include <locale.h>
#include <setlocal.h>

/***
*int _mbbtype(c, ctype) - Return type of byte based on previous byte (MBCS)
*
*Purpose:
*       Returns type of supplied byte.  This decision is context
*       sensitive so a control test condition is supplied.  Normally,
*       this is the type of the previous byte in the string.
*
*Entry:
*       unsigned char c = character to be checked
*       int ctype = control test condition (i.e., type of previous char)
*
*Exit:
*       _MBC_LEAD      = if 1st byte of MBCS char
*       _MBC_TRAIL     = if 2nd byte of MBCS char
*       _MBC_SINGLE    = valid single byte char
*
*       _MBC_ILLEGAL   = if illegal char
*
*Exceptions:
*
*******************************************************************************/

extern "C" int __cdecl _mbbtype_l(
        unsigned char c,
        int ctype,
        _locale_t plocinfo
        )
{
        _LocaleUpdate _loc_update(plocinfo);

        switch(ctype) {

            case(_MBC_LEAD):
                if ( _ismbbtrail_l(c, _loc_update.GetLocaleT()) )
                    return(_MBC_TRAIL);
                else
                    return(_MBC_ILLEGAL);

            case(_MBC_TRAIL):
            case(_MBC_SINGLE):
            case(_MBC_ILLEGAL):
            default:
                if ( _ismbblead_l(c, _loc_update.GetLocaleT()) )
                    return(_MBC_LEAD);
                else if (_ismbbprint_l( c, _loc_update.GetLocaleT()))
                    return(_MBC_SINGLE);
                else
                    return(_MBC_ILLEGAL);

        }

}

extern "C" int (__cdecl _mbbtype)(
        unsigned char c,
        int ctype
        )
{
        return( _mbbtype_l(c, ctype, NULL) );
}

#endif  /* _MBCS */
