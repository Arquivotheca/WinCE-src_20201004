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
*getenv.c - get the value of an environment variable
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines getenv() - searches the environment for a string variable
*       and returns the value of it.
*
*Revision History:
*       11-22-83  RN    initial version
*       04-13-87  JCR   added const to declaration
*       11-09-87  SKS   avoid indexing past end of strings (add strlen check)
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       06-01-88  PHG   Merged normal/DLL versions
*       03-14-90  GJF   Replaced _LOAD_DS with _CALLTYPE1, added #include
*                       <cruntime.h>, removed #include <register.h> and fixed
*                       the copyright. Also, cleaned up the formatting a bit.
*       04-05-90  GJF   Added #include <string.h>.
*       07-25-90  SBM   Removed redundant include (stdio.h)
*       08-13-90  SBM   Compiles cleanly with -W3 (made length unsigned int)
*       10-04-90  GJF   New-style function declarator.
*       01-18-91  GJF   ANSI naming.
*       02-06-91  SRW   Added _WIN32_ conditional for GetEnvironmentVariable
*       02-18-91  SRW   Removed _WIN32_ conditional for GetEnvironmentVariable
*       01-10-92  GJF   Final unlock and return statements shouldn't be in
*                       if-block.
*       03-11-92  GJF   Use case-insensitive comparison for Win32.
*       04-27-92  GJF   Repackaged MTHREAD support for Win32 to create a
*                       _getenv_nolock.
*       06-05-92  PLM   Added _MAC_ 
*       06-10-92  PLM   Added _envinit for _MAC_ 
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*                       Remove OS/2, POSIX support
*       04-08-93  SKS   Replace strnicmp() with ANSI-conforming _strnicmp()
*       09-14-93  GJF   Small change for Posix compatibility.
*       11-29-93  CFW   Wide char enable.
*       12-07-93  CFW   Change _TCHAR to _TSCHAR.
*       01-15-94  CFW   Use _tcsnicoll for global match.
*       02-04-94  CFW   POSIXify.
*       03-31-94  CFW   Should be ifndef POSIX.
*       02-14-95  CFW   Debug CRT allocs.
*       02-16-95  JWM   Spliced _WIN32 & Mac versions.
*       08-01-96  RDK   For Pmac, change data type of initialization pointer.
*       07-09-97  GJF   Added a check that the environment initialization has
*                       been executed. Also, detab-ed and got rid of obsolete
*                       _CALLTYPE1 macros.
*       03-05-98  GJF   Exception-safe locking.
*       12-18-98  GJF   Changes for 64-bit size_t.
*       04-28-99  PML   Wrap __declspec(allocate()) in _CRTALLOC macro.
*       05-17-99  PML   Remove all Macintosh support.
*       11-02-03  AC    Added secure version.
*       02-03-04  SJ    VSW#228728 - validate param to getenv
*       03-10-04  AC    More parameter validation (name and value of getenv/putenv cannot
*                       be longer than _MAX_ENV)
*       09-08-04  AC    Added _dupenv_s family.
*                       VSW#355011
*       10-30-04  AC    Validate varname in _dupenv_s family.
*       04-01-05  MSL   Integer overflow protection
*
*******************************************************************************/

#include <sect_attribs.h>
#include <cruntime.h>
#include <internal.h>
#include <mtdll.h>
#include <stdlib.h>
#include <string.h>
#include <tchar.h>
#include <awint.h>

#ifndef CRTDLL

/*
 * Flag checked by getenv() and _putenv() to determine if the environment has
 * been initialized.
 */
extern int __env_initialized;

#endif

errno_t __cdecl _wgetenv_s_helper(size_t *pReturnValue, wchar_t *buffer, size_t sizeInWords, const wchar_t *varname);
errno_t __cdecl _getenv_s_helper(size_t *pReturnValue, char *buffer, size_t sizeInBytes, const char *varname);

#ifdef _DEBUG
errno_t __cdecl _wdupenv_s_helper(wchar_t **pBuffer, size_t *pBufferSizeInWords, const wchar_t *varname,
    int nBlockUse, const char *szFileName, int nLine);
errno_t __cdecl _dupenv_s_helper(char **pBuffer, size_t *pBufferSizeInWords, const char *varname,
    int nBlockUse, const char *szFileName, int nLine);
#else
errno_t __cdecl _wdupenv_s_helper(wchar_t **pBuffer, size_t *pBufferSizeInWords, const wchar_t *varname);
errno_t __cdecl _dupenv_s_helper(char **pBuffer, size_t *pBufferSizeInWords, const char *varname);
#endif

#ifdef _UNICODE
#define _tgetenv_helper_nolock _wgetenv_helper_nolock
#define _tgetenv_s_helper _wgetenv_s_helper
#define _tdupenv_s_helper _wdupenv_s_helper
#else
#define _tgetenv_helper_nolock _getenv_helper_nolock
#define _tgetenv_s_helper _getenv_s_helper
#define _tdupenv_s_helper _dupenv_s_helper
#endif

/***
*char *getenv(option) - search environment for a string
*
*Purpose:
*       searches the environment for a string of the form "option=value",
*       if found, return value, otherwise NULL.
*
*Entry:
*       const char *option - variable to search for in environment
*
*Exit:
*       returns the value part of the environment string if found,
*       otherwise NULL
*
*Exceptions:
*
*******************************************************************************/

_TSCHAR * __cdecl _tgetenv (
        const _TSCHAR *option
        )
{
        _TSCHAR *retval;

        _VALIDATE_RETURN( (option != NULL), EINVAL, NULL);
        _VALIDATE_RETURN( (_tcsnlen(option, _MAX_ENV) < _MAX_ENV), EINVAL, NULL);

        _mlock( _ENV_LOCK );
        __try {
            retval = (_TSCHAR *)_tgetenv_helper_nolock(option);
        }
        __finally {
            _munlock( _ENV_LOCK );
        }

        return(retval);
}

const _TSCHAR * __cdecl _tgetenv_helper_nolock (
        const _TSCHAR *option
        )
{
#ifdef  _POSIX_
        char **search = environ;
#else
        _TSCHAR **search = _tenviron;
#endif
        size_t length;

#ifndef CRTDLL
        /*
         * Make sure the environment is initialized.
         */
        if ( !__env_initialized ) 
            return NULL;
#endif  /* CRTDLL */

        /*
         * At startup, we obtain the 'native' flavor of environment strings
         * from the OS. So a "main" program has _environ and a "wmain" has
         * _wenviron loaded at startup. Only when the user gets or puts the
         * 'other' flavor do we convert it.
         */

#ifndef _POSIX_

#ifdef  _UNICODE
        if (!search && _environ)
        {
            /* don't have requested type, but other exists, so convert it */
            _wenvptr = __crtGetEnvironmentStringsW();
            if ( _wsetenvp() < 0 )
                if (__mbtow_environ() != 0)
                return NULL;

            /* now requested type exists */
            search = _wenviron;
        }
#else
        if (!search && _wenviron)
        {
            /* don't have requested type, but other exists, so convert it */
            if (__wtomb_environ() != 0)
                return NULL;

            /* now requested type exists */
            search = _environ;
        }
#endif

#endif  /* _POSIX_ */

        if (search && option)
        {
                length = _tcslen(option);

                /*
                ** Make sure `*search' is long enough to be a candidate
                ** (We must NOT index past the '\0' at the end of `*search'!)
                ** and that it has an equal sign ('=') in the correct spot.
                ** If both of these requirements are met, compare the strings.
                */
                while (*search)
                {
                        if (_tcslen(*search) > length && 
                            (*(*search + length) == _T('=')) && 
                            (_tcsnicoll(*search, option, length) == 0))
                        {
                                /* internal consistency check: the environment string should never use a buffer bigger than _MAX_ENV
                                 * see also SDK function SetEnvironmentVariable
                                 */
                                _ASSERTE(_tcsnlen(*search + length + 1, _MAX_ENV) < _MAX_ENV);

                                return(*search + length + 1);
                        }

                        search++;
                }
        }

        return NULL;
}

/***
*errno_t getenv_s(pReturnValue, buffer, size, option) - search environment for a string
*
*Purpose:
*       searches the environment for a string of the form "option=value",
*       if found, copies the value in buffer.
*
*Entry:
*       size_t *pReturnValue - indicates if the variable has been found and
*           size needed
*       char *buffer - destination string
*       size_t sizeInChars - size of the destination buffer
*       const char *varname - variable to search for in environment
*
*Exit:
*       return the error code
*
*Exceptions:
*
*******************************************************************************/

errno_t __cdecl _tgetenv_s (
        size_t *pReturnValue,
        _TSCHAR *buffer,
        size_t sizeInTChars,
        const _TSCHAR *varname
        )
{
        errno_t retval;

        _mlock( _ENV_LOCK );
        __try {
            retval = _tgetenv_s_helper(pReturnValue, buffer, sizeInTChars, varname);
        }
        __finally {
            _munlock( _ENV_LOCK );
        }

        return(retval);
}

static
__forceinline
errno_t __cdecl _tgetenv_s_helper (
        size_t *pReturnValue,
        _TSCHAR *buffer,
        size_t sizeInTChars,
        const _TSCHAR *varname
        )
{
    const _TSCHAR *str;

    /* validation section */
    _VALIDATE_RETURN_ERRCODE(pReturnValue != NULL, EINVAL);
    *pReturnValue = 0;
    _VALIDATE_RETURN_ERRCODE((buffer != NULL && sizeInTChars > 0) || (buffer == NULL && sizeInTChars == 0), EINVAL);
    if (buffer != NULL)
    {
        *buffer = '\0';
    }
    /* varname is already validated in _tgetenv_helper_nolock */

    str = _tgetenv_helper_nolock(varname);
    if (str == NULL)
    {
        return 0;
    }

    *pReturnValue = _tcslen(str) + 1;
    if (sizeInTChars == 0)
    {
        /* we just return the size of the needed buffer */
        return 0;
    }

    if (*pReturnValue > sizeInTChars)
    {
        /* the buffer is too small: we return EINVAL, and we give the user another chance to
         * call getenv_s with a bigger buffer
         */
        return ERANGE;
    }
    
    _ERRCHECK(_tcscpy_s(buffer, sizeInTChars, str));

    return 0;
}

/***
*errno_t _dupenv_s(pBuffer, pBufferSize, varname) - search environment for a string
*
*Purpose:
*       searches the environment for a string of the form "option=value",
*       if found, copies the value in buffer.
*
*Entry:
*       char **pBuffer - pointer to a buffer which will be allocated
*       size_t *pBufferSize - pointer to the size of the buffer (optional)
*       const char *varname - variable to search for in environment
*
*Exit:
*       return the error code
*
*Exceptions:
*
*******************************************************************************/

#ifdef _DEBUG
errno_t __cdecl _tdupenv_s (
        _TSCHAR **pBuffer,
        size_t *pBufferSizeInTChars,
        const _TSCHAR *varname
        )
{
    return _tdupenv_s_dbg(pBuffer, pBufferSizeInTChars, varname, _NORMAL_BLOCK, NULL, 0);
}

errno_t __cdecl _tdupenv_s_dbg (
        _TSCHAR **pBuffer,
        size_t *pBufferSizeInTChars,
        const _TSCHAR *varname,
        int nBlockUse,
        const char * szFileName,
        int nLine
        )
#else
errno_t __cdecl _tdupenv_s (
        _TSCHAR **pBuffer,
        size_t *pBufferSizeInTChars,
        const _TSCHAR *varname
        )
#endif
{
        errno_t retval;

        _mlock( _ENV_LOCK );
        __try {
#ifdef _DEBUG
            retval = _tdupenv_s_helper(pBuffer, pBufferSizeInTChars, varname, nBlockUse, szFileName, nLine);
#else
            retval = _tdupenv_s_helper(pBuffer, pBufferSizeInTChars, varname);
#endif
        }
        __finally {
            _munlock( _ENV_LOCK );
        }

        return retval;
}

static
__forceinline
#ifdef _DEBUG
errno_t __cdecl _tdupenv_s_helper (
        _TSCHAR **pBuffer,
        size_t *pBufferSizeInTChars,
        const _TSCHAR *varname,
        int nBlockUse,
        const char * szFileName,
        int nLine
        )
#else
errno_t __cdecl _tdupenv_s_helper (
        _TSCHAR **pBuffer,
        size_t *pBufferSizeInTChars,
        const _TSCHAR *varname
        )
#endif
{
    const _TSCHAR *str;
    size_t size;

    /* validation section */
    _VALIDATE_RETURN_ERRCODE(pBuffer != NULL, EINVAL);
    *pBuffer = NULL;
    if (pBufferSizeInTChars != NULL)
    {
        *pBufferSizeInTChars = 0;
    }
    _VALIDATE_RETURN_ERRCODE(varname != NULL, EINVAL);

    str = _tgetenv_helper_nolock(varname);
    if (str == NULL)
    {
        return 0;
    }

    size = _tcslen(str) + 1;
#ifdef _DEBUG
    *pBuffer = (_TSCHAR*)_calloc_dbg(size, sizeof(_TSCHAR), nBlockUse, szFileName, nLine);
#else
    *pBuffer = (_TSCHAR*)calloc(size, sizeof(_TSCHAR));
#endif
    if (*pBuffer == NULL)
    {
        errno = ENOMEM;
        return errno;
    }
    
    _ERRCHECK(_tcscpy_s(*pBuffer, size, str));
    if (pBufferSizeInTChars != NULL)
    {
        *pBufferSizeInTChars = size;
    }
    return 0;
}
