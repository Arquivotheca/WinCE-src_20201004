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
*putch.c - contains the _putch() routine
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       The routine "_putch()" writes a single character to the console.
*
*       NOTE: In real-mode MS-DOS the character is actually written to standard
*       output, and is therefore redirected when standard output is redirected.
*       However, under Win32 console mode, the character is ALWAYS written
*       to the console, even when standard output has been redirected.
*
*Revision History:
*       06-08-89  PHG   Module created, based on asm version
*       03-13-90  GJF   Made calling type _CALLTYPE1, added #include
*                       <cruntime.h> and fixed copyright. Also, cleaned up
*                       the formatting a bit.
*       06-05-90  SBM   Recoded as pure 32-bit, using new file handle state bits
*       07-24-90  SBM   Removed '32' from API names
*       10-01-90  GJF   New-style function declarators.
*       12-04-90  SRW   Changed to include <oscalls.h> instead of <doscalls.h>
*       12-06-90  SRW   Added _CRUISER_ and _WIN32 conditionals.
*       01-16-91  GJF   ANSI naming.
*       02-19-91  SRW   Adapt to OpenFile/CreateFile changes (_WIN32_)
*       02-25-91  MHL   Adapt to ReadFile/WriteFile changes (_WIN32_)
*       07-26-91  GJF   Took out init. stuff and cleaned up the error
*                       handling [_WIN32_].
*       03-20-93  GJF   Use WriteConsole instead of WriteFile.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       09-06-94  CFW   Remove Cruiser support.
*       09-06-94  CFW   Replace MTHREAD with _MT.
*       12-03-94  SKS   Clean up OS/2 references
*       12-08-95  SKS   _confh is now initialized on demand
*       02-07-98  GJF   Changes for Win64: type of _confh is now intptr_t.
*       04-29-02  GB    Added try-finally arounds lock-unlock.
*
*******************************************************************************/

#ifndef _WIN32_WCE

#include <cruntime.h>
#include <oscalls.h>
#include <conio.h>
#include <internal.h>
#include <mtdll.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>

/*
 * declaration for console handle
 */
extern intptr_t _confh;

/***
*int _putch(c) - write a character to the console
*
*Purpose:
*       Calls WriteConsole to output the character
*       Note: in Win32 console mode always writes to console even
*       when stdout redirected
*
*Entry:
*       c - Character to be output
*
*Exit:
*       If an error is returned from WriteConsole
*           Then returns EOF
*       Otherwise
*           returns character that was output
*
*Exceptions:
*
*******************************************************************************/

/* normal version lock and unlock the console, and then call the _lk version
   which directly accesses the console without locking. */

int __cdecl _putch (
        int c
        )
{
        int ch;

        _mlock(_CONIO_LOCK);            /* secure the console lock */
        __TRY
            ch = _putch_nolock(c);              /* output the character */
        __FINALLY
            _munlock(_CONIO_LOCK);          /* release the console lock */
        __END_TRY_FINALLY

        return ch;
}

/* define version which accesses the console directly - normal version in
   non-_MT situations, special _lk version in _MT */

int __cdecl _putch_nolock (
        int c
        )
{
        _ptiddata ptd = _getptd();
        unsigned char *ch_buf = ptd->_con_ch_buf;
        unsigned short *pch_buf_used = &(ptd->_ch_buf_used);

        /* can't use ch directly unless sure we have a big-endian machine */
        unsigned char ch = (unsigned char)c;
        wchar_t wchar;

        /*
         * Why are we using putwch to write to Console when we could have
         * written straight away to Console? The problem we have in writing to
         * Console is that CRT codepage is different from Console codepage and
         * thus to write to console, we will need to convert the codepage. Here
         * we can use unicode version of these routines and this way we will
         * only have to do one conversion and rest will be handled by putwch.
         */

        /*
         * The usual way people call putch is character by character. Also
         * there is noway we can convert partial MBCS to unicode character. To
         * address this issue, we buffer all the lead bytes and combine them
         * with trail bytes and then do the conversion.
         */
        if (*pch_buf_used == 1)
        {
            _ASSERTE(isleadbyte(ch_buf[0]) != 0);

            ch_buf[1] = ch;
        }
        else
        {
            ch_buf[0] = ch;
        }

        if (*pch_buf_used == 0 && isleadbyte(ch_buf[0]))
        {
            /*
             * We still need trail byte, wait for it.
             */
            *pch_buf_used = 1;
        }
        else
        {
            if (mbtowc(&wchar, ch_buf, (*pch_buf_used)+1) == -1 ||
                    _putwch_nolock(wchar) == WEOF)
            {
                ch = EOF;
            }
            /*
             * Since we have processed full MBCS character, we should reset ch_buf_used.
             */
            (*pch_buf_used) = 0;
        }

        return ch;
}

#endif // _WIN32_WCE