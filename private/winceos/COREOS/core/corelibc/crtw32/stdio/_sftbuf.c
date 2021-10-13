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
*_sftbuf.c - temporary buffering initialization and flushing
*
*
*Purpose:
*   temporary buffering initialization and flushing. if stdout/err is
*   unbuffered, buffer it temporarily so that string is sent to kernel as
*   a batch of chars, not char-at-a-time. if appropriate, make buffering
*   permanent.
*
*   [NOTE 1: These routines assume that the temporary buffering is only
*   used for output.  In particular, note that _stbuf() sets _IOWRT.]
*
*   [NOTE 2: It is valid for this module to assign a value directly to
*   _flag instead of simply twiddling bits since we are initializing the
*   buffer data base.]
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <file2.h>
#include <malloc.h>
#include <mtdll.h>
#include <dbgint.h>
#include <internal.h>
#include <msdos.h>

/* Buffer pointers for stdout and stderr */
void *_stdbuf[2] = { NULL, NULL};

/***
*int _stbuf(stream) - set temp buffer on stdout, stdprn, stderr
*
*Purpose:
*   if stdout/stderr is still unbuffered, buffer it.
*   this function works intimately with _ftbuf, and accompanies it in
*   bracketing normally unbuffered output. these functions intended for
*   library use only.
*
*   Multi-thread: It is assumed that the caller has already aquired the
*   stream lock.
*
*Entry:
*   FILEX *stream - stream to temp buffer
*
*Exit:
*   returns 1 if buffer initialized, 0 if not
*   sets fields in stdout or stderr to indicate buffering
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _stbuf (
    FILEX *str
    )
{
    REG1 FILEX *stream;
    int index;

    _ASSERTE(str != NULL);

    /* Init near stream pointer */
    stream = str;

        /* do nothing if not a tty device */
    if (!_isatty(stream))
                return(0);

    /* Make sure stream is stdout/stderr and init _stdbuf index */
    if (stream == stdout)
        index = 0;
    else if (stream == stderr)
        index = 1;
    else
        return(0);

    /* Make sure the stream is not already buffered. */
    if (anybuf(stream))
        return(0);

    /* Allocate a buffer for this stream if we haven't done so yet. */
    if (_stdbuf[index] == NULL)
        if ( (_stdbuf[index]=_malloc_crt(_INTERNAL_BUFSIZ)) == NULL )
            return(0);  /* error */

    /* Set up the buffer */
    stream->_ptr = stream->_base = _stdbuf[index];
    stream->_cnt = stream->_bufsiz = _INTERNAL_BUFSIZ;
    stream->_flag |= (_IOWRT | _IOYOURBUF | _IOFLRTN);

    return(1);
}


/***
*void _ftbuf(flag, stream) - take temp buffering off a stream
*
*Purpose:
*   If stdout/stderr is being buffered and it is a device, _flush and
*   dismantle the buffer. if it's not a device, leave the buffering on.
*   This function works intimately with _stbuf, and accompanies it in
*   bracketing normally unbuffered output. these functions intended for
*   library use only
*
*   Multi-thread: It is assumed that the caller has already aquired the
*   stream lock.
*
*Entry:
*   int flag     - a flag to tell whether to dismantle temp buffering on a
*              stream
*   FILEX *stream - the stream
*
*Exit:
*   no return value
*   sets fields in stdout/stderr
*
*Exceptions:
*
*******************************************************************************/

void __cdecl _ftbuf (
    int flag,
    FILEX *str
    )
{
    REG1 FILEX *stream;

    _ASSERTE(flag == 0 || flag == 1);

    /* Init near stream pointers */
    stream = str;

    if (flag) {

        if (stream->_flag & _IOFLRTN) {

            /* Flush the stream and tear down temp buffering. */
            _flush(stream);
            stream->_flag &= ~(_IOYOURBUF | _IOFLRTN);
            stream->_bufsiz = 0;
            stream->_base = stream->_ptr = NULL;
        }

        /* Note: If we expand the functionality of the _IOFLRTN bit to
        include other streams, we may want to clear that bit here under
        an 'else' clause (i.e., clear bit in the case that we leave the
        buffer permanently assigned.  Given our current use of the bit,
        the extra code is not needed. */

    } /* end flag = 1 */
}
