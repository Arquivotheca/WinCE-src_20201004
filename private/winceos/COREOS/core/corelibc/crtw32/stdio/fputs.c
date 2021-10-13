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
*fputs.c - write a string to a stream
*
*
*Purpose:
*   defines fputs() - writes a string to a stream
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <dbgint.h>
#include <file2.h>
#include <string.h>
#include <internal.h>
#include <mtdll.h>

/***
*int fputs(string, stream) - write a string to a file
*
*Purpose:
*   Output the given string to the stream, don't write the '\0' or
*   supply a '\n'.  Uses _stbuf and _ftbuf for efficiency reasons.
*
*Entry:
*   char *string - string to write
*   FILEX *stream - stream to write to.
*
*Exit:
*   Good return   = 0
*   Error return  = EOF
*
*Exceptions:
*
*******************************************************************************/

int __cdecl fputs2(
    const void* string,
    FILEX *stream,
    int fWide,
    int fPutLF
    )
{
    REG2 int buffing;
    REG1 unsigned int length;
    int  retval = -1;

    _VALIDATE_RETURN((string != NULL), EINVAL, -1);
    _VALIDATE_RETURN((stream != NULL), EINVAL, -1);

    length = (fWide ? wcslen((WCHAR*)string) : strlen((char*)string));

    if (!_lock_validate_str(stream))
        return retval;

    __STREAM_TRY
    {
        buffing = _stbuf(stream);

        if(fWide)
        {
            while (length--)
            {
#pragma warning(suppress: 4213) // nonstandard extension used : cast on l-value
                if (_putwc_lk(*((WCHAR*)string)++, stream) == EOF)
                    goto done;
            }
            if(fPutLF)
            {
                if (_putwc_lk(L'\n', stream) == EOF)
                    goto done;
            }
        }
        else
        {
            if(length != _fwrite_lk(string,1,length,stream))
                goto done;
            if(fPutLF)
                _putc_lk('\n', stream);
        }
        retval = 0;

done:
        _ftbuf(buffing, stream);
    }
    __STREAM_FINALLY
    {
        _unlock_str(stream);
    }

    return retval;
}

int __cdecl fputws(const wchar_t *string, FILEX *stream)
{
    return (WCHAR)fputs2(string, stream, TRUE, FALSE);
}

int __cdecl fputs(const char *string, FILEX *stream)
{
    return (char)fputs2(string, stream, FALSE, FALSE);
}

int __cdecl _putws(const wchar_t *string)
{
    /* Initialize before using *internal* stdout */
    if(!CheckStdioInit())
        return WEOF;

    return (WCHAR)fputs2(string, stdout, TRUE, TRUE);
}

int __cdecl puts(const char *string)
{
    /* Initialize before using *internal* stdin */
    if(!CheckStdioInit())
        return EOF;

    return (char)fputs2(string, stdout, FALSE, TRUE);
}

