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

    _VALIDATE_RETURN( (stream != NULL), EINVAL, WEOF);

    if (!_lock_validate_str(stream))
        return WEOF;

    __STREAM_TRY
    {
        retval = _getwc_lk(stream);
    }
    __STREAM_FINALLY
    {
        _unlock_str(stream);
    }

    return(retval);
}

