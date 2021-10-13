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
//------------------------------------------------------------------------------
//
//
//------------------------------------------------------------------------------

typedef unsigned short wchar_t;



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
unsigned int 
strlenW(
    const wchar_t *wcs
    )
{
    const wchar_t *eos = wcs;

    while (*eos)
        ++eos;

    return eos-wcs;
}




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
strcmpW(
    const wchar_t *pwc1,
    const wchar_t *pwc2
    )
{
    int ret = 0;

    while ( (0 == (ret = *pwc1 - *pwc2)) && *pwc2)
        ++pwc1, ++pwc2;
    return ret;
}



