//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
