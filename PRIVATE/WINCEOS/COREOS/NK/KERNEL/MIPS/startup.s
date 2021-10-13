//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
#include "kpage.h"
#include "mem_mips.h"

#define jalfix(func)    \
        jal func;   \
        nop;

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

//------------------------------------------------------------------------------
// KPAGE : allocate RAM space for the KDATA section (mapped to high address)
//------------------------------------------------------------------------------
        .data   .KDATA
        .globl  KPAGE_VIRT            
KPAGE_VIRT: .space KPAGE_LENGTH
    
    
        .text

//
// the following 3 variables can be updated by OEM using FIXUPVAR in config.bib
// to tell us what the CPU type is and we'll by-pass CPU type detection if the 
// value changed. 
//      fNKMIPS16Sup - set to 1 if the CPU support MIPS16 instructions, 0 if not
//      fNKTinyPageSup - set to 1 if the CPU support tiny pages (e.g. R41XX processors), 0 if not
//      fNKHasFPU - set to 1 if the FPU exist, 0 if not
//
// we'll try our best to detect the CPU types if the values are left unchanged (-1)
//
        .globl  fNKMIPS16Sup
        .globl  fNKTinyPageSup
        .globl  fNKHasFPU

fNKMIPS16Sup:           .word -1
fNKTinyPageSup:         .word -1
fNKHasFPU:              .word -1

// legacy MIPSII
#define PRID_R41XX      0x0C00                  // bit 8-15 of prid == 0xC
#define R41XX_M16SUP    0x00100000              // bit 20 of config on R41XX indicate MIPS16 support

// MIPS32/MIPS64
#define CFG1_MIPS16     0x4                     // bit 2 of config1 indicate MIPS16 support
#define CFG1_FPU        0x1                     // bit 0 of config1 indicate FPU

//------------------------------------------------------------------------------
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

        move    k0, zero
        move    k1, zero

        lw      s0, fNKMIPS16Sup
        lw      s1, fNKTinyPageSup
        lw      s2, fNKHasFPU

        li      t0, -1
        bne     t0, s0, 3f              // don't bother detect if value changed
        nop

        // try to figure out the information from config+procid registers
        move    s0, zero                // default no MIPS16 support
        move    s1, zero                // default not tiny page support
        move    s2, zero                // default no FPU

        mfc0    t0, config              // (t0) = config register
        nop                             // 1 cycle hazard??

        bltz    t0, 2f                  // bit 31 config indicate if config1 register exist
        nop
        
        // bit 31 is 0, legacy MIPSII CPUs
        mfc0    t2, prid                // t2 = prid
        nop                             // 1 cycle hazard??
        
        andi    t2, t2, 0xff00          // bit [8-15] is the processor family
        li      t1, PRID_R41XX          // (t1) = prid of R41XX family (0xc00)
        
        bne     t2, t1, 3f              // use default if not R41XX
        nop

        // R41XX (t0) == config register
        li      s1, 1                   // support tiny page
        li      t1, R41XX_M16SUP        // MIPS16 support is bit 20 of config register on R41XX
        j       3f
        and     s0, t0, t1              // (delay slot) s0 == support MIPS16

2:
        // MIPS32/MIPS64
        //mfc0    t0, config, 1         // read config1 register
        .word	0x40088001              // encoding for MIPS32 instruction "mfc0 t0, config, 1"
        
        andi    s0, t0, CFG1_MIPS16     // (s0) = support MIPS16?
        andi    s2, t0, CFG1_FPU        // (s2) = has FPU?

3:
        //
        //      (s0) - non-zero if support MIPS16
        //      (s1) - non-zero if support tiny page (R41XX)
        //      (s2) - non-zero if support FPU  (might not be needed since we can detect FPU by try-except
        //

        // CPU with tiny page support need a different pagemask
        beqz    s1, 4f
        li      s3, PFN_SHIFT_MIPS32    // (delay slot) (s3) = default PFN_SHIFT

        // tiny page support
        li      t0, 0x1800              // page mask for 4K pagesize on R4100
        li      s3, PFN_SHIFT_R41XX     // PFN_SHIFT is different with tiny page support
        mtc0    t0, pagemask

4:
        // (s3) = PFN_SHIFT
        li      s4, PAGE_SIZE           // (s4) = PAGE_SIZE
        srlv    s4, s4, s3              // (s4) = (PAGE_SIZE >> PFN_SHIFT) == PFN_INCR

        la      t3, KPAGE_VIRT
        li      t1, 0xA0000000
        or      t3, t3, t1
        
        li      t1, PFN_MASK
        and     t4, t3, t1              // (t4) = (KPAGE_VIRT | 0xa0000000) & PFN_MASK
        srlv    t4, t4, s3              // (t4) >>= PFN_SHIFT
        // t3 = virtual uncached KPAGE base
        // t4 = PFN of KPAGE_BASE
        
        //
        // Zero out kernel data page.
        //
        move    t0, t3
        li      t1, KPAGE_LENGTH
8:      sw      zero, (t0)
        subu    t1, 4
        bgtz    t1, 8b
        addiu   t0, 4

        LIGHTS(t0,t1, 0xCC)
        //
        // Initialize SectionTable in KPage
        //
        addi    t0, t3, (SectionTable-KData)
        li      t1, SECTION_MASK+1
        la      t2, NullSection
9:      sw      t2, (t0)
        subu    t1, 1
        bgtz    t1, 9b
        addiu   t0, 4
        //
        // Initialize the interrupt dispatch tables
        //
        addi    t0, t3, (FalseInt-KData)
        la      t1, FalseInterrupt
        sw      t1, (t0)
        la      t1, DisabledInterruptHandler
        sw      t1, 4(t0)
        sw      t1, 8(t0)
        sw      t1, 12(t0)
        sw      t1, 16(t0)
        sw      t1, 20(t0)
        sw      t1, 24(t0)
    
        //
        // Load temp stack pointer & global pointer.
        //
        addi    sp, t3, (KStack-KData)
        li      gp, 0
        li      t1, 0x80000000
        sw      t1, HandleBase-KData(t3)
        la      t1, APICallReturn
        sw      t1, PtrAPIRet-KData(t3)
        
        LIGHTS(t0,t1, 0xF0)

        //  Initialize address translation hardware.
        li      v0, 0x80000000          // Unmapped address
        lw      v1, OEMTLBSize          // (v1) = index & loop counter
10:     mtc0    zero, entrylo0          // Clear entrylo0 - Invalid entry
        mtc0    zero, entrylo1          // Clear entrylo1 - Invalid entry
        mtc0    v0, entryhi             // Clear entryhi - Ivalid address
        mtc0    v1, index               // Set index
        add     v0, 0x2000
        nop                             // Fill delay slot
        tlbwi                           // Write entry (indexed)
        addiu   v1, v1, -1              // Decrement index, loop counter
        bgez    v1, 10b                 // If not done (<0) do next one
        nop
        //
        // Initialize wired TLB entries as follows:
        //  0: virtual 0x00005000 to the kernel's data page (read-only)
        //  1: virtual 0xFFFFD000 to kernel's data page (read-write)
        //      (s4) = PFN_INCR
        //
        li      v0,  2                  // load 2 into wired
        mtc0    v0, wired               // load 2 into wired
        li      v0, KPAGE_USER
        mtc0    v0, entryhi

        ori     v0, t4, 0x03 | PG_CACHE // valid, global, cached
        add     v1, v0, s4              // valid, global, cached (next page : always 4k)
        mtc0    v0, entrylo0            // even page info
        mtc0    v1, entrylo1            // odd page info
        mtc0    zero, index
        nop
        nop
        tlbwi                           // write indexed entry
        nop
        nop
        
        li      v0, KData
        mtc0    v0, entryhi
        ori     v0, t4, 0x07 | PG_CACHE // valid, dirty, global, cached
        add     v1, v0, s4              // valid, dirty, global, cached (next page : always 4k)
        mtc0    v0, entrylo0            // even page info
        mtc0    v1, entrylo1            // odd page info
        li      v0, 1
        mtc0    v0, index               // index = 1
        nop
        nop
        tlbwi                           // write indexed entry
        nop
        nop
        nop


        // we have wired TLB entries for KData, now we can start using it
        

        // Install TLBMiss handler
        li      a0, 0x0000
#if defined(CELOG)
        la      a1, CeLogTLBMissHandler
        la      a2, CeLogTLBMissHandler_End
#else   
        la      a1, TLBMissHandler
        la      a2, TLBMissHandler_End
#endif
        move    a3, s1
        jalfix(InstallHandler)

        // Install 64-bit TLBMiss handler
#ifdef MIPSIV
        li      a0, 0x0080
#ifdef CELOG
        la      a1, CeLogTLBMissHandler
        la      a2, CeLogTLBMissHandler_End
#else
        la      a1, TLBMissHandler
        la      a2, TLBMissHandler_End
#endif
        move    a3, zero
        jalfix(InstallHandler)
#endif  //  MIPSIV

        // Install General Exception handler
        li      a0, 0x0180
        la      a1, GeneralException
        la      a2, GeneralException_End
        move    a3, s1
        jalfix(InstallHandler)

        // Install CacheError handler
        li      a0, 0x0100
        la      a1, CacheErrorHandler
        la      a2, CacheErrorHandler_End
        move    a3, s1
        jalfix(InstallHandler)
        nop
        nop

        LIGHTS(t0,t1, 0xFC)

        // Switch execution and stack to cached, un-mapped region.
        la      t1, cached
        j       t1                      // jump to cached segment
        nop

cached: li      sp, KStack              // (sp) = 0xFFFFF???

        LIGHTS(t0,t1, ~1)
        
        // update CPU specific parameters
        // (s0) = MIPS16 support
        // (s1) = tiny page support (indidate R41XX cpu)
        // (s3) = PFN_SHIFT
        // (s4) = PFN_INCR
        li      t0, (VA_PAGE+1)
        sw      s3, PfnShift
        sw      s1, IsR41XX
        subu    t0, t0, s3              // (t0) == (VA_PAGE+1) - PFN_SHIFT
        sw      s0, MIPS16Sup
        sw      s4, PfnIncr
        sw      t0, TlbShift            // TlbShift = (VA_PAGE+1) - PFN_SHIFT

        lw      a0, pTOC
        jalfix(KernelRelocate)
        
        LIGHTS(t0,t1, ~2)


// Since the kernel can not manipulate the PSR with atomic instructions
// the MIPS kernel maintains a BasePSR value that can be manipulated
// atomically.  This value is then copied into the PSR register.  Note: 
// While the BasePSR value has a valid interrupt mask, it NEVER has the
// IE bit set! Enable the base PSR for operating system use.
// Until now, all exceptions were sent to the boot ROM.  The kernel is
// not initialized far enough to take the virtual memory exceptions with the 
// exception of statically mapped regions created with NKCreateStaticMapping().
// Clear the BEV bit so that all vectors are delivered to the kernel.

	sw	zero, BasePSR
        mtc0	zero, psr

        LIGHTS(t0,t1, ~4)
        
        mfc0    a0, prid                // (a0) = process ID information
        nop                             // 1 cycle hazard??
        
        jalfix(MIPSInit)                // MIPS general initialization

        LIGHTS(t0,t1, 0)
        
	jalfix(OEMInit)

// OEMInit updates the interrupt mask within the BasePSR.  Clear the
// interrupt mask to prevent interrupts from accidently being delivered
// during the kernel initialization.  Note: GetKHeap uses INTERRUPT_ON
// which actually sets IE, GetKHeap is called during KernelInit before
// the kernel is fully initialized.  Deliver interrupts only after the
// kernel is fully initialized.

        lw      t0, BasePSR
        nop
        li      s0, ( 0xff << PSR_INTMASK )
        and     s0, t0                      //  Locate enabled interrupts.
        xor     t0, s0                      //  Mask all interrupts.
        sw      t0, BasePSR

	jalfix(KernelFindMemory)

// Enable 64-bit instructions as appropriate, but keep interrupts disabled.

	li	s1, PSR_XX_C | PSR_FR_C | PSR_UX_C //  Enable 64-bit instructions.
        mtc0	s1, psr
	sw	s1, BasePSR

// Finish the kernel initialization.

	jalfix(KernelInit)

// Now it is finally time to enable interrupts.  The kernel is now fully
// initialized and can handle the device interrupts.

        or      s1, s0                  //  Update the interrupt mask.
	sw	s1, BasePSR
	ori	s1, ( 1 << PSR_IE )     //  Enable interrupts.
	mtc0	s1, psr
	nop

// Schedule the first thread.
	j	FirstSchedule
	nop
	.end	StartUp


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
//      (a3) = Is this a R41XX cpu?
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
    

