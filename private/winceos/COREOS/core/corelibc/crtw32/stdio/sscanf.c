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
*sscanf.c - read formatted data from string
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines scanf() - reads formatted data from string
*
*Revision History:
*       09-02-83  RN    initial version
*       04-13-87  JCR   added const to declaration
*       06-24-87  JCR   (1) Made declaration conform to ANSI prototype and use
*                       the va_ macros; (2) removed SS_NE_DS conditionals.
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       06-13-88  JCR   Fake _iob entry is now static so that other routines can
*                       assume _iob entries are in DGROUP.
*       06-06-89  JCR   386 mthread support -- threads share one locked iob.
*       08-18-89  GJF   Clean up, now specific to OS/2 2.0 (i.e., 386 flat
*                       model). Also fixed indents.
*       02-16-90  GJF   Fixed copyright
*       03-19-90  GJF   Made calling type _CALLTYPE2, added #include
*                       <cruntime.h> and removed #include <register.h>.
*       03-26-90  GJF   Added #include <string.h>.
*       07-23-90  SBM   Replaced <assertm.h> by <assert.h>
*       10-03-90  GJF   New-style function declarator.
*       02-18-93  SRW   Make FILE a local and remove lock usage.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       02-06-94  CFW   assert -> _ASSERTE.
*       01-06-99  GJF   Changes for 64-bit size_t.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-23-03  SJ    Secure Version for scanf family which takes an extra 
*                       size parameter from the var arg list.
*       01-21-05  MSL   Fix integer overflow issue with output sizes
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <dbgint.h>
#include <stdarg.h>
#include <string.h>
#include <internal.h>
#include <mtdll.h>
#include <tchar.h>

/***
*static int vscan_fn([w]inputfn, string, [count], format, ...)
*
*Purpose:
*       this is a helper function which is called by the other functions
*       in this file - sscanf/swscanf/snscanf etc. It calls either _(w)input or
*       _(w)input_s depending on the first parameter.
*
*******************************************************************************/
static int __cdecl vscan_fn (
        TINPUTFN inputfn,
        const _TCHAR *string,
#ifdef _SNSCANF
        size_t count,
#endif
        const _TCHAR *format,
        _locale_t plocinfo,
        va_list arglist
        )
/*
 * 'S'tring 'SCAN', 'F'ormatted
 */
{
        FILE str = { 0 };
        FILE *infile = &str;
        int retval;
#ifndef _SNSCANF
        size_t count;
#endif

        _VALIDATE_RETURN( (string != NULL), EINVAL, EOF);
        _VALIDATE_RETURN( (format != NULL), EINVAL, EOF);

#ifndef _SNSCANF
        count=_tcslen(string);
#endif

        infile->_flag = _IOREAD|_IOSTRG|_IOMYBUF;
        infile->_ptr = infile->_base = (char *) string;

        if(count>(INT_MAX/sizeof(_TCHAR)))
        {
            /* old-style functions allow any large value to mean unbounded */
            infile->_cnt = INT_MAX;
        }
        else
        {
            infile->_cnt = (int)count*sizeof(_TCHAR);
        }

        retval = (inputfn(infile, format, plocinfo, arglist));

        return(retval);
}

/***
*int sscanf(string, format, ...) - read formatted data from string
*
*Purpose:
*       Reads formatted data from string into arguments.  _input does the real
*       work here.  Sets up a FILE so file i/o operations can be used, makes
*       string look like a huge buffer to it, but _filbuf will refuse to refill
*       it if it is exhausted.
*
*       Allocate the 'fake' _iob[] entryit statically instead of on
*       the stack so that other routines can assume that _iob[] entries are in
*       are in DGROUP and, thus, are near.
*
*       Multi-thread: (1) Since there is no stream, this routine must never try
*       to get the stream lock (i.e., there is no stream lock either).  (2)
*       Also, since there is only one staticly allocated 'fake' iob, we must
*       lock/unlock to prevent collisions.
*
*Entry:
*       char *string - string to read data from
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
/***
*int snscanf(string, size, format, ...) - read formatted data from string of 
*    given length
*
*Purpose:
*       Reads formatted data from string into arguments.  _input does the real
*       work here.  Sets up a FILE so file i/o operations can be used, makes
*       string look like a huge buffer to it, but _filbuf will refuse to refill
*       it if it is exhausted.
*
*       Allocate the 'fake' _iob[] entryit statically instead of on
*       the stack so that other routines can assume that _iob[] entries are in
*       are in DGROUP and, thus, are near.
*
*       Multi-thread: (1) Since there is no stream, this routine must never try
*       to get the stream lock (i.e., there is no stream lock either).  (2)
*       Also, since there is only one staticly allocated 'fake' iob, we must
*       lock/unlock to prevent collisions.
*
*Entry:
*       char *string - string to read data from
*       size_t count - length of string
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
#ifdef _UNICODE
#ifdef _SNSCANF
int __cdecl _snwscanf (
#else
int __cdecl swscanf (
#endif
#else
#ifdef _SNSCANF
int __cdecl _snscanf (
#else
int __cdecl sscanf (
#endif
#endif
        const _TCHAR *string,
#ifdef _SNSCANF
        size_t count,
#endif
        const _TCHAR *format,
        ...
        )
{
        va_list arglist;
        va_start(arglist, format);
#ifdef _SNSCANF
        return vscan_fn(_tinput_l, string, count, format, NULL, arglist);
#else
        return vscan_fn(_tinput_l, string, format, NULL, arglist);
#endif

}

#ifdef _UNICODE
#ifdef _SNSCANF
int __cdecl _snwscanf_l (
#else
int __cdecl _swscanf_l (
#endif
#else
#ifdef _SNSCANF
int __cdecl _snscanf_l (
#else
int __cdecl _sscanf_l (
#endif
#endif
        const _TCHAR *string,
#ifdef _SNSCANF
        size_t count,
#endif
        const _TCHAR *format,
        _locale_t plocinfo,
        ...
        )
{
        va_list arglist;
        va_start(arglist, plocinfo);
#ifdef _SNSCANF
        return vscan_fn(_tinput_l, string, count, format, plocinfo, arglist);
#else
        return vscan_fn(_tinput_l, string, format, plocinfo, arglist);
#endif

}

/***
*int sscanf_s(string, format, ...)
*   Same as sscanf above except that it calls _input_s to do the real work.
*
*int snscanf_s(string, size, format, ...)
*   Same as snscanf above except that it calls _input_s to do the real work.
*
*   _input_s has a size check for array parameters.
*
*******************************************************************************/
#ifdef _UNICODE
#ifdef _SNSCANF
int __cdecl _snwscanf_s (
#else
int __cdecl swscanf_s (
#endif
#else
#ifdef _SNSCANF
int __cdecl _snscanf_s (
#else
int __cdecl sscanf_s (
#endif
#endif
        const _TCHAR *string,
#ifdef _SNSCANF
        size_t count,
#endif
        const _TCHAR *format,
        ...
        )
{
        va_list arglist;
        va_start(arglist, format);
#ifdef _SNSCANF
        return vscan_fn(_tinput_s_l, string, count, format, NULL, arglist);
#else
        return vscan_fn(_tinput_s_l, string, format, NULL, arglist);
#endif

}

#ifdef _UNICODE
#ifdef _SNSCANF
int __cdecl _snwscanf_s_l (
#else
int __cdecl _swscanf_s_l (
#endif
#else
#ifdef _SNSCANF
int __cdecl _snscanf_s_l (
#else
int __cdecl _sscanf_s_l (
#endif
#endif
        const _TCHAR *string,
#ifdef _SNSCANF
        size_t count,
#endif
        const _TCHAR *format,
        _locale_t plocinfo,
        ...
        )
{
        va_list arglist;
        va_start(arglist, plocinfo);
#ifdef _SNSCANF
        return vscan_fn(_tinput_s_l, string, count, format, plocinfo, arglist);
#else
        return vscan_fn(_tinput_s_l, string, format, plocinfo, arglist);
#endif

}

