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
*openo.cpp - C++ Version of open which takes a default pmode parameter
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*
*Revision History:
*       09-11-03  SJ    Created Initial File
*       09-03-04  AC    Call the helper function and do not validate pmode
*                       VSW#350466
*       09-16-04  AC    Changed the signature of _sopen_s family
*                       VSW#362611
*
*******************************************************************************/

#ifndef _POSIX_

#include <io.h>
#include <share.h>
#include <internal.h>

_CRTIMP int __cdecl _open(const char * path, int oflag, int pmode /* = 0 */)
{
    int fh;
    /* Last parameter passed as 0 because we don't want to validate pmode from _open */
    errno_t e = _sopen_helper(path, oflag, _SH_DENYNO, pmode, &fh, 0);
    return e ? -1 : fh;
}

_CRTIMP int __cdecl _sopen(const char * path, int oflag, int shflag, int pmode /* = 0 */)
{
    int fh;
    /* Last parameter passed as 0 because we don't want to validate pmode from _sopen */
    errno_t e = _sopen_helper(path, oflag, shflag, pmode, &fh, 0);
    return e ? -1 : fh;
}

_CRTIMP int __cdecl _wopen(const wchar_t * path, int oflag, int pmode /* = 0 */)
{
    int fh;
    /* Last parameter passed as 0 because we don't want to validate pmode from _wopen */
    errno_t e = _wsopen_helper(path, oflag, _SH_DENYNO, pmode, &fh, 0);
    return e ? -1 : fh;
}

_CRTIMP int __cdecl _wsopen(const wchar_t * path, int oflag, int shflag, int pmode /* = 0 */)
{
    int fh;
    /* Last parameter passed as 0 because we don't want to validate pmode from _wsopen */
    errno_t e = _wsopen_helper(path, oflag, shflag, pmode, &fh, 0);
    return e ? -1 : fh;    
}

#endif
