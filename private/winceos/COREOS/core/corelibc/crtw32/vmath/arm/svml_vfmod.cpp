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

// Vectorized fmod C library function.

// This is a serialized implementation.

#include <arm_neon.h>
#include <intrin.h>
#include <math.h>

extern "C"
{

// simple implementation of 4xsingle fmod
// we just dispatch serially to regular fmod
__n128 __vdecl_fmodf4(__n128 x, __n128 y)
{
   float p0 = vgetq_lane_f32(x, 0);
   float q0 = vgetq_lane_f32(y, 0);
   float r0 = fmodf(p0, q0);
   x = vsetq_lane_f32(r0, x, 0);

   float p1 = vgetq_lane_f32(x, 1);
   float q1 = vgetq_lane_f32(y, 1);
   float r1 = fmodf(p1, q1);
   x = vsetq_lane_f32(r1, x, 1);

   float p2 = vgetq_lane_f32(x, 2);
   float q2 = vgetq_lane_f32(y, 2);
   float r2 = fmodf(p2, q2);
   x = vsetq_lane_f32(r2, x, 2);

   float p3 = vgetq_lane_f32(x, 3);
   float q3 = vgetq_lane_f32(y, 3);
   float r3 = fmodf(p3, q3);
   x = vsetq_lane_f32(r3, x, 3);

   return x;
}

}
