//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/***
*fgets.c - get string from a file
*
*
*Purpose:
*   defines fgets() - read a string from a file
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <dbgint.h>
#include <file2.h>
#include <internal.h>
#include <mtdll.h>
#include <crttchar.h>

#define MAXSTR  INT_MAX
#define MAXWSTR ((INT_MAX-1)/sizeof(WCHAR))

/***
*char *fgets(string, count, stream) - input string from a stream
*
*Purpose:
*   get a string, up to count-1 chars or '\n', whichever comes first,
*   append '\0' and put the whole thing into string. the '\n' IS included
*   in the string. if count<=1 no input is requested. if EOF is found
*   immediately, return NULL. if EOF found after chars read, let EOF
*   finish the string as '\n' would.
*
*Entry:
*   char *string - pointer to place to store string
*   int count - max characters to place at string (include \0)
*   FILEX *stream - stream to read from
*
*Exit:
*   returns string with text read from file in it.
*   if count <= 0 return NULL
*   if count == 1 put null string in string
*   returns NULL if error or end-of-file found immediately
*
*Exceptions:
*
*******************************************************************************/

void* fgets2(void* string, int count, FILEX* str, int fWide, int fNukeLF)
{
    REG2 void *pointer = string;
    void *retval = string;
    int ch;

    if(!CheckStdioInit())
        return NULL;

    _ASSERTE(string != NULL);
    _ASSERTE(str != NULL);

    if (count <= 0)
        return(NULL);

    if (! _lock_validate_str(str))
        return NULL;

    while (--count)
    {
        ch = (fWide ? _getwc_lk(str) : _getc_lk(str));
        if(ch == (fWide ? WEOF : EOF))
        {
            if (pointer == string) 
            {
                retval=NULL;
                goto done;
            }
            break;
        }
        if(fWide)
            *((WCHAR*)pointer)++ = (WCHAR)ch;
        else
            *((char*)pointer)++ = (char)ch;

        if(ch == '\n')  // '\n' same in wide & ansi
            break;
    }
    if(fNukeLF && ch=='\n')
    {
        if(fWide)
            ((WCHAR*)pointer)--;
        else
            ((char*)pointer)--;
    }
    
    if(fWide)
        *((WCHAR*)pointer) = 0;
    else
        *((char*)pointer) = 0;

done:
    _unlock_str(str);
    return(retval);
}

wchar_t* __cdecl fgetws(WCHAR* string, int count, FILEX *str) 
{ 
    return (wchar_t*)fgets2(string, count, str, TRUE, FALSE); 
}

char* __cdecl fgets(char* string, int count, FILEX *str) 
{ 
    return (char*)fgets2(string, count, str, FALSE, FALSE); 
}

char* __cdecl gets(char* string) 
{ 
    // need to call CheckStdioInit before using stdin/stdout/stderr macros
    if(!CheckStdioInit())
        return NULL;
    return (char*)fgets2(string, MAXSTR, stdin, FALSE, TRUE);
}

wchar_t* __cdecl _getws(WCHAR* string) 
{ 
    // need to call CheckStdioInit before using stdin/stdout/stderr macros
    if(!CheckStdioInit())
        return NULL;
    return (wchar_t*)fgets2(string, MAXWSTR, stdin, TRUE, TRUE);
}
