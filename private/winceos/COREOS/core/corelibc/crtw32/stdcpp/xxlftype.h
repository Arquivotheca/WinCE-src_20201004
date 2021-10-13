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
/* xxlftype.h -- parameters for long double floating-point type */
#include <yvals.h>
#include <float.h>

#define FTYPE	long double
#define FCTYPE	_Lcomplex
#define FBITS	LDBL_MANT_DIG
#define FEPS	LDBL_EPSILON
#define FMAX	LDBL_MAX
#define FMIN	LDBL_MIN
#define FMAXEXP	LDBL_MAX_EXP
#define FFUN(fun)	fun##l
#define FMACRO(x)	L##x
#define FNAME(fun)	_L##fun
#define FCONST(obj)	_L##obj._Long_double
#define FLIT(lit)	lit##L
#define FISNEG(exp)	LSIGN(exp)
#define FSETLSB(x) (((_Lval *)(char *)&(x))->_Sh[_Lg] |= 1)

 #if _IS_EMBEDDED
#define FCPTYPE	float_complex	/* place holder */

 #else /* _IS_EMBEDDED */
#define FCPTYPE	complex<long double>
 #endif /* _IS_EMBEDDED */

/*
 * Copyright (c) 1992-2012 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
V6.00:0009 */
