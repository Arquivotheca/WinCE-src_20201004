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
#include <kernel.h>

// _stricmp function is not defined in ARM CRT, add it here
int _stricmp (const char * str1, const char * str2)
{
    int ret = 0;

    while (0 == (ret = _tolower (*str1) - _tolower(*str2)) && *str2) {
        ++str1;
        ++str2;
    }
    return ret;
    
}


