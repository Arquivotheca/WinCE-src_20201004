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

// Vectorized tan C library function.
// Vectorized cotan (where cotan(x) = 1/tan(x))

// Implementation derived from the work described in "Software Manual for the
// Elementary Functions" by William J Cody, Jr. and William Waite.

// The implementation is designed to be portable accross multiple vector
// architectures. See svml_vop.h.

#include <type_traits>
#include "svml_vop.h"

template <typename float_t>
class VTan
{
    static_assert(std::is_floating_point<float_t>::value, "float_t must be a floating point type");

    static const vector_t eps, eps1, ymax; // limits
    static const vector_t c1, c2; // portions of pi/2

    // polynomial coefficients
    static const vector_t p0, p1, p2, p3;
    static const vector_t q0, q1, q2, q3, q4;

public:
    static __forceinline  vector_t tan(vector_t x)
    {
        vector_t y = ABS(x);
        return tan_impl<true>(x, y);
    }

    static __forceinline  vector_t cotan(vector_t x)
    {
        vector_t y = ABS(x);
        vector_t result = tan_impl<false>(x, y);

        // check for edge cases

        result = CONDEXEC(CMPLT(y, eps1), OR(SIGNOF(x), INF), result); // y < eps1, return +/-inf

        return result;
    }

private:

    template<bool is_tan>
    static __forceinline vector_t tan_impl(vector_t x, vector_t y)
    {
        const vector_t two_over_pi = CREATE((float_t)0.63661977236758134308);

        const vector_t zero = CREATE((float_t)0.0);
        const vector_t one = CREATE((float_t)1.0);

        vector_t n = INTRND(MUL(x, two_over_pi));
        vector_t xn = CVT_INT2FP(n);

        // determine f 
        // f = x - xn*(pi/2), use extended precision
        vector_t f = SUB(SUB(x, MUL(xn,c1)), MUL(xn, c2));

        // polynomial approximations
        // xnum = f * P(g), g = f^2
        // xden = Q(g), g = f^2
        vector_t g = MUL(f, f);
        vector_t xnum, xden;
        if (sizeof(float_t) == sizeof(float))
        {
            // xnum = p1*g*f + f; // leave off p0, since p0=1.0
            // xden = (q2*g + q1)*g + q0;
            xnum = ADD(MUL(MUL(p1, g), f), f);
            xden = ADD(MUL(ADD(MUL(q2, g), q1), g), q0);
        }
        else
        {
            // xnum = ((p3*g + p2)*g + p1) * g * f + f; // leave off p0, since p0=1.0
            // xden = (((q4*g + q3)*g + q2)*g + q1)*g + q0;
            xnum = ADD(MUL(MUL(ADD(MUL(ADD(MUL(p3, g), p2), g), p1), g), f), f);
            xden = ADD(MUL(ADD(MUL(ADD(MUL(ADD(MUL(q4, g), q3), g), q2), g), q1), g), q0);
        }

        // if (abs(f) < eps) we don't need the approximation
        vector_t absf_lt_eps = ACMPLT(f, eps);
        xnum = CONDEXEC(absf_lt_eps, f, xnum);
        xden = CONDEXEC(absf_lt_eps, one, xden);

        //if (n%2!=0)
        //    xnum = -xnum;
        vector_t n_is_odd = IS_ODD(n);
        xnum = CONDEXEC(n_is_odd, NEG(xnum), xnum);

        vector_t result;
        if (is_tan)
        {
            result = CONDEXEC(n_is_odd, DIV(xden, xnum), DIV(xnum, xden));
        }
        else
        {
            result = CONDEXEC(n_is_odd, DIV(xnum, xden), DIV(xden, xnum));
        }

        // check for edge cases

#if 0
        //  Cody & Waite suggest returning an error for values larger than
        //    ymax as the result has no significant digits, however none of
        //    our CRT implementations do this so i'm disabling this code
        result = CONDEXEC(CMPGT(y, ymax), IND, result); // y > ymax, return NaN
#endif

        result = CONDEXEC(CMPEQ(y, y), result, y);      // y is NaN, return NaN
        result = CONDEXEC(CMPEQ(y, INF), IND, result);  // y is inf, return NaN

        return result;
    }
};

// limits
const vector_t VTan<double>::ymax = VectorOp<double>::create(105414357.0);  // 2^(53/2) * pi/2
const vector_t VTan<float>::ymax = VectorOp<float>::create(6433.0f);        // 2^(24/2) * pi/2

const vector_t VTan<double>::eps1 = VectorOp<double>::create(2.2250738585072014-308);  // min double
const vector_t VTan<float>::eps1 = VectorOp<float>::create(1.1754943508222875e-038f);  // min float

const vector_t VTan<double>::eps = VectorOp<double>::create(1.4901161193847656e-008);  // 2^(-53/2)
const vector_t VTan<float>::eps = VectorOp<float>::create(2.44140625e-004f);           // 2^(-24/2)

// c1 is an exactly representable value near pi/2
// c1 + c2 = pi/2
const vector_t VTan<double>::c1 = VectorOp<double>::create(0x3ff9220000000000ull);        // 3217/2048
const vector_t VTan<double>::c2 = VectorOp<double>::create(-4.454455103380768678308e-6);  // pi/2 - c1

const vector_t VTan<float>::c1 = VectorOp<float>::create(0x3fc90000u);                    // 201/128
const vector_t VTan<float>::c2 = VectorOp<float>::create(4.83826794897e-4f);              // pi/2 - c1

// polynomial coefficients
const vector_t VTan<double>::p0 = VectorOp<double>::create( 1.0);
const vector_t VTan<double>::p1 = VectorOp<double>::create(-0.13338350006421960681e+0);
const vector_t VTan<double>::p2 = VectorOp<double>::create( 0.34248878235890589960e-2);
const vector_t VTan<double>::p3 = VectorOp<double>::create(-0.17861707342254426711e-4);

const vector_t VTan<double>::q0 = VectorOp<double>::create( 1.0);
const vector_t VTan<double>::q1 = VectorOp<double>::create(-0.46671683339755294240e+0);
const vector_t VTan<double>::q2 = VectorOp<double>::create( 0.25663832289440112864e-1);
const vector_t VTan<double>::q3 = VectorOp<double>::create(-0.31181531907010027307e-3);
const vector_t VTan<double>::q4 = VectorOp<double>::create( 0.49819433993786512270e-6);

const vector_t VTan<float>::p0 = VectorOp<float>::create( 1.0f);
const vector_t VTan<float>::p1 = VectorOp<float>::create(-0.958017723e-1f);
const vector_t VTan<float>::p2 = VectorOp<float>::create( 0.0f);
const vector_t VTan<float>::p3 = VectorOp<float>::create( 0.0f);

const vector_t VTan<float>::q0 = VectorOp<float>::create( 1.0f);
const vector_t VTan<float>::q1 = VectorOp<float>::create(-0.429135777e+0f);
const vector_t VTan<float>::q2 = VectorOp<float>::create( 0.971685835e-2f);
const vector_t VTan<float>::q3 = VectorOp<float>::create( 0.0f);
const vector_t VTan<float>::q4 = VectorOp<float>::create( 0.0f);

extern "C"
{
    vector_t __vdecl_tanf4(vector_t x) { return VTan<float>::tan(x); }
    vector_t __vdecl_cotanf4(vector_t x) { return VTan<float>::cotan(x); }
}
