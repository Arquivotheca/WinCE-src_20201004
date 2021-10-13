//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

    if(!CheckStdioInit())
        return retval;

    _ASSERTE(string != NULL);
    _ASSERTE(stream != NULL);

    length = (fWide ? wcslen((WCHAR*)string) : strlen((char*)string));
    if (! _lock_validate_str(stream))
        return retval;

    buffing = _stbuf(stream);

    if(fWide)
    {
        while (length--)
        {
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
    _unlock_str(stream);
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
    if(!CheckStdioInit())
        return WEOF;
    return (WCHAR)fputs2(string, stdout, TRUE, TRUE);
}

int __cdecl puts(const char *string)
{
    if(!CheckStdioInit())
        return EOF;
    return (char)fputs2(string, stdout, FALSE, TRUE);
}

