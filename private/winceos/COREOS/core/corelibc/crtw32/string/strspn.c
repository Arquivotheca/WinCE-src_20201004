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
#include <string.h>

#define _STRSPN     1
#define _STRCSPN    2
#define _STRPBRK    3

#if defined(SSTRCSPN)
#define ROUTINE _STRCSPN
#elif defined(SSTRPBRK)
#define ROUTINE _STRPBRK
#else
#define ROUTINE _STRSPN
#endif

#if !defined(_M_IX86) || (ROUTINE != _STRSPN)

/* Routine prototype */
#if ROUTINE == _STRSPN
size_t __cdecl strspn (
#elif ROUTINE == _STRCSPN
size_t __cdecl strcspn (
#else /* ROUTINE == STRPBRK */
char * __cdecl strpbrk (
#endif
    const char * string,
    const char * control
    ) {
    const unsigned char *str = (const unsigned char *)string;
    const unsigned char *ctrl = (const unsigned char *)control;

    unsigned char map[32];
    int count;

    /* Clear out bit map */
    for (count=0; count<32; count++)
        map[count] = 0;

    /* Set bits in control map */
    while (*ctrl) {
        map[*ctrl >> 3] |= (1 << (*ctrl & 7));
        ctrl++;
    }

#if ROUTINE == _STRSPN
    /* 1st char NOT in control map stops search */
    if (*str) {
        count=0;
        while (map[*str >> 3] & (1 << (*str & 7))) {
            count++;
            str++;
        }
        return(count);
    }
    return(0);
#elif ROUTINE == _STRCSPN
    /* 1st char in control map stops search */
    count=0;
    map[0] |= 1;    /* null chars not considered */
    while (!(map[*str >> 3] & (1 << (*str & 7)))) {
        count++;
        str++;
    }
    return(count);
#else /* (ROUTINE == _STRPBRK) */
    /* 1st char in control map stops search */
    while (*str) {
        if (map[*str >> 3] & (1 << (*str & 7)))
            return((char *)str);
        str++;
    }
    return(NULL);
#endif
}

#endif // !defined(_M_IX86) || ROUTINE != _STRSPN

