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
*setenv.c -set an environment variable in the environment
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines __crtsetenv() - adds a new variable to environment.
*       Internal use only.
*
*Revision History:
*       11-30-93  CFW   Module created, most of it grabbed from putenv.c.
*       12-07-93  CFW   Change _TCHAR to _TSCHAR.
*       01-15-94  CFW   Use _tcsnicoll for global match.
*       01-28-94  CFW   Copy environment when re-alloc.
*       03-25-94  GJF   Declaration of __[w]initenv moved to internal.h.
*       01-10-95  CFW   Debug CRT allocs.
*       01-18-95  GJF   Must replace _tcsdup with _malloc_crt/_tcscpy for
*                       _DEBUG build.
*       06-01-95  CFW   Free strings for removed environemnt variables.
*       03-03-98  RKP   Add support for 64 bit
*       05-28-99  GJF   When appropriate, free up the option string.
*       08-03-99  PML   Fix use-after-free bug in __crtsetenv()
*       02-23-00  GB    Fix __crtwsetenv() so as to work on Win9x.
*       05-17-00  GB    Use ERROR_CALL_NOT_IMPLEMENTED for existance of W API
*       05-23-00  GB    return error (-1) for API returning error
*       05-03-02  GB    Shutting down prefast warining in realloc_crt calls.
*       05-06-02  MSL   Change __crt[w]setenv to return info as to whether it
*                       took ownership of a pointer; now we don't corrupt
*                       heap if ::SetEnvironmentVariable fails, for example
*                       VS7 527539
*       11-02-03  AC    Added validation.
*       03-13-04  MSL   Add internal logic check detected by prefast
*       03-18-04  AC    More parameter validation (name and value of getenv/putenv cannot
*                       be longer than _MAX_ENV)
*       10-31-04  AC    Prefast fixes
*                       VSW#373224
*       04-01-05  MSL   Integer overflow protection
*    03-27-06  AEI   VSW#585114. At the end of __crtsetenv, null out the incoming pointer 
*                       because the content was freed.
*
*******************************************************************************/

#include <windows.h>
#include <cruntime.h>
#include <internal.h>
#include <stdlib.h>
#include <tchar.h>
#include <rterr.h>
#include <dbgint.h>
#include <awint.h>
#include <limits.h>

static _TSCHAR **copy_environ(_TSCHAR **);

#ifdef  _UNICODE
static int __cdecl wfindenv(const wchar_t *name, int len);
#else
static int __cdecl findenv(const char *name, int len);
#endif

/***
*int __crtsetenv(option) - add/replace/remove variable in environment
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
*       TCHAR **poption - pointer to option string to set in the environment list.
*           should be of the form "option=value".
*           This function takes ownership of this pointer in the success case.
*       int primary - Only the primary call to _crt[w]setenv needs to
*           create new copies or set the OS environment.
*           1 indicates that this is the primary call.
*
*Exit:
*       returns 0 if OK, -1 if fails.
*       If *poption is non-null on exit, we did not free it, and the caller should
*       If *poption is null on exit, we did free it, and the caller should not.
*
*Exceptions:
*
*Warnings:
*       This code will not work if variables are removed from the environment
*       by deleting them from environ[].  Use _putenv("option=") to remove a 
*       variable.
*
*       The option argument will be taken ownership of by this code and may be freed!
*
*******************************************************************************/

#ifdef  _UNICODE
int __cdecl __crtwsetenv (
#else
int __cdecl __crtsetenv (
#endif
        _TSCHAR **poption,
        const int primary
        )
{
        int ix;
        int retval = 0;
        int remove; /* 1 if variable is to be removed */
        _TSCHAR *option=NULL;           /* The option string passed in */
        _TSCHAR **env;
        _TSCHAR *name, *value;
        const _TSCHAR *equal;

        /* Validate poption and dereference it */
        _VALIDATE_RETURN(poption != NULL, EINVAL, -1);

        option=*poption;

        /*
         * check that the option string is valid, find the equal sign
         * and verify '=' is not the first character in string.
         */
        if ( (option == NULL) || ((equal = _tcschr(option, _T('='))) == NULL)
            || option == equal)
        {
            errno = EINVAL;
            return -1;
        }

        /* internal consistency check: the environment string should never use buffers bigger than _MAX_ENV
         * see also SDK function SetEnvironmentVariable
         */
        _ASSERTE(equal - option < _MAX_ENV);
        _ASSERTE(_tcsnlen(equal + 1, _MAX_ENV) < _MAX_ENV);

        /* if the character following '=' is null, we are removing the
         * the environment variable. Otherwise, we are adding or updating
         * an environment variable.
         */
        remove = (*(equal + 1) == _T('\0'));

        /*
         * the first time _[w]putenv() is called, copy the environment
         * block that was passed to [w]main to avoid making a
         * dangling pointer if the block is re-alloced.
         */
#ifdef  _UNICODE
        if (_wenviron == __winitenv)
            _wenviron = copy_environ(_wenviron);
#else
        if (_environ == __initenv)
            _environ = copy_environ(_environ);
#endif

        /* see if requested environment array exists */
        if (_tenviron == NULL) {

            /*
             * The requested type of environment does not exist.
             * See if other type exists, if so convert it to requested type.
             * The functions that convert the enviroment (__mbtow_environ and
             * __wtomb_environ) will call this function (__crt[w]setenv) once
             * for each of the pre-existing environment variables. To avoid
             * an infinite loop, test the primary flag.
             */

#ifdef  _UNICODE
            if (primary && _environ)
            {
                _wenvptr = __crtGetEnvironmentStringsW();
                if ( _wsetenvp() < 0 )
                    if (__mbtow_environ() != 0)
                    {
                        errno = EINVAL;
                        return -1;
                    }
            }
#else
            if (primary && _wenviron)
            {
                if (__wtomb_environ() != 0)
                {
                    errno = EINVAL;
                    return -1;
                }
            }
#endif
            else {
                /* nothing to remove, return */
                if ( remove )
                    return 0;
                else {
                    /* create ones that do not exist */

                    if (_environ == NULL)
                    {
                        if ( (_environ = _malloc_crt(sizeof(char *))) == NULL)
                            return -1;
                        *_environ = NULL;
                    }

                    if (_wenviron == NULL)
                    {
                        if ( (_wenviron = _malloc_crt(sizeof(wchar_t *))) == NULL)
                            return -1;
                        *_wenviron = NULL;
                    }
                }
            }
        }

        /*
         * At this point, the two types of environments are in sync (as much
         * as they can be anyway). The only way they can get out of sync
         * (besides users directly modifiying the environment) is if there
         * are conversion problems: If the user sets two Unicode EVs,
         * "foo1" and "foo2" and converting then to multibyte yields "foo?"
         * and "foo?", then the environment blocks will differ.
         */

        /* init env pointers */
        env = _tenviron;
        if(!env)
        {
            _ASSERTE(("CRT Logic error during setenv",0));
            return -1;
        }

        /* See if the string is already in the environment */
#ifdef  _UNICODE
        ix = wfindenv(option, (int)(equal - option));
#else
        ix = findenv(option, (int)(equal - option));
#endif

        if ((ix >= 0) && (*env != NULL)) {
            /* 
             * String is already in the environment. Free up the original
             * string. Then, install the new string or shrink the environment,
             * whichever is warranted.
             */
            _free_crt(env[ix]);

            if (remove) {

                /* removing -- move all the later strings up */
                for ( ; env[ix] != NULL; ++ix) {
                    env[ix] = env[ix+1];
                }

                /* shrink the environment memory block
                   (ix now has number of strings, including NULL) --
                   this realloc probably can't fail, since we're
                   shrinking a mem block, but we're careful anyway. */
                if (ix < (SIZE_MAX / sizeof(_TSCHAR *)) &&
                    (env = (_TSCHAR **) _recalloc_crt(_tenviron, ix, sizeof(_TSCHAR *))) != NULL)
                    _tenviron = env;
            }
            else {
                /* replace the option */
                env[ix] = (_TSCHAR *) option;

                /* we now own the pointer, so NULL out the incoming pointer */
                *poption=NULL;
            }
        }
        else {
            /*
             * String is NOT in the environment
             */
            if ( !remove )  {
                /*
                 * Append the string to the environ table. Note that
                 * table must be grown to do this.
                 */
                if (ix < 0)
                    ix = -ix;    /* ix = length of environ table */

                if ((ix + 2) < ix ||
                    (ix + 2) >= (SIZE_MAX / sizeof(_TSCHAR *)) ||
                    (env = (_TSCHAR **)_recalloc_crt(_tenviron, sizeof(_TSCHAR *), (ix + 2))) == NULL)
                    return -1;

                env[ix] = (_TSCHAR *)option;
                env[ix + 1] = NULL;

                /* we now own the pointer, so NULL out the incoming pointer */
                *poption=NULL;

                _tenviron = env;
            }
            else {
                /*
                 * We are asked to remove an environment var that isn't there.
                 * Free the option string and return success.
                 */
                _free_crt(option);

                /* we now freed the pointer, so NULL out the incoming pointer */
                *poption=NULL;

                return 0;
            }
        }

        /*
         * Update the OS environment. Don't give an error if this fails
         * since the failure will not affect the user unless he/she is making
         * direct API calls. Only need to do this for one type, OS converts
         * to other type automatically.
         */
        if ( primary &&
            (name = (_TSCHAR *)_calloc_crt((_tcslen(option) + 2), sizeof(_TSCHAR))) != NULL )
        {
            _ERRCHECK(_tcscpy_s(name, _tcslen(option) + 2, option));
            value = name + (equal - option);
            *value++ = _T('\0');
#ifdef _WIN32_WCE // CE doesn't support SetEnvironmentVariable
            retval = -1; 
#else
#ifdef _UNICODE
            if ( SetEnvironmentVariableW(name, remove ? NULL : value) == 0)
            {
                retval = -1;
            }
#else
            if (SetEnvironmentVariable(name, remove ? NULL : value) == 0)
                retval = -1;
#endif
#endif // _WIN32_WCE
            if (retval == -1)
            {
                errno = EILSEQ;
            }

            _free_crt(name);
        }

        if (remove) {
            /* free option string since it won't be used anymore */
            _free_crt(option);
            *poption = NULL;
        }

        return retval;
}


/***
*int findenv(name, len) - [STATIC]
*
*Purpose:
*       Scan for the given string within the environment
*
*Entry:
*
*Exit:
*       Returns the offset in "environ[]" of the given variable
*       Returns the negative of the length of environ[] if not found.
*       Returns 0 if the environment is empty.
*
*       [NOTE: That a 0 return can mean that the environment is empty
*       or that the string was found as the first entry in the array.]
*
*Exceptions:
*
*******************************************************************************/

#ifdef  _UNICODE
static int __cdecl wfindenv (
#else
static int __cdecl findenv (
#endif
        const _TSCHAR *name,
        int len
        )
{
        _TSCHAR **env;

        for ( env = _tenviron ; *env != NULL ; env++ ) {
            /*
             * See if first len characters match, up to case
             */
            if ( _tcsnicoll(name, *env, len) == 0 )
                /*
                 * the next character of the environment string must
                 * be an '=' or a '\0'
                 */
                if ( (*env)[len] == _T('=') || (*env)[len] == _T('\0') )
                    return(int)(env - _tenviron);
//
// We cannot break here since findenv must report the total number of strings.
//              else
//                  break;
        }

        return(-(int)(env - _tenviron));
}


/***
*copy_environ - copy an environment block
*
*Purpose:
*       Create a copy of an environment block.
*
*Entry:
*       _TSCHAR **oldenviron - pointer to enviroment to be copied.
*
*Exit:
*       Returns a pointer to newly created environment.
*
*Exceptions:
*
*******************************************************************************/

static _TSCHAR **copy_environ(_TSCHAR **oldenviron)
{
        int cvars = 0;
        _TSCHAR **oldenvptr = oldenviron;
        _TSCHAR **newenviron, **newenvptr;

        /* no environment */
        if (oldenviron == NULL)
            return NULL;

        /* count number of environment variables */
        while (*oldenvptr++)
            cvars++;

        /* need pointer for each string, plus one null ptr at end */
        if ( (newenviron = newenvptr = (_TSCHAR **)
            _calloc_crt((cvars+1), sizeof(_TSCHAR *))) == NULL )
            _amsg_exit(_RT_SPACEENV);
#ifdef _WIN32_WCE        
        PREFAST_ASSERT(newenvptr);
#endif
        /* duplicate the environment variable strings */
        oldenvptr = oldenviron;
        while (*oldenvptr)
#ifdef  _DEBUG
        {
            size_t envptrSize = _tcslen(*oldenvptr) + 1;
            if ( (*newenvptr = _calloc_crt(envptrSize, sizeof(_TSCHAR))) != NULL )
                _ERRCHECK(_tcscpy_s(*newenvptr, envptrSize, *oldenvptr));
            oldenvptr++;
            newenvptr++;
        }
#else   /* ndef _DEBUG */
            *newenvptr++ = _tcsdup(*oldenvptr++);
#endif  /* _DEBUG */

        *newenvptr = NULL;

        return newenviron;
}
