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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
/* _Dtest function -- IEEE 754 version */
#include "xmath.h"
_C_STD_BEGIN

_CRTIMP2_PURE short __CLRCALL_PURE_OR_CDECL _Dtest(double *px)
	{	/* categorize *px */
	unsigned short *ps = (unsigned short *)(char *)px;

	if ((ps[_D0] & _DMASK) == _DMAX << _DOFF)
		return ((short)((ps[_D0] & _DFRAC) != 0 || ps[_D1] != 0
			|| ps[_D2] != 0 || ps[_D3] != 0 ? _NANCODE : _INFCODE));
	else if ((ps[_D0] & ~_DSIGN) != 0 || ps[_D1] != 0
		|| ps[_D2] != 0 || ps[_D3] != 0)
		return ((ps[_D0] & _DMASK) == 0 ? _DENORM : _FINITE);
	else
		return (0);
	}
_C_STD_END

/*
 * Copyright (c) 1992-2007 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
 V5.03:0009 */
