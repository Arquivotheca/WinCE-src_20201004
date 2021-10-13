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
*getwclk.c - get a wide character from a stream
*
*
*Purpose:
*   defines fgetwc() - read a wide character from a stream
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
*_getwc_lk() -  getwc() core routine (locked version)
*
*Purpose:
*   Core getwc() routine; assumes stream is already locked.
*
*   [See getwc() above for more info.]
*
*Entry: [See getwc()]
*
*Exit:  [See getwc()]
*
*Exceptions:
*
*******************************************************************************/

wint_t __cdecl _getwc_lk (
    REG1 FILEX *stream
    )
{

    if (!(stream->_flag & _IOSTRG) && (_osfilestr(stream) & FTEXT))
    {
        int size = 1;
                int ch;
        char mbc[4];
        wchar_t wch;
        
        /* text (multi-byte) mode */
        if ((ch = _getc_lk(stream)) == EOF)
            return WEOF;

        mbc[0] = (char)ch;

        if (isleadbyte((unsigned char)mbc[0]))
        {
            if ((ch = _getc_lk(stream)) == EOF)
            {
                _ungetc_lk(mbc[0], stream);
                return WEOF;
            }
            mbc[1] = (char)ch;
            size = 2;
        }
        // if (mbtowc(&wch, mbc, size) == -1) ...ARULM
        if (change_to_widechar(&wch, mbc, size) <= 0)
        {
            /*
             * Conversion failed! Set errno and return
             * failure.
             */
            //errno = EILSEQ;
            return WEOF;
        }
        return wch;
    }

    /* binary (Unicode) mode */
    if ((stream->_cnt -= sizeof(wchar_t)) >= 0)
    {
#pragma warning(suppress: 4213) // nonstandard extension used : cast on l-value
        return *((wchar_t *)(stream->_ptr))++;
    }
    else
    {
        return (wint_t)_filwbuf(stream);
    }

}

