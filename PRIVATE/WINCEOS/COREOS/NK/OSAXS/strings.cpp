//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

#define ToUpper(c) (('a' <= (c) && (c) <= 'z') ? (c) - ('a' - 'A') : (c))
/*----------------------------------------------------------------------------
    kdbgwcsnicmp

----------------------------------------------------------------- ------- --*/
int kdbgwcsnicmp(const WCHAR *wz1, const WCHAR *wz2, size_t i)
{
    if (i == 0)
        return 0;
    
    if (wz1 == NULL || wz2 == NULL)
        return (wz1 != wz2);

    while (*wz1 != '\0' && i)
        {
        if (*wz1 != *wz2)
            {
            if (ToUpper((int) *wz1) != ToUpper((int) *wz2))
                return (int) *wz1 - *wz2;
            }
        wz1++;
        wz2++;
        i--;
        }
    if (i == 0 || *wz2 == '\0')
        return 0;

    return 1;
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

