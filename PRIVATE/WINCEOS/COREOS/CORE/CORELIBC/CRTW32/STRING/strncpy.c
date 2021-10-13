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
#pragma function(strncpy)

char * __cdecl strncpy(char * dest, const char * source, size_t count) {
	char *start = dest;
	while (count && (*dest++ = *source++))	  /* copy string */
		count--;
	if (count)				/* pad out with zeroes */
		while (--count)
			*dest++ = '\0';
	return(start);
}

