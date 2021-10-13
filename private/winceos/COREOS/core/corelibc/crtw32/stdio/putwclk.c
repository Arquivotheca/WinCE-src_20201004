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
        char mbc[MB_LEN_MAX];

        /* text (multi-byte) mode */
        if (__internal_wctomb_s(&size, mbc, MB_LEN_MAX, ch) != 0)
        {
            /*
             * Conversion failed! Set errno and return
             * failure.
             */
            _SET_ERRNO(EILSEQ);
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
    {
#pragma warning(suppress: 4213) // nonstandard extension used : cast on l-value
        return (wint_t) (0xffff & (*((wchar_t *)(str->_ptr))++ = (wchar_t)ch));
    }
    else
    {
        return _flswbuf(ch, str);
    }
}

