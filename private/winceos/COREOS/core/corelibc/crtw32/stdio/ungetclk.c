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
*ungetclk.c - unget a character from a stream
*
*
*Purpose:
*   defines ungetc() - pushes a character back onto an input stream
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <file2.h>
#include <dbgint.h>
#include <internal.h>
#include <mtdll.h>


/***
*_ungetc_lk() -  Ungetc() core routine (locked version)
*
*Purpose:
*   Core ungetc() routine; assumes stream is already locked.
*
*   [See ungetc() above for more info.]
*
*Entry: [See ungetc()]
*
*Exit:  [See ungetc()]
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _ungetc_lk (
    REG2 int ch,
    FILEX *str
    )

{
    REG1 FILEX *stream;

    _ASSERTE(str != NULL);

    /* Init stream pointer and file descriptor */
    stream = str;

    /* Stream must be open for read and can NOT be currently in write mode.
       Also, ungetc() character cannot be EOF. */

    if (
          (ch == EOF) ||
          !(
        (stream->_flag & _IOREAD) ||
        ((stream->_flag & _IORW) && !(stream->_flag & _IOWRT))
           )
       )
        return(EOF);

    /* If stream is unbuffered, get one. */

    if (stream->_base == NULL)
        _getbuf(stream);

    /* now we know _base != NULL; since file must be buffered */

    if (stream->_ptr == stream->_base) {
        if (stream->_cnt)
            /* my back is against the wall; i've already done
             * ungetc, and there's no room for this one
             */
            return(EOF);

        stream->_ptr++;
    }

    if (stream->_flag & _IOSTRG) {
            /* If stream opened by sscanf do not modify buffer */
                if (*--stream->_ptr != (char)ch) {
                        ++stream->_ptr;
                        return(EOF);
                }
        } else
                *--stream->_ptr = (char)ch;

    stream->_cnt++;
    stream->_flag &= ~_IOEOF;
    stream->_flag |= _IOREAD;   /* may already be set */

    return(0xff & ch);
}
