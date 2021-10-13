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
*mktemp.c - create a unique file name
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines _mktemp() - create a unique file name
*
*Revision History:
*       06-02-86  JMB   eliminated unneccesary routine exits
*       05-26-87  JCR   fixed bug where mktemp was incorrectly modifying
*                       the errno value.
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       07-11-88  JCR   Optimized REG allocation
*       03-12-90  GJF   Replaced _LOAD_DS with _CALLTYPE1, added #include
*                       <cruntime.h>, removed #include <register.h> and
*                       fixed the copyright. Also, cleaned up the formatting
*                       a bit.
*       04-04-90  GJF   Added #include <process.h> and #include <io.h>. Removed
*                       #include <sizeptr.h>.
*       07-23-90  SBM   Replaced <assertm.h> by <assert.h>
*       08-13-90  SBM   Compiles cleanly with -W3
*       09-28-90  GJF   New-style function declarator.
*       01-16-91  GJF   ANSI naming.
*       11-30-92  KRS   Ported _MBCS code from 16-bit tree.
*       06-18-93  KRS   MBCS-only update ported from 16-bit tree.
*       08-03-93  KRS   Call _ismbstrail instead of isdbcscode.
*       11-01-93  CFW   Enable Unicode variant.
*       02-21-94  SKS   Use ThreadID instead of ProcessID in multi-thread libs.
*       04-11-94  CFW   Fix first X handling, cycle 'a'-'z'.
*       02-06-95  CFW   assert -> _ASSERTE.
*       02-15-95  GJF   Appended Mac version of source file (somewhat cleaned
*                       up), with appropriate #ifdef-s.
*       03-28-96  GJF   Detab-ed. Also, replaced isdbcscode with __isdbcscode.
*       07-08-96  GJF   Replaced defined(_WIN32) with !defined(_MAC), and
*                       defined(_M_M68K) || defined(_M_MPPC) with 
*                       defined(_MAC). Removed obsolete REG* macros. Also, 
*                       cleaned up the format a bit.
*       05-17-99  PML   Remove all Macintosh support.
*       05-08-02  GB    Fix for VS7#504127, Fixed possiblity of buffer underrun.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-20-03  SJ    errno shouldn't be set to 0 on success
*       04-25-05  AC    Added debug filling
*                       VSW#2459
*
*******************************************************************************/

#include <mtdll.h>
#include <cruntime.h>
#include <stdio.h>
#include <io.h>
#include <process.h>
#include <errno.h>
#include <dbgint.h>
#include <stddef.h>
#ifdef  _MBCS
#include <mbctype.h>
#include <mbdata.h>
#endif
#include <tchar.h>
#include <internal.h>
#include <internal_securecrt.h>

/***
*_TSCHAR *_mktemp_s(template, size) - create a unique file name
*
*Purpose:
*       given a template of the form "fnamXXXXXX", insert number on end
*       of template, insert unique letter if needed until unique filename
*       found or run out of letters.  The number is generated from the Win32
*       Process ID for single-thread libraries, or the Win32 Thread ID for
*       multi-thread libraries.
*
*Entry:
*       _TSCHAR *template - template of form "fnamXXXXXX"
*       size_t size - size of the template buffer
*
*Exit:
*       returns 0 on success & sets pfile
*       returns errno_t on failure.
*       The template buffer contains the generated filename on success
*
*Exceptions:
*
*******************************************************************************/

errno_t  __cdecl _tmktemp_s (_TSCHAR *template, size_t sizeInChars)
{
        _TSCHAR *string = template;
        unsigned number;
        int letter = _T('a');
        size_t xcount = 0;
        errno_t save_errno;

        _VALIDATE_RETURN_ERRCODE((template != NULL) && (sizeInChars > 0), EINVAL);
        xcount = _tcsnlen(template, sizeInChars);
        if (xcount >= sizeInChars)
        {
            _RESET_STRING(template, sizeInChars);
            _RETURN_DEST_NOT_NULL_TERMINATED(template, sizeInChars);
        }
        _FILL_STRING(template, sizeInChars, xcount + 1);

        /*
         * The Process ID is not a good choice in multi-threaded programs
         * because of the likelihood that two threads might call mktemp()
         * almost simultaneously, thus getting the same temporary name.
         * Instead, the Win32 Thread ID is used, because it is unique across
         * all threads in all processes currently running.
         *
         * Note, however, that unlike *NIX process IDs, which are not re-used
         * until all values up to 32K have been used, Win32 process IDs are
         * re-used and tend to always be relatively small numbers.  Same for
         * thread IDs.
         */
        number = __threadid();
        
        /* string points to the terminating null */
        string += xcount;

        if((xcount < 6) || (sizeInChars <= xcount))
        {
            _RESET_STRING(template, sizeInChars);
            _VALIDATE_RETURN_ERRCODE(("Incorrect Input for mktemp", 0), EINVAL);
        }

        xcount = 0;
        
        /* replace last five X's */
#ifdef  _MBCS
        while ((--string>=template) && (!_ismbstrail(template,string))
                && (*string == 'X') && xcount < 5)
#else
        while ((--string>=template) && *string == _T('X') && xcount < 5)
#endif
        {
                xcount++;
                *string = (_TSCHAR)((number % 10) + '0');
                number /= 10;
        }

        /* too few X's ? */
        if (*string != _T('X') || xcount < 5)
        {
            _RESET_STRING(template, sizeInChars);
            _VALIDATE_RETURN_ERRCODE(("Incorrect Input for mktemp", 0), EINVAL);
        }

        /* set first X */
        *string = letter++;

        save_errno = errno;
        errno = 0;

        /* check all the files 'a'-'z' */
        while ((_taccess_s(template,0) == 0) || (errno == EACCES))
        /* while file exists */
        {
                if (letter == _T('z') + 1) {
                        _RESET_STRING(template, sizeInChars);
                        errno = EEXIST;
                        return errno;
                }

                *string = (_TSCHAR)letter++;
                errno = 0;
        }
        
        /* restore the old value of errno and return success */
        errno = save_errno;
        return 0;
}

/***
*_TSCHAR *_mktemp(template) - create a unique file name
*
*Purpose:
*       given a template of the form "fnamXXXXXX", insert number on end
*       of template, insert unique letter if needed until unique filename
*       found or run out of letters.  The number is generated from the Win32
*       Process ID for single-thread libraries, or the Win32 Thread ID for
*       multi-thread libraries.
*
*Entry:
*       _TSCHAR *template - template of form "fnamXXXXXX"
*
*Exit:
*       return pointer to modifed template
*       returns NULL if template malformed or no more unique names
*
*Exceptions:
*
*******************************************************************************/
_TSCHAR * __cdecl _tmktemp(
        _TSCHAR *template
        )
{
    errno_t e;
    
    _VALIDATE_RETURN( (template != NULL), EINVAL, NULL);
    
    e = _tmktemp_s(template, (size_t)(_tcslen(template) + 1));
    
    return  e ? NULL : template ;
}        
