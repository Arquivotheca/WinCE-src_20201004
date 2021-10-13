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
*lseek.c - change file position
*
*
*Purpose:
*   defines _lseek() - move the file pointer
*
*******************************************************************************/

#include <cruntime.h>
#include <mtdll.h>
#include <internal.h>
#include <stdlib.h>
#include <msdos.h>
#include <stdio.h>

/***
*long _lseek(fh,pos,mthd) - move the file pointer
*
*Purpose:
*   Moves the file pointer associated with fh to a new position.
*   The new position is pos bytes (pos may be negative) away
*   from the origin specified by mthd.
*
*   If mthd == SEEK_SET, the origin in the beginning of file
*   If mthd == SEEK_CUR, the origin is the current file pointer position
*   If mthd == SEEK_END, the origin is the end of the file
*
*Entry:
*   int fh - file handle to move file pointer on
*   long pos - position to move to, relative to origin
*   int mthd - specifies the origin pos is relative to (see above)
*
*Exit:
*   returns the offset, in bytes, of the new position from the beginning
*   of the file.
*   returns -1L (and sets errno) if fails.
*   Note that seeking beyond the end of the file is not an error.
*   (although seeking before the beginning is.)
*
*Exceptions:
*
*******************************************************************************/

/* define locking/validating lseek */
long __cdecl _lseek (
    FILEX* str,
    long pos,
    int mthd
    )
{
    _osfilestr(str) &= ~FEOFLAG;    /* clear the ctrl-z flag on the file */
    return SetFilePointer(_osfhndstr(str), pos, NULL, mthd);
}

/*
 * Convenient union for accessing the upper and lower 32-bits of a 64-bit
 * integer.
 */
typedef union doubleint {
    __int64 bigint;
    struct {
        unsigned long lowerhalf;
        long upperhalf;
    } twoints;
} DINT;


// 64-bit version
__int64 __cdecl _lseeki64(
    FILEX* str,
    __int64 pos,
    int mthd
    )
{
    DINT newpos;            /* new file position */

    _osfilestr(str) &= ~FEOFLAG;    /* clear the ctrl-z flag on the file */

    newpos.bigint = pos;
    newpos.twoints.lowerhalf = SetFilePointer(_osfhndstr(str), 
        newpos.twoints.lowerhalf, &(newpos.twoints.upperhalf), mthd);

    return( newpos.bigint );
}


