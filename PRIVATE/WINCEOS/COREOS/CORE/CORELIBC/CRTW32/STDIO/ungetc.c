//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/***
*ungetc.c - unget a character from a stream
*
*
*Purpose:
*   defines ungetc() - pushes a character back onto an input stream
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <file2.h>
#include <dbgint.h>
#include <internal.h>
#include <mtdll.h>

/***
*int ungetc(ch, stream) - put a character back onto a stream
*
*Purpose:
*   Guaranteed one char pushback on a stream as long as open for reading.
*   More than one char pushback in a row is not guaranteed, and will fail
*   if it follows an ungetc which pushed the first char in buffer. Failure
*   causes return of EOF.
*
*Entry:
*   char ch - character to push back
*   FILEX *stream - stream to push character onto
*
*Exit:
*   returns ch
*   returns EOF if tried to push EOF, stream not opened for reading or
*   or if we have already ungetc'd back to beginning of buffer.
*
*Exceptions:
*
*******************************************************************************/

int __cdecl ungetc (
    REG2 int ch,
    REG1 FILEX *stream
    )
{
    int retval;

    if(!CheckStdioInit())
        return EOF;

    _ASSERTE(stream != NULL);

    if (! _lock_validate_str(stream))
        return EOF;

    retval = _ungetc_lk (ch, stream);

    _unlock_str(stream);

    return(retval);
}

