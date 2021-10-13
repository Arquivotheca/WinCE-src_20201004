//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/***
*fputc.c - write a character to an output stream
*
*
*Purpose:
*   defines fputc() - writes a character to a stream
*   defines fputwc() - writes a wide character to a stream
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <dbgint.h>
#include <file2.h>
#include <internal.h>
#include <mtdll.h>

/***
*int fputc(ch, stream) - write a character to a stream
*
*Purpose:
*   Writes a character to a stream.  Function version of putc().
*
*Entry:
*   int ch - character to write
*   FILEX *stream - stream to write to
*
*Exit:
*   returns the character if successful
*   returns EOF if fails
*
*Exceptions:
*
*******************************************************************************/

int __cdecl fputc (
    int ch,
    FILEX *str
    )
{
    REG1 FILEX *stream;
    REG2 int retval;
    
    if(!CheckStdioInit())
        return EOF;

    _ASSERTE(str != NULL);

    /* Init stream pointer */
    stream = str;

    if (! _lock_validate_str(stream))
        return EOF;

    retval = _putc_lk(ch,stream);
    _unlock_str(stream);

    return(retval);
}

/***
#undef putc

int __cdecl putc (
    int ch,
    FILEX *str
    )
{
    return fputc(ch, str);
}
**/

int __cdecl putchar(int ch)
{
    if(!CheckStdioInit())
        return EOF;
    return(fputc(ch, stdout));
}

