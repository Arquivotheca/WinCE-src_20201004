//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
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

