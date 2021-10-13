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
*rename.c - rename file
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines rename() - rename a file
*
*Revision History:
*       06-06-89  PHG   Module created, based on asm version
*       03-07-90  GJF   Made calling type _CALLTYPE2 (for now), added #include
*                       <cruntime.h>, fixed compiler warnings and fixed the
*                       copyright. Also, cleaned up the formatting a bit.
*       03-30-90  GJF   Now _CALLTYPE1.
*       07-24-90  SBM   Removed '32' from API names
*       09-27-90  GJF   New-style function declarator.
*       12-04-90  SRW   Changed to include <oscalls.h> instead of <doscalls.h>
*       12-06-90  SRW   Added _CRUISER_ and _WIN32 conditionals.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       11-01-93  CFW   Enable Unicode variant, rip out Cruiser.
*       02-08-95  JWM   Spliced _WIN32 & Mac versions.
*       07-01-96  GJF   Replaced defined(_WIN32) with !defined(_MAC). Also,
*                       detab-ed and cleaned up the format a bit.
*       05-17-99  PML   Remove all Macintosh support.
*
*******************************************************************************/

#include <cruntime.h>
#include <oscalls.h>
#include <internal.h>
#include <io.h>
#include <tchar.h>
#include <malloc.h>
#include <dbgint.h>
#include <awint.h>

/***
*int rename(oldname, newname) - rename a file
*
*Purpose:
*       Renames a file to a new name -- no file with new name must
*       currently exist.
*
*Entry:
*       _TSCHAR *oldname -      name of file to rename
*       _TSCHAR *newname -      new name for file
*
*Exit:
*       returns 0 if successful
*       returns not 0 and sets errno if not successful
*
*Exceptions:
*
*******************************************************************************/

#ifndef _UNICODE

int __cdecl rename( 
        const char *oldname,
        const char *newname )
{
    /*
       We want to use MoveFileExW but not MoveFileExA (or MoveFile).
       So convert the strings to Unicode representation and call _wrename.

       Note that minwin does not support OEM CP for conversion via
       the FileAPIs APIs. So we unconditionally use the ACP for conversion
    */
    int oldnamelen;
    int newnamelen;
    wchar_t *widenamesbuffer = NULL;
    wchar_t *oldnamew = NULL;
    wchar_t *newnamew = NULL;
    int retval;
    UINT codePage = CP_ACP;

#ifndef _WIN32_WCE
#ifndef _CORESYS
    if (!__crtIsPackagedApp() && !AreFileApisANSI())
        codePage = CP_OEMCP;
#endif
#endif // _WIN32_WCE

    if (  ( (oldnamelen = MultiByteToWideChar(codePage, 0, oldname, -1, 0, 0) ) == 0 )
        || ( (newnamelen = MultiByteToWideChar(codePage, 0, newname, -1, 0, 0) ) == 0 ) )
    {
        _dosmaperr(GetLastError());
        return -1;
    }

    /* allocate enough space for both */
    if ( (widenamesbuffer = (wchar_t*)_malloc_crt( (oldnamelen+newnamelen) * sizeof(wchar_t) ) ) == NULL )
    {
        /* errno is set by _malloc */
        return -1;
    }
    
    /* now do the conversion */
    oldnamew = widenamesbuffer;
    newnamew = widenamesbuffer + oldnamelen;

    if (  ( MultiByteToWideChar(codePage, 0, oldname, -1, oldnamew, oldnamelen) == 0 )
        || ( MultiByteToWideChar(codePage, 0, newname, -1, newnamew, newnamelen) == 0 ) )
    {
        _free_crt(widenamesbuffer);
        _dosmaperr(GetLastError());
        return -1;
    }

    /* and event call the wide-character variant */
    retval = _wrename(oldnamew,newnamew);
    _free_crt(widenamesbuffer); /* _free_crt leaves errno alone if everything completes as expected */
    return retval;
}

#else /* _UNICODE */

int __cdecl _wrename (
        const wchar_t *oldname,
        const wchar_t *newname
        )
{
        ULONG dosretval;

#ifdef _WIN32_WCE
        if (!MoveFileW(oldname, newname))
#else        
        if (!MoveFileExW(oldname, newname, MOVEFILE_COPY_ALLOWED /* allow moving to a different volume */))
#endif // _WIN32_WCE
            dosretval = GetLastError();
        else
            dosretval = 0;

        if (dosretval) {
            /* error occured -- map error code and return */
            _dosmaperr(dosretval);
            return -1;
        }

        return 0;
}

#endif /* _UNICODE */
