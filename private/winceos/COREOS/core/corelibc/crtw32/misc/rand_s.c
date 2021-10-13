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
*rand_s.c - random number generator
*
*
*Purpose:
*   defines rand_s() - random number generator
*
*******************************************************************************/

#if defined(WINCEMACRO) && defined(COREDLL)
#define CeGenRandom xxx_GenRandom
#endif

#include <windows.h>
#include <bcrypt.h>
#include <cruntime.h>
#include <stddef.h>
#include <stdlib.h>
#include <internal.h>
#include <coredll.h>

#pragma warning(disable: 4054 4055) // 'type cast' : to/from function pointer 'pfn' from/to data pointer 'void *'

/***
*errno_t rand_s(unsigned int * RandomValue)
*
*Purpose:
*   Returns a random number or fails if at least CeGenRandom isn't available.
*
*Entry:
*   Non NULL out parameter.
*
*Exit:
*   errno_t - 0 if sucessful
*             error value on failure
*
*   Out parameter -
*             set to random value on success
*             set to 0 on error
*
*Exceptions:
*
*******************************************************************************/

typedef NTSTATUS (WINAPI * PBCRYPTGENRANDOM)(BCRYPT_ALG_HANDLE, PUCHAR, ULONG, ULONG);

#define NOT_AVAILABLE ((PBCRYPTGENRANDOM)(-1))

static PBCRYPTGENRANDOM g_pfnBCryptGenRandom;

errno_t __cdecl rand_s
(
    unsigned int * RandomValue
)
{
    NTSTATUS status = STATUS_INVALID_PARAMETER;
    PBCRYPTGENRANDOM pfnGenRandom = (PBCRYPTGENRANDOM)_decode_pointer((PVOID)g_pfnBCryptGenRandom);

    _VALIDATE_RETURN_ERRCODE(RandomValue != NULL, EINVAL);

    *RandomValue = 0;

    if (pfnGenRandom == NULL)
    {
        PBCRYPTGENRANDOM encoded;
        const void* enull;

        // BCrypt.dll is unloaded when the app exits.
        //
        HMODULE hBCrypt = LoadLibraryW(L"BCRYPT.DLL");
        if (hBCrypt != NULL)
        {
            pfnGenRandom = (PBCRYPTGENRANDOM)GetProcAddressA(hBCrypt, "BCryptGenRandom");
        }

        if (pfnGenRandom == NULL)
        {
            pfnGenRandom = NOT_AVAILABLE;
        }

        // Update the global pointer so we don't have to go through this again.
        //
        encoded = (PBCRYPTGENRANDOM)_encode_pointer((PVOID)pfnGenRandom);
        enull = _encoded_null();
        if (InterlockedExchangePointer(
                (void**)&g_pfnBCryptGenRandom,
                (void*)encoded)
            != enull)
        {
            if (hBCrypt != NULL)
            {
                // A different thread has already loaded BCrypt.dll.
                //
                FreeLibrary(hBCrypt);
            }
        }
    }

    _ASSERTE(pfnGenRandom != NULL);

    if (pfnGenRandom != NOT_AVAILABLE)
    {
        status = pfnGenRandom(
                    NULL,
                    (PUCHAR)RandomValue,
                    sizeof(*RandomValue),
                    BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    }

    // Fall back on CeGenRandom if BCryptGenRandom is unavailable or fails.
    //
    if (!NT_SUCCESS(status) && CeGenRandom(sizeof(*RandomValue), (PBYTE)RandomValue))
    {
        status = STATUS_SUCCESS;
    }

    if (!NT_SUCCESS(status))
    {
        _SET_ERRNO(ENOSYS);
        return ENOSYS;
    }

    return 0;
}

