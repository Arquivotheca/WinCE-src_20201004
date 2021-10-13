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

// Vectorized atan and atan C library functions.

// Implementation derived from the work described in "Software Manual for the
// Elementary Functions" by William J Cody, Jr. and William Waite.

// The implementation is designed to be portable accross multiple vector
// architectures. See svml_vop.h.

#include <type_traits>
#include "svml_vop.h"

template <typename float_t>
class VATan
{
    static_assert(std::is_floating_point<float_t>::value, "float_t must be a floating point type");

    static const vector_t eps;
    static const vector_t two_minus_sqrt_three, sqrt_three;
    static vector_t A(vector_t index);  // constants {0, pi/6, pi/2, pi/3}

    // polynomial coefficients
    static const vector_t p0, p1, p2, p3;
    static const vector_t q0, q1, q2, q3, q4;

public:
    static __forceinline vector_t atan(vector_t x)
    {
        const vector_t zero = CREATE((float_t)0.0);

        vector_t result = atan_impl(ABS(x));

        result = CONDEXEC(CMPLT(x, zero), NEG(result), result);

        // check edge cases
        
        result = CONDEXEC(CMPEQ(x, x), result, x);                // if x is NaN, result = NaN

        return result;
    }

    static __forceinline vector_t atan2(vector_t v, vector_t u)
    {
        const vector_t zero = CREATE((float_t)0.0);
        const vector_t negzero = CREATE((float_t)-0.0);
        const vector_t pi = CREATE((float_t)3.14159265358979323846);
        const vector_t pi_over_two = CREATE((float_t)1.57079632679489661923);
        const vector_t pi_over_four = CREATE((float_t)0.785398163397448309616);
        const vector_t three_pi_over_four = CREATE((float_t)2.35619449019234492885);

        // limits for overflow/underflow
        const vector_t overflow_limit = CREATE((unsigned)(VectorOp<float_t>::maxExponent - 3));
        const vector_t underflow_limit = CREATE((unsigned)(VectorOp<float_t>::minExponent + 3));

        // calculate atan2(v,u) = atan(v/u)
        vector_t result = atan_impl(ABS(DIV(v,u)));

        result = CONDEXEC(CMPLT(u, zero), SUB(pi, result), result);           // u < zero, (negate and) shift
        result = CONDEXEC(CMPLT(v, zero), NEG(result), result);               // v < zero, negate

        // check for edge cases

        vector_t u_is_zero = CMPEQ(u, zero);
        vector_t v_is_zero = CMPEQ(v, zero);
        vector_t is_overflow = ICMPGE((ISUB(INTXP(v), INTXP(u))), overflow_limit);
        vector_t is_underflow = ICMPLE((ISUB(INTXP(v), INTXP(u))), underflow_limit);

        result = CONDEXEC(OR(u_is_zero, is_overflow), pi_over_two, result);   // if u = 0 or v/u overflow, return pi/2
        result = CONDEXEC(is_underflow, zero, result);                        // if v/u underflow, return 0

        result = CONDEXEC(AND(CMPEQ(u,u), CMPEQ(v,v)), result, u);            // if x is NaN or y is NaN, return NaN

        vector_t sign_of_v = SIGNOF(v);

        result = CONDEXEC(AND(v_is_zero, CMPLT(u, zero)), OR(sign_of_v, pi), result);        // if v is +/-0.0 and u < 0.0 return +/-pi
        result = CONDEXEC(AND(v_is_zero, CMPGT(u, zero)), OR(sign_of_v, zero), result);      // if v is +/-0.0 and u > 0.0 return +/-0.0
        result = CONDEXEC(AND(v_is_zero, ICMPEQ(u, negzero)), OR(sign_of_v, pi), result);    // if v is +/-0.0 and u is -0.0 return +/-pi
        result = CONDEXEC(AND(v_is_zero, ICMPEQ(u, zero)), OR(sign_of_v, zero), result);     // if v is +/-0.0 and u is +0.0 return +/-0.0

        vector_t absv_is_inf = CMPEQ(ABS(v), INF);

        result = CONDEXEC(AND(absv_is_inf, CMPEQ(u, INF)), OR(sign_of_v, three_pi_over_four), result);  // if v is +/-inf and u is -inf, return +/-(3*pi/4)
        result = CONDEXEC(AND(absv_is_inf, CMPEQ(u, NEGINF)), OR(sign_of_v, pi_over_four), result);     // if v is +/-inf and u is +inf, return +/-(pi/4)

        return result;
    }

private:

    static __forceinline vector_t atan_impl(vector_t f)
    {
        const vector_t half = CREATE((float_t)0.5);
        const vector_t one = CREATE((float_t)1.0);
        const vector_t pi_over_two = CREATE((float_t)1.57079632679489661923);

        vector_t n = CREATE(0u);

        //if (f > one)
        //{
        //     f = 1/f;
        //     n = 2;
        //}
        vector_t f_gt_one = CMPGT(f, one);
        f = CONDEXEC(f_gt_one, RECIP(f), f);
        n = CONDEXEC(f_gt_one, CREATE(2u), n);

        //if (f > two_minus_sqrt_three)
        //{
        //    
        //   f = ((((sqrt_three - 1) * f - half ) - half) + f) / (sqrt_three + f);
        //   ++n;
        //}
        vector_t f_gt_tmst = CMPGT(f, two_minus_sqrt_three);
        f = CONDEXEC(f_gt_tmst, DIV(ADD(SUB(SUB(MUL(SUB(sqrt_three, one), f), half), half), f), ADD(sqrt_three, f)), f);
        n = CONDEXEC(f_gt_tmst, IADD(n, CREATE(1u)), n);
        
        // g = f * f;
        // compute R(g) = g * P(g)/Q(g)
        vector_t g = MUL(f, f);
        
        vector_t gP, Q;
        if (sizeof(float_t) == sizeof(float))
        {
            //float_t gP = (p1 * g + p0) * g;
            //float_t Q = g + q0;
            gP = MUL(ADD(MUL(p1, g), p0), g);
            Q = ADD(g, q0);
        }
        else
        {
            //float_t gP = (((p3 * g + p2) * g + p1) * g + p0) * g;
            //float_t Q =  (((g + q3) * g + q2) * g + q1) * g + q0;
            gP = MUL(ADD(MUL(ADD(MUL(ADD(MUL(p3, g), p2), g), p1), g), p0), g);
            Q = ADD(MUL(ADD(MUL(ADD(MUL(ADD(g, q3), g), q2), g), q1), g), q0);
        }

        vector_t R = DIV(gP, Q);
        vector_t result = ADD(f, MUL(f, R));

        // if abs(f) < eps, result = f
        result = CONDEXEC(ACMPLT(f, eps), f, result);

        //if (n > 1)
        //    result = -result;
        result = CONDEXEC(ICMPGT(n, CREATE(1u)), NEG(result), result);

        result = ADD(A(n), result);

        // check edge cases

        result = CONDEXEC(CMPEQ(f, INF), pi_over_two, result);      // if x==+/-inf, result = +/-(pi/2)

        return result;
    }
};

template<> vector_t VATan<double>::A(vector_t index)
{
    const double a[4] = {  /* 0.0  */ 0.0, 
                            /* pi/6 */ 0.52359877559829887308, 
                            /* pi/2 */ 1.57079632679489661923,
                            /* pi/3 */ 1.04719755119659774615 };

    unsigned __int64 x0, x1;
    VectorOp<double>::unpack(index, x0, x1);
    return VectorOp<double>::create(a[x0], a[x1]);
}

template<> vector_t VATan<float>::A(vector_t index)
{
    const float a[4] = {   /* 0.0  */ 0.0f, 
                            /* pi/6 */ 0.52359877559829887308f, 
                            /* pi/2 */ 1.57079632679489661923f,
                            /* pi/3 */ 1.04719755119659774615f };

    unsigned x0, x1, x2, x3;
    VectorOp<float>::unpack(index, x0, x1, x2, x3);
    return VectorOp<float>::create(a[x0], a[x1], a[x2], a[x3]);
}

// constants

const vector_t VATan<double>::eps = VectorOp<double>::create(1.4901161193847656e-008);  // 2^(-53/2)
const vector_t VATan<float>::eps = VectorOp<float>::create(2.44140625e-004f);            // 2^(-24/2)

// 2-sqrt(3)
const vector_t VATan<double>::two_minus_sqrt_three = VectorOp<double>::create(0x3fd126145e9ecd56ull);
const vector_t VATan<float>::two_minus_sqrt_three = VectorOp<float>::create(0x3e8930a2u);

// sqrt(3)
const vector_t VATan<double>::sqrt_three = VectorOp<double>::create(0x3ffbb67ae8584caaull);
const vector_t VATan<float>::sqrt_three = VectorOp<float>::create(0x3fddb3d7u);

// polynomial coefficients

const vector_t VATan<double>::p0 = VectorOp<double>::create(-0.13688768894191926929e+2);
const vector_t VATan<double>::p1 = VectorOp<double>::create(-0.20505855195861651981e+2);
const vector_t VATan<double>::p2 = VectorOp<double>::create(-0.84946240351320683534e+1);
const vector_t VATan<double>::p3 = VectorOp<double>::create(-0.83758299368150059274e+0);

const vector_t VATan<double>::q0 = VectorOp<double>::create(0.41066306682575781263e+2);
const vector_t VATan<double>::q1 = VectorOp<double>::create(0.86157349597130242515e+2);
const vector_t VATan<double>::q2 = VectorOp<double>::create(0.59578436142597344465e+2);
const vector_t VATan<double>::q3 = VectorOp<double>::create(0.15024001160028576121e+2);
const vector_t VATan<double>::q4 = VectorOp<double>::create(1.0);

const vector_t VATan<float>::p0 = VectorOp<float>::create(-0.4708325141e+0f);
const vector_t VATan<float>::p1 = VectorOp<float>::create(-0.5090958253e-1f);
const vector_t VATan<float>::p2 = VectorOp<float>::create( 0.0f);
const vector_t VATan<float>::p3 = VectorOp<float>::create( 0.0f);

const vector_t VATan<float>::q0 = VectorOp<float>::create(0.1412500740e+1f);
const vector_t VATan<float>::q1 = VectorOp<float>::create(1.0f);
const vector_t VATan<float>::q2 = VectorOp<float>::create(0.0f);
const vector_t VATan<float>::q3 = VectorOp<float>::create(0.0f);
const vector_t VATan<float>::q4 = VectorOp<float>::create(0.0f);

extern "C"
{
    vector_t __vdecl_atanf4(vector_t x) { return VATan<float>::atan(x); }
    vector_t __vdecl_atan2f4(vector_t y, vector_t x) { return VATan<float>::atan2(y, x); }
}
