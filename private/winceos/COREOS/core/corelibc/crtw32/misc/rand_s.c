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

#ifdef _WIN32_WCE

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
    PBCRYPTGENRANDOM pfnGenRandom = (PBCRYPTGENRANDOM)DecodePointer ((PVOID)g_pfnBCryptGenRandom);

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
        encoded = (PBCRYPTGENRANDOM) EncodePointer((PVOID)pfnGenRandom);
        enull = EncodePointer(NULL);
        if (InterlockedCompareExchangePointer(
                (void**)&g_pfnBCryptGenRandom,
                (void*)encoded,
                enull)
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
        _set_errno(ENOSYS);
        return ENOSYS;
    }

    return 0;
}

#else // Keep VS11 original source

#include <windows.h>
#include <cruntime.h>
#include <mtdll.h>
#include <stddef.h>
#include <stdlib.h>
#include <internal.h>
#include <ntsecapi.h>

/***
*errno_t rand_s(unsigned int *_RandomValue) - returns a random number
*
*Purpose:
*	returns a random number.
*
*Entry:
*	Non NULL out parameter.
*
*Exit:
*   errno_t - 0 if sucessful
*             error value on failure
*
*	Out parameter -
*             set to random value on success
*             set to 0 on error
*
*Exceptions:
*   Works only in Win2k and above. Will call invalid parameter if RtlGenRandom is not
*   available.
*
*******************************************************************************/

#define __TO_STR(x) #x
#define _TO_STR(x)  __TO_STR(x)



typedef BOOL (APIENTRY *PGENRANDOM)( PVOID, ULONG );

static PGENRANDOM g_pfnRtlGenRandom;

void __cdecl _initp_misc_rand_s(void* enull)
{
    g_pfnRtlGenRandom = (PGENRANDOM) enull;
}

errno_t __cdecl rand_s 
(
    unsigned int *_RandomValue
)
{
    PGENRANDOM pfnRtlGenRandom = DecodePointer(g_pfnRtlGenRandom);
    _VALIDATE_RETURN_ERRCODE( _RandomValue != NULL, EINVAL );
    *_RandomValue = 0; // Review : better value to initialize it to?
    
    if ( pfnRtlGenRandom == NULL )
    {
        PGENRANDOM encoded;
        void* enull;        
#ifdef _CORESYS
#define RAND_DLL L"cryptbase.dll"
#else
#define RAND_DLL L"ADVAPI32.DLL"
#endif
        // advapi32.dll/cryptbase.dll is unloaded when the App exits.
        HMODULE hRandDll = LoadLibraryExW(RAND_DLL, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
#ifndef _CORESYS
        if (!hRandDll && GetLastError() == ERROR_INVALID_PARAMETER)
        {
            // LOAD_LIBRARY_SEARCH_SYSTEM32 is not supported on this platfrom,
            // try one more time using default options
            hRandDll = LoadLibraryExW(RAND_DLL, NULL, 0);
        }
#endif        
        if (!hRandDll)
        {
            _VALIDATE_RETURN_ERRCODE(("rand_s is not available on this platform", 0), EINVAL);
        }

        pfnRtlGenRandom = ( PGENRANDOM ) GetProcAddress( hRandDll, _TO_STR( RtlGenRandom ) );
        if ( pfnRtlGenRandom == NULL )
        {
            _VALIDATE_RETURN_ERRCODE(("rand_s is not available on this platform", 0), _get_errno_from_oserr(GetLastError()));
        }
        encoded = (PGENRANDOM) EncodePointer(pfnRtlGenRandom);
        enull = EncodePointer(NULL);
#ifdef _M_IX86
        if ( (void*)(LONG_PTR)InterlockedExchange( 
                ( LONG* )&g_pfnRtlGenRandom, 
                ( LONG )( LONG_PTR )encoded) 
            != enull )
#else
        if ( InterlockedExchangePointer( 
                ( void** )&g_pfnRtlGenRandom, 
                ( void* )encoded) 
            != enull )
#endif
        {
            /* A different thread has already loaded advapi32.dll/cryptbase.dll. */
            FreeLibrary( hRandDll );
        }
    }

    if ( !(*pfnRtlGenRandom)( _RandomValue, ( ULONG )sizeof( unsigned int ) ) )
    {
        errno = ENOMEM;
        return errno;
    }
    return 0;
}
#endif // _WIN32_WCE