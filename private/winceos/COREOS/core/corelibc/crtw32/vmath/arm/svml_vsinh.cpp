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

#include <type_traits>

#include "svml_vop.h"

template <typename float_t>
class VSinh
{
    static const vector_t eps, ybar, wmax;
    static const vector_t ln_v, one_over_v_squared, v_over_two_minus_1; // ln(v), v^-2, (v/2)-1
    
    // polynomial coefficients
    static const vector_t p0, p1, p2, p3;
    static const vector_t q0, q1, q2, q3;

public:
    static __forceinline vector_t sinh(vector_t x)
    {
        static_assert(sizeof(float_t)==sizeof(float), "f64 not supported");

        const vector_t zero = CREATE((float_t)0.0);
        const vector_t one = CREATE((float_t)1.0);

        vector_t y = ABS(x);

        // compute approximation using cosh (used if y > 1)
        vector_t func_result = cosh_impl<false>(y);
        func_result = CONDEXEC(CMPLT(x, zero), NEG(func_result), func_result);

        // compuate polynomial approximation (used if eps < y <= 1)
        // result = x + x * R(x^2)
        // R(x^2) = R(f) = f * (P(f)/Q(f))
        vector_t P, Q;
        vector_t f = MUL(x, x);
        if (sizeof(float_t) == sizeof(float))
        {
            //P = p1 * f + p0;
            //Q = f + q0;
            P = ADD(MUL(p1, f), p0);
            Q = ADD(f, q0);
        }
        else
        {
            //P = ((p3 * f + p2) * f + p1) * f + p0;
            //Q = ((f + q2) * f + q1) * f + q0;
            ADD(MUL(ADD(MUL(ADD(MUL(p3, f), p2), f), p1), f), p0);
            ADD(MUL(ADD(MUL(ADD(f, q2), f), q1), f), q0);
        }

        vector_t R = MUL(f, DIV(P, Q));
        vector_t poly_approx = CONDEXEC(CMPLT(y, eps), x, ADD(x, MUL(x, R)));

        vector_t result = CONDEXEC(CMPGT(y, one), func_result, poly_approx);

        // check edge cases

        result = CONDEXEC(CMPEQ(x, x), result, x);      // if x is NaN, return NaN
        result = CONDEXEC(CMPEQ(y, INF), x, result);    // if x is +/inf, return x

        return result;
    }

    static __forceinline vector_t cosh(vector_t x)
    {
        vector_t y = ABS(x);
        vector_t result = cosh_impl<true>(y);
        
        // check edge cases

        result = CONDEXEC(CMPEQ(x, x), result, x);      // if x is NaN, return NaN
        result = CONDEXEC(CMPEQ(y, INF), INF, result);  // if x is +/-inf, return +inf       

        return result;
    }
    
    static float_t sinh(float_t x) { return TO_SCALAR(sinh(CREATE(x))); }
    static float_t cosh(float_t x) { return TO_SCALAR(cosh(CREATE(x))); }

private:

    template<bool is_cosh>
    static __forceinline vector_t cosh_impl(vector_t y)
    {
        vector_t y_gt_ybar = CMPGT(y, ybar);

        // if y > ybar
        vector_t w = SUB(y, ln_v);
        vector_t z = EXP(w);
        vector_t result1 = ADD(z, MUL(v_over_two_minus_1, z));

        // if y <= ybar
        vector_t u = EXP(y);
        vector_t result2;
        if (is_cosh) { // cosh
            // result = (z + 1.0/z) / 2.0;
            result2 = SDX(ADD(u, RECIP(u)), CREATE(1u));
        } else {  // sinh
            // result = (z - 1.0/z) / 2.0;
            result2 = SDX(SUB(u, RECIP(u)), CREATE(1u));
        }

        vector_t result = CONDEXEC(y_gt_ybar, result1, result2);

        // check edge cases

        result = CONDEXEC(AND(y_gt_ybar, CMPGT(w, wmax)), INF, result);  // if y > ybar && w > wmax, return infinity

        return result;
    }
};

// constants

const vector_t VSinh<double>::eps = VectorOp<double>::create(1.4901161193847656e-008);  // 2^(-53/2)
const vector_t VSinh<float>::eps = VectorOp<float>::create(2.44140625e-004f);           // 2^(-24/2)

// limit on exp(y) and 1/exp(y)
const vector_t VSinh<double>::ybar = VectorOp<double>::create(709.78271289338397);
const vector_t VSinh<float>::ybar = VectorOp<float>::create(88.72283935546875f);

// wmax < ybar and wmax < ln(xmax)-ln(v)+0.69
const vector_t VSinh<double>::wmax = VectorOp<double>::create(709.7796);
const vector_t VSinh<float>::wmax = VectorOp<float>::create(88.71968f);

// ln(v) is an exact machine value slightly larger than ln(2)
const vector_t VSinh<double>::ln_v = VectorOp<double>::create(0x3fe62e6000000000ull);
const vector_t VSinh<float>::ln_v = VectorOp<float>::create(0x3f317300u);

const vector_t VSinh<double>::one_over_v_squared = VectorOp<double>::create(0.24999308500451499336e+0);
const vector_t VSinh<float>::one_over_v_squared =  VectorOp<float>::create(0.24999308500451499336e+0f);

const vector_t VSinh<double>::v_over_two_minus_1 = VectorOp<double>::create(0.13830277879601902638e-4);
const vector_t VSinh<float>::v_over_two_minus_1 = VectorOp<float>::create(0.13830277879601902638e-4f);

// polynomial coefficients

const vector_t VSinh<double>::p0 = VectorOp<double>::create(-0.35181283430177117881e+6);
const vector_t VSinh<double>::p1 = VectorOp<double>::create(-0.11563521196851768270e+5);
const vector_t VSinh<double>::p2 = VectorOp<double>::create(-0.16375798202630751372e+3);
const vector_t VSinh<double>::p3 = VectorOp<double>::create(-0.78966127417357099479e+0);

const vector_t VSinh<double>::q0 = VectorOp<double>::create(-0.21108770058106271242e+7);
const vector_t VSinh<double>::q1 = VectorOp<double>::create( 0.36162723109421836460e+5); 
const vector_t VSinh<double>::q2 = VectorOp<double>::create(-0.27773523119650701667e+3);
const vector_t VSinh<double>::q3 = VectorOp<double>::create( 1.0);

const vector_t VSinh<float>::p0 = VectorOp<float>::create(-0.713793159e+1f);
const vector_t VSinh<float>::p1 = VectorOp<float>::create(-0.190333399e+0f);
const vector_t VSinh<float>::p2 = VectorOp<float>::create( 0.0f);
const vector_t VSinh<float>::p3 = VectorOp<float>::create( 0.0f);

const vector_t VSinh<float>::q0 = VectorOp<float>::create(-0.428277109e+2f);
const vector_t VSinh<float>::q1 = VectorOp<float>::create( 1.0f);
const vector_t VSinh<float>::q2 = VectorOp<float>::create( 0.0f);
const vector_t VSinh<float>::q3 = VectorOp<float>::create( 0.0f);

extern "C"
{
    vector_t __vdecl_sinhf4(vector_t x) { return VSinh<float>::sinh(x); }
    vector_t __vdecl_coshf4(vector_t x) { return VSinh<float>::cosh(x); }
}
