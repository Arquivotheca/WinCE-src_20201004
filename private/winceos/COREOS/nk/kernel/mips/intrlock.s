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
//      TITLE("Interlock memory operations")
//------------------------------------------------------------------------------
//
//
// Module Name:
//
//    intrlock.s
//
// Abstract:
//
//    This module implements the InterlockedIncrement, I...Decrement,
// I...Exchange and I...TestExchange APIs.
//
//    WARNING: This module makes special use of register K1 to inform the
//  GeneralException handler in except.s that an interlocked operation
//  is being performed. Because the exception handler code has detailed
//  knowledge of this code, extreme care must be exercised when modifying
//  this module. For example, the store instruction must be followed
//  immediately by a "j ra" instruction.
//
// Environment:
//
//    Kernel mode or User mode.
//
//------------------------------------------------------------------------------
#include "ksmips.h"
#include "nkintr.h"
#include "mipskpg.h"

        .data

ILEx:           .word   SingleCoreILEX          // InterlockedExchange
ILCmpEx:        .word   SingleCoreILCMPEX       // InterlockedCompareExchange
ILTstEx:        .word   SingleCoreILTSTEX       // InterlockedTestExchange
ILInc:          .word   SingleCoreILInc         // InterlockedIncrement
ILDec:          .word   SingleCoreILDec         // InterlockedDecrement
ILXAdd:         .word   SingleCoreILXAdd        // InterlockedExchangeAdd
ReadBarrier:    .word   ssNoOp                  // ReadBarrier
WriteBarrier:   .word   ssNoOp                  // WriteBarrier

        

        .text

        LEAF_ENTRY(__ILockAPIs__)
        .set    noreorder

//---------------------------------------------------------------------------------------
//      Single-core implementation of Interlocked functions
//---------------------------------------------------------------------------------------
SingleCoreILEX:
        la      t0, ilexRestart
ilexRestart:
        addiu   k1, t0, 3 * 4           // (k1) = address at "j ra", indicate interlocked
                                        //        operation in progress
        lw      v0, (a0)                // (v0) = original contents
        sw      a1, (a0)                // store new contents
        
        j       ra                      
        move    k1, zero                // (delay slot) interlocked operation complete

SingleCoreILCMPEX:
        la      t0, icexRestart
icexRestart:
        addiu   k1, t0, 5 * 4           // (k1) = &ICExDone, indicate interlocked
                                        //        operation in progress
        lw      v0, (a0)                // (v0) = original contents
        bne     v0, a2, icexDone             
        nop                             
        sw      a1, (a0)                // store new contents
icexDone:        
        j       ra                      
        move    k1, zero                // (delay slot) interlocked operation complete

SingleCoreILTSTEX:
        la      t0, itexRestart
itexRestart:
        addiu   k1, t0, 5 * 4           // (k1) = &ITExDone, indicate interlocked
                                        //        operation in progress
        lw      v0, (a0)                // (v0) = original contents
        bne     v0, a1, itexDone             
        nop                             
        sw      a2, (a0)                // store new contents
itexDone:        
        j       ra                      
        move    k1, zero                // (delay slot) interlocked operation complete

SingleCoreILInc:
        la      t0, iincRestart
iincRestart:
        addiu   k1, t0, 4 * 4           // (k1) = address at "j ra", indicate interlocked
                                        //        operation in progress
        lw      v0, (a0)                // (v0) = original contents
        addu    v0, 1        
        sw      v0, (a0)                // store new contents
        
        j       ra                      
        move    k1, zero                // (delay slot) interlocked operation complete
        
SingleCoreILDec:
        la      t0, idecRestart
idecRestart:
        addiu   k1, t0, 4 * 4           // (k1) = address at "j ra", indicate interlocked
                                        //        operation in progress
        lw      v0, (a0)                // (v0) = original contents
        subu    v0, 1                   
        sw      v0, (a0)                // store new contents
        
        j       ra                      
        move    k1, zero                // (delay slot) interlocked operation complete

SingleCoreILXAdd:
        la      t0, ixaddRestart
ixaddRestart:
        addiu   k1, t0, 4 * 4           // (k1) = address at "j ra", indicate interlocked
                                        //        operation in progress
        lw      v0, (a0)                // (v0) = original contents
        addu    t1, v0, a1              // (t1) = incremented value
        sw      t1, (a0)                // store new contents
        
        j       ra                      
        move    k1, zero                // (delay slot) interlocked operation complete


//---------------------------------------------------------------------------------------
//      Multi-core implementation of Interlocked functions
//---------------------------------------------------------------------------------------
MultiCoreILEX:
        move    t0, a1                  
        ll      v0, (a0)                // (v0) = original contents
        sc      t0, (a0)                // store new contents
        beqz    t0, MultiCoreILEX       // check if sc was successful
        nop
        j       ra
        nop

MultiCoreILCMPEX:
        ll      v0, (a0)                // (v0) = original contents
        bne     v0, a2, 10f             
        move    t0, a1                  // (delay slot) (t0) = new value
        sc      t0, (a0)                // store new contents
        beqz    t0, MultiCoreILCMPEX    // check if sc was successful
        nop
10:     j       ra
        nop

MultiCoreILTSTEX:
        ll      v0, (a0)                // (v0) = original contents
        bne     v0, a1, 10f             
        move    t0, a2                  // (delay slot) (t0) = new value
        sc      t0, (a0)                // store new contents
        beqz    t0, MultiCoreILTSTEX    // check if sc was successful
        nop
10:     j       ra
        nop
        
MultiCoreILInc:
        ll      v0, (a0)                // (v0) = original contents
        addu    t0, v0, 1               // (t0) = original contents + 1
        sc      t0, (a0)                // store new contents
        beqz    t0, MultiCoreILInc      // check if sc was successful
        nop
        j       ra
        addu    v0, 1                   // (delay slot) (v0) = original contents + 1

MultiCoreILDec:
        ll      v0, (a0)                // (v0) = original contents
        subu    t0, v0, 1               // (t0) = original contents - 1
        sc      t0, (a0)                // store new contents
        beqz    t0, MultiCoreILDec      // check if sc was successful
        nop
        j       ra
        subu    v0, 1                   // (delay slot) (v0) = original contents - 1

MultiCoreILXAdd:
        ll      v0, (a0)                // (v0) = original contents
        addu    t0, v0, a1              // (t0) = incremented value
        sc      t0, (a0)                // store new contents
        beqz    t0, MultiCoreILXAdd     // check if sc was successful
        nop
ssNoOp:
        j       ra
        nop


        ALTERNATE_ENTRY(InterlockedExchange)
        lw      t0, ILEx
        j       t0
        nop
        
        ALTERNATE_ENTRY(InterlockedCompareExchange)
        lw      t0, ILCmpEx
        j       t0
        nop
        ALTERNATE_ENTRY(InterlockedTestExchange)
        lw      t0, ILTstEx
        j       t0
        nop
        ALTERNATE_ENTRY(InterlockedIncrement)
        lw      t0, ILInc
        j       t0
        nop
        ALTERNATE_ENTRY(InterlockedDecrement)
        lw      t0, ILDec
        j       t0
        nop
        ALTERNATE_ENTRY(InterlockedExchangeAdd)
        lw      t0, ILXAdd
        j       t0
        nop
        ALTERNATE_ENTRY(DataSyncBarrier)
        lw      t0, WriteBarrier
        j       t0
        nop
        ALTERNATE_ENTRY(DataMemoryBarrier)
        lw      t0, ReadBarrier
        j       t0
        nop

        ALTERNATE_ENTRY(InitInterlockedFunctions)

        lw      t0, NUM_CPUS
        subu    t0, 1
        beqz    t0, DoneInit
        nop

        // more than one CPUs, use multi-core version
        la      t0, MultiCoreILEX
        la      t1, MultiCoreILCMPEX
        la      t2, MultiCoreILTSTEX
        la      t3, MultiCoreILInc
        la      t4, MultiCoreILDec
        la      t5, MultiCoreILXAdd

        sw      t0, ILEx
        sw      t1, ILCmpEx
        sw      t2, ILTstEx
        sw      t3, ILInc
        sw      t4, ILDec
        sw      t5, ILXAdd
        
DoneInit:
        j       ra
        nop
        
        .end __ILockAPIs__


