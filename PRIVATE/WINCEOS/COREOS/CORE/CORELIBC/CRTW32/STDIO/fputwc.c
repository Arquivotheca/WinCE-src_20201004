//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/***
*fputwc.c - write a wide character to an output stream
*
*
*Purpose:
*   defines fputwc() - writes a wide character to a stream
*
*******************************************************************************/

#include <cruntime.h>
#include <crtmisc.h>
#include <stdio.h>
#include <stdlib.h>
#include <dbgint.h>
#include <file2.h>
#include <internal.h>
#include <mtdll.h>
#include <msdos.h>
#include <crttchar.h>

/***
*wint_t fputwc(ch, stream) - write a wide character to a stream
*
*Purpose:
*   Writes a wide character to a stream.  Function version of putwc().
*
*Entry:
*   wint_t ch - wide character to write
*   FILEX *stream - stream to write to
*
*Exit:
*   returns the wide character if successful
*   returns WEOF if fails
*
*Exceptions:
*
*******************************************************************************/

wint_t __cdecl fputwc (
    wint_t ch,
    FILEX *str
    )
{
    REG1 FILEX *stream;
    REG2 wint_t retval;

    if(!CheckStdioInit())
        return WEOF;

    _ASSERTE(str != NULL);

    /* Init stream pointer */
    stream = str;

    if (! _lock_validate_str(stream))
        return WEOF;

    retval = _putwc_lk(ch,stream);
    _unlock_str(stream);

    return(retval);
}

