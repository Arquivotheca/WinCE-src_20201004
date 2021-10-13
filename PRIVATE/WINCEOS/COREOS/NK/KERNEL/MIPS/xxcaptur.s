//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//------------------------------------------------------------------------------
//      TITLE("Capture and Restore Context")
//------------------------------------------------------------------------------
//
//
// Module Name:
//
//    capture.s
//
// Abstract:
//
//    This module implements the code necessary to capture and restore
//    the context of the caller.
//
//------------------------------------------------------------------------------

#include "ksmips.h"

//
// Define local symbols.
//

#if defined(R3000)

#define UserPsr (1 << PSR_CU1) | (0xff << PSR_INTMASK) | (1 << PSR_IEC) | \
                (1 << PSR_KUC) | (1 << PSR_IEP) | (1 << PSR_KUP) | \
                (1 << PSR_IEO) | (1 << PSR_KUO) // constant user PSR value

#endif

#if defined(R4000)

#define UserPsr PSR_XX_C | (1 << PSR_CU1) | PSR_FR_C | (0xff << PSR_INTMASK) | \
                PSR_UX_C | (0x2 << PSR_KSU) | (1 << PSR_IE) // constant user PSR value

#endif

        SBTTL("Capture Context")
//------------------------------------------------------------------------------
//
// VOID
// RtlCaptureContext (
//    OUT PCONTEXT ContextRecord
//    )
//
// Routine Description:
//
//    This function captures the context of the caller in the specified
//    context record.
//
//    N.B. The context record is not guaranteed to be quadword aligned
//       and, therefore, no double floating store instructions can be
//       used.
//
// Arguments:
//
//    ContextRecord (a0) - Supplies the address of a context record.
//
// Return Value:
//
//    None.
//
//------------------------------------------------------------------------------
#ifdef  _MIPS64
#define S_REG   sd
#else   //  _MIPS64
#define S_REG   sw
#endif  //  _MIPS64

LEAF_ENTRY(RtlCaptureContext)
        .set    noreorder
        .set    noat

        //
        // Save integer registers zero, at - t9, s8, gp, sp, ra, lo, and hi.
        //
        S_REG   zero,CxIntZero(a0)      // store integer register zero
        S_REG   AT,CxIntAt(a0)          // store integer registers at - t9
        S_REG   v0,CxIntV0(a0)          //
        mflo    v0                      //
        S_REG      v1,CxIntV1(a0)          //
        mfhi    v1                      //
        S_REG      v0,CxIntLo(a0)          //
        S_REG      v1,CxIntHi(a0)          //
        S_REG      a0,CxIntA0(a0)          //
        S_REG      a1,CxIntA1(a0)          //
        S_REG      a2,CxIntA2(a0)          //
        S_REG      a3,CxIntA3(a0)          //
        S_REG      t0,CxIntT0(a0)          //
        S_REG      t1,CxIntT1(a0)          //
        S_REG      t2,CxIntT2(a0)          //
        S_REG      t3,CxIntT3(a0)          //
        S_REG      t4,CxIntT4(a0)          //
        S_REG      t5,CxIntT5(a0)          //
        S_REG      t6,CxIntT6(a0)          //
        S_REG      t7,CxIntT7(a0)          //
        S_REG      s0,CxIntS0(a0)          //
        S_REG      s1,CxIntS1(a0)          //
        S_REG      s2,CxIntS2(a0)          //
        S_REG      s3,CxIntS3(a0)          //
        S_REG      s4,CxIntS4(a0)          //
        S_REG      s5,CxIntS5(a0)          //
        S_REG      s6,CxIntS6(a0)          //
        S_REG      s7,CxIntS7(a0)          //
        S_REG      t8,CxIntT8(a0)          //
        S_REG      t9,CxIntT9(a0)          //
        S_REG      s8,CxIntS8(a0)          //
        S_REG      gp,CxIntGp(a0)          // save integer registers gp, sp, and ra
        S_REG      sp,CxIntSp(a0)          //
        S_REG      ra,CxIntRa(a0)          //

        //
        // Save control information and set context flags.
        //
        sw      v0,CxFsr(a0)            //
        sw      ra,CxFir(a0)            // set continuation address
        li      v0,UserPsr              // set constant user processor status
        bgez    sp,10f                  // if gez, called from user mode

        .set    noreorder
        .set    noat
        mfc0    v0,psr                  // get current processor status
        nop                             //
        .set    at
        .set    reorder


10:     sw      v0,CxPsr(a0)            // set processor status
        li      t0,CONTEXT_FULL         // set context control flags
        sw      t0,CxContextFlags(a0)   //
        j       ra                      // return

        .end    RtlCaptureContext

