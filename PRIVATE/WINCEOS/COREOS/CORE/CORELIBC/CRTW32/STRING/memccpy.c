//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <corecrt.h>

void * __cdecl _memccpy(void * dest, const void * src, int c, unsigned count) {
	while (count && (*((char *)(dest = (char *)dest + 1) - 1) =
		*((char *)(src = (char *)src + 1) - 1)) != (char)c)
		count--;
	return(count ? dest : NULL);
}

