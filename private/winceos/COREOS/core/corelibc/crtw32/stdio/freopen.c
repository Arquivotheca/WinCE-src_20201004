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
*freopen.c - close a stream and assign it to a new file
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines freopen() - close and reopen file, typically used to redirect
*       stdin/out/err/prn/aux.
*
*Revision History:
*       09-02-83  RN    initial version
*       04-13-87  JCR   added const to declarations
*       11-02-87  JCR   Multi-thread support
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       05-27-88  PHG   Merged DLL and normal versions
*       06-15-88  JCR   Near reference to _iob[] entries; improve REG variables
*       08-25-88  GJF   Don't use FP_OFF() macro for the 386
*       11-14-88  GJF   _openfile() now takes a file sharing flag, also some
*                       cleanup (now specific to the 386)
*       08-17-89  GJF   Clean up, now specific to OS/2 2.0 (i.e., 386 flat
*                       model). Also fixed copyright and indents.
*       02-15-90  GJF   Fixed copyright
*       03-19-90  GJF   Made calling type _CALLTYPE1, added #include
*                       <cruntime.h> and removed #include <register.h>.
*       07-23-90  SBM   Replaced <assertm.h> by <assert.h>
*       10-02-90  GJF   New-style function declarator.
*       01-22-91  GJF   ANSI naming.
*       03-27-92  DJM   POSIX support.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       11-01-93  CFW   Enable Unicode variant.
*       01-17-94  GJF   Ignore possible failure of __fclose_lk (ANSI 4.9.5.4)
*       04-11-94  CFW   Remove unused 'done' label to avoid warnings.
*       02-06-94  CFW   assert -> _ASSERTE.
*       02-20-95  GJF   Replaced WPRFLAG with _UNICODE.
*       03-07-95  GJF   _[un]lock_str macros now take FILE * arg.
*       03-02-98  GJF   Exception-safe locking.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-20-03  SJ    errno shouldn't be set to 0 on success
*       04-07-04  MSL   Removed excessive filename validation
*                       VSW#274124
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <file2.h>
#include <share.h>
#include <dbgint.h>
#include <internal.h>
#include <mtdll.h>
#include <tchar.h>

errno_t __cdecl _tfreopen_helper (
        FILE ** pfile,
        const _TSCHAR *filename,
        const _TSCHAR *mode,
        FILE *str,
        int shflag
        )
{
    FILE *stream;

    _VALIDATE_RETURN_ERRCODE( (pfile != NULL), EINVAL);
    *pfile = NULL;
    _VALIDATE_RETURN_ERRCODE( (filename != NULL), EINVAL);
    _VALIDATE_RETURN_ERRCODE( (mode != NULL), EINVAL);
    _VALIDATE_RETURN_ERRCODE( (str != NULL), EINVAL);

    /* We deliberately don't hard-validate for empty strings here. All other invalid
    path strings are treated as runtime errors by the inner code in _open and openfile.
    This is also the appropriate treatment here. Since fopen is the primary access point
    for file strings it might be subjected to direct user input and thus must be robust to
    that rather than aborting. The CRT and OS do not provide any other path validator (because
    WIN32 doesn't allow such things to exist in full generality).
    */
    if (*filename==_T('\0'))
    {
        errno=EINVAL;
        return errno;
    }

    /* Init stream pointer */
    stream = str;

    _lock_str(stream);
    __try {
        /* If the stream is in use, try to close it. Ignore possible
         * error (ANSI 4.9.5.4). */
        if ( inuse(stream) )
        {
            _fclose_nolock(stream);
        }

        stream->_ptr = stream->_base = NULL;
        stream->_cnt = stream->_flag = 0;
#ifdef _POSIX_
#ifdef _UNICODE
        *pfile = _wopenfile(filename,mode,stream);
#else
        *pfile = _openfile(filename,mode,stream);
#endif
#else
#ifdef _UNICODE
        *pfile = _wopenfile(filename,mode,shflag,stream);
#else
        *pfile = _openfile(filename,mode,shflag,stream);
#endif
#endif
    }
    __finally {
        _unlock_str(stream);
    }

    if(*pfile)
        return 0;

    return errno;
}

/***
*FILE *freopen(filename, mode, stream) - reopen stream as new file
*
*Purpose:
*       Closes the file associated with stream and assigns stream to a new
*       file with current mode.  Usually used to redirect a standard file
*       handle.
*
*Entry:
*       char *filename - new file to open
*       char *mode - new file mode, as in fopen()
*       FILE *stream - stream to close and reassign
*
*Exit:
*       returns stream if successful
*       return NULL if fails
*
*Exceptions:
*
*******************************************************************************/

FILE * __cdecl _tfreopen (
        const _TSCHAR *filename,
        const _TSCHAR *mode,
        FILE *str
        )
{
    FILE * fp = NULL;

    _tfreopen_helper(&fp,filename,mode,str,_SH_DENYNO);

    return fp;
}        

/***
*errno_t freopen(pfile,filename, mode, stream) - reopen stream as new file
*
*Purpose:
*       Closes the file associated with stream and assigns stream to a new
*       file with current mode.  Usually used to redirect a standard file
*       handle.This is the secure version fopen - it opens the file 
*       in _SH_DENYRW share mode.

*
*Entry:
*       FILE **pfile - Pointer to return the FILE handle into.
*       char *filename - new file to open
*       char *mode - new file mode, as in fopen()
*       FILE *stream - stream to close and reassign
*
*Exit:
*       returns 0 on success & sets pfile
*       returns errno_t on failure.
*
*Exceptions:
*
*******************************************************************************/

errno_t __cdecl _tfreopen_s (
        FILE ** pfile,
        const _TSCHAR *filename,
        const _TSCHAR *mode,
        FILE *str
        )
{
    return _tfreopen_helper(pfile,filename,mode,str,_SH_SECURE);
}        
