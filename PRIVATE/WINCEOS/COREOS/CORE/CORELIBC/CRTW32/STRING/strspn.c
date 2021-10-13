//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
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
    const unsigned char *str = string;
    const unsigned char *ctrl = control;

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

