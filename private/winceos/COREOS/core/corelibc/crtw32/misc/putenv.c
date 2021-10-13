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
*putenv.c - put an environment variable into the environment
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines _putenv() - adds a new variable to environment; does not
*       change global environment, only the process' environment.
*
*Revision History:
*       08-08-84  RN    initial version
*       02-23-88  SKS   check for environment containing only the NULL string
*       05-31-88  PHG   Merged DLL and normal versions
*       07-14-88  JCR   Much simplified since (1) __setenvp always uses heap, and
*                       (2) envp array and env strings are in seperate heap blocks
*       07-03-89  PHG   Now "option=" string removes string from environment
*       08-17-89  GJF   Removed _NEAR_, _LOAD_DS and fixed indents.
*       09-14-89  KRS   Don't give error if 'option' not defined in "option=".
*       11-20-89  GJF   Added const to arg type. Also, fixed copyright.
*       03-15-90  GJF   Made the calling type _CALLTYPE1, added #include
*                       <cruntime.h> and removed #include <register.h>.
*       04-05-90  GJF   Made findenv() _CALLTYPE4.
*       04-26-90  JCR   Update if environ is NULL (stubbed out _setenvp)
*       07-25-90  SBM   Removed redundant include (stdio.h)
*       10-04-90  GJF   New-style function declarators.
*       01-21-91  GJF   ANSI naming.
*       02-06-91  SRW   Added _WIN32_ conditional for SetEnvironmentVariable
*       02-18-91  SRW   Changed _WIN32_ conditional for SetEnvironmentVariable
*                       to be in addition to old logic instead of replacement
*       04-23-92  GJF   Made findenv insensitive to the case of name for Win32.
*                       Also added support for 'current drive' environment
*                       strings in Win32.
*       04-29-92  GJF   Repackaged so that __putenv_lk could be easily added for
*                       for Win32.
*       05-05-92  DJM   POSIX not supported.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       06-05-92  PLM   Added _MAC_ 
*       11-24-93  CFW   Rip out Cruiser, disallow "=C:=C:\foo" format putenvs.
*       11-29-93  CFW   Wide char enable, convert between wide and narrow
*                       types. Mucho code moved to setenv.c
*       12-07-93  CFW   Change _TCHAR to _TSCHAR.
*       01-15-94  CFW   Use _tcsnicoll for global match.
*       09-06-94  CFW   Replace MTHREAD with _MT.
*       01-10-95  CFW   Debug CRT allocs.
*       02-14-95  CFW   Debug CRT allocs for Mac version.
*       02-16-95  JWM   Spliced _WIN32 & Mac versions.
*       06-01-95  CFW   Copy environment string before passing to _crt[w]setenv.
*       07-09-97  GJF   Added a check that the environment initialization has
*                       been executed. Also, cleaned up the format a bit and 
*                       got rid of obsolete _CALLTYPE* macros.
*       03-03-98  RKP   Added 64 bit support.
*       03-05-98  GJF   Exception-safe locking.
*       08-28-98  GJF   Use CP_ACP instead of CP_OEMCP.
*       05-17-99  PML   Remove all Macintosh support.
*       05-25-99  GJF   Free up buffers allocated to hold env string when there
*                       there is a failure.
*       05-09-02  MSL   Fixed to free memory correctly during failures of
*                       __crt[w]setenv VS7 527539
*       10-29-03  AC    Added secure version.
*       01-15-04  SJ    Added check for NULL in putenv - VSW#218293
*       03-10-04  AC    More parameter validation (name and value of getenv/putenv cannot
*                       be longer than _MAX_ENV)
*       04-01-05  MSL   Integer overflow protection
*
*******************************************************************************/

#include <windows.h>
#include <cruntime.h>
#include <internal.h>
#include <mtdll.h>
#include <stdlib.h>
#include <string.h>
#include <tchar.h>
#include <dbgint.h>

#ifndef CRTDLL

/*
 * Flag checked by getenv() and _putenv() to determine if the environment has
 * been initialized.
 */
extern int __env_initialized;

#endif

int __cdecl _wputenv_helper(const wchar_t *, const wchar_t *);
int __cdecl _putenv_helper(const char *, const char *);

#ifdef _UNICODE
#define _tputenv_helper _wputenv_helper
#else
#define _tputenv_helper _putenv_helper
#endif

/***
*int _putenv(option) - add/replace/remove variable in environment
*
*Purpose:
*       option should be of the form "option=value".  If a string with the
*       given option part already exists, it is replaced with the given
*       string; otherwise the given string is added to the environment.
*       If the string is of the form "option=", then the string is
*       removed from the environment, if it exists.  If the string has
*       no equals sign, error is returned.
*
*Entry:
*       char *option - option string to set in the environment list.
*           should be of the form "option=value".
*
*Exit:
*       returns 0 if OK, -1 if fails.
*
*Exceptions:
*
*Warning:
*       This code will not work if variables are removed from the
*       environment by deleting them from environ[].  Use _putenv("option=")
*       to remove a variable.
*
*******************************************************************************/

int __cdecl _tputenv (
        const _TSCHAR *option
        )
{
        int retval;

        _mlock( _ENV_LOCK );

        __try {
            retval = _tputenv_helper(option, NULL);
        }
        __finally {
            _munlock( _ENV_LOCK );
        }

        return retval;
}


errno_t _tputenv_s (
        const _TSCHAR *name,
        const _TSCHAR *value
        )
{
        int retval;

        /* validation section */
        _VALIDATE_RETURN_ERRCODE(value != NULL, EINVAL);

        _mlock( _ENV_LOCK );

        __try {
            retval = ((_tputenv_helper(name, value) == 0) ? 0 : errno);
        }
        __finally {
            _munlock( _ENV_LOCK );
        }

        return retval;
}

static int __cdecl _tputenv_helper (
        const _TSCHAR *name,
        const _TSCHAR *value
        )
{
        int size;
        _TSCHAR * newoption = NULL;
        size_t newoption_size = 0;

#ifndef CRTDLL
        /*
         * Make sure the environment is initialized.
         */
        if  ( !__env_initialized )
            return -1;
#endif  /* CRTDLL */

        /*
         * At startup, we obtain the 'native' flavor of environment strings
         * from the OS. So a "main" program has _environ and a "wmain" has
         * _wenviron loaded at startup. Only when the user gets or puts the
         * 'other' flavor do we convert it.
         */

        _VALIDATE_RETURN(name != NULL, EINVAL, -1);

        /* copy the new environent string */
        if (value == NULL)
        {
            const _TSCHAR *equal = _tcschr(name, _T('='));

            if (equal != NULL)
            {
                /* validate the length of both name and value */
                _VALIDATE_RETURN(equal - name < _MAX_ENV, EINVAL, -1);
                _VALIDATE_RETURN(_tcsnlen(equal + 1, _MAX_ENV) < _MAX_ENV, EINVAL, -1);
            }

            /* the string is already complete in name */
            newoption_size = _tcslen(name) + 1;
            if ((newoption = (_TSCHAR *)_calloc_crt(newoption_size, sizeof(_TSCHAR))) == NULL)
            {
                return -1;
            }

            _tcscpy_s(newoption, newoption_size, name);
        }
        else
        {
            size_t namelen = _tcsnlen(name, _MAX_ENV);
            size_t valuelen = _tcsnlen(value, _MAX_ENV);

            /* validate the length of both name and value */
            _VALIDATE_RETURN(namelen < _MAX_ENV, EINVAL, -1);
            _VALIDATE_RETURN(valuelen < _MAX_ENV, EINVAL, -1);

            /* we assemble the string from name and value (we assume _tcslen("=") == 1) */
            newoption_size = namelen + 1 + valuelen + 1;
            if ((newoption = (_TSCHAR *)_calloc_crt(newoption_size, sizeof(_TSCHAR))) == NULL)
            {
                return -1;
            }

            _tcscpy_s(newoption, newoption_size, name);
            newoption[namelen++] = _T('=');
            _tcscpy_s(newoption + namelen, newoption_size - namelen, value);
        }

#ifdef  _UNICODE
        if ( __crtwsetenv(&newoption, 1) != 0 )
        {
            /* if the set failed, we will free the option only if it was not consumed */
            if(newoption)
            {
                _free_crt(newoption);
                newoption=NULL;
            }
            return -1;
        }

        /* If other environment type exists, set it */
        if (_environ)
        {
            char *mboption = NULL;
            int temp_size = 0;

            /* find out how much space is needed */
            if ( (size = WideCharToMultiByte(CP_ACP, 0, name, -1, NULL,
                 0, NULL, NULL)) == 0 )
            {
                errno = EILSEQ;
                return -1;
            }

            if (value != NULL)
            {
                /* account for the '=' */
                size += 1;  

                if ( (temp_size = WideCharToMultiByte(CP_ACP, 0, value, -1, NULL,
                    0, NULL, NULL)) == 0 )
                {
                    errno = EILSEQ;
                    return -1;
                }
                size += temp_size;
            }

            /* allocate space for variable */
            if ((mboption = (char *) _calloc_crt(size, sizeof(char))) == NULL)
                return -1;

            /* convert it */
            if ( WideCharToMultiByte(CP_ACP, 0, name, -1, mboption, size,
                 NULL, NULL) == 0 )
            {
                _free_crt(mboption);
                errno = EILSEQ;
                return -1;
            }

            if (value != NULL)
            {
                size_t len = strlen(mboption);
                mboption[len++] = '=';

                if ( WideCharToMultiByte(
                        CP_ACP, 
                        0, 
                        value, 
                        -1, 
                        mboption + len, 
                        size - (int)len,
                        NULL, 
                        NULL) == 0 )
                {
                    _free_crt(mboption);
                    errno = EILSEQ;
                    return -1;
                }
            }

            /* set it - this is not primary call, so set primary == 0 */
            if ( __crtsetenv(&mboption, 0) != 0 )
            {
                /* if the set failed, we will free the option only if it was not consumed */
                if(mboption)
                {
                    _free_crt(mboption);
                    mboption=NULL;
                }
                return -1;
            }
        }
#else
        /* Set requested environment type, primary call */
        if ( __crtsetenv(&newoption, 1) != 0 )
        {
            /* if the set failed, we will free the option only if it was not consumed */
            if(newoption)
            {
                _free_crt(newoption);
                newoption=NULL;
            }
            return -1;
        }

        /* If other environment type exists, set it */
        if (_wenviron)
        {
            wchar_t *woption = NULL;
            int temp_size = 0;

            /* find out how much space is needed */
            if ( (size = MultiByteToWideChar(CP_ACP, 0, name, -1, NULL, 0))
                 == 0 )
            {
                errno = EILSEQ;
                return -1;
            }

            if (value != NULL)
            {
                /* account for the '=' */
                size += 1;

                if ( (temp_size = MultiByteToWideChar(CP_ACP, 0, value, -1, NULL, 0))
                    == 0 )
                {
                    errno = EILSEQ;
                    return -1;
                }
                size += temp_size;
            }

            /* allocate space for variable */
            if ( (woption = (wchar_t *) _calloc_crt(size, sizeof(wchar_t)))
                 == NULL )
                return -1;

            /* convert it */
            if ( MultiByteToWideChar(CP_ACP, 0, name, -1, woption, size)
                 == 0 )
            {
                _free_crt(woption);
                errno = EILSEQ;
                return -1;
            }

            if (value != NULL)
            {
                size_t len = wcslen(woption);
                woption[len++] = L'=';

                if ( MultiByteToWideChar(
                        CP_ACP, 
                        0, 
                        value, 
                        -1, 
                        woption + len, 
                        size - (int)len) == 0 )
                {
                    _free_crt(woption);
                    errno = EILSEQ;
                    return -1;
                }
            }

            /* set it - this is not primary call, so set primary == 0 */
            if ( __crtwsetenv(&woption, 0) != 0 )
            {
                /* if the set failed, we will free the option only if it was not consumed */
                if(woption)
                {
                    _free_crt(woption);
                    woption = NULL;
                }
                return -1;
            }
        }
#endif

        return 0;
}
