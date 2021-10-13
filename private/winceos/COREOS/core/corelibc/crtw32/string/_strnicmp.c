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

#if !defined(_M_IX86)
int _strnicmp(const char *psz1, const char *psz2, size_t cb) {
    unsigned char ch1 = 0, ch2 = 0; // Zero for case cb = 0.
    while (cb--) {
        ch1 = (unsigned char)*psz1++;
        ch2 = (unsigned char)*psz2++;

        if (ch1 != ch2) {
            ch1 = (unsigned char)tolower(ch1);
            ch2 = (unsigned char)tolower(ch2);
        }
        
        if (!ch1 || (ch1 != ch2))
            break;
    }
    return (ch1 - ch2);
}
#endif // _M_IX86

