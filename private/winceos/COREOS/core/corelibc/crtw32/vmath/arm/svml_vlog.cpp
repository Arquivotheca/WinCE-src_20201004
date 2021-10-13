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

// Vectorized log and log10 C library functions.

// Implementation derived from the work described in "Software Manual for the
// Elementary Functions" by William J Cody, Jr. and William Waite.

// The implementation is designed to be portable accross multiple vector
// architectures. See svml_vop.h.

#include <type_traits>
#include "svml_vop.h"

template<typename float_t>
class VLog
{
    static_assert(std::is_floating_point<float_t>::value, "float_t must be a floating point type");

    // constants
    static const vector_t C0, C1, C2, C3;
    // ploynomial coefficients
    static const vector_t a0, a1, a2;
    static const vector_t b0, b1, b2, b3;

public:
    static __forceinline vector_t log(vector_t x)
    {
        vector_t result = log_impl(x);
        result = log_check_special_case(x, result);
        return result;
    }

    static __forceinline vector_t log10(vector_t x)
    {
        vector_t result = log_impl(x);

        result = MUL(result, C3);
        result = log_check_special_case(x, result);

        return result;
    }

private:

    static __forceinline vector_t log_impl(vector_t x)
    {
        vector_t half = CREATE((float_t)0.5);

        // determine f, n
        vector_t n = INTXP(x);
        vector_t f = SETXP(x, CREATE(0u));

        // determine z and w (aka z^2)
        vector_t znum, zden;
        //if (f > C0)
        //{
        //    --n;
        //    znum = SUB(f, half);
        //    zden = ADD(MUL(znum, half), half);
        //}
        //else
        //{
        //    znum = SUB(SUB(f, half), half);
        //    zden = ADD(MUL(f, half), half);
        //}
        vector_t f_gt_c0 = CMPGT(f, C0);
        n = CONDEXEC(f_gt_c0, n, ISUB(n, CREATE(1u)));
        znum = CONDEXEC(f_gt_c0, (SUB(SUB(f, half), half)), (SUB(f, half)));
        zden = CONDEXEC(f_gt_c0, (ADD(MUL(f, half), half)), (ADD(MUL(znum, half), half)));

        vector_t z = DIV(znum, zden);
        vector_t w = MUL(z, z);

        // evaulate polynomial approximation R(z)
        // R(z) = z + z * R(z^2)
        // R(z^2) = R(w) = w * A(w) / B(w)
        vector_t a, b;
        if (sizeof(float_t) == sizeof(float))
        {
            // A(w) = a0
            // B(w) = b1*w + b0 (b1==1.0)
            a = a0;
            b = ADD(w, b0);
        }
        else
        {
            // A(w) = a0 + a1*w + a2*w^2
            // B(w) = b0 + b1*w + b2*w^2 + b3*w^3 (b3==1.0)
            a = ADD(MUL(ADD(MUL(a2, w), a1), w), a0);
            b = ADD(MUL(ADD(MUL(ADD(w, b2), w), b1), w), b0);
        }
        vector_t r_w = DIV(MUL(w, a), b);
        vector_t r_z = ADD(z, MUL(z, r_w));

        vector_t xn = CVT_INT2FP(n);
        vector_t result = ADD(ADD(MUL(xn, C2), r_z), MUL(xn, C1));

        return result;
    }
    
    // check for corner cases, run after log10's multipy to ensure that IND remains intact
    static __forceinline vector_t log_check_special_case(vector_t x, vector_t result)
    {
        vector_t zero = CREATE((float_t)0.0);
        vector_t one = CREATE((float_t)1.0);

        result = CONDEXEC(CMPEQ(x, one), zero, result);    // if x is 1.0, return 0.0
        result = CONDEXEC(CMPEQ(x, zero), NEGINF, result); // if x is +/-0.0, return -inf
        result = CONDEXEC(CMPLT(x, zero), IND, result);    // if x < 0, return NaN
        result = CONDEXEC(CMPEQ(x, INF), x, result);       // if x is +inf, return NaN
        result = CONDEXEC(CMPEQ(x, x), result, x);         // if x is NaN, return NaN

        return result;
    }
};

const vector_t VLog<double>::C0 = VectorOp<double>::create(0.70710678118654752440);  // sqrt(.5)
const vector_t VLog<float>::C0 = VectorOp<float>::create(0.70710678118654752440f);

// exactly 355/512 = 0.543 (octal) = .b18 (hex)
const vector_t VLog<double>::C1 = VectorOp<double>::create(0x3fe6300000000000ull);
const vector_t VLog<float>::C1 = VectorOp<float>::create(0x3f318000u);

const vector_t VLog<double>::C2 = VectorOp<double>::create(-2.121944400546905827679e-4);
const vector_t VLog<float>::C2 = VectorOp<float>::create(-2.121944400546905827679e-4f);

// log(e)
const vector_t VLog<double>::C3 = VectorOp<double>::create(0x3fdbcb7b1526e50eull);
const vector_t VLog<float>::C3 = VectorOp<float>::create(0x3ede5bd8u);

// polynomial coefficients
const vector_t VLog<double>::a0 = VectorOp<double>::create(-0.64124943423745581147e+2);
const vector_t VLog<double>::a1 = VectorOp<double>::create( 0.16383943563021534222e+2);
const vector_t VLog<double>::a2 = VectorOp<double>::create(-0.78956112887491257267e+0);
const vector_t VLog<double>::b0 = VectorOp<double>::create(-0.76949932108494879777e+3);
const vector_t VLog<double>::b1 = VectorOp<double>::create( 0.31203222091924532844e+3);
const vector_t VLog<double>::b2 = VectorOp<double>::create(-0.35667977739034646171e+2);
const vector_t VLog<double>::b3 = VectorOp<double>::create( 1.0);

const vector_t VLog<float>::a0 = VectorOp<float>::create(-0.5527074855e+0f);
const vector_t VLog<float>::a1 = VectorOp<float>::create( 0.0f);
const vector_t VLog<float>::a2 = VectorOp<float>::create( 0.0f);
const vector_t VLog<float>::b0 = VectorOp<float>::create(-0.6632718214e+1f);
const vector_t VLog<float>::b1 = VectorOp<float>::create( 1.0f);
const vector_t VLog<float>::b2 = VectorOp<float>::create( 0.0f);
const vector_t VLog<float>::b3 = VectorOp<float>::create( 0.0f);

extern "C"
{
    vector_t __vdecl_logf4(vector_t x) { return VLog<float>::log(x); }
    vector_t __vdecl_log10f4(vector_t x) { return VLog<float>::log10(x); }
}
