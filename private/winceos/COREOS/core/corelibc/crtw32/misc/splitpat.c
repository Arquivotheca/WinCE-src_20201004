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
*splitpath.c - break down path name into components
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       To provide support for accessing the individual components of an
*       arbitrary path name
*
*Revision History:
*       06-14-87  DFW   initial implementation
*       09-23-87  JCR   Removed 'const' from declarations (fixed cl warnings)
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       11-20-89  GJF   Fixed indents, copyright. Added const attribute to
*                       type of path.
*       03-15-90  GJF   Replaced _LOAD_DS with _CALLTYPE1 and added #include
*                       <cruntime.h>.
*       07-25-90  SBM   Removed redundant include (stdio.h), replaced local
*                       MIN macro with standard min macro
*       10-04-90  GJF   New-style function declarator.
*       01-22-91  GJF   ANSI naming.
*       11-20-92  KRS   Port _MBCS support from 16-bit tree.
*       05-12-93  KRS   Add fix for MBCS max path handling.
*       12-07-93  CFW   Wide char enable.
*       10-15-95  BWT   _NTSUBSET_ doesn't do MBCS here.
*       09-09-96  JWM   Test length of input string before accessing (Orion 7985).
*       04-28-98  GJF   No more _ISLEADBYTE macro.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-20-03  SJ    errno shouldn't be set to 0 on success
*       02-24-04  AC    Do not zero-terminate the output buffer at function entry.
*       03-10-04  AC    Return ERANGE when buffer is too small
*       08-05-04  AC    Moved secure version in tsplitpath_s.inl and splitpath_s.c
*       08-12-04  AC    Use tsplitpath_helper
*       11-06-04  AC    Use _SECURECRT_FILL_BUFFER_PATTERN
*                       VSW#2459
*
*******************************************************************************/

#ifdef _NTSUBSET_
#undef _MBCS
#endif

/* we don't want to fill up the buffers in debug to mantain back-compat */
#define _SECURECRT_FILL_BUFFER 0

#include <stdlib.h>
#ifdef _MBCS
#include <mbstring.h>
#endif
#include <tchar.h>
#include <internal_securecrt.h>

/***
*_splitpath() - split a path name into its individual components
*
*Purpose:
*       to split a path name into its individual components
*
*Entry:
*       path  - pointer to path name to be parsed
*       drive - pointer to buffer for drive component, if any
*       dir   - pointer to buffer for subdirectory component, if any
*       fname - pointer to buffer for file base name component, if any
*       ext   - pointer to buffer for file name extension component, if any
*
*Exit:
*       drive - pointer to drive string.  Includes ':' if a drive was given.
*       dir   - pointer to subdirectory string.  Includes leading and trailing
*           '/' or '\', if any.
*       fname - pointer to file base name
*       ext   - pointer to file extension, if any.  Includes leading '.'.
*
*Exceptions:
*
*******************************************************************************/

#ifdef _UNICODE
#define _tsplitpath_helper _wsplitpath_helper
#else
#define _tsplitpath_helper _splitpath_helper
#endif

#define _FUNC_PROLOGUE static
#define _FUNC_NAME _tsplitpath_helper
#define _CHAR _TSCHAR

#ifndef _MBCS
#define _MBS_SUPPORT 0
#else
#define _MBS_SUPPORT 1
/* _splitpath uses _ismbblead and not _ismbblead_l */
#undef _ISMBBLEAD
#define _ISMBBLEAD(_Character) \
    _ismbblead((_Character))
#endif

/* _tsplitpath_helper does not call invalid_parameter if one of the buffer is too small */
#undef _RETURN_BUFFER_TOO_SMALL
#define _RETURN_BUFFER_TOO_SMALL(_String, _Size) \
    return (errno = ERANGE)

/* _tsplitpath_helper does not pad the string */
#define _TCSNCPY_S(_Dst, _Size, _Src, _Count) _tcsncpy_s((_Dst), ((size_t)-1), (_Src), (_Count))
#undef _RESET_STRING
#define _RESET_STRING(_String, _Size) \
    *(_String) = 0;

#include <tsplitpath_s.inl>

void __cdecl _tsplitpath (
        register const _TSCHAR *path,
        _TSCHAR *drive,
        _TSCHAR *dir,
        _TSCHAR *fname,
        _TSCHAR *ext
        )
{
    _tsplitpath_helper(
        path,
        drive, drive ? _MAX_DRIVE : 0,
        dir, dir ? _MAX_DIR : 0,
        fname, fname ? _MAX_FNAME : 0,
        ext, ext ? _MAX_EXT : 0);
}
