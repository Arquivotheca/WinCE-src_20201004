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
class VTanh
{
    static_assert(std::is_floating_point<float_t>::value, "non-fp not supported");

    static const vector_t eps, xbig;

    // polynomial coefficients
    static const vector_t p0, p1, p2;
    static const vector_t q0, q1, q2, q3;

public:

    static __forceinline vector_t tanh(vector_t x)
    {
        static_assert(sizeof(float_t)==sizeof(float), "f64 not supported");

        const vector_t zero = CREATE((float_t)0.0);
        const vector_t one = CREATE((float_t)1.0);
        const vector_t half = CREATE((float_t)0.5);
        const vector_t ln3_over_two = CREATE((float_t)0.54930614433405484570); // ln(3)/2

        vector_t f = ABS(x);

        // exp based approximation
        // result = half - one/(exp(f+f) + one);
        // result = result + result;
        vector_t exp_approx = SUB(half, RECIP(ADD(EXP(ADD(f, f)), one)));
        exp_approx = ADD(exp_approx, exp_approx);

        // rational polynomial approximation R(g)
        // R(g) = g * P(g) / Q(g)
        vector_t gP, Q;
        vector_t g = MUL(f, f);
        if (sizeof(float_t) == sizeof(float))
        {
            //gP = (p1 * g + p0) * g;
            //Q = g + q0;
            gP = MUL(ADD(MUL(p1, g), p0), g);
            Q = ADD(g, q0);
        }
        else
        {
            // gP = ((p2 * g + p1) * g + p0) * g;
            // Q = ((g + q2) * g + q1) * g + q0;
            gP = MUL(ADD(MUL(ADD(MUL(p2, g), p1), g), p0), g);
            Q = ADD(MUL(ADD(MUL(ADD(g, q2), g), q1), g), q0);
        }
        vector_t R = DIV(gP, Q);
        vector_t poly_approx = ADD(f, MUL(f, R));

        // if f < eps, result = f
        // if eps < f < ln3_over_two, result = poly_approx
        // if ln3_over_two < f < xbig, result = exp_approx
        // if f > xbig, result = 1.0
        vector_t result = CONDEXEC(CMPGT(f, ln3_over_two), exp_approx, poly_approx);
        result = CONDEXEC(CMPGT(f, xbig), one, result);
        result = CONDEXEC(CMPLT(f, eps), f, result);
        result = CONDEXEC(CMPLT(x, zero), NEG(result), result);  // if x < 0, return -result

        // check edge cases
        result = CONDEXEC(CMPEQ(f, INF), OR(SIGNOF(x), one), result);  // if x is +/-inf, result = +/-1.0
        result = CONDEXEC(CMPEQ(x, x), result, x);                     // if x is NaN, result = NaN

        return result;
    }

    static __forceinline float_t tanh(float_t x) { return TO_SCALAR(tanh(CREATE(x))); }
};

// constants

const vector_t VTanh<double>::eps = VectorOp<double>::create(1.4901161193847656e-008);  // 2^(-53/2)
const vector_t VTanh<float>::eps = VectorOp<float>::create(2.44140625e-004f);           // 2^(-24/2)

// as tanh gets larger than this value it becomes exactly 1.0
const vector_t VTanh<double>::xbig = VectorOp<double>::create(19.061548233032227);
const vector_t VTanh<float>::xbig = VectorOp<float>::create(9.010913f);

// polynomial coefficients

const vector_t VTanh<double>::p0 = VectorOp<double>::create(-0.16134119021996228053e+4);
const vector_t VTanh<double>::p1 = VectorOp<double>::create(-0.99225929672236083313e+2);
const vector_t VTanh<double>::p2 = VectorOp<double>::create(-0.96437492777225469787e+0);

const vector_t VTanh<double>::q0 = VectorOp<double>::create(0.48402357071988688686e+4);
const vector_t VTanh<double>::q1 = VectorOp<double>::create(0.22337720718962312926e+4);
const vector_t VTanh<double>::q2 = VectorOp<double>::create(0.11274474380534949335e+3);
const vector_t VTanh<double>::q3 = VectorOp<double>::create(1.0);

const vector_t VTanh<float>::p0 = VectorOp<float>::create(-0.8237728127e+0f);
const vector_t VTanh<float>::p1 = VectorOp<float>::create(-0.3831010665e-2f);
const vector_t VTanh<float>::p2 = VectorOp<float>::create( 0.0f);

const vector_t VTanh<float>::q0 = VectorOp<float>::create( 0.2471319654e+1f);
const vector_t VTanh<float>::q1 = VectorOp<float>::create( 1.0f);
const vector_t VTanh<float>::q2 = VectorOp<float>::create( 0.0f);
const vector_t VTanh<float>::q3 = VectorOp<float>::create( 0.0f);

extern "C"
{
    vector_t __vdecl_tanhf4(vector_t x) { return VTanh<float>::tanh(x); }
}
