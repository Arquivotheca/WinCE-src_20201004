//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/***
*ungetwclk.c - unget a wide character from a stream
*
*
*Purpose:
*   defines ungetwc() - pushes a wide character back onto an input stream
*
*******************************************************************************/

#include <cruntime.h>
#include <crtmisc.h>
#include <stdio.h>
#include <stdlib.h>
#include <file2.h>
#include <dbgint.h>
#include <internal.h>
#include <mtdll.h>
#include <msdos.h>


/***
*_ungetwc_lk() -  Ungetwc() core routine (locked version)
*
*Purpose:
*   Core ungetwc() routine; assumes stream is already locked.
*
*   [See ungetwc() above for more info.]
*
*Entry: [See ungetwc()]
*
*Exit:  [See ungetwc()]
*
*Exceptions:
*
*******************************************************************************/

wint_t __cdecl _ungetwc_lk (
    wint_t ch,
    FILEX *str
    )
{
    _ASSERTE(str != NULL);

    /*
     * Requirements for success:
     *
     * 1. Character to be pushed back on the stream must not be WEOF.
     *
     * 2. The stream must currently be in read mode, or must be open for
     *    update (i.e., read/write) and must NOT currently be in write
     *    mode.
     */
    if ( (ch != WEOF) &&
         ( (str->_flag & _IOREAD) || ((str->_flag & _IORW) &&
        !(str->_flag & _IOWRT))
         )
       )
    {

/* ???? */
        /* If stream is unbuffered, get one. */
        if (str->_base == NULL)
            _getbuf(str);
/* ???? */
        if (!(str->_flag & _IOSTRG) && (_osfilestr(str) & FTEXT))
        {
            /*
             * Text mode, sigh... Convert the wc to a mbc.
             */
            int size;
            char mbc[4];

            // if ((size = wctomb(mbc, ch)) == -1) ...ARULM
            if ((size = change_to_multibyte(mbc, MB_LEN_MAX, ch)) <= 0)
            {
                /*
                 * Conversion failed! Set errno and return
                 * failure.
                 */
                //errno = EILSEQ;
                return WEOF;
            }

            /* we know _base != NULL; since file is buffered */
            if (str->_ptr == str->_base)
            {
                if (str->_cnt)
                /* my back is against the wall; i've already done
                 * ungetwc, and there's no room for this one
                 */
                    return WEOF;
                str->_ptr += size;
            }

            if ( size == 1 )
            {
                        *--str->_ptr = mbc[0];
            }
            else /* size == 2 */
            {
                        *--str->_ptr = mbc[1];
                        *--str->_ptr = mbc[0];
            }

            str->_cnt += size;

            str->_flag &= ~_IOEOF;
            str->_flag |= _IOREAD;  /* may already be set */
            return (wint_t) (0x0ffff & ch);
        }
            /*
         * Binary mode - push back the wide char.
         */
        /* we know _base != NULL; since file is buffered */
        if (str->_ptr == str->_base)
        {
            if (str->_cnt)
                /* my back is against the wall; i've already done
                 * ungetc, and there's no room for this one
                 */
                return WEOF;
            str->_ptr += sizeof(wchar_t);
        }


        if (str->_flag & _IOSTRG) 
        {
            /* If stream opened by sscanf do not modify buffer */
                if (*--((wchar_t*)(str->_ptr)) != ch) 
                {
                    // different char. can't unget. return error
                    (wchar_t*)(str->_ptr)++;
                    return(WEOF);
                }
        } 
        else
            *--((wchar_t*)(str->_ptr)) = (wchar_t)(ch & 0xFFFF);

        str->_cnt += sizeof(wchar_t);
        str->_flag &= ~_IOEOF;
        str->_flag |= _IOREAD;  /* may already be set */
        
        return(0xffff & ch);
    }
    return WEOF;
}
