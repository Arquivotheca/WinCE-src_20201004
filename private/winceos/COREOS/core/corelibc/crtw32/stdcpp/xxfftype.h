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
/* xxfftype.h -- parameters for float floating-point type */
#include <yvals.h>
#include <float.h>

#define FTYPE	float
#define FCTYPE	_Fcomplex
#define FBITS	FLT_MANT_DIG
#define FEPS	FLT_EPSILON
#define FMAX	FLT_MAX
#define FMIN	FLT_MIN
#define FMAXEXP	FLT_MAX_EXP
#define FFUN(fun)	fun##f
#define FMACRO(x)	F##x
#define FNAME(fun)	_F##fun
#define FCONST(obj)	_F##obj._Float
#define FLIT(lit)	lit##F
#define FISNEG(exp)	FSIGN(exp)
#define FSETLSB(x) (((_Fval *)(char *)&(x))->_Sh[_Fg] |= 1)

 #if _IS_EMBEDDED
#define FCPTYPE	float_complex

 #else /* _IS_EMBEDDED */
#define FCPTYPE	complex<float>
 #endif /* _IS_EMBEDDED */

/*
 * Copyright (c) 1992-2012 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
V6.00:0009 */
