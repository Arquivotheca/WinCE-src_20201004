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
*cgets.c - buffered keyboard input
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines _cgets() - read a string directly from console
*
*Revision History:
*       06-09-89  PHG   Module created, based on asm version
*       03-12-90  GJF   Made calling type _CALLTYPE1, added #include
*                       <cruntime.h> and fixed copyright. Also, cleaned
*                       up the formatting a bit.
*       06-05-90  SBM   Recoded as pure 32-bit, using new file handle state bits
*       07-24-90  SBM   Removed '32' from API names
*       08-13-90  SBM   Compiles cleanly with -W3
*       09-28-90  GJF   New-style function declarator.
*       12-04-90  SRW   Changed to include <oscalls.h> instead of <doscalls.h>
*       12-13-90  GJF   Fixed a couple of bugs.
*       12-06-90  SRW   Added _CRUISER_ and _WIN32 conditionals.
*       01-16-91  GJF   ANSI naming.
*       01-25-91  SRW   Get/SetConsoleMode parameters changed (_WIN32_)
*       02-18-91  SRW   Get/SetConsoleMode required read/write access (_WIN32_)
*       02-19-91  SRW   Adapt to OpenFile/CreateFile changes (_WIN32_)
*       02-25-91  MHL   Adapt to ReadFile/WriteFile changes (_WIN32_)
*       07-26-91  GJF   Took out init. stuff and cleaned up the error
*                       handling [_WIN32_].
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       04-19-93  GJF   Use ReadConsole instead of ReadFile.
*       09-06-94  CFW   Remove Cruiser support.
*       12-03-94  SKS   Clean up OS/2 references
*       03-02-95  GJF   Treat string[0] as an unsigned value.
*       12-08-95  SKS   _coninph is now initialized on demand
*       02-07-98  GJF   Changes for Win64: _coninph is now an intptr_t.
*       04-29-02  GB    Added try-finally arounds lock-unlock.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       03-13-04  MSL   Rename variable in enclosing scope for prefast
*       10-18-04  MSL   Fix cgetws to actually work again
*       10-28-04  JL    Fix _cgets_s to recognize return error code properly.
*                       Also fix initialization problem in _cgets. VSW#372552
*       11-16-04  AC    Fix return value for _cgets_s.
*                       VSW#372552
*       04-25-05  AC    Added debug filling
*                       VSW#2459
*       05-08-05  PAL   Initialize *pSizeRead in _cgets_s.
*
*******************************************************************************/

#include <cruntime.h>
#include <oscalls.h>
#include <mtdll.h>
#include <conio.h>
#include <stdlib.h>
#include <internal.h>
#include <internal_securecrt.h>

/*
 * mask to clear the bits required to be 0 in the handle state passed to
 * DOSSETFHSTATE.
 */
#define FHSTATEMASK 0xffd07888

/*
 * declaration for console handle
 */

extern intptr_t _coninpfh;

/*
 * Use of the following buffer variables is primarily for syncronizing with
 * _cgets_s. _cget_s fills the MBCS buffer and if the user passes in single
 * character buffer and the unicode character is not converted to single byte
 * MBC, then _cget_s should buffer that character so that next call to
 * _cgetws_s can return the same character.
 */
extern wchar_t __console_wchar_buffer;
extern int __console_wchar_buffer_used;

/***
*errno_t _cgets_s(string, sizeInBytes, pSizeRead) - read string from console
*
*Purpose:
*       Reads a string from the console. Always null-terminate the buffer.
*
*Entry:
*       char *string - place to store read string
*       sizeInBytes  - max length to read
*       pSizeRead - sets the max read in this
*
*Exit:
*       returns error code or 0 if no error occurs
*
*Exceptions:
*
*******************************************************************************/

errno_t __cdecl _cgets_s (
        char *string,
        size_t sizeInBytes,
        size_t * pSizeRead
        )
{
#ifdef _WIN32_WCE
        return ERROR_NOT_SUPPORTED;   
#else
        errno_t err = 0;
        size_t available;

        _VALIDATE_CLEAR_OSSERR_RETURN_ERRCODE((string != NULL), EINVAL);
        _VALIDATE_CLEAR_OSSERR_RETURN_ERRCODE((sizeInBytes > 0), EINVAL);
        _RESET_STRING(string, sizeInBytes);
        
        _VALIDATE_CLEAR_OSSERR_RETURN_ERRCODE((pSizeRead != NULL), EINVAL);

        available = sizeInBytes - 1;
        
        _mlock(_CONIO_LOCK);            /* lock the console */
        __TRY
            /*
             * The implementation of cgets is slightly tricky. The reason being,
             * the code page for console is different from the CRT code page.
             * What this means is the program may interpret character
             * differently from it's acctual value. To fix this, what we really
             * want to do is read the input as unicode string and then convert
             * it to proper MBCS representation.
             *
             * This fix this we are really converting from Unicode to MBCS.
             * This adds performance problem as we may endup doing this
             * character by character. The basic problem here is that we have
             * no way to know how many UNICODE characters will be needed to fit
             * them in given size of MBCS buffer. To fix this issue we will be
             * converting one Unicode character at a time to MBCS. This makes
             * this slow, but then this is already console input,
             */
            *pSizeRead = 0;
            do {
                wchar_t wchar_buff[2];
                size_t sizeRead=0;
                if ((err = _cgetws_s(wchar_buff, _countof(wchar_buff), &sizeRead)) == 0)
                {
                    int sizeConverted = 0;
                    if (wchar_buff[0] == L'\0')
                    {
                        break;
                    }
                    else if ((err = wctomb_s(&sizeConverted, string, available, wchar_buff[0])) == 0)
                    {
                        string += sizeConverted;
                        (*pSizeRead) += sizeConverted;
                        available -= sizeConverted;
                    }
                    else
                    {
                        /*
                         * Put the wchar back to the buffer so that the
                         * unutilized wchar is still in the stream.
                         */
                        __console_wchar_buffer = wchar_buff[0];
                        __console_wchar_buffer_used = 1;
                        break;
                    }
                }
                else
                {
                    break;
                }
            } while (available > 0);

        __FINALLY
            _munlock(_CONIO_LOCK);          /* unlock the console */
        __END_TRY_FINALLY

        *string++ = '\0';

        if (err != 0)
        {
            errno = err;
        }
        return err;
#endif // _WIN32_WCE
}

/***
*char *_cgets(string) - read string from console
*
*Purpose:
*       Reads a string from the console via ReadConsole on a cooked console
*       handle.  string[0] must contain the maximum length of the
*       string.  Returns pointer to str[2].
*
*       NOTE: _cgets() does NOT check the pushback character buffer (i.e.,
*       _chbuf).  Thus, _cgets() will not return any character that is
*       pushed back by the _ungetch() call.
*
*Entry:
*       char *string - place to store read string, str[0] = max length.
*
*Exit:
*       returns pointer to str[2], where the string starts.
*       returns NULL if error occurs
*
*Exceptions:
*
*******************************************************************************/
char * __cdecl _cgets (
        char *string)
{
#ifdef _WIN32_WCE
        return NULL;
#else
        size_t sizeInBytes;
        size_t sizeRead = 0;
        errno_t err;

        _VALIDATE_CLEAR_OSSERR_RETURN((string != NULL), EINVAL, NULL);

        sizeInBytes = (size_t)string[0];

        err = _cgets_s(string + 2, sizeInBytes, &sizeRead);

        string[1] = (char)(sizeRead);

        if (err != 0)
        {
            return NULL;
        }

        return string + 2;
#endif // _WIN32_WCE
}
