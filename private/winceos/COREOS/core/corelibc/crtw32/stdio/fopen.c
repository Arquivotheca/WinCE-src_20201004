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
*fopen.c - open a file
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines fopen() and _fsopen() - open a file as a stream and open a file
*       with a specified sharing mode as a stream
*
*Revision History:
*       09-02-83  RN    initial version
*       04-13-87  JCR   added const to declarations
*       11-01-87  JCR   Multi-thread support
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       05-31-88  PHG   Merged DLL and normal versions
*       11-14-88  GJF   Added _fsopen().
*       02-15-90  GJF   Fixed copyright and indents
*       03-19-90  GJF   Replaced _LOAD_DS with _CALLTYPE1, added #include
*                       <cruntime.h> and removed #include <register.h>.
*       07-23-90  SBM   Replaced <assertm.h> by <assert.h>
*       10-02-90  GJF   New-style function declarators.
*       01-21-91  GJF   ANSI naming.
*       03-26-92  DJM   POSIX support
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       11-01-93  CFW   Enable Unicode variant.
*       02-06-94  CFW   assert -> _ASSERTE.
*       02-20-95  GJF   Replaced WPRFLAG with _UNICODE.
*       03-07-95  GJF   _[un]lock_str macros now take FILE * arg.
*       02-27-98  GJF   Exception-safe locking.
*       10-06-99  PML   Set errno EMFILE when out of streams.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-20-03  SJ    VSW#179433 - _tfopen_s not update FILE * output param
*       12-08-03  SJ    VSW#189853 - Incorrect check in _tfopen_s corrected
*       04-07-04  MSL   Removed excessive filename validation
*                       VSW#274124
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <share.h>
#include <dbgint.h>
#include <internal.h>
#include <mtdll.h>
#include <file2.h>
#include <tchar.h>
#include <errno.h>

/***
*FILE *_fsopen(file, mode, shflag) - open a file
*
*Purpose:
*       Opens the file specified as a stream.  mode determines file mode:
*       "r": read       "w": write      "a": append
*       "r+": read/write                "w+": open empty for read/write
*       "a+": read/append
*       Append "t" or "b" for text and binary mode. shflag determines the
*       sharing mode. Values are the same as for sopen().
*
*Entry:
*       char *file - file name to open
*       char *mode - mode of file access
*
*Exit:
*       returns pointer to stream
*       returns NULL if fails
*
*Exceptions:
*
*******************************************************************************/

FILE * __cdecl _tfsopen (
        const _TSCHAR *file,
        const _TSCHAR *mode
#ifndef _POSIX_
        ,int shflag
#endif
        )
{
    FILE *stream=NULL;
    FILE *retval=NULL;

    _VALIDATE_RETURN((file != NULL), EINVAL, NULL);
    _VALIDATE_RETURN((mode != NULL), EINVAL, NULL);
    _VALIDATE_RETURN((*mode != _T('\0')), EINVAL, NULL);

    /* Get a free stream */
    /* [NOTE: _getstream() returns a locked stream.] */

    if ((stream = _getstream()) == NULL)
    {
        errno = EMFILE;
        return(NULL);
    }

    __try {
        /* We deliberately don't hard-validate for empty strings here. All other invalid
        path strings are treated as runtime errors by the inner code in _open and openfile. 
        This is also the appropriate treatment here. Since fopen is the primary access point
        for file strings it might be subjected to direct user input and thus must be robust to 
        that rather than aborting. The CRT and OS do not provide any other path validator (because 
        WIN32 doesn't allow such things to exist in full generality).
        */

        if(*file==_T('\0'))
        {
            errno=EINVAL;
            return NULL;
        }

        /* open the stream */
#ifdef _POSIX_
#ifdef _UNICODE
        retval = _wopenfile(file,mode, stream);
#else
        retval = _openfile(file,mode, stream);
#endif
#else
#ifdef _UNICODE
        retval = _wopenfile(file,mode,shflag,stream);
#else
        retval = _openfile(file,mode,shflag,stream);
#endif
#endif

    }
    __finally {
        _unlock_str(stream);
    }

    return(retval);
}

  
/***
*FILE *fopen(file, mode) - open a file
*
*Purpose:
*       Opens the file specified as a stream.  mode determines file mode:
*       "r": read       "w": write      "a": append
*       "r+": read/write                "w+": open empty for read/write
*       "a+": read/append
*       Append "t" or "b" for text and binary mode
*
*Entry:
*       char *file - file name to open
*       char *mode - mode of file access
*
*Exit:
*       returns pointer to stream
*       returns NULL if fails
*
*Exceptions:
*
*******************************************************************************/

FILE * __cdecl _tfopen (
        const _TSCHAR *file,
        const _TSCHAR *mode
        )
{
#ifdef _POSIX_
        return( _tfsopen(file, mode) );
#else
        return( _tfsopen(file, mode, _SH_DENYNO) );
#endif
}

/***
*errno_t _tfopen_s(pfile, file, mode) - open a file
*
*Purpose:
*       Opens the file specified as a stream.  mode determines file mode:
*       "r": read       "w": write      "a": append
*       "r+": read/write                "w+": open empty for read/write
*       "a+": read/append
*       Append "t" or "b" for text and binary mode
*       This is the secure version fopen - it opens the file in _SH_DENYRW
*       share mode.
*
*Entry:
*       FILE **pfile - Pointer to return the FILE handle into.
*       char *file - file name to open
*       char *mode - mode of file access
*
*Exit:
*       returns 0 on success & sets pfile
*       returns errno_t on failure.
*
*Exceptions:
*
*******************************************************************************/

errno_t __cdecl _tfopen_s (
        FILE ** pfile,
        const _TSCHAR *file,
        const _TSCHAR *mode
        )
{
        _VALIDATE_RETURN_ERRCODE((pfile != NULL), EINVAL);
        *pfile = _tfsopen(file, mode, _SH_SECURE);
        
        if(*pfile != NULL)
            return 0;

        return errno;
}
