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
//      TITLE("Kernel Initialization")
//------------------------------------------------------------------------------
//
//
// Module Name:
//
//    startup.s
//
// Abstract:
//
//    This module implements the code necessary to initialize the Kernel to
// run on an R4000 series processor.
//
//
// Environment:
//
//    Kernel mode only.
//
//------------------------------------------------------------------------------

#include "ksmips.h"
#include "nkintr.h"
#include "mipskpg.h"

//------------------------------------------------------------------------------
// KPAGE : allocate RAM space for the KDATA section (mapped to high address)
//------------------------------------------------------------------------------
        .data   .KDATA
        .globl  KPAGE_VIRT            
KPAGE_VIRT: .space KPAGE_LENGTH

        .text

//------------------------------------------------------------------------------
// KernelStart - Kernel entry point for nkloader
//------------------------------------------------------------------------------
LEAF_ENTRY(KernelStart)
        .set noreorder

        mtc0    zero, cause             // Clear interrupts
        mtc0    zero, entryhi           // Clear asid
        mtc0    zero, context           // clear the context register
        mtc0    zero, entrylo0          
        mtc0    zero, entrylo1          
        mtc0    zero, pagemask          
        mtc0    zero, count             // clear count (time since restart)
        mtc0    zero, wired
        mtc0    zero, index

        move    k0, zero
        move    k1, zero
        move    gp, zero

        // Switch execution and stack to cached, un-mapped region.
        la      t1, cached
        j       t1                      // jump to cached segment
        ehb

cached: //
        // Zero out kernel data page.
        //
        la      s3, KPAGE_VIRT          // (s3) = KDBase, cached, non-TLB address
        lui     t1, 0xa000
        or      s3, t1                  // (s3) = KDBase, uncache, non-TLB address

        move    t0, s3
        li      t1, KPAGE_LENGTH
10:     sw      zero,   (t0)
        sw      zero,  4(t0)
        sw      zero,  8(t0)
        sw      zero, 12(t0)
        sw      zero, 16(t0)
        sw      zero, 20(t0)
        sw      zero, 24(t0)
        sw      zero, 28(t0)
        subu    t1, 32
        bgtz    t1, 10b
        addiu   t0, 32

        addi    sp, s3, (KStack-KDBase)  // sp = KStack, non-TLB address

        // update g_pKData->pTOC
        lw      s0, pTOC
        sw      s0, OFST_TOCADDR(s3)

        // call kernel relocate to initialize kernel globals
        // (s0) = pTOC
        jal     KernelRelocate
        move    a0, s0                  // (delay slot) argument (a0) = pTOC

        // call SetOsAxsDataBlockPointer to update KDataStruct entry
        jal     SetOsAxsDataBlockPointer
        addi    a0, s3, (KData-KDBase)  // (delay slot) argument (a0) = KData

        // find the entry point of kernel.dll
        jal     FindKernelEntry
        move    a0, s0                  // (delay slot) argument (a0) = pTOC

        // (v0) = entry point of kernel.dll
        la      a1, OEMInitGlobals     // address of OEMInitGlobals
        j       v0
        move    a0, s3                  // (delay slot) argument (a0) = KDBase (NOTE: NOT KData like other CPUs)

        .end    KernelStart

