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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//

#ifndef _ARM_
#error FPCRT is available only on ARM platforms
#endif

// architecture specific APIs
//
FP_CRT(__addd)
FP_CRT(__adds)
FP_CRT(__divd)
FP_CRT(__divs)
FP_CRT(__dtoi)
FP_CRT(__dtoi64)
FP_CRT(__dtos)
FP_CRT(__dtou)
FP_CRT(__dtou64)
FP_CRT(__eqd)
FP_CRT(__eqs)
FP_CRT(__ged)
FP_CRT(__ges)
FP_CRT(__gtd)
FP_CRT(__gts)
FP_CRT(__i64tod)
FP_CRT(__i64tos)
FP_CRT(__itod)
FP_CRT(__itos)
FP_CRT(__led)
FP_CRT(__les)
FP_CRT(__ltd)
FP_CRT(__lts)
FP_CRT(__muld)
FP_CRT(__muls)
FP_CRT(__ned)
FP_CRT(__negd)
FP_CRT(__negs)
FP_CRT(__nes)
FP_CRT(__stod)
FP_CRT(__stoi)
FP_CRT(__stoi64)
FP_CRT(__stou)
FP_CRT(__stou64)
FP_CRT(__subd)
FP_CRT(__subs)
FP_CRT(__u64tod)
FP_CRT(__u64tos)
FP_CRT(__utod)
FP_CRT(__utos)

// architecture independent APIs
//
FP_CRT(_cabs)
FP_CRT(_chgsign)
FP_CRT(_clearfp)
FP_CRT(_controlfp)
FP_CRT(_controlfp_s)
FP_CRT(_copysign)
FP_CRT(_finite)
FP_CRT(_fpclass)
FP_CRT(_fpieee_flt)
FP_CRT(_fpreset)
FP_CRT(_frnd)
FP_CRT(_fsqrt)
FP_CRT(_hypot)
FP_CRT(_isnan)
FP_CRT(_isnanf)
FP_CRT(_isunordered)
FP_CRT(_isunorderedf)
FP_CRT(_j0)
FP_CRT(_j1)
FP_CRT(_jn)
FP_CRT(_logb)
FP_CRT(_nextafter)
FP_CRT(_scalb)
FP_CRT(_statusfp)
FP_CRT(_y0)
FP_CRT(_y1)
FP_CRT(_yn)
FP_CRT(acos)
FP_CRT(asin)
FP_CRT(atan)
FP_CRT(atan2)
FP_CRT(ceil)
FP_CRT(ceilf)
FP_CRT(cos)
FP_CRT(cosh)
FP_CRT(exp)
FP_CRT(fabs)
FP_CRT(fabsf)
FP_CRT(floor)
FP_CRT(floorf)
FP_CRT(fmod)
FP_CRT(fmodf)
FP_CRT(frexp)
FP_CRT(ldexp)
FP_CRT(log)
FP_CRT(log10)
FP_CRT(modf)
FP_CRT(pow)
FP_CRT(sin)
FP_CRT(sinh)
FP_CRT(sqrt)
FP_CRT(sqrtf)
FP_CRT(tan)
FP_CRT(tanh)


