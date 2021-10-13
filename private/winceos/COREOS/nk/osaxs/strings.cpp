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
/*++

Module Name:

    strings.cpp

Abstract:

    Utility functions.

--*/

#include "osaxs_p.h"

void WideToAnsi (const WCHAR *wcs, char *sz, DWORD cch)
{
#ifdef TARGET_BUILD
    DWORD i = 0;

    cch--;
    while (i < cch && wcs[i])
    {
        sz[i] = (char) wcs[i];
        i++;
    }

    sz[i] = 0;
#endif
}

/*----------------------------------------------------------------------------
    kdbgwcslen

----------------------------------------------------------------- ------- --*/
size_t kdbgwcslen(const wchar_t * pWstr)
{
    const wchar_t *eos = pWstr;
    while( *eos++ )
        ;
    return( (size_t)(eos - pWstr - 1) );
}


