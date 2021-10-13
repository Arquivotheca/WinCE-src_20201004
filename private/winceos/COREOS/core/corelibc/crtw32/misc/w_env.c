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
*w_env.c - W version of GetEnvironmentStrings.
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Wrapper for GetEnvironmentStringsW.
*
*Revision History:
*       03-29-94  CFW   Module created.
*       12-27-94  CFW   Call direct, all OS's have stubs.
*       01-10-95  CFW   Debug CRT allocs.
*       04-07-95  CFW   Create __crtGetEnvironmentStringsA.
*       07-03-95  GJF   Modified to always malloc a buffer for the 
*                       environment strings, and to free the OS's buffer.
*       06-10-96  GJF   Initialize aEnv and wEnv to NULL in
*                       __crtGetEnvironmentStringsA. Also, detab-ed.
*       05-14-97  GJF   Split off from aw_env.c.
*       03-03-98  RKP   Supported 64 bits
*       08-21-98  GJF   Use CP_ACP instead of __lc_codepage.
*       01-08-99  GJF   Changes for 64-bit size_t.
*       05-17-00  GB    Use ERROR_CALL_NOT_IMPLEMENTED for existance of W API
*       04-01-05  MSL   Integer overflow protection
*       01-22-06  JKS   Dead code removal for Win95
*
*******************************************************************************/

#include <cruntime.h>
#include <internal.h>
#include <stdlib.h>
#include <setlocal.h>
#include <awint.h>
#include <dbgint.h>

/***
*LPVOID __cdecl __crtGetEnvironmentStringsW - Get wide environment.
*
*Purpose:
*       Internal support function.
*
*Entry:
*       VOID
*
*Exit:
*       LPVOID - pointer to environment block
*
*Exceptions:
*
*******************************************************************************/

LPVOID __cdecl __crtGetEnvironmentStringsW(
        VOID
        )
{
#ifdef _WIN32_WCE // CE doesn't support GetEnvironmentStrings
    return NULL; 
#else
    void *penv;
    wchar_t *pwch;
    wchar_t *wbuffer;
    int total_size;

    if ( NULL == (penv = GetEnvironmentStringsW()) )
        return NULL;

    /* find out how big a buffer is needed */

    pwch = penv;
    while ( *pwch != L'\0' ) {
        if ( *++pwch == L'\0' )
            pwch++;
    }

    total_size = (int)((char *)pwch - (char *)penv) +
                 (int)sizeof( wchar_t );

    /* allocate the buffer */

    if ( NULL == (wbuffer = _malloc_crt( total_size )) ) {
        FreeEnvironmentStringsW( penv );
        return NULL;
    }

    /* copy environment strings to buffer */

    memcpy( wbuffer, penv, total_size );

    FreeEnvironmentStringsW( penv );

    return (LPVOID)wbuffer;
#endif // _WIN32_WCE
}
