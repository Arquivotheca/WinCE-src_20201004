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
*rmtmp.c - remove temporary files created by tmpfile.
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*
*Revision History:
*       09-15-83  TC    initial version
*       11-02-87  JCR   Multi-thread support
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       05-27-88  PHG   Merged normal and DLL versions
*       06-10-88  JCR   Use near pointer to reference _iob[] entries
*       08-18-89  GJF   Clean up, now specific to OS/2 2.0 (i.e., 386 flat
*                       model). Also fixed copyright and indents.
*       02-15-90  GJF   Fixed copyright
*       03-19-90  GJF   Made calling type _CALLTYPE1, added #include
*                       <cruntime.h> and removed #include <register.h>.
*       10-03-90  GJF   New-style function declarator.
*       01-21-91  GJF   ANSI naming.
*       07-30-91  GJF   Added support for termination scheme used on
*                       non-Cruiser targets [_WIN32_].
*       03-11-92  GJF   Replaced _tmpnum(stream) with stream->_tmpfname for
*                       Win32.
*       03-17-92  GJF   Got rid of definition of _tmpoff.
*       03-31-92  GJF   Merged with Stevesa's changes.
*       04-16-92  GJF   Merged with Darekm's changes.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       10-29-93  GJF   Define entry for termination section (used to be in
*                       i386\cinittmp.asm). Also, replaced MTHREAD with _MT
*                       and deleted old Cruiser support.
*       04-04-94  GJF   #ifdef-ed out definition _tmpoff for msvcrt*.dll, it
*                       is unnecessary. Made definitions of _tempoff and
*                       _old_pfxlen conditional on ndef DLL_FOR_WIN32S.
*       02-20-95  GJF   Merged in Mac version.
*       03-07-95  GJF   Converted to walk the __piob[] table (rather than
*                       the _iob[] table).
*       03-16-95  GJF   Must be sure __piob[i]!=NULL before trying to lock it!
*       03-28-95  SKS   Fix declaration of _prmtmp (__cdecl goes BEFORE the *)
*       08-01-96  RDK   For PMac, change termination pointer data type to static.
*       03-02-98  GJF   Exception-safe locking.
*       04-28-99  PML   Wrap __declspec(allocate()) in _CRTALLOC macro.
*       05-13-99  PML   Remove Win32s
*       05-17-99  PML   Remove all Macintosh support.
*       02-19-01  PML   Avoid allocating unnecessary locks in _rmtmp, part of
*                       vs7#172586.
*       02-11-05  AC    Remove redundant #pragma section
*                       VSW#445138
*
*******************************************************************************/

#include <sect_attribs.h>
#include <cruntime.h>
#include <stdio.h>
#include <file2.h>
#include <internal.h>
#include <mtdll.h>

_CRTALLOC(".CRT$XPX") static _PVFV pterm = _rmtmp;

/*
 * Definitions for _tmpoff, _tempoff and _old_pfxlen. These will cause this
 * module to be linked in whenever the termination code needs it.
 */
#ifndef CRTDLL
unsigned _tmpoff = 1;
#endif  /* CRTDLL */

unsigned _tempoff = 1;
unsigned _old_pfxlen = 0;


/***
*int _rmtmp() - closes and removes temp files created by tmpfile
*
*Purpose:
*       closes and deletes all open files that were created by tmpfile.
*
*Entry:
*       None.
*
*Exit:
*       returns number of streams closed
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _rmtmp (
        void
        )
{
    int count = 0;
    int i;

    _mlock(_IOB_SCAN_LOCK);
    __try {
        for ( i = 0 ; i < _nstream ; i++)
            if ( __piob[i] != NULL && inuse( (FILE *)__piob[i] )) {
                /*
                 * lock the stream. this is not done until testing
                 * the stream is in use to avoid unnecessarily creating
                 * a lock for every stream. the price is having to
                 * retest the stream after the lock has been asserted.
                 */
                _lock_str2(i, __piob[i]);
                __try {
                    /*
                     * if the stream is STILL in use (it may have
                     * been closed before the lock was asserted),
                     * see about flushing it.
                     */
                    if ( inuse( (FILE *)__piob[i] )) {
                        if ( ((FILE *)__piob[i])->_tmpfname != NULL )
                        {
                            _fclose_nolock( __piob[i] );
                            count++;
                        }
                    }
                }
                __finally {
                    _unlock_str2(i, __piob[i]);
                }
            }
        }
        __finally {
            _munlock(_IOB_SCAN_LOCK);
        }

        return(count);
}
