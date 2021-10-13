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
#include <corecrt.h>

#define _TOLOWER(c) (((c) >= 'A') && ((c) <= 'Z') ? ((c) - 'A' + 'a') : (c))

int __cdecl _memicmp(const void *first, const void *last, unsigned int count) {
    int f = 0;
    int l = 0;
    while (count--) {
        if ((*(unsigned char *)first == *(unsigned char *)last) ||
            ((f = _TOLOWER(*(unsigned char *)first)) == (l = _TOLOWER(*(unsigned char *)last)))) {
            first = (char *)first + 1;
            last = (char *)last + 1;
        } else
            break;
   }
   return ( f - l );
}
