//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//------------------------------------------------------------------------------
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

    while ( !(ret = *pwc1 - *pwc2) && *pwc2)
        ++pwc1, ++pwc2;
    return ret;
}



