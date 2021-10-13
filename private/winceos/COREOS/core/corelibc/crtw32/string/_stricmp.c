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

int _stricmp(const char *str1, const char *str2) {
    unsigned char ch1, ch2;
    for (;*str1 && *str2; str1++, str2++) {
        ch1 = (unsigned char)*str1;
        ch2 = (unsigned char)*str2;
        if (ch1 != ch2) {
            ch1 = (unsigned char)tolower(ch1);
            ch2 = (unsigned char)tolower(ch2);
            if (ch1 != ch2)
                return ch1 - ch2;
        }        
    }
    // Check last character.
    return (unsigned char)_tolower(*str1) - (unsigned char)_tolower(*str2);
}

#endif

