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

char * __cdecl strstr(const char * str1, const char * str2) {
    char *cp = (char *) str1;
    char *s1, *s2;
    if (!*str2)
        return((char *)str1);
    while (*cp) {
        s1 = cp;
        s2 = (char *) str2;
        while ( *s1 && *s2 && !(*s1-*s2) )
            s1++, s2++;
        if (!*s2)
            return(cp);
        cp++;
    }
    return(NULL);
}

