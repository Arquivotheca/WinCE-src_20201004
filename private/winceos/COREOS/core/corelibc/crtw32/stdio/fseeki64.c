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
*fseeki64.c - reposition file pointer on a stream
*
*
*Purpose:
*       defines _fseeki64() - move the file pointer to new place in file
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <file2.h>
#include <dbgint.h>
#include <msdos.h>
#include <malloc.h>
#include <internal.h>
#include <mtdll.h>

/***
*int _fseeki64(stream, offset, whence) - reposition file pointer
*
*Purpose:
*
*       Reposition file pointer to the desired location.  The new location
*       is calculated as follows:
*                                { whence=0, beginning of file }
*               <offset> bytes + { whence=1, current position  }
*                                { whence=2, end of file       }
*
*       Be careful to coordinate with buffering.
*
*Entry:
*       FILEX *stream  - file to reposition file pointer on
*       _int64 offset - offset to seek to
*       int whence    - origin offset is measured from (0=beg, 1=current pos,
*                       2=end)
*
*Exit:
*       returns 0 if succeeds
*       returns -1 and sets errno if fails
*       fields of FILEX struct will be changed
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _fseeki64 (
    FILEX *stream,
    __int64 offset,
    int whence
    )
{
    int retval;

    _VALIDATE_RETURN((stream != NULL), EINVAL, -1);
    _VALIDATE_RETURN(((whence == SEEK_SET) ||
                      (whence == SEEK_CUR) ||
                      (whence == SEEK_END)), EINVAL, -1);

    if (!_lock_validate_str(stream))
        return -1;

    __STREAM_TRY
    {
        retval = _fseeki64_lk (stream, offset, whence);
    }
    __STREAM_FINALLY
    {
        _unlock_str(stream);
    }

    return(retval);
}


/***
*_fseeki64_lk() - Core _fseeki64() routine (stream is locked)
*
*Purpose:
*       Core _fseeki64() routine; assumes that caller has the stream locked.
*
*Entry:
*
*Exit:
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _fseeki64_lk (
    FILEX *str,
    __int64 offset,
    int whence
    )
{
    REG1 FILEX *stream;

    /* Init stream pointer */
    stream = str;

    if (!inuse(stream))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return(-1);
    }

    /* Clear EOF flag */

    stream->_flag &= ~_IOEOF;

    /* If seeking relative to current location, then convert to
       a seek relative to beginning of file.  This accounts for
       buffering, etc. by letting fseek() tell us where we are. */

    if (whence == SEEK_CUR) {
        offset += _ftelli64_lk(stream);
        whence = SEEK_SET;
    }

    /* Flush buffer as necessary */

    _flush(stream);

    /* If file opened for read/write, clear flags since we don't know
       what the user is going to do next. If the file was opened for
       read access only, decrease _bufsiz so that the next _filbuf
       won't cost quite so much */

    if (stream->_flag & _IORW)
        stream->_flag &= ~(_IOWRT|_IOREAD);
    else if ((stream->_flag & _IOREAD) && (stream->_flag & _IOMYBUF) &&
             !(stream->_flag & _IOSETVBUF) )
             stream->_bufsiz = _SMALL_BUFSIZ;

    /* Seek to the desired locale and return. */

    return(_lseeki64(stream, offset, whence) == -1i64 ? -1 : 0);
}
