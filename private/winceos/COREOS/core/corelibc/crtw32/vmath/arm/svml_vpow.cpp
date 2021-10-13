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

// Vectorized pow C library function.

// Implementation derived from the work described in "Software Manual for the
// Elementary Functions" by William J Cody, Jr. and William Waite.

// The implementation is designed to be portable accross multiple vector
// architectures. See svml_vop.h.

#include <type_traits>
#include "svml_vop.h"

template<typename float_t>
class VPow
{
    static_assert(std::is_floating_point<float_t>::value, "float_t must be a floating point type");

    static const vector_t bigx, smallx;
    static const vector_t K; // derived from log2(e)

    // integer powers of 2^(-1/16)
    // A1[j] = 2^((1-j)/16)  for (j=1->17)
    // A2[j] = 2^((1-2j)/16) - A1[2j]  for (j=1->8)
    static vector_t A1(vector_t index);
    static vector_t A2(vector_t index);
    
    // polynomial coefficients
    static const vector_t p1, p2, p3, p4;
    static const vector_t q1, q2, q3, q4, q5, q6, q7;
    
    static const vector_t lsb_mask;

public:
	// calculate x^y as 2^w, where w = y - log2(x)
    static __forceinline vector_t pow(vector_t x, vector_t y)
    {
        const vector_t zero = CREATE((float_t)0.0);
        const vector_t negzero = CREATE((float_t)-0.0);
        const vector_t one = CREATE((float_t)1.0);
        const vector_t negone = CREATE((float_t)-1.0);

        vector_t absx = ABS(x);
        vector_t result = pow_impl(absx, y);

        // corner cases

        // is y an (odd) integer
        vector_t iy = CVT_FP2INT(y);
        vector_t y_is_int = CMPEQ(CVT_INT2FP(iy),y);
        vector_t y_is_odd_int = AND(y_is_int, IS_ODD(iy));

        vector_t x_is_zero = CMPEQ(x, zero);
        vector_t y_gt_zero = CMPGT(y, zero);
        vector_t y_lt_zero = CMPLT(y, zero);
        vector_t x_is_zero_and_y_gt_zero = AND(x_is_zero, y_gt_zero);
        vector_t x_is_zero_and_y_lt_zero = AND(x_is_zero, y_lt_zero);

        result = CONDEXEC(AND(x_is_zero_and_y_gt_zero, y_is_odd_int), x, result);                   // if x == 0, y > zero, y is odd int, return +/-0.0
        result = CONDEXEC(AND(x_is_zero_and_y_gt_zero, NOT(y_is_odd_int)), zero, result);           // if x == 0, y > zero, return +0.0
        result = CONDEXEC(AND(x_is_zero_and_y_lt_zero, y_is_odd_int), OR(SIGNOF(x), INF), result);  // if x == 0, y < zero, y is odd int, return +/-inf
        result = CONDEXEC(AND(x_is_zero_and_y_lt_zero, NOT(y_is_odd_int)), INF, result);            // if x == 0, y < zero, return +inf

        vector_t y_is_inf = CMPEQ(y, INF);
        vector_t y_is_neginf = CMPEQ(y, NEGINF);
        vector_t absx_lt_one = CMPLT(absx, one);
        vector_t absx_gt_one = CMPGT(absx, one);

        result = CONDEXEC(AND(OR(y_is_inf, y_is_neginf), CMPEQ(x, negone)), one, result);           // y == inf, y == -inf, x == -1.0, return 1.0
        result = CONDEXEC(AND(y_is_neginf, absx_lt_one), INF, result);                              // y == -inf, abs(x) < 1, return +inf
        result = CONDEXEC(AND(y_is_neginf, absx_gt_one), zero, result);                             // y == -inf, abs(x) > 1, return +0.0
        result = CONDEXEC(AND(y_is_inf, absx_lt_one), zero, result);                                // y == inf, abx(x) < 1, return +0.0
        result = CONDEXEC(AND(y_is_inf, absx_gt_one), INF, result);                                 // y == inf, abs(x) > 1, return +inf

        vector_t x_is_inf = CMPEQ(x, INF);
        vector_t x_is_neginf = CMPEQ(x, NEGINF);
		vector_t x_is_neginf_and_y_lt_zero = AND(x_is_neginf, y_lt_zero);
		vector_t x_is_neginf_and_y_gt_zero = AND(x_is_neginf, y_gt_zero);

        result = CONDEXEC(AND(x_is_neginf_and_y_lt_zero, y_is_odd_int), negzero, result);           // x == -inf, y < 0, y is odd int, return -0.0
        result = CONDEXEC(AND(x_is_neginf_and_y_lt_zero, NOT(y_is_odd_int)), zero, result);         // x == -inf, y < 0, return +0.0
        result = CONDEXEC(AND(x_is_neginf_and_y_gt_zero, y_is_odd_int), NEGINF, result);            // x == -inf, y > 0, y is odd int, return -inf
        result = CONDEXEC(AND(x_is_neginf_and_y_gt_zero, NOT(y_is_odd_int)), INF, result);          // x == -inf, y > 0, return +inf

        result = CONDEXEC(AND(x_is_inf, y_lt_zero), zero, result);                                  // x == inf, y < 0, return +0.0
        result = CONDEXEC(AND(x_is_inf, y_gt_zero), INF, result);                                   // x == inf, return +inf

		vector_t x_lt_zero = CMPLT(x, zero);
		vector_t x_is_finite = NOT(CMPEQ(absx, INF));
		vector_t x_is_finite_and_x_lt_zero = AND(x_is_finite, x_lt_zero);

        result = CONDEXEC(AND(x_is_finite_and_x_lt_zero, NOT(y_is_int)), IND, result);              // x is finite, x < 0, y is not int, return NaN
        result = CONDEXEC(AND(x_is_finite_and_x_lt_zero, y_is_odd_int), NEG(result), result);       // x is finite, x < 0, y is odd int, return -result

#if 0
        // Disabled - our CRT implementation doesn't do this
        vector_t x_is_one = CMPEQ(x, one);
        vector_t y_is_zero = CMPEQ(y, zero);
        vector_t x_is_ordered = CMPEQ(x, x);
        vector_t y_is_ordered = CMPEQ(y, y);
        //result = CONDEXEC(OR(y_is_ordered, x_is_one), result, IND);                               // if y is NaN and x!=1.0 return NaN 
        //result = CONDEXEC(OR(x_is_ordered, y_is_zero), result, IND);                              // if x is NaN and y!=0.0 return NaN
#else
        // Our CRT emits NaN for all input NaNs
        result = CONDEXEC(CMPEQ(y, y), result, y);                                                  // if y is NaN, return NaN 
        result = CONDEXEC(CMPEQ(x, x), result, x);                                                  // if x is NaN, return NaN
#endif

		return result;
	}

private:

    // calculate x^y as 2^w, where w = y - log2(x)
	// assume x > 0
    static __forceinline vector_t pow_impl(vector_t x, vector_t y)
    {
        const vector_t zero = CREATE((float_t)0.0);
        const vector_t one_sixteenth = CREATE((float_t)0.0625);
        const vector_t sixteen = CREATE((float_t)16.0);
        const vector_t izero = CREATE(0u);
        const vector_t ione = CREATE(1u);

        // determine m (integer exponent)
        vector_t m = INTXP(x);

        // determine g (fixed point of x)
        vector_t g = SETXP(x, izero);

        // determine p (binary search)
        //int p = 1;
        //if (g <= A1(9))
        //    p = 9;
        //if (g <= A1(p+4))
        //    p += 4;
        //if (g <= A1(p+2))
        //    p += 2;
        vector_t p = ione;
        vector_t i = CREATE(9u);
        p = CONDEXEC(CMPLE(g, A1(i)), i, p);
        i = IADD(p, CREATE(4u));
        p = CONDEXEC(CMPLE(g, A1(i)), i, p);
        i = IADD(p, CREATE(2u));
        p = CONDEXEC(CMPLE(g, A1(i)), i, p);

        // determine z
        // z = 2s = 2[(g-a)/(g+a)]
        // float_t z = ((g-A1(p+1)) - A2((p+1)/2)) / (g + A1(p+1));
        // z = z + z;
        i = IADD(p, ione);
        vector_t z = DIV(SUB(SUB(g, A1(i)), A2(SHR(i,1))), ADD(g, A1(i)));
        z = ADD(z, z);

        // determine R(z)
        // v = z^2
        // R(z) = z * v * P(v)
        vector_t R;
        vector_t v = MUL(z, z);
        if (sizeof(float_t) == sizeof(float))
        {
            // R = p1 * v * z;
            R = MUL(MUL(p1, v), z);
        }
        else
        {
            // R = (((p4 * v + p3) * v + p2) * v + p1) * v * z;
            R = MUL(MUL(ADD(MUL(ADD(MUL(ADD(MUL(p4, v), p3), v), p2), v), p1), v), z);
        }

        //R = R + K*R;
        R = ADD(R, MUL(K, R));

        // determine U1, U2
        // U2 = (z + R) * log2(e)
        // U1 = FLOAT((k*m-r)*16-p) * 0.0625, k=1, r=0
        // float_t u2 = (R + z * K) + z;
        // float_t u1 = ((float_t)(m*16-p)) * one_sixteenth;
        vector_t u2 = ADD(ADD(R, MUL(z, K)), z);
        vector_t u1 = MUL(CVT_INT2FP(ISUB(SHL(m, 4), p)), one_sixteenth);

        // determine w and related
        // float_t y1 = REDUCE(y);
        // float_t y2 = y-y1;
        vector_t y1 = REDUCE(y);
        vector_t y2 = SUB(y, y1);

        //float_t w = u2*y + u1*y2;
        //float_t w1 = REDUCE(w);
        //float_t w2 = w-w1;
        vector_t w = ADD(MUL(u2, y), MUL(u1, y2));
        vector_t w1 = REDUCE(w);
        vector_t w2 = SUB(w, w1);
        
        //w = w1 + u1*y1;
        //w1 = REDUCE(w);
        //w2 = w2 + (w-w1);
        w = ADD(w1, MUL(u1, y1));
        w1 = REDUCE(w);
        w2 = ADD(w2, SUB(w, w1));

        //w = REDUCE(w2);
        //int iw1 = (int)(((float_t)16.0)*(w1+w));
        //w2 = w2-w;
        w = REDUCE(w2);
        vector_t iw1 = CVT_FP2INT(MUL(sixteen, ADD(w1, w)));
        w2 = SUB(w2, w);

        //if (w2 > 0) {
        //    w2 = w2 - one_sixteenth;
        //    ++iw1;
        //}
        vector_t w2_gt_zero = CMPGT(w2, zero);
        w2 = CONDEXEC(w2_gt_zero, SUB(w2, one_sixteenth), w2);
        iw1 = CONDEXEC(w2_gt_zero, IADD(iw1, ione), iw1);

        // determine p', m', r' (r=r'=0)
        //int i = (iw1 < 0) ? 0 : 1;
        //int m_ = iw1/16 + i;
        //int p_ = 16*m_ - iw1;
        i = CONDEXEC(ICMPLT(iw1, izero), izero, ione);
        vector_t m_ = IADD(IDIV_BY16(iw1), i);
        vector_t p_ = ISUB(SHL(m_, 4), iw1);

        // determine Z
        // Z = (2^w2)
        // Q(w2) = (2^w2)-1 for -0.0625 <= w2 <= 0
        // Z = w2 * Q(w2)
        vector_t Z;
        if (sizeof(float_t) == sizeof(float))
        {
            // q3*w2^3 + q2*w2^2 + q1*w2
            // Z = ((q3 * w2 + q2) * w2 + q1) * w2;
            Z = MUL(ADD(MUL(ADD(MUL(q3, w2), q2), w2), q1), w2);
        }
        else
        {
            // Z = ((((((q7 * w2 + q6) * w2 + q5) * w2 + q4) * w2 + q3) * w2 + q2) * w2 + q1) * w2;
            Z = MUL(ADD(MUL(ADD(MUL(ADD(MUL(ADD(MUL(ADD(MUL(ADD(MUL(q7, w2), q6), w2), q5), w2), q4), w2), q3), w2), q2), w2), q1), w2);
        }

        // result = Z*2^m * 2^(-p'/16)
        // Z = A1(p_+1) + A1(p_+1)*Z; // 2^(-p/16)
        vector_t a1 = A1(IADD(p_, CREATE(1u)));
        Z = ADD(a1, MUL(a1, Z));

        vector_t result = ADX(Z, m_);

        // make sure 2^w is representable 
        result = CONDEXEC(CMPGT(w, bigx), INF, result);                                     // if w > bigx, overflow: result = +inf
        result = CONDEXEC(CMPLT(w, smallx), zero, result );                                 // if w < small, underflow: result = +0.0

        return result;
    }
};

// values in octal 2^(-n/16)
//  0   1.000000000000000000000000000000 
//  1   0.752225750522220663026173472062
//  2   0.725403067175644416227320132727
//  3   0.701463367302522470210406261124
//  4   0.656423746255323532572071515057
//  5   0.634222140521760440161742153016
//  6   0.612634520425240666556476125503
//  7   0.572042434765401415537250402117
//  8   0.552023631477473631102131373047
//  9   0.532540767244124122543111401243
// 10   0.513773265233052116165034572776
// 11   0.475724623011064104326640442174
// 12   0.460337602430667052267506532214
// 13   0.443417233472542160633017655544
// 14   0.427127017076521365563373710612
// 15   0.413253033174611036612305622556
// 16   0.400000000000000000000000000000

#define ASFLOAT(i) (FLOAT(i).asFloat)
#define ASDOUBLE(i) (DOUBLE(i).asDouble)

// gather-style load
// convert 1-based array to 0-based array
template<> __forceinline vector_t VPow<double>::A1(vector_t index)
{
    // 2^((1-n)/16) for 1 to 17 @ machine precision
    const unsigned __int64 a1[] = {
        0x3ff0000000000000ull,
        0x3feea4afa2a490d9ull,
        0x3fed5818dcfba487ull,
        0x3fec199bdd85529cull,
        0x3feae89f995ad3adull,
        0x3fe9c49182a3f090ull,
        0x3fe8ace5422aa0dbull,
        0x3fe7a11473eb0186ull,
        0x3fe6a09e667f3bccull,
        0x3fe5ab07dd485429ull,
        0x3fe4bfdad5362a27ull,
        0x3fe3dea64c123422ull,
        0x3fe306fe0a31b715ull,
        0x3fe2387a6e756238ull,
        0x3fe172b83c7d517aull,
        0x3fe0b5586cf9890full,
        0x3fe0000000000000ull
    };

    unsigned __int64 x0, x1;
    VectorOp<double>::unpack(index, x0, x1);
    return VectorOp<double>::create(a1[x0-1], a1[x1-1]);
}
template<> __forceinline vector_t VPow<float>::A1(vector_t index)
{
    // 2^((1-j)/16) for j 1 to 17 @ machine precision
    const unsigned a1[] = {
       0x3f800000u,
       0x3f75257du,
       0x3f6ac0c6u,
       0x3f60ccdeu,
       0x3f5744fcu,
       0x3f4e248cu,
       0x3f45672au,
       0x3f3d08a3u,
       0x3f3504f3u,
       0x3f2d583eu,
       0x3f25fed6u,
       0x3f1ef532u,
       0x3f1837f0u,
       0x3f11c3d3u,
       0x3f0b95c1u,
       0x3f05aac3u,
       0x3f000000u
    };

    unsigned x0, x1, x2, x3;
    VectorOp<float>::unpack(index, x0, x1, x2, x3);
    return VectorOp<float>::create(a1[x0-1], a1[x1-1], a1[x2-1], a1[x3-1]);
}

template<> __forceinline vector_t VPow<double>::A2(vector_t index)
{
    // 2^((1-2j)/16) - A1[2j] for j 1 to 8
    const unsigned __int64 a2[] = {
        // 0ull,
        0x3c90b1ee74320000ull,
        // 0x3c72ed02d75c0000ull,
        0x3c71106589500000ull,
        // 0x3c87a1cd345e0000ull,
        0x3c6c7c46b0700000ull,
        // 0x3c86e9f156860000ull,
        0x3c9afaa2044f0000ull,
        // 0x3c921165f6270000ull,
        0x3c86324c05460000ull,
        // 0x3c6d4397aff00000ull,
        0x3c7ada0911f00000ull,
        // 0x3c76f46ad2300000ull,
        0x3c89b07eb6c80000ull,
        // 0x3c9b9bef918a0000ull,
        0x3c88a62e4adc0000ull,
        // 0ull
    };

    unsigned __int64 x0, x1;
    VectorOp<double>::unpack(index, x0, x1);
    return  VectorOp<double>::create(a2[x0-1], a2[x1-1]);
}

template<> __forceinline vector_t VPow<float>::A2(vector_t index)
{
    // 2^((1-2j)/16) - A1[2j] for j 1 to 8
    const unsigned a2[] = {
        // 0u,
        0x31a92436u,
        // 0x3367dd24u,
        0x336c2a94u,
        // 0x334ad69du,
        0x31a8fc24u,
        // 0x318aa836u,
        0x331f580cu,
        // 0x324fe779u,
        0x336a42a1u,
        // 0x3329b151u,
        0x32c12342u,
        // 0x32a31b71u,
        0x32e75623u,
        // 0x3363ea8bu,
        0x32cf9890u,
        // 0u
     };

    unsigned x0, x1, x2, x3;
    VectorOp<float>::unpack(index, x0, x1, x2, x3);
    return VectorOp<float>::create(a2[x0-1], a2[x1-1], a2[x2-1], a2[x3-1]);
}

// constants
const vector_t VPow<double>::K = VectorOp<double>::create(0.44269504088896340736);
const vector_t VPow<float>::K = VectorOp<float>::create(0.44269504088896340736f);

const vector_t VPow<double>::bigx = VectorOp<double>::create(11355.523406294144);
const vector_t VPow<float>::bigx = VectorOp<float>::create(1418.5654296875f);

const vector_t VPow<double>::smallx = VectorOp<double>::create(-11333.342696516225);
const vector_t VPow<float>::smallx = VectorOp<float>::create(-1396.384765625f);

const vector_t VPow<double>::lsb_mask = VectorOp<double>::create(0x0000000000000001ull);
const vector_t VPow<float>::lsb_mask = VectorOp<float>::create(0x00000001u);

// polynomial coeffecients
const vector_t VPow<double>::p1 = VectorOp<double>::create(0.83333333333333211405e-1);
const vector_t VPow<double>::p2 = VectorOp<double>::create(0.12500000000503799174e-1);
const vector_t VPow<double>::p3 = VectorOp<double>::create(0.22321421285924258967e-2);
const vector_t VPow<double>::p4 = VectorOp<double>::create(0.43445775672163119635e-3);

const vector_t VPow<float>::p1 = VectorOp<float>::create(0.83357541e-1f);
const vector_t VPow<float>::p2 = VectorOp<float>::create(0.0f);
const vector_t VPow<float>::p3 = VectorOp<float>::create(0.0f);
const vector_t VPow<float>::p4 = VectorOp<float>::create(0.0f);

// polynomial coeffecients
const vector_t VPow<double>::q1 = VectorOp<double>::create(0.69314718055994529629e+0);
const vector_t VPow<double>::q2 = VectorOp<double>::create(0.24022650695909537056e+0);
const vector_t VPow<double>::q3 = VectorOp<double>::create(0.55504108664085595326e-1);
const vector_t VPow<double>::q4 = VectorOp<double>::create(0.96181290595172416964e-2);
const vector_t VPow<double>::q5 = VectorOp<double>::create(0.13333541313585784703e-2);
const vector_t VPow<double>::q6 = VectorOp<double>::create(0.15400290440989764601e-3);
const vector_t VPow<double>::q7 = VectorOp<double>::create(0.14928852680595608186e-4);

const vector_t VPow<float>::q1 = VectorOp<float>::create(0.69314675e+0f);
const vector_t VPow<float>::q2 = VectorOp<float>::create(0.24018510e+0f);
const vector_t VPow<float>::q3 = VectorOp<float>::create(0.54360383e-1f);
const vector_t VPow<float>::q4 = VectorOp<float>::create(0.0f);
const vector_t VPow<float>::q5 = VectorOp<float>::create(0.0f);
const vector_t VPow<float>::q6 = VectorOp<float>::create(0.0f);
const vector_t VPow<float>::q7 = VectorOp<float>::create(0.0f);

extern "C"
{
    vector_t __vdecl_powf4(vector_t x, vector_t y) { return VPow<float>::pow(x, y); }
}
