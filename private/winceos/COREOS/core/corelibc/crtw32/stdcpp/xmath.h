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
/* xmath.h internal header */
#ifndef _XMATH
#define _XMATH
#include <ymath.h>
#include <errno.h>
#include <math.h>
#include <stddef.h>

 #pragma warning(disable: 4244)

		/* FLOAT PROPERTIES */
#ifndef _D0
 #define _D0	3	/* little-endian, small long doubles */
 #define _D1	2
 #define _D2	1
 #define _D3	0

 #define _DBIAS	0x3fe
 #define _DOFF	4

 #define _FBIAS	0x7e
 #define _FOFF	7
 #define _FRND	1

 #define _DLONG	0
 #define _LBIAS	0x3fe
 #define _LOFF	4

#elif _D0 == 0		/* other params defined in <yvals.h> */
 #define _D1	1	/* big-endian */
 #define _D2	2
 #define _D3	3

#else /* _D0 */
 #define _D1	2	/* little-endian */
 #define _D2	1
 #define _D3	0
#endif /* _D0 */

		/* IEEE 754 double properties */
#define _DFRAC	((unsigned short)((1 << _DOFF) - 1))
#define _DMASK	((unsigned short)(0x7fff & ~_DFRAC))
#define _DMAX	((unsigned short)((1 << (15 - _DOFF)) - 1))
#define _DSIGN	((unsigned short)0x8000)
#define DSIGN(x)	(((_Dval *)(char *)&(x))->_Sh[_D0] & _DSIGN)
#define HUGE_EXP	(int)(_DMAX * 900L / 1000)
#define HUGE_RAD	2.73e9	/* ~ 2^33 / pi */
#define SAFE_EXP	((short)(_DMAX >> 1))

		/* IEEE 754 float properties */
#define _FFRAC	((unsigned short)((1 << _FOFF) - 1))
#define _FMASK	((unsigned short)(0x7fff & ~_FFRAC))
#define _FMAX	((unsigned short)((1 << (15 - _FOFF)) - 1))
#define _FSIGN	((unsigned short)0x8000)
#define FSIGN(x)	(((_Fval *)(char *)&(x))->_Sh[_F0] & _FSIGN)
#define FHUGE_EXP	(int)(_FMAX * 900L / 1000)
#define FHUGE_RAD	40.7	/* ~ 2^7 / pi */
#define FSAFE_EXP	((short)(_FMAX >> 1))

 #if _D0 == 0
  #define _F0	0	/* big-endian */
  #define _F1	1

 #else /* _D0 == 0 */
  #define _F0	1	/* little-endian */
  #define _F1	0
 #endif /* _D0 == 0 */

		/* IEEE 754 long double properties */
#define _LFRAC	((unsigned short)(-1))
#define _LMASK	((unsigned short)0x7fff)
#define _LMAX	((unsigned short)0x7fff)
#define _LSIGN	((unsigned short)0x8000)
#define LSIGN(x)	(((_Lval *)(char *)&(x))->_Sh[_L0] & _LSIGN)
#define LHUGE_EXP	(int)(_LMAX * 900L / 1000)
#define LHUGE_RAD	2.73e9	/* ~ 2^33 / pi */
#define LSAFE_EXP	((short)(_LMAX >> 1))

 #if _D0 == 0
  #define _L0	0	/* big-endian */
  #define _L1	1
  #define _L2	2
  #define _L3	3
  #define _L4	4
  #define _L5	5	/* 128-bit only */
  #define _L6	6
  #define _L7	7
  #define _Lg	_L7

 #elif _DLONG == 0
  #define _L0	3	/* little-endian, 64-bit long doubles */
  #define _L1	2
  #define _L2	1
  #define _L3	0
  #define _Lg	_L3
  #define _L4	xxx	/* should never be used */
  #define _L5	xxx
  #define _L6	xxx
  #define _L7	xxx

 #elif _DLONG == 1
  #define _L0	4	/* little-endian, 80-bit long doubles */
  #define _L1	3
  #define _L2	2
  #define _L3	1
  #define _L4	0
  #define _Lg	_L4
  #define _L5	xxx	/* should never be used */
  #define _L6	xxx
  #define _L7	xxx

 #else /* _DLONG */
  #define _L0	7	/* little-endian, 128-bit long doubles */
  #define _L1	6
  #define _L2	5
  #define _L3	4
  #define _L4	3
  #define _L5	2
  #define _L6	1
  #define _L7	0
  #define _Lg	_L7
 #endif /* _DLONG */

 #define _Fg	_F1	/* least-significant 16-bit word */
 #define _Dg	_D3

		/* return values for _Stopfx/_Stoflt */
#define FL_ERR	0
#define FL_DEC	1
#define FL_HEX	2
#define FL_INF	3
#define FL_NAN	4
#define FL_NEG	8

_C_STD_BEGIN
 #if !defined(MRTDLL) && !defined(_M_CEE_PURE)
_C_LIB_DECL
 #endif /* defined(MRTDLL) etc. */

_CRTIMP2_PURE int __CLRCALL_PURE_OR_CDECL _Stopfx(const char **, char **);
_CRTIMP2_PURE int __CLRCALL_PURE_OR_CDECL _Stoflt(const char *, const char *, char **,
	long[], int);
_CRTIMP2_PURE int __CLRCALL_PURE_OR_CDECL _Stoxflt(const char *, const char *, char **,
	long[], int);
_CRTIMP2_PURE int __CLRCALL_PURE_OR_CDECL _WStopfx(const wchar_t **, wchar_t **);
_CRTIMP2_PURE int __CLRCALL_PURE_OR_CDECL _WStoflt(const wchar_t *, const wchar_t *, wchar_t **,
	long[], int);
_CRTIMP2_PURE int __CLRCALL_PURE_OR_CDECL _WStoxflt(const wchar_t *, const wchar_t *, wchar_t **,
	long[], int);

		/* double declarations */
typedef union
	{	/* pun floating type as integer array */
	unsigned short _Sh[8];
	double _Val;
	} _Dval;

_CRTIMP2_PURE double __CLRCALL_PURE_OR_CDECL _Atan(double, int);
_CRTIMP2_PURE short __CLRCALL_PURE_OR_CDECL _Dint(double *, short);
_CRTIMP2_PURE short __CLRCALL_PURE_OR_CDECL _Dnorm(_Dval *);
_CRTIMP2_PURE short __CLRCALL_PURE_OR_CDECL _Dscale(double *, long);
_CRTIMP2_PURE short __CLRCALL_PURE_OR_CDECL _Dunscale(short *, double *);
_CRTIMP2_PURE double __CLRCALL_PURE_OR_CDECL _Hypot(double, double, int *);
_CRTIMP2_PURE double __CLRCALL_PURE_OR_CDECL _Poly(double, const double *, int);
_CRTIMP2_PURE extern /* const */ _Dconst _Eps, _Rteps;
_CRTIMP2_PURE extern /* const */ double _Xbig, _Zero;

_CRTIMP2_PURE double __CLRCALL_PURE_OR_CDECL _Xp_getw(double *, int);
_CRTIMP2_PURE double *__CLRCALL_PURE_OR_CDECL _Xp_setn(double *, int, long);
_CRTIMP2_PURE double *__CLRCALL_PURE_OR_CDECL _Xp_setw(double *, int, double);
_CRTIMP2_PURE double *__CLRCALL_PURE_OR_CDECL _Xp_addh(double *, int, double);
_CRTIMP2_PURE double *__CLRCALL_PURE_OR_CDECL _Xp_mulh(double *, int, double);
_CRTIMP2_PURE double *__CLRCALL_PURE_OR_CDECL _Xp_movx(double *, int, const double *);
_CRTIMP2_PURE double *__CLRCALL_PURE_OR_CDECL _Xp_addx(double *, int, double *, int);
_CRTIMP2_PURE double *__CLRCALL_PURE_OR_CDECL _Xp_subx(double *, int, double *, int);
_CRTIMP2_PURE double *__CLRCALL_PURE_OR_CDECL _Xp_ldexpx(double *, int, int);
_CRTIMP2_PURE double *__CLRCALL_PURE_OR_CDECL _Xp_mulx(double *, int, double *, int, double *);
_CRTIMP2_PURE double *__CLRCALL_PURE_OR_CDECL _Xp_invx(double *, int, double *);
_CRTIMP2_PURE double *__CLRCALL_PURE_OR_CDECL _Xp_sqrtx(double *, int, double *);

		/* float declarations */
typedef union
	{	/* pun floating type as integer array */
	unsigned short _Sh[8];
	float _Val;
	} _Fval;

_CRTIMP2_PURE float __CLRCALL_PURE_OR_CDECL _FAtan(float, int);
_CRTIMP2_PURE short __CLRCALL_PURE_OR_CDECL _FDint(float *, short);
_CRTIMP2_PURE short __CLRCALL_PURE_OR_CDECL _FDnorm(_Fval *);
_CRTIMP2_PURE short __CLRCALL_PURE_OR_CDECL _FDscale(float *, long);
_CRTIMP2_PURE short __CLRCALL_PURE_OR_CDECL _FDunscale(short *, float *);
_CRTIMP2_PURE float __CLRCALL_PURE_OR_CDECL _FHypot(float, float, int *);
_CRTIMP2_PURE float __CLRCALL_PURE_OR_CDECL _FPoly(float, const float *, int);
extern _CRTIMP2_PURE /* const */ _Dconst _FEps, _FRteps;
extern _CRTIMP2_PURE /* const */ float _FXbig, _FZero;

_CRTIMP2_PURE float __CLRCALL_PURE_OR_CDECL _FXp_getw(float *, int);
_CRTIMP2_PURE float *__CLRCALL_PURE_OR_CDECL _FXp_setn(float *, int, long);
_CRTIMP2_PURE float *__CLRCALL_PURE_OR_CDECL _FXp_setw(float *, int, float);
_CRTIMP2_PURE float *__CLRCALL_PURE_OR_CDECL _FXp_addh(float *, int, float);
_CRTIMP2_PURE float *__CLRCALL_PURE_OR_CDECL _FXp_mulh(float *, int, float);
_CRTIMP2_PURE float *__CLRCALL_PURE_OR_CDECL _FXp_movx(float *, int, const float *);
_CRTIMP2_PURE float *__CLRCALL_PURE_OR_CDECL _FXp_addx(float *, int, float *, int);
_CRTIMP2_PURE float *__CLRCALL_PURE_OR_CDECL _FXp_subx(float *, int, float *, int);
_CRTIMP2_PURE float *__CLRCALL_PURE_OR_CDECL _FXp_ldexpx(float *, int, int);
_CRTIMP2_PURE float *__CLRCALL_PURE_OR_CDECL _FXp_mulx(float *, int, float *, int, float *);
_CRTIMP2_PURE float *__CLRCALL_PURE_OR_CDECL _FXp_invx(float *, int, float *);
_CRTIMP2_PURE float *__CLRCALL_PURE_OR_CDECL _FXp_sqrtx(float *, int, float *);

		/* long double declarations */
typedef union
	{	/* pun floating type as integer array */
	unsigned short _Sh[8];
	long double _Val;
	} _Lval;

_CRTIMP2_PURE long double __CLRCALL_PURE_OR_CDECL _LAtan(long double, int);
_CRTIMP2_PURE short __CLRCALL_PURE_OR_CDECL _LDint(long double *, short);
_CRTIMP2_PURE short __CLRCALL_PURE_OR_CDECL _LDnorm(_Lval *);
_CRTIMP2_PURE short __CLRCALL_PURE_OR_CDECL _LDscale(long double *, long);
_CRTIMP2_PURE short __CLRCALL_PURE_OR_CDECL _LDunscale(short *, long double *);
_CRTIMP2_PURE long double __CLRCALL_PURE_OR_CDECL _LHypot(long double, long double, int *);
_CRTIMP2_PURE long double __CLRCALL_PURE_OR_CDECL _LPoly(long double, const long double *, int);
_CRTIMP2_PURE extern /* const */ _Dconst _LEps, _LRteps;
_CRTIMP2_PURE extern /* const */ long double _LXbig, _LZero;

_CRTIMP2_PURE long double __CLRCALL_PURE_OR_CDECL _LXp_getw(long double *, int);
_CRTIMP2_PURE long double *__CLRCALL_PURE_OR_CDECL _LXp_setn(long double *, int, long);
_CRTIMP2_PURE long double *__CLRCALL_PURE_OR_CDECL _LXp_setw(long double *, int, long double);
_CRTIMP2_PURE long double *__CLRCALL_PURE_OR_CDECL _LXp_addh(long double *, int, long double);
_CRTIMP2_PURE long double *__CLRCALL_PURE_OR_CDECL _LXp_mulh(long double *, int, long double);
_CRTIMP2_PURE long double *__CLRCALL_PURE_OR_CDECL _LXp_movx(long double *, int, const long double *);
_CRTIMP2_PURE long double *__CLRCALL_PURE_OR_CDECL _LXp_addx(long double *, int,
	long double *, int);
_CRTIMP2_PURE long double *__CLRCALL_PURE_OR_CDECL _LXp_subx(long double *, int,
	long double *, int);
_CRTIMP2_PURE long double *__CLRCALL_PURE_OR_CDECL _LXp_ldexpx(long double *, int, int);
_CRTIMP2_PURE long double *__CLRCALL_PURE_OR_CDECL _LXp_mulx(long double *, int, long double *,
	int, long double *);
_CRTIMP2_PURE long double *__CLRCALL_PURE_OR_CDECL _LXp_invx(long double *, int, long double *);
_CRTIMP2_PURE long double *__CLRCALL_PURE_OR_CDECL _LXp_sqrtx(long double *, int, long double *);

 #if !defined(MRTDLL) && !defined(_M_CEE_PURE)
_END_C_LIB_DECL
 #endif /* !defined(MRTDLL) etc. */
_C_STD_END
#endif /* _XMATH */

/*
 * Copyright (c) 1992-2012 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
V6.00:0009 */
