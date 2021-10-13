//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <corecrt.h>

#pragma warning(disable:4163)
#pragma function(strncmp)

int __cdecl strncmp(const char * first, const char * last, size_t count) {
	if (!count)
		return(0);
	while (--count && *first && *first == *last) {
		first++;
		last++;
	}
	return(*(unsigned char *)first - *(unsigned char *)last);
}

