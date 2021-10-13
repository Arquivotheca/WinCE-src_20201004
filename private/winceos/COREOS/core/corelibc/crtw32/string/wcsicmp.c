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

#include <stdlib.h>

int
wcsicmp(
    const wchar_t* dst,
    const wchar_t* src
    )
{
    wchar_t f;
    wchar_t l;

    do
    {
        f = *dst++;
        l = *src++;

        if (f != l)
        {
            f = towlower(f);
            l = towlower(l);
        }
    }
    while (f && (f == l));

    return(int)(f - l);
}

