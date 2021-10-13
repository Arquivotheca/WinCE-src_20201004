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
*_filbuf.c - fill buffer and get character
*
*
*Purpose:
*   defines _filbuf() - fill buffer and read first character, allocate
*   buffer if there is none.  Used from getc().
*   defines _filwbuf() - fill buffer and read first wide character, allocate
*   buffer if there is none.  Used from getwc().
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <file2.h>
#include <dbgint.h>
#include <malloc.h>
#include <internal.h>
#include <msdos.h>
#include <mtdll.h>

/***
*int _filbuf(stream) - fill buffer and get first character
*
*Purpose:
*   get a buffer if the file doesn't have one, read into it, return first
*   char. try to get a buffer, if a user buffer is not assigned. called
*   only from getc; intended for use only within library. assume no input
*   stream is to remain unbuffered when memory is available unless it is
*   marked _IONBF. at worst, give it a single char buffer. the need for a
*   buffer, no matter how small, becomes evident when we consider the
*   ungetc's necessary in scanf
*
*   [NOTE: Multi-thread - _filbuf() assumes that the caller has aquired
*   the stream lock, if needed.]
*
*Entry:
*   FILEX *stream - stream to read from
*
*Exit:
*   returns first character from buffer (next character to be read)
*   returns EOF if the FILEX is actually a string, or not open for reading,
*   or if open for writing or if no more chars to read.
*   all fields in FILEX structure may be changed except _file.
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _real_filbuf2(FILEX *str, int fWide)
{
    REG1 FILEX *stream;
    _ASSERTE(str != NULL);


    /* Init pointer to _iob2 entry. */
    stream = str;
    if (!inuse(stream) || stream->_flag & _IOSTRG)
        goto reteof;

    if (stream->_flag & _IOWRT)
    {
        stream->_flag |= _IOERR;
        goto reteof;
    }

    stream->_flag |= _IOREAD;

    /* Get a buffer, if necessary. */
    if (!anybuf(stream))
        _getbuf(stream);
    else
        stream->_ptr = stream->_base;

    stream->_cnt = _read(stream, stream->_base, stream->_bufsiz);

    if ((stream->_cnt == 0) || (stream->_cnt == -1) || (fWide && (stream->_cnt == 1)))
    {
        stream->_flag |= stream->_cnt ? _IOERR : _IOEOF;
        stream->_cnt = 0;
        goto reteof;
    }

    if (  !(stream->_flag & (_IOWRT|_IORW)) && ((_osfilestr(stream) & (FTEXT|FEOFLAG)) == (FTEXT|FEOFLAG)) )
        stream->_flag |= _IOCTRLZ;
    /* Check for small _bufsiz (_SMALL_BUFSIZ). If it is small and
       if it is our buffer, then this must be the first _filbuf after
       an fseek on a read-access-only stream. Restore _bufsiz to its
       larger value (_INTERNAL_BUFSIZ) so that the next _filbuf call,
       if one is made, will fill the whole buffer. */
    if ( (stream->_bufsiz == _SMALL_BUFSIZ) && (stream->_flag &
          _IOMYBUF) && !(stream->_flag & _IOSETVBUF) )
    {
        stream->_bufsiz = _INTERNAL_BUFSIZ;
    }

    if(fWide)
    {
        stream->_cnt -= sizeof(wchar_t);
#pragma warning(suppress: 4213) // nonstandard extension used : cast on l-value
        return (0xffff & *((wchar_t *)(stream->_ptr))++);
    }
    else
    {
        stream->_cnt--;
        return (0xff & *stream->_ptr++);
    }

reteof:
    return (fWide ? WEOF : EOF);
}

/***
*int _flsbuf(ch, stream) - flush buffer and output character.
*
*Purpose:
*   flush a buffer if this stream has one. if not, try to get one. put the
*   next output char (ch) into the buffer (or output it immediately if this
*   stream can't have a buffer). called only from putc. intended for use
*   only within library.
*
*   [NOTE: Multi-thread - It is assumed that the caller has aquired
*   the stream lock.]
*
*Entry:
*   FILEX *stream - stream to flish and write on
*   int ch - character to output.
*
*Exit:
*   returns -1 if FILEX is actually a string, or if can't write ch to
*   unbuffered file, or if we flush a buffer but the number of chars
*   written doesn't agree with buffer size.  Otherwise returns ch.
*   all fields in FILEX struct can be affected except _file.
*
*Exceptions:
*
*******************************************************************************/



int __cdecl _real_flsbuf2(int ch, FILEX *str, int fWide)



{
    REG1 FILEX *stream;
    REG2 int charcount;
    REG3 int written;
    int csize = (fWide ? sizeof(WCHAR) : sizeof(char));

    _ASSERTE(str != NULL);

    /* Init file handle and pointers */
    stream = str;

    if (!(stream->_flag & (_IOWRT|_IORW)) || (stream->_flag & _IOSTRG))
    {
        stream->_flag |= _IOERR;
        goto reteof;
    }

    /* Check that _IOREAD is not set or, if it is, then so is _IOEOF. Note
       that _IOREAD and IOEOF both being set implies switching from read to
       write at end-of-file, which is allowed by ANSI. Note that resetting
       the _cnt and _ptr fields amounts to doing an fflush() on the stream
       in this case. Note also that the _cnt field has to be reset to 0 for
       the error path as well (i.e., _IOREAD set but _IOEOF not set) as
       well as the non-error path. */

    if (stream->_flag & _IOREAD) {
        stream->_cnt = 0;
        if (stream->_flag & _IOEOF) {
            stream->_ptr = stream->_base;
            stream->_flag &= ~_IOREAD;
        }
        else {
            stream->_flag |= _IOERR;
            goto reteof;
        }
    }

    stream->_flag |= _IOWRT;
    stream->_flag &= ~_IOEOF;
    written = charcount = stream->_cnt = 0;

    /* Get a buffer for this stream, if necessary. */
    if (!anybuf(stream)) {

        /* Do NOT get a buffer if (1) stream is stdout/stderr, and
           (2) stream is NOT a tty.
           [If stdout/stderr is a tty, we do NOT set up single char
           buffering. This is so that later temporary buffering will
           not be thwarted by the _IONBF bit being set (see
           _stbuf/_ftbuf usage).]
        */
        if (!( ((stream==stdout) || (stream==stderr)) && (_isatty(stream)) ))
            _getbuf(stream);

    } /* end !anybuf() */

    /* If big buffer is assigned to stream... */
    if (bigbuf(stream))
    {
        _ASSERTE(("inconsistent IOB fields", stream->_ptr - stream->_base >= 0));

        charcount = stream->_ptr - stream->_base;
        stream->_ptr = stream->_base + csize;
        stream->_cnt = stream->_bufsiz - csize;

        if (charcount > 0)
            written = _write(stream, stream->_base, charcount);
        else
            if (_osfilestr(stream) & FAPPEND)
                _lseek(stream,0L,SEEK_END);

        if(fWide)
            *(wchar_t *)(stream->_base) = (wchar_t)(ch & 0xffff);
        else
            *stream->_base = (char)ch;
    }

    /* Perform single character output (either _IONBF or no buffering) */
    else {
        charcount = csize;
        written = _write(stream, &ch, charcount);
        /***
        {   char mbc[4];
            *(wchar_t *)mbc = (wchar_t)(ch & 0xffff);
            written = _write(stream, mbc, charcount);}
        ***/
    }

    /* See if the _write() was successful. */
    if (written != charcount)
    {
        stream->_flag |= _IOERR;
        goto reteof;
    }

    return (ch & (fWide ? 0xffff : 0xff));

reteof:
    // NOTE: _flswbuf should return WEOF on failure, but we can't due to BC.
    // See Windows CE .NET 134886
    return EOF;
}
