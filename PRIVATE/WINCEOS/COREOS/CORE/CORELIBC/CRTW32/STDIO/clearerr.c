//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
*void clearerr(stream) - clear error and eof flags on a stream
*
*Purpose:
*   Resets the error and eof indicators for a stream to 0
*
*Entry:
*   FILEX *stream - stream to set indicators on
*
*Exit:
*   No return value.
*   changes the _flag field of the FILEX struct.
*
*Exceptions:
*
*******************************************************************************/

void __cdecl clearerr (
    FILEX *stream
    )
{
    if(!CheckStdioInit())
        return;

    _ASSERTE(stream != NULL);

    if (! _lock_validate_str((_FILEX*)stream))
        return;

    /* Clear stdio level flags */
    stream->_flag &= ~(_IOERR|_IOEOF);

    /* Clear lowio level flags */

    _osfilestr(stream) &= ~(FEOFLAG);
    _unlock_str(stream);
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

    if(!CheckStdioInit())
        return 0;

    if (! _lock_validate_str (stream))
        return 0;

    res = (stream)->_flag & _IOEOF;
    
    _unlock_str(stream);

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

    if(!CheckStdioInit())
        return _IOERR;

    if (! _lock_validate_str (stream))
        return _IOERR;

    res = (stream)->_flag & _IOERR;

    _unlock_str(stream);

    return res;
}

