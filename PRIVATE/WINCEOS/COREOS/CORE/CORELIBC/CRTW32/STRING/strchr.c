//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <corecrt.h>

char * __cdecl strchr (const char * string, int ch) {
	while (*string && *string != (char)ch)
		string++;
	if (*string == (char)ch)
		return((char *)string);
	return(NULL);
}

