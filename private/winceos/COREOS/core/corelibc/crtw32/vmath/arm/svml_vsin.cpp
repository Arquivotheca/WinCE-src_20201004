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

// Vectorized sin and cos C library functions.

// Implementation derived from the work described in "Software Manual for the
// Elementary Functions" by William J Cody, Jr. and William Waite.

// The implementation is designed to be portable accross multiple vector
// architectures. See svml_vop.h.

#include <type_traits>
#include "svml_vop.h"

template <typename float_t>
class VSin
{
    static_assert(std::is_floating_point<float_t>::value, "float_t must be a floating point type");

private:
    static const vector_t ymax;        // pi * 2 ^ (t/2) where t is the bits in the mantissa
    static const vector_t eps;         // small value near zero such that sin(f) == f for |f| < eps
    static const vector_t c1;          // exact portion of PI
    static const vector_t c2;          // remainder of PI such that c1+c2 exactly equals pi to at least machine precision (2^(-t/2) where t is the number of bits in the mantissa)
    
    static const vector_t r1, r2, r3, r4, r5, r6, r7, r8; // polynomial coeffecients
    
    static const vector_t sign_mask;
    static const vector_t one;
    static const vector_t half;
public:

    static vector_t __forceinline sin(vector_t x)
    {
        vector_t sign = AND(x, sign_mask); // each element is now 0.0 or -0.0
        sign = OR(sign, one);            // each element is now 1.0 or -1.0

        vector_t y = ABS(x);

        return sincos(x, y, sign);
    }

    static vector_t __forceinline cos(vector_t x)
    {
        const vector_t pi_over_two = CREATE((float_t)1.57079632679489661923);

        vector_t y = ABS(x); 
        y = ADD(y, pi_over_two); // cos(x) == sin(x+pi/2)
        
        return sincos(x, y, one /* sign doesn't matter for cos since cos(x) == cos(-x) */);
    }

private:

    static __forceinline vector_t sincos(vector_t x, vector_t y, vector_t sign)
    {
        const vector_t pi = CREATE((float_t)3.14159265358979323846);
        const vector_t one_over_pi = CREATE((float_t)0.31830988618379067154);

       // float_t xn = _frnd(y * #);
       // int n = (int)(xn);
        vector_t n = CVT_FP2INT(ADD(MUL(y, one_over_pi), half)); // y > 0, so floor(x+.5) is nearest(x)
        vector_t xn = CVT_INT2FP(n);
        vector_t absx = ABS(x);

        // if (n % 2 != 0)
        //     sign = -sign;
        sign = CONDEXEC(IS_ODD(n), NEG(sign), sign);

        // if (abs(x)!=y)
        //     xn -= (float_t)0.5; 
        xn = CONDEXEC(CMPEQ(absx, y), xn, SUB(xn, half));

        // do this at higher precision
        // f = abs(x) - xn * pi;
        // which is this at "pseudo-higher" prescision
        // f = (abs(x) - xn * c1) - xn * c2;
        vector_t f = SUB(SUB(absx, MUL(xn, c1)), MUL(xn, c2));

        // do R(g) -- polynomial approximation
        vector_t R;
        vector_t g = MUL(f, f);
        if (sizeof(float_t) == sizeof(double)) {
            // R = (((((((r8 * g + r7) * g + r6) * g + r5) * g + r4) * g + r3) * g + r2) * g + r1) * g;
            R = MUL(ADD(MUL(ADD(MUL(ADD(MUL(ADD(MUL(ADD(MUL(ADD(MUL(ADD(MUL(r8, g), r7), g), r6), g), r5), g), r4), g), r3), g), r2), g), r1), g);
        } else {
            // R = (((r4 * g + r3) * g + r2) * g + r1) * g;
            R = MUL(ADD(MUL(ADD(MUL(ADD(MUL(r4, g), r3), g), r2), g), r1), g);
        }

        // result = f + f*R
        vector_t result = ADD(f, MUL(f,R));

        //if (abs(f) < eps)
        //{
        //    result = f;
        //}
        result = CONDEXEC(ACMPLT(f, eps), f, result);

        // result *= sign;
        result = MUL(result, sign);

        // check for error conditions

#if 0
        //  Cody & Waite suggest returning an error for values larger than
        //    ymax as the result has no significant digits, however none of
        //    our CRT implementations do this so i'm disabling this code
        result = CONDEXEC(CMPGT(y, ymax), IND, result);    // if y > ymax, result=NaN (not enough precision)
#endif

        result = CONDEXEC(CMPEQ(y, y), result, y);            // if y is NaN, result=NaN  (y!=y ==> y==NaN)
        result = CONDEXEC(CMPEQ(y, INF), IND, result);        // if y is Inf, result=NaN

        return result;
    }
};


// constants

vector_t const VSin<double>::ymax = VectorOp<double>::create(298156826.86479);
vector_t const VSin<float>::ymax = VectorOp<float>::create(12867.9635f);

vector_t const VSin<double>::eps = VectorOp<double>::create(1.05367121277235079e-8);
vector_t const VSin<float>::eps = VectorOp<float>::create(0.000244140625f);

vector_t const VSin<double>::c1 = VectorOp<double>::create(0x4009220000000000ull);         // 3217/1024
vector_t const VSin<double>::c2 = VectorOp<double>::create(-8.908910206761537356617e-6);   // pi - C1
vector_t const VSin<float>::c1 = VectorOp<float>::create(0x40491000u);                     // 3217/1024
vector_t const VSin<float>::c2 = VectorOp<float>::create(-8.90891020676e-6f);              // pi - C1
//  these are the constants recomended by Cody & Waite for single precision, but they
//    don't provide enough accuracy, so i'm using the double precision constants instead.
// vector_t const VSin<float>::c1 = VectorOp<float>::create(0x40490000u);                     // 201/64
// vector_t const VSin<float>::c2 = VectorOp<float>::create(9.67653589793e-4f);               // pi - C1

// polynomial coefficients
vector_t const VSin<double>::r1 = VectorOp<double>::create(-0.16666666666666665052e+0);
vector_t const VSin<double>::r2 = VectorOp<double>::create( 0.83333333333331650314e-2);
vector_t const VSin<double>::r3 = VectorOp<double>::create(-0.19841269841201840457e-3);
vector_t const VSin<double>::r4 = VectorOp<double>::create( 0.27557319210152756119e-5);
vector_t const VSin<double>::r5 = VectorOp<double>::create(-0.25052106798274584544e-7);
vector_t const VSin<double>::r6 = VectorOp<double>::create( 0.16058936490371589114e-9);
vector_t const VSin<double>::r7 = VectorOp<double>::create(-0.76429178068910467734e-12);
vector_t const VSin<double>::r8 = VectorOp<double>::create( 0.27204790957888846175e-14);

vector_t const VSin<float>::r1 = VectorOp<float>::create(-0.1666665668e+0f);
vector_t const VSin<float>::r2 = VectorOp<float>::create( 0.8333025139e-2f);
vector_t const VSin<float>::r3 = VectorOp<float>::create(-0.1980741872e-3f);
vector_t const VSin<float>::r4 = VectorOp<float>::create( 0.2601903036e-5f);
vector_t const VSin<float>::r5 = VectorOp<float>::create(0.0f);
vector_t const VSin<float>::r6 = VectorOp<float>::create(0.0f);
vector_t const VSin<float>::r7 = VectorOp<float>::create(0.0f);
vector_t const VSin<float>::r8 = VectorOp<float>::create(0.0f);

vector_t const VSin<double>::sign_mask = VectorOp<double>::create(0x800000000000000ull);
vector_t const VSin<float>::sign_mask = VectorOp<float>::create(0x80000000u);

vector_t const VSin<double>::one = VectorOp<double>::create(1.0);
vector_t const VSin<float>::one = VectorOp<float>::create(1.0f);

vector_t const VSin<double>::half = VectorOp<double>::create(0.5);
vector_t const VSin<float>::half = VectorOp<float>::create(0.5f);

extern "C"
{
    vector_t __vdecl_sinf4(vector_t x) { return VSin<float>::sin(x); }
    vector_t __vdecl_cosf4(vector_t x) { return VSin<float>::cos(x); }
}
