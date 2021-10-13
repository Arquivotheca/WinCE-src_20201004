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

#if defined(_M_IX86)
#error This file should not be compiled for x86 since an optimized version exists
#endif // _M_IX86

char * _strrev (char * string) {
    char *start = string;
    char *left = string;
    char ch;
    while (*string++)         /* find end of string */
        ;
    string -= 2;
    while (left < string) {
        ch = *left;
        *left++ = *string;
        *string-- = ch;
    }
    return(start);
}

