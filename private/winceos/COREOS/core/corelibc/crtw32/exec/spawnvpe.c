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
*spawnvpe.c - spawn a child process with given environ (search PATH)
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines _spawnvpe() - spawn a child process with given environ (search
*       PATH)
*
*Revision History:
*       04-15-84  DFW   written
*       10-29-85  TC    added spawnvpe capability
*       11-19-86  SKS   handle both kinds of slashes
*       12-01-86  JMB   added Kanji file name support under conditional KANJI
*                       switches.  Corrected header info.  Removed bogus check
*                       for env = b after call to strncpy
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       09-05-88  SKS   Treat EACCES the same as ENOENT -- keep trying
*       10-17-88  GJF   Removed copy of PATH string to local array, changed
*                       bbuf to be a malloc-ed buffer. Removed bogus limits
*                       on the size of that PATH string.
*       10-25-88  GJF   Don't search PATH when relative pathname is given (per
*                       Stevesa). Also, if the name built from PATH component
*                       and filename is a UNC name, allow any error.
*       05-17-89  MT    Added "include <jstring.h>" under KANJI switch
*       05-24-89  PHG   Reduce _amblksiz to use minimal memory (DOS only)
*       08-29-89  GJF   Use _getpath() to retrieve PATH components, fixing
*                       several problems in handling unusual or bizarre
*                       PATH's.
*       11-20-89  GJF   Added const attribute to types of filename, argv and
*                       envptr.
*       03-08-90  GJF   Replaced _LOAD_DS with _CALLTYPE1, added #include
*                       <cruntime.h> and removed #include <register.h>
*       07-24-90  SBM   Removed redundant includes, replaced <assertm.h> by
*                       <assert.h>
*       09-27-90  GJF   New-style function declarator.
*       01-17-91  GJF   ANSI naming.
*       09-25-91  JCR   Changed ifdef "OS2" to "_DOS_" (unused in 32-bit tree)
*       11-30-92  KRS   Port _MBCS code from 16-bit tree.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       12-07-93  CFW   Wide char enable.
*       01-10-95  CFW   Debug CRT allocs.
*       02-06-95  CFW   assert -> _ASSERTE.
*       02-06-98  GJF   Changes for Win64: changed return type to intptr_t.
*       11-02-03  AC    Added validation.
*       04-01-05  MSL   Integer overflow fix during allocation 
*
*******************************************************************************/

#include <cruntime.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <internal.h>
#include <process.h>
#include <mbstring.h>
#include <tchar.h>
#include <dbgint.h>

#define SLASH _T("\\")
#define SLASHCHAR _T('\\')
#define XSLASHCHAR _T('/')
#define DELIMITER _T(";")

#ifdef _MBCS
/* note, the macro below assumes p is to pointer to a single-byte character
 * or the 1st byte of a double-byte character, in a string.
 */
#define ISPSLASH(p)     ( ((p) == _mbschr((p), SLASHCHAR)) || ((p) == \
_mbschr((p), XSLASHCHAR)) )
#else
#define ISSLASH(c)      ( ((c) == SLASHCHAR) || ((c) == XSLASHCHAR) )
#endif

/***
*_spawnvpe(modeflag, filename, argv, envptr) - spawn a child process
*
*Purpose:
*       Spawns a child process with the given arguments and environ,
*       searches along PATH for given file until found.
*       Formats the parameters and calls _spawnve to do the actual work. The
*       NULL environment pointer indicates that the new process will inherit
*       the parents process's environment.  NOTE - at least one argument must
*       be present.  This argument is always, by convention, the name of the
*       file being spawned.
*
*Entry:
*       int modeflag - defines mode of spawn (WAIT, NOWAIT, or OVERLAY)
*                       only WAIT and OVERLAY supported
*       _TSCHAR *filename - name of file to execute
*       _TSCHAR **argv - vector of parameters
*       _TSCHAR **envptr - vector of environment variables
*
*Exit:
*       returns exit code of spawned process
*       if fails, returns -1
*
*Exceptions:
*
*******************************************************************************/

intptr_t __cdecl _tspawnvpe (
        int modeflag,
        const _TSCHAR *filename,
        const _TSCHAR * const *argv,
        const _TSCHAR * const *envptr
        )
{
        intptr_t i;
        _TSCHAR *envbuf = NULL;
        _TSCHAR *env;
        _TSCHAR *buf = NULL;
        _TSCHAR *pfin;
        errno_t save_errno;

        /* validation section */
        _VALIDATE_RETURN(filename != NULL, EINVAL, -1);
        _VALIDATE_RETURN(*filename != _T('\0'), EINVAL, -1);
        _VALIDATE_RETURN(argv != NULL, EINVAL, -1);
        _VALIDATE_RETURN(*argv != NULL, EINVAL, -1);
        _VALIDATE_RETURN(**argv != _T('\0'), EINVAL, -1);

        save_errno = errno;
        errno = 0;

        if (
        (i = _tspawnve(modeflag, filename, argv, envptr)) != -1
                /* everything worked just fine; return i */

        || (errno != ENOENT)
                /* couldn't spawn the process, return failure */

        || (_tcschr(filename, XSLASHCHAR) != NULL)
                /* filename contains a '/', return failure */

        || (_ERRCHECK_EINVAL(_tdupenv_s_crt(&envbuf, NULL, _T("PATH"))) != 0)
        || (envbuf == NULL)
                /* no PATH environment string name, return failure */

        || ( (buf = _calloc_crt(_MAX_PATH, sizeof(_TSCHAR))) == NULL )
                /* cannot allocate buffer to build alternate pathnames, return
                 * failure */
        ) {
                goto done;
        }

        /* could not find the file as specified, search PATH. try each
         * component of the PATH until we get either no error return, or the
         * error is not ENOENT and the component is not a UNC name, or we run
         * out of components to try.
         */

        env = envbuf;
#ifdef WPRFLAG
        while ( (env = _wgetpath(env, buf, _MAX_PATH - 1)) && (*buf) ) {
#else
        while ( (env = _getpath(env, buf, _MAX_PATH - 1)) && (*buf) ) {
#endif            

                pfin = buf + _tcslen(buf) - 1;

                /* if necessary, append a '/'
                 */
#ifdef _MBCS
                if (*pfin == SLASHCHAR) {
                        if (pfin != _mbsrchr(buf,SLASHCHAR))
                        /* fin is the second byte of a double-byte char */
                                strcat_s(buf, _MAX_PATH, SLASH );
                }
                else if (*pfin !=XSLASHCHAR)
                        _ERRCHECK(strcat_s(buf, _MAX_PATH, SLASH));
#else
                if (*pfin != SLASHCHAR && *pfin != XSLASHCHAR)
                        _ERRCHECK(_tcscat_s(buf, _MAX_PATH, SLASH));
#endif
                /* check that the final path will be of legal size. if so,
                 * build it. otherwise, return to the caller (return value
                 * and errno rename set from initial call to _spawnve()).
                 */
                if ( (_tcslen(buf) + _tcslen(filename)) < _MAX_PATH )
                        _ERRCHECK(_tcscat_s(buf, _MAX_PATH, filename));
                else
                        break;

                /* try spawning it. if successful, or if errno comes back with a
                 * value other than ENOENT and the pathname is not a UNC name,
                 * return to the caller.
                 */
                errno = 0;
                if ( (i = _tspawnve(modeflag, buf, argv, envptr)) != -1
                        || (((errno != ENOENT) && (_doserrno != ERROR_NOT_READY))
#ifdef _MBCS
                                && (!ISPSLASH(buf) || !ISPSLASH(buf+1))) )
#else
                                && (!ISSLASH(*buf) || !ISSLASH(*(buf+1)))) )
#endif
                        break;

        }

done:
        if (errno == 0)
        {
            errno = save_errno;
        }
        if (buf != NULL)
            _free_crt(buf);
        if (envbuf != NULL)
            _free_crt(envbuf);
        return(i);
}
