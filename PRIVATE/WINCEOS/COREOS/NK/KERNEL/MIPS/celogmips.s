//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//------------------------------------------------------------------------------
//
//  
//------------------------------------------------------------------------------
//
//  Module Name:  
//  
//      celogmips.s
//  
//  Abstract:  
//
//      Implements the event logging functions for MIPS interrupt service
//      routines and thread migrations.
//      
//------------------------------------------------------------------------------
#include "ksmips.h"


        .data

#ifdef _MIPS64
        // MIPS64 required 8-byte align for LD/SD instructions
        .align 3
#endif
Save_t: .space  15 * REG_SIZE

        .text

#ifdef CELOG

LEAF_ENTRY(CeLogInterruptMIPS)
        .set noreorder

        la      v0, Save_t              // save temps
        S_REG   t0, 0*REG_SIZE(v0)
        S_REG   t1, 1*REG_SIZE(v0)
        S_REG   t2, 2*REG_SIZE(v0)
        S_REG   t3, 3*REG_SIZE(v0)
        S_REG   t4, 4*REG_SIZE(v0)
        S_REG   t5, 5*REG_SIZE(v0)
        S_REG   t6, 6*REG_SIZE(v0)
        S_REG   t7, 7*REG_SIZE(v0)
        S_REG   ra, 8*REG_SIZE(v0)
        S_REG   t8, 9*REG_SIZE(v0)
        S_REG   t9,10*REG_SIZE(v0)
        S_REG   v1,11*REG_SIZE(v0)
        S_REG   a3,12*REG_SIZE(v0)
        mfhi    t0
        mflo    t1
        S_REG   t0, 13*REG_SIZE(v0)
        S_REG   t1, 14*REG_SIZE(v0)

        jal     CeLogInterrupt
        subu    sp, 4*REG_SIZE          // delay slot, reserver space
                                        // for 4 registers per C calling
                                        // convention

        addu    sp, 4*REG_SIZE          // reclaim the stack space
        
        la      v0, Save_t              // restore temps
        L_REG   t0, 13*REG_SIZE(v0)
        L_REG   t1, 14*REG_SIZE(v0)
        mthi    t0
        mtlo    t1
        L_REG   t0, 0*REG_SIZE(v0)
        L_REG   t1, 1*REG_SIZE(v0)
        L_REG   t2, 2*REG_SIZE(v0)
        L_REG   t3, 3*REG_SIZE(v0)
        L_REG   t4, 4*REG_SIZE(v0)
        L_REG   t5, 5*REG_SIZE(v0)
        L_REG   t6, 6*REG_SIZE(v0)
        L_REG   t7, 7*REG_SIZE(v0)
        L_REG   ra, 8*REG_SIZE(v0)
        L_REG   t8, 9*REG_SIZE(v0)
        L_REG   t9,10*REG_SIZE(v0)
        L_REG   v1,11*REG_SIZE(v0)
        L_REG   a3,12*REG_SIZE(v0)

        j       ra
        nop
        .end

#endif // CELOG


//------------------------------------------------------------------------------
//
// SetCPUASID calls this routine.  On entry, A0 = hProc.
//
//------------------------------------------------------------------------------
LEAF_ENTRY(CELOG_ThreadMigrateMIPS)
        .set noreorder

        subu    sp, 13*REG_SIZE         // adjust sp
        S_REG   ra, 0*REG_SIZE(sp)      // save RA
        S_REG   t0, 1*REG_SIZE(sp)      // save temps
        S_REG   t1, 2*REG_SIZE(sp)
        S_REG   t2, 3*REG_SIZE(sp)
        S_REG   t3, 4*REG_SIZE(sp)
        S_REG   t4, 5*REG_SIZE(sp)
        S_REG   t5, 6*REG_SIZE(sp)
        S_REG   t6, 7*REG_SIZE(sp)
        S_REG   t7, 8*REG_SIZE(sp)
        S_REG   t8, 9*REG_SIZE(sp)
        S_REG   t9,10*REG_SIZE(sp)
        S_REG   v0,11*REG_SIZE(sp)
        S_REG   v1,12*REG_SIZE(sp)

        move    a1, zero                // (a1) = 0

        //
        // Call the C function, CeLogThreadMigrate
        //
        jal     CeLogThreadMigrate
        subu    sp, 4*REG_SIZE          // delay slot, reserver space
                                        // for 4 registers per C calling
                                        // convention

        addu    sp, 4*REG_SIZE          // reclaim the stack space
        

        L_REG   ra, 0*REG_SIZE(sp)      // restore RA
        L_REG   t0, 1*REG_SIZE(sp)      // restore temps
        L_REG   t1, 2*REG_SIZE(sp)
        L_REG   t2, 3*REG_SIZE(sp)
        L_REG   t3, 4*REG_SIZE(sp)
        L_REG   t4, 5*REG_SIZE(sp)
        L_REG   t5, 6*REG_SIZE(sp)
        L_REG   t6, 7*REG_SIZE(sp)
        L_REG   t7, 8*REG_SIZE(sp)
        L_REG   t8, 9*REG_SIZE(sp)
        L_REG   t9,10*REG_SIZE(sp)
        L_REG   v0,11*REG_SIZE(sp)
        L_REG   v1,12*REG_SIZE(sp)
        addu    sp,13*REG_SIZE          // adjust sp

        j       ra
        nop
        .end


