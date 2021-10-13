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
*searchenv.c - find a file using paths from an environment variable
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       to search a set a directories specified by an environment variable
*       for a specified file name.  If found the full path name is returned.
*
*Revision History:
*       06-15-87  DFW   initial implementation
*       08-06-87  JCR   Changed directory delimeter from '/' to '\'.
*       09-24-87  JCR   Removed 'const' from declarations (caused cl warnings).
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       02-17-88  JCR   Added 'const' copy_path local to get rid of cl warning.
*       07-19-88  SKS   Fixed bug if root directory is current directory
*       08-03-89  JCR   Allow quoted strings in file/path names
*       08-29-89  GJF   Changed copy_path() to _getpath() and moved it to it's
*                       own source file. Also fixed handling of multiple semi-
*                       colons.
*       11-20-89  GJF   Added const attribute to types of fname and env_var.
*       03-15-90  GJF   Replaced _LOAD_DS with _CALLTYPE1 and added #include
*                       <cruntime.h>. Also, cleaned up the formatting a bit.
*       07-25-90  SBM   Removed redundant include (stdio.h)
*       10-04-90  GJF   New-style function declarator.
*       01-22-91  GJF   ANSI naming.
*       08-26-92  GJF   Include unistd.h for POSIX build.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       12-07-93  CFW   Wide char enable.
*       01-31-95  GJF   Use _fullpath instead of _getcwd, to convert a file
*                       that exists relative to the current directory, to a
*                       fully qualified path.
*       02-16-95  JWM   Mac merge.
*       03-29-95  BWT   Fix POSIX build by sticking with getcwd.
*       10-20-95  GJF   Use local buffer instead of the caller's buffer to
*                       build the pathname (Olympus0 9336).
*       05-17-99  PML   Remove all Macintosh support.
*       06-19-02  MSL   Fix hypothetical potential buffer overrun in searchenv
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       01-15-04  SJ    Changed returns & set errno - VSW#216759,216781
*       01-27-04  SJ    VSW#216606 - errno set by _taccess isn't relevant for 
*                       _searchenv
*       02-24-04  AC    Do not zero-terminate the output buffer at function entry.
*       10-21-04  AC    Allow empty name. Always set errno to ENOENT if the name is not found.
*                       VSW#387748
*       04-01-05  MSL   Integer overflow protection
*
*******************************************************************************/

#include <cruntime.h>
#ifdef  _POSIX_
#include <unistd.h>
#else
#include <direct.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <internal.h>
#include <tchar.h>
#include <dbgint.h>

/***
*_searchenv_s - search for file along paths from environment variable
*
*Purpose:
*       to search for a specified file in the directory(ies) specified by
*       a given environment variable, and, if found, to return the full
*       path name of the file.  The file is first looked for in the current
*       working directory, prior to looking in the paths specified by env_var.
*
*Entry:
*       const _TSCHAR * fname - name of file to search for
*       const _TSCHAR * env_var - name of environment variable to use for paths
*       _TSCHAR * path - pointer to storage for the constructed path name
*       size_t sz - the size of the path buffer
*
*Exit:
*       returns 0 on success & sets pfile
*       returns errno_t on failure.
*       The path parameter is filled with the fullpath of found file on success
*
*Exceptions:
*
*******************************************************************************/

errno_t __cdecl _tsearchenv_s (
        const _TSCHAR *fname,
        const _TSCHAR *env_var,
        _TSCHAR *path,
        size_t sz
        )
{
        register _TSCHAR *p;
        register int c;
        _TSCHAR *envbuf = NULL;
        _TSCHAR *env_p, *save_env_p;
        size_t len;
        _TSCHAR pathbuf[_MAX_PATH + 4];
        _TSCHAR * pbuf = NULL;
        size_t fnamelen, buflen;
        errno_t save_errno;
        int ret;
        errno_t retvalue = 0;

        _VALIDATE_RETURN_ERRCODE( (path != NULL), EINVAL);
        _VALIDATE_RETURN_ERRCODE( (sz > 0), EINVAL);
        if (fname == NULL)
        {
            *path = _T('\0');
            _VALIDATE_RETURN_ERRCODE( (fname != NULL), EINVAL);
        }

        /* special case: fname is an empty string: just return an empty path, errno is set to ENOENT */
        if (fname[0] == 0)
        {
            *path = _T('\0');
            errno = ENOENT;
            retvalue = errno;
            goto cleanup;
        }

        save_errno = errno;
        ret = _taccess_s(fname, 0);
        errno = save_errno;
        
        if (ret == 0 ) {

            /* exists, convert it to a fully qualified pathname and
               return */
            if ( _tfullpath(path, fname, sz) == NULL )
            {
                *path = _T('\0'); /* fullpath will set errno in this case */
                retvalue = errno;
                goto cleanup;
            }

            retvalue = 0;
            goto cleanup;
        }

        if (_ERRCHECK_EINVAL(_tdupenv_s_crt(&envbuf, NULL, env_var)) != 0 || envbuf == NULL) {
            /* no such environment var. and not in cwd, so return empty
               string, set errno to ENOENT */
            *path = _T('\0');
            errno = ENOENT;
            retvalue = errno;
            goto cleanup;
        }

        env_p = envbuf;
        fnamelen = _tcslen(fname);
        pbuf = pathbuf;
        buflen = _countof(pathbuf);

        if(fnamelen >= buflen)
        {
            /* +2 for the trailing '\' we may need to add & the '\0' */
            buflen = _tcslen(env_p) + fnamelen + 2;
            pbuf = (_TSCHAR *)_calloc_crt( buflen, sizeof(_TSCHAR));
            if(!pbuf)
            {
                 *path = _T('\0');
                 errno = ENOMEM;
                 retvalue = errno;
                 goto cleanup;
            }
        }

        save_errno = errno;
        
        while(env_p)
        {
            save_env_p = env_p;
            env_p = _tgetpath(env_p, pbuf, buflen - fnamelen - 1);
            
            if( env_p == NULL && pbuf == pathbuf && errno == ERANGE)
            {
                buflen = _tcslen(save_env_p) + fnamelen + 2;
                pbuf = (_TSCHAR *)_calloc_crt( buflen, sizeof(_TSCHAR));
                if(!pbuf)
                {
                     *path = _T('\0');
                     errno = ENOMEM;
                     retvalue = errno;
                     goto cleanup;
                }
                env_p = _tgetpath(save_env_p, pbuf, buflen - fnamelen);
            }

            if(env_p == NULL || *pbuf == _T('\0'))
                break;

            /* path now holds nonempty pathname from env_p, concatenate
               the file name and go */
            /* If we reached here, we know that buflen is enough to hold
            the concatenation. If not, the getpath would have failed */
            
            len = _tcslen(pbuf);
            p = pbuf + len;
            if ( ((c = *(p - 1)) != _T('/')) && (c != _T('\\')) &&
                 (c != _T(':')) )
            {
                /* add a trailing '\' */
                *p++ = _T('\\');
                len++;
            }
            /* p now points to character following trailing '/', '\'
               or ':' */

            _ERRCHECK(_tcscpy_s(p, buflen - (p - pbuf), fname));

            if ( _taccess_s(pbuf, 0) == 0 ) {
                /* found a match, copy the full pathname into the caller's
                   buffer */
                if(len + fnamelen >= sz) {
                    *path = _T('\0');
                    
                    if(pbuf != pathbuf)
                        _free_crt(pbuf);

                    errno = ERANGE;
                    retvalue = errno;
                    goto cleanup;
                }
                
                errno = save_errno;

                _ERRCHECK(_tcscpy_s(path, sz, pbuf));

                if(pbuf != pathbuf)
                    _free_crt(pbuf);

                retvalue = 0;
                goto cleanup;
            }

        }
        /* if we get here, we never found it, return empty string */
        *path = _T('\0');
        errno = ENOENT;
        retvalue = errno;

cleanup:
        if(pbuf != pathbuf)
            _free_crt(pbuf);

        _free_crt(envbuf);

        return retvalue;
}

/***
*_searchenv() - search for file along paths from environment variable
*
*Purpose:
*       to search for a specified file in the directory(ies) specified by
*       a given environment variable, and, if found, to return the full
*       path name of the file.  The file is first looked for in the current
*       working directory, prior to looking in the paths specified by env_var.
*
*Entry:
*       fname - name of file to search for
*       env_var - name of environment variable to use for paths
*       path - pointer to storage for the constructed path name
*
*Exit:
*       path - pointer to constructed path name, if the file is found, otherwise
*              it points to the empty string.
*
*Exceptions:
*
*******************************************************************************/

void __cdecl _tsearchenv (
        const _TSCHAR *fname,
        const _TSCHAR *env_var,
        _TSCHAR *path
        )
{
    _tsearchenv_s(fname, env_var, path, _MAX_PATH);
}        
