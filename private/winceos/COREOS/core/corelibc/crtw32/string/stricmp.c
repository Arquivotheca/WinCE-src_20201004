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
*stricmp.c - contains case-insensitive string comp routine _stricmp/_strcmpi
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       contains _stricmp(), also known as _strcmpi()
*
*Revision History:
*       05-31-89  JCR   C version created.
*       02-27-90  GJF   Fixed calling type, #include <cruntime.h>, fixed
*                       copyright.
*       07-25-90  SBM   Added #include <ctype.h>
*       10-02-90  GJF   New-style function declarator.
*       01-18-91  GJF   ANSI naming.
*       10-11-91  GJF   Update! Comparison of final bytes must use unsigned
*                       chars.
*       11-08-91  GJF   Fixed compiler warning.
*       09-02-93  GJF   Replaced _CALLTYPE1 with __cdecl.
*       09-21-93  CFW   Avoid cast bug.
*       10-18-94  GJF   Sped up C locale. Also, made multi-thread safe.
*       12-29-94  CFW   Merge non-Win32.
*       09-26-95  GJF   New locking macro, and scheme, for functions which
*                       reference the locale.
*       11-15-95  BWT   Fix _NTSUBSET_
*       08-10-98  GJF   Revised multithread support based on threadlocinfo
*                       struct.
*       08-26-98  GJF   Split out ASCII-only version.
*       09-17-98  GJF   Silly errors in __ascii_stricmp (found by DEC folks)
*       05-17-99  PML   Remove all Macintosh support.
*       01-26-00  GB    Modified stricmp for performance.
*       03-09-00  GB    Moved the performance code to toupper and tolower.
*                       restored the original file.
*       08-22-00  GB    Self included this file so as that stricmp and strcmpi
*                       have same code.
*       11-12-01  GB    Added support for new locale implementation.
*       03-26-03  SSM   Use C version of the function for __ascii_stricmp for
*                       i386
*       10-02-03  AC    Added validation.
*       10-08-04  AGH   Added validations to _stricmp
*
*******************************************************************************/

#include <cruntime.h>
#include <string.h>
#include <locale.h>
#include <ctype.h>
#include <internal.h>
#include <mtdll.h>
#include <setlocal.h>


/***
*int _strcmpi(dst, src), _strcmpi(dst, src) - compare strings, ignore case
*
*Purpose:
*       _stricmp/_strcmpi perform a case-insensitive string comparision.
*       For differences, upper case letters are mapped to lower case.
*       Thus, "abc_" < "ABCD" since "_" < "d".
*
*Entry:
*       char *dst, *src - strings to compare
*
*Return:
*       Returns <0 if dst < src
*       Returns 0 if dst = src
*       Returns >0 if dst > src
*       Returns _NLSCMPERROR is something went wrong
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

#ifndef _WCE_BOOTCRT
// BOOTCRT doesn't need the NLS
extern "C" int __cdecl _stricmp_l (
        const char * dst,
        const char * src,
        _locale_t plocinfo
        )
{
    int f,l;
    _LocaleUpdate _loc_update(plocinfo);

    /* validation section */
    _VALIDATE_RETURN(dst != NULL, EINVAL, _NLSCMPERROR);
    _VALIDATE_RETURN(src != NULL, EINVAL, _NLSCMPERROR);

    if ( _loc_update.GetLocaleT()->locinfo->locale_name[LC_CTYPE] == NULL )
    {
        return __ascii_stricmp(dst, src);
    }
    else 
    {
        do
        {   
            f = _tolower_l( (unsigned char)(*(dst++)), _loc_update.GetLocaleT() );
            l = _tolower_l( (unsigned char)(*(src++)), _loc_update.GetLocaleT() );
        } while ( f && (f == l) );
    }

    return(f - l);
}

extern "C" int __cdecl _stricmp (
        const char * dst,
        const char * src
        )
{
#if  !defined(_NTSUBSET_)
    if (__locale_changed == 0)
    {
        /* validation section */
        _VALIDATE_RETURN(dst != NULL, EINVAL, _NLSCMPERROR);
        _VALIDATE_RETURN(src != NULL, EINVAL, _NLSCMPERROR);

        return __ascii_stricmp(dst, src);
    }
    else
    {
        return _stricmp_l(dst, src, NULL);
    }
#else
    return __ascii_stricmp(dst, src);
#endif  /* !_NTSUBSET_ */
}
#else
extern "C" int __cdecl _stricmp (
        const char * dst,
        const char * src
        )
{
    return __ascii_stricmp(dst, src);
}

  
#endif // _WCE_BOOTCRT

#ifndef _M_IX86
 
extern "C" int __cdecl __ascii_stricmp (  
        const char * dst,  
        const char * src  
        )  
{  
    int f, l;  
  
    do  
    {  
        if ( ((f = (unsigned char)(*(dst++))) >= 'A') && (f <= 'Z') )  
            f -= 'A' - 'a';  
        if ( ((l = (unsigned char)(*(src++))) >= 'A') && (l <= 'Z') )  
            l -= 'A' - 'a';  
    }  
    while ( f && (f == l) );  
  
    return(f - l);  
}  
#endif //_M_IX86