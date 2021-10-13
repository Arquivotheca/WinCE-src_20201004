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
#include <corecrt.h>

extern _PVFV __xi_a[], __xi_z[];    /* C initializers */
extern _PVFV __xc_a[], __xc_z[];    /* C++ initializers */

void
_initterm(
    _PVFV * pfbegin,
    _PVFV * pfend
    )
{
    while (pfbegin < pfend)
    {
        if (*pfbegin)
        {
            (**pfbegin)();
        }
        ++pfbegin;
    }
}

void
_cinit(void)
{
    _initterm(__xi_a, __xi_z);
    _initterm(__xc_a, __xc_z);
}

