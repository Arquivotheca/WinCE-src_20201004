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
*_strerr.c - routine for indexing into system error list
*
*   Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*   Returns system error message index by errno; conforms to the
*   XENIX standard, much compatibility with 1983 uniforum draft standard.
*
*Revision History:
*       02-24-87  JCR   Renamed this routine from "strerror" to "_strerror"
*                       for MS. The new "strerror" routine conforms to the
*                       ANSI interface.
*       11-10-87  SKS   Remove IBMC20 switch
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       01-05-87  JCR   Mthread support
*       05-31-88  PHG   Merged DLL and normal versions
*       06-06-89  JCR   386 mthread support
*       11-20-89  GJF   Fixed copyright, indents. Removed unreferenced local.
*                       Added const attribute to type of message
*       03-13-90  GJF   Replaced _LOAD_DS with _CALLTYPE1, added #include
*                       <cruntime.h> and removed #include <register.h>
*       07-25-90  SBM   Removed redundant include (stdio.h)
*       10-04-90  GJF   New-style function declarator.
*       07-18-91  GJF   Multi-thread support for Win32 [_WIN32_].
*       02-17-93  GJF   Changed for new _getptd().
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       09-06-94  CFW   Remove Cruiser support.
*       09-06-94  CFW   Replace MTHREAD with _MT.
*       01-10-95  CFW   Debug CRT allocs.
*       29-11-99  GB    Added support for wide char by adding _wcserror() 
*       12-12-01  BWT   Use getptd_noexit and return NULL if it fails.
*       05-08-02  GB    Fix for VS7#514433, Fixed possiblity of BO on bad user input.
*       07-12-02  GB    Fixed _ERRMSGLEN_ macro, should have it's value in ()
*       05-08-02  GB    Fix for VS7#514433, Fixed possiblity of BO on bad user input.
*       07-12-02  GB    Fixed _ERRMSGLEN_ macro, should have it's value in ()
*       10-08-03  AC    Added secure version.
*       04-01-05  MSL   Integer overflow protection
*
*******************************************************************************/

#include <cruntime.h>
#include <stdlib.h>
#include <errmsg.h>
#include <syserr.h>
#include <string.h>
#include <tchar.h>
#include <malloc.h>
#include <mtdll.h>
#include <dbgint.h>
#include <internal.h>

/* Max length of message = user_string(94)+system_string+2 */
/* [NOTE: The mthread error message buffer is shared by both strerror
   and _strerror so must be the max length of both. */
#define _ERRMSGLEN_ (94+_SYS_MSGMAX+2)

#ifdef _UNICODE
#define _terrmsg    _werrmsg
#else
#define _terrmsg    _errmsg
#endif


/***
*char *_strerror(message) - get system error message
*
*Purpose:
*   builds an error message consisting of the users error message
*   (the message parameter), followed by ": ", followed by the system
*   error message (index through errno), followed by a newline.  If
*   message is NULL or a null string, returns a pointer to just
*   the system error message.
*
*Entry:
*   char *message - user's message to prefix system error message
*
*Exit:
*   returns pointer to static memory containing error message.
*   returns NULL if malloc() fails in multi-thread versions.
*
*Exceptions:
*
*******************************************************************************/

#ifdef _UNICODE
wchar_t * __cdecl __wcserror(
#else
char * __cdecl _strerror (
#endif
    const _TCHAR *message
    )
{
    const char *sysErrorMsg = NULL;
    _TCHAR *bldmsg;
    _ptiddata ptd = _getptd_noexit();
    if (!ptd)
        return NULL;

    /* Use per thread buffer area (malloc space, if necessary) */
    /* [NOTE: This buffer is shared between _strerror and streror.] */

    if ( (ptd->_terrmsg == NULL) && ((ptd->_terrmsg =
            _calloc_crt(_ERRMSGLEN_, sizeof(_TCHAR))) == NULL) )
            return(NULL);
    bldmsg = ptd->_terrmsg;

    /* Build the error message */

    bldmsg[0] = '\0';

    if (message && *message) {
        // should leave space for ": \n\0" 
        _ERRCHECK(_tcsncat_s( bldmsg, _ERRMSGLEN_, message, _ERRMSGLEN_-4 ));
        _ERRCHECK(_tcscat_s( bldmsg, _ERRMSGLEN_, _T(": ")));
    }

    //  We should have extra space for "\n\0"
    sysErrorMsg = _get_sys_err_msg(errno);

#ifdef _UNICODE
    _ERRCHECK(mbstowcs_s(NULL, bldmsg + wcslen(bldmsg), _ERRMSGLEN_ - wcslen(bldmsg), sysErrorMsg, _ERRMSGLEN_ - wcslen(bldmsg) - 2));
#else
    _ERRCHECK(strncat_s(bldmsg, _ERRMSGLEN_, sysErrorMsg, _ERRMSGLEN_ - strlen(bldmsg) - 2));
#endif

    _ERRCHECK(_tcscat_s( bldmsg, _ERRMSGLEN_, _T("\n")));
    return bldmsg;
}

/***
*errno_t _strerror_s(buffer, sizeInTChars, message) - get system error message
*
*Purpose:
*   builds an error message consisting of the users error message
*   (the message parameter), followed by ": ", followed by the system
*   error message (index through errno), followed by a newline.  If
*   message is NULL or a null string, returns a pointer to just
*   the system error message.
*
*Entry:
*   TCHAR * buffer - Destination buffer.
*   size_t sizeInTChars - Size of the destination buffer.
*   TCHAR * message - user's message to prefix system error message
*
*Exit:
*   The error code.
*
*Exceptions:
*   Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

#define _MIN_MSG_LENGTH 5

#ifdef _UNICODE
errno_t __cdecl __wcserror_s(
#else
errno_t __cdecl _strerror_s(
#endif
    TCHAR* buffer,
    size_t sizeInTChars,
    const _TCHAR *message
    )
{
    errno_t e = 0;

    /* validation section */
    _VALIDATE_RETURN_ERRCODE(buffer != NULL, EINVAL);
    _VALIDATE_RETURN_ERRCODE(sizeInTChars > 0, EINVAL);
    buffer[0] = '\0';

    if (message && 
        *message &&
        _tcslen(message) < (sizeInTChars - 2 - _MIN_MSG_LENGTH))
    {
        _ERRCHECK(_tcscpy_s(buffer, sizeInTChars, message));
        _ERRCHECK(_tcscat_s(buffer, sizeInTChars, _T(": ")));
    }

    /* append the error message at the end of the buffer */
    return _tcserror_s(buffer + _tcslen(buffer), sizeInTChars - _tcslen(buffer), errno);
}
