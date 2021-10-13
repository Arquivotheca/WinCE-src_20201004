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
*fflush.c - flush a stream buffer
*
*
*Purpose:
*   defines fflush() - flush the buffer on a stream
*       _flushall() - flush all stream buffers
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <file2.h>
#include <mtdll.h>
#include <internal.h>
#include <dbgint.h>


/* Values passed to flsall() to distinguish between _flushall() and
 * fflush(NULL) behavior
 */
#define FLUSHALL    1
#define FFLUSHNULL  0

/* Core routine for fflush(NULL) and flushall()
 */
static int __cdecl flsall(int);


/***
*int fflush(stream) - flush the buffer on a stream
*
*Purpose:
*   if file open for writing and buffered, flush the buffer. if problems
*   flushing the buffer, set the stream flag to error
*   Always flushes the stdio stream and forces a commit to disk if file
*   was opened in commit mode.
*
*Entry:
*   FILEX *stream - stream to flush
*
*Exit:
*   returns 0 if flushed successfully, or no buffer to flush
*   returns EOF and sets file error flag if fails.
*   FILEX struct entries affected: _ptr, _cnt, _flag.
*
*Exceptions:
*
*******************************************************************************/

int __cdecl fflush (
    REG1 FILEX *stream
    )
{
    int rc;

    /* if stream is NULL, flush all streams
     */
    if (stream == NULL)
        return(flsall(FFLUSHNULL));

    if (!_lock_validate_str(stream))
        return EOF;

    __STREAM_TRY
    {
        rc = _fflush_lk(stream);
    }
    __STREAM_FINALLY
    {
        _unlock_str(stream);
    }

    return(rc);
}


/***
*_fflush_lk() - Flush the buffer on a stream (stream is already locked)
*
*Purpose:
*   Core flush routine; assumes stream lock is held by caller.
*
*   [See fflush() above for more information.]
*
*Entry:
*   [See fflush()]
*Exit:
*   [See fflush()]
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _fflush_lk (
    REG1 FILEX *str
    )
{
    if (_flush(str) != 0)
    {
        /* _flush failed, don't attempt to commit */
        return(EOF);
    }

    /* lowio commit to ensure data is written to disk */
    if (str->_flag & _IOCOMMIT)
    {
        return (_commit(str) ? 0 : EOF);
    }
    return 0;
}


/***
*int _flush(stream) - flush the buffer on a single stream
*
*Purpose:
*   If file open for writing and buffered, flush the buffer.  If
*   problems flushing the buffer, set the stream flag to error.
*   Multi-thread version assumes stream lock is held by caller.
*
*Entry:
*   FILEX* stream - stream to flush
*
*Exit:
*   Returns 0 if flushed successfully, or if no buffer to flush.,
*   Returns EOF and sets file error flag if fails.
*   File struct entries affected: _ptr, _cnt, _flag.
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _flush (
    FILEX *str
    )
{
    REG1 FILEX *stream;
    REG2 int rc = 0; /* assume good return */
    REG3 int nchar;

    /* Init pointer to stream */
    stream = str;

    if ((stream->_flag & (_IOREAD | _IOWRT)) == _IOWRT && bigbuf(stream)
        && (nchar = stream->_ptr - stream->_base) > 0)
    {
        if ( _write(stream, stream->_base, nchar) == nchar ) {
            /* if this is a read/write file, clear _IOWRT so that
             * next operation can be a read
             */
            if ( _IORW & stream->_flag )
                stream->_flag &= ~_IOWRT;
        }
        else {
            stream->_flag |= _IOERR;
            rc = EOF;
        }
    }

    stream->_ptr = stream->_base;
    stream->_cnt = 0;

    return(rc);
}


/***
*int _flushall() - flush all output buffers
*
*Purpose:
*   flushes all the output buffers to the file, clears all input buffers.
*
*Entry:
*   None.
*
*Exit:
*   returns number of open streams
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _flushall (
    void
    )
{
    return(flsall(FLUSHALL));
}


/***
*static int flsall(flushflag) - flush all output buffers
*
*Purpose:
*   Flushes all the output buffers to the file and, if FLUSHALL is passed,
*   clears all input buffers. Core routine for both fflush(NULL) and
*   flushall().
*
*   MTHREAD Note: All the locking/unlocking required for both fflush(NULL)
*   and flushall() is performed in this routine.
*
*Entry:
*   int flushflag - flag indicating the exact semantics, there are two
*           legal values: FLUSHALL and FFLUSHNULL
*
*Exit:
*   if flushflag == FFLUSHNULL then flsbuf returns:
        0, if successful
*       EOF, if an error occurs while flushing one of the streams
*
*   if flushflag == FLUSHALL then flsbuf returns the number of streams
*   successfully flushed
*
*Exceptions:
*
*******************************************************************************/

static int __cdecl flsall (
    int flushflag
    )
{
    REG1 int i;
    int count = 0;
    int errcode = 0;

    if(!CheckStdioInit())
        return EOF;

    EnterCriticalSection(&csIobScanLock); // protects the __piob table

    for ( i = 0 ; i < _nstream ; i++ ) {

        if ( (__piob[i] != NULL) && (inuse(__piob[i])) ) {

            /*
             * lock the stream. this is not done until testing
             * the stream is in use to avoid unnecessarily creating
             * a lock for every stream. the price is having to
             * retest the stream after the lock has been asserted.
             */
            _lock_str2(i, __piob[i]);

            __STREAM_TRY
            {
                /*
                 * if the stream is STILL in use (it may have been
                 * closed before the lock was asserted), see about
                 * flushing it.
                 */
                if ( inuse(__piob[i]) ) {
                    if ( flushflag == FLUSHALL ) {
                        /*
                         * FLUSHALL functionality: fflush the read or
                         * write stream and, if successful, update the
                         * count of flushed streams
                         */
                        if ( _fflush_lk(__piob[i]) != EOF )
                            /* update count of successfully flushed
                             * streams
                             */
                            count++;
                    }
                    else if ( (flushflag == FFLUSHNULL) &&
                          (__piob[i]->_flag & _IOWRT) ) {
                        /*
                         * FFLUSHNULL functionality: fflush the write
                         * stream and kept track of the error, if one
                         * occurs
                         */
                        if ( _fflush_lk(__piob[i]) == EOF )
                            errcode = EOF;
                    }
                }
            }
            __STREAM_FINALLY
            {
                _unlock_str2(i, __piob[i]);
            }
        }
    }

    LeaveCriticalSection(&csIobScanLock);

    if ( flushflag == FLUSHALL )
        return(count);
    else
        return(errcode);
}


/***
* __delete_stream
*
* Completely remove a stream.  The csIobScanLock should be held when this is
* called.
*
*******************************************************************************/

int _deletestream(int i)
{
    int ret = 0;

    if (__piob[i] != NULL)
    {
        // if the stream is in use, close it
        if (inuse(__piob[i]) && (fclose(__piob[i]) != EOF))
        {
            ret = 1;
        }

        DeleteCriticalSection(&(__piob[i]->lock));
        _free_crt(__piob[i]);
        __piob[i] = NULL;
    }

    return ret;
}


/***
*int _fcloseall() - close all open streams
*
*Purpose:
*   Closes all streams currently open except for stdin/out/err/aux/prn.
*   tmpfile() files are among those closed.
*
*Entry:
*   None.
*
*Exit:
*   returns number of streams closed if OK
*   returns EOF if fails.
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _fcloseall (
    void
    )
{
    REG2 int count = 0;
    int i;

    if(!CheckStdioInit())
        return EOF;

    EnterCriticalSection(&csIobScanLock); // protects the __piob table

    for ( i = 3 ; i < _nstream ; i++ ) {
        count += _deletestream(i);
    }

    LeaveCriticalSection(&csIobScanLock);

    return(count);
}

