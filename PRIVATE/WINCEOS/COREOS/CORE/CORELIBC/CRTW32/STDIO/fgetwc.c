//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/***
*fgetwc.c - get a wide character from a stream
*
*
*Purpose:
*   defines fgetwc() - read a wide character from a stream
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
*wint_t fgetwc(stream) - read a wide character from a stream
*
*Purpose:
*   reads a wide character from the given stream
*
*Entry:
*   FILEX *stream - stream to read wide character from
*
*Exit:
*   returns the wide character read
*   returns WEOF if at end of file or error occurred
*
*Exceptions:
*
*******************************************************************************/

wint_t __cdecl fgetwc (
    REG1 FILEX *stream
    )
{
    wint_t retval;
    
    if(!CheckStdioInit())
        return WEOF;

    _ASSERTE(stream != NULL);

    if (! _lock_validate_str(stream))
        return WEOF;

    retval = _getwc_lk(stream);
    _unlock_str(stream);

    return(retval);
}

