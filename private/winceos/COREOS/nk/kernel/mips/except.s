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
//      TITLE("Interrupt and Exception Processing")
//------------------------------------------------------------------------------
//
//
// Module Name:
//
//    except.s
//
// Abstract:
//
//    This module implements the code necessary to field and process MIPS
//    interrupt and exception conditions.
//
//    WARNING: This module executes in KSEG0 and, in general, cannot
//       tolerate a TLB Miss. Registers k0 and k1 are used during initial
//   interrupt and exception processing, and therefore, extreme care must
//   be exercised when modifying this module.
//
//------------------------------------------------------------------------------

#define ASM_ONLY
#include "apicall.h"
#include "ksmips.h"
#include "nkintr.h"
#include "mipskpg.h"
#include "vmlayout.h"
#include "vmmips.h"
#include "psyscall.h"
#include "oemglobal.h"

#define VR5432_BP_BUG 1

#define PERFORMCALLBACK         -30         // iMethod of PerformCallBack

#define MASK_ALL_INTERRUPT      0xfc00      // to masking all hardware interrupts

#if defined(VR5432_BP_BUG)
    //
    // An external routine calling into this code is at risk for
        // this bug if there is a conditional branch close to the call
        // and interrupts are enabled at the time of the branch
        //
    // MAKE SURE THE LABEL 199 IS NOT USED IN THIS FILE
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


#define SAVE_VOLATILE_REGS(RegPth, RegSp, RegPsr)   \
        sw      ra, TcxFir(RegPth);         \
        sw      RegPsr, TcxPsr(RegPth);     \
        S_REG   RegSp, TcxIntSp(RegPth);    \
        S_REG   s0, TcxIntS0(RegPth);       \
        S_REG   s1, TcxIntS1(RegPth);       \
        S_REG   s2, TcxIntS2(RegPth);       \
        S_REG   s3, TcxIntS3(RegPth);       \
        S_REG   s4, TcxIntS4(RegPth);       \
        S_REG   s5, TcxIntS5(RegPth);       \
        S_REG   s6, TcxIntS6(RegPth);       \
        S_REG   s7, TcxIntS7(RegPth);       \
        S_REG   s8, TcxIntS8(RegPth);       \
        S_REG   gp, TcxIntGp(RegPth);       \
        sw      zero, TcxContextFlags(RegPth);
        
        .text

PosTable:
        .byte 0,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1
        .byte 6,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1
        .byte 7,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1
        .byte 6,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1
        .byte 8,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1
        .byte 6,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1
        .byte 7,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1
        .byte 6,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LEAF_ENTRY (GetHighPos)
        .set noreorder
        
        addi    t0, zero, -1
        andi    t1, a0, 0xff
        lb      v0, PosTable(t1)
        bne     zero, v0, res
        srl     a0, a0, 8
        
        addi    t0, t0, 8
        andi    t1, a0, 0xff
        lb      v0, PosTable(t1)
        bne     zero, v0, res
        srl     a0, a0, 8
        
        addi    t0, t0, 8
        andi    t1, a0, 0xff
        lb      v0, PosTable(t1)
        bne     zero, v0, res
        srl     a0, a0, 8
        
        addi    t0, t0, 8
        andi    t1, a0, 0xff
        lb      v0, PosTable(t1)
        bne     zero, v0, res
        nop
        
        addi    v0, v0, 9
res:
        j       ra
        add     v0, v0, t0
        
        .end GetHighPos


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LEAF_ENTRY(INTERRUPTS_ON)
        .set noreorder
        
        lw      t0, BASEPSR
        nop
        or      t0, 1
        mtc0    t0, psr
        j       ra
        ehb        // (delay slot) hazzard barrier
        
        .end INTERRUPTS_ON

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LEAF_ENTRY(INTERRUPTS_OFF)
        .set noreorder
        
        CP0_STOP_PREFETCH(mtc0, zero, psr, t0);
        nop         // 3 cycle delay
        j       ra
        ehb
        
        .end INTERRUPTS_OFF


//------------------------------------------------------------------------------
// Enable interrupts if input parameter is non-zero (previously had interrupts on)
//------------------------------------------------------------------------------
LEAF_ENTRY(INTERRUPTS_ENABLE)
        .set noreorder

        CP0_STOP_PREFETCH(mfc0, v0, psr, t0);

        sltu    a0, zero, a0            // (a0) = 1 if non-zero, 0 otherwise
        move    t0, v0                  // (t0) = current PSR

        // clear IE bit of t0
        srl     t0, 1
        sll     t0, 1

        // update IE bit of t0 based on the argument
        add     t0, t0, a0

        // update PSR
        mtc0    t0, psr

        // hazzard barrier
        ehb
        
        // return
        j       ra
        and     v0, v0, 1               // (v0) = 1 if interrupt was enabled, 0 otherwise. (delay slot)

        .end INTERRUPTS_ENABLE

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
        NESTED_ENTRY(NKCallIntChain, 0, zero)
        subu    sp, sp, 6*REG_SIZE              // reserve space on stack, 4 extra register save area per C calling convention
        S_REG   ra, 4*REG_SIZE(sp)              // save return address
        PROLOGUE_END
        
        jal     NKCallIntChainWrapped
        nop

        L_REG   ra, 4*REG_SIZE(sp)              // restore return address
        
        j       ra                              // return
        addu    sp, sp, 6*REG_SIZE              // (delay slot) reclaim stack
        .end NKCallIntChain

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LEAF_ENTRY(SetCPUHardwareWatch)
        .set noreorder
        
        lui     t0, 0x1fff
        ori     t0, t0, 0xfff8
        and     a0, a0, t0
        or      a0, a0, a1
        CP0_STOP_PREFETCH(mtc0, a0, $18, t0);
        nop
        nop
        mtc0    zero, $19
        nop
        nop
        j       ra
        ehb                 // (delay slot) hazzard barrier
        
        .end SetCPUHardwareWatch


LEAF_ENTRY(MIPSSetASID)
        
#if ENTRYHI_PID != 0
        sll     a0, ENTRYHI_PID
#endif
        CP0_STOP_PREFETCH(mtc0, a0, entryhi, t0);    // set ASID

        j ra
        ehb

        .end MIPSSetASID


        .struct 0
KCF_A0:         .space  REG_SIZE
KCF_A1:         .space  REG_SIZE
KCF_A2:         .space  REG_SIZE
KCF_A3:         .space  REG_SIZE
KCF_RA:         .space  REG_SIZE
KCF_SP:
KCF_LENGTH:

//------------------------------------------------------------------------------
//++
// The following code is never executed. Its purpose is to support unwinding
// through the call to the exception dispatcher.
//------------------------------------------------------------------------------
NESTED_ENTRY(xKCall, 0, zero)
        .set    noreorder
        .set    noat
        sw      sp,KCF_LENGTH(sp) // save stack pointer
        sw      ra,KCF_RA(sp)     // save return address
        PROLOGUE_END


//------------------------------------------------------------------------------
//
// KCall - call kernel function
//
//  KCall invokes a kernel function in a non-preemtable state by incrementing
// the kernel nest level and switching onto a kernel stack.
//
//  While in a preemtible state, the thread's register save area is
// volatile. On the way in, nothing can be saved into the thread
// structure until KNest is set and on the way out anything needed from the
// thread structure must be loaded before restoring KNest.
//
//  The sequence of stack switching must be handled carefully because
// whenever KNest != 1, the general exception handler assumes that the kernel
// stack is current and will not switch stacks. On the way in, we must switch
// to the kernel stack before setting KNest but not use it until after KNest
// is set.  On the way out, we must reset KNest before restoring the thread's
// stack pointer.
//
//  Entry   (a0) = ptr to function to call
//      (a1) = first function arg
//      (a2) = second fucntion arg
//      (a3) = third function arg
//  Exit    (v0) = function return value
//  Uses    v0, a0-a3, t0-t9
//
//------------------------------------------------------------------------------
ALTERNATE_ENTRY(KCall)

        .set    noreorder
        move    t0, a0                  // (t0) = ptr to function to call
        lh      t1, ReschedFlag         // (t1) = resched flag + nest level
        move    a0, a1                  // ripple args down
        move    a1, a2
        subu    t1, 256
        bltz    t1, nestedkcall         // nested kernel call
        move    a2, a3
//
// Entering non-preemptible state. We must switch onto the kernel stack
// before setting KNest in case an interrupt occurs during the switch.
//
        move    t2, sp                  // (t2) = original stack pointer
        la      sp, KStack              // switch to kernel stack
        sh      t1, ReschedFlag         // enter non-preemtible state
        sw      ra, KCF_RA(sp)          // thread will resume at return address
        jal     t0
        sw      t2, KCF_SP(sp)          // save thread's stack pointer

//
// Function complete. Return to preemtible state after checking if a reschedule
// is needed.
//
        mtc0    zero, psr
        nop
        nop
        ehb                             // hazzard barrier


        lw      t8, ownspinlock         // (t8) = ppcb->ownspinlock
        lh      t9, ReschedFlag         // (t9) = resched flag + nest level
        lw      ra, KCF_RA(sp)          // restore ra

        bne     t8, zero, 18f           // no reschedule if we own a spinlock
        li      t1, 1                   // (delay slot) (t1) = 1
        
        beq     t9, t1, 20f             // reschedule needed
        lw      t2, dwKCRes             // (delay slot) (t2) = ppcb->dwKCRes
        
        bne     t2, zero, 20f           // reschedule needed
        nop

18:
        lw      t2, KCF_SP(sp)          // (t2) = original stack pointer
        addu    t9, 256
        sh      t9, ReschedFlag         // leave non-preemtible state

        lw      t9, BASEPSR
        or      t9, 1
#ifdef MIPS_HAS_FPU
        lw      t8, PCBCurFPUOwner
        lw      t7, CurThdPtr
        bne     t7, t8, 19f
        lw      t8, CoProcBits          // (delay slot) (t8) = co-proc enable bits 
        or      t9, t8
19:
#endif
        mtc0    t9, psr
        ehb                             // hazzard barrier

        j       ra
        move    sp, t2                  // restore stack pointer

//
// ReschedFlag set, so must run the scheduler to find which thread
// to dispatch next.
//
//  (v0) = KCall return value
//
20:
        lw      t9, BASEPSR
        or      t9, 1
        mtc0    t9, psr
        
        lw      t0, CurThdPtr           // (t0) = ptr to current thread
        lw      t2, KCF_SP(sp)          // (t2) = original stack pointer
        li      t3, KERNEL_MODE         // thread resume in kernel mode
        S_REG   v0, TcxIntV0(t0)        // save return value

        SAVE_VOLATILE_REGS(t0, t2, t3)

        j       resched
        move    s0, t0                  // (s0) = ptr to current thread struct


// Nested KCall. Just invoke the function directly.
//
//  (t0) = function address
//  (a0) = 1st function argument
//  (a1) = 2nd function argument
//  (a2) = 3rd function argument
//
nestedkcall:
        j       t0
        nop

        .set reorder
        .end xKCall


//-------------------------------------------------------------------------------
// release a spinlock and reschedule
//
// NOTE: Thread context must be saved before releasing the spinlock. For other CPU
//       can pickup the thread and start running it once spinlock is released.
//
// (a0) = spinlock to release
//-------------------------------------------------------------------------------
LEAF_ENTRY (MDReleaseSpinlockAndReschedule)

        .set    noreorder

        //
        // DEBUGCHK (!InPrivilegeCall () && (1 == GetPCB ()->ownspinlock));
        //
        lw      t0, CurThdPtr           // (t0) = ptr to current thread
        li      t3, KERNEL_MODE         // thread resume in kernel mode

        // switch to KStack and decrement cNest
        move    t2, sp
        la      sp, KStack              // switch to kernel stack
        sb      zero, KNest             // decrement ppcb->cNest (since it's 1 at the entrance, simply set it to 0)
        lw      a1, next_owner (a0)     // (a1) = psplock->next_owner

        SAVE_VOLATILE_REGS(t0, t2, t3)

        //
        // thread context saved, ready to release spinlock
        //
        // NOTE: reference to pCurThread is not allowed after this point.
        //
        //      (a0) = psplock
        //      (a1) = psplock->next_owner
        //
        sw      a1, owner_cpu(a0)       // psplock->owner_cpu = psplock->next_owner
        sw      zero, ownspinlock       // ppcb->ownspinlock  = 0

        j       resched
        move    s0, zero                // (s0) = ptr to current thread struct == NULL

        .set reorder

        .end MDReleaseSpinlockAndReschedule

#ifdef MIPS_HAS_FPU

//------------------------------------------------------------------------------
// note: added extra nop after updating CU1 bit in the status register to support
// MIPS 5k cores.
//------------------------------------------------------------------------------
LEAF_ENTRY(GetAndClearFloatCode)
        .set    noreorder
        
        CP0_STOP_PREFETCH(mfc0, a1, psr, a2);
        lui     a2, 0x2000
        or      a1, a2
        mtc0    a1, psr
        nop
        nop
        nop
        ehb                             // hazzard barrier
        cfc1    v0, fsr
        
        li      t0, 0xfffc0fff
        and     t0, t0, v0
        
        ctc1    t0, fsr
        j       ra
        nop
        
        .end GetCode

//------------------------------------------------------------------------------
// note: added extra nop after updating CU1 bit in the status register to support
// MIPS 5k cores.
//------------------------------------------------------------------------------
LEAF_ENTRY(SaveFloatContext)
        .set    noreorder

        CP0_STOP_PREFETCH(mfc0, t0, psr, t1);
        lui     t1, 0x2000
        or      t0, t1
        mtc0    t0, psr
        nop
        nop
        nop
        ehb                             // hazzard barrier
        cfc1    t0, fsr
        sw      t0, TcxFsr(a0)

        // MIPSII - use sdc1 on all even number FP registers
        sdc1    f0, TcxFltF0(a0)
        sdc1    f2, TcxFltF2(a0)
        sdc1    f4, TcxFltF4(a0)
        sdc1    f6, TcxFltF6(a0)
        sdc1    f8, TcxFltF8(a0)
        sdc1    f10, TcxFltF10(a0)
        sdc1    f12, TcxFltF12(a0)
        sdc1    f14, TcxFltF14(a0)
        sdc1    f16, TcxFltF16(a0)
        sdc1    f18, TcxFltF18(a0)
        sdc1    f20, TcxFltF20(a0)
        sdc1    f22, TcxFltF22(a0)
        sdc1    f24, TcxFltF24(a0)
        sdc1    f26, TcxFltF26(a0)
        sdc1    f28, TcxFltF28(a0)
        sdc1    f30, TcxFltF30(a0)
#ifdef MIPSIV
        // MIPSIV - also save the odd number FP registers
        sdc1    f1, TcxFltF1(a0)
        sdc1    f3, TcxFltF3(a0)
        sdc1    f5, TcxFltF5(a0)
        sdc1    f7, TcxFltF7(a0)
        sdc1    f9, TcxFltF9(a0)
        sdc1    f11, TcxFltF11(a0)
        sdc1    f13, TcxFltF13(a0)
        sdc1    f15, TcxFltF15(a0)
        sdc1    f17, TcxFltF17(a0)
        sdc1    f19, TcxFltF19(a0)
        sdc1    f21, TcxFltF21(a0)
        sdc1    f23, TcxFltF23(a0)
        sdc1    f25, TcxFltF25(a0)
        sdc1    f27, TcxFltF27(a0)
        sdc1    f29, TcxFltF29(a0)
        sdc1    f31, TcxFltF31(a0)
#endif
        j ra
        nop
        .end SaveFloatContext

           
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LEAF_ENTRY(RestoreFloatContext)
        
        CP0_STOP_PREFETCH(mfc0, t0, psr, t1);
        lui     t1, 0x2000
        or      t0, t1
        mtc0    t0, psr
        nop
        nop
        ehb                             // hazzard barrier
        lw      t0, TcxFsr(a0)
        // For some reason unknown, bit 17 of FSR can be set, which
        // causes the ctc1 instruction to except. Mask bit 17 off the FSR
        // for now.
        li      t1, 0xfffdffff
        and     t0, t0, t1
        ctc1    t0, fsr
        // MIPSII - use sdc1 on all even number FP registers
        ldc1    f0, TcxFltF0(a0)
        ldc1    f2, TcxFltF2(a0)
        ldc1    f4, TcxFltF4(a0)
        ldc1    f6, TcxFltF6(a0)
        ldc1    f8, TcxFltF8(a0)
        ldc1    f10, TcxFltF10(a0)
        ldc1    f12, TcxFltF12(a0)
        ldc1    f14, TcxFltF14(a0)
        ldc1    f16, TcxFltF16(a0)
        ldc1    f18, TcxFltF18(a0)
        ldc1    f20, TcxFltF20(a0)
        ldc1    f22, TcxFltF22(a0)
        ldc1    f24, TcxFltF24(a0)
        ldc1    f26, TcxFltF26(a0)
        ldc1    f28, TcxFltF28(a0)
        ldc1    f30, TcxFltF30(a0)
#ifdef MIPSIV
        // MIPSIV - also save the odd number FP registers
        ldc1    f1, TcxFltF1(a0)
        ldc1    f3, TcxFltF3(a0)
        ldc1    f5, TcxFltF5(a0)
        ldc1    f7, TcxFltF7(a0)
        ldc1    f9, TcxFltF9(a0)
        ldc1    f11, TcxFltF11(a0)
        ldc1    f13, TcxFltF13(a0)
        ldc1    f15, TcxFltF15(a0)
        ldc1    f17, TcxFltF17(a0)
        ldc1    f19, TcxFltF19(a0)
        ldc1    f21, TcxFltF21(a0)
        ldc1    f23, TcxFltF23(a0)
        ldc1    f25, TcxFltF25(a0)
        ldc1    f27, TcxFltF27(a0)
        ldc1    f29, TcxFltF29(a0)
        ldc1    f31, TcxFltF31(a0)
#endif
        j ra
        nop
        .end RestoreFloatContext
#endif

           
           
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LEAF_ENTRY(FirstSchedule)
        .set noreorder

        lw      s1, BASEPSR
        ori     s1, ( 1 << PSR_IE )     //  Enable interrupts.
        mtc0    s1, psr
        nop
        
        move    s0, zero                // no current running thread
        li      t0, 1
        sw      t0, dwKCRes             // force reschedule
        j       resched
        nop
        
        .end    FirstSchedule



//------------------------------------------------------------------------------
//
// The following code is never executed. Its purpose is to support unwinding
// through the call to the exception dispatcher.
//
//------------------------------------------------------------------------------
NESTED_ENTRY(GeneralExceptionP, 0, zero)
        .set    noreorder
        .set    noat
        S_REG   sp,TcxIntSp(sp)         // save stack pointer
        S_REG   ra,TcxIntRa(sp)         // save return address
        sw      ra,TcxFir(sp)           // save return address
        sw      gp,TcxIntGp(sp)         // save integer register gp
        sw      s0,TcxIntS0(sp)         // save S0
        move    s0, sp                  // set pointer to thread
        S_REG   gp,TcxIntGp(sp)         // save integer register gp
        S_REG   s0,TcxIntS0(sp)         // save S0
        move    s0, sp                  // set pointer to thread
        PROLOGUE_END

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
ALTERNATE_ENTRY(GeneralException)
        .set    noreorder
        .set    noat
            
        mfc0    k0, epc                 // (k0) = resume address
#if NO_LL
        bne     zero, k1, 30f           // Check if interlock operation is in progress
        nop
#endif

1:      S_REG   t0, SaveT0              // get a working register
        lw      t0, dwPslTrapSeed       // (t0) = ppcb->dwPslTrapSeed
        xor     t0, t0, k0              // (t0) = API encoding
        li      k1, LAST_METHOD
        subu    k1, t0, k1              // (k1) = API encoding - LAST_METHOD
        li      t0, MAX_METHOD_NUM-LAST_METHOD  // t0 = max value k1 can be for an API call
        sltu    k1, t0, k1              // (k1) = ((MAX_METHOD_NUM-LAST_METHOD) < (epc - LAST_METHOD))
        beqz    k1, 200f                //      == 0 iff API call
        mfc0    k1, cause               // (delay slot) (k1) = exception cause

        // this doesn't make sense
        // exception b is "co-proc not usable, and are used for FPU handling
        // Why ignore it???
        lw      t0, ArchFlags           // (t0) = g_pKData->dwArchFlags
        andi    t0, MIPS_FLAG_IGNORE_COPROC_UNUSABLE    // ignore exception b?
        beqz    t0, 3f
        andi    t0, k1, 0xff            // (in delay slot, but okay) ignore exception b's - We're getting extraneous exception B's
        xori    t0, t0, 0x2c            // ignore exception b's - ignore them
        beq     t0, zero, 140f          // ignore exception b's - 
//
// The read and write miss codes differ by exactly one bit such that they
// can be tested for by a single mask operation followed by a test for the
// read miss code.
//
3:
        and     t0,k1,MISS_MASK         // isolate exception code
        xori    t0,XCODE_READ_MISS      // get exception code for read miss
        beq     t0, zero, 100f          // read or write TLB miss

5:
        // Unhandled TLB miss jump back to here
        // (k0) = epc
        // (k1) = cause
        //  
        lb      t0, KNest               // (delay slot) (t0) = reentrancy count
        subu    t0, 1
        bne     zero, t0, 35f           // nested exception
        sb      t0, KNest
        lw      t0, CurThdPtr           // (t0) = ptr to current thread
        S_REG   sp, TcxIntSp(t0)
        S_REG   gp, TcxIntGp(t0)
        la      sp, KStack
//
// Reenter here from nested exception setup.
//
10:     sw      k0, TcxFir(t0)
        S_REG   s0, TcxIntS0(t0)
        move    s0, t0                  // (s0) = ptr to thread structure
        S_REG   v0, TcxIntV0(s0)
        S_REG   a0, TcxIntA0(s0)
//
// ISR is now 'cooked'. i.e. ISR can now be written in C, instead of assembly only.
// save all volatile registers..
//
        S_REG   v1, TcxIntV1(s0)
        L_REG   t0, SaveT0              // restore T0
        S_REG   t1, TcxIntT1(s0)
        S_REG   t2, TcxIntT2(s0)
        S_REG   t3, TcxIntT3(s0)
        S_REG   t0, TcxIntT0(s0)
        mfhi    t1
        mflo    t2
        S_REG   t4, TcxIntT4(s0)
        S_REG   t5, TcxIntT5(s0)
        S_REG   t6, TcxIntT6(s0)
        S_REG   t7, TcxIntT7(s0)
        and     t0, k1, XCODE_MASK
        S_REG   t8, TcxIntT8(s0)
        S_REG   t9, TcxIntT9(s0)
        S_REG   t1, TcxIntHi(s0)        // save HI mul/div register
        S_REG   t2, TcxIntLo(s0)        // save LO mul/div register
        S_REG   a2, TcxIntA2(s0)
        S_REG   ra, TcxIntRa(s0)        // save return address
        S_REG   AT, TcxIntAt(s0)
        S_REG   a3, TcxIntA3(s0)
        bne     t0, zero, 40f           // not a h/w interrupt
        S_REG   a1, TcxIntA1(s0)        // (delay slot) save a1

        .set at
        lw      a0, BASEPSR             // (a0) = interrupt mask

        subu    sp, 6*REG_SIZE          // reserver space
                                        // for 4 registers per C calling
                                        // convention

        and     a0, k1                  // (a0) = bits 15-10 pending interrupts
        srl     a0, CAUSE_INTPEND+2
        and     a0, 0x3F                // (a0) = hw interrupt cause bits
        lw      a1, g_pIntrPrio         // (a1) = ptr to IntrPrio array
        move    k1, zero                // reset atomic op. flag
        addu    a1, a0                  // (a1) = &IntrPrio[pending_int]
        lb      a0, (a1)                // (a0) = highest pri interrupt * 4
        mfc0    k0, psr
        lw      v0, ISR_TABLE(a0)       // (v0) = interrupt handler
        
        sw      k0, TcxPsr(s0)
        
        lw      a1, g_pIntrMask         // (a1) = ptr to IntrMask array
        sra     a0, 2                   // Keep the sign for spurious interrupts
        addi    a0, 1
        addu    a1, a0                  // (a1) = &IntrMask[highest_prio_int]
        lbu     a0, (a1)
        sll     a0, 10
        
        not     a0
        and     k0, k0, a0
        mtc0    k0, psr

        
        // clear User mode, EXL & ERL bits in PSR
        ori     k0, (1<<PSR_EXL) | (1<<PSR_ERL) | (1<<PSR_PMODE)
        xori    k0, (1<<PSR_EXL) | (1<<PSR_ERL) | (1<<PSR_PMODE)


        //===================================================================
        //
        // Log the ISR entry event to CeLog
        //
        
        // NOTE : Since we need to allow TLB misses, we must assure that we have
        //        don't rely on registers that can be changed in TLB miss handler.

        // Skip logging if KInfoTable[KINX_CELOGSTATUS] == 0
        lw      a0, CELOG_STATUS         // (a0) = KInfoTable[KINX_CELOGSTATUS]
        beq     zero, a0, GE_NoCeLogISREntry
        nop
        
        // save v0 (function to call), and k0 (psr)
        S_REG   v0, 4*REGSIZE(sp)
        S_REG   k0, 5*REGSIZE(sp)

        // mask all hardware interrupts, but enable interrupts such that we can handle
        // TLB miss correctly
        ori     a1, k0, MASK_ALL_INTERRUPT
        xori    a1, a1, MASK_ALL_INTERRUPT
        mtc0    a1, psr
        ehb                             //Hazard barrier after disabling interrupt

        jal     CELOG_Interrupt
        li      a0, 0x80000000          // delay slot (a0 = mark as ISR entry)

        // restore v0 (function to call), and k0 (psr)
        L_REG   v0, 4*REGSIZE(sp)
        L_REG   k0, 5*REGSIZE(sp)

GE_NoCeLogISREntry:

        //===================================================================

        mfc0    a0, epc                 // (a0) = interrupted PC
        jal     v0                      // Call OEM ISR
        mtc0    k0, psr                 // (delay slot) enable interrupts

        // is it an IPI?
        // (v0) = sysintr
        li      t0, SYSINTR_IPI
        bne     v0, t0, NotifyOEM
        nop

        // call IPI Handler
        jal     HandleIpi
        nop
        
NotifyOEM:
        // call the ISR hook (pfnOEMIntrOccurs is set to a faked function if not changed by OEM)
        jal     OEMNotifyIntrOccurs     // call pfnOEMIntrOccurs
        move    a0, v0                  // (delay slot) (a0) = SYSINTR == parameter to pfnOEMIntrOccurs
        
        mfc0    a1, psr


        //===================================================================
        //
        // Log the ISR exit event to CeLog
        //
        
        // NOTE : The only volatile register that we need to save/restore is v0 since it holds 
        //        the return value of ISR.

        // Skip logging if KInfoTable[KINX_CELOGSTATUS] == 0
        lw      a0, CELOG_STATUS         // (a0) = KInfoTable[KINX_CELOGSTATUS]
        beq     zero, a0, GE_NoCeLogISRExit
        nop
        
        // mask all hardware interrupts, but enable interrupts such that we can handle
        // TLB miss correctly
        ori     a2, a1, MASK_ALL_INTERRUPT
        xori    a2, a2, MASK_ALL_INTERRUPT
        mtc0    a2, psr
        ehb                             //Hazard barrier after masking interrupt

        // save v0 (SYSINTR value), and a1 (psr)
        S_REG   v0, 4*REGSIZE(sp)
        S_REG   a1, 5*REGSIZE(sp)

        // prepare arguments
        lb      a0, KNest               // (a0) = nest level (0, -1, -2, etc)
        sub     a0, zero, a0            // (a0) = nest level (0,  1,  2, etc)
        sll     a0, a0, 16              // (a0) = (a0) << 16

        jal     CELOG_Interrupt
        or      a0, a0, v0              // delay slot (a0 = (-KNest << 16) | SYSINTR)

        // restore v0 (SYSINTR value), and a1 (psr)
        L_REG   v0, 4*REGSIZE(sp)
        L_REG   a1, 5*REGSIZE(sp)

GE_NoCeLogISRExit:

        //===================================================================


        ori     a1, (1<<PSR_IE)
        xori    a1, (1<<PSR_IE)
        mtc0    a1, psr                 // clear interrupt enable
        lw      a1, TcxPsr(s0)
        ehb                             // hazard barrier after clearing interrupt enable
        mtc0    a1, psr                 // now restore real psr, also toggling interrupt enable
        ehb                             // hazard barrier after changing psr
        
        .set noat




        addu    sp, 6*REG_SIZE          // reclaim the stack space
        
        
#if SYSINTR_NOP != 0
 #error
#endif
        beq     v0, zero, 20f           // return == SYSINTR_NOP
        li      a3, SYSINTR_BREAK
        beq     v0, a3, 25f             // debug break interrupt
        subu    v0, SYSINTR_DEVICES
        bltz    v0, 15f                 // not device signal, treat as reschedule
        sltu    a3, v0, SYSINTR_MAX_DEVICES
        beq     a3, zero, 20f           // out of range, ignore
        lw      a1, PendEvents1         // (a1) = PendEvent1

// The ISR has requested that an interrupt event be signalled

        subu    a3, v0, 32              // (a3) = (ISR requested - 32)
        bgez    a3, 27f                 // handle ISR requested >= 32 if (a3) >= 0
        li      a2, 1                   // (delay slot) (a2) = 1
        
        // ISR requested < 32
        sllv    a3, a2, v0              // (a3) = 1 << ISR#
        or      a1, a1, a3              // (a1) = PendEvents1 | (1 << ISR#)
        sw      a1, PendEvents1         // Update PendEvents1
                
15:     lb      v0, ReschedFlag
        or      v0, 1
        sb      v0, ReschedFlag         // set reschedule flag

// Interrupt processing is complete. If the reschedule flag has been set and
// this is not a nested exception, then save the full thread context and
// obtain a new thread to run.

20:     lh      k0, ReschedFlag         // (k0) = resched + nested exception
        li      a0, 1
        beq     k0, a0, 41f             // reschedule needed
        move    k1, zero                // (k1) = 0 (no atomic op in progress)
        addu    k0, 256                 // remove one level of nesting
        sh      k0, ReschedFlag         // save updated nest info
        L_REG   t0, TcxIntHi(s0)        // save HI mul/div register
        L_REG   t1, TcxIntLo(s0)        // save LO mul/div register
        L_REG   AT, TcxIntAt(s0)
        L_REG   ra, TcxIntRa(s0)
        L_REG   a0, TcxIntA0(s0)        
        L_REG   a1, TcxIntA1(s0)
        L_REG   a2, TcxIntA2(s0)
        L_REG   a3, TcxIntA3(s0)
        mthi    t0
        mtlo    t1
        lw      k0, TcxFir(s0)          // (k0) = exception return address
        L_REG   v0, TcxIntV0(s0)        // restore return value
        L_REG   v1, TcxIntV1(s0)
        L_REG   t0, TcxIntT0(s0)
        L_REG   t1, TcxIntT1(s0)
        L_REG   t2, TcxIntT2(s0)
        L_REG   t3, TcxIntT3(s0)
        L_REG   t4, TcxIntT4(s0)
        L_REG   t5, TcxIntT5(s0)
        L_REG   t6, TcxIntT6(s0)
        L_REG   t7, TcxIntT7(s0)
        L_REG   t8, TcxIntT8(s0)
        L_REG   t9, TcxIntT9(s0)

        mtc0    k0, epc                 // restore EPC to interrupted stream
                                        
        // MUST use lw instead of L_REG so sp get automatic sign extension.
        // The reason is that we do arithmetics on SP during exception handling
        // and SP can become a positive 64 bit value.
        lw      sp, TcxIntSp(s0)        // restore stack pointer
        L_REG   gp, TcxIntGp(s0)
        L_REG   s0, TcxIntS0(s0)
        
        ehb                             // 3 cycle hazard on epc between mtc0 and eret
        eret                            // restore user status
        nop
        nop
        eret
//
// HW Break button pushed. Pass an exception to the debugger.
//
25:     li      k1, 1                   // (k1) = cause (1 for h/w break)
        j       41f                     // route into vanilla exception path
        move    a2, zero                // clear BadVAddr for exception handler


        // ISR # >= 32
        // (a2) = 1
        // (a3) = ISR# - 32
27:     lw      a1, PendEvents2         // (a1) = PendEvents2 
        sllv    a3, a2, a3              // (a3) = 1 << (ISR#-32)
        or      a1, a1, a3              // (a1) = PendEvents2 | 1 << (ISR#-32)
        b       15b                     // reschedule
        sw      a1, PendEvents2         // (delay slot) Update PendEvents2


//
// An atomic instruction sequence was interrupted. Check if we are past the
// store instruction and reset the EPC back to the starting point if not.
// Note that since we know what the interrupted stream is it is safe to
// trash certain registers.
//
// this code needs to be more careful about addressing user memory!
//
//  (k1) = address of "IXXXDone"
//  (k0) = exception PC
//  (t0) = beginning of Interlocked funciton
//
#if NO_LL
        //
        //      if ((k1 != 0) && (k1 != epc)) {
        //              epc = t0;       // actuall, change k0 which holds epc
        //      }

30:     beq     k0, k1, 1b              // If (epc == k1)
        nop                             //      done, don't restart

        //
        // Interlock operation in progress, reset restart address
        //
        b       1b                      
        move    k0, t0                  // (k0) = restart address
#endif

// A nested exception has occured. If this a h/w interrupt, create a temporary
// thread structure on the stack and save the current state into that.

35:     subu    t0, sp, TcxSizeof       // (t0) = ptr to dummy kernel thread
        S_REG   sp, TcxIntSp(t0)        // save current stack ptr
        move    sp, t0
        S_REG   gp, TcxIntGp(t0)
        j       10b
        sw      zero, ThPcstkTop(t0)    // clear call stack pointer

// Handle an exception which is not a h/w interrupt nor an API call.
//
//  (k1) = cause
//  (s0) = ptr to thread structure
//  all volatile registers saved

40:     mfc0    a2, badvaddr            // (a2) = fault address

// Interrupt reschedules enter here to save register context.
//
//  (k1) = exception cause (0 if resched, 1 if h/w break button)

41:     mfc0    k0, psr                 // (k0) = processor status
        move    a1, k1                  // (a1) = exception cause
        move    k1, zero                // no interlocked api in progress
        and     v0, k0, MODE_MASK       // (v0) = thread's mode

        // clear User mode, EXL & ERL bits in PSR
        ori     k0, (1<<PSR_EXL) | (1<<PSR_ERL) | (1<<PSR_PMODE)
        xori    k0, (1<<PSR_EXL) | (1<<PSR_ERL) | (1<<PSR_PMODE)

        mtc0    k0, psr                 // enable interrupts
        sw      v0, TcxPsr(s0)          // record the original mode in Thread struct
        S_REG   s1, TcxIntS1(s0)
        S_REG   s2, TcxIntS2(s0)
        S_REG   s3, TcxIntS3(s0)
        S_REG   s4, TcxIntS4(s0)
        S_REG   s5, TcxIntS5(s0)
        S_REG   s6, TcxIntS6(s0)
        S_REG   s7, TcxIntS7(s0)
        S_REG   s8, TcxIntS8(s0)
        li      t0, CONTEXT_CONTROL | CONTEXT_INTEGER
                
        beq     a1, zero, 45f
        sw      t0, TcxContextFlags(s0)
        jal     HandleException         // jump to handler
        move    a0, s0                  // (a0) = ptr to thread
        bne     v0, zero, 60f           // resume current thread
        nop

45:
        lw      t0, ownspinlock         // (t0) = psplock->ownspinlock
        bnez    t0, 60f                 // resume current thread if own a spinlock
//
//  The current thread has been blocked or a reschedule is pending.
//  Save the remaining CPU state and call the scheduler to obtain the
//  highest priority thread to run.
//
//  (s0) = CurThdPtr.
//
resched:
        //
        // DEBUGCHK (!ppcb->ownspinlock);
        //
        la      s2, g_schedLock         // (s2) = &g_schedLock
        jal     AcquireSpinLock         // grab scheduler lock
        move    a0, s2                  // (delay slot) (a0) = &g_schedLock

50:             
        lh      t0, ReschedFlag
        beq     zero, t0, 52f
        nop     
                
        jal     NextThread
        sh      zero, ReschedFlag       // clear reschedule, still in kernel
                
52:     lw      s1, dwKCRes
        beq     zero, s1, 53f
        nop     
                
        jal     KCNextThread
        sw      zero, dwKCRes           // clear KCreschedule
                
        lw      s1, dwKCRes
        bne     zero, s1, 50b
        nop     
                
53:             
        jal     ReleaseSpinLock         // release scheduler lock
        move    a0, s2                  // (delay slot) (a0) = &g_schedLock

        lw      v0, pthSched            // (v0) = ppcb->pthSched
                
        beq     zero, v0, Idle          // no threads to run, do idle processing
        nop     
        beq     s0, v0, 60f             // resume current thread
        move    s0, v0                  // (delay slot) (s0) = new current thread

        // thread switched
        jal     SetCPUASID              // call CPUASID
        move    a0, s0                  // (delay slot) argument to SetCPUASID is the new thread

        // update pCurThread, dwCurThId, and global TLS
        lw      t0, ThID(s0)            // (t0) = pCurThread->dwID
        lw      t1, ThTlsPtr(s0)        // (t1) = pCurThread->tlsPtr
        sw      s0, CurThdPtr           // pCurThread = (s0)
        sw      t0, dwCurThId           // dwCurThId = pCurThread->dwId
        sw      t1, lpvTls              // KVPTls = pCurThread->tlsPtr

// Restore the complete thread state.
//
//  (s0) = ptr to thread structure

60:     L_REG   s1, TcxIntS1(s0)        // Restore thread's permanent registers
        L_REG   s2, TcxIntS2(s0)
        L_REG   s3, TcxIntS3(s0)
        L_REG   s4, TcxIntS4(s0)
        L_REG   s5, TcxIntS5(s0)
        L_REG   s6, TcxIntS6(s0)
        lw      v0, TcxContextFlags(s0)
        L_REG   s7, TcxIntS7(s0)
        
        andi    v1, v0, CONTEXT_INTEGER & 0xFF
        beq     v1, zero, 63f
        L_REG   s8, TcxIntS8(s0)
        L_REG   t0, TcxIntHi(s0)        // (t0) = HI mul/div register
        L_REG   t1, TcxIntLo(s0)        // (t1) = LO mul/div register
        mthi    t0
        mtlo    t1
        L_REG   v1, TcxIntV1(s0)
        L_REG   t0, TcxIntT0(s0)
        L_REG   t1, TcxIntT1(s0)
        L_REG   t2, TcxIntT2(s0)
        L_REG   t3, TcxIntT3(s0)
        L_REG   t4, TcxIntT4(s0)
        L_REG   t5, TcxIntT5(s0)
        L_REG   t6, TcxIntT6(s0)
        L_REG   t7, TcxIntT7(s0)
        L_REG   t8, TcxIntT8(s0)
        L_REG   t9, TcxIntT9(s0)
        L_REG   AT, TcxIntAt(s0)
        L_REG   a0, TcxIntA0(s0)
        L_REG   a1, TcxIntA1(s0)
        L_REG   a2, TcxIntA2(s0)
        L_REG   a3, TcxIntA3(s0)    
        
63:
        CP0_STOP_PREFETCH(mtc0, zero, psr, v0);
        ehb                             // hazzard barrier
        L_REG   ra, TcxIntRa(s0)
        L_REG   v0, TcxIntV0(s0)        // restore return value

        // can't reschedule if we own a spinlock
        lw      k0, ownspinlock         // (k0) = ppcb->ownspinlock
        bnez    k0, noresched
        lh      k1, ReschedFlag         // (delay slot) (k1) = resched + nested exception

        // reschedule if reschedule pending
        li      k0, 1
        beq     k1, k0, 68f             // reschedule pending
        nop
        
noresched:
        // (k1) = resched + nested exception
        addu    k1, 256                 // remove one level of nesting
        sh      k1, ReschedFlag
        lw      k0, BASEPSR             // (k0) = global default status value
        lw      k1, TcxPsr(s0)          // (k1) = thread's default status
        // MUST use lw instead of L_REG so sp get automatic sign extension.
        // The reason is that we do arithmetics on SP during exception handling
        // and SP can become a positive 64 bit value.
        lw      sp, TcxIntSp(s0)        // restore stack pointer
        or      k1, k0                  // (k1) = thread + global status
#ifdef MIPS_HAS_FPU
        lw      k0, PCBCurFPUOwner
        bne     k0, s0, 66f
        lw      k0, CoProcBits          // (delay slot) (k0) = co-proc enable bits 
        or      k1, k0
66:
#endif
        mtc0    k1, psr                 // restore status
        lw      k0, TcxFir(s0)          // (k0) = exception return address
        L_REG   gp, TcxIntGp(s0)        
        L_REG   s0, TcxIntS0(s0)        // Restore thread's permanent registers
        mtc0    k0, epc                 // set continuation address for Eret
        move    k1, zero                // (k1) = 0 (no atomic op in progress)
        ssnop
        ssnop
        ehb                             // 3 cycle hazard on epc between mtc0 and eret
        eret                            // restore user status
        nop
        nop
        eret

//
// No threads to run. Check the resched flag and it is not set, 
// then call OEMIdle.
//
Idle:   
        CP0_STOP_PREFETCH(mtc0, zero, psr, v0); // all interrupts off
        nop                             //
        nop                             // 3 cycle hazard
        ehb                             // hazzard barrier
        lh      v0, ReschedFlag         // (v0) = resched + nested exception
        lw      a0, BASEPSR             // (a0) = global default status value
        bgtz    v0, 68f                 // reschedule pending
        nop                             
        jal     NKIdle                  // let OEM stop clocks, etc.
        nop
        li      v1, 1
        sb      v1, ReschedFlag

//
// Pending reschedule found during final dispatch, re-enable 
// interrupts and try again.
//
68:     lw      a0, BASEPSR             // (a0) = global default status value
        move    k1, zero                // (k1) = 0 (no atomic op in progress)
        ori     a0, 1 << PSR_IE         // (t0) = current status + int enable
        j       resched
        mtc0    a0, psr                 // re-enable interrupts

// TLB load or store exception. These exceptions are routed to the general
// exception vector either because the TLBMiss handler could not translate
// the virtual address or because there is a matching but invalid entry loaded
// into the TLB. If a TLB probe suceeds then, this is due to an invalid entry.
// In that case, the page tables are examined to find a matching valid entry
// for the page causing the fault. If a matching entry is found, then the TLB
// is updated and the instruction is restarted. If no match is found, the 
// exception is processed via the normal exception path.
//
//  (k0) = EPC (maybe updated for InterlockedXXX API)
//  (k1) = cause register
//  interrupted T0 saved in SaveT0.

100:
        mtc0    k0, epc                             // update epc
        andi    k1, k1, XCODE_MODIFY                // k1 = fWrite

        MFC_REG k0, badvaddr                        // (k0) = faulting virtual address
        
#ifdef  _MIPS64
        sll     t0, k0, 0                           // Sign extend the 32-bit address.
        bne     k0, t0, 160f                        // Invalid 32-bit address - generate exception.
#endif  //  _MIPS64

        ehb                                         // hazard barrier
        tlbp                                        // probe TLB (3 cycle hazzard)

        // if ((DWORD) badaddr < VM_SHARED_HEAP_BASE) {
        //      find mapping in current page directory
        // } else if (IsInKMode) {
        //      find mapping in kernel's page directory
        // } else if (!fWrite && ((int) badaddr > 0)) {
        //      find mapping in kernel's page directory
        // } else {
        //      fail
        // }

        li      t0, VM_SHARED_HEAP_BASE             // (delay slot if MIPS64) (t0) = VM_SHARED_HEAP_BASE
        sltu    t0, k0, t0                          // (t0) = ((DWORD) badvaddr < VM_SHARED_HEAP_BASE)
        bnez    t0, 110f                            // jump to FindMapping if true
        lw      t0, pProcVM                         // (delay slo) (t0) = current VM process

        // address above VM_SHARED_HEAP_BASE
        mfc0    t0, psr                             // (t0) = psr
        andi    t0, t0, 1 << PSR_PMODE              // (t0) = mode (0 iff in kmode)

        beqz    t0, 110f                            // jump to FindMapping if in k-mode
        lw      t0, pProcNK                         // (delay slo) (t0) = kernel process

        // in user mode, address above VM_SHARED_HEAP_BASE, only read from shared heap is allowed
        // (k1) = fWrite
        bnez    k1, 160f                            // fail if fWrite
        nop
        blez    k0, 160f                            // fail if k-mode address
        li      k1, VM_KMODE_BASE                   // (delay slot) (k1) set to 0x80000000 
        
110:    // FindMapping
        // (t0) = process whose VM to be used
        // (k0) = badvaddr
        // (k1) < 0     iff force read-only (read miss)
        //      >= 0    otherwise

        S_REG   t1, SaveK1                          // get an extra register to work with

        lw      t0, ProcPpdir(t0)                   // (t0) = pprc->ppdir
        srl     t1, k0, 20                          // (t1) = badaddr >> 20
        andi    t1, t1, OFFSET_MASK                 // (t1) = offset into page directory
        addu    t0, t1                              // (t0) = ptr to the pd entry
        lw      t0, (t0)                            // (t0) = page table

        bgez    t0, 150f                            // fail if not kernel address
        srl     k0, k0, 10                          // (delay slot) (k0) = badvaddr >> 10
        andi    k0, k0, OFFSET_MASK                 // (k0) = offset into page directory for the page
        addu    t1, t0, k0                          // (t1) = ptr to pt entry
        andi    k0, k0, EVEN_OFST_MASK              // (k0) = offset into page directory for the page

        lw      t1, (t1)                            // (t1) = pt entry for the faulted address

        addu    t0, k0                              // (t0) = ptr to even page
        andi    t1, t1, PG_VALID_MASK               // (t1) != 0 if page is valid
        beqz    t1, 150f                            // fail if not a valid page

        // valid page, fill TLB
        lw      k0, (t0)                            // (delay slot) (k0) = even page

        bgez    k1, 120f
        lw      t0, 4(t0)                           // (delay slot) (t0) = odd page

        // force read-only
        li      t1, ~PG_DIRTY_MASK
        and     k0, t1
        and     t0, t1
        
120:    // Found Valid entry in the page table
        // (k0): to entry for odd entry
        // (t0): entry for even entry

        // set the global bit for kernel addresses
        mfc0    t1, badvaddr
        slt     t1, t1, zero
        or      k0, t1
        or      t0, t1

        mfc0    k1, index               // (k1) = index
        mtc0    k0, entrylo0            // set even entry to write into TLB
        mtc0    t0, entrylo1            // set odd entry to write into TLB
        ssnop                           // 2 cycle hazard on index, entrylo0 and entrylo1 between mtc0 and tlbwi
        ehb                             // hazzard barrier
        bltz    k1, 130f                // index invalid, write random
        L_REG   t1, SaveK1              // (delay slot) restore t1
        tlbwi                           // write indexed entry into TLB
        b       140f                    // 3 cycle hazard
        ehb                             // (delay slot) hazard barrier after tlbwi


130:    //
        // no match entry in TLB, use random
        //
        tlbwr                           // write to random entry of TLB 
        ssnop                           // 3 cycle hazard
        ehb                             // hazard barrier after tlbwr

140:    //
        // TLB error handled, restore all registers and restart the instruction
        //
        move    k1, zero                // no atomic op in progress
        L_REG   t0, SaveT0              // restore t0 value
        eret                            //
        nop                             // errata...
        nop                             //
        eret                            //
        
150:    // Restore T1 and Fail
        L_REG   t1, SaveK1

160:    // TLBMissNotHandled
        // reload k1, k0, and jump to general fault handler
        mfc0    k1, cause
        b       5b
        mfc0    k0, epc


        .end    GeneralExceptionP

//
// Call Stack structure fields
//
        .struct 0
cstk_args:          .space      MAX_SIZE_PSL_ARGS           // args
cstk_pcstkNext:     .space      4                           // pcstkNext
cstk_retAddr:       .space      4                           // retaddr
cstk_dwPrevSP:      .space      4                           // dwPrevSP
cstk_dwPrcInfo:     .space      4                           // dwPrcInfo
cstk_pprcLast:      .space      4                           // pprcLast
cstk_pprcVM:        .space      4                           // pprcVM
cstk_phd:           .space      4                           // phd
cstk_pOldTls:       .space      4                           // pOldTls
cstk_iMethod:       .space      4                           // iMethod
cstk_dwNewSP:       .space      4                           // dwNewSP
cstk_reg_S0:        .space      REGSIZE                     // regs[s0]
cstk_reg_S1:        .space      REGSIZE                     // regs[s1]
cstk_reg_S2:        .space      REGSIZE                     // regs[s2]
cstk_reg_S3:        .space      REGSIZE                     // regs[s3]
cstk_reg_S4:        .space      REGSIZE                     // regs[s4]
cstk_reg_S5:        .space      REGSIZE                     // regs[s5]
cstk_reg_S6:        .space      REGSIZE                     // regs[s6]
cstk_reg_S7:        .space      REGSIZE                     // regs[s7]
cstk_reg_S8:        .space      REGSIZE                     // regs[s8]
cstk_reg_GP:        .space      REGSIZE                     // regs[gp]
cstk_sizeof:

//------------------------------------------------------------------------------
// MDSwitchToUserCode - final step of process creation
//------------------------------------------------------------------------------
LEAF_ENTRY(MDSwitchToUserCode)
        .set    noreorder
        move    v0, a0                  // (v0) = function to call
        b       CallUModeFunc
        move    t6, a1                  // (delay slot) (t6) = new SP
        .end MDSwitchToUserCode

//-------------------------------------------------------------------------------
// DWORD MDCallKernelHAPI (FARPROC pfnAPI, DWORD cArgs, LPVOID phObj, REGTYPE *pArgs)
//   - calling a k-mode handle-based API
//-------------------------------------------------------------------------------
NESTED_ENTRY (MDCallKernelHAPI, MAX_SIZE_PSL_ARGS, zero)
        .set    noreorder
        subu    sp, sp, MAX_SIZE_PSL_ARGS+2*REGSIZE     // room for arguments
        S_REG   ra, MAX_SIZE_PSL_ARGS(sp)               // save return address
        PROLOGUE_END


        // copy argument (cArgs-4) from source (&arg[4]) to top of stack
        addu    t0, a3, 3*REGSIZE               // (t0) = &arg[4] == source (pArgs == &arg[1])
        addu    t1, sp, 4*REGSIZE               // (t1) = &sp[4] == dest
        subu    a1, a1, 4

        // copy argument (cArgs-4) from source (&arg[4]) to top of stack
nextcopy:

        // copy 2 arguments in every iteration
        bltz    a1, docall
        L_REG   t2, (t0)                        // (in delay slot)
        L_REG   t3, REGSIZE(t0)
        S_REG   t2, (t1)
        S_REG   t3, REGSIZE(t1)

        addu    t0, 2*REGSIZE
        addu    t1, 2*REGSIZE
        b       nextcopy
        subu    a1, a1, 2                       // (delay slot) decrement # to copy by 2
        
docall:
        move    t5, a0                          // (t5) = pfnAPI
        move    t1, a3                          // (t1) = pArgs
        move    a0, a2                          // (a0) = phObj

        // setup a1-a3 and call the funciton
        L_REG   a1,          (t1)               
        L_REG   a2,   REGSIZE(t1)
        jal     t5
        L_REG   a3, 2*REGSIZE(t1)               // (delay slot)
        

        // pop off stack and return
        L_REG   ra, MAX_SIZE_PSL_ARGS(sp)
        j       ra
        addu    sp, sp, MAX_SIZE_PSL_ARGS+2*REGSIZE

        .end MDCallKernelHAPI

//-------------------------------------------------------------------------------
// DWORD MDCallUserHAPI (PHDATA phd, PDHCALL_STRUCT phc)
//   - calling a u-mode handle-based API
//-------------------------------------------------------------------------------
NESTED_ENTRY (MDCallUserHAPI, cstk_sizeof, zero)
        .set    noreorder
        move    t0, sp
        subu    sp, sp, cstk_sizeof
        sw      ra, cstk_retAddr(sp)
        PROLOGUE_END

        // (sp) = pcstk

        sw      t0, cstk_dwPrevSP(sp)           // pcstk->dwPrevSP = old sp

        // save all callee saved registers
        S_REG   s0, cstk_reg_S0(sp)
        S_REG   s1, cstk_reg_S1(sp)
        S_REG   s2, cstk_reg_S2(sp)
        S_REG   s3, cstk_reg_S3(sp)
        S_REG   s4, cstk_reg_S4(sp)
        S_REG   s5, cstk_reg_S5(sp)
        S_REG   s6, cstk_reg_S6(sp)
        S_REG   s7, cstk_reg_S7(sp)
        S_REG   s8, cstk_reg_S8(sp)
        S_REG   gp, cstk_reg_GP(sp)

        move    a2, sp                          // (a2) = pcstk

        jal     SetupCallToUserServer
        subu    sp, sp, 4*REGSIZE               // (delay slot) C-calling convention, room for 4 registers

        addu    sp, sp, 4*REGSIZE               // reclaim stack space
        
        // (v0) = function to call
        lw      t6, cstk_dwNewSP(sp)            // (t6) = new SP, 0 iff k-mode (error, in this case)

        bnez    t6, CallUModeFunc

        // resotre a0-a3
        L_REG   a0,          (sp)               // (delay slot)
        L_REG   a1,   REGSIZE(sp)
        L_REG   a2, 2*REGSIZE(sp)
        jal     v0
        L_REG   a3, 3*REGSIZE(sp)                // (delay slot)

        // jump to API return point
        b       APIRet
        nop

        .end MDCallUserHAPI


//------------------------------------------------------------------------------
// NKPerformCallBack - calling back to user process
//------------------------------------------------------------------------------
NESTED_ENTRY(NKPerformCallBack, cstk_sizeof, zero)
        .set    noreorder

        // save arguments a0-a3 on stack (per C calling convention, sp already reserved space
        // for 4 args)
        S_REG   a0,          (sp)
        S_REG   a1,   REGSIZE(sp)
        S_REG   a2, 2*REGSIZE(sp)
        S_REG   a3, 3*REGSIZE(sp)

        move    t0, sp                              // (t0) = original sp
        subu    sp, sp, cstk_sizeof                 // reserver callstack structure on stack
        sw      ra, cstk_retAddr(sp)                // save return address
        sw      t0, cstk_dwPrevSP(sp)               // save original SP


        // save all callee saved registers
        S_REG   s0, cstk_reg_S0(sp)
        S_REG   s1, cstk_reg_S1(sp)
        S_REG   s2, cstk_reg_S2(sp)
        S_REG   s3, cstk_reg_S3(sp)
        S_REG   s4, cstk_reg_S4(sp)
        S_REG   s5, cstk_reg_S5(sp)
        S_REG   s6, cstk_reg_S6(sp)
        S_REG   s7, cstk_reg_S7(sp)
        S_REG   s8, cstk_reg_S8(sp)
        S_REG   gp, cstk_reg_GP(sp)
        
        PROLOGUE_END

        move    a0, sp                              // argument = pcstk

        // call NKPrepareCallback to setup callstack, figure out callee information
        jal     NKPrepareCallback
        subu    sp, sp, 4*REGSIZE                   // (delay slot) C-calling convention, room for 4 registers

        addu    sp, sp, 4*REGSIZE                   // reclaim stack space

        // (v0) = function to call
        lw      t6, cstk_dwNewSP(sp)                // (t6) = target SP, 0 iff k-mode 
        
        bnez    t6, CallUModeFunc
        lw      ra, cstk_retAddr(sp)                // (delay slot) retore ra

        // pop off the callstack strucure        
        addu    sp, sp, cstk_sizeof

        // restore a0 - a3, and jump to function
        L_REG   a0,          (sp)
        L_REG   a1,   REGSIZE(sp)
        L_REG   a2, 2*REGSIZE(sp)
        j       v0
        L_REG   a3, 3*REGSIZE(sp)                   // (delay slot)


CallUModeFunc:
        //
        // (v0) = function to call
        // (t6) = target sp, with arugments on stack
        //
        //
        lw      t0, CurThdPtr

        // reload argument a0-a3
        L_REG   a0,          (t6)
        L_REG   a1,   REGSIZE(t6)
        lw      t1, ThTlsNonSecure(t0)              // (t1) = pCurThread->tlsNonSecure
        L_REG   a2, 2*REGSIZE(t6)
        L_REG   a3, 3*REGSIZE(t6)

        // switch TLS
        sw      t1, ThTlsPtr(t0)
        sw      t1, lpvTls

        // return address is SYSCALL_RETRUN
        lw      ra, dwSyscallReturnTrap

        b       RtnToUMode
        move    t5, v0                              // (delay slot) (t5) = funciton to call
        
        .end NKPerformCallBack

//------------------------------------------------------------------------------
// The following code is never executed. Its purpose is to support unwinding
// through the calls to ObjectCall or ServerCallReturn.
//------------------------------------------------------------------------------
NESTED_ENTRY(APICall, cstk_sizeof, zero)
        .set    noreorder
        .set    noat
        subu    sp, cstk_sizeof
        sw      ra, cstk_retAddr(sp)                // unwinder: (ra) = APICallReturn
        PROLOGUE_END
//
// Process an API Call or return.
//
//  (k0) = EPC (encodes the API set & method index)
//  (k1) = cause
//
200:    lb      t0, KNest                           // (t0) = kernel nest depth
        blez    t0, 5b                              // non-preemtible, API Calls not allowed
        lw      t0, dwPslTrapSeed                   // (delay slot) (t0) = ppcb->dwPslTrapSeed

        xor     t0, t0, k0                          // (t0) = API encoding
        subu    t0, t0, FIRST_METHOD                // (t0) = API encoding - FIRST_METHOD

        mfc0    t1, psr                             // (t1) = processor status

        lw      t2, CurThdPtr                       // (t2) = pCurThread
        lw      t3, BASEPSR                         // (t3) = BasePsr
        move    k1, zero                            // reset atomic op. flag
        sra     t0, 2                               // (t0) = iMethod
        lw      t4, ThPcstkTop(t2)                  // (t4) = pCurThread->pcstkTop

        // Add FPU enable bits if we're FPU owner
        lw      t8, PCBCurFPUOwner                  // (t8) = ppcb->pCurFPUOwner
        bne     t2, t8, 201f                        // (pCurThread == ppcb->pCurFPUOwner)?
        lw      t9, CoProcBits                      // (delay slot) (t9) = co-proc enable bits 

        // we're FPU owner, update target PSR 
        or      t3, t9                              // (t3) = BasePSR | g_pOemGlobal->dwCoProcEnableBits
201:

        //        
        // INTERRUPTS DISABLED
        // (t0) = iMethod
        // (t1) = current psr
        // (t2) = pCurThread
        // (t3) = BasePSR 
        // (t4) = pCurThread->pcstkTop
        // (sp) = sp at the point of call
        //

        addu    t7, t0, 1                           // (t7) != 0 if not Trap return
        and     t6, t1, 1 << PSR_PMODE              // (t6) = previous mode, 0 iff in k-mode
        or      t3, 1                               // (t3) = current interrupt mask + int enable
        
        bnez    t7, NotTrapRtn
        lw      t5, ThTlsSecure(t2)                 // (delay slot) (t5) = pCurThread->tlsSecure
        
        // possible trap return

        bnez    t4, TrapRtn                         // Trap return if pCurThread->pcstkTop != 0
        nop
        
NotTrapRtn:

        //
        // INTERRUPTS DISABLED
        // (t0) = iMethod
        // (t2) = pCurThread
        // (t3) = target psr (with IE bit set)
        // (t4) = pCurThread->pcstkTop
        // (t5) = pCurThread->tlsSecure
        // (t6) = mode (0 == kmode)
        // (sp) = sp at the point of call
        //
        beqz    t6, KPSLCall
        sw      t5, lpvTls                          // (delay slot) KPlpvTls = pCurThread->tlsSecure
        
        li      t6, CST_MODE_FROM_USER              // (t6) = CST_MODE_FROM_USER
        
        bnez    t4, PSLCommon                       // 1st trip into PSL? (pcstkTop == 0)?
        sw      t5, ThTlsPtr(t2)                    // (delay slot) pCurThread->tlsPtr = pCurThread->tlsSecure

        // 1st trip into PSL, target SP = pCurThread->tlsSecure - SECURESTK_RESERVE
        subu    t4, t5, SECURESTK_RESERVE           // (t4) = pCurThread->tlsSecure - SECURESTK_RESERVE

PSLCommon:

        //
        // INTERRUPTS DISABLED
        // (t0) = iMethod
        // (t3) = target psr (with IE bit set)
        // (t4) = current sp on secure stack
        // (t6) = mode 
        // (sp) = sp at the point of call
        //
        move    t5, sp                              // (t5) = old sp
        subu    sp, t4, cstk_sizeof                 // (sp) = new sp (with room for callstack strucutre reserved)
        mtc0    t3, psr                             // enable interrupts

        // INTERRUPTS ENABLED FROM HERE ONE
        // (t0) = iMethod
        // (t5) = old sp
        // (t6) = mode (0 == kmode)
        // (sp) = new sp on secure stack with room for callstack strucutre reserved (pcstk)

        // setup callstack structure
        sw      t0, cstk_iMethod(sp)                // pcstk->iMethod = iMethod
        sw      t5, cstk_dwPrevSP(sp)               // pcstk->dwPrevSP = old sp
        sw      t6, cstk_dwPrcInfo(sp)              // pcstk->dwPrcInfo = mode
        sw      ra, cstk_retAddr(sp)                // pcstk->retAddr = ra

        // save arguments a0-a3 on pcstk
        S_REG   a0,          (sp)
        S_REG   a1,   REGSIZE(sp)
        S_REG   a2, 2*REGSIZE(sp)
        S_REG   a3, 3*REGSIZE(sp)

        // save all callee saved registers
        S_REG   s0, cstk_reg_S0(sp)
        S_REG   s1, cstk_reg_S1(sp)
        S_REG   s2, cstk_reg_S2(sp)
        S_REG   s3, cstk_reg_S3(sp)
        S_REG   s4, cstk_reg_S4(sp)
        S_REG   s5, cstk_reg_S5(sp)
        S_REG   s6, cstk_reg_S6(sp)
        S_REG   s7, cstk_reg_S7(sp)
        S_REG   s8, cstk_reg_S8(sp)
        S_REG   gp, cstk_reg_GP(sp)

        move    a0, sp                              // argument to ObjectCall == pcstk

        jal     ObjectCall
        subu    sp, sp, 4*REGSIZE                   // (delay slot) C-calling convention, room for 4 registers

        addu    sp, sp, 4*REGSIZE                   // reclaim stack space
        
        // (v0) = function to call

        lw      t6, cstk_dwNewSP(sp)                // (t6) = new SP, != 0 iff user-mode server
        bnez    t6, CallUModeFunc
        
        // kernel mode server, call direct

        // reload arguments a0-a3 from pcstk
        L_REG   a0,          (sp)                   // (delay slot) 
        L_REG   a1,   REGSIZE(sp)
        L_REG   a2, 2*REGSIZE(sp)
        jal     v0
        L_REG   a3, 3*REGSIZE(sp)                   // (delay slot)


ALTERNATE_ENTRY(APIRet)

        //
        // (v1:v0) - return value
        // (sp)    - pcstk
        //

        // save return value in s1:s0, and call ServerCallReturn
        move    s0, v0                              // (s0) = v0
        move    s1, v1                              // (s1) = v1
 
        jal     ServerCallReturn        
        subu    sp, sp, 4*REGSIZE                   // (delay slot) C-calling convention, room for 4 registers

        addu    sp, sp, 4*REGSIZE                   // reclaim stack space

        lw      t0, cstk_dwPrcInfo(sp)              // (t0) = pcstk->dwPrcInfo, 0 iff return to k-mode

        // restore return value
        move    v0, s0
        move    v1, s1

        // restore callee saved registers
        L_REG   s0, cstk_reg_S0(sp)
        L_REG   s1, cstk_reg_S1(sp)
        L_REG   s2, cstk_reg_S2(sp)
        L_REG   s3, cstk_reg_S3(sp)
        L_REG   s4, cstk_reg_S4(sp)
        L_REG   s5, cstk_reg_S5(sp)
        L_REG   s6, cstk_reg_S6(sp)
        L_REG   s7, cstk_reg_S7(sp)
        L_REG   s8, cstk_reg_S8(sp)
        L_REG   gp, cstk_reg_GP(sp)

        // retrieve old-sp
        lw      t6, cstk_dwPrevSP(sp)               // (t6) = old SP

        // return to caller based on mode
        bnez    t0, RtnToUMode
        lw      t5, cstk_retAddr(sp)                // (delay slot) load return address

        // k-mode caller, just reload sp and jump to return address
        // (t5) - return address
        // (t6) - target SP
        j       t5
        move    sp, t6                              // (delay slot) reload sp

RtnToUMode:

        //
        // Return to user mode. To do this: build a new PSR value from the thread's mode
        // and BasePSR. This must be done with interrupts disabled so that BasePSR is not
        // changing.
        // (t5) - return address
        // (t6) - target SP
        //


        CP0_STOP_PREFETCH(mtc0, zero, psr, t1);     // all interrupts off
        ehb                                         // hazzard barrier
        li      t0, USER_MODE                       // (t0) = USER_MODE
        lw      t7, CurThdPtr                       // (t7) = pCurThread
        move    sp, t6                              // (sp) = target SP
        lw      t8, PCBCurFPUOwner                  // (t8) = ppcb->pCurFPUOwner
        lw      t1, BASEPSR                         // (t1) = BasePSR
        mtc0    t5, epc                             // (epc) - return address

        bne     t7, t8, 251f
        lw      t9, CoProcBits                      // (delay slot) (t9) = co-proc enable bits 
        or      t1, t9
251:

        or      t1, t0                              // (t1) = merged status
        mtc0    t1, psr                             // reload status

        ssnop                                       
        ssnop
        ehb                                         // 3 cycle hazard on psr between mtc0 and eret
        eret
        nop
        nop
        eret

KPSLCall:
        // making a trapped API call from k-mode 
        // INTERRUPTS DISABLED
        // (t0) = iMethod
        // (t3) = target psr (with IE bit set)
        // (t6) = 0 == kmode
        // (t5) = pCurThread->tlsSecure
        // (sp) = sp at the point of call
        //
        subu    t1, t0, PERFORMCALLBACK             // (t1) = iMethod - PERFORMCALLBACK
        bnez    t1, PSLCommon                       // jump to PSL common code if not callback
        move    t4, sp                              // (delay slot) (t4) = current SP

        // callback, enable interrupts and jump to NKPerformCallback
        mtc0    t3, psr                             // enable interrupts
        b       NKPerformCallBack
        ehb                                         // hazzard barrier

TrapRtn:
        // INTERRUPTS DISABLED
        // v1:v0 - return value
        // (t2) = pCurThread
        // (t3) = target psr (with IE bit set)
        // (t4) = pCurThread->pcstkTop
        // (t5) = pCurThread->tlsSecure

        // switch stack
        sw      t5, ThTlsPtr(t2)                    // pCurThread->tlsPtr = pCurThread->tlsSecure
        move    sp, t4                              // (sp) = pCurThread->pcstkTop
        mtc0    t3, psr                             // enable interrupts
        b       APIRet                              // jump to common code
        sw      t5, lpvTls                          // (delay slot) KPlpvTls = pCurThread->tlsSecure

END_REGION(GeneralException_End)
        .set at
        .set reorder
        .end    APICall


//------------------------------------------------------------------------------
//
//  Function: CacheErrorHandler()
//
//  This function handles cache errors that occur on a MIPS platform. The kernel
//  initializes the vector table to this function when a cache error occurs.
//
//------------------------------------------------------------------------------
LEAF_ENTRY(CacheErrorHandler)
        .set    noreorder
        .globl  CacheErrorHandler_End

        nop
        nop
        break     1
        eret
        
CacheErrorHandler_End:

     .end CacheErrorHandler


//------------------------------------------------------------------------------
// This routine services interrupts which have been disabled. It masks the
// interrupt enable bit in the PSR to prevent further interrupts and treats
// the interrupt as a NOP.
//
//  Entry   (a0) = interrupt level * 4
//  Exit    (v0) = SYSINTR_NOP
//  Uses    a0, a1, v0, v1, at
//------------------------------------------------------------------------------
LEAF_ENTRY(DisabledInterruptHandler)
        .set noreorder
        srl     a0, 2
        li      v0, 0x400
        sllv    a0, v0, a0
        li      v0, -1
        xor     a0, v0                  // (a0) = ~(intr. mask)
        mfc0    v0, psr                 
        lw      v1, g_pKData            // (v1) = g_pKData
        lw      a1, BasePSR_KData-KData(v1)    // (a1) = g_pKData->basePsr
        and     v0, a0                  // (v0) = psr w/intr disabled
        mtc0    v0, psr                 
        and     a1, a0                  // (a0) = base PSR w/intr disabled
        sw      a1, BasePSR_KData-KData(v1)
        li      v0, SYSINTR_NOP
        j       ra
        ehb                             // hazard barrier after masking interrupt
        
        
        .end DisabledIntrHandler


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LEAF_ENTRY(FalseInterrupt)
        .set    noreorder

        j       ra
        li      v0, SYSINTR_NOP

        .set    reorder
        .end    FalseInterrupt


//------------------------------------------------------------------------------
// CaptureContext is invoked in kernel context on the user thread's stack to
// build a context structure to be used for exception unwinding.
//
//  (sp) = aligned stack pointer
//------------------------------------------------------------------------------
LEAF_ENTRY(CaptureContext)
        .set noreorder
        .set noat
        subu    sp, ContextFrameLength+ExceptionRecordLength  // (sp) = ptr to CONTEXT buffer, exception record follows
        sw      zero, 0(sp)             // make sure that the stack is addressable
        .end CaptureContext
        
        NESTED_ENTRY(xxCaptureContext, ContextFrameLength+ExceptionRecordLength, zero)
        .set noreorder
        .set noat
        S_REG   sp, CxIntSp(sp)         // fixed up by ExceptionDispatch, really don't have to be saved
        S_REG   a0, CxIntA0(sp)
        S_REG   a1, CxIntA1(sp)
        S_REG   a2, CxIntA2(sp)
        S_REG   a3, CxIntA3(sp)
        S_REG   gp, CxIntGp(sp)
        S_REG   s0, CxIntS0(sp)
        S_REG   s1, CxIntS1(sp)
        S_REG   s2, CxIntS2(sp)
        S_REG   s3, CxIntS3(sp)
        S_REG   s4, CxIntS4(sp)
        S_REG   s5, CxIntS5(sp)
        S_REG   s6, CxIntS6(sp)
        S_REG   s7, CxIntS7(sp)
        S_REG   s8, CxIntS8(sp)
        S_REG   v0, CxIntV0(sp)
        S_REG   v1, CxIntV1(sp)
        S_REG   AT, CxIntAt(sp)
        S_REG   ra, CxIntRa(sp)
        S_REG   t0, CxIntT0(sp)
        S_REG   t1, CxIntT1(sp)
        S_REG   t2, CxIntT2(sp)
        S_REG   t3, CxIntT3(sp)
        mfhi    t0
        mflo    t1
        S_REG   t4, CxIntT4(sp)
        S_REG   t5, CxIntT5(sp)
        S_REG   t6, CxIntT6(sp)
        S_REG   t7, CxIntT7(sp)
        S_REG   t8, CxIntT8(sp)
        S_REG   t0, CxIntHi(sp)         // save HI mul/div register
        S_REG   t1, CxIntLo(sp)         // save LO mul/div register
        S_REG   t9, CxIntT9(sp)
        sw      ra, CxFir(sp)           // fixed up by ExceptionDispatch
        S_REG   zero, CxIntZero(sp)
        
        PROLOGUE_END
            
        move    a1, sp                      // (a1) = pCtx: context record
        li      t0, CONTEXT_SEH
        sw      t0, CxContextFlags(sp)
        
        jal     ExceptionDispatch
        addu    a0, a1, ContextFrameLength  // (delay slot) (a0) = pExr: Exception Record

        // returns from ExcepitonDispatch
        bnez    v0, RunSEH                  // run SEH if return non-zero
        move    a1, sp                      // (delay slot) (a1) = pointer to context record

        // not running SEH, resume from context

        L_REG   s0, CxIntS0(sp)         // Restore thread's permanent registers
        L_REG   s1, CxIntS1(sp)
        L_REG   s2, CxIntS2(sp)
        L_REG   s3, CxIntS3(sp)
        L_REG   s4, CxIntS4(sp)
        L_REG   s5, CxIntS5(sp)
        L_REG   s6, CxIntS6(sp)
        L_REG   s7, CxIntS7(sp)
        L_REG   s8, CxIntS8(sp)
        L_REG   t0, CxIntHi(sp)         // (t0) = HI mul/div register
        L_REG   t1, CxIntLo(sp)         // (t1) = LO mul/div register
        mthi    t0
        mtlo    t1
        L_REG   v1, CxIntV1(sp)
        L_REG   t0, CxIntT0(sp)
        L_REG   t1, CxIntT1(sp)
        L_REG   t2, CxIntT2(sp)
        L_REG   t3, CxIntT3(sp)
        L_REG   t4, CxIntT4(sp)
        L_REG   t5, CxIntT5(sp)
        L_REG   t6, CxIntT6(sp)
        L_REG   t7, CxIntT7(sp)
        L_REG   t8, CxIntT8(sp)
        L_REG   t9, CxIntT9(sp)
        L_REG   AT, CxIntAt(sp)
        
        mtc0    zero, psr               // all interrupts off!
        L_REG   a3, CxIntA3(sp)         // restore regs while       
        L_REG   a2, CxIntA2(sp)         //  waiting for interruts to
        L_REG   a1, CxIntA1(sp)         //  be disabled.
        ehb                             // hazzard barrier
        lw      a0, BASEPSR             // (a0) = global default status value
        lw      v0, CxPsr(sp)           // (v0) = thread's default status
        L_REG   ra, CxIntRa(sp)
        or      v0, a0                  // (v0) = thread + global status

        lw      a0, PCBCurFPUOwner
        lw      gp, CurThdPtr
        bne     a0, gp, ccnofp
        lw      a0, CoProcBits          // (delay slot) a0 = co-proc enable bits
        or      v0, a0
ccnofp:
        S_REG   v0, SaveK0

        L_REG   a0, CxIntA0(sp)
        lw      k0, CxFir(sp)           // (k0) = exception return address
        L_REG   gp, CxIntGp(sp)
        L_REG   v0, CxIntV0(sp)         // restore return value
        // MUST use lw instead of L_REG so sp get automatic sign extension.
        // The reason is that we do arithmetics on SP during exception handling
        // and SP can become a positive 64 bit value.
        lw      sp, CxIntSp(sp)         // restore stack pointer

        mtc0    k0, epc                 // set continuation address
        L_REG   k0, SaveK0
        mtc0    k0, psr
        ssnop
        ssnop
        ehb                             // 3 cycle hazard on psr between mtc0 and eret
        eret                            // restore user status
        nop                           
        nop
        eret

RunSEH:
        .set at

        // (a1) = pCtx

        lw      t1, CxPsr(a1)                   // (t1) = PSR
        lw      t2, g_pfnUsrRtlDispExcp         // (t2) = user mode exception dispatch
        andi    t5, t1, 1 << PSR_PMODE          // (t5) = 0 iff in k-mode
        lw      t3, g_pfnKrnRtlDispExcp         // (t3) = kernel mode exception dispatch

        beqz    t5, CallSEH                     // user current stack if in k-mode
        move    t4, sp                          // (delay slot) target stack current sp

        // user mode, switch stack
        lw      t4, CxIntSp(a1)                 // (t4) = new SP for user stack
        move    t3, t2                          // (t3) = user mdoe exception dispatch
        
CallSEH:
        // 
        //  (t1) = target psr
        //  (t3) = function to dispatch to
        //  (t4) = target stack == pCtx
        //

        CP0_STOP_PREFETCH(mtc0, zero, psr, t0)  // interrupts off
        // 3 cycle hazard
        move    a1, t4                          // (a1) = pCtx: context record 
        addu    a0, a1, ContextFrameLength      // (a0) = pExr: exception record
        ehb                                     // hazard barrier

        lw      t2, CurThdPtr
        lw      t0, PCBCurFPUOwner
        bne     t0, t2, sehnofp
        lw      t0, CoProcBits                  // (delay slot) t0 = co-proc enable bits
        or      t1, t0                          // (t1) = psr | co-proc enable bits
sehnofp:

        mtc0    t3, epc                         // (epc) = exception dispatch function, based on mode
        lw      t2, BASEPSR                     // (t2) = BasePSR
        move    sp, t4                          // update sp
        or      t2, t1                          // (t2) = target psr
        mtc0    t2, psr                         // update psr
        ssnop                                   // 
        ssnop                                   // 3 cycle hazard on psr between mtc0 and eret
        ehb                                     // hazard barrier
        eret
        nop                                     // errata
        nop
        eret

        .set at
        .set reorder

        .end xxCaptureContext


// void HwTrap (struct _HDSTUB_EVENT2 *)
LEAF_ENTRY(HwTrap)
        .set noreorder
        break 1                                 // debug break
        j ra                                    // return
        nop                                     // delay slot
        nop                                     // 4 dwords

        nop
        nop
        nop
        nop

        nop
        nop
        nop
        nop

        nop
        nop
        nop
        nop                                     // 16 dwords
        .set reorder
ENTRY_END


