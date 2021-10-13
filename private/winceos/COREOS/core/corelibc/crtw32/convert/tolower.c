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
*tolower.c - convert character to lower case
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines function versions of _tolower() and tolower().
*
*Revision History:
*       11-09-84  DFW   created
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       02-23-89  GJF   Added function version of _tolower and cleaned up.
*       03-26-89  GJF   Migrated to 386 tree
*       03-06-90  GJF   Fixed calling type, added #include <cruntime.h> and
*                       fixed copyright.
*       09-27-90  GJF   New-style function declarators.
*       10-11-91  ETC   Locale support for tolower under _INTL switch.
*       12-10-91  ETC   Updated nlsapi; added multithread.
*       12-17-92  KRS   Updated and optimized for latest NLSAPI.  Bug-fixes.
*       01-19-93  CFW   Fixed typo.
*       03-25-93  CFW   _tolower now defined when _INTL.
*       04-06-93  SKS   Replace _CRTAPI* with _cdecl
*       06-01-93  CFW   Simplify "C" locale test.
*       06-02-93  SRW   ignore _INTL if _NTSUBSET_ defined.
*       09-15-93  CFW   Change buffer to unsigned char to fix nasty cast bug.
*       09-15-93  CFW   Use ANSI conformant "__" names.
*       09-22-93  CFW   Use __crtxxx internal NLS API wrapper.
*       09-28-93  GJF   Merged NT SDK and Cuda versions.
*       11-09-93  CFW   Add code page for __crtxxx().
*       09-06-94  CFW   Remove _INTL switch.
*       10-17-94  GJF   Sped up for C locale. Also, added __tolower_lk.
*       01-07-95  CFW   Mac merge cleanup.
*       09-26-95  GJF   New locking macro, and scheme, for functions which
*                       reference the locale.
*       04-01-96  BWT   POSIX work.
*       06-25-96  GJF   Removed DLL_FOR_WIN32S. Replaced defined(_WIN32) with
*                       !defined(_MAC). Polished the format a bit.
*       03-17-97  RDK   Added error flag to __crtLCMapStringA.
*       08-27-98  GJF   Revised multithread support based on threadlocinfo
*                       struct.
*       05-17-99  PML   Remove all Macintosh support.
*       09-03-00  GB    Modified for increased performance.
*       04-03-01  PML   Reverse lead/trail bytes in composed char (vs7#232853)
*       11-12-01  GB    Added support for new locale implementation.
*       09-26-04  JL    Modified _tolower_l to be consistent with Everett
*                       with respect to undefined behavior.
*
*******************************************************************************/

#include <cruntime.h>
#include <stddef.h>
#include <locale.h>
#include <awint.h>
#include <ctype.h>
#include <internal.h>
#include <mtdll.h>
#include <setlocal.h>

/* remove macro defintions of _tolower() and tolower()
 */
#undef  _tolower
#undef  tolower

/* define function-like macro equivalent to _tolower()
 */
#define mklower(c)  ( (c)-'A'+'a' )

/***
*int _tolower(c) - convert character to lower case
*
*Purpose:
*       _tolower() is simply a function version of the macro of the same name.
*
*Entry:
*       c - int value of character to be converted
*
*Exit:
*       returns int value of lower case representation of c
*
*Exceptions:
*
*******************************************************************************/

extern "C" int __cdecl _tolower (
        int c
        )
{
        return(mklower(c));
}

/***
*int _tolower_l(int c, _locale_t _loc_update.GetLocaleT()) - convert character to lower case
*
*Purpose:
*       Multi-thread function only!
*
*Entry:
*
*Exit:
*
*Exceptions:
*
*******************************************************************************/

extern "C" int __cdecl _tolower_l (
        int c, 
        _locale_t plocinfo
        )
{
    int size;
    unsigned char inbuffer[3];
    unsigned char outbuffer[3];
    _LocaleUpdate _loc_update(plocinfo);

    /* if checking case of c does not require API call, do it */
    if ( (unsigned)c < 256 )
    {
        if (_isupper_l(c, _loc_update.GetLocaleT()))
            return _loc_update.GetLocaleT()->locinfo->pclmap[c];
        else
            return c;
    }

    /* convert int c to multibyte string */
    if (_loc_update.GetLocaleT()->locinfo->mb_cur_max > 1 && _isleadbyte_l(c >> 8 & 0xff, _loc_update.GetLocaleT()) )
    {
        inbuffer[0] = (c >> 8 & 0xff); /* put lead-byte at start of str */
        inbuffer[1] = (unsigned char)c;
        inbuffer[2] = 0;
        size = 2;
    } else {
#ifndef _WCE_BOOTCRT
        /* this is an undefined behavior, should probably use towlower instead */
        errno = EILSEQ;
#endif
        inbuffer[0] = (unsigned char)c;
        inbuffer[1] = 0;
        size = 1;
    }

    /* convert to lowercase */
#ifndef _WCE_BOOTCRT
    if ( 0 == (size = __crtLCMapStringA(
                    _loc_update.GetLocaleT(),
                    _loc_update.GetLocaleT()->locinfo->locale_name[LC_CTYPE],
                    LCMAP_LOWERCASE,
                    (LPCSTR)inbuffer, 
                    size, 
                    (LPSTR)outbuffer, 
                    3, 
                    _loc_update.GetLocaleT()->locinfo->lc_codepage,
                    TRUE)) )
#else
    wchar_t inbuf[3], outbuf[3];
    if ( 0 == (size = __crtLCMapStringA2(
                    _loc_update.GetLocaleT(),
                    _loc_update.GetLocaleT()->locinfo->locale_name[LC_CTYPE],
                    LCMAP_LOWERCASE,
                    (LPCSTR)inbuffer, 
                    size, 
                    (LPSTR)outbuffer, 
                    3, 
                    _loc_update.GetLocaleT()->locinfo->lc_codepage,
                    inbuf, outbuf,
                    TRUE)) )
#endif
    {
        return c;
    }

    /* construct integer return value */
    if (size == 1)
        return ((int)outbuffer[0]);
    else
        return ((int)outbuffer[1] | ((int)outbuffer[0] << 8));
}

/***
*int tolower(c) - convert character to lower case
*
*Purpose:
*       tolower() is simply a function version of the macro of the same name.
*
*Entry:
*       c - int value of character to be converted
*
*Exit:
*       if c is an upper case letter, returns int value of lower case
*       representation of c. otherwise, it returns c.
*
*Exceptions:
*
*******************************************************************************/


extern "C" int __cdecl tolower (
        int c
        )
{
#if   !defined (_NTSUBSET_) && !defined(_POSIX_)

    if (__locale_changed == 0)
    {
        return __ascii_towlower(c);
    }
    else
    {
        return _tolower_l(c, NULL);
    }
#else

    return(isupper(c) ? mklower(c) : c);

#endif
}

