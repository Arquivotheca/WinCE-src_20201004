//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

