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
*a_env.c - A version of GetEnvironmentStrings.
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Use GetEnvironmentStringsW if available, otherwise use A version.
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
*       05-14-97  GJF   Split off W version into another file and renamed this
*                       one as a_env.c.
*       03-03-98  RKP   Supported 64 bits
*       05-17-00  GB    Use ERROR_CALL_NOT_IMPLEMENTED for existance of W API
*
*******************************************************************************/

#include <cruntime.h>
#include <internal.h>
#include <stdlib.h>
#include <setlocal.h>
#include <awint.h>
#include <dbgint.h>

/***
*LPVOID __cdecl __crtGetEnvironmentStringsA - Get normal environment block
*
*Purpose:
*       Internal support function. Since GetEnvironmentStrings returns in OEM
*       and we want ANSI (note that GetEnvironmentVariable returns ANSI!) and
*       SetFileApistoAnsi() does not affect it, we have no choice but to 
*       obtain the block in wide character and convert to ANSI.
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

LPVOID __cdecl __crtGetEnvironmentStringsA(
        VOID
        )
{
#ifdef _WIN32_WCE // CE doesn't support GetEnvironmentStrings
        return NULL;
#else
        wchar_t *wEnv;
        wchar_t *wTmp;
        char *aEnv = NULL;
        int nSizeW;
        int nSizeA;

        /* obtain wide environment block */

        if ( NULL == (wEnv = GetEnvironmentStringsW()) )
            return NULL;

        /* look for double null that indicates end of block */
        wTmp = wEnv;
        while ( *wTmp != L'\0' ) {
            if ( *++wTmp == L'\0' )
                wTmp++;
        }

        /* calculate total size of block, including all nulls */
        nSizeW = (int)(wTmp - wEnv + 1);

        /* find out how much space needed for multi-byte environment */
        nSizeA = WideCharToMultiByte(   CP_ACP,
                                        0,
                                        wEnv,
                                        nSizeW,
                                        NULL,
                                        0,
                                        NULL,
                                        NULL );

        /* allocate space for multi-byte string */
        if ( (nSizeA == 0) ||
             ((aEnv = (char *)_malloc_crt(nSizeA)) == NULL) )
        {
            FreeEnvironmentStringsW( wEnv );
            return NULL;
        }

        /* do the conversion */
        if ( !WideCharToMultiByte(  CP_ACP,
                                    0,
                                    wEnv,
                                    nSizeW,
                                    aEnv,
                                    nSizeA,
                                    NULL,
                                    NULL ) )
        {
            _free_crt( aEnv );
            aEnv = NULL;
        }

        FreeEnvironmentStringsW( wEnv );
        return aEnv;
#endif // _WIN32_WCE
}
