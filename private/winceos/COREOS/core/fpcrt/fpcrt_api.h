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

#ifndef _ARM_
#error FPCRT is available only on ARM platforms
#endif

// architecture specific APIs
//
FP_CRT_VFP  (__addd)
FP_CRT_VFP  (__adds)
FP_CRT_VFP  (__divd)
FP_CRT_VFP  (__divs)
FP_CRT_VFP  (__dtoi)
FP_CRT      (__dtoi64)
FP_CRT_VFP  (__dtos)
FP_CRT_VFP  (__dtou)
FP_CRT      (__dtou64)
FP_CRT_VFP  (__eqd)
FP_CRT_VFP  (__eqs)
FP_CRT_VFP  (__ged)
FP_CRT_VFP  (__ges)
FP_CRT_VFP  (__gtd)
FP_CRT_VFP  (__gts)
FP_CRT      (__i64tod)
FP_CRT      (__i64tos)
FP_CRT_VFP  (__itod)
FP_CRT_VFP  (__itos)
FP_CRT_VFP  (__led)
FP_CRT_VFP  (__les)
FP_CRT_VFP  (__ltd)
FP_CRT_VFP  (__lts)
FP_CRT_VFP  (__muld)
FP_CRT_VFP  (__muls)
FP_CRT_VFP  (__ned)
FP_CRT_VFP  (__negd)
FP_CRT_VFP  (__negs)
FP_CRT_VFP  (__nes)
FP_CRT_VFP  (__stod)
FP_CRT_VFP  (__stoi)
FP_CRT      (__stoi64)
FP_CRT_VFP  (__stou)
FP_CRT      (__stou64)
FP_CRT_VFP  (__subd)
FP_CRT_VFP  (__subs)
FP_CRT      (__u64tod)
FP_CRT      (__u64tos)
FP_CRT_VFP  (__utod)
FP_CRT_VFP  (__utos)

// architecture independent APIs
//
FP_CRT      (_cabs)
FP_CRT      (_chgsign)
FP_CRT      (_clearfp)
FP_CRT      (_controlfp)
FP_CRT      (_controlfp_s)
FP_CRT      (_copysign)
FP_CRT      (_finite)
FP_CRT      (_fpclass)
FP_CRT      (_fpieee_flt)
FP_CRT      (_fpreset)
FP_CRT      (_frnd)
FP_CRT      (_fsqrt)
FP_CRT      (_hypot)
FP_CRT      (_isnan)
FP_CRT      (_isnanf)
FP_CRT      (_isunordered)
FP_CRT      (_isunorderedf)
FP_CRT      (_j0)
FP_CRT      (_j1)
FP_CRT      (_jn)
FP_CRT      (_logb)
FP_CRT      (_nextafter)
FP_CRT      (_scalb)
FP_CRT      (_statusfp)
FP_CRT      (_y0)
FP_CRT      (_y1)
FP_CRT      (_yn)
FP_CRT_VFP  (acos)
FP_CRT_VFP  (asin)
FP_CRT_VFP  (atan)
FP_CRT_VFP  (atan2)
FP_CRT_VFP  (ceil)
FP_CRT_VFP  (ceilf)
FP_CRT_VFP  (cos)
FP_CRT_VFP  (cosh)
FP_CRT_VFP  (exp)
FP_CRT_VFP  (fabs)
FP_CRT_VFP  (fabsf)
FP_CRT_VFP  (floor)
FP_CRT_VFP  (floorf)
FP_CRT_VFP  (fmod)
FP_CRT_VFP  (fmodf)
FP_CRT_VFP  (frexp)
FP_CRT_VFP  (ldexp)
FP_CRT_VFP  (log)
FP_CRT_VFP  (log10)
FP_CRT_VFP  (modf)
FP_CRT_VFP  (pow)
FP_CRT_VFP  (sin)
FP_CRT_VFP  (sinh)
FP_CRT_VFP  (sqrt)
FP_CRT_VFP  (sqrtf)
FP_CRT_VFP  (tan)
FP_CRT_VFP  (tanh)


