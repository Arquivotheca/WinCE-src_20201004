//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/***
*fgetc.c - get a character from a stream
*
*
*Purpose:
*   defines fgetc() and getc() - read  a character from a stream
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <dbgint.h>
#include <file2.h>
#include <internal.h>
#include <mtdll.h>

/***
*int fgetc(stream), getc(stream) - read a character from a stream
*
*Purpose:
*   reads a character from the given stream
*
*Entry:
*   FILEX *stream - stream to read character from
*
*Exit:
*   returns the character read
*   returns EOF if at end of file or error occurred
*
*Exceptions:
*
*******************************************************************************/

int __cdecl fgetc (
    REG1 FILEX *stream
    )
{
    int retval;

    if(!CheckStdioInit())
        return EOF;

    _ASSERTE(stream != NULL);

    if (! _lock_validate_str(stream))
        return EOF;

    retval = _getc_lk(stream);
    _unlock_str(stream);

    return(retval);
}

/***
#undef getc

int __cdecl getc (
    FILEX *stream
    )
{
    return fgetc(stream);
}
***/

int __cdecl getchar(void)
{
    // need to call CheckStdioInit before using stdin/stdout/stderr macros
    if(!CheckStdioInit())
        return EOF;
    return fgetc(stdin);
}

