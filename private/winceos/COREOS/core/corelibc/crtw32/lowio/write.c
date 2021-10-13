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
*write.c - write to a file handle
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines _write() - write to a file handle
*
*Revision History:
*       06-14-89  PHG   Module created, based on asm version
*       03-13-90  GJF   Made calling type _CALLTYPE2 (for now), added #include
*                       <cruntime.h>, fixed compiler warnings and fixed the
*                       copyright. Also, cleaned up the formatting a bit.
*       04-03-90  GJF   Now _CALLTYPE1.
*       07-24-90  SBM   Removed '32' from API names
*       08-14-90  SBM   Compiles cleanly with -W3
*       10-01-90  GJF   New-style function declarators.
*       12-04-90  GJF   Appended Win32 version onto source with #ifdef-s.
*                       Should come back latter and do a better merge.
*       12-04-90  SRW   Changed to include <oscalls.h> instead of <doscalls.h>
*       12-06-90  SRW   Changed to use _osfile and _osfhnd instead of _osfinfo
*       12-28-90  SRW   Added _CRUISER_ conditional around check_stack pragma
*       12-28-90  SRW   Added cast of void * to char * for Mips C Compiler
*       01-17-91  GJF   ANSI naming.
*       02-25-91  MHL   Adapt to ReadFile/WriteFile changes (_WIN32_)
*       04-09-91  PNT   Added _MAC_ conditional
*       07-18-91  GJF   Removed unreferenced local variable from __write_lk
*                       routine [_WIN32_].
*       10-24-91  GJF   Added LPDWORD casts to make MIPS compiler happy.
*                       ASSUMES THAT sizeof(int) == sizeof(DWORD).
*       02-13-92  GJF   Replaced _nfile by _nhandle for Win32.
*       02-15-92  GJF   Increased BUF_SIZE and simplified LF translation code
*                       for Win32.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       09-06-94  CFW   Remove Cruiser support.
*       09-06-94  CFW   Replace MTHREAD with _MT.
*       12-03-94  SKS   Clean up OS/2 references
*       01-04-95  GJF   _WIN32_ -> _WIN32
*       02-15-95  GJF   Appended Mac version of source file (somewhat cleaned
*                       up), with appropriate #ifdef-s.
*       06-12-95  GJF   Changed _osfile[] and _osfhnd[] to _osfile() and
*                       _osfhnd(), which reference __pioinfo[].
*       06-27-95  GJF   Added check that the file handle is open.
*       07-09-96  GJF   Replaced defined(_WIN32) with !defined(_MAC) and
*                       defined(_M_M68K) || defined(_M_MPPC) with 
*                       defined(_MAC). Also, detab-ed and cleaned up the 
*                       format a bit.
*       12-30-97  GJF   Exception-safe locking.
*       03-03-98  RKP   Forced number of bytes written to always be an int
*       05-17-99  PML   Remove all Macintosh support.
*       11-10-99  GB    Replaced lseek for lseeki64 so as to able to append
*                       files longer than 4GB
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-14-03  AC    Added special case for fileno == -1
*       11-13-03  SJ    File Handle checks made consistent - VSW#199372
*       12-02-03  SJ    Reroute Unicode I/O
*       03-16-04  SSM   Fix bug in UTF8 handling. Wrong variable was being
*                       updated in the loop.
*       04-01-04  GB    Fixed the bug where we output to console in different
*                       code page from the code page of console.
*       09-14-04  PL    Fixed VSW 259369, corrected the check for odd byte count
*                       when text mode is UTF8 or UTF16LE.
*       10-18-04  JL    Fix detection for writing to console
*       10-22-04  AGH   Don't convert unicode output to console to 
*                       ANSI multibyte.
*       10-31-04  AC    Prefast fixes
*                       VSW#373224
*       03-03-06  SSM   Moved Common file related macros to internal.h
*       08-10-06  ATC   DevDivBugs#12336: Added code to handle the user calling _write()
*                       byte by byte when outputing to console in a
*                       non-C locale
*       09-04-06  WL    DevDivBugs#23854: Changed to increase the counter charcount by 2 
*                       if tmode is UNICODE
*       11-10-06  PMB   Removed most _INTEGRAL_MAX_BITS #ifdefs
*                       DDB#11174
*       01-31-07  SRA   Increase BUF_SIZE so that default settings for text mode
*                       usually call WriteFile once.  The savings show up in
*                       kernel time.
*
*******************************************************************************/

#include <cruntime.h>
#include <oscalls.h>
#include <ctype.h>
#include <io.h>
#include <errno.h>
#include <msdos.h>
#include <mtdll.h>
#include <stdlib.h>
#include <string.h>
#include <internal.h>
#include <setlocal.h>
#include <locale.h>
#include <wchar.h>

#ifndef _WIN32_WCE
#define BUF_SIZE    5*1024    /* size of LF translation buffer */
                              /* default buffer is 4K, plus extra for LFs */
#else
#define BUF_SIZE    1024      /* size of LF translation buffer */
                              /* default buffer is smaller in CE because
                                 of CE's limited stack space. */
#endif  /* _WIN32_WCE */


/***
*refresh_std_handles - std handles may have been changed, ensure we use the correct ones
*/

#ifndef _WIN32_WCE // CE doesn't need to refresh the stdio handles.
static void refresh_std_handles(int fh)
{
    ioinfo *pio;
    if (fh == 1 || fh == 2)
    {
        if (pio = __pioinfo[0] + fh)
            pio->osfhnd = (intptr_t)GetStdHandle(fh == 1 ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE);
    }
}
#endif

/***
*int _write(fh, buf, cnt) - write bytes to a file handle
*
*Purpose:
*       Writes count bytes from the buffer to the handle specified.
*       If the file was opened in text mode, each LF is translated to
*       CR-LF.  This does not affect the return value.  In text
*       mode ^Z indicates end of file.
*
*       Multi-thread notes:
*       (1) _write() - Locks/unlocks file handle
*           _write_nolock() - Does NOT lock/unlock file handle
*
*Entry:
*       int fh - file handle to write to
*       char *buf - buffer to write from
*       unsigned int cnt - number of bytes to write
*
*Exit:
*       returns number of bytes actually written.
*       This may be less than cnt, for example, if out of disk space.
*       returns -1 (and set errno) if fails.
*
*Exceptions:
*
*******************************************************************************/

/* define normal version that locks/unlocks, validates fh */
int __cdecl _write (
        int fh,
        const void *buf,
        unsigned cnt
        )
{
        int r;                          /* return value */

        /* validate handle */
        _CHECK_FH_CLEAR_OSSERR_RETURN( fh, EBADF, -1 );
        _CHECK_IO_INIT(-1);
        _VALIDATE_CLEAR_OSSERR_RETURN((fh >= 0 && (unsigned)fh < (unsigned)_nhandle), EBADF, -1);
        _VALIDATE_CLEAR_OSSERR_RETURN((_osfile(fh) & FOPEN), EBADF, -1);


        _lock_fh(fh);                   /* lock file */
#ifdef _WIN32_WCE
        if (fh < 3 && _osfhnd(fh) == (intptr_t)INVALID_HANDLE_VALUE) {
            BOOL is_dev = TRUE;
            _osfhnd(fh) = (intptr_t)OpenStdConsole(fh, &is_dev);
            if (!is_dev)
                _osfile(fh) &= (char)~FDEV;
        }
#endif

        __try {
                if ( _osfile(fh) & FOPEN )
                        r = _write_nolock(fh, buf, cnt);    /* write bytes */
                else {
                        errno = EBADF;
                        _doserrno = 0;  /* not o.s. error */
                        r = -1;
                        _ASSERTE(("Invalid file descriptor. File possibly closed by a different thread",0));                        
                }
        }
        __finally {
                _unlock_fh(fh);         /* unlock file */
        }

        return r;
}


static __inline
BOOL
__crtWriteFile(
    HANDLE h,
    LPCVOID buffer,
    DWORD cnt,
    LPDWORD pWrote,
    LPOVERLAPPED x
    )
{
#ifdef _WIN32_WCE
    if (h == _CRT_DBGOUT_HANDLE)
    {
        wchar_t *write_buf = (wchar_t*)malloc(sizeof(wchar_t)*(cnt+1));
        if (write_buf)
        {
            if(MultiByteToWideChar(CP_ACP, (DWORD)0, (LPCSTR)buffer, cnt, (LPWSTR)write_buf, cnt+1))
            {
                write_buf[cnt] = '\0';
                OutputDebugStringW(write_buf);
            }
            else
            {
                OutputDebugStringW(L"ERROR: Failed to Convert to wide char in __crtWriteFile\n\r");
            }
            free(write_buf);
            *pWrote = cnt;
        }
        else
        {
            *pWrote = 0;
        }
        return TRUE;
    }
    else
    {
        return WriteFile(h, buffer, cnt, pWrote, x);
    }
#else
    return WriteFile(h, buffer, cnt, pWrote, x);
#endif
}

/* now define version that doesn't lock/unlock, validate fh */
int __cdecl _write_nolock (
        int fh,
        const void *buf,
        unsigned cnt
        )
{
        int lfcount;            /* count of line feeds */
        int charcount;          /* count of chars written so far */
        int written;            /* count of chars written on this write */
        ULONG dosretval;        /* o.s. return value */
        char tmode ;            /* textmode - ANSI or UTF-16 */
        BOOL toConsole = 0;     /* true when writing to console */
        BOOL isCLocale = 0;     /* true when locale handle is C locale */

        lfcount = charcount = 0;        /* nothing written yet */

        if (cnt == 0)
                return 0;               /* nothing to do */

        _VALIDATE_CLEAR_OSSERR_RETURN( (buf != NULL), EINVAL, -1 );

        _CHECK_IO_INIT(-1);

#ifndef _WIN32_WCE // CE doesn't need to refresh the stdio handles.
        refresh_std_handles(fh);
#endif 

        tmode = _textmode(fh);
        
        if(tmode == __IOINFO_TM_UTF16LE ||
                tmode == __IOINFO_TM_UTF8)
        {
            /* For a UTF-16 file, the count must always be an even number */            
            _VALIDATE_CLEAR_OSSERR_RETURN(((cnt & 1) == 0), EINVAL, -1);
        }

        if (_osfile(fh) & FAPPEND) {
                /* appending - seek to end of file; ignore error, because maybe
                   file doesn't allow seeking */
                (void)_lseeki64_nolock(fh, 0, FILE_END);
        }

        /* check for text mode with LF's in the buffer */

        /*
         * Note that in case the handle belongs to Console, write file will
         * generate garbage output. For user to print these characters
         * correctly, we will need to print ANSI.
         *
         * Also note that in case of printing to Console, we still have to
         * convert the characters to console codepage.
         */

#ifndef _WIN32_WCE
        if (_isatty(fh) && (_osfile(fh) & FTEXT))
        {
            DWORD dwMode;
            _ptiddata ptd = _getptd();
            isCLocale = (ptd->ptlocinfo->locale_name[LC_CTYPE] == NULL);
            toConsole = GetConsoleMode((HANDLE)_osfhnd(fh), &dwMode);
        }

        /* don't need double conversion if it's ANSI mode C locale */
        if (toConsole && !(isCLocale && (tmode == __IOINFO_TM_ANSI))) {
            UINT consoleCP = GetConsoleCP();
            char mboutbuf[MB_LEN_MAX];
            wchar_t tmpchar;
            int size = 0;
            int written = 0;
            char *pch;

            for (pch = (char *)buf; (unsigned)(pch - (char *)buf) < cnt; ) {
                BOOL bCR;

                if (tmode == __IOINFO_TM_ANSI) {
                    bCR = *pch == LF;
                    /*
                     * Here we need to do double convert. i.e. convert from
                     * multibyte to unicode and then from unicode to multibyte in
                     * Console codepage.
                     */

                    /*
                     * Here, we have take into account that _write() might be called
                     * byte by byte, so when we see a lead byte without a trail byte, 
                     * we have to store it and return no error.
                     */
                    if ( _dbcsBufferUsed(fh) ) {
                        /*
                         * we got something buffered, join it with the lead byte
                         * and convert
                         */
                        _ASSERTE(isleadbyte(_dbcsBuffer(fh)));
                        mboutbuf[0]=_dbcsBuffer(fh);
                        mboutbuf[1] = *pch;
                        /* reseting the flag */
                        _dbcsBufferUsed(fh) = FALSE;

                        if (mbtowc(&tmpchar, mboutbuf, 2) == -1) {
                            break;
                        }
                    } else {
                        if (isleadbyte(*pch)) {
                            if ((cnt - (pch - (char*)buf)) > 1) {
                                // and we have more bytes to read, just convert
                                if (mbtowc(&tmpchar, pch, 2) == -1) {
                                    break;
                                }
                                /*
                                 * Increment pch to accomodate DBCS character.
                                 */
                                ++pch;
                            } else {
                                /* and we ran out of bytes to read, buffer the lead byte */
                                _dbcsBuffer(fh) = *pch;
                                _dbcsBufferUsed(fh) = TRUE;

                                /* lying that we actually wrote the last character, 
                                 * so it doesn't error
                                 */
                                charcount++;
                                break;
                            }
                        } else {
                            // single char conversion
                            if (mbtowc(&tmpchar, pch, 1) == -1) {
                                break;
                            }
                        }
                    }
                    ++pch;
                } else if (tmode == __IOINFO_TM_UTF8 || tmode == __IOINFO_TM_UTF16LE) {
                    /*
                     * Note that bCR set above is not valid in case of UNICODE
                     * stream. We need to set it using unicode character.
                     */
                    tmpchar = *(wchar_t *)pch;
                    bCR = tmpchar == LF;
                    pch += 2;
                }

                if (tmode == __IOINFO_TM_ANSI) 
                {
                    if( (size = WideCharToMultiByte(consoleCP,
                                                    0,
                                                    &tmpchar,
                                                    1,
                                                    mboutbuf,
                                                    sizeof(mboutbuf),
                                                    NULL,
                                                    NULL)) == 0) {
                        break;
                    } else {
                        if ( __crtWriteFile( (HANDLE)_osfhnd(fh),
                                        mboutbuf,
                                        size,
                                        (LPDWORD)&written,
                                        NULL) ) {
                            /* When we are converting, some convertion can result in
                             * 2 mbcs char -> 1 wchar -> 1 mbcs 
                             * (ie. printing out Japanese characters in English ConsoleCP,
                             * the Japanese char will be converted to a single question mark)
                             * Therefore, we want to use how many bytes we converted + lfcount
                             * instead of how many bytes we actually wrote 
                             */
                            charcount = lfcount + (int)(pch - (char*) buf);
                            if (written < size)
                                break;
                        } else {
                            dosretval = GetLastError();
                            break;
                        }
                    }
                    
                    if (bCR) {
                        size = 1;
                        mboutbuf[0] = CR;
                        if (__crtWriteFile((HANDLE)_osfhnd(fh),
                                      mboutbuf,
                                      size,
                                      (LPDWORD)&written,
                                      NULL) ) {
                            if (written < size)
                                break;
                            lfcount ++;
                            charcount++;
                        } else {
                            dosretval = GetLastError();
                            break;
                        }
                    }
                }
                else if ( tmode == __IOINFO_TM_UTF8 || tmode == __IOINFO_TM_UTF16LE) 
                {
                    if ( _putwch_nolock(tmpchar) == tmpchar )
                    {
                        charcount+=2;
                    }
                    else
                    {
                        dosretval = GetLastError();
                        break;
                    }
                    if (bCR) /* emit carriage return */
                    {
                        size = 1;
                        tmpchar = CR;
                        if ( _putwch_nolock(tmpchar) == tmpchar )
                        {
                            charcount++;
                            lfcount++;
                        }
                        else
                        {
                            dosretval = GetLastError();
                            break;
                        }
                    }                       
                }
            }
        } else
#endif // _WIN32_WCE
        if ( _osfile(fh) & FTEXT ) {
            /* text mode, translate LF's to CR/LF's on output */

            dosretval = 0;          /* no OS error yet */

            if(tmode == __IOINFO_TM_ANSI) {
                char ch;                    /* current character */
                char *p = NULL, *q = NULL;  /* pointers into buf and lfbuf resp. */
                char lfbuf[BUF_SIZE];
                p = (char *)buf;        /* start at beginning of buffer */                
                while ( (unsigned)(p - (char *)buf) < cnt ) {
                    q = lfbuf;      /* start at beginning of lfbuf */

                    /* fill the lf buf, except maybe last char */
                    while ( q - lfbuf < sizeof(lfbuf) - 1 &&
                            (unsigned)(p - (char *)buf) < cnt ) {
                        ch = *p++;
                        if ( ch == LF ) {
                            ++lfcount;
                            *q++ = CR;
                        }
                        *q++ = ch;
                    }

                    /* write the lf buf and update total */
                    if ( __crtWriteFile( (HANDLE)_osfhnd(fh),
                                lfbuf,
                                (int)(q - lfbuf),
                                (LPDWORD)&written,
                                NULL) )
                    {
                        charcount += written;
                        if (written < q - lfbuf)
                            break;
                    }
                    else {
                        dosretval = GetLastError();
                        break;
                    }
                }
            } else if ( tmode == __IOINFO_TM_UTF16LE ){
                char lfbuf[BUF_SIZE];
                wchar_t wch;            /* current wide char */
                wchar_t *pu = (wchar_t *)buf;
                wchar_t *qu = NULL;

                while ( (unsigned)((char *)pu - (char *)buf) < cnt ) {
                    qu = (wchar_t *)lfbuf; /* start at beginning of lfbuf */

                    /* fill the lf buf, except maybe last wchar_t */
                    while ( (((char *)qu - lfbuf) < (sizeof(lfbuf) - 2)) &&
                            ((unsigned)((char *)pu - (char *)buf) < cnt )) {
                        wch = *pu++;
                        if ( wch == LF ) {
                            lfcount+=2;
                            *qu++ = CR;
                        }
                        *qu++ = wch;
                    }

                    /* write the lf buf and update total */
                    if ( __crtWriteFile( (HANDLE)_osfhnd(fh),
                                lfbuf,
                                (int)((char*)qu - lfbuf),
                                (LPDWORD)&written,
                                NULL) )
                    {
                        charcount += written;
                        if (written < ((char *)qu - lfbuf))
                            break;
                    }
                    else {
                        dosretval = GetLastError();
                        break;
                    }
                }
            } else {
                /*
                 * Let's divide the lfbuf in 1:2 wher 1 is for storing
                 * widecharacters and 2 if for converting it to UTF8.  This takes
                 * into account the worst case scenario where all the UTF8
                 * characters are 4 byte long.
                 */
                char utf8_buf[(BUF_SIZE*2)/3];
                wchar_t utf16_buf[BUF_SIZE/6];

                wchar_t wch;            /* current wide char */
                wchar_t *pu = (wchar_t *)buf;
                wchar_t *qu = NULL;

                pu = (wchar_t *)buf;
                while ((unsigned)((char *)pu - (char *)buf) < cnt) {
                    int bytes_converted = 0;
                    qu = utf16_buf; /* start at beginning of lfbuf */

                    while ( (((char *)qu - (char *)utf16_buf) < 
                             (sizeof(utf16_buf) - 2)) &&
                            ((unsigned)((char *)pu - (char *)buf) < cnt )) {
                        wch = *pu++;
                        if ( wch == LF ) {
                            /* no need to count the linefeeds here: we calculate the written chars in another way */
                            *qu++ = CR;
                        }
                        *qu++ = wch;
                    } 

                    bytes_converted = WideCharToMultiByte( 
                            CP_UTF8,
                            0,
                            utf16_buf,
                            ((int)((char *)qu - (char *)utf16_buf))/2,
                            utf8_buf,
                            sizeof(utf8_buf),
                            NULL,
                            NULL);

                    if (bytes_converted == 0) {
                        dosretval = GetLastError();
                        break;
                    } else {
                        /*
                         * Here we need to make every attempt to write all the
                         * converted characters. The resaon behind this is,
                         * incase half the bytes of a UTF8 character is
                         * written, it may currupt whole of the stream or file.
                         *
                         * The loop below will make sure we exit only if all
                         * the bytes converted are written (which makes sure no
                         * partial MBCS is written) or there was some error in
                         * the stream.
                         */
                        int bytes_written = 0;
                        do {
                            if (__crtWriteFile(
                                        (HANDLE)_osfhnd(fh),
                                        utf8_buf + bytes_written,
                                        bytes_converted - bytes_written,
                                        &written,
                                        NULL)) {
                                bytes_written += written;
                            } else {
                                dosretval = GetLastError();
                                break;
                            }
                        } while ( bytes_converted > bytes_written);

                        /*
                         * Only way the condition below could be true is if
                         * there was en error. In case of error we need to
                         * break this loop as well.
                         */
                        if (bytes_converted > bytes_written) {
                            break;
                        }
                        /* if this chunk has been committed successfully, update charcount */
                        charcount = (int)((char *)pu - (char *)buf);
                    }
                }
            }
        }
        else {
                /* binary mode, no translation */
                if ( __crtWriteFile( (HANDLE)_osfhnd(fh),
                                (LPVOID)buf,
                                cnt,
                               (LPDWORD)&written,
                                NULL) )
                {
                        dosretval = 0;
                        charcount = written;
                }
                else
                        dosretval = GetLastError();
        }

        if (charcount == 0) {
                /* If nothing was written, first check if an o.s. error,
                   otherwise we return -1 and set errno to ENOSPC,
                   unless a device and first char was CTRL-Z */
                if (dosretval != 0) {
                        /* o.s. error happened, map error */
                        if (dosretval == ERROR_ACCESS_DENIED) {
                            /* wrong read/write mode should return EBADF, not
                               EACCES */
                                errno = EBADF;
                                _doserrno = dosretval;
                        }
                        else
                                _dosmaperr(dosretval);
                        return -1;
                }
                else if ((_osfile(fh) & FDEV) && *(char *)buf == CTRLZ)
                        return 0;
                else {
                        errno = ENOSPC;
                        _doserrno = 0;  /* no o.s. error */
                        return -1;
                }
        }
        else
                /* return adjusted bytes written */
                return charcount - lfcount;
}
