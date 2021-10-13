//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//------------------------------------------------------------------------------
//      TITLE("TLB Miss handler")
//------------------------------------------------------------------------------
//
//
// Module Name:
//
//    except.s
//
// Abstract:
//
//    This module contains the code to handle reloading the hardware
//  Translation Lookaside Buffer.
//
//------------------------------------------------------------------------------
#include "ksmips.h"
#include "nkintr.h"
#include "kpage.h"
#include "mem_mips.h"

#define jalfix(func)    \
        jal     func;   \
        nop;

#define VR5432_BP_BUG 1

#if defined(VR5432_BP_BUG)
        //
        // An external routine calling into this code is at risk for
	// this bug if there is a conditional branch close to the call
	// and interrupts are enabled at the time of the branch
	//
#define CP0_STOP_PREFETCH(inst, parm1, parm2, tempreg)  \
	la      tempreg, 199f;                           \
	j       tempreg;                                \
        nop;                                            \
199:                                                     \
        inst    parm1, parm2;
#else
#define CP0_STOP_PREFETCH(inst, parm1, parm2, tempreg) \
        inst    parm1, parm2;
#endif

        .text
        .globl  NullSection
NullSection:
        .repeat BLOCK_MASK+1
        .word   0
        .endr


//------------------------------------------------------------------------------
// CELOG replaces the TLB MISS Handler with its own.
//------------------------------------------------------------------------------
#ifdef CELOG
LEAF_ENTRY(CeLogTLBMissHandler)
        .set noreorder

        S_REG   t0, SaveT0              //                              :1
        la      k0, dwCeLogTLBMiss      // Global variable              :2-3
        lw      t0, 0(k0)               //                              :4
        addi    t0, t0, 1               // increment                    :5
        sw      t0, 0(k0)               // Update counter               :6

        L_REG   t0, SaveT0              //                              :7
        la      k0, TLBMissHandler      // Load address of real handler :8-9
        j       k0                      //                              :10
        nop                             //                              :11

        END_REGION(CeLogTLBMissHandler_End)

        .set    reorder
        .set    at
        .end    CeLogTLBMissHandler
#endif


//  The user mode virtual address space is 2GB split into 64 sections
// of 512 blocks of 16 4K pages. For some platforms, the # of blocks in
// a section may be limited to fewer than 512 blocks to reduce the size
// of the intermeditate tables. E.G.: Since the PeRP only has 5MB of total
// RAM, the sections are limited to 4MB. This results in 64 block sections.
//
// Virtual address format:
//  3322222 222221111 1111 110000000000
//  1098765 432109876 5432 109876543210
//  zSSSSSS BBBBBBBBB PPPP oooooooooooo
//
// Context register format:
//  33222222222211111 111110000 0000 00
//  10987654321098765 432109876 5432 10
//  zzzzzzzzzzzSSSSSS BBBBBBBBB PPPP zz


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LEAF_ENTRY(TLBMissHandler)
        .set    noreorder
        .set    noat

//  Upon entry, BadVAddr, Context, XContext and EntryHi registers hold the
//  virtual address that failed address translation.  The EntryHi register
//  also contains the ASID from which the translation fault occurred.  The
//  Random register normally contains a valid location in which to place the
//  replacement TLB entry.  The contents of the EntryLo register are undefined.
//  Note: The PageMask register contains the value written during KernelStart.

        mfc0    k0, badvaddr            // (k0) = faulting virtual addr :1
        S_REG   t0, SaveT0              //                              :2
        S_REG   k1, SaveK1              //                              :3
        srl     t0, k0, VA_SECTION-2    //                              :4
        bltz    k0, 80f                 // address out of range         :5
        and     t0, SECTION_MASK*4      // (t0) = section * 4           :6
        lw      t0, SectionTable(t0)    // (t0) = ptr to block table    :7
        srl     k1, k0, VA_BLOCK-2      //                              :8
        and     k1, BLOCK_MASK*4        // (k1) = block * 4             :9
        addu    t0, k1                  // (t0) = block table entry     :10
        lw      t0, (t0)                // (t0) = ptr to MEMBLOCK structure
        srl     k0, VA_PAGE-2           //                              :12
        and     k0, (PAGE_MASK/2)*8     // (k0) = (even page #) * 4     :13
        bgez    t0, 80f                 // unmapped memblock            :14
        addu    k0, t0                  // (k0) = ptr to page entry     :15
        lw      k1, mb_lock(t0)         // (k1) = block access lock     :16
        lw      t0, CurAKey             //                              :17
        and     k1, t0                  // (k1) = 0 if access not allowed
        lw      t0, mb_pages(k0)        // (t0) = even page info        :19
        beq     zero, k1, 80f           // access not allowed           :20
        lw      k0, mb_pages+4(k0)      // (k0) = odd page info         :21
        MTC_REG t0, entrylo0            // set even entry to write into TLB
        MTC_REG k0, entrylo1            // set odd entry to write into TLB
        L_REG   t0, SaveT0              // restore t0 register          :24
        L_REG   k1, SaveK1              //                              :25
        tlbwr                           // write entry randomly into TLB:26
        nop                             //                              :27
        nop                             // 3 cycle hazzard              :28
70:     eret                            //                              :29

#ifndef CELOG
80:     L_REG   k1, SaveK1              //                              :30
        .word   0x08000060              // j 0x80000180                 :31
        L_REG   t0, SaveT0              //                              :32
#else
        // when CELOG defined, CeLogTLBMissHandler is copied to exception
        // vector location instead of TLBMissHandler.  So we can not use
        // 'j 0x80000180' above directly, or it will fail if we were a
        // flash image.(the jump may too far apart and exceed MIPS 26-bit
        // jump limitation). The good thing is we don't have 32 dword limit
80:     L_REG   k1, SaveK1              //                              :30
        lui     k0, 0x8000              // load address for             :31
        addu    k0, k0, 0x180           // general exception handler    :32
        j       k0                      // jump!                        :33
        L_REG   t0, SaveT0              //                              :34
#endif

        END_REGION(TLBMissHandler_End)

        .set    reorder
        .set    at
        .end    TLBMissHandler




