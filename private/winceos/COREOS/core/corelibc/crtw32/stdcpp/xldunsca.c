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
/* _LDunscale function -- IEEE 754 version */
#include "xmath.h"
_C_STD_BEGIN
 #if !defined(MRTDLL)
_C_LIB_DECL
 #endif /* defined(MRTDLL) */
 #if _DLONG == 0
_CRTIMP2_PURE short (__CLRCALL_PURE_OR_CDECL _LDunscale)(short *pex, long double *px)
	{	/* separate *px to 1/2 <= |frac| < 1 and 2^*pex -- 64-bit */
	return (_Dunscale(pex, (double *)px));
	}

 #elif _DLONG == 1
_CRTIMP2_PURE short (__CLRCALL_PURE_OR_CDECL _LDunscale)(short *pex, long double *px)
	{	/* separate *px to 1/2 <= |frac| < 1 and 2^*pex -- 80-bit */
	_Lval *ps = (_Lval *)(char *)px;
	short xchar = ps->_Sh[_L0] & _LMASK;

	if (xchar == _LMAX)
		{	/* NaN or INF */
		*pex = 0;
		return ((ps->_Sh[_L1] & 0x7fff) != 0 || ps->_Sh[_L2] != 0
			|| ps->_Sh[_L3] != 0 || ps->_Sh[_L4] != 0 ? _NANCODE : _INFCODE);
		}
	else if (ps->_Sh[_L1] != 0 || ps->_Sh[_L2] != 0
		|| ps->_Sh[_L3] != 0 || ps->_Sh[_L4] != 0)
		{	/* finite, reduce to [1/2, 1) */
		if (xchar == 0)
			xchar = 1;	/* correct denormalized exponent */
		xchar += _LDnorm(ps);
		ps->_Sh[_L0] = ps->_Sh[_L0] & _LSIGN | _LBIAS;
		*pex = xchar - _LBIAS;
		return (_FINITE);
		}
	else
		{	/* zero */
		*pex = 0;
		return (0);
		}
	}

 #else	/* 1 < _DLONG */
_CRTIMP2_PURE short (__CLRCALL_PURE_OR_CDECL _LDunscale)(short *pex, long double *px)
	{	/* separate *px to 1/2 <= |frac| < 1 and 2^*pex -- 128-bit SPARC */
	_Lval *ps = (_Lval *)(char *)px;
	short xchar = ps->_Sh[_L0] & _LMASK;

	if (xchar == _LMAX)
		{	/* NaN or INF */
		*pex = 0;
		return (ps->_Sh[_L1] != 0 || ps->_Sh[_L2] != 0 || ps->_Sh[_L3] != 0
			|| ps->_Sh[_L4] != 0 || ps->_Sh[_L5] != 0 || ps->_Sh[_L6] != 0
			|| ps->_Sh[_L7] != 0 ? _NANCODE : _INFCODE);
		}
	else if (0 < xchar || (xchar = _LDnorm(ps)) <= 0)
		{	/* finite, reduce to [1/2, 1) */
		ps->_Sh[_L0] = ps->_Sh[_L0] & _LSIGN | _LBIAS;
		*pex = xchar - _LBIAS;
		return (_FINITE);
		}
	else
		{	/* zero */
		*pex = 0;
		return (0);
		}
	}
 #endif /* _DLONG */
 #if !defined(MRTDLL)
_END_C_LIB_DECL
 #endif /* !defined(MRTDLL) */
_C_STD_END

/*
 * Copyright (c) 1992-2012 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
V6.00:0009 */
