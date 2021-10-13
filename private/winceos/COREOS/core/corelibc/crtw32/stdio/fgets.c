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
*fgets.c - get string from a file
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines fgets() - read a string from a file
*
*Revision History:
*       09-02-83  RN    initial version
*       04-16-87  JCR   changed count from an unsigned int to an int (ANSI)
*                       and modified comparisons accordingly
*       11-06-87  JCR   Multi-thread support
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       05-31-88  PHG   Merged DLL and normal versions
*       06-14-88  JCR   Use near pointer to reference _iob[] entries
*       08-24-88  GJF   Don't use FP_OFF() macro for the 386
*       08-28-89  JCR   Removed _NEAR_ for 386
*       02-15-90  GJF   Fixed copyright and indents
*       03-19-90  GJF   Replaced _LOAD_DS with _CALLTYPE1, added #include
*                       <cruntime.h> and added #include <register.h>. Also,
*                       removed some leftover 16-bit support.
*       07-24-90  SBM   Replaced <assertm.h> by <assert.h>
*       08-14-90  SBM   Compiles cleanly with -W3
*       10-02-90  GJF   New-style function declarator.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       10-01-93  CFW   Enable for fgetws().
*       12-07-93  CFW   Change _TCHAR to _TSCHAR.
*       09-06-94  CFW   Replace MTHREAD with _MT.
*       02-06-94  CFW   assert -> _ASSERTE.
*       02-22-95  GJF   Replaced WPRFLAG with _UNICODE.
*       03-07-95  GJF   _[un]lock_str macros now take FILE * arg.
*       02-27-98  GJF   Exception-safe locking.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       12-02-03  SJ    Reroute Unicode I/O
*       03-04-04  AC    Fixed validation.
*       03-13-04  MSL   Avoid returning from __try for prefast
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <dbgint.h>
#include <file2.h>
#include <internal.h>
#include <mtdll.h>
#include <tchar.h>

/***
*char *fgets(string, count, stream) - input string from a stream
*
*Purpose:
*       get a string, up to count-1 chars or '\n', whichever comes first,
*       append '\0' and put the whole thing into string. the '\n' IS included
*       in the string. if count<=1 no input is requested. if EOF is found
*       immediately, return NULL. if EOF found after chars read, let EOF
*       finish the string as '\n' would.
*
*Entry:
*       char *string - pointer to place to store string
*       int count - max characters to place at string (include \0)
*       FILE *stream - stream to read from
*
*Exit:
*       returns string with text read from file in it.
*       if count <= 0 return NULL
*       if count == 1 put null string in string
*       returns NULL if error or end-of-file found immediately
*
*Exceptions:
*
*******************************************************************************/

_TSCHAR * __cdecl _fgetts (
        _TSCHAR *string,
        int count,
        FILE *str
        )
{
    FILE *stream;
    _TSCHAR *pointer = string;
    _TSCHAR *retval = string;
    int ch;

    _VALIDATE_RETURN(( string != NULL ) || ( count == 0 ), EINVAL, NULL);
    _VALIDATE_RETURN(( count >= 0 ), EINVAL, NULL);
    _VALIDATE_RETURN(( str != NULL ), EINVAL, NULL);
    _CHECK_IO_INIT(NULL);

    if (count == 0)
    {
        return NULL;
    }

    /* The C Standard states the input buffer should remain
    unchanged if EOF is encountered immediately. Hence we
    do not blank out the input buffer here */

    /* Init stream pointer */
    stream = str;

    _lock_str(stream);
    __try {
#ifndef _UNICODE
        _VALIDATE_STREAM_ANSI_SETRET(stream, EINVAL, retval, NULL);
#endif
        if (retval!=NULL)
        {
            while (--count)
            {
                if ((ch = _fgettc_nolock(stream)) == _TEOF)
                {
                    if (pointer == string) {
                                    retval=NULL;
                                    goto done;
                    }

                    break;
                }

                if ((*pointer++ = (_TSCHAR)ch) == _T('\n'))
                    break;
            }

            *pointer = _T('\0');
        }

        
/* Common return */
done: ;
    }
    __finally {
        _unlock_str(stream);
    }

    return(retval);
}
