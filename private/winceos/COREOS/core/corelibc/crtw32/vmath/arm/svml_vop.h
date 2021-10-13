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

// Common vector library used by SVML math library.

// This library provides a method for developing a protable vector math
// library. It is currently used to implement the SVML for ARM.

// Additional vector achtechitecures should simply provide their own
// implentation of VectorOp

#pragma once

#include <type_traits>

// Macro langauge to be used with in templated math library functions

// floating point operations
#define ADD(a,b)      VectorOp<float_t>::add(a,b)
#define SUB(a,b)      VectorOp<float_t>::sub(a,b)
#define MUL(a,b)      VectorOp<float_t>::mul(a,b)
#define DIV(a,b)      VectorOp<float_t>::div(a,b)
#define RECIP(a)      VectorOp<float_t>::recip(a)
#define SQRT(a)       VectorOp<float_t>::sqrt(a)
#define NEG(a)        VectorOp<float_t>::neg(a)
#define ABS(a)        VectorOp<float_t>::abs(a)
#define EXP(a)        VectorOp<float_t>::exp(a)

// floating point comparison operations
#define CMPEQ(a,b)    VectorOp<float_t>::cmpeq(a,b)
#define CMPGT(a,b)    VectorOp<float_t>::cmpgt(a,b)
#define CMPLE(a,b)    VectorOp<float_t>::cmple(a,b)
#define CMPLT(a,b)    VectorOp<float_t>::cmplt(a,b)
#define ACMPLT(a,b)   VectorOp<float_t>::acmplt(a,b)
#define ACMPGT(a,b)   VectorOp<float_t>::acmpgt(a,b)
#define TEST(a,b)     VectorOp<float_t>::test(a,b)

// bit-wise operations
#define AND(a,b)      VectorOp<float_t>::and(a,b)
#define OR(a,b)       VectorOp<float_t>::or(a,b)
#define NOT(a)        VectorOp<float_t>::not(a)
#define SHR(a,b)      VectorOp<float_t>::shr<b>(a)
#define SAR(a,b)      VectorOp<float_t>::sar<b>(a)
#define SHL(a,b)      VectorOp<float_t>::shl<b>(a)

// conditional execution: m=mask, t=value if mask is all-ones, f=value if mask is all-zeros
#define CONDEXEC(m,t,f) VectorOp<float_t>::condexec(m,t,f)

// conversion operations
#define CVT_INT2FP(a) VectorOp<float_t>::cvtint2fp(a)      // int-to-float
#define CVT_FP2INT(a) VectorOp<float_t>::cvtfp2int(a)      // float-to-int, floor
#define INTRND(f)         VectorOp<float_t>::intrnd(f)          // float-to-int, round-to-nearest, handles negatives and positives
#define INTRND_POSONLY(f) VectorOp<float_t>::intrnd_posonly(f)  // float-to-int, round-to-nearest, only handles positive values

// integer operations
#define IADD(a,b)     VectorOp<float_t>::iadd(a,b)
#define ISUB(a,b)     VectorOp<float_t>::isub(a,b)
#define INEG(a,b)     VectorOp<float_t>::ineg(a)
#define IDIV_BY16(a)  VectorOp<float_t>::idiv_by16(a)
#define ICMPEQ(a,b)   VectorOp<float_t>::icmpeq(a,b)
#define ICMPLT(a,b)   VectorOp<float_t>::icmplt(a,b)
#define ICMPGT(a,b)   VectorOp<float_t>::icmpgt(a,b)
#define ICMPLE(a,b)   VectorOp<float_t>::icmple(a,b)
#define ICMPGE(a,b)   VectorOp<float_t>::icmpge(a,b)
#define IS_ODD(i)     VectorOp<float_t>::is_odd(i)

// vector creation/maninpuation
#define CREATE(a)     VectorOp<float_t>::create(a)
#define TO_SCALAR(a)  VectorOp<float_t>::to_scalar(a)    // return the first slot of a vector as a scalar

// floating point manipulation
#define INTXP(a)      VectorOp<float_t>::intxp(a)        // returns exponent as an integer
#define SETXP(f,exp)  VectorOp<float_t>::setxp(f,exp)    // set the integer exponent
#define ADX(f,exp)    VectorOp<float_t>::adx(f,exp)      // add an integer to the exponent
#define SDX(f,exp)    VectorOp<float_t>::sdx(f,exp)      // sub an integer from the exponent
#define SIGNOF(f)     VectorOp<float_t>::signof(f)       // returns sign as an integer (0 or 1)
#define REDUCE(f)     VectorOp<float_t>::reduce(f)       // reduction operation

// floating point constants
#define INF           VectorOp<float_t>::inf
#define NEGINF        VectorOp<float_t>::neginf
#define IND           VectorOp<float_t>::ind

template<int size> struct int_by_size { };
template<> struct int_by_size<4> { typedef __int32 type; };
template<> struct int_by_size<8> { typedef __int64 type; };

#if defined(_M_ARM)

// Implementation of VectorOp functionality for ARM using NEON intrinsics

#include <arm_neon.h>

// some of this code gets run from dynamic initializers. we must avoid
// executing NEON code in dynamic intializers as they machine may not
// support NEON. Just catch the illegal instruction exception and init
// to garbage.
// REMOVE THIS CODE WHEN NEON IS FULLY SUPPORTED BY ALL TARGETS
#include <excpt.h>
#define TRY_NEON(x) __try { x; } __except(GetExceptionCode()==0xc000001d) { }
// REMOVE THIS CODE WHEN NEON IS FULLY SUPPORTED BY ALL TARGETS

typedef __n128 vector_t;

template <typename float_t>
struct VectorOp
{
    // integer type same width as float_t
    typedef typename int_by_size<sizeof(float_t)>::type int_t;

    // floating point internals
    static const __n128 exponentBias;
    static const int significandBitCount = (sizeof(float_t)==sizeof(float)) ? 23: 53;
    static const __n128 exponentMask;
    static const __n128 notExponentMask;
    static const int maxExponent = (sizeof(float_t)==sizeof(float)) ? 128 : 1024;
    static const int minExponent = (sizeof(float_t)==sizeof(float)) ? -125 : -1021;

    // special FP constants
    static const __n128 inf;
    static const __n128 neginf;
    static const __n128 ind;

    static __n128 mul(__n128 x, __n128 y);
    static __n128 add(__n128 x, __n128 y);
    static __n128 sub(__n128 x, __n128 y);
    static __forceinline __n128 div(__n128 x, __n128 y)
    {
        return mul(x, recip(y));
    }

    static __n128 recip(__n128 x);
    static __n128 sqrt(__n128 x);
    static __n128 neg(__n128 x);
    static __n128 abs(__n128 x);
    static __n128 exp(__n128 x);

    static __n128 and(__n128 x, __n128 y);
    static __n128 or(__n128 x, __n128 y);
    static __n128 not(__n128 x);

    template<int count>
    static __forceinline __n128 shr(__n128 x)
    {
        if (sizeof(float_t)==sizeof(float))
            return vshrq_n_u32(x, count);
        else
            return vshrq_n_u64(x, count);
    }
    
    template<int count>
    static __forceinline __n128 sar(__n128 x)
    {
        if (sizeof(float_t)==sizeof(float))
            return vshrq_n_s32(x, count);
        else
            return vshrq_n_s64(x, count);
    }

    template<int count>
    static __forceinline __n128 shl(__n128 x)
    {
        if (sizeof(float_t)==sizeof(float))
            return vshlq_n_u32(x, count);
        else
            return vshlq_n_u64(x, count);
    }

    static __n128 cmpeq(__n128 x, __n128 y);
    static __n128 cmpgt(__n128 x, __n128 y);
    static __n128 cmplt(__n128 x, __n128 y);
    static __n128 cmple(__n128 x, __n128 y);

    static __n128 acmplt(__n128 x, __n128 y);
    static __n128 acmpgt(__n128 x, __n128 y);

    static __n128 test(__n128 x, __n128 y);

    static __forceinline __n128 condexec(__n128 mask, __n128 trueValue, __n128 falseValue)
    {
        // note: type on vbslq is meaningless so i just picked vbslq_u8
        return vbslq_u8(mask, trueValue, falseValue);
    }

    static __n128 cvtfp2int(__n128 x);
    static __n128 cvtint2fp(__n128 x);

    static float_t to_scalar(__n128 x);

    // integer opts
    static __n128 iadd(__n128 x, __n128 y);
    static __n128 isub(__n128 x, __n128 y);
    static __n128 ineg(__n128 x);
    static __n128 icmpeq(__n128 x, __n128 y);
    static __n128 icmplt(__n128 x, __n128 y);
    static __n128 icmpgt(__n128 x, __n128 y);
    static __n128 icmple(__n128 x, __n128 y);
    static __n128 icmpge(__n128 x, __n128 y);
    static __forceinline __n128 is_odd(__n128 x)
    {
        return test(create(1u), x);
    }
    // fast integer divide implementation... stolen from UTC ARM lower
    static __forceinline __n128 idiv_by16(__n128 x)
    {
        vector_t a = sar<3>(x);
        vector_t b = shr<28>(a);
        vector_t c = iadd(x, b);
        vector_t d = sar<4>(c);
        return d;
    }

    // return the exponent of the fp as an integer
    static __forceinline __n128 intxp(__n128 x)
    {
        __n128 exponent = and(x, exponentMask);
        exponent = shr<significandBitCount>(exponent);
        exponent = isub(exponent, exponentBias);
        return exponent;
    }

    // set the exponent of the fp
    static __forceinline __n128 setxp(__n128 x, __n128 exp)
    {
        __n128 exponent = iadd(exp, exponentBias);
        exponent = shl<significandBitCount>(exponent);
        __n128 significand = and(x, notExponentMask);
        __n128 result = or(significand, exponent);
        return result;
    }

    // add exp to x's exponent.
    // NOTE: this implmentation assumes no overflow!
    static __forceinline __n128 adx(__n128 x, __n128 exp)
    {
        __n128 expAddend = shl<significandBitCount>(exp);
        __n128 newFloat = iadd(x, expAddend);
        return newFloat;
    }

    // sub exp from x's exponent.
    // NOTE: this implmentation assumes no overflow!
    static __forceinline __n128 sdx(__n128 x, __n128 exp)
    {
        __n128 expAddend = shl<significandBitCount>(exp);
        __n128 newFloat = isub(x, expAddend);
        return newFloat;
    }

    // note this handles positive and negative values
    static __forceinline __n128 intrnd(__n128 x)
    {
        const __n128 zero = create((float_t)0.0);

        vector_t y = abs(x);
        vector_t n = intrnd_posonly(y);
        return condexec(cmplt(x, zero), ineg(n), n);
    }

    // note this handles ONLY positive values
    static __forceinline __n128 intrnd_posonly(__n128 x)
    {
        const __n128 half = create((float_t)0.5);
        return cvtfp2int(add(x, half)); // x >= 0, so floor(x+.5) is nearest(y)
    }

    static __forceinline __n128 signof(__n128 x)
    {
        const std::make_unsigned<int_t>::type mask = (sizeof(float_t)==sizeof(float)) ? 0x80000000u : 0x8000000000000000ull;
        return and(create(mask), x);
    }

    static __forceinline __n128 reduce(__n128 x)
    {
        return mul(cvtint2fp(cvtfp2int(mul(create((float_t)16.0), x))), create((float_t)0.0625));
    }

    // vector manipulation
    static __n128 create(unsigned __int64 x);
    static __n128 create(double x);
    static __n128 create(double x0, double x1);
    static __n128 create(unsigned __int64 x0, unsigned __int64 x1);
    static void unpack(__n128 v, unsigned __int64 &x0, unsigned __int64 &x1);
    static __n128 create(unsigned x);
    static __n128 create(float x);
    static __n128 create(float x0, float x1, float x2, float x3);
    static __n128 create(unsigned x0, unsigned x1, unsigned x2, unsigned x3);
    static void unpack(__n128 v, unsigned &x0, unsigned &x1, unsigned &x2, unsigned &x3);
};

__declspec(selectany) const __n128 VectorOp<float>::exponentMask = VectorOp<float>::create(0x7F800000u);
__declspec(selectany) const __n128 VectorOp<float>::notExponentMask = VectorOp<float>::create(0x807FFFFFu);

__declspec(selectany) const __n128 VectorOp<double>::exponentMask = VectorOp<double>::create(0x7FF0000000000000ull);
__declspec(selectany) const __n128 VectorOp<double>::notExponentMask = VectorOp<double>::create(0x800FFFFFFFFFFFFFull);

__declspec(selectany) const __n128 VectorOp<float>::exponentBias = VectorOp<float>::create(126u);
__declspec(selectany) const __n128 VectorOp<double>::exponentBias = VectorOp<double>::create(1022u);

__declspec(selectany) const __n128 VectorOp<float>::inf = VectorOp<float>::create(0x7f800000u);
__declspec(selectany) const __n128 VectorOp<float>::neginf = VectorOp<float>::create(0xff800000u);
__declspec(selectany) const __n128 VectorOp<float>::ind = VectorOp<float>::create(0xffc00000u);

__declspec(selectany) const __n128 VectorOp<double>::inf = VectorOp<double>::create(0x7ff0000000000000ull);
__declspec(selectany) const __n128 VectorOp<double>::neginf = VectorOp<double>::create(0xfff0000000000000ull);
__declspec(selectany) const __n128 VectorOp<double>::ind = VectorOp<double>::create(0xfff8000000000000ull);


template <> __forceinline __n128 VectorOp<float>::mul(__n128 x, __n128 y)
{
    return vmulq_f32(x, y);
}

template <> __forceinline __n128 VectorOp<float>::add(__n128 x, __n128 y)
{
    return vaddq_f32(x, y);
}

template <> __forceinline __n128 VectorOp<float>::sub(__n128 x, __n128 y)
{
    return vsubq_f32(x, y);
}

template <> __forceinline __n128 VectorOp<float>::recip(__n128 x)
{
    __n128 recipe = vrecpeq_f32(x);
    __n128 recips = vrecpsq_f32(x, recipe);
    recipe = vmulq_f32(recipe, recips);
    recips = vrecpsq_f32(x, recipe);
    recipe = vmulq_f32(recipe, recips);
    return recipe;
}

// estimate + 2 iterations of Newton-Rahpson
template <> __forceinline __n128 VectorOp<float>::sqrt(__n128 x)
{
    __n128 rsqrte = vrsqrteq_f32(x);
    __n128 rsqrts = vrsqrtsq_f32(x, vmulq_f32(rsqrte, rsqrte));
    rsqrte = vmulq_f32(rsqrte, rsqrts);
    rsqrts = vrsqrtsq_f32(x, vmulq_f32(rsqrte, rsqrte));
    rsqrte = vmulq_f32(rsqrte, rsqrts);
    return recip(rsqrte);
}

template <> __forceinline __n128 VectorOp<float>::neg(__n128 x)
{
    return vnegq_f32(x);
}

template <> __forceinline __n128 VectorOp<float>::abs(__n128 x)
{
    return vabsq_f32(x);
}

extern "C" vector_t __vdecl_expf4(vector_t x);
template <> __forceinline __n128 VectorOp<float>::exp(__n128 x)
{
    return __vdecl_expf4(x);
}

template <> __forceinline __n128 VectorOp<double>::and(__n128 x, __n128 y)
{
    return vandq_s64(x, y);
}

template <> __forceinline __n128 VectorOp<float>::and(__n128 x, __n128 y)
{
    return vandq_s32(x, y);
}

template <> __forceinline __n128 VectorOp<double>::or(__n128 x, __n128 y)
{
    return vorrq_s64(x, y);
}

template <> __forceinline __n128 VectorOp<float>::or(__n128 x, __n128 y)
{
    return vorrq_s32(x, y);
}

template <> __forceinline __n128 VectorOp<float>::not(__n128 x)
{
    return vmvnq_s32(x);
}

template <> __forceinline __n128 VectorOp<float>::cmpeq(__n128 x, __n128 y)
{
    return vceqq_f32(x, y);
}
template <> __forceinline __n128 VectorOp<float>::cmpgt(__n128 x, __n128 y)
{
    return vcgtq_f32(x, y);
}
template <> __forceinline __n128 VectorOp<float>::cmplt(__n128 x, __n128 y)
{
    return vcltq_f32(x, y);
}
template <> __forceinline __n128 VectorOp<float>::cmple(__n128 x, __n128 y)
{
    return vcleq_f32(x, y);
}

template <> __forceinline __n128 VectorOp<float>::acmpgt(__n128 x, __n128 y)
{
    return vacgtq_f32(x, y);
}
template <> __forceinline __n128 VectorOp<float>::acmplt(__n128 x, __n128 y)
{
    return vacltq_f32(x, y);
}

template <> __forceinline __n128 VectorOp<float>::test(__n128 x, __n128 y)
{
    return vtstq_s32(x, y);
}

template <> __forceinline __n128 VectorOp<float>::cvtfp2int(__n128 x)
{
    return vcvtq_s32_f32(x);
}

template <> __forceinline __n128 VectorOp<float>::cvtint2fp(__n128 x)
{
    return vcvtq_f32_s32(x);
}

template <> __forceinline float VectorOp<float>::to_scalar(__n128 x)
{
    return vgetq_lane_f32(x, 0);
}

template <> __forceinline double VectorOp<double>::to_scalar(__n128 x)
{
    unsigned __int64 y = vgetq_lane_u64(x, 0);
    return *(double *)&y;
}

// integer ops


template <> __forceinline __n128 VectorOp<float>::iadd(__n128 x, __n128 y)
{
    return vaddq_s32(x, y);
}
template <> __forceinline __n128 VectorOp<float>::isub(__n128 x, __n128 y)
{
    return vsubq_s32(x, y);
}
template <> __forceinline __n128 VectorOp<float>::ineg(__n128 x)
{
    return vnegq_s32(x);
}
template <> __forceinline __n128 VectorOp<float>::icmpeq(__n128 x, __n128 y)
{
    return vceqq_s32(x, y);
}
template <> __forceinline __n128 VectorOp<float>::icmplt(__n128 x, __n128 y)
{
    return vcltq_s32(x, y);
}
template <> __forceinline __n128 VectorOp<float>::icmpgt(__n128 x, __n128 y)
{
    return vcgtq_s32(x, y);
}
template <> __forceinline __n128 VectorOp<float>::icmple(__n128 x, __n128 y)
{
    return vcleq_s32(x, y);
}
template <> __forceinline __n128 VectorOp<float>::icmpge(__n128 x, __n128 y)
{
    return vcgeq_s32(x, y);
}
template <> __forceinline __n128 VectorOp<double>::create(unsigned __int64 x)
{
    __n128 n;
    TRY_NEON(n = vdupq_n_u64(x));
    return n;
}

template <> __forceinline __n128 VectorOp<double>::create(unsigned x)
{
    __n128 n;
    TRY_NEON(n = vdupq_n_u64((unsigned __int64)x));
    return n;
}

template <> __forceinline __n128 VectorOp<double>::create(double x)
{
    __n128 n;
    TRY_NEON(n = vdupq_n_u64(*(unsigned __int64*)&x));
    return n;
}

template <> __forceinline __n128 VectorOp<double>::create(double x0, double x1)
{
    __n128 n;
    n.n128_u64[0] = *(unsigned __int64*)&x0;
    n.n128_u64[1] = *(unsigned __int64*)&x1;
    return n;
}

template <> __forceinline __n128 VectorOp<double>::create(unsigned __int64 x0, unsigned __int64 x1)
{
    __n128 n;
    n.n128_u64[0] = x0;
    n.n128_u64[1] = x1;
    return n;
}

template <> __forceinline void VectorOp<double>::unpack(__n128 v, unsigned __int64 &x0, unsigned __int64 &x1)
{
    x0 = vgetq_lane_u64(v, 0);
    x1 = vgetq_lane_u64(v, 1);
}

template <> __forceinline __n128 VectorOp<float>::create(unsigned x)
{
    __n128 n;
    TRY_NEON(n = vdupq_n_u32(x));
    return n;
}

template <> __forceinline __n128 VectorOp<float>::create(float x)
{
    __n128 n;
    TRY_NEON(n = vdupq_n_f32(x));
    return n;
}

template <> __forceinline __n128 VectorOp<float>::create(float x0, float x1, float x2, float x3)
{
    __n128 n;
    n.n128_f32[0] = x0;
    n.n128_f32[1] = x1;
    n.n128_f32[2] = x2;
    n.n128_f32[3] = x3;
    return n;
}

template <> __forceinline __n128 VectorOp<float>::create(unsigned x0, unsigned x1, unsigned x2, unsigned x3)
{
    __n128 n;
    n.n128_u32[0] = x0;
    n.n128_u32[1] = x1;
    n.n128_u32[2] = x2;
    n.n128_u32[3] = x3;
    return n;
}

template <> __forceinline void VectorOp<float>::unpack(__n128 v, unsigned &x0, unsigned &x1, unsigned &x2, unsigned &x3)
{
    x0 = vgetq_lane_u32(v, 0);
    x1 = vgetq_lane_u32(v, 1);
    x2 = vgetq_lane_u32(v, 2);
    x3 = vgetq_lane_u32(v, 3);
}

#else

// TBD: implement VectorOp for other achitectures
#error helpers not defined for this platform

#endif
