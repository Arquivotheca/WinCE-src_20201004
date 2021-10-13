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

// Vectorized asin and acos C library functions.

// Implementation derived from the work described in "Software Manual for the
// Elementary Functions" by William J Cody, Jr. and William Waite.

// The implementation is designed to be portable accross multiple vector
// architectures. See svml_vop.h.

#include <type_traits>
#include "svml_vop.h"

template <typename float_t>
class VASin
{
    static_assert(std::is_floating_point<float_t>::value, "float_t must be a floating point type");

    static const vector_t eps; // limit

    // constants arrays
    static vector_t A(vector_t index);
    static vector_t B(vector_t index);

    // polynomial coefficients
    static const vector_t p1, p2, p3, p4, p5;
    static const vector_t q0, q1, q2, q3, q4, q5;

public:
    static __forceinline vector_t asin(vector_t x)
    {
        const vector_t zero = CREATE((float_t)0.0);

        vector_t i = CREATE(0u);
        vector_t result = asin_impl(x, i);

        // result = (a[i] + result) + a[i];
        vector_t a = A(i);
        result = ADD(ADD(a, result), a);

        // if (x < 0) result = -result;
        // return result;
        result = CONDEXEC(CMPLT(x, zero), NEG(result), result);

        return asin_check_special_case(x, result);
    }

    static __forceinline vector_t acos(vector_t x)
    {
        const vector_t zero = CREATE((float_t)0.0);

        vector_t i = CREATE(1u);
        vector_t result = asin_impl(x, i);
       
        //if (x < 0)
        //    return (B[i] + result) + B[i];
        //else
        //    return (A[i] - result) + A[i];

        vector_t b = B(i);
        vector_t a = A(i);
        vector_t x_lt_zero_result = ADD(ADD(b, result), b);
        vector_t x_ge_zero_result = ADD(SUB(a, result), a);

        result = CONDEXEC(CMPLT(x, zero), x_lt_zero_result, x_ge_zero_result);

        return asin_check_special_case(x, result);
    }

private:

    static __forceinline vector_t asin_impl(const vector_t x, vector_t &index)
    {
        const vector_t half = CREATE((float_t)0.5);
        const vector_t one = CREATE((float_t)1.0);

        vector_t absx = ABS(x);
        vector_t y = absx;

        // if (y > half)
        //      index = 1 - index;
        //      g = (one-y) * half;
        //      y = -sqrt(g)
        //      y = y + y
        // else
        //      g = y * y
        vector_t y_gt_half = CMPGT(y, half);
        index = CONDEXEC(y_gt_half, ISUB(CREATE(1u), index), index);
        vector_t g = CONDEXEC(y_gt_half, MUL(SUB(one, y), half), MUL(y, y));
        vector_t neg_sqrt_g = NEG(SQRT(g));
        y = CONDEXEC(y_gt_half, ADD(neg_sqrt_g, neg_sqrt_g), y);

        // evaluate R(g) = g * P(g)/Q(g)
        vector_t gP, Q;
        if (sizeof(float_t)==sizeof(float))
        {
            // gP = (p2 * g + p1) * g;
            // Q =  (q2 * g + q1) * g + q0;
            gP = MUL(ADD(MUL(p2, g), p1), g);
            Q = ADD(MUL(ADD(g, q1), g), q0);
        }
        else
        {
            // gP = ((((p5 * g + p4) * g + p3) * g + p2) * g + p1) * g;
            // Q =  ((((q5 * g + q4) * g + q3) * g + q2) * g + q1) * g + q0;
            gP = MUL(ADD(MUL(ADD(MUL(ADD(MUL(ADD(MUL(p5, g), p4), g), p3), g), p2), g), p1), g);
            Q = ADD(MUL(ADD(MUL(ADD(MUL(ADD(MUL(ADD(g, q4), g), q3), g), q2), g), q1), g), q0);
        }

        // R = gP / Q;
        vector_t R = DIV(gP, Q);

        //return y + y * R;
        vector_t result = ADD(y, MUL(y, R));

        // Check for Edge cases

        result = CONDEXEC(CMPLT(absx, eps), y, result);       // if -eps < x < eps, return x

        return result;
    }

    // these have to be checked last as an operation on IND will change it to QNAN
    static __forceinline vector_t asin_check_special_case(const vector_t x, vector_t result)
    {
        const vector_t one = CREATE((float_t)1.0);

        result = CONDEXEC(ACMPGT(x, one), IND, result);       // if not -1 <= x <= 1, return IND
        result = CONDEXEC(CMPEQ(x, x), result, x);            // if x is NaN, return NaN
        return result;
    }
};


// limits
const vector_t VASin<double>::eps = VectorOp<double>::create(1.4901161193847656e-008);   // 2^(-53/2)
const vector_t VASin<float>::eps = VectorOp<float>::create(2.44140625e-004f);            // 2^(-24/2)

// polynomial coefficients
const vector_t VASin<double>::p1 = VectorOp<double>::create(-0.27368494524164255994e+2);
const vector_t VASin<double>::p2 = VectorOp<double>::create( 0.57208227877891731407e+2);
const vector_t VASin<double>::p3 = VectorOp<double>::create(-0.39688862997504877339e+2);
const vector_t VASin<double>::p4 = VectorOp<double>::create( 0.10152522233806463645e+2);
const vector_t VASin<double>::p5 = VectorOp<double>::create(-0.69674573447350646411e+0);

const vector_t VASin<double>::q0 = VectorOp<double>::create(-0.16421096714498560795e+3);
const vector_t VASin<double>::q1 = VectorOp<double>::create( 0.41714430248260412556e+3);
const vector_t VASin<double>::q2 = VectorOp<double>::create(-0.38186303361750149284e+3);
const vector_t VASin<double>::q3 = VectorOp<double>::create( 0.15095270841030604719e+3);
const vector_t VASin<double>::q4 = VectorOp<double>::create(-0.23823859153670238830e+2);
const vector_t VASin<double>::q5 = VectorOp<double>::create( 1.0);

const vector_t VASin<float>::p1 = VectorOp<float>::create( 0.933935835e+0f);
const vector_t VASin<float>::p2 = VectorOp<float>::create(-0.504400557e+0f);
const vector_t VASin<float>::p3 = VectorOp<float>::create( 0.0f);
const vector_t VASin<float>::p4 = VectorOp<float>::create( 0.0f);
const vector_t VASin<float>::p5 = VectorOp<float>::create( 0.0f);

const vector_t VASin<float>::q0 = VectorOp<float>::create( 0.560363004e+1f);
const vector_t VASin<float>::q1 = VectorOp<float>::create(-0.554846723e+1f);
const vector_t VASin<float>::q2 = VectorOp<float>::create( 1.0f);
const vector_t VASin<float>::q3 = VectorOp<float>::create( 0.0f);
const vector_t VASin<float>::q4 = VectorOp<float>::create( 0.0f);
const vector_t VASin<float>::q5 = VectorOp<float>::create( 0.0f);

// contant data arrays

template<> __forceinline vector_t VASin<double>::A(vector_t index)
{
    // a[2] = { 0, pi/4 }
    const unsigned __int64 a[2] = { 0ull, 0x3fe921fb54442d18ull };

    unsigned __int64 x0, x1;
    VectorOp<double>::unpack(index, x0, x1);
    return VectorOp<double>::create(a[x0], a[x1]);
}

template<> __forceinline vector_t VASin<double>::B(vector_t index)
{
    // b[2] = { pi/2, pi/4 }
    const unsigned __int64 b[2] = { 0x3ff921fb54442d18ull, 0x3fe921fb54442d18ull };

    unsigned __int64 x0, x1;
    VectorOp<double>::unpack(index, x0, x1);
    return VectorOp<double>::create(b[x0], b[x1]);
}

template<> __forceinline vector_t VASin<float>::A(vector_t index)
{
    // a[2] = { 0, pi/4 }
    const unsigned a[2] = { 0u, 0x3f490fdau };

    unsigned x0, x1, x2, x3;
    VectorOp<float>::unpack(index, x0, x1, x2, x3);
    return VectorOp<float>::create(a[x0], a[x1], a[x2], a[x3]);
}

template<> __forceinline vector_t VASin<float>::B(vector_t index)
{
    // b[2] = { pi/2, pi/4 }
    const unsigned b[2] = { 0x3fc90fdau, 0x3f490fdau };

    unsigned x0, x1, x2, x3;
    VectorOp<float>::unpack(index, x0, x1, x2, x3);
    return VectorOp<float>::create(b[x0], b[x1], b[x2], b[x3]);
}

extern "C"
{
    vector_t __vdecl_asinf4(vector_t x) { return VASin<float>::asin(x); }
    vector_t __vdecl_acosf4(vector_t x) { return VASin<float>::acos(x); }
}
