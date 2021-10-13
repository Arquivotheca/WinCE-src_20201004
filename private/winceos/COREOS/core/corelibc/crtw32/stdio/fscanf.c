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
*fscanf.c - read formatted data from stream
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines fscanf() - reads formatted data from stream
*
*Revision History:
*       09-02-83  RN    initial version
*       04-13-87  JCR   added const to declaration
*       06-24-87  JCR   (1) Made declaration conform to ANSI prototype and use
*                       the va_ macros; (2) removed SS_NE_DS conditionals.
*       11-06-87  JCR   Multi-thread support
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       05-31-88  PHG   Merged DLL and normal versions
*       02-15-90  GJF   Fixed copyright and indents
*       03-19-90  GJF   Replaced _LOAD_DS with _CALLTYPE2 and added #include
*                       <cruntime.h>.
*       03-26-90  GJF   Added #include <internal.h>.
*       07-23-90  SBM   Replaced <assertm.h> by <assert.h>
*       10-02-90  GJF   New-style function declarator.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       09-06-94  CFW   Replace MTHREAD with _MT.
*       02-06-94  CFW   assert -> _ASSERTE.
*       03-07-95  GJF   _[un]lock_str macros now take FILE * arg.
*       03-02-98  GJF   Exception-safe locking.
*       10-23-03  SJ    Secure Version for scanf family which takes an extra 
*                       size parameter from the var arg list.
*       12-02-03  SJ    Reroute Unicode I/O
*       03-13-04  MSL   Avoid returning from __try for prefast
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <dbgint.h>
#include <stdarg.h>
#include <file2.h>
#include <internal.h>
#include <mtdll.h>

/***
*int vfscanf(stream, format, ...) - read formatted data from stream
*
*Purpose:
*       This is a helper function to be called from fscanf & fscanf_s
*
*Entry:
*       INPUTFN inputfn - fscanf & fscanf_s pass either _input or _input_s
*                   which is then used to do the real work.            
*       FILE *stream    - stream to read data from
*       char *format    - format string
*       va_list arglist - arglist of output pointers
*
*Exit:
*       returns number of fields read and assigned
*
*Exceptions:
*
*******************************************************************************/

int __cdecl vfscanf (
        INPUTFN inputfn,
        FILE *stream,
        const char *format,
        _locale_t plocinfo,
        va_list arglist
        )
/*
 * 'F'ile (stream) 'SCAN', 'F'ormatted
 */
{
    int retval=0;

    _VALIDATE_RETURN((stream != NULL), EINVAL, EOF);
    _VALIDATE_RETURN((format != NULL), EINVAL, EOF);
    _CHECK_IO_INIT(EOF);

    _lock_str(stream);
    __try {
        _VALIDATE_STREAM_ANSI_SETRET(stream, EINVAL, retval, EOF);

        if (retval==0)
        {
            retval = (inputfn(stream, format, plocinfo, arglist));
        }
    }
    __finally {
        _unlock_str(stream);
    }

    return(retval);
}

/***
*int fscanf(stream, format, ...) - read formatted data from stream
*
*Purpose:
*       Reads formatted data from stream into arguments.  _input does the real
*       work here.
*
*Entry:
*       FILE *stream - stream to read data from
*       char *format - format string
*       followed by list of pointers to storage for the data read.  The number
*       and type are controlled by the format string.
*
*Exit:
*       returns number of fields read and assigned
*
*Exceptions:
*
*******************************************************************************/

int __cdecl fscanf (
        FILE *stream,
        const char *format,
        ...
        )
{
        va_list arglist;
        va_start(arglist, format);
        return vfscanf(_input_l, stream, format, NULL, arglist);
}

int __cdecl _fscanf_l (
        FILE *stream,
        const char *format,
        _locale_t plocinfo,
        ...
        )
{
        va_list arglist;
        va_start(arglist, plocinfo);
        return vfscanf(_input_l, stream, format, plocinfo, arglist);
}

/***
*int fscanf_s(stream, format, ...) 
*
*   Same as fscanf above except that it calls _input_s to do the real work.
*   _input_s has a size check for array parameters.
*
*******************************************************************************/

int __cdecl fscanf_s (
        FILE *stream,
        const char *format,
        ...
        )
{
        va_list arglist;
        va_start(arglist, format);
        return vfscanf(_input_s_l, stream, format, NULL, arglist);
}

int __cdecl _fscanf_s_l (
        FILE *stream,
        const char *format,
        _locale_t plocinfo,
        ...
        )
{
        va_list arglist;
        va_start(arglist, plocinfo);
        return vfscanf(_input_s_l, stream, format, plocinfo, arglist);
}
