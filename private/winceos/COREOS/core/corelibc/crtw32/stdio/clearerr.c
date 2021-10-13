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
*clearerr.c - clear error and eof flags
*
*
*Purpose:
*   defines clearerr() - clear error and eof flags from a stream
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <dbgint.h>
#include <file2.h>
#include <mtdll.h>
#include <internal.h>
#include <msdos.h>

/***
*errno_t clearerr(stream) - clear error and eof flags on a stream
*
*Purpose:
*       Resets the error and eof indicators for a stream to 0
*
*Entry:
*       FILE *stream - stream to set indicators on
*
*Exit:
*       0 on success, Otherwise error code
*
*Exceptions:
*
*******************************************************************************/

errno_t __cdecl clearerr_s (FILEX * stream)
{
    _VALIDATE_RETURN_ERRCODE((stream != NULL), EINVAL);

    if (!_lock_validate_str(stream))
        return EINVAL;

    __STREAM_TRY
    {
        /* Clear stdio level flags */
        stream->_flag &= ~(_IOERR|_IOEOF);

        /* Clear lowio level flags */
        _osfilestr(stream) &= ~(FEOFLAG);
    }
    __STREAM_FINALLY
    {
        _unlock_str(stream);
    }

    return 0;
}

/***
*void clearerr(stream) - clear error and eof flags on a stream
*
*Purpose:
*       Resets the error and eof indicators for a stream to 0
*
*Entry:
*       FILE *stream - stream to set indicators on
*
*Exit:
*       No return value.
*       changes the _flag field of the FILE struct.
*
*Exceptions:
*
*******************************************************************************/

void __cdecl clearerr (
        FILE *stream
        )
{
    clearerr_s(stream);
}

/***
*int feof(stream) - test for end-of-file on stream
*
*Purpose:
*   Tests whether or not the given stream is at end-of-file. Normally
*   feof() is a macro, but it must also be available as a true function
*   for ANSI.
*
*Entry:
*   FILEX *stream - stream to test
*
*Exit:
*   returns nonzero (_IOEOF to be more precise) if and only if the stream
*   is at end-of-file
*
*Exceptions:
*
*******************************************************************************/

int __cdecl feof (
    FILEX *stream
    )
{
    int res;

    if (!_lock_validate_str (stream))
        return 0;

    __STREAM_TRY
    {
        res = (stream)->_flag & _IOEOF;
    }
    __STREAM_FINALLY
    {
        _unlock_str(stream);
    }

    return res;
}


/***
*int ferror(stream) - test error indicator on stream
*
*Purpose:
*   Tests the error indicator for the given stream. Normally, feof() is
*   a macro, but it must also be available as a true function for ANSI.
*
*Entry:
*   FILEX *stream - stream to test
*
*Exit:
*   returns nonzero (_IOERR to be more precise) if and only if the error
*   indicator for the stream is set.
*
*Exceptions:
*
*******************************************************************************/

int __cdecl ferror (
    FILEX *stream
    )
{
    int res;

    if (! _lock_validate_str (stream))
        return _IOERR;

    __STREAM_TRY
    {
        res = (stream)->_flag & _IOERR;
    }
    __STREAM_FINALLY
    {
        _unlock_str(stream);
    }

    return res;
}

