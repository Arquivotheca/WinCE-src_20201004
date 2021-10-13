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

#include <windows.h>


LONG
WINAPI
xxx_InterlockedIncrement(
    LONG volatile *lpAddend
    )
{
    __asm {
        mov     eax, 1
        mov     ecx, lpAddend
        xadd    [ecx], eax
        inc     eax
    }
}

LONG
WINAPI
xxx_InterlockedDecrement(
    LONG volatile *lpAddend
    )
{
    __asm {
        mov     eax, -1
        mov     ecx, lpAddend
        xadd    [ecx], eax
        dec     eax
    }
}

LONG
WINAPI
xxx_InterlockedExchange(
    LONG volatile *Target,
    LONG Value
    )
{
    __asm {
        mov     eax, Value
        mov     ecx, Target
        xchg  [ecx], eax
    }
}

LONG
WINAPI
xxx_InterlockedCompareExchange(
    LONG volatile *Target,
    LONG Exchange,
    LONG Comperand
    )
{
    __asm {
        mov     eax, Comperand
        mov     ecx, Target
        mov     edx, Exchange
        cmpxchg [ecx], edx
    }
}

LONG
WINAPI
xxx_InterlockedExchangeAdd(
    LONG volatile *lpAddend,
    LONG  Value
    )
{
    __asm {
        mov     ecx, lpAddend
        mov     eax, Value
        xadd    [ecx], eax
    }
}


