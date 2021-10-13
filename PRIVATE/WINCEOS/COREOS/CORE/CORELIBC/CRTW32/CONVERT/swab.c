//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <corecrt.h>

void __cdecl _swab(char *src, char *dest, int nbytes) {
	char b1, b2;
	while (nbytes > 1) {
		b1 = *src++;
		b2 = *src++;
		*dest++ = b2;
		*dest++ = b1;
		nbytes -= 2;
	}
}

