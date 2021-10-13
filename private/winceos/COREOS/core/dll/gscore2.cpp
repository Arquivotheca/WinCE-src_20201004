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
*gscore2.c - coredll-specific security error handling routines
*
*Purpose:
*       Define __security_gen_cookie_cgr which is called instead of __security_gen_cookie
*       if the filesystem component is included.
*
*Entrypoints:
*       __security_gen_cookie_cgr
*
*******************************************************************************/

#if defined(WINCEMACRO) && defined(COREDLL)
#define CeGenRandom xxx_GenRandom
#endif

#include <windows.h>
#include <sha.h>

extern "C" UINT_PTR __cdecl __security_gen_cookie2();

/***
*__security_gen_cookie_cgr() - generate a cookie for the /GS runtime
*
*Purpose:
*   Same as __security_gen_cookie2, but attempts to use CeGenRandom before
*   falling back on the old algorithm.
*
*Entry:
*
*Exit:
*   returns the generated value
*
*Exceptions:
*
*******************************************************************************/

extern "C"
UINT_PTR
__cdecl
__security_gen_cookie_cgr()
{
    UINT_PTR cookie = 0;
    BOOL fRet = FALSE;

    if (WaitForAPIReady(SH_FILESYS_APIS, 0) == WAIT_OBJECT_0)
    {
        fRet = CeGenRandom(sizeof(cookie), reinterpret_cast<BYTE *>(&cookie));
    }

    if (!fRet)
    {
        /*
         * Fall back on poor sources of entropy used by __security_gen_cookie2,
         * but use SHA to combine them.
         */

        FILETIME ft = {0};
        LARGE_INTEGER perfctr = {0};
        UINT_PTR hashBuf[7];
        A_SHA_CTX shaCtx;

        GetCurrentFT(&ft);
        hashBuf[0] = ft.dwLowDateTime;
        hashBuf[1] = ft.dwHighDateTime;

        hashBuf[2] = GetCurrentProcessId();
        hashBuf[3] = GetCurrentThreadId();
        hashBuf[4] = GetTickCount();

        QueryPerformanceCounter(&perfctr);
        hashBuf[5] = perfctr.LowPart;
        hashBuf[6] = perfctr.HighPart;

        A_SHAInit(&shaCtx);
        A_SHAUpdate(&shaCtx, (BYTE *)hashBuf, sizeof(hashBuf));
        C_ASSERT(sizeof(hashBuf) >= A_SHA_DIGEST_LEN);
        A_SHAFinal(&shaCtx, (BYTE *)hashBuf);

        cookie = hashBuf[0];
    }

    return cookie;
}

