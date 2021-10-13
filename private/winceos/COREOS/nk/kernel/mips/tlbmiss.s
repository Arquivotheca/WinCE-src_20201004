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
//      TITLE("TLB Miss handler")
//------------------------------------------------------------------------------
//
//
// Module Name:
//
//    tlbmiss.s
//
// Abstract:
//
//    This module contains the code to handle reloading the hardware
//  Translation Lookaside Buffer.
//
//------------------------------------------------------------------------------
#define ASM_ONLY
#include "ksmips.h"
#include "nkintr.h"
#include "mipskpg.h"
#include "vmlayout.h"
#include "vmmips.h"

//------------------------------------------------------------------------------
// TLBMissHandler - handle TLB Refill exception
// !! SIZE RESTRICTION - 32 instructions !!
//------------------------------------------------------------------------------
LEAF_ENTRY(TLBMissHandler)
        .set    noreorder
        .set    noat

//  Upon entry, BadVAddr, Context, XContext and EntryHi registers hold the
//  virtual address that failed address translation.  The EntryHi register
//  also contains the ASID from which the translation fault occurred.  The
//  Random register normally contains a valid location in which to place the
//  replacement TLB entry.  The contents of the EntryLo register are undefined.
//  Note: The PageMask register contains the value written during NKStartUp.

        mfc0    k0, badvaddr            // (k0) = faulted address               : 1+#nops
        S_REG   k1, SaveK1              //                                      : 2+#nops
        S_REG   t0, SaveT0              // save t0 and k1                       : 3+#nops

        //
        // NOTE: address above 0x70000000 is never mappped in user process, i.e. 
        //      reading shared heap from user process will not be handled here
        //
        lw      k1, pProcVM             // (k1) = current VM process            : 4+#nops
        bgez    k0, 20f                 // (badvaddr >= 0)?                     : 5+#nops
        srl     t0, k0, 20              // (delay slot) (t0) = badaddr >> 20    : 6+#nops

        // kernel address, use kernel's page directory
        lw      k1, pProcNK             // k1 = g_pprcNK                        : 7+#nops
        
20:     // common processing
        lw      k1, ProcPpdir(k1)       // (k1) = pprc->ppdir                   : 8+#nop
        andi    t0, t0, OFFSET_MASK     // (t0) = offset into page directory    : 9+#nop
        addu    t0, k1                  // (t0) = ptr to the pd entry           : 10+#nop
        lw      t0, (t0)                // (t0) = page table                    : 11+#nop
        slt     k1, k0, zero            // (k1) = (badvaddr < 0)                ; 12+#nop
        bgez    t0, 80f                 // bad if PT is not kernel address      : 13+#nop
        srl     k0, k0, 10              // (delay slot) k0 = badaddr >> 10      : 14+#nop
        andi    k0, k0, EVEN_OFST_MASK  // (k0) = offset of even page           : 15+#nop
        addu    k0, t0                  // (k0) = ptr to pt entry of even page  : 16+#nop

        lw      t0, (k0)                // (t0) = even page                     : 17+#nop
        lw      k0, 4(k0)               // (k0) = odd page                      : 18+#nop

        // or the "global" bit for kernel addresses
        or      t0, t0, k1              //                                      : 19+#nop
        or      k0, k0, k1              //                                      : 20+#nop

        MTC_REG t0, entrylo0            // set even entry to write into TLB     : 21+#nop
        MTC_REG k0, entrylo1            // set odd entry to write into TLB      : 22+#nop
        L_REG   t0, SaveT0              // restore t0 register                  : 23+#nop
        L_REG   k1, SaveK1              //                                      : 24+#nop
        ehb                             // 2 cycle hazard on entrylo1           : 25+#nop
                                        // between MTC_REG and tlbwr
        tlbwr                           // write entry randomly into TLB        : 26+#nop
                                        //
        ssnop                           //                                      : 27+#nop
        ehb                             //                                      : 28+#nop


#ifdef NKPROF

// On profiling builds, CeLog replaces the TLB miss handler with its own,
// so the 32-instruction size restriction does not apply.
//
// Maintain a counter of the number of times there was a profiler interrupt
// pending.  Comparing the total number of profiler interrupts against this
// counter gives us a percentage of time spent servicing TLB misses.
//
        S_REG   t0, SaveT0
        S_REG   k1, SaveK1
        
        // Check whether the profiler interrupt is pending
        mfc0    k1, cause               // (k1) = exception cause
        and     t0, k1, PROFILER_OFST_MASK // (t0) = non-zero if profiler interrupt pending
        beq     t0, zero, 60f           // no interrupt held up by a TLB miss
        nop
        
        // At this point we know there is a profiler interrupt waiting for the
        // TLB miss to end, so increment the counter
        la      k0, g_ProfilerState_dwProfilerIntsInTLB
        lw      t0, 0(k0)               // (t0) = g_ProfilerState_dwProfilerIntsInTLB
        addi    t0, t0, 1               // g_ProfilerState_dwProfilerIntsInTLB++
        sw      t0, 0(k0)               // Store counter
        
60:
        L_REG   t0, SaveT0
        L_REG   k1, SaveK1
        
#endif // NKPROF

        eret                            // Clears both Inst and Exec Hazards    : 29+#nop
        


#ifndef NKPROF

80:     // failed, restore t0, k1, and jump to GeneralException
        L_REG   k1, SaveK1              //                                      : 30+#nop
        .word   0x08000060              // j 0x80000180                         : 31+#nop
        L_REG   t0, SaveT0              //                                      : 32+#nop

#else

        // when NKPROF defined, CeLogTLBMissHandler is copied to exception
        // vector location instead of TLBMissHandler.  So we can not use
        // 'j 0x80000180' above directly, or it will fail if we were a
        // flash image.(the jump may too far apart and exceed MIPS 26-bit
        // jump limitation). The good thing is we don't have 32 dword limit
80:     L_REG   k1, SaveK1              //                              :30
        lui     k0, 0x8000              // load address for             :31
        addu    k0, k0, 0x180           // general exception handler    :32
        j       k0                      // jump!                        :33
        L_REG   t0, SaveT0              //                              :34

#endif  // NKPROF


        END_REGION(TLBMissHandler_End)

        .set    reorder
        .set    at
        .end    TLBMissHandler


//------------------------------------------------------------------------------
// On profiling builds, CeLog replaces the TLB miss handler with its own.
//------------------------------------------------------------------------------
#ifdef NKPROF
LEAF_ENTRY(CeLogTLBMissHandler)
        .set noreorder
        .set noat

        lw      k0, TlbMissCnt          //                              :1
        addi    k0, k0, 1               // g_pKData->dwTlbMissCnt ++    :2
        sw      k0, TlbMissCnt          //                              :3

        la      k0, TLBMissHandler      // Load address of real handler :4
        j       k0                      //                              :5
        nop                             //                              :6

        END_REGION(CeLogTLBMissHandler_End)

        .set    reorder
        .set    at
        .end    CeLogTLBMissHandler
#endif  // NKPROF
