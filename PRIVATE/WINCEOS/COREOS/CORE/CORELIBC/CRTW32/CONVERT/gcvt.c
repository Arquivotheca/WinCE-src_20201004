//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include <corecrt.h>

char * __cdecl _gcvt (double value, int ndec, char *buf) {
	int magnitude;
	unsigned char *rc, *str, *stop;
    _LDOUBLE ld;
    FOS autofos;
    __dtold(&ld, &value);
    $I10_OUTPUT(ld,&autofos);
	magnitude = autofos.exp - 1;
	if (magnitude < -1  ||  magnitude > ndec-1)
		rc = str = _cftoe(&value, buf, ndec-1, 0);
	else
		rc = str = _cftof(&value, buf, ndec - autofos.exp);
	while (*str && *str != '.')
		str++;
	if (*str++) {
		while (*str && *str != 'e')
			str++;
		stop = str--;
		while (*str == '0')
			str--;
		while (*++str = *stop++)
			;
	}
	return rc;
}

