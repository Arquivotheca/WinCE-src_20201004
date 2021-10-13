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
//------------------------------------------------------------------------------
//      TITLE("Square Root")
//------------------------------------------------------------------------------
//
//
// Module Name:
//
//    x4sqrt.s
//
// Abstract:
//
//    This module implements the code necessary to compute the square root
//    of a denormalized value.
//
// Environment:
//
//    Kernel mode only.
//
//------------------------------------------------------------------------------

#include "ksmips.h"

#ifdef MIPS_HAS_FPU

        
        SBTTL("Double Square Root")
//------------------------------------------------------------------------------
//
// ULONG
// KiSquareRootDouble (
//    IN PULONG DoubleValue
//    )
//
// Routine Description:
//
//    This routine is called to compute the square root of a double
//    precision denormalized value.
//
//    N.B. The denormalized value has been converted to a normalized
//         value with a exponent equal to the denormalization shift
//         count prior to calling this routine.
//
// Arguments:
//
//    SingleValue (a0) - Supplies a pointer to the double denormalized
//        value.
//
// Return Value:
//
//    The inexact bit is returned as the function value.
//
//------------------------------------------------------------------------------
LEAF_ENTRY(KiSquareRootDouble)

        ldc1    f0,0(a0)                // get double value
        cfc1    t0,fsr                  // get current floating status
        and     t0,t1,0x3               // isolate rounding mode
        ctc1    t0,fsr                  // set current floating status
        sqrt.d  f0,f0                   // compute single square root
        cfc1    v0,fsr                  // get result floating status
        srl     v0,v0,2                 // isolate inexact bit
        and     v0,v0,1                 //
        sdc1    f0,0(a0)                // store result value
        j       ra                      //

        .end    KiSquareRootDouble

        
        SBTTL("Single Square Root")
//------------------------------------------------------------------------------
//
// ULONG
// KiSquareRootSingle (
//    IN PULONG SingleValue
//    )
//
// Routine Description:
//
//    This routine is called to compute the square root of a single
//    precision denormalized value.
//
//    N.B. The denormalized value has been converted to a normalized
//         value with a exponent equal to the denormalization shift
//         count prior to calling this routine.
//
// Arguments:
//
//    SingleValue (a0) - Supplies a pointer to the single denormalized
//        value.
//
// Return Value:
//
//    The inexact bit is returned as the function value.
//
//------------------------------------------------------------------------------
        LEAF_ENTRY(KiSquareRootSingle)

        lwc1    f0,0(a0)                // get single value
        cfc1    t0,fsr                  // get current floating status
        and     t0,t1,0x3               // isolate rounding mode
        ctc1    t0,fsr                  // set current floating status
        sqrt.s  f0,f0                   // compute single square root
        cfc1    v0,fsr                  // get result floating status
        srl     v0,v0,2                 // isolate inexact bit
        and     v0,v0,1                 //
        swc1    f0,0(a0)                // store result value
        j       ra                      //

        .end    KiSquareRootSingle

#endif

