//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <corecrt.h>

#undef isspace
static int isspace(int c) {
	return ( c == ' ' || c == '\n' || c == '\t' || c == '\r' || c == '\f' || c == '\v');
}

long __cdecl atol(const char *nptr) {
	int c;			/* current char */
	long total;		/* current total */
	int sign;		/* if '-', then negative, otherwise positive */
	while (isspace((int)(unsigned char)*nptr))
		nptr++;
	c = (int)(unsigned char)*nptr++;
	sign = c;		/* save sign indication */
	if (c == '-' || c == '+')
		c = (int)(unsigned char)*nptr++;	/* skip sign */
	total = 0;
	while ((c >= '0') && (c <= '9')) {
		total = 10 * total + (c - '0'); 	/* accumulate digit */
		c = (int)(unsigned char)*nptr++;	/* get next char */
	}
	if (sign == '-')
		return -total;
	else
		return total;
}

int __cdecl atoi(const char *nptr) {
	return (int)atol(nptr);
}

