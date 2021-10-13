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
*Purpose:
*       Read from the specified stream into the user's buffer.
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <mtdll.h>
#include <string.h>
#include <limits.h>
#include <internal.h>
#include <file2.h>
#include <dbgint.h>

/***
*size_t fread_s(void *buffer, size_t bufferSize, size_t elementSize, size_t count, FILE *stream) -
*   read from specified stream into the specified buffer.
*
*size_t fread(void *buffer, size_t elementSize, size_t count, FILE *stream) -
*   read from specified stream into the specified buffer.
*
*Purpose:
*   Read 'count' items of size 'elementSize' from the specified stream into
*   the specified buffer. Return when 'count' items have been read in
*   or no more items can be read from the stream.
*
*Entry:
*   buffer  - pointer to user's buffer
*   bufferSize - size of the destination buffer (in bytes)
*   elementSize - size of the item to read in (in bytes)
*   count   - number of items to read
*   stream  - stream to read from
*
*Exit:
*   Returns the number of (whole) items that were read into the buffer.
*   This may be less than 'count' if an error or eof occurred. In this
*   case, ferror() or feof() should be used to distinguish between the
*   two conditions.
*
*Notes:
*   fread will attempt to buffer the stream (side effect of the _filbuf
*   call) if necessary.
*
*   No more than 0xFFFE bytes may be read in at a time by a call to
*   read(). Further, read() does not handle huge buffers. Therefore,
*   in large data models, the read request is broken down into chunks
*   that do not violate these considerations. Each of these chunks is
*   processed much like an fread() call in a small data model (by a
*   call to _read()).
*
*******************************************************************************/

#ifdef _DEBUG
#define _BUFFER_FILL_PATTERN _SECURECRT_FILL_BUFFER_PATTERN
#else
#define _BUFFER_FILL_PATTERN 0
#endif

/* define the normal version */
static size_t __cdecl _fread_nolock_s(
    void *buffer,
    size_t bufferSize,
    size_t elementSize,
    size_t num,
    FILEX *stream
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

    ASSERT(elementSize != 0);
    ASSERT(num != 0);

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

            /* calc chars to read -- (count/streambufsize) * streambufsize */
            nbytes = (streambufsize ? (unsigned)(count - count % streambufsize) :
                       (unsigned)count);
            if (nbytes > dataSize)
            {
                if (bufferSize != SIZE_MAX)
                {
                    memset(buffer, _BUFFER_FILL_PATTERN, bufferSize);
                }
                _VALIDATE_RETURN(("buffer too small", 0), ERANGE, 0)
            }

            nread = _read(stream, data, nbytes);
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

/* define locking/unlocking version */
size_t __cdecl fread_s(
    void *buffer,
    size_t bufferSize,
    size_t elementSize,
    size_t count,
    FILEX *stream
)
{
    size_t retval = 0;

    if (elementSize == 0 || count == 0)
    {
        return 0;
    }

    /* validation */
    _VALIDATE_RETURN((buffer != NULL), EINVAL, 0);
    if (stream == NULL || count > (SIZE_MAX / elementSize))
    {
        if (bufferSize != SIZE_MAX)
        {
            memset(buffer, _BUFFER_FILL_PATTERN, bufferSize);
        }

        _VALIDATE_RETURN((stream != NULL), EINVAL, 0);
        _VALIDATE_RETURN(count <= (SIZE_MAX / elementSize), EINVAL, 0);
    }

    if (!_lock_validate_str(stream))                 /* lock stream */
        return 0;

    __STREAM_TRY
    {
        /* do the read; _fread_nolock_s will make sure we do not buffer overrun */
        retval = _fread_nolock_s(buffer, bufferSize, elementSize, count, stream);
    }
    __STREAM_FINALLY
    {
        _unlock_str(stream);
    }

    return retval;
}

size_t __cdecl fread(
    void *buffer,
    size_t elementSize,
    size_t count,
    FILEX *stream
)
{
    /* assumes there is enough space in the destination buffer */
    return fread_s(buffer, SIZE_MAX, elementSize, count, stream);
}

