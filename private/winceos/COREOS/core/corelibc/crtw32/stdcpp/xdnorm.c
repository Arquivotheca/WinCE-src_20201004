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
/* _Dnorm function -- IEEE 754 version */
#include "xmath.h"
_C_STD_BEGIN

_CRTIMP2_PURE short __CLRCALL_PURE_OR_CDECL _Dnorm(_Dval *ps)
	{	/* normalize double fraction */
	short xchar;
	unsigned short sign = (unsigned short)(ps->_Sh[_D0] & _DSIGN);

	xchar = 1;
	if ((ps->_Sh[_D0] &= _DFRAC) != 0 || ps->_Sh[_D1]
		|| ps->_Sh[_D2] || ps->_Sh[_D3])
		{	/* nonzero, scale */
		for (; ps->_Sh[_D0] == 0; xchar -= 16)
			{	/* shift left by 16 */
			ps->_Sh[_D0] = ps->_Sh[_D1], ps->_Sh[_D1] = ps->_Sh[_D2];
			ps->_Sh[_D2] = ps->_Sh[_D3], ps->_Sh[_D3] = 0;
			}
		for (; ps->_Sh[_D0] < 1 << _DOFF; --xchar)
			{	/* shift left by 1 */
			ps->_Sh[_D0] = (unsigned short)(ps->_Sh[_D0] << 1 | ps->_Sh[_D1] >> 15);
			ps->_Sh[_D1] = (unsigned short)(ps->_Sh[_D1] << 1 | ps->_Sh[_D2] >> 15);
			ps->_Sh[_D2] = (unsigned short)(ps->_Sh[_D2] << 1 | ps->_Sh[_D3] >> 15);
			ps->_Sh[_D3] <<= 1;
			}
		for (; 1 << (_DOFF + 1) <= ps->_Sh[_D0]; ++xchar)
			{	/* shift right by 1 */
			ps->_Sh[_D3] = (unsigned short)(ps->_Sh[_D3] >> 1 | ps->_Sh[_D2] << 15);
			ps->_Sh[_D2] = (unsigned short)(ps->_Sh[_D2] >> 1 | ps->_Sh[_D1] << 15);
			ps->_Sh[_D1] = (unsigned short)(ps->_Sh[_D1] >> 1 | ps->_Sh[_D0] << 15);
			ps->_Sh[_D0] >>= 1;
			}
		ps->_Sh[_D0] &= _DFRAC;
		}
	ps->_Sh[_D0] |= sign;
	return (xchar);
	}
_C_STD_END

/*
 * Copyright (c) 1992-2007 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
 V5.03:0009 */
