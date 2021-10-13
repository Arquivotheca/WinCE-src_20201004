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
*Purpose:
*       defines fgets() - read a string from a file
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <dbgint.h>
#include <file2.h>
#include <internal.h>
#include <mtdll.h>
#include <crttchar.h>

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

CRT__TSCHAR * __cdecl _fgetts (
    CRT__TSCHAR *string,
    int count,
    FILE *str
    )
{
    REG1 FILEX *stream;
    REG2 CRT__TSCHAR *pointer = string;
    CRT__TSCHAR *retval = string;
    int ch;

    _VALIDATE_RETURN((string != NULL ) || ( count == 0 ), EINVAL, NULL);
    _VALIDATE_RETURN((count >= 0 ), EINVAL, NULL);
    _VALIDATE_RETURN((str != NULL ), EINVAL, NULL);

    if (count == 0)
    {
        return NULL;
    }

    /* The C Standard states the input buffer should remain
    unchanged if EOF is encountered immediately. Hence we
    do not blank out the input buffer here */

    /* Init stream pointer */
    stream = str;

    if (!_lock_validate_str(stream))
        return NULL;

    __STREAM_TRY
    {
#ifndef CRT_UNICODE
// Don't support this
//    _VALIDATE_STREAM_ANSI_SETRET(stream, EINVAL, retval, NULL);
#endif
        if (retval != NULL)
        {
            while (--count)
            {
                if ((ch = _gettc_lk(stream)) == CRT_TEOF)
                {
                    if (pointer == string) {
                        retval=NULL;
                        goto done;
                    }

                    break;
                }

                if ((*pointer++ = (CRT__TSCHAR)ch) == CRT_T('\n'))
                {
                    break;
                }
            }
            *pointer = CRT_T('\0');
        }

/* Common return */
done:;

    }
    __STREAM_FINALLY
    {
        _unlock_str(stream);
    }

    return(retval);
}

