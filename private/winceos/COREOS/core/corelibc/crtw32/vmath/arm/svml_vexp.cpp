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

// Vectorized exp C library function.

// Implementation derived from the work described in "Software Manual for the
// Elementary Functions" by William J Cody, Jr. and William Waite.

// The implementation is designed to be portable accross multiple vector
// architectures. See svml_vop.h.

#include <type_traits>
#include "svml_vop.h"

template<typename float_t>
class VExp
{
    static_assert(std::is_floating_point<float_t>::value, "float_t must be a floating point type");

    // constants
    static const vector_t ln2, inv_ln2;
    static const vector_t bigx, smallx, eps;
    static const vector_t C1, C2;

    // ploynomial coefficients
    static const vector_t p0, p1, p2;
    static const vector_t q0, q1, q2;
    
public:
    static __forceinline vector_t exp(vector_t x)
    {
        const vector_t zero = CREATE((float_t)0.0);
        const vector_t one = CREATE((float_t)1.0);

        vector_t n = INTRND(MUL(x, inv_ln2));
        vector_t xn = CVT_INT2FP(n);

        // g = (x - xn*C1) - xn*C2;
        vector_t g = SUB(SUB(x, MUL(xn,C1)), MUL(xn, C2));

        // compute approximation R(g)
        // R(g) = .5 + g*P(z)/(Q(z)-g*P(z))
        //   where z = g^2

        vector_t z = MUL(g , g);
        
        vector_t g_P_z, Q_z;
        if (sizeof(float_t)==sizeof(float))
        {
            // g * P(z) = g * (p1*z + p0)
            // Q(z) = z*q1 + q0
            g_P_z = MUL(ADD(MUL(z, p1), p0), g);
            Q_z = ADD(MUL(z, q1), q0);
        }
        else
        {
            // g * P(z) = g * (p2*z^2 + p1*z + p0)
            // Q(z) = q2*z^2 + q1*z + q0
            g_P_z = MUL(ADD(MUL(ADD(MUL(p2, z), p1), z), p0), g);
            Q_z =  ADD(MUL(ADD(MUL(q2, z), q1), z), q0);
        }

        vector_t R = ADD(CREATE((float_t)0.5), DIV(g_P_z, SUB(Q_z, g_P_z)));

        n = IADD(n, CREATE(1u)); // scaling
        
        // return ADX(R, n);
        vector_t result = ADX(R, n);

        // check for edge cases

        result = CONDEXEC(CMPGT(x, bigx), INF, result);       // if x > bigx, result = +inf (overflow)
        result = CONDEXEC(CMPLT(x, smallx), zero, result);    // if x < smallx, result = +0.0 (underflow)
        result = CONDEXEC(ACMPLT(x, eps), one, result);       // if -eps < x < eps, result = +1.0

        result = CONDEXEC(CMPEQ(x, x), result, x);            // if x is NaN, result = NaN

        return result;
    }
};

const vector_t VExp<double>::ln2 = VectorOp<double>::create(0.6931471805599453094172321);
const vector_t VExp<float>::ln2 = VectorOp<float>::create(0.6931471805599453094172321f);

const vector_t VExp<double>::inv_ln2 = VectorOp<double>::create(1.4426950408889634074);
const vector_t VExp<float>::inv_ln2 = VectorOp<float>::create(1.4426950408889634074f);

const vector_t VExp<double>::bigx = VectorOp<double>::create(709.78271289338397);
const vector_t VExp<float>::bigx = VectorOp<float>::create(88.72283935546875f);

const vector_t VExp<double>::smallx = VectorOp<double>::create(-708.39641853226408);
const vector_t VExp<float>::smallx = VectorOp<float>::create(-87.3365478515625f);

// 2^-n / 2 where n is the binary digits in significand
const vector_t VExp<double>::eps = VectorOp<double>::create(0.55511151231257827e-16);
const vector_t VExp<float>::eps = VectorOp<float>::create(0.29802322387695313e-7f);

// exactly 355/512 = CREATE(0.543 (octal) = .b18 (hex)
const vector_t VExp<double>::C1 = VectorOp<double>::create(0x3fe6300000000000ull);
const vector_t VExp<float>::C1 = VectorOp<float>::create(0x3f318000u);

const vector_t VExp<double>::C2 = VectorOp<double>::create(-2.121944400546905827679e-4);
const vector_t VExp<float>::C2 = VectorOp<float>::create(-2.121944400546905827679e-4f);

// ploynomial coeffecients
const vector_t VExp<double>::p0 = VectorOp<double>::create(0.249999999399999993e+0);
const vector_t VExp<double>::p1 = VectorOp<double>::create(0.694360001511792852e-2);
const vector_t VExp<double>::p2 = VectorOp<double>::create(0.165203300268279130e-4);
const vector_t VExp<double>::q0 = VectorOp<double>::create(0.500000000000000000e+0);
const vector_t VExp<double>::q1 = VectorOp<double>::create(0.555538666969001188e-1);
const vector_t VExp<double>::q2 = VectorOp<double>::create(0.495862884905441294e-3);
const vector_t VExp<float>::p0 = VectorOp<float>::create(0.24999999950e+0f);
const vector_t VExp<float>::p1 = VectorOp<float>::create(0.41602886268e-2f);
const vector_t VExp<float>::p2 = VectorOp<float>::create(0.0f); // unused
const vector_t VExp<float>::q0 = VectorOp<float>::create(0.50000000000e+0f);
const vector_t VExp<float>::q1 = VectorOp<float>::create(0.49987178778e-1f);
const vector_t VExp<float>::q2 = VectorOp<float>::create(0.0f); // unused

extern "C"
{
    vector_t __vdecl_expf4(vector_t x) { return VExp<float>::exp(x); }
}
