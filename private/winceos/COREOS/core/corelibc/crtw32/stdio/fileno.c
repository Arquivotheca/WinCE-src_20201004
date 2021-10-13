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
*fileno.c - defines _fileno()
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	Defines fileno() - return the file handle for the specified stream
*
*Revision History:
*	03-13-89  GJF	Module created
*	03-27-89  GJF	Moved to 386 tree
*	02-15-90  GJF	_file is now an int. Also, fixed copyright.
*	03-19-90  GJF	Made calling type _CALLTYPE1 and added #include
*			<cruntime.h>.
*	10-02-90  GJF	New-style function declarator.
*	01-21-91  GJF	ANSI naming.
*	04-06-93  SKS	Replace _CRTAPI* with __cdecl
*	09-11-03  SJ	Secure CRT Work - Assertions & Validations
*
*******************************************************************************/

#include <cruntime.h>
#include <internal.h>
#include <stdio.h>
#ifdef _WIN32_WCE
#include <msdos.h>
#include <io.h>
#endif

/* remove macro definition for fileno()
 */
#undef	_fileno

/***
*int _fileno(stream) - return the file handle for stream
*
*Purpose:
*	Returns the file handle for the given stream is. Normally fileno()
*	is a macro, but it is also available as a true function (for
*	consistency with ANSI, though it is not required).
*
*Entry:
*	FILE *stream - stream to fetch handle for
*
*Exit:
*	returns the file handle for the given stream
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _fileno (
	FILE *stream
	)
{

	_VALIDATE_RETURN((stream != NULL), EINVAL, -1);
#ifdef _WIN32_WCE
    if ( !(stream->_flag & _IOSTRG) && stream->_file < 3 ) {
        int fh = stream->_file;

        _CHECK_IO_INIT(-1);
        _lock_fh(fh);
        if (_osfhnd(fh) == (intptr_t)INVALID_HANDLE_VALUE) {
            BOOL is_dev = TRUE;
            _osfhnd(fh) = (intptr_t)OpenStdConsole(fh, &is_dev);
            if (!is_dev)
                _osfile(fh) &= (char)~FDEV;
        }
        _unlock_fh(fh);
    }
#endif

	return( stream->_file );
}
