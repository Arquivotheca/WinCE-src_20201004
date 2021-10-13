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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
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
#include <cruntime.h>
#include <stddef.h>
#include <stdlib.h>
#include <internal.h>
#include <coredll.h>

/***
*errno_t rand_s(unsigned int *_RandomValue) - returns a random number
*
*Purpose:
*   returns a random number.
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
*   Available only if CeGenRandom is available.
*
*******************************************************************************/

errno_t __cdecl rand_s
(
    unsigned int *_RandomValue
)
{
    _VALIDATE_RETURN_ERRCODE(_RandomValue != NULL, EINVAL);
    *_RandomValue = 0; // Review : better value to initialize it to?

    if (!CeGenRandom((DWORD)sizeof(*_RandomValue), (PBYTE)_RandomValue))
    {
        _SET_ERRNO(EINVAL);
        return EINVAL;
    }

    return 0;
}

