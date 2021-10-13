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
*chdir.c - change directory
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This file has the _chdir() function - change current directory.
*
*Revision History:
*       06-06-89  PHG   Module created, based on asm version
*       03-07-90  GJF   Made calling type _CALLTYPE2 (for now), added #include
*                       <cruntime.h>, fixed copyright and fixed compiler
*                       warnings. Also, cleaned up the formatting a bit.
*       03-30-90  GJF   Now _CALLTYPE1.
*       07-24-90  SBM   Removed '32' from API names
*       09-27-90  GJF   New-style function declarator.
*       12-04-90  SRW   Changed to include <oscalls.h> instead of <doscalls.h>
*       12-06-90  SRW   Added _CRUISER_ and _WIN32 conditionals.
*       01-16-91  GJF   ANSI naming.
*       05-19-92  GJF   Revised to support the 'current directory' environment
*                       variables of Win32/NT.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       11-24-93  CFW   Rip out Cruiser.
*       11-24-93  CFW   No longer store current drive in CRT env strings.
*       12-01-93  CFW   Set OS drive letter variables.
*       12-07-93  CFW   Wide char enable.
*       01-25-95  GJF   New current directory can be a UNC path!
*       02-08-95  JWM   Spliced _WIN32 & Mac versions.
*       07-01-96  GJF   Replaced defined(_WIN32) with !defined(_MAC). Also,
*                       detab-ed and cleaned up the format a bit.
*       05-17-99  PML   Remove all Macintosh support.
*       04-26-02  GB    fixed bug if path is greater then MAX_PATH, i.e. "\\?\"
*                       prepended to path.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-01-03  SJ    VSW#175081 - Incorrect Return Value
*       04-01-05  MSL   Integer overflow fix during allocation 
*
*******************************************************************************/

#include <cruntime.h>
#include <oscalls.h>
#include <mtdll.h>
#include <internal.h>
#include <direct.h>
#include <stdlib.h>
#include <tchar.h>
#include <malloc.h>
#include <dbgint.h>

/***
*int _chdir(path) - change current directory
*
*Purpose:
*       Changes the current working directory to that given in path.
*
*Entry:
*       _TSCHAR *path - directory to change to
*
*Exit:
*       returns 0 if successful,
*       returns -1 and sets errno if unsuccessful
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _tchdir (
        const _TSCHAR *path
        )
{
#ifdef _WIN32_WCE
        _dosmaperr(ERROR_NOT_SUPPORTED);
        return -1;  
#else
        _TSCHAR env_var[4];
        _TSCHAR abspath[MAX_PATH+1];
        _TSCHAR *apath=abspath;
        int memfree=0;
        int r;
        int retval = -1;

        _VALIDATE_CLEAR_OSSERR_RETURN((path != NULL), EINVAL, -1);
        
        if ( SetCurrentDirectory((LPTSTR)path) )
        {
            /*
             * If the new current directory path is NOT a UNC path, we must
             * update the OS environment variable specifying the current
             * directory for what is now current drive. To do this, get the
             * full current directory, build the environment variable string
             * and call SetEnvironmentVariable(). We need to do this because
             * SetCurrentDirectory does not (i.e., does not update the
             * current-directory-on-drive environment variables) and other
             * functions (fullpath, spawn, etc) need them to be set.
             *
             * If associated with a 'drive', the current directory should
             * have the form of the example below:
             *
             *  D:\nt\private\mytests
             *
             * so that the environment variable should be of the form:
             *
             *  =D:=D:\nt\private\mytests
             *
             */
            r = GetCurrentDirectory(MAX_PATH+1,(LPTSTR)apath);
            if (r > MAX_PATH) {
                if ((apath = (_TSCHAR *)_calloc_crt(r+1, sizeof(_TSCHAR))) == NULL) {
                    r = 0;
                } else {
                    memfree = 1;
                }

                if (r)
                    r = GetCurrentDirectory(r+1,(LPTSTR)apath);
            }
            if (r)
            {
                /*
                 * check if it is a UNC name, just return if is
                 */
                if ( ((apath[0] == _T('\\')) || (apath[0] == _T('/'))) &&
                     (apath[0] == apath[1]) )
                    retval = 0;
                else {

                    env_var[0] = _T('=');
                    env_var[1] = (_TSCHAR) _totupper((_TUCHAR)apath[0]);
                    env_var[2] = _T(':');
                    env_var[3] = _T('\0');

                    if ( SetEnvironmentVariable(env_var, apath) )
                        retval = 0;
                }
            }
        }
        
        if( 0 != retval )
        {
            /* error occured -- map error code */
            _dosmaperr( GetLastError() );
        }

        if (memfree)
            _free_crt(apath);
        return retval;
#endif // _WIN32_WCE
}
