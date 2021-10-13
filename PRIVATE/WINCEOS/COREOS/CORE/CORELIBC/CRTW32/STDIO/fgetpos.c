//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
