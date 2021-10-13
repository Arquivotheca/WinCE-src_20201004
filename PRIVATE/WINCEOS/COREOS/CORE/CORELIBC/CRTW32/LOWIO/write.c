//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/***
*write.c - write to a file handle
*
*
*Purpose:
*   defines _write() - write to a file handle
*
*******************************************************************************/

#include <cruntime.h>
#include <msdos.h>
#include <mtdll.h>
#include <stdlib.h>
#include <string.h>
#include <internal.h>
#include <crtmisc.h>

#define BUF_SIZE    1025    /* size of LF translation buffer */

#define LF '\n'      /* line feed */
#define CR '\r'      /* carriage return */
#define CTRLZ 26     /* ctrl-z */







/***
*int _write(fh, buf, cnt) - write bytes to a file handle
*
*Purpose:
*   Writes count bytes from the buffer to the handle specified.
*   If the file was opened in text mode, each LF is translated to
*   CR-LF.  This does not affect the return value.  In text
*   mode ^Z indicates end of file.
*
*Entry:
*   int fh - file handle to write to
*   char *buf - buffer to write from
*   unsigned int cnt - number of bytes to write
*
*Exit:
*   returns number of bytes actually written.
*   This may be less than cnt, for example, if out of disk space.
*   returns -1 (and set errno) if fails.
*
*Exceptions:
*
*******************************************************************************/



BOOL MyWriteFile(HANDLE h, LPCVOID buffer, DWORD cnt, LPDWORD pWrote, LPOVERLAPPED x)  {
  if( h==(HANDLE) (-2) ) { 
    wchar_t *MyWriteBuff = (wchar_t*) malloc(sizeof(wchar_t)*(cnt+1));
    if (MyWriteBuff) {
        if(MultiByteToWideChar(  CP_ACP, (DWORD) 0,   (LPCSTR) buffer, cnt , (LPWSTR) MyWriteBuff, cnt+1 )) {
          MyWriteBuff[cnt] = '\0';
          OutputDebugStringW(MyWriteBuff);
        } else
          OutputDebugStringW(L"ERROR: Failed to Convert to wide char in MyWriteFile\n\r");
        free (MyWriteBuff);
        *pWrote = cnt;
    } else
        *pWrote = 0;
    return TRUE;
   } else {
    return WriteFile(h,buffer,cnt,pWrote,x);
   }
}

static char writebuff[64];

int __cdecl _write (
    FILEX* str,
    const void *buf,
    unsigned cnt
    )
{
    int lfcount;        /* count of line feeds */
    int charcount;      /* count of chars written so far */
    int written;        /* count of chars written on this write */
    char ch;        /* current character */
    char *p, *q;        /* pointers into buf and lfbuf resp. */
    char lfbuf[BUF_SIZE];   /* lf translation buffer */
    BOOL fErr = FALSE;
    
    lfcount = charcount = 0;    /* nothing written yet */
    
    if (cnt == 0)
        return 0;       /* nothing to do */

    if(_osfhndstr(str)==INVALID_HANDLE_VALUE && (str->fd < 3))
        OpenStdConsole(str->fd);
    
        if (_osfilestr(str) & FAPPEND) 
    {
        /* appending - seek to end of file; ignore error, because maybe
           file doesn't allow seeking */
        (void)_lseek(str, 0, FILE_END);
    }
    /* check for text mode with LF's in the buffer */
    if ( _osfilestr(str) & FTEXT ) 
    {
        /* text mode, translate LF's to CR/LF's on output */
        p = (char *)buf;    /* start at beginning of buffer */
        while ( (unsigned)(p - (char *)buf) < cnt ) 
        {
            q = lfbuf;  /* start at beginning of lfbuf */

            /* fill the lf buf, except maybe last char */
            while ( q - lfbuf < BUF_SIZE - 1 &&
                (unsigned)(p - (char *)buf) < cnt ) 
            {
                ch = *p++;
                if ( ch == LF ) 
                {
                    ++lfcount;
                    *q++ = CR;
                }
                *q++ = ch;
            }

            /* write the lf buf and update total */
            
            if (MyWriteFile(_osfhndstr(str),
                    lfbuf, q - lfbuf, (LPDWORD)&written, NULL) )
            {
                    charcount += written;
                if (written < q - lfbuf)
                    break;
            }
            else 
            {
                    fErr = TRUE;
                break;
            }
        }
    }
    else 
    {
        /* binary mode, no translation */
        if(MyWriteFile(_osfhndstr(str), (LPVOID)buf, cnt, (LPDWORD)&written,NULL))
            charcount = written;
        else
            fErr = TRUE;
    }

    if (charcount == 0) 
    {
        /* If nothing was written, first check if an o.s. error,
           otherwise we return -1 and set errno to ENOSPC,
           unless a device and first char was CTRL-Z */
        if (fErr) 
            return -1; // NT already set error
        else if ((_osfilestr(str) & FDEV) && *(char *)buf == CTRLZ)
            return 0;
        else 
        {
            SetLastError(ERROR_DISK_FULL); // we need to set error
            return -1;
        }
    }
    else
        /* return adjusted bytes written */
        return charcount - lfcount;
}







