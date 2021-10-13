//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/***
*putwclk.c - write a wide character to an output stream
*
*
*Purpose:
*   defines fputwc() - writes a wide character to a stream
*
*******************************************************************************/

#include <cruntime.h>
#include <crtmisc.h>
#include <stdio.h>
#include <stdlib.h>
#include <dbgint.h>
#include <file2.h>
#include <internal.h>
#include <mtdll.h>
#include <msdos.h>
#include <crttchar.h>


/***
*_putwc_lk() -  putwc() core routine (locked version)
*
*Purpose:
*   Core putwc() routine; assumes stream is already locked.
*
*   [See putwc() above for more info.]
*
*Entry: [See putwc()]
*
*Exit:  returns EOF if end of file reached, otherwise returns the character
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _putwc_lk (
    wint_t ch,
    FILEX *str
    )
{

    if (!(str->_flag & _IOSTRG) && (_osfilestr(str) & FTEXT))
    {
        int size;
        char mbc[4];

        /* text (multi-byte) mode */
        // if ((size = wctomb(mbc, ch)) == -1) ...ARULM
        if ((size = change_to_multibyte(mbc,  MB_LEN_MAX, ch)) <= 0)
        {
            /*
             * Conversion failed! Set errno and return
             * failure.
             */
            //errno = EILSEQ;
            return EOF;
        }
        else if ( size == 1 )
        {
            if ( _putc_lk(mbc[0], str) == EOF )
                return EOF;
            return (wint_t)(0xffff & ch);
        }
        else { /* size == 2 */
            if ( (_putc_lk(mbc[0], str) == EOF) ||
                 (_putc_lk(mbc[1], str) == EOF) )
                return EOF;
            return (wint_t)(0xffff & ch);
        }
    }

    /* binary (Unicode) mode */
    if ( (str->_cnt -= sizeof(wchar_t)) >= 0 )
        return (wint_t) (0xffff & (*((wchar_t *)(str->_ptr))++ = (wchar_t)ch));
    else
        return _flswbuf(ch, str);
}

