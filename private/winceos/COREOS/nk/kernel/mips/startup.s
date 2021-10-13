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
#include "vmlayout.h"
#include "vmmips.h"
#include "oemglobal.h"

#if 0   // PERP
#define LIGHTS(t0, t1, value)   \
        lui     t0, 0xaa00; \
        li      t1, value;  \
        sw      t1, 0x1010(t0);
#endif

#if 0
#define LIGHTS(t0, t1, value)   \
        lui     t0, 0xb040; \
        lh  t1, 2(t0);  \
        and t1, 0xff;   \
        or  t1, value<<8;   \
        sh      t1, 2(t0);
#endif

#ifndef LIGHTS
#define LIGHTS(t0, t1, value)
#endif


        .text


// legacy MIPSII
#define PRID_R41XX      0x0C00                  // bit 8-15 of prid == 0xC
#define R41XX_M16SUP    0x00100000              // bit 20 of config on R41XX indicate MIPS16 support

// MIPS32/MIPS64
// MIPS_FLAG_FPU_PRESENT(0x1) and MIPS_FLAG_MIPS16(0x4) are defined for the exact same bits
// as config1 registers.

//------------------------------------------------------------------------------
// MIPSSetup: setup MIPS CPU, with OEM information
//
//  (a0) - ptr to the non-TLB, uncached address of KDBase
//  (a1) - OAL entry point address
//
//------------------------------------------------------------------------------
        LEAF_ENTRY(NKStartup)
        .set noreorder

        move    s8, a0                  // (s8) = KDBase, uncached, non-TLB address
        mfc0    s7, prid                // (s7) = prid
        nop                             // 1 cycle hazard??

        //
        //  Set stack pointer using uncached, non-TLB address
        //
        addu    sp, s8, KStack-KDBase

        //
        // exchange NK/OAL globals
        //
        jal     LoadOAL                 // find OAL entry point and exchange NK/OEM globals
        lw      a0, OFST_TOCADDR(s8)    // (delay slot) (a0) = pTOC

        //
        // detect architecture flags
        //
        jal     OEMGetArchFlagOverride  // see if OEM specifies architecture override
        nop

        li      t0, MIPS_FLAG_NO_OVERRIDE
        bne     t0, v0, 30f             // don't bother detect if architecture override is specified
        move    s0, v0                  // (delay slot) (s0) = architecture override

        // architecture override not specified, detect cpu parameters
        move    s0, zero                // (s0) = default architecture flags == 0
        mfc0    t0, config              // (t0) = config register
        nop                             // 1 cycle hazard??
        bltz    t0, 20f                 // bit 31 config indicate if config1 register exist
        nop
        
        // bit 31 is 0, legacy MIPSII CPUs
        // (s7) = prid
        andi    t2, s7, 0xff00          // (t2) = bit [8-15] is the processor family
        li      t1, PRID_R41XX          // (t1) = prid of R41XX family (0xc00)
        
        bne     t2, t1, 30f             // use default if not R41XX
        nop

        // R41XX -  support tiny page, support MIPS16, and need to insert nop at the beginning of exception handler
        j       30f
        li      s0, MIPS_FLAG_TINY_PAGE|MIPS_FLAG_INSERT_NOP_FOR_HANDLER|MIPS_FLAG_MIPS16   // (delay slot) (s0) = MIPS Flags

20:
        // MIPS32/MIPS64
        //mfc0    t0, config, 1         // read config1 register
        .word   0x40088001              // encoding for MIPS32 instruction "mfc0 t0, config, 1"
        
        andi    t1, t0, MIPS_FLAG_FPU_PRESENT|MIPS_FLAG_MIPS16     // (t1) = support MIPS16/FPU?
        or      s0, t1                  // (s0) = architecture flags

        //
        // Figure PFN_SHIFT, PFN_INCR, and update pagemask register if tiny-page is supported
        //      (s0) - architecture flags
        //
30:
        andi    t0, s0, MIPS_FLAG_TINY_PAGE // (t0) = support tiny page
        beqz    t0, 40f
        li      s2, PFN_SHIFT_MIPS32        // (delay slot) (s2) = PFN_SHIFT_MIPS32 (default)

        // support tiny page
        li      t1, 0x1800                  // (t0) = pagemask when support tiny page
        li      s2, PFN_SHIFT_TINY_PAGE     // (s2) = PFN_SHIFT_TINY_PAGE
        mtc0    t1, pagemask                // update pagemask

40:
        // (s0) = architecture flags
        // (s2) = PFN_SHIFT


        li      s3, VM_PAGE_SIZE            // (s3) = PAGE_SIZE
        srlv    s3, s3, s2                  // (s3) = (PAGE_SIZE >> PFN_SHIFT) == PFN_INCR

        LIGHTS(t0,t1, 0xF0)

        //
        // Initialize the interrupt dispatch tables
        //
        addi    t0, s8, OFST_FALSEINT
        la      t1, FalseInterrupt
        sw      t1, (t0)
        la      t1, DisabledInterruptHandler
        sw      t1, 4(t0)
        sw      t1, 8(t0)
        sw      t1, 12(t0)
        sw      t1, 16(t0)
        sw      t1, 20(t0)
        sw      t1, 24(t0)
    
        //  clear TLB
        //
        // (s0) = architecture flags
        // (s2) = PFN_SHIFT
        // (s3) = PFN_INCR
        // (s7) = prid
        // (s8) = KDBase, non-TLB address
        //

        jal     OEMGetLastTLBIdx
        ehb                             // hazard barrier required (found empirically), due to pagemask?
        // (v0) = last TLB index

        li      t0, 0x80000000          // Unmapped address
50:     mtc0    zero, entrylo0          // Clear entrylo0 - Invalid entry
        mtc0    zero, entrylo1          // Clear entrylo1 - Invalid entry
        mtc0    t0, entryhi             // Clear entryhi - Ivalid address
        mtc0    v0, index               // Set index
        add     t0, 0x2000              // increment (t0) by 2 pages
        ssnop
        ehb                             // 2 cycle hazard on index between mtc0 and tlbwi
        tlbwi                           // Write entry (indexed)
        addiu   v0, v0, -1              // Decrement index, loop counter
        bgez    v0, 50b                 // If not done (<0) do next one
        ehb                             // hazzard barrier


        move    a0, s8                  // (a0) = KDBase = PCB Base of master CPU
        move    a1, s8                  // (a1) = KData Base
        move    a2, s2                  // (a2) = PfnShift

        jal     WireTLB
        move    a3, s3                  // (delay slot) (a3) = PfnIncr

        //
        // kpage/PCB wired, we can start using them.
        //
        // NOTE: We only access the non-TLB address of kpage uncached prior to this point. 
        //       i.e. we don't have to worry about cache coherency when we start accessing it via
        //       the wired address.
        //
        //       From here on, we'll access KDBase/kstack via the wired address.
        // 
        // (s0) = architecture flags
        // (s1) = Page table entry for KDBase (0xffffc000)
        // (s2) = PFN_SHIFT
        // (s3) = PFN_INCR
        // (s7) = prid
        // (s8) = KDBase, uncached non-TLB address

        la      sp, KStack              // (sp) = KStack (0xffffd7c0)
        sw      s0, ArchFlags           // update KData.dwArchFlags
        sw      s2, PfnShift_KData      // update PFN_SHIFT
        sw      s3, PfnIncr_KData       // update PFN_INCR
        subu    s8, s8, 0x20000000      // (s8) = cached address of KDBase

        la      a0, APIRet              // (a0) = APIRet offset
        sw      a0, PtrAPIRet_KData

        // Install TLBMiss handler
        andi    s6, s0, MIPS_FLAG_INSERT_NOP_FOR_HANDLER    // (s6) = nop required for handler
        li      a0, 0x0000
#if defined(NKPROF)
        la      a1, CeLogTLBMissHandler
        la      a2, CeLogTLBMissHandler_End
#else   
        la      a1, TLBMissHandler
        la      a2, TLBMissHandler_End
#endif  // NKPROF
        jal     InstallHandler
        move    a3, s6                  // (delay slot): (a3) insert nop or not

        // Install 64-bit TLBMiss handler
#ifdef MIPSIV
        li      a0, 0x0080
#ifdef NKPROF
        la      a1, CeLogTLBMissHandler
        la      a2, CeLogTLBMissHandler_End
#else
        la      a1, TLBMissHandler
        la      a2, TLBMissHandler_End
#endif  // NKPROF
        jal     InstallHandler
        move    a3, s6                // (delay slot): (a3) insert nop or not
#endif  //  MIPSIV

        // Install General Exception handler
        li      a0, 0x0180
        la      a1, GeneralException
        la      a2, GeneralException_End
        jal     InstallHandler
        move    a3, s6                // (delay slot): (a3) insert nop or not

        // Install CacheError handler
        li      a0, 0x0100
        la      a1, CacheErrorHandler
        la      a2, CacheErrorHandler_End
        jal     InstallHandler
        move    a3, s6                // (delay slot): (a3) insert nop or not
        nop

        LIGHTS(t0,t1, 0xFC)

        // Since the kernel can not manipulate the PSR with atomic instructions
        // the MIPS kernel maintains a BasePSR value that can be manipulated
        // atomically.  This value is then copied into the PSR register.  Note: 
        // While the BasePSR value has a valid interrupt mask, it NEVER has the
        // IE bit set! Enable the base PSR for operating system use.
        // Until now, all exceptions were sent to the boot ROM.  The kernel is
        // not initialized far enough to take the virtual memory exceptions with the 
        // exception of statically mapped regions created with NKCreateStaticMapping().
        // Clear the BEV bit so that all vectors are delivered to the kernel.

        sw      zero, BasePSR_KData
        mtc0    zero, psr

        LIGHTS(t0,t1, ~4)

        move    a0, s7                  // (a0) = process ID information
        move    a1, s1                  // (s1) = page table entry for KDBase
        
        jal     MIPSInit                // MIPS general initialization
        addiu   a2, s8, KData-KDBase    // (delay slot) (a2) = pKData

        LIGHTS(t0,t1, 0)
        
        jal     OEMInit
        nop
        jal     InitAllOtherCPUs
        nop

        // OEMInit updates the interrupt mask within the BasePSR.  Clear the
        // interrupt mask to prevent interrupts from accidently being delivered
        // during the kernel initialization.  Note: GetKHeap uses INTERRUPT_ON
        // which actually sets IE, GetKHeap is called during KernelInit before
        // the kernel is fully initialized.  Deliver interrupts only after the
        // kernel is fully initialized.

        lw      t0, BasePSR_KData
        nop
        li      s0, (0xff << PSR_INTMASK)
        and     s0, t0                      //  Locate enabled interrupts.
        xor     t0, s0                      //  Mask all interrupts.
        sw      t0, BasePSR_KData

        // Enable 64-bit instructions as appropriate, but keep interrupts disabled.

        li      s1, PSR_XX_C | PSR_FR_C | PSR_UX_C //  Enable 64-bit instructions.
        mtc0    s1, psr
        sw      s1, BasePSR_KData

        // Finish the kernel initialization.

        jal     KernelInit
        ehb

        // Now it is finally time to enable interrupts.  The kernel is now fully
        // initialized and can handle the device interrupts.

        or      s1, s0                  //  Update the interrupt mask.
        sw      s1, BasePSR_KData

        // Schedule the first thread.
        j       FirstSchedule
        ehb                             // hazzard barrier
        .end    NKStartUp


//------------------------------------------------------------------------------
// InstallHandler(vector, handler start, handler end, fIsR41XX)
//
//  Install an exception handler by copying the code into memory at the
// exception vector location. This method requires that a handler fit in the
// space between its vector location and the next vector.
//
//  Entry   (a0) = vector address
//      (a1) = ptr to start of handler code
//      (a2) = ptr to end of handler code
//      (a3) = need nop at the beginning of handler?
//  Exit    none
//  Uses    t0-2
//
//------------------------------------------------------------------------------
LEAF_ENTRY(InstallHandler)

        .set noreorder
        lui     t0, 0xA000
        or      a0, t0                  // force into un-cached segment
                                        
        subu    t2, a2, a1              // (t2) = # of bytes to copy
        sltu    t1, t2, 0x81            // (t1) = 0 iff length > 0x80
        beq     zero, t1, 20f           // handler too large to copy
        nop                             
        or      a1, t0                  // force into un-cached segment
10:     lw      t0, (a1)                
        lw      t1, 4(a1)               
        sw      t0, (a0)                
        sw      t1, 4(a0)               
        subu    t2, 8                   // (t2) = remaining bytes copy count
        addu    a0, 8                   // (a0) = ptr to dest for next 8 bytes
        bgtz    t2, 10b                 
        addu    a1, 8                   // (a1) = ptr to src of next 8 bytes
        
        j       ra
        nop

        // The handler is more than 0x80 bytes, just install a jump to it instead
        // of copying the entire handler.

20:     
        beq     zero, a3, 30f
        nop
        // Install handler for R41XX
        sw      zero, 0(a0)
        sw      zero, 4(a0)
        srl     a2, a1, 16
        sh      a2, 8(a0)
        li      a2, 0x3c1a
        sh      a2, 10(a0)              // lui k0, [high 16 bits of address]
        sh      a1, 12(a0)

        li      a2, 0x375a
        sh      a2, 14(a0)              // ori k0, k0, [low 16 bits of address
        li      a2, 0x03400008
        sw      a2, 16(a0)              // j k0
        j       ra
        sw      zero, 20(a0)            // nop
30:
        // Install handler for R4300
        srl     a2, a1, 16
        sh      a2, 0(a0)
        li      a2, 0x3c1a
        sh      a2, 2(a0)               // lui k0, [high 16 bits of address]
        sh      a1, 4(a0)

        li      a2, 0x375a
        sh      a2, 6(a0)               // ori k0, k0, [low 16 bits of address
        li      a2, 0x03400008
        sw      a2, 8(a0)               // j k0
        j       ra
        sw      zero, 12(a0)            // nop

        .set reorder
        .end    InstallHandler
    
        LEAF_ENTRY(NKMpStart)
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

        lw      t0, masterConfig
        mtc0    t0, config              // enable cache for KSEG0 with this statement
        ssnop
        ssnop
        ssnop
        ssnop
        ssnop
        ehb

        // Switch execution and stack to cached, un-mapped region.
        la      t1, waitformpstart
        j       t1                      // jump to cached segment
        nop

waitformpstart:
        // wait until g_fStartMP is set
        lw      t0, g_fStartMP
        beqz    t0, waitformpstart
        nop

        subu    t0, 1
        bnez    t0, gotpcb
        nop
        
        // get our index to g_ppcbs
        la      t1, idxCpu
retry:
        ll      t0, (t1)
        addu    t2, t0, 1
        sc      t2, (t1)
        beqz    t2, retry               // check if sc was successful
        addu    t0, 1                   // (dealy slot) (t0) = our index

gotpcb:
        // (t0) = idx to PCB
        la      t1, g_ppcbs
        sll     t0, t0, 2               // (t0) *= 4 ==> offset to g_ppcbs
        add     t0, t1                  // (t0) = &g_ppcbs[our index]
        lw      a0, (t0)                // (a0) = ppcb
        subu	a0, KData-KDBase		// (a0) = ppcb - (KData - KDBase)
        lw      a1, g_pKData            // (a1) = g_pKData
        subu	a1, KData-KDBase		// (a1) = KDBase
        lw      a2, PfnShift_KData-KDBase(a1)   // (a2) = PFN_SHIFT

        jal     WireTLB
        lw      a3, PfnIncr_KData-KDBase(a1)    // (delay slot) (a3) = PFN_INCR

        // TLB wired, we can start using PCB
        la      sp, KStack

        lw      t1, BASEPSR
        mtc0	t1, psr
    
        jal     MpStartContinue
        addu	a0, KData-KDBase		// (delay slot) (a0) = ppcb

        // Schedule the first thread.
        j       FirstSchedule
        nop

        .set reorder
        .end NKMpStart

        LEAF_ENTRY(ReadConfigRegister)
        .set noreorder
        mfc0    v0, config
        j       ra
        nop
        .set reorder
        .end ReadConfigRegister

//------------------------------------------------------------------------------
// WireTLB (ppcb, pKData, pfnshift, pfnincr)
// entry:
//      (a0) = PCB Base   (KSEG0/KSEG1 address)
//      (a1) = KData Base (KSEG0/KSEG1 address)
//      (a2) = PfnShift
//      (a3) = PfnIncr
// exit:
//      wired 3 TLB entries to map KDBase (2-page), KPAGE_USER (1-page), and PPCB_USER (1-page)
//
// use: t2-t5, v0, v1, at
//------------------------------------------------------------------------------

        LEAF_ENTRY(WireTLB)
        .set noreorder
        //
        //
        // Initialize wired TLB entries as follows:
        //  0: virtual 0x00005000 to the kernel's data page (read-only, 1 page)
        //  1: virtual 0x0000d000 to user's PCB page (read-only, 1 page)
        //  2: virtual 0xFFFFC000 to kernel's PCB page (read-write, 2 pages)
        //

        li      t2, PHYS_MASK           // (t2) = 0x1FFFF000 = phys addr mask

        and     t3, a1, t2              // (t3) = KDBase & PHYS_MASK
        srlv    t3, t3, a2              // (t3) >>= PFN_SHIFT == PFN for KDBase

        and     t4, a0, t2              // (t4) = PCB Base & PHYS_MASK
        srlv    t4, t4, a2              // (t4) >>= PFN_SHIFT == PFN for PCB Base

        li      v0, PG_GLOBAL_MASK      // (v0) = PG_GLOBAL_MASK
                                        //  MIPS SPEC require G bit to be set on both entrylo0 and entrylo1
                                        //  for the entry to be global

        li      t5,  3                  // load 3 into wired
        mtc0    t5, wired               // load 3 into wired

        //
        // wire KPAGE_USER (0x4000)
        //
        li      t5, KPAGE_USER
        mtc0    t5, entryhi

        ori     t5, t3, 0x03 | PG_CACHE // valid, global, cached = tlb entry for even page
        add     v1, t5, a3              // (v1) = (t5) + PFN_INCR = tlb entry for odd page
                                        //
        mtc0    v0, entrylo0            // even page info (unmapped)
        mtc0    v1, entrylo1            // odd page info
        mtc0    zero, index
        ssnop                           
        ehb                             // 2 cycle hazard on index between mtc0 and tlbwi
        tlbwi                           // write indexed entry
        ssnop
        ssnop

        //
        // wire PPCB_PAGE_USER (0xc000)
        //
        li      t5, PPCB_PAGE_USER
        mtc0    t5, entryhi

        ori     t5, t4, 0x03 | PG_CACHE // valid, global, cached = tlb entry for even page
        add     v1, t5, a3              // (v1) = (t5) + PFN_INCR = tlb entry for odd page

        // entrylo0 already setup to PG_GLOBAL_MASK while we map KPAGE_USER
        mtc0    v1, entrylo1            // odd page info
        li      v1, 1
        mtc0    v1, index
        ssnop                           
        ehb                             // 2 cycle hazard on index between mtc0 and tlbwi
        tlbwi                           //  write indexed entry
        ssnop
        ssnop


        //
        // wire PCB Base (0xffffc000)
        //
        li      v0, KDBase
        mtc0    v0, entryhi
        ori     v0, t4, 0x07 | PG_CACHE // valid, dirty, global, cached = tlb entry for even page
        add     v1, v0, a3              // (v1) = (v0) + PFN_INCR = tlb entry for odd page
        mtc0    v0, entrylo0            // even page info
        mtc0    v1, entrylo1            // odd page info
        li      v0, 2
        mtc0    v0, index               // index = 2
        ssnop                           
        ehb                             // 2 cycle hazard on index between mtc0 and tlbwi
        tlbwi                           // write indexed entry
        ssnop
        ssnop
        ssnop
        jr.hb   ra                      // Clear both execution and instruction hazards
        ssnop                           // 5 cycle hazard between tlbwi and instruction fetch using new TLB entry
        .set reorder
        .end    WireTLB

