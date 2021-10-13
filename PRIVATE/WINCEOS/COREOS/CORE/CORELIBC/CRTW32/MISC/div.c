//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <corecrt.h>

div_t div(int numer, int denom) {
	div_t result;
	result.quot = numer / denom;
	result.rem = numer % denom;
	if (numer < 0 && result.rem > 0) {
		/* did division wrong; must fix up */
		++result.quot;
		result.rem -= denom;
	}
	return result;
}

