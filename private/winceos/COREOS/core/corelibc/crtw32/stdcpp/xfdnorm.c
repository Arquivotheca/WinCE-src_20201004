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
/* _FDnorm function -- IEEE 754 version */
#include "xmath.h"
_C_STD_BEGIN

_CRTIMP2_PURE short __CLRCALL_PURE_OR_CDECL _FDnorm(_Fval *ps)
	{	/* normalize float fraction */
	short xchar;
	unsigned short sign = (unsigned short)(ps->_Sh[_F0] & _FSIGN);

	xchar = 1;
	if ((ps->_Sh[_F0] &= _FFRAC) != 0 || ps->_Sh[_F1])
		{	/* nonzero, scale */
		if (ps->_Sh[_F0] == 0)
			ps->_Sh[_F0] = ps->_Sh[_F1], ps->_Sh[_F1] = 0, xchar -= 16;
		for (; ps->_Sh[_F0] < 1 << _FOFF; --xchar)
			{	/* shift left by 1 */
			ps->_Sh[_F0] = (unsigned short)(ps->_Sh[_F0] << 1 | ps->_Sh[_F1] >> 15);
			ps->_Sh[_F1] <<= 1;
			}
		for (; 1 << (_FOFF + 1) <= ps->_Sh[_F0]; ++xchar)
			{	/* shift right by 1 */
			ps->_Sh[_F1] = (unsigned short)(ps->_Sh[_F1] >> 1 | ps->_Sh[_F0] << 15);
			ps->_Sh[_F0] >>= 1;
			}
		ps->_Sh[_F0] &= _FFRAC;
		}
	ps->_Sh[_F0] |= sign;
	return (xchar);
	}
_C_STD_END

/*
 * Copyright (c) 1992-2007 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
 V5.03:0009 */
