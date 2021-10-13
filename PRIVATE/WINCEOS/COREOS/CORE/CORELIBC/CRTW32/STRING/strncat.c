//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <corecrt.h>

char * __cdecl strncat(char * front, const char * back, size_t count) {
	char *start = front;
	while (*front++)
		;
	front--;
	while (count--)
		if (!(*front++ = *back++))
			return(start);
	*front = '\0';
	return(start);
}

