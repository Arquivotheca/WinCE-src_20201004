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
*setmode.c - set file translation mode
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defined _setmode() - set file translation mode of a file
*
*Revision History:
*       08-16-84  RN    initial version
*       10-29-87  JCR   Multi-thread support
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       05-25-88  PHG   Merged DLL and normal versions
*       03-13-90  GJF   Made calling type _CALLTYPE1, added #include
*                       <cruntime.h>, removed #include <register.h> and
*                       fixed the copyright. Also, cleaned up the formatting
*                       a bit.
*       04-04-90  GJF   Added #include <io.h>.
*       10-01-90  GJF   New-style function declarators.
*       12-04-90  GJF   Appended Win32 version onto the source with #ifdef-s.
*                       Two versions should be merged together, the differences
*                       are trivial.
*       12-06-90  SRW   Changed to use _osfile and _osfhnd instead of _osfinfo
*       01-17-91  GJF   ANSI naming.
*       02-13-92  GJF   Replaced _nfile by _nhandle for Win32.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       09-06-94  CFW   Replace MTHREAD with _MT.
*       01-04-95  GJF   _WIN32_ -> _WIN32
*       06-12-95  GJF   Replaced _osfile[] with _osfile() (macro referencing
*                       field in ioinfo struct).
*       06-27-95  GJF   Revised check that the file handle is open.
*       07-09-96  GJF   Replaced defined(_WIN32) with !defined(_MAC). Also,
*                       detab-ed.
*       08-01-96  RDK   For PMac, add check for handle being open.
*       12-29-97  GJF   Exception-safe locking.
*       05-17-99  PML   Remove all Macintosh support.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       11-13-03  SJ    File Handle checks made consistent - VSW#199372
*       12-02-03  SJ    Reroute Unicode I/O
*       10-31-04  AC    Prefast fixes
*                       VSW#373224
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <errno.h>
#include <msdos.h>
#include <mtdll.h>
#include <stddef.h>
#include <internal.h>
#include <stdlib.h>

/***
*int _setmode(fh, mode) - set file translation mode
*
*Purpose:
*       changes file mode to text/binary, depending on mode arg. this affects
*       whether read's and write's on the file translate between CRLF and LF
*       or is untranslated
*
*Entry:
*       int fh - file handle to change mode on
*       int mode - file translation mode (one of O_TEXT and O_BINARY)
*
*Exit:
*       returns old file translation mode
*       returns -1 and sets errno if fails
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _setmode (
        int fh,
        int mode
        )
{
        int retval;

        _VALIDATE_RETURN(((mode == _O_TEXT) ||
                          (mode == _O_BINARY) ||
                          (mode == _O_WTEXT) ||
                          (mode == _O_U8TEXT) ||
                          (mode == _O_U16TEXT)),
                         EINVAL, -1);

        _CHECK_FH_RETURN( fh, EBADF, -1 );
        _CHECK_IO_INIT(-1);
        _VALIDATE_RETURN((fh >= 0 && (unsigned)fh < (unsigned)_nhandle), EBADF, -1);
        _VALIDATE_RETURN((_osfile(fh) & FOPEN), EBADF, -1);

        /* lock the file */
        _lock_fh(fh);

        __try {
                if ( _osfile(fh) & FOPEN )
                        /* set the text/binary mode */
                        retval = _setmode_nolock(fh, mode);
                else {
                        errno = EBADF;
                        _ASSERTE(("Invalid file descriptor. File possibly closed by a different thread",0));
                        retval = -1;
                }
        }
        __finally {
                /* unlock the file */
                _unlock_fh(fh);
        }

        /* Return to user (_setmode_nolock sets errno, if needed) */
        return(retval);
}

/***
*_setmode_nolock() - Perform core setmode operation
*
*Purpose:
*       Core setmode code.  Assumes:
*       (1) Caller has validated fh to make sure it's in range.
*       (2) Caller has locked the file handle.
*
*       [See _setmode() description above.]
*
*Entry: [Same as _setmode()]
*
*Exit:  [Same as _setmode()]
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _setmode_nolock (
        int fh,
        int mode
        )
{
        int oldmode;
        int oldtextmode;

        oldmode = _osfile(fh) & FTEXT;
        oldtextmode = _textmode(fh);
        
        switch(mode) {
            case _O_BINARY :
                _osfile(fh) &= ~FTEXT;
                break;

            case _O_TEXT :
                _osfile(fh) |= FTEXT;
                _textmode(fh) = __IOINFO_TM_ANSI;
                break;
            
            case _O_U8TEXT :
                _osfile(fh) |= FTEXT;
                _textmode(fh) = __IOINFO_TM_UTF8;
                break;
            
            case _O_U16TEXT:
            case _O_WTEXT :
                _osfile(fh) |= FTEXT;
                _textmode(fh) = __IOINFO_TM_UTF16LE;
                break;
        }

        if(oldmode == 0) {
            return _O_BINARY;
        }
        
        if(oldtextmode == __IOINFO_TM_ANSI) {
            return _O_TEXT;
        }
        else {
            return _O_WTEXT;
        }
}

errno_t __cdecl _set_fmode(int mode)
{
    _VALIDATE_RETURN_ERRCODE(((mode == _O_TEXT) || (mode == _O_BINARY) || (mode == _O_WTEXT)), EINVAL);

    _BEGIN_SECURE_CRT_DEPRECATION_DISABLE
    InterlockedExchange(&_fmode, mode);
    _END_SECURE_CRT_DEPRECATION_DISABLE

    return 0;
    
}

errno_t __cdecl _get_fmode(int * pMode)
{
    _VALIDATE_RETURN_ERRCODE((pMode != NULL), EINVAL);
    
    _BEGIN_SECURE_CRT_DEPRECATION_DISABLE
    *pMode = _fmode;
    _END_SECURE_CRT_DEPRECATION_DISABLE

    return 0;
}
