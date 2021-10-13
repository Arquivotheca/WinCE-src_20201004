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
*wperror.c - print system error message (wchar_t version)
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines _wperror() - print wide system error message
*       System error message are indexed by errno.
*
*Revision History:
*       12-07-93  CFW   Module created from perror.
*       02-07-94  CFW   POSIXify.
*       01-10-95  CFW   Debug CRT allocs.
*       01-06-98  GJF   Exception-safe locking.
*       09-23-98  GJF   Fixed handling of NULL or empty string arg.
*       01-06-99  GJF   Changes for 64-bit size_t.
*       05-08-02  GB    Fix for VS7#514437, Fixed possiblity of heap overflow.
*       04-01-05  MSL   Integer overflow protection
*
*******************************************************************************/

#ifndef _POSIX_

#include <cruntime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syserr.h>
#include <mtdll.h>
#include <io.h>
#include <dbgint.h>
#include <limits.h>
#include <internal.h>

/***
*void _wperror(wmessage) - print system error message
*
*Purpose:
*       prints user's error message, then follows it with ": ", then the system
*       error message, then a newline.  All output goes to stderr.  If user's
*       message is NULL or a null string, only the system error message is
*       printer.  If errno is weird, prints "Unknown error".
*
*Entry:
*       const wchar_t *wmessage - users message to prefix system error message
*
*Exit:
*       Prints message; no return value.
*
*Exceptions:
*
*******************************************************************************/

void __cdecl _wperror (
        const wchar_t *wmessage
        )
{
        int fh = 2;
        size_t size = 0;
        char *amessage;
        const char *sysmessage;

        /* ensure I/O buffers are initialized */
        _CHECK_IO_INIT_NOERRNO();

        /* convert WCS string into ASCII string */

        if ( wmessage && *wmessage )
        {
            _ERRCHECK_EINVAL_ERANGE(wcstombs_s( &size, NULL, 0, wmessage, INT_MAX));

            if ( size==0 || (amessage = (char *)_calloc_crt(size, sizeof(char))) == NULL )
                return;

            if ( _ERRCHECK_EINVAL_ERANGE(wcstombs_s(NULL, amessage, size, wmessage, _TRUNCATE)) != 0)
            {
                _free_crt(amessage);
                return;
            }
        }
        else
            amessage = NULL;

        _lock_fh( fh );         /* acquire file handle lock */
        __try {
			if ( amessage )
			{
					_write_nolock(fh,(char *)amessage,(unsigned)strlen(amessage));
					_write_nolock(fh,": ",2);
			}

			_free_crt(amessage);    /* note: freeing NULL is legal and benign */

			sysmessage = _get_sys_err_msg( errno );
			_write_nolock(fh, sysmessage,(unsigned)strlen(sysmessage));
			_write_nolock(fh,"\n",1);
        }
        __finally {
            _unlock_fh( fh );   /* release file handle lock */
        }
}

#endif /* _POSIX_ */
