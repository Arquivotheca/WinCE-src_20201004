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
*fread.c - read from a stream
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Read from the specified stream into the user's buffer.
*
*Revision History:
*       06-23-89  PHG   Module created, based on asm version
*       02-15-90  GJF   _iob[], _iob2[] merge. Also, fixed copyright and
*                       indents.
*       03-19-90  GJF   Made calling type _CALLTYPE1 and added #include
*                       <cruntime.h>.
*       08-14-90  SBM   Compiles cleanly with -W3
*       10-02-90  GJF   New-style function declarators.
*       01-22-91  GJF   ANSI naming.
*       03-27-92  DJM   POSIX support.
*       06-22-92  GJF   Must return 0 if EITHER size-of-item or number-of-
*                       item arguments is 0 (TNT Bug #523)
*       08-26-92  GJF   Include unistd.h for POSIX build.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       05-11-93  GJF   Replaced BUFSIZ with _INTERNAL_BUFSIZ.
*       10-22-93  GJF   Fix divide-by-0 error in unbuffered case. Also,
*                       replaced MTHREAD with _MT.
*       12-30-94  GJF   _MAC_ merge.
*       03-07-95  GJF   _[un]lock_str macros now take FILE * arg.
*       03-02-98  GJF   Exception-safe locking.
*       01-04-99  GJF   Changes for 64-bit size_t.
*       05-17-99  PML   Remove all Macintosh support.
*       11-05-02  SJ    Checking for stream->_cnt <0 which causes crash.
*                       Added a check & an ASSERT (vsWhidbey#2400)
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       04-05-05  AC    Add fread_s and _fread_nolock_s
*                       VSW#434916
*       02-15-05  ATC   vsw578341.  Enable the parameter validation
*                       in _fread_nolock_s()
*       03-30-06  ATC   vsw585945.  Check fread() so it calls _read()
*                       in 2GB chunks.
*
*******************************************************************************/

#include <cruntime.h>
#ifdef  _POSIX_
#include <unistd.h>
#endif
#include <stdio.h>
#include <mtdll.h>
#include <io.h>
#include <string.h>
#include <limits.h>
#include <internal.h>
#include <file2.h>
#include <dbgint.h>

/***
*size_t fread_s(void *buffer, size_t bufferSize, size_t elementSize, size_t count, FILE *stream) -
*       read from specified stream into the specified buffer.
*
*size_t fread(void *buffer, size_t elementSize, size_t count, FILE *stream) -
*       read from specified stream into the specified buffer.
*
*Purpose:
*       Read 'count' items of size 'elementSize' from the specified stream into
*       the specified buffer. Return when 'count' items have been read in
*       or no more items can be read from the stream.
*
*Entry:
*       buffer  - pointer to user's buffer
*       bufferSize - size of the destination buffer (in bytes)
*       elementSize - size of the item to read in (in bytes)
*       count   - number of items to read
*       stream  - stream to read from
*
*Exit:
*       Returns the number of (whole) items that were read into the buffer.
*       This may be less than 'count' if an error or eof occurred. In this
*       case, ferror() or feof() should be used to distinguish between the
*       two conditions.
*
*Notes:
*       fread will attempt to buffer the stream (side effect of the _filbuf
*       call) if necessary.
*
*       No more than 0xFFFE bytes may be read in at a time by a call to
*       read(). Further, read() does not handle huge buffers. Therefore,
*       in large data models, the read request is broken down into chunks
*       that do not violate these considerations. Each of these chunks is
*       processed much like an fread() call in a small data model (by a
*       call to _nfread()).
*
*       MTHREAD/DLL - Handled in three layers. fread() handles the locking
*       and DS saving/loading/restoring (if required) and calls _fread_nolock()
*       to do the work. _fread_nolock() is the same as the single-thread,
*       large data model version of fread(). It breaks up the read request
*       into digestible chunks and calls _nfread() to do the actual work.
*
*       386/MTHREAD/DLL - Handled in just the two layers since it is small
*       data model. The outer layer, fread(), takes care of the stream locking
*       and calls _fread_nolock() to do the actual work. _fread_nolock() is the same
*       as the single-thread version of fread().
*
*******************************************************************************/

#ifdef _DEBUG
#define _BUFFER_FILL_PATTERN _SECURECRT_FILL_BUFFER_PATTERN
#else
#define _BUFFER_FILL_PATTERN 0
#endif

/* define locking/unlocking version */
size_t __cdecl fread_s(
    void *buffer,
    size_t bufferSize,
    size_t elementSize,
    size_t count,
    FILE *stream
)
{
    size_t retval = 0;

    if (elementSize == 0 || count == 0)
    {
        return 0;
    }


    // only checking for stream == NULL here for _lock_str()
    // the rest of the validation is in _fread_nolock_s()
    if (stream == NULL )
    {
        if (bufferSize != SIZE_MAX)
        {
            memset(buffer, _BUFFER_FILL_PATTERN, bufferSize);
        }

        _VALIDATE_RETURN((stream != NULL), EINVAL, 0);
    }

    _lock_str(stream);
    __try
    {
        /* do the read; _fread_nolock_s will make sure we do not buffer overrun */
        retval = _fread_nolock_s(buffer, bufferSize, elementSize, count, stream);
    }
    __finally
    {
        _unlock_str(stream);
    }

    return retval;
}

/* define the normal version */
size_t __cdecl _fread_nolock_s(
    void *buffer,
    size_t bufferSize,
    size_t elementSize,
    size_t num,
    FILE *stream
)
{
    char *data;                     /* point inside the destination buffer to where we need to copy the read chars */
    size_t dataSize;                /* space left in the destionation buffer (in bytes) */
    size_t total;                   /* total bytes to read */
    size_t count;                   /* num bytes left to read */
    unsigned streambufsize;         /* size of stream buffer */
    unsigned nbytes;                /* how much to read now */
    unsigned nread;                 /* how much we did read */
    int c;                          /* a temp char */

    /* initialize local vars */
    data = buffer;
    dataSize = bufferSize;

    if (elementSize == 0 || num == 0)
    {
        return 0;
    }

    /* validation */
    _VALIDATE_RETURN((buffer != NULL), EINVAL, 0);
    if (stream == NULL || num > (SIZE_MAX / elementSize))
    {
        if (bufferSize != SIZE_MAX)
        {
            memset(buffer, _BUFFER_FILL_PATTERN, bufferSize);
        }

        _VALIDATE_RETURN((stream != NULL), EINVAL, 0);
        _VALIDATE_RETURN(num <= (SIZE_MAX / elementSize), EINVAL, 0);
    }


    count = total = elementSize * num;

    if (anybuf(stream))
    {
        /* already has buffer, use its size */
        streambufsize = stream->_bufsiz;
    }
    else
    {
        /* assume will get _INTERNAL_BUFSIZ buffer */
        streambufsize = _INTERNAL_BUFSIZ;
    }

    /* here is the main loop -- we go through here until we're done */
    while (count != 0) {
        /* if the buffer exists and has characters, copy them to user
            buffer */
        if (anybuf(stream) && stream->_cnt != 0)
        {
            if(stream->_cnt < 0)
            {
                _ASSERTE(("Inconsistent Stream Count. Flush between consecutive read and write", stream->_cnt >= 0));
                stream->_flag |= _IOERR;
                return (total - count) / elementSize;
            }

            /* how much do we want? */
            nbytes = (count < (size_t)stream->_cnt) ? (unsigned)count : stream->_cnt;
            if (nbytes > dataSize)
            {
                if (bufferSize != SIZE_MAX)
                {
                    memset(buffer, _BUFFER_FILL_PATTERN, bufferSize);
                }
                _VALIDATE_RETURN(("buffer too small", 0), ERANGE, 0)
            }
            memcpy_s(data, dataSize, stream->_ptr, nbytes);

            /* update stream and amt of data read */
            count -= nbytes;
            stream->_cnt -= nbytes;
            stream->_ptr += nbytes;
            data += nbytes;
            dataSize -= nbytes;
        }
        else if (count >= streambufsize)
        {
            /* If we have more than streambufsize chars to read, get data
                by calling read with an integral number of bufsiz
                blocks.  Note that if the stream is text mode, read
                will return less chars than we ordered. */

            if (streambufsize)
            {
                /* In 64bit apps size_t is bigger than unsigned 
                 * (which is 32bit even in 64 bit machines), so
                 * we need to split the read into INT_MAX chunks
                 * since _read() only support up to _signed_ int 
                 * (even though the in parameter is unsigned).
                 */
                if (count > INT_MAX)
                {
                    /* calc chars to read -- the largest multiple of streambufsize 
                     * smaller then INT_MAX
                     */
                    nbytes = (unsigned)(INT_MAX - INT_MAX % streambufsize);
                }
                else
                {
                    /* calc chars to read -- (count/streambufsize) * streambufsize */
                    nbytes = (unsigned)(count - count % streambufsize);
                }
            }
            else
            {
                nbytes = (count > INT_MAX)?(unsigned)INT_MAX: (unsigned)count;
            }

            if (nbytes > dataSize)
            {
                if (bufferSize != SIZE_MAX)
                {
                    memset(buffer, _BUFFER_FILL_PATTERN, bufferSize);
                }
                _VALIDATE_RETURN(("buffer too small", 0), ERANGE, 0)
            }

#ifdef  _POSIX_
            nread = read(fileno(stream), data, nbytes);
#else
            nread = _read_nolock(_fileno(stream), data, nbytes);
#endif
            if (nread == 0) {
                    /* end of file -- out of here */
                    stream->_flag |= _IOEOF;
                    return (total - count) / elementSize;
            }
            else if (nread == (unsigned)-1) {
                    /* error -- out of here */
                    stream->_flag |= _IOERR;
                    return (total - count) / elementSize;
            }

            /* update count and data to reflect read */
            count -= nread;
            data += nread;
            dataSize -= nread;
        }
        else
        {
            /* less than streambufsize chars to read, so call _filbuf to
                fill buffer */
            if ((c = _filbuf(stream)) == EOF) {
                    /* error or eof, stream flags set by _filbuf */
                    return (total - count) / elementSize;
            }

            /* _filbuf returned a char -- store it */
            if (dataSize == 0)
            {
                if (bufferSize != SIZE_MAX)
                {
                    memset(buffer, _BUFFER_FILL_PATTERN, bufferSize);
                }
                _VALIDATE_RETURN(("buffer too small", 0), ERANGE, 0)
            }
            *data++ = (char) c;
            --count;
            --dataSize;

            /* update buffer size */
            streambufsize = stream->_bufsiz;
        }
    }

    /* we finished successfully, so just return num */
    return num;
}

size_t __cdecl fread(
    void *buffer,
    size_t elementSize,
    size_t count,
    FILE *stream
)
{
    /* assumes there is enough space in the destination buffer */
    return fread_s(buffer, SIZE_MAX, elementSize, count, stream);
}

size_t __cdecl _fread_nolock(
    void *buffer,
    size_t elementSize,
    size_t count,
    FILE *stream
)
{
    /* assumes there is enough space in the destination buffer */
    return _fread_nolock_s(buffer, SIZE_MAX, elementSize, count, stream);
}
