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
/* xxdftype.h -- parameters for double floating-point type */
#include <yvals.h>
#include <float.h>

#define FTYPE	double
#define FCTYPE	_Dcomplex
#define FBITS	DBL_MANT_DIG
#define FEPS	DBL_EPSILON
#define FMAX	DBL_MAX
#define FMIN	DBL_MIN
#define FMAXEXP	DBL_MAX_EXP
#define FFUN(fun)	fun
#define FMACRO(x)	x
#define FNAME(fun)	_##fun
#define FCONST(obj)	_##obj._Double
#define FLIT(lit)	lit
#define FISNEG(exp)	DSIGN(exp)
#define FSETLSB(x) (((_Dval *)(char *)&(x))->_Sh[_Dg] |= 1)

 #if _IS_EMBEDDED
#define FCPTYPE	double_complex

 #else /* _IS_EMBEDDED */
#define FCPTYPE	complex<double>
 #endif /* _IS_EMBEDDED */

/*
 * Copyright (c) 1992-2012 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
V6.00:0009 */
