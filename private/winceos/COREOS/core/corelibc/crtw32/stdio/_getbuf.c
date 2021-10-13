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
*_getbuf.c - Get a stream buffer
*
*
*Purpose:
*   Allocate a buffer and init stream data bases.
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <file2.h>
#include <malloc.h>
#include <internal.h>
#include <dbgint.h>

/***
*_getbuf() - Allocate a buffer and init stream data bases
*
*Purpose:
*   Allocates a buffer for a stream and inits the stream data bases.
*
*   [NOTE  1: This routine assumes the caller has already checked to make
*   sure the stream needs a buffer.
*
*   [NOTE 2: Multi-thread - Assumes caller has aquired stream lock, if
*   needed.]
*
*Entry:
*   FILEX *stream = stream to allocate a buffer for
*
*Exit:
*   void
*
*Exceptions:
*
*******************************************************************************/

void __cdecl _getbuf (
    FILEX *str
    )
{
    REG1 FILEX *stream;

    _ASSERTE(str != NULL);

    /* Init pointers */
    stream = str;

    /* Try to get a big buffer */
    stream->_base = _malloc_crt(_INTERNAL_BUFSIZ);
    if (stream->_base)
    {
        /* Got a big buffer */
        stream->_flag |= _IOMYBUF;
        stream->_bufsiz = _INTERNAL_BUFSIZ;
    }
    else 
    {
        /* Did NOT get a buffer - use single char buffering. */
        stream->_flag |= _IONBF;
        stream->_base = (char *)&(stream->_charbuf);
        stream->_bufsiz = 2;
    }

    stream->_ptr = stream->_base;
    stream->_cnt = 0;

    return;
}


/***
*void _freebuf(stream) - release a buffer from a stream
*
*Purpose:
*   free a buffer if at all possible. free() the space if malloc'd by me.
*   forget about trying to free a user's buffer for him; it may be static
*   memory (not from malloc), so he has to take care of it. this function
*   is not intended for use outside the library.
*
*ifdef _MT
*   Multi-thread notes:
*   _freebuf() does NOT get the stream lock; it is assumed that the
*   caller has already done this.
*endif
*
*Entry:
*   FILEX *stream - stream to free bufer on
*
*Exit:
*   Buffer may be freed.
*   No return value.
*
*Exceptions:
*
*******************************************************************************/

void __cdecl _freebuf (
    REG1 FILEX *stream
    )
{

    _ASSERTE(stream != NULL);

    if (inuse(stream) && mbuf(stream))
    {
        _free_crt(stream->_base);
        stream->_flag &= ~(_IOMYBUF | _IOSETVBUF);
        stream->_base = stream->_ptr = NULL;
        stream->_cnt = 0;
    }
}

