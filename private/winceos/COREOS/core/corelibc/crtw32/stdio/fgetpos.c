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
*fgetpos.c - Contains the fgetpos runtime
*
*
*Purpose:
*       Get file position (in an internal format).
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <internal.h>

/***
*int fgetpos(stream,pos) - Get file position (internal format)
*
*Purpose:
*       Fgetpos gets the current file position for the file identified by
*       [stream].  The file position is returned in the object pointed to
*       by [pos] and is in internal format; that is, the user is not supposed
*       to interpret the value but simply use it in the fsetpos call.  Our
*       implementation simply uses fseek/ftell.
*
*Entry:
*       FILEX *stream = pointer to a file stream value
*       fpos_t *pos = pointer to a file position value
*
*Exit:
*       Successful fgetpos call returns 0.
*       Unsuccessful fgetpos call returns non-zero (!0) value and sets
*       ERRNO (this is done by ftell and passed back by fgetpos).
*
*Exceptions:
*       None.
*
*******************************************************************************/

int __cdecl fgetpos (
        FILEX *stream,
        fpos_t *pos
        )
{
    if(!CheckStdioInit())
        return -1L;

#if _INTEGRAL_MAX_BITS >= 64
    if ( (*pos = _ftelli64(stream)) != -1i64 )
#else
    if ( (*pos = ftell(stream)) != -1L )
#endif
        return(0);
    else
        return(-1);
}
