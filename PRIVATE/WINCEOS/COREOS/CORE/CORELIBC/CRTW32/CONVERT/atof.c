//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <corecrt.h>

double __cdecl atof(const char *nptr) {
    const char *EndPtr;
    _LDBL12 ld12;
    double x;
	while (isspace((int)(unsigned char)*nptr))
		nptr++;
    if (__strgtold12(&ld12, &EndPtr, nptr) & SLD_NODIGITS)
    	return 0.0;
	_ld12tod(&ld12, &x);
    return x;
}

