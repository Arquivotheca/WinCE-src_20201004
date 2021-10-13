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
*_freebuf.c - release a buffer from a stream
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines _freebuf() - release a buffer from a stream
*
*Revision History:
*       09-19-83  RN    initial version
*       02-15-90  GJF   Fixed copyright, alignment.
*       03-16-90  GJF   Replaced cdecl _LOAD_DS with _CALLTYPE1, added #include
*                       <cruntime.h> and removed #include <register.h>.
*       07-23-90  SBM   Replaced <assertm.h> by <assert.h>
*       10-03-90  GJF   New-style function declarator.
*       02-14-92  GJF   Replaced _nfile with _nhandle for Win32.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       06-22-93  GJF   Clear _IOSETVBUF flag (new).
*       09-05-94  SKS   Change "#ifdef" inside comments to "*ifdef" to avoid
*                       problems with CRTL source release process.
*       09-06-94  CFW   Replace MTHREAD with _MT.
*       01-04-95  GJF   _WIN32_ -> _WIN32.
*       01-10-95  CFW   Debug CRT allocs.
*       02-06-94  CFW   assert -> _ASSERTE.
*       02-16-95  GJF   Merged in Mac version.
*       09-06-95  GJF   Removed inappropriate ASSERTE()-s.
*       05-17-99  PML   Remove all Macintosh support.
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <file2.h>
#include <dbgint.h>
#include <internal.h>
#include <stdlib.h>

/***
*void _freebuf(stream) - release a buffer from a stream
*
*Purpose:
*       free a buffer if at all possible. free() the space if malloc'd by me.
*       forget about trying to free a user's buffer for him; it may be static
*       memory (not from malloc), so he has to take care of it. this function
*       is not intended for use outside the library.
*
*       Multi-thread notes:
*       _freebuf() does NOT get the stream lock; it is assumed that the
*       caller has already done this.
*
*Entry:
*       FILE *stream - stream to free bufer on
*
*Exit:
*       Buffer may be freed.
*       No return value.
*
*Exceptions:
*
*******************************************************************************/

void __cdecl _freebuf (
        FILE *stream
        )
{
        _ASSERTE(stream != NULL);

        if (inuse(stream) && mbuf(stream))
        {
                _free_crt(stream->_base);

                stream->_flag &= ~(_IOMYBUF | _IOSETVBUF);
                stream->_base = stream->_ptr = NULL;
                stream->_cnt = 0;
        }
}
