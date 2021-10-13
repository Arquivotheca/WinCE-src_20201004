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
*read.c - read from a file handle
*
*
*Purpose:
*   defines _read() - read from a file handle
*
*******************************************************************************/

#include <cruntime.h>
#include <mtdll.h>
#include <internal.h>
#include <stdlib.h>
#include <msdos.h>
#include <coredll.h>

#define LF 10       /* line feed */
#define CR 13       /* carriage return */
#define CTRLZ 26    /* ctrl-z means eof for text */


/* Shim to allow us to redirect output to OutputDebugString */
#define __crtReadFile(h, buffer, cnt, pRead, x) \
    (((h)==_CRT_DBGOUT_HANDLE) ? (*(pRead)=0, 0) : ReadFile((h), (buffer), (cnt), (pRead), (x)))


/***
*int _read(fh, buf, cnt) - read bytes from a file handle
*
*Purpose:
*   Attempts to read cnt bytes from fh into a buffer.
*   If the file is in text mode, CR-LF's are mapped to LF's, thus
*   affecting the number of characters read.  This does not
*   affect the file pointer.
*
*   NOTE:  The stdio _IOCTRLZ flag is tied to the use of FEOFLAG.
*   Cross-reference the two symbols before changing FEOFLAG's use.
*
*Entry:
*   int fh - file handle to read from
*   char *buf - buffer to read into
*   int cnt - number of bytes to read
*
*Exit:
*   Returns number of bytes read (may be less than the number requested
*   if the EOF was reached or the file is in text mode).
*   returns -1 (and sets errno) if fails.
*
*Exceptions:
*
*******************************************************************************/

/* define normal version that locks/unlocks, validates fh */
int __cdecl _read (
    FILEX* str,
    void *buf,
    unsigned cnt
    )
{
    int bytes_read;         /* number of bytes read */
    char *buffer;           /* buffer to read to */
    int os_read;            /* bytes read on OS call */
    char *p, *q;            /* pointers into buffer */
    char peekchr = '\0';    /* peek-ahead character */

    bytes_read = 0;         /* nothing read yet */
    buffer = buf;

    if((_osfhndstr(str) == INVALID_HANDLE_VALUE) && (str->fd < 3))
    {
        OpenStdConsole(str->fd);
    }

    if ((cnt == 0) || (_osfilestr(str) & FEOFLAG))
    {
        /* nothing to read or at EOF, so return 0 read */
        return 0;
    }

    /* if device and lookahead non-empty: read the lookahead char */
    if ((_osfilestr(str) & (FPIPE|FDEV)) && (str->peekch != LF))
    {
        *buffer++ = str->peekch;
        ++bytes_read;
        --cnt;
        str->peekch = LF;       /* mark as empty */
    }

    /* read the data */
repeat:
    if (!__crtReadFile(_osfhndstr(str), buffer, cnt, (LPDWORD)&os_read,NULL))
    {
        return -1;
    }

    /* Hack for COM ports. If we read 0 bytes from a COM port, just retry */
    if((os_read == 0)
    && (str == stdin)
    && (hCOMPort != INVALID_HANDLE_VALUE)
    && (_osfhndstr(str) == hCOMPort))
    {
        DEBUGMSG (DBGSTDIO, (TEXT("Stdio: _read: Got 0 bytes from COM port. Retrying\r\n")));
        goto repeat;
    }

    bytes_read += os_read;  /* update bytes read */

    if (_osfilestr(str) & FTEXT)
    {
        /* now must translate CR-LFs to LFs in the buffer */

        /* set CRLF flag to indicate LF at beginning of buffer */
        if ((os_read != 0) && (*(char *)buf == LF))
        {
            _osfilestr(str) |= FCRLF;
        }
        else
        {
            _osfilestr(str) &= ~FCRLF;
        }

        /* convert chars in the buffer: p is src, q is dest */
        p = q = buf;
        while (p < ((char *)buf + bytes_read))
        {
            if (*p == CTRLZ)
            {
                /* if fh is not a device, set ctrl-z flag */
                if (!(_osfilestr(str) & FDEV))
                {
                    _osfilestr(str) |= FEOFLAG;
                }
                break;          /* stop translating */
            }
            else if (*p != CR)
            {
                *q++ = *p++;
            }
            else if (str==stdin && hCOMPort!=INVALID_HANDLE_VALUE && _osfhndstr(str)==hCOMPort)
            {
                /* *p is CR, and we are reading from a COM Port.
                   Here is what we want to do:
                   *p   peekchr     Store
                1- CR   other       LF (and store other for next time)
                2- CR   LF          LF
                3- CR   timeout     LF
                4- CR   fail        CR (ASSERT(0)) case */

                if (p < ((char *)buf + bytes_read - 1))
                {
                    /* case 1 - convert CR to LF */
                    *q++ = LF;
                    p++;
                    if (*p == LF)
                    {
                        /* case 2 - also skip the next LF */
                        p++;
                    }
                }
                else
                {
                    /* CR occurs at end of buffer. Read ahead to next char */
                    COMMTIMEOUTS CommTimeouts;
                    BOOL fReadFileOK = FALSE;

                    /* increment p to skip the CR */
                    p++;

                    /* decrease the timeout and check if there is another
                       character waiting to be read */
                    if (g_pfnGetCommTimeouts && g_pfnSetCommTimeouts && (*g_pfnGetCommTimeouts)(hCOMPort, &CommTimeouts))
                    {
                        DWORD dwTimeoutSaved = CommTimeouts.ReadTotalTimeoutConstant;
                        CommTimeouts.ReadTotalTimeoutConstant = 100;
                        (*g_pfnSetCommTimeouts)(hCOMPort, &CommTimeouts);
                        fReadFileOK = __crtReadFile(_osfhndstr(str), &peekchr, 1,(LPDWORD)&os_read, NULL);
                        CommTimeouts.ReadTotalTimeoutConstant = dwTimeoutSaved;
                        (*g_pfnSetCommTimeouts)(hCOMPort, &CommTimeouts);
                    }

                    if (!fReadFileOK)
                    {
                        /* case 4 - CR fail*/
                        *q++ = CR;
                        ASSERT(0);
                    }
                    else
                    {
                        /* case 1, 2, 3 - store LF */
                        *q++ = LF;
                        if ((os_read > 0) && (peekchr != LF))
                        {
                            /* case 1 - also store peekchr for next time */
                            str->peekch = peekchr;
                        }
                    }
                }
            }
            else
            {
                /* *p is CR, so must check next char for LF */
                if (p < (char *)buf + bytes_read - 1)
                {
                    if (*(p+1) == LF)
                    {
                        p += 2;
                        *q++ = LF;  /* convert CR-LF to LF */
                    }
                    else
                    {
                        *q++ = *p++;    /* store char normally */
                    }
                }
                else
                {
                    /* This is the hard part.  We found a CR at end of
                       buffer.  We must peek ahead to see if next char
                       is an LF. */
                    ++p;

                    if (!__crtReadFile(_osfhndstr(str), &peekchr, 1, (LPDWORD)&os_read, NULL) || (os_read == 0))
                    {
                        /* couldn't read ahead, store CR */
                        *q++ = CR;
                    }
                    else
                    {
                        /* peekchr now has the extra character -- we now have
                           several possibilities:
                        1. disk file and char is not LF; just seek back and copy CR
                        2. disk file and char is LF; seek back and discard CR
                        3. disk file, char is LF but this is a one-byte read:
                           store LF, don't seek back
                        4. pipe/device and char is LF; store LF.
                        5. pipe/device and char isn't LF, store CR and put
                           char in pipe lookahead buffer. */
                        if (_osfilestr(str) & (FDEV|FPIPE))
                        {
                            /* non-seekable device */
                            if (peekchr == LF)
                            {
                                *q++ = LF;
                            }
                            else
                            {
                                *q++ = CR;
                                str->peekch = peekchr;
                            }
                        }
                        else
                        {
                            /* disk file */
                            if ((q == buf) && (peekchr == LF))
                            {
                                /* nothing read yet; must make some progress */
                                *q++ = LF;
                            }
                            else
                            {
                                /* seek back */
                                _lseek(str, -1, FILE_CURRENT);
                                if (peekchr != LF)
                                {
                                    *q++ = CR;
                                }
                            }
                        }
                    }
                }
            }
        }

        /* we now change bytes_read to reflect the true number of chars
           in the buffer */
        bytes_read = q - (char *)buf;
    }

    return bytes_read;          /* and return */
}

