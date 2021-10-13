//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#include "ksmips.h"
#include "nkintr.h"
#include "kpage.h"
#include "mem_mips.h"
#include "psyscall.h"
#include "xtime.h"

                .struct 0
_argarea:       .space 4 * REG_SIZE     // minimum argument area for stack frame
_SaveT0:        .space REG_SIZE
_SaveT1:        .space REG_SIZE
_SaveT2:        .space REG_SIZE
_SaveT3:        .space REG_SIZE
_SaveT4:        .space REG_SIZE
_SaveT5:        .space REG_SIZE
_SaveT6:        .space REG_SIZE
_SaveT7:        .space REG_SIZE
_SaveT8:        .space REG_SIZE
_SaveT9:        .space REG_SIZE
_SaveV1:        .space REG_SIZE
_SaveRa:        .space REG_SIZE
_SaveHi:        .space REG_SIZE
_SaveLo:        .space REG_SIZE
size_save_area:

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
NESTED_ENTRY(NKCallIntChain, 0, zero)
        
        .set    noreorder
        
        subu    sp, size_save_area
        
        //
        // We've got to call a chained ISR, so let's save registers so we can
        // call a "C" routine.
        //
        S_REG   t0, _SaveT0(sp)
        S_REG   t1, _SaveT1(sp)
        S_REG   t2, _SaveT2(sp)
        S_REG   t3, _SaveT3(sp)
        S_REG   t4, _SaveT4(sp)
        S_REG   t5, _SaveT5(sp)
        S_REG   t6, _SaveT6(sp)
        S_REG   t7, _SaveT7(sp)
        S_REG   t8, _SaveT8(sp)
        S_REG   t9, _SaveT9(sp)
        S_REG   ra, _SaveRa(sp)
        mfhi    t0
        S_REG   t0, _SaveHi(sp)
        mflo    t0
        S_REG   t0, _SaveLo(sp)
        
        jal     NKCallIntChainWrapped
        S_REG   v1, _SaveV1(sp)
        
        //
        // Restore registers
        //
        L_REG   t0, _SaveLo(sp)
        mtlo    t0
        L_REG   t0, _SaveHi(sp)
        mthi    t0
        L_REG   t0, _SaveT0(sp)
        L_REG   t1, _SaveT1(sp)
        L_REG   t2, _SaveT2(sp)
        L_REG   t3, _SaveT3(sp)
        L_REG   t4, _SaveT4(sp)
        L_REG   t5, _SaveT5(sp)
        L_REG   t6, _SaveT6(sp)
        L_REG   t7, _SaveT7(sp)
        L_REG   t8, _SaveT8(sp)
        L_REG   t9, _SaveT9(sp)
        L_REG   ra, _SaveRa(sp)
        L_REG   v1, _SaveV1(sp)
        
        j       ra
        addu    sp, size_save_area
        
        .end    NKCallIntChain


        LEAF_ENTRY (NKIsSysIntrValid)

        .set    noreorder
        .set    noat
        
        subu    a0, SYSINTR_DEVICES             // (a0) = idInt - SYSINTR_DEVICE?
        bltz    a0, NKIRet                      // invalid idInt if (a0) < 0
        move    v0, zero                        // (delay slot) return 0
        
        sltu    v0, a0, SYSINTR_MAX_DEVICES     // v0 = (a0 < SYSINTR_MAX_DEVICES)
        beq     v0, zero, NKIRet                // out of range, return 0
        sll     a0, a0, 2                       // (delay slot) a0 *= 4

        // valid range, return IntrEvents[idInt-SYSINTR_DEVICES]
        lw      v0, IntrEvents(a0)

NKIRet:

        j       ra
        nop

        .set    at
        .set    reorder

        .end    NKIsSysIntrValid
        
