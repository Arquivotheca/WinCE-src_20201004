//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

#include "ksmips.h"
#include "nkintr.h"
#include "kpage.h"
#include "mem_mips.h"
#include "psyscall.h"
#include "xtime.h"

#define VR5432_BP_BUG 1

// must match the value in kernel.h
#define SECURESTK_RESERVE       (32 + 4*REG_SIZE)      // SIZE_PRETLS + 4 * REG_SIZE
#define MAX_PSL_ARGS            (14 * REG_SIZE)        // 14 max PSL args

#define CALLEE_SAVED_REGS       (10 * REG_SIZE)        // (s0 - s8, gp)

#define PERFORMCALLBACK         -113    // MUST be -PerformCallback Win32Methods in kwin32.c 
                                        // 113 == -(APISet 0, method 113)
#define RAISEEXCEPTION          -114    // MUST be -RaiseException Win32Methods in kwin32.c
#define KERN_TRUST_FULL         2

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
        
        .text
        .globl  MD_CBRtn
        .globl  PtrCurMSec

PtrCurMSec: 
        .word   AddrCurMSec

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
        
        lw      t0, BasePSR
        nop
        or      t0, 1
        j       ra
        mtc0    t0, psr
        
        .end INTERRUPTS_ON

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LEAF_ENTRY(INTERRUPTS_OFF)
        .set noreorder
        
        CP0_STOP_PREFETCH(mtc0, zero, psr, t0);
        nop         // 3 cycle delay
        j       ra
        nop
        
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

        // return
        j       ra
        and     v0, v0, 1               // (v0) = 1 if interrupt was enabled, 0 otherwise. (delay slot)

        .end INTERRUPTS_ENABLE

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
        nop
        
        .end SetCPUHardwareWatch


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LEAF_ENTRY(SetCPUASID)
        .set noreorder

        //----- CeLogThreadMigrate -----
        //
        // NOTE : To make things relatively consistent, I'm going to make
        // the registers v0, t0-t9, a0, a1 available for use. On entry to
        // CeLogThreadMigrateMIPS, a0 will contain the handle of the new process.
        //
        // I'm assuming that at this point the only registers of these that
        // I need to preserve are A0 and RA.
        //

        lw      t0, CeLogStatus         // (t0) = KInfoTable[KINX_CELOGSTATUS]
        bne     t0, 1, celog10          // if status != CELOGSTATUS_ENABLED_GENERAL then skip CeLog call
        nop
        
        lw      t0, ThProc(a0)          // (t0) = ptr to new process
        lw      t1, PrcHandle(t0)       // (t1) = handle of new process
        lw      t0, hCurProc            // (t0) = current process handle
        beq     t0, t1, celog10         // if procs are equal then skip CeLog call
        nop

        subu    sp, 2*REG_SIZE          // adjust sp
        sw      ra, 0*REG_SIZE(sp)      // save RA
        S_REG   a0, 1*REG_SIZE(sp)      // save arg

        move    a0, t1                  // (a0) = handle of new process

        jal     CELOG_ThreadMigrateMIPS
        nop

        lw      ra, 0*REG_SIZE(sp)      // restore RA
        L_REG   a0, 1*REG_SIZE(sp)      // restore arg
        addu    sp, 2*REG_SIZE          // adjust sp

celog10:
        //--- End CeLogThreadMigrate ---

        lw      t0, ThProc(a0)          // (t0) = ptr to current process
        lw      t1, ThAKey(a0)          // (t1) = thread's access key
        lw      t2, PrcHandle(t0)       // (t2) = handle of current process
        sw      t0, CurPrcPtr           // remember current process pointer
        sw      t1, CurAKey             // save access key for TLB handler
        lb      t1, PrcID(t0)           // (t0) = process ID
        beq     t1, zero, UseNKSection  // slot 1 -- use NKSection
        sw      t2, hCurProc            // remember current process handle (delay slot)

        // not slot 1, find it from section table
        lw      t2, PrcVMBase(t0)       // (t2) = memory section base address
        srl     t2, VA_SECTION-2        // (t2) = index into section table
        b       ContinueSetCpuASID
        lw      t2, SectionTable(t2)    // (t2) = process's memory section (delay slot)

UseNKSection:
        la      t2, NKSection

ContinueSetCpuASID:
        
#if ENTRYHI_PID != 0
        sll     t1, ENTRYHI_PID
#endif
        CP0_STOP_PREFETCH(mtc0, t1, entryhi, t0);    // set ASID
        j       ra
        sw      t2, SectionTable(zero)  // swap in default process slot
        
        .end SetCPUASID




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
        bltz    t1, 50f                 // nested kernel call
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
        nop

        lh      t9, ReschedFlag         // (t9) = resched flag + nest level
        li      t1, 1
        beq     t9, t1, 20f             // reschedule needed
        lw      ra, KCF_RA(sp)          // (ra) = return address

        lw      t2, dwKCRes
        bne t2, zero, 20f       // reschedule needed
        nop

        lw      t2, KCF_SP(sp)          // (t2) = original stack pointer
        addu    t9, 256
        sh      t9, ReschedFlag         // leave non-preemtible state

        lw      t9, BasePSR
        or t9, 1
#ifdef MIPS_HAS_FPU
        lw      t8, g_CurFPUOwner
        lw      t7, CurThdPtr
        bne     t7, t8, 19f
        nop
        la      t8, dwNKCoProcEnableBits
        lw      t8, (t8)
        or      t9, t8
19:
#endif
        mtc0    t9, psr

        j       ra
        move    sp, t2                  // restore stack pointer

//
// ReschedFlag set, so must run the scheduler to find which thread
// to dispatch next.
//
//  (v0) = KCall return value
//
20:
        lw      t9, BasePSR
        or      t9, 1
        mtc0    t9, psr
        
        lw      t0, CurThdPtr           // (t0) = ptr to current thread
        lw      t2, KCF_SP(sp)          // (t2) = original stack pointer
        li      t3, KERNEL_MODE
        sw      ra, TcxFir(t0)          // thread will resume at return address
        sw      t3, TcxPsr(t0)          //  & in kernel mode
        S_REG   t2, TcxIntSp(t0)        // save stack pointer
        S_REG   v0, TcxIntV0(t0)        // save return value
        S_REG   s0, TcxIntS0(t0)        // save thread's permanent registers
        S_REG   s1, TcxIntS1(t0)        // ...
        S_REG   s2, TcxIntS2(t0)        // ...
        S_REG   s3, TcxIntS3(t0)        // ...
        S_REG   s4, TcxIntS4(t0)        // ...
        S_REG   s5, TcxIntS5(t0)        // ...
        S_REG   s6, TcxIntS6(t0)        // ...
        S_REG   s7, TcxIntS7(t0)        // ...
        S_REG   s8, TcxIntS8(t0)        // ...
        S_REG   gp, TcxIntGp(t0)
        sw      zero, TcxContextFlags(t0)
        
        j       resched
        move    s0, t0                  // (s0) = ptr to current thread struct

25:     j       resched
        move    s0, zero                // no current thread

// Nested KCall. Just invoke the function directly.
//
//  (t0) = function address
//  (a0) = 1st function argument
//  (a1) = 2nd function argument
//  (a2) = 3rd function argument
//
50:     j       t0
        nop

        .set reorder
        .end xKCall



//------------------------------------------------------------------------------
//
// Interlocked singly-linked list functions.
//
//  These functions are used internally by the kernel. They are not exported
// because they require privileged instructions on some CPUs.
//
// InterlockedPopList - remove first item from list
//
//  Entry   (a0) = ptr to list head
//  Return  (v0) = item at head of list
//      If (list not emtpy) next item is now head of list
//  Usea    t0, t1
//
//------------------------------------------------------------------------------
LEAF_ENTRY(InterlockedPopList)
#if NO_LL
        la      t0, IPopRestart
        .set    noreorder
IPopRestart:
        addiu   k1, t0, 6 * 4           // (k1) = &IPopDone, indicate interlocked
                                        //        operation in progress
        lw      v0, (a0)                // (v0) = original contents
        beq     v0, zero, 10f           
        nop                             
        lw      t1, (v0)                // (t1) = next item on the list
        sw      t1, (a0)                // update head of list
IPopDone:
10:     j       ra                      
        move    k1, zero                // interlocked operation complete
#else
 #error Unimplemented.
#endif
        .end   InterlockedPopList



//------------------------------------------------------------------------------
// InterlockedPushList - add an item to head of list
//
//  Entry   (a0) = ptr to list head
//      (a1) = ptr to item
//  Return  (v0) = old head of list
//  Uses    t0, v0
//------------------------------------------------------------------------------
LEAF_ENTRY(InterlockedPushList)
#if NO_LL
        la      t0, IPushRestart
        .set    noreorder
IPushRestart:
        addiu   k1, t0, 4 * 4           // (k1) = &IPushDone, indicate interlocked
                                        //        operation in progress
        lw      v0, (a0)                // (v0) = original list head
        sw      v0, (a1)                // store linkage
        sw      a1, (a0)                // store new list head
IPushDone:
        j       ra                      
        move    k1, zero                // interlocked operation complete
#else
 #error Unimplemented.
#endif
        .end   InterlockedPushList

        
        
#ifdef MIPS_HAS_FPU

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LEAF_ENTRY(GetAndClearFloatCode)
        
        CP0_STOP_PREFETCH(mfc0, a1, psr, a2);
        lui     a2, 0x2000
        or      a1, a2
        mtc0    a1, psr
        nop
        nop
        nop
        cfc1    v0, fsr
        
        li      t0, 0xfffc0fff
        and     t0, t0, v0
        
        ctc1    t0, fsr
        j       ra
        nop
        
        .end GetCode

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LEAF_ENTRY(SaveFloatContext)
        CP0_STOP_PREFETCH(mfc0, t0, psr, t1);
        lui     t1, 0x2000
        or      t0, t1
        mtc0    t0, psr
        nop
        nop
        nop
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
        nop
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
        .set reorder
        
        la      s0, KData
        j       resched
        
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

1:      ori     k1, k0, 0xFFFC          // (k1) = 0xFFFFFFFE iff an API call
        addu    k1, 2                   // (k1) = 0 iff an API call
        beq     zero, k1, 200f          // go process an API call or API return
        mfc0    k1, cause               // (k1) = exception cause
        S_REG   t0, SaveT0              // get a working register

        lw      t0, IsR41XX             // is this R41XX?
        beqz    t0, 3f
        andi    t0, k1, 0xff            // (in delay slot, but okay) ignore exception b's - We're getting extraneous exception B's
        xori    t0, t0, 0x2c            // ignore exception b's - ignore them
        beq     t0, zero, 104f          // ignore exception b's - 
//
// The read and write miss codes differ by exactly one bit such that they
// can be tested for by a single mask operation followed by a test for the
// read miss code.
//
3:
        and     t0,k1,MISS_MASK         // isolate exception code
        xori    t0,XCODE_READ_MISS      // get exception code for read miss
        beq     t0,zero,100f            // read or write TLB miss
5:      lb      t0, KNest               // (t0) = reentrancy count
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
        and     t0, k1, XCODE_MASK
        bne     t0, zero, 40f           // not a h/w interrupt
        L_REG   t0, SaveT0              // restore T0
        lw      a0, BasePSR             // (a0) = interrupt mask
        S_REG   a1, TcxIntA1(s0)
        S_REG   AT, TcxIntAt(s0)
        .set at
        and     a0, k1                  // (a0) = bits 15-10 pending interrupts
        srl     a0, CAUSE_INTPEND+2
        and     a0, 0x3F                // (a0) = hw interrupt cause bits
        lb      a0, IntrPriority(a0)    // (a0) = highest pri interrupt * 4
        S_REG   a2, TcxIntA2(s0)
        move    k1, zero                // reset atomic op. flag
        lw      v0, ISRTable(a0)        // (v0) = interrupt handler
        S_REG   ra, TcxIntRa(s0)        // save return address
        
        mfc0    k0, psr
        sw      k0, TcxPsr(s0)
        
        sra     a0, 2                   // Keep the sign for spurious interrupts
        addi    a0, 1
        lbu     a0, IntrMask(a0)
        sll     a0, 10
        
        not     a0
        and     k0, k0, a0
        mtc0    k0, psr
#if defined(CELOG)
	//
        // NOTE : To make things relatively consistent, I'm going to make
        // the registers v0, AT, a0-a3 available for use. On entry to CeLogInterruptMIPS
        // A0 will contain the value to be logged (cNest + SYSINTR value)
        //
        // I'm assuming that at this point the only register of these that
        // I need to preserve is V0 (ISR pointer), and A3 (not yet saved)
        //
        move    k0, v0                  // save v0

        jal     CeLogInterruptMIPS
        li      a0, 0x80000000          // delay slot (a0 = mark as ISR entry)

        move    v0, k0                  // restore v0

        mfc0    k0, psr
#endif

        // clear User mode, EXL & ERL bits in PSR
        ori     k0, (1<<PSR_EXL) | (1<<PSR_ERL) | (1<<PSR_PMODE)
        xori    k0, (1<<PSR_EXL) | (1<<PSR_ERL) | (1<<PSR_PMODE)

        mtc0    k0, psr                 // enable interrupts
        jal     v0                      // Call OEM ISR
        S_REG   a3, TcxIntA3(s0)

        mfc0    a0, psr
        ori     a0, (1<<PSR_IE)
        xori    a0, (1<<PSR_IE)
        mtc0    a0, psr                 // clear interrupt enable
        lw      a0, TcxPsr(s0)
        nop
        mtc0    a0, psr                 // now restore real psr, also toggling interrupt enable
        
        .set noat


#if defined(CELOG)
        //
        // NOTE : To make things relatively consistent, I'm going to make
        // the registers v0, AT, a0-a3 available for use. On entry to CeLogInterruptMIPS
        // A0 will contain the value to be logged (cNest + SYSINTR value)
        //
        // I'm assuming that at this point the only register of these that
        // I need to preserve is V0 (SYSINTR value)
        //
        move    k0, v0                  // save V0
        lb      a0, KNest               // (a0) = nest level (0, -1, -2, etc)
        sub     a0, zero, a0            // (a0) = nest level (0,  1,  2, etc)
        sll     a0, a0, 16              // (a0) = (a0) << 16

        jal     CeLogInterruptMIPS
        or      a0, a0, v0              // delay slot (a0 = (-KNest << 16) | SYSINTR)

        move    v0, k0                  // restore V0
#endif
        
        
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
        lw      a1, PendEvents

// The ISR has requested that an interrupt event be signalled

        li      a2, 1
        sll     a3, a2, v0
        or      a1, a1, a3
        sw      a1, PendEvents
                
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
        L_REG   AT, TcxIntAt(s0)
        L_REG   ra, TcxIntRa(s0)
        L_REG   a0, TcxIntA0(s0)        
        L_REG   a1, TcxIntA1(s0)
        L_REG   a2, TcxIntA2(s0)
        L_REG   a3, TcxIntA3(s0)
        lw      k0, TcxFir(s0)          // (k0) = exception return address
        L_REG   v0, TcxIntV0(s0)        // restore return value

        mtc0    k0, epc                 // restore EPC to interrupted stream

        // MUST use lw instead of L_REG so sp get automatic sign extension.
        // The reason is that we do arithmetics on SP during exception handling
        // and SP can become a positive 64 bit value.
        lw      sp, TcxIntSp(s0)        // restore stack pointer
        L_REG   gp, TcxIntGp(s0)
        L_REG   s0, TcxIntS0(s0)

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
//  original V0, A0, S0, Sp, Gp, & EPC saved into thread struct

40:     S_REG   a1, TcxIntA1(s0)
        S_REG   a2, TcxIntA2(s0)
        S_REG   a3, TcxIntA3(s0)
        S_REG   ra, TcxIntRa(s0)
        S_REG   AT, TcxIntAt(s0)
        mfc0    a2, badvaddr

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
        S_REG   v1, TcxIntV1(s0)
        S_REG   t0, TcxIntT0(s0)
        S_REG   t1, TcxIntT1(s0)
        S_REG   t2, TcxIntT2(s0)
        S_REG   t3, TcxIntT3(s0)
        mfhi    t0
        mflo    t1
        S_REG   t4, TcxIntT4(s0)
        S_REG   t5, TcxIntT5(s0)
        S_REG   t6, TcxIntT6(s0)
        S_REG   t7, TcxIntT7(s0)
        S_REG   t8, TcxIntT8(s0)
        S_REG   t9, TcxIntT9(s0)
        S_REG   t0, TcxIntHi(s0)        // save HI mul/div register
        S_REG   t1, TcxIntLo(s0)        // save LO mul/div register
        S_REG   s1, TcxIntS1(s0)
        S_REG   s2, TcxIntS2(s0)
        S_REG   s3, TcxIntS3(s0)
        S_REG   s4, TcxIntS4(s0)
        S_REG   s5, TcxIntS5(s0)
        S_REG   s6, TcxIntS6(s0)
        S_REG   s7, TcxIntS7(s0)
        S_REG   s8, TcxIntS8(s0)
        li      t0, CONTEXT_CONTROL | CONTEXT_INTEGER
                
        beq     a1, zero, 50f
        sw      t0, TcxContextFlags(s0)
        jal     HandleException         // jump to handler
        move    a0, s0                  // (a0) = ptr to thread
        bne     v0, zero, 60f           // resume current thread
        nop
//
//  The current thread has been blocked or a reschedule is pending.
//  Save the remaining CPU state and call the scheduler to obtain the
//  highest priority thread to run.
//
//  (s0) = CurThdPtr.
//
resched:
50:     lb      t0, BPowerOff           // if power off flag set
        bne     zero, t0, 90f
        nop     
                
51:             
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
        bne     zero, s1, 51b
        nop     
                
53:             
        la      t0, RunList
        lw      v0, 4(t0)
                
        beq     zero, v0, Idle          // no threads to run, do idle processing
        nop     
        beq     s0, v0, 60f             // resume current thread
        move    s0, v0                  // (s0) = new current thread
        lw      t0, ThHandle(s0)        // (t0) = thread's handle
        sw      s0, CurThdPtr           // remember current THREAD pointer
        sw      t0, hCurThread          //   and current thread handle
        lw      t0, ThProc(s0)          // (t0) = ptr to current process
        lw      t4, ThAKey(s0)          // (t4) = thread's access key
        lw      v0, PrcHandle(t0)       // (v0) = handle of current process
        lb      t1, PrcID(t0)           // (t1) = process ID
        sw      t0, CurPrcPtr           // remember current process pointer
        sw      v0, hCurProc            //   and current process handle
        beq     t1, zero, 58f           // slot 1 special case
        lw      t3, ThTlsPtr(s0)        // (t3) == tlsPtr (delay slot)

        // not slot 1, use section table        
        lw      t2, PrcVMBase(t0)       // (t2) = memory section base address
        srl     t2, VA_SECTION-2        // (t2) = index into section table
        b       59f
        lw      t2, SectionTable(t2)    // (t2) = process's memory section (delay slot)

58:     la      t2, NKSection           // (t2) = &NKSection

59:     // rest of the common code
        sw      t3, lpvTls
        sw      t4, CurAKey             // save access key for TLB handler
#if ENTRYHI_PID != 0
        sll     t1, ENTRYHI_PID
#endif
        mtc0    t1, entryhi             // set ASID
        sw      t2, SectionTable(zero)  // swap in default process slot

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
        beq     v1, zero, 65f
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
        
65:
        CP0_STOP_PREFETCH(mtc0, zero, psr, v0);
        nop
        L_REG   ra, TcxIntRa(s0)
        L_REG   v0, TcxIntV0(s0)        // restore return value
        lh      k1, ReschedFlag         // (k1) = resched + nested exception
        li      k0, 1
        beq     k1, k0, 68f             // reschedule pending
        addu    k1, 256                 // remove one level of nesting
        sh      k1, ReschedFlag
        lw      k0, BasePSR             // (k0) = global default status value
        lw      k1, TcxPsr(s0)          // (k1) = thread's default status
        // MUST use lw instead of L_REG so sp get automatic sign extension.
        // The reason is that we do arithmetics on SP during exception handling
        // and SP can become a positive 64 bit value.
        lw      sp, TcxIntSp(s0)        // restore stack pointer
        or      k1, k0                  // (k1) = thread + global status
#ifdef MIPS_HAS_FPU
        lw      k0, g_CurFPUOwner
        bne     k0, s0, 66f
        nop
        la      k0, dwNKCoProcEnableBits
        lw      k0, (k0)
        or      k1, k0
66:
#endif
        mtc0    k1, psr                 // restore status
        lw      k0, TcxFir(s0)          // (k0) = exception return address
        L_REG   gp, TcxIntGp(s0)        
        L_REG   s0, TcxIntS0(s0)        // Restore thread's permanent registers
        mtc0    k0, epc                 // set continuation address for Eret
        move    k1, zero                // (k1) = 0 (no atomic op in progress)

        nop
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
        nop
        lh      v0, ReschedFlag         // (v0) = resched + nested exception
        lw      a0, BasePSR             // (a0) = global default status value
        bgtz    v0, 68f                 // reschedule pending
        nop                             
        jal     OEMIdle                 // let OEM stop clocks, etc.
        nop
        li      v1, 1
        sb      v1, ReschedFlag

//
// Pending reschedule found during final dispatch, re-enable 
// interrupts and try again.
//
//  (a0) = BasePSR
//
68:     lw      a0, BasePSR             // (a0) = global default status value
        move    k1, zero                // (k1) = 0 (no atomic op in progress)
        ori     a0, 1 << PSR_IE         // (t0) = current status + int enable
        j       resched
        mtc0    a0, psr                 // re-enable interrupts

//++++++
// The power off flag has been set. Call DoPowerOff() to notify the file system
// and window manager and invoke OEMPowerOff.
//
90:     jal     DoPowerOff              // call power off handler
        sb      zero, BPowerOff         // clear power off flag
        b       resched
        move    s0, zero                // no current thread


// TLB load or store exception. These exceptions are routed to the general
// exception vector either because the TLBMiss handler could not translate
// the virtual address or because there is a matching but invalid entry loaded
// into the TLB. If a TLB probe suceeds then, this is due to an invalid entry.
// In that case, the page tables are examined to find a matching valid entry
// for the page causing the fault. If a matching entry is found, then the TLB
// is updated and the instruction is restarted. If no match is found or the
// TLB probe fails, the exception is processed via the normal exception path.
//
//  (k0) = EPC (maybe updated for InterlockedXXX API)
//  (k1) = cause register
//  interrupted T0 saved in SaveT0.

100:    mtc0    k0, epc                 // update resume address
        nop
        MFC_REG k0, badvaddr            // (k0) = faulting virtual address
#ifdef  _MIPS64
        sll     k1, k0, 0               // Sign extend the 32-bit address.
        bne     k0, k1, 105f            // Invalid 32-bit address - generate exception.
#endif  //  _MIPS64
        tlbp                            // 3 cycle hazard
        bltz    k0, 120f                // kernel address
        srl     t0, k0, VA_SECTION-2    //
        and     t0, SECTION_MASK*4      // (t0) = section * 4
        mfc0    k1, index               // (k1) = TLB index of invalid entry
        lw      t0, SectionTable(t0)    // (t0) = ptr to block table
        bltz    k1, 130f                // not an invalid entry fault
101:    
        // (t0) = pscn (pointer to SECTION)
        srl     k1, k0, VA_BLOCK-2      // (in delay slot, but okay)
        and     k1, BLOCK_MASK*4        // (k1) = block * 4
        addu    t0, k1                  // (t0) = block table entry
        lw      t0, (t0)                // (t0) = ptr to MEMBLOCK structure
        srl     k0, VA_PAGE-2           //
        and     k0, PAGE_MASK*4         // (k0) = page # * 4
        bgez    t0, 105f                // unmapped memblock
        addu    k1, k0, t0              // (k1) = ptr to page entry
        lw      k1, mb_pages(k1)        // (k1) = page table entry
        and     k0, 0xfff8              // (k1) = even page index
        and     k1, PG_VALID_MASK       // (k1) = 0 if invalid page
        beq     zero, k1, 105f          // the page is invalid
        addu    k0, t0                  // (k0) = ptr to even page of even/odd pair
        lw      k1, mb_lock(t0)         // (k1) = block access lock
        lw      t0, CurAKey             //
        and     k1, t0                  // (k1) = 0 if access not allowed
        lw      t0, mb_pages(k0)        // (t0) = even page info
        beq     zero, k1, 105f          // access not allowed
        lw      k0, mb_pages+4(k0)      // (k0) = odd page info
        mtc0    t0, entrylo0            // set even entry to write into TLB
        mtc0    k0, entrylo1            // set odd entry to write into TLB
        nop
        nop
        tlbwi                           // write indexed entry into TLB
        nop                             // 3 cycle hazard
        nop                             //
        nop
104:
        move    k1, zero                // no atomic op in progress
        L_REG   t0, SaveT0              // restore t0 value
        eret                            //
        nop                             // errata...
        nop                             //
        eret                            //

// Invalid access. Reload k0 & k1 and return to general exception processing.

105:    mfc0    k1, cause
        b       5b
        mfc0    k0, epc

#define SECURE_VMBASE   0xc2000000

107:
        // faulting address is not in Secure Section
        // (k0) = badaddr
        // (t0) = badaddr - NK_IO_BASE
        la      k1, dwStaticMapLength   // (k1) = &dwStaticMapLength
        lw      k1, (k1)                // (k1) = dwStaticMapLength 
        subu    k1, k1, t0              // (k1) = dwStaticMapLength - (badaddr - NK_IO_BASE)
        blez    k1, 105b                // fault if out of range
        nop
        
        // the address is within IO range. Map it
        la      k1, dwStaticMapBase     // (k1) = &dwStaticMapBase
        lw      k1, (k1)                // (k1) = dwStaticMapBase
        addu    t0, t0, k1              // (t0) = badaddr - NK_IOBASE + dwStaticMapBase
        
        srl     t0, VA_PAGE+1           // clear the lower 13 bits to make the even page number
        lw      k0, TlbShift            // (k0) = (VA_PAGE+1) - PFN_SHIFT
        sllv    t0, t0, k0              // (t0) = even page number

        // add the page protection bits
        or      t0, t0, PG_DIRTY_MASK|PG_VALID_MASK|PG_NOCACHE   // (t0) = tlb entry for even page

        // now for odd page
        lw      k0, PfnIncr
        add     k1, t0, k0              // (k1) = tlb entry for odd page

        // update TLB
        mtc0    t0, entrylo0
        mtc0    k1, entrylo1

        // 3 cycle hazard
        nop
        nop
        tlbwr
        nop
        nop

        b       104b
        nop
120:
//
// Kernel address. Check if it's a mapped address
//
//      (k0) = badaddr
//

        // check if we're in IO space
        li      t0, NK_IO_BASE          // (t0) = NK_IO_BASE
        subu    t0, k0, t0              // (t0) = badaddr - NK_IO_BASE
        bgez    t0, 107b                // >= NK_IO_BASE, check IO addresses
        li      k1, SECURE_VMBASE       // (k1) = SECURE_VMBASE (delay slot)

        // check if we're in SECURE_SECTION
        subu    t0, k0, k1              // (t0) = badaddr - SECURE_VMBASE
        bltz    t0, 105b                // fault if < SECURE_VMBASE
        mfc0    k1, index               // (k1) = index register (delay slot)

        // we're in SECURE_SECTION
        // update t0 and index (if necessary) and jump back to general TLB miss handler        
        la      t0, NKSection           // (t0) = &NKSection
        bgez    k1, 101b
        nop

        // fall thru to get use random as index

// Sometimes TLB misses go to the general exception vector instead
// of the TLB miss vector. Select an entry to replace by copying random to index.

125:    mfc0    k1, random
        j       101b
        mtc0    k1, index


130:
        // index register is invalid
        // R41XX: get a random index and continue
        // others: report fault (return to general exception processing)
        lw      k1, IsR41XX
        beqz    k1, 105b
        mfc0    k1, random              // (in delay slot, but okay)
        j       101b
        mtc0    k1, index               // (delay slot)

        .end    GeneralExceptionP


        
//------------------------------------------------------------------------------
// Stack structure during API call processing.
        .struct 0
_apiArg:        .space 4 * REG_SIZE     // argument register save area (call standard, room fo 4 registers)
apiSaveRet:     .space REG_SIZE         // return value
_filler_:       .space 4         // padding. make 8 bytes aligned for MIP32
objcallstr:
apiMethod:      .space 4                // API method
svrrtnstr:
apiPrevSP:      .space 4                // previous SP if stack changed
apiMode:        .space 4                // (pMode) argument
apiSaveGp:      .space 4                // extra cpu dependent info (Global Pointer)
apiSaveRA:      .space 4                // return address
size_api_args:                          // length of stack frame
apiArg0:        .space REG_SIZE         // caller argument save area
apiArg1:        .space REG_SIZE
apiArg2:        .space REG_SIZE
apiArg3:        .space REG_SIZE

//------------------------------------------------------------------------------
// The following code is never executed. Its purpose is to support unwinding
// through the calls to ObjectCall or ServerCallReturn.
//------------------------------------------------------------------------------
NESTED_ENTRY(APICall, 0, zero)
        .set    noreorder
        .set    noat
        subu    sp, size_api_args
        sw      ra, apiSaveRA(sp)       // unwinder: (ra) = APICallReturn
        PROLOGUE_END
//
// Process an API Call or return.
//
//  (k0) = EPC (encodes the API set & method index)
//
200:    lb      t0, KNest               // (t0) = kernel nest depth
        mfc0    t1, psr                 // (t1) = processor status
        blez    t0, 5b                  // non-preemtible, API Calls not allowed
        subu    t0, k0, FIRST_METHOD
        lw      t3, BasePSR
        move    k1, zero                // reset atomic op. flag
        or      t3, 1                   // (t3) = current interrupt mask + int enable
#ifdef MIPS_HAS_FPU
        lw      t8, g_CurFPUOwner
        lw      t7, CurThdPtr
        bne     t7, t8, 201f
        nop
        la      t9, dwNKCoProcEnableBits
        lw      t9, (t9)
        or      t3, t9
201:
#endif
        mtc0    t3, psr                 // enable interrupts
  #if APICALL_SCALE == 2
        sra     t0, 1                   // (t0) = method index
  #elif APICALL_SCALE == 4              
        sra     t0, 2                   // (t0) = method index
  #else
    #error Invalid value for APICALL_SCALE
  #endif

        addu    t3, t0, 1               // (t3) = 0 if return from CALLBACK
        beq     zero, t3, MD_CBRtn
        and     t1, MODE_MASK           // (t1) = thread's execution mode (delay slot)

        and     t2, t1, 1 << PSR_PMODE
        beq     t2, zero, KPSLCall      // calling from kernel mode if = 0
        li      t4, 0                   // (t4) = prevSP, init to 0 (delay slot)

        // PSL call from User Mode
        // (t0) = iMethod
        // (t1) = caller's mode

        // special case RaiseException
        li      t2, RAISEEXCEPTION
        beq     t0, t2, PSLCallCommon   // do not switch stack if it's RaiseException

        // check trust level
        lw      t3, CurPrcPtr           // (t3) = pCurProc (in delay slot, okay to execute if branch taken)
        lb      t3, PrcTrust(t3)        // (t3) = pCurProc->bTrustLevel
        li      t2, KERN_TRUST_FULL     // (t2) = KERN_TRUST_FULL
        beq     t2, t3, PSLCallCommon   // go to common code if fully trusted
        nop

        // not fully trusted, need to perform stack switch
        lw      t3, CurThdPtr           // (t3) = pCurThread
        lw      t2, ThTlsSecure(t3)     // (t2) = pCurThread->tlsSecure
        sw      t2, ThTlsPtr(t3)        // pCurThread->tlsPtr = pCurThread->tlsSecure
        sw      t2, lpvTls              // update TLS in KPAGE

        // find the 'real' stack top (exception handler might put in faked ones)
        lw      t3, ThPcstkTop(t3)          // (t3) = pCurThread->pcstkTop
        move    t4, sp                      // (t4) = caller's SP (delay slot)
        subu    sp, t2, SECURESTK_RESERVE   // init new SP to be (tlsptr - reserve)

NextCStk:
        beq     t3, zero, CopyStack     // start copying if no call stack
        move    t6, t3                  // (t6) == current pcstk (delay slot)
        
        lw      t5, CstkAkyLast(t6)     // akyLast == 0 if exception handler put it in

        beq     t5, zero, NextCStk      // get to next pcstk if akyLast == 0
        lw      t3, CstkNext(t6)        // (t3) == pcstk->pcstkNext (delay slot)

        // found a callstack, update sp (DEBUGCHK (pcstk->dwPrevSP != 0))
        lw      sp, CstkPrevSP(t6)

CopyStack:
        // copy arguments from caller stack to the new stack
        // (t4) == old SP
        // (sp) == new SP
        subu    sp, MAX_PSL_ARGS                // MAX_PSL_ARGS == 56

        // don't need to copy the 1st 4 since they are in register
        L_REG      t2, 4 * REG_SIZE(t4)
        L_REG      t3, 5 * REG_SIZE(t4)
        L_REG      t5, 6 * REG_SIZE(t4)
        L_REG      t6, 7 * REG_SIZE(t4)
        S_REG      t2, 4 * REG_SIZE(sp)
        S_REG      t3, 5 * REG_SIZE(sp)
        S_REG      t5, 6 * REG_SIZE(sp)
        S_REG      t6, 7 * REG_SIZE(sp)
        L_REG      t2, 8 * REG_SIZE(t4)
        L_REG      t3, 9 * REG_SIZE(t4)
        L_REG      t5, 10 * REG_SIZE(t4)
        L_REG      t6, 11 * REG_SIZE(t4)
        S_REG      t2, 8 * REG_SIZE(sp)
        S_REG      t3, 9 * REG_SIZE(sp)
        S_REG      t5, 10 * REG_SIZE(sp)
        S_REG      t6, 11 * REG_SIZE(sp)
        L_REG      t2, 12 * REG_SIZE(t4)
        L_REG      t3, 13 * REG_SIZE(t4)
        S_REG      t2, 12 * REG_SIZE(sp)
        
        b          PSLCallCommon
        S_REG      t3, 13 * REG_SIZE(sp)              // (delay slot)

KPSLCall:
        // calling from KMODE, check PerformCallBack
        // (t0) = method
        // (t1) = KERNEL_MODE
        // (t4) = 0
        li      t3, PERFORMCALLBACK
        beq     t0, t3, DoPerformCallBack
        nop


PSLCallCommon:        
        // (t0) = iMethod
        // (t1) = caller's mode
        // a0-a3 = arguments
        // (t4) = previous SP
        
        subu    sp, size_api_args       // make room for new args + temps

        // Save api arguments onto the stack
        S_REG   a0, apiArg0(sp)
        S_REG   a1, apiArg1(sp)
        S_REG   a2, apiArg2(sp)
        S_REG   a3, apiArg3(sp)

        // setup ObjectCall arguments
        sw      t1, apiMode(sp)         // what mode we're from
        sw      t4, apiPrevSP(sp)       // previous AP
        sw      t0, apiMethod(sp)       // method
        sw      ra, apiSaveRA(sp)       // return address

        jal     ObjectCall
        addu    a0, sp, objcallstr      // (a0) = ptr to OBJCALLSTRUCT  (delay slot)
//
// Invoke server function. If the thread is running in kernel mode, then
// we just call the function directly from here.
//
//  (v0) = address of server function
//
        L_REG   a0, apiArg0(sp)         // reload argument registers.
        L_REG   a1, apiArg1(sp)
        L_REG   a2, apiArg2(sp)
        L_REG   a3, apiArg3(sp)

        // always run PSL calls in KMODE
        jal     v0
        addu    sp, size_api_args       // remove extra stuff from the stack

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
ALTERNATE_ENTRY(APICallReturn)
        subu    sp, size_api_args       // recreate entry frame
//
//  (v0) = api return value (must be preserved)

        S_REG   v0, apiSaveRet(sp)      // save return value
        jal     ServerCallReturn        
        addu    a0, sp, svrrtnstr       // (a0) = ptr to SVRRTNSTRUCT   (delay slot)
        
        lw      t0, apiMode(sp)         // (t0) = mode to return to
        move    ra, v0                  // (ra) = return address
        and     t1, t0, 1 << PSR_PMODE
        beq     t1, zero, 255f          // returning to kernel mode
        L_REG   v0, apiSaveRet(sp)      // restore return value (delay slot)

//
// return to user mode, check trust level and switch stack if necessary
//
        lw      t2, apiPrevSP(sp)       // (t2) = previous SP
        beq     t2, zero, UPSLRtnCommon // go to common code if prevsp == 0
        move    t4, ra                  // (t4) = user mode return address (delay slot)

        // need to perform stack switch
        //   (t2) = previous SP
        lw      t1, CurThdPtr           // (t1) = pCurThread
        lw      t3, ThTlsNonSecure(t1)  // (t3) = pCurThread->tlsNonSecure
        sw      t3, ThTlsPtr(t1)        // pCurThread->tlsPtr = pCurThread->tlsNonSecure
        sw      t3, lpvTls              // update TLS in KPAGE

        subu    sp, t2, size_api_args   // (restore SP, but subtract siz_api_args, will be add back later

UPSLRtnCommon:
//
// Return to user mode. To do this: build a new PSR value from the thread's mode
// and BasePSR. This must be done with interrupts disabled so that BasePSR is not
// changing.
//
//  (t0) = new mode bits
//  (t4) = return address
//
        CP0_STOP_PREFETCH(mtc0, zero, psr, t1); // all interrupts off
        nop                             // 3 cycle hazard
        nop
        addu    sp, size_api_args
        lw      t1, BasePSR

        mtc0    t4, epc

#ifdef MIPS_HAS_FPU
        lw      t8, g_CurFPUOwner
        lw      t7, CurThdPtr
        bne     t7, t8, 251f
        nop
        la      t9, dwNKCoProcEnableBits
        lw      t9, (t9)
        or      t1, t9
251:
#endif
        or      t1, t0                  // (t1) = merged status
        mtc0    t1, psr                 // reload status
        nop

        nop
        nop
        eret

// Return to kernel mode.

255:    j       ra
        addu    sp, size_api_args       // remove extra stuff from the stack (delay slot)

DoPerformCallBack:
        // (t1) = caller's mode
        // a0-a3 = arguments
        subu    sp, size_api_args       // make room for new args + temps

        // Save api arguments and ra onto the stack
        S_REG   a0, apiArg0(sp)
        S_REG   a1, apiArg1(sp)
        S_REG   a2, apiArg2(sp)
        S_REG   a3, apiArg3(sp)

        // setup PerformCallbackExt arguments
        sw      t1, apiMode(sp)
        addu    t3, sp, size_api_args   // (t3) == original SP
        sw      t3, apiPrevSP(sp)
        sw      ra, apiSaveRA(sp)
        
        
        // call PerformCallBackExt
        jal     PerformCallBackExt
        addu    a0, sp, objcallstr      // (arg0) == ptr to OBJCALLSTRUCT (delay slot)

        // v0 == function to call
        
        // restore arguments and return address
        L_REG   a0, apiArg0(sp)
        L_REG   a1, apiArg1(sp)
        L_REG   a2, apiArg2(sp)
        L_REG   a3, apiArg3(sp)

        move    t4, v0                  // (t4) = function to call
        
        // check if need stack switch
        lw      t3, apiPrevSP(sp)       // (t3) == new SP if != 0
        beq     t3, zero, CBCommon
        lw      t0, apiMode(sp)         // (t0) == mode to call to (delay slot)

        // stack switch is required...

        // (t0) = mode to return to
        // (t3) = new SP
        // (t4) = function to call

        // must save callee saved registers in secure stack or security risk.
        // NOTE: must do this before updating sp and tls, or DemmandCommit might fail
        //      pcstk->dwPrevSP has been updated by PerformCallBackExt to reserve enough
        //      room for the registers
        addu    sp, sp, size_api_args - CALLEE_SAVED_REGS    // (sp) = old SP - room enough to save callee saved registers
        S_REG   s0, 0 * REG_SIZE(sp)
        S_REG   s1, 1 * REG_SIZE(sp)
        S_REG   s2, 2 * REG_SIZE(sp)
        S_REG   s3, 3 * REG_SIZE(sp)
        S_REG   s4, 4 * REG_SIZE(sp)
        S_REG   s5, 5 * REG_SIZE(sp)
        S_REG   s6, 6 * REG_SIZE(sp)
        S_REG   s7, 7 * REG_SIZE(sp)
        S_REG   s8, 8 * REG_SIZE(sp)
        S_REG   gp, 9 * REG_SIZE(sp)

        // update TLS
        // (t1) == pCurThread
        lw      t1, CurThdPtr
        lw      t2, ThTlsNonSecure(t1)  // (t2) = pCurThread->tlsNonSecure
        sw      t2, ThTlsPtr(t1)        // pCurThread->tlsPtr = pCurThread->tlsNonSecure
        sw      t2, lpvTls              // update TLS in KPAGE

        // change stack
        addu    v0, sp, CALLEE_SAVED_REGS // v0 == old SP
        subu    t3, MAX_PSL_ARGS          // room for arguments on new stack
        subu    sp, t3, size_api_args     // (sp) is now the new stack with room for size_api_args

        // copy performcallback arguments (assume a max of 8)
        L_REG      t1, 4 * REG_SIZE(v0)
        L_REG      t2, 5 * REG_SIZE(v0)
        L_REG      t5, 6 * REG_SIZE(v0)
        L_REG      t6, 7 * REG_SIZE(v0)
        S_REG      t1, 4 * REG_SIZE(t3)
        S_REG      t2, 5 * REG_SIZE(t3)
        S_REG      t5, 6 * REG_SIZE(t3)
        S_REG      t6, 7 * REG_SIZE(t3)

CBCommon:
        // (t0) = mode to return to
        // (t4) = function to call
        and     t1, t0, 1 << PSR_PMODE
        bne     t1, zero, UPSLRtnCommon         // callback to user mode?
        li      ra, SYSCALL_RETURN              // return address is a trap 

        // calling the callback in kmode (will override ra)
        jal     t4
        addu    sp, size_api_args       // remove extra stuff from the stack (delay slot)
        
MD_CBRtn:
        // (t1) = caller's mode
        // check if we need to switch stack
        lw      t2, CurThdPtr           // (t2) == pCurThread
        lw      t3, ThPcstkTop(t2)      // (t3) = pCurThread->pcstkTop
        lw      t0, CstkPrevSP(t3)      // (t0) = previous SP

        beq     t0, 0, NoSwitch
        subu    sp, size_api_args       // make room for svrrtnstrut (delay slot)

        // need to switch stack
        // update TLS
        lw      t3, ThTlsSecure(t2)     // (t3) = secure TLS
        sw      t3, ThTlsPtr(t2)        // pCurThread->tls = pCurThread->tlsSecure
        sw      t3, lpvTls              // update TLS in KPAGE

        // restore callee saved registers
        // (t0) == pCurThread->pcstkTop->dwPrevSP
        L_REG   s0, 0 * REG_SIZE(t0)
        L_REG   s1, 1 * REG_SIZE(t0)
        L_REG   s2, 2 * REG_SIZE(t0)
        L_REG   s3, 3 * REG_SIZE(t0)
        L_REG   s4, 4 * REG_SIZE(t0)
        L_REG   s5, 5 * REG_SIZE(t0)
        L_REG   s6, 6 * REG_SIZE(t0)
        L_REG   s7, 7 * REG_SIZE(t0)
        L_REG   s8, 8 * REG_SIZE(t0)
        L_REG   gp, 9 * REG_SIZE(t0)
        
        subu    sp, t0, size_api_args - CALLEE_SAVED_REGS   // change SP, make room for svrrtnstruct

NoSwitch:
        // save return value
        S_REG   v0, apiSaveRet(sp)

        jal     CallbackReturn
        addu    a0, sp, apiSaveGp       // arg0 = ptr to linkage (delay slot)

        // (v0) == return address
        move    ra, v0
        
        // restore return value
        L_REG   v0, apiSaveRet(sp)

        // return to caller in kmode
        j       ra
        addu    sp, sp, size_api_args   // pop the stack (delay slot)

END_REGION(GeneralException_End)
        .set at
        .set reorder
        .end    APICall


//------------------------------------------------------------------------------
// This routine services interrupts which have been disabled. It masks the
// interrupt enable bit in the PSR to prevent further interrupts and treats
// the interrupt as a NOP.
//
//  Entry   (a0) = interrupt level * 4
//  Exit    (v0) = SYSINTR_NOP
//  Uses    a0, a1, v0
//------------------------------------------------------------------------------
LEAF_ENTRY(DisabledInterruptHandler)
        .set noreorder
        srl     a0, 2
        li      v0, 0x400
        sllv    a0, v0, a0
        li      v0, -1
        xor     a0, v0                  // (a0) = ~(intr. mask)
        mfc0    v0, psr                 
        lw      a1, BasePSR             
        and     v0, a0                  // (v0) = psr w/intr disabled
        mtc0    v0, psr                 
        and     a1, a0                  // (a0) = base PSR w/intr disabled
        sw      a1, BasePSR
        j       ra
        li      v0, SYSINTR_NOP
        
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
        subu    sp, ContextFrameLength  // (sp) = ptr to CONTEXT buffer
        sw      zero, 0(sp)             // make sure that the stack is addressable
        .end CaptureContext
        
        NESTED_ENTRY(xxCaptureContext, ContextFrameLength, zero)
        .set noreorder
        .set noat
        S_REG   sp, CxIntSp(sp)         // fixed up by ExceptionDispatch
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
            
        li      t0, CONTEXT_FULL
        sw      t0, CxContextFlags(sp)
        jal     ExceptionDispatch
        move    a0, sp                  // (a0) = context record argument

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
        lw      a0, BasePSR             // (a0) = global default status value
        lw      v0, CxPsr(sp)           // (v0) = thread's default status
        L_REG   ra, CxIntRa(sp)
        or      v0, a0                  // (v0) = thread + global status

#ifdef MIPS_HAS_FPU
        lw      a0, g_CurFPUOwner
        lw      gp, CurThdPtr
        bne     a0,gp, ccnofp
        nop
        la      a0, dwNKCoProcEnableBits
        lw      a0, (a0)
        or      v0, a0
ccnofp:
#endif
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
        nop
        nop
        eret                            // restore user status
        nop
        nop
        eret

        .set at
        .set reorder

        .end xxCaptureContext



//------------------------------------------------------------------------------
//
// Define call frame for calling exception handlers.
//
//------------------------------------------------------------------------------

        .struct 0
#ifdef  _MIPS64
CfArg:  .space  4 * 8                   // argument register save area
CfRa:   .space  8                       // saved return address
                                                                                // Must be 8 byte aligned
CfFrameLength:                          // length of stack frame
CfA0:   .space  8                       // caller argument save area
CfA1:   .space  8                       //
CfA2:   .space  8                       //
CfA3:   .space  8                       //
CfExr:  .space  8                       // address of exception routine
Cfcstk: .space  8                       // pcstk for user-mode handler
CfPsr:  .space  8                       // mode to execute handler in

#else   //  _MIPS64
CfArg:  .space  4 * 4                   // argument register save area
CfRa:   .space  4                       // saved return address
_Pad_:  .space  4                       // pad stack frame to stack alignment (8 bytes)

CfFrameLength:                          // length of stack frame
CfA0:   .space  4                       // caller argument save area
CfA1:   .space  4                       //
CfA2:   .space  4                       //
CfA3:   .space  4                       //
CfExr:  .space  4                       // address of exception routine
Cfcstk: .space  4                       // pcstk for user-mode handler
CfPsr:  .space  4                       // mode to execute handler in
#endif  //  _MIPS64

        SBTTL("Execute Handler for Exception")
//------------------------------------------------------------------------------
//
// EXCEPTION_DISPOSITION
// RtlpExecuteHandlerForException (
//    IN PEXCEPTION_RECORD ExceptionRecord,
//    IN ULONG EstablisherFrame,
//    IN OUT PCONTEXT ContextRecord,
//    IN OUT PDISPATCHER_CONTEXT DispatcherContext,
//    IN PEXCEPTION_ROUTINE ExceptionRoutine,
//    IN OUT PCALLSTACK pcstk,
//    IN ULONG ExceptionMode
//    )
//
// Routine Description:
//
//    This function allocates a call frame, stores the establisher frame
//    pointer in the frame, establishes an exception handler, and then calls
//    the specified exception handler as an exception handler. If a nested
//    exception occurs, then the exception handler of this function is called
//    and the establisher frame pointer is returned to the exception dispatcher
//    via the dispatcher context parameter. If control is returned to this
//    routine, then the frame is deallocated and the disposition status is
//    returned to the exception dispatcher.
//
// Arguments:
//
//    ExceptionRecord (a0) - Supplies a pointer to an exception record.
//
//    EstablisherFrame (a1) - Supplies the frame pointer of the establisher
//       of the exception handler that is to be called.
//
//    ContextRecord (a2) - Supplies a pointer to a context record.
//
//    DispatcherContext (a3) - Supplies a pointer to the dispatcher context
//       record.
//
//    ExceptionRoutine (4 * REG_SIZE(sp)) - supplies a pointer to the exception handler
//       that is to be called.
//
//    pcstk (5 * REG_SIZE(sp)) - callstack structure for user-mode handler
//
//    ExceptionMode (6 * REG_SIZE(sp)) - PSR value for running ExceptionRoutine
//
// Return Value:
//
//    The disposition value returned by the specified exception handler is
//    returned as the function value.
//
//------------------------------------------------------------------------------
EXCEPTION_HANDLER(RtlpExceptionHandler)

NESTED_ENTRY(RtlpExecuteHandlerForException, CfFrameLength, zero)

        subu    sp,sp,CfFrameLength     // allocate stack frame
        S_REG   ra,CfRa(sp)             // save return address
        PROLOGUE_END

        lw      t1, CfPsr(sp)           // (t1) = target status
        lw      t0, CfExr(sp)           // get address of exception routine
        and     t2, t1, 1 << PSR_PMODE  // (t2) = previous execution mode
        S_REG   a3, CfA3(sp)            // save address of dispatcher context
        bne     t2, zero, 20f           // must switch to user mode
        jal     t0                      // call exception exception handler
10:     L_REG   ra,CfRa(sp)             // restore return address
        addu    sp,sp,CfFrameLength     // deallocate stack frame
        j       ra                      // return

        .set    noreorder
20:     
        CP0_STOP_PREFETCH(mtc0, zero, psr, t4);    // disable interrupts

        lw      t4, Cfcstk(sp)          // (t4) = pcstk
        la      t3, 10b                 // (t3) = address to return to
        lw      t2, CurThdPtr           // (t2) = pCurThread
        sw      t3, CstkRa(t4)          // pcstk->retAddr = (t3)
        
        lw      t3, BasePSR
        or      t3, t1
        mtc0    t3, psr
        li      ra, SYSCALL_RETURN

        mtc0    t0, epc
        sw      t4, ThPcstkTop(t2)      // pCurThread->pcstkTop = pcstk
        nop
        eret                            // switch to user mode & jump to handler
        nop                             //
        nop                             // errata
        eret                            //

        .set reorder

        .end    RtlpExecuteHandlerForException

        
        
        SBTTL("Local Exception Handler")
//------------------------------------------------------------------------------
//
// EXCEPTION_DISPOSITION
// RtlpExceptionHandler (
//    IN PEXCEPTION_RECORD ExceptionRecord,
//    IN ULONG EstablisherFrame,
//    IN OUT PCONTEXT ContextRecord,
//    IN OUT PDISPATCHER_CONTEXT DispatcherContext
//    )
//
// Routine Description:
//
//    This function is called when a nested exception occurs. Its function
//    is to retrieve the establisher frame pointer from its establisher's
//    call frame, store this information in the dispatcher context record,
//    and return a disposition value of nested exception.
//
// Arguments:
//
//    ExceptionRecord (a0) - Supplies a pointer to an exception record.
//
//    EstablisherFrame (a1) - Supplies the frame pointer of the establisher
//       of this exception handler.
//
//    ContextRecord (a2) - Supplies a pointer to a context record.
//
//    DispatcherContext (a3) - Supplies a pointer to the dispatcher context
//       record.
//
// Return Value:
//
//    A disposition value ExceptionNestedException is returned if an unwind
//    is not in progress. Otherwise a value of ExceptionContinueSearch is
//    returned.
//
//------------------------------------------------------------------------------
LEAF_ENTRY(RtlpExceptionHandler)
        .set noreorder
        
        lw      t0,ErExceptionFlags(a0) // get exception flags
        nop
        and     t0,t0,EXCEPTION_UNWIND  // check if unwind in progress
        .set reorder
        bne     zero,t0,10f             // if neq, unwind in progress

//
// Unwind is not in progress - return nested exception disposition.
//
        L_REG   t0,CfA3 - CfA0(a1)      // get dispatcher context address
        li      v0,ExceptionNestedException // set disposition value
        .set noreorder
        lw      t1,DcEstablisherFrame(t0) // copy the establisher frame pointer
        nop
        sw      t1,DcEstablisherFrame(a3) // to current dispatcher context
        .set reorder
        j       ra                      // return

//
// Unwind is in progress - return continue search disposition.
//
10:     li      v0,ExceptionContinueSearch // set disposition value
        j       ra                      // return

        .end    RtlpExceptionHandler)

        

        SBTTL("Execute Handler for Unwind")
        EXCEPTION_HANDLER(RtlpUnwindHandler)
//------------------------------------------------------------------------------
//
// EXCEPTION_DISPOSITION
// RtlpExecuteHandlerForUnwind (
//    IN PEXCEPTION_RECORD ExceptionRecord,
//    IN PVOID EstablisherFrame,
//    IN OUT PCONTEXT ContextRecord,
//    IN OUT PVOID DispatcherContext,
//    IN PEXCEPTION_ROUTINE ExceptionRoutine
//    IN OUT PCALLSTACK pcstk,
//    )
//
// Routine Description:
//
//    This function allocates a call frame, stores the establisher frame
//    pointer and the context record address in the frame, establishes an
//    exception handler, and then calls the specified exception handler as
//    an unwind handler. If a collided unwind occurs, then the exception
//    handler of of this function is called and the establisher frame pointer
//    and context record address are returned to the unwind dispatcher via
//    the dispatcher context parameter. If control is returned to this routine,
//    then the frame is deallocated and the disposition status is returned to
//    the unwind dispatcher.
//
// Arguments:
//
//    ExceptionRecord (a0) - Supplies a pointer to an exception record.
//
//    EstablisherFrame (a1) - Supplies the frame pointer of the establisher
//       of the exception handler that is to be called.
//
//    ContextRecord (a2) - Supplies a pointer to a context record.
//
//    DispatcherContext (a3) - Supplies a pointer to the dispatcher context
//       record.
//
//    ExceptionRoutine (4 * REG_SIZE(sp)) - supplies a pointer to the exception handler
//       that is to be called.
//
//    pcstk (5 * REG_SIZE(sp)) - callstack structure for user-mode handler
//
// Return Value:
//
//    The disposition value returned by the specified exception handler is
//    returned as the function value.
//
//------------------------------------------------------------------------------
NESTED_ENTRY(RtlpExecuteHandlerForUnwind, CfFrameLength, zero)

        subu    sp,sp,CfFrameLength     // allocate stack frame
        S_REG   ra,CfRa(sp)             // save return address
        PROLOGUE_END

        lw      t1, CxPsr(a2)           // (t1) = target status
        lw      t0,CfExr(sp)            // get address of exception routine
        and     t2, t1, 1 << PSR_PMODE  // (t2) = previous execution mode
        S_REG   a3,CfA3(sp)             // save address of dispatcher context
        bne     t2, zero, 20f           // must switch to user mode
        jal     t0                      // call exception exception handler
10:     L_REG   ra,CfRa(sp)             // restore return address
        addu    sp,sp,CfFrameLength     // deallocate stack frame
        j       ra                      // return

        .set    noreorder
20:     
        CP0_STOP_PREFETCH(mtc0, zero, psr, t4);      // disable interrupts

        lw      t4, Cfcstk(sp)          // (t4) = pcstk
        la      t3, 10b                 // (t3) = address to return to
        lw      t2, CurThdPtr           // (t2) = pCurThread
        sw      t3, CstkRa(t4)          // pcstk->retAddr = (t3)
        
        lw      t3, BasePSR
        or      t3, t1
        mtc0    t3, psr
        li      ra, SYSCALL_RETURN

        mtc0    t0, epc
        sw      t4, ThPcstkTop(t2)      // pCurThread->pcstkTop = pcstk
        nop
        eret                            // switch to user mode & jump to handler
        nop                             //
        nop                             // errata
        eret                            //

        .set reorder

        .end    RtlpExecuteHandlerForUnwind


        SBTTL("Local Unwind Handler")
//------------------------------------------------------------------------------
//
// EXCEPTION_DISPOSITION
// RtlpUnwindHandler (
//    IN PEXCEPTION_RECORD ExceptionRecord,
//    IN PVOID EstablisherFrame,
//    IN OUT PCONTEXT ContextRecord,
//    IN OUT PVOID DispatcherContext
//    )
//
// Routine Description:
//
//    This function is called when a collided unwind occurs. Its function
//    is to retrieve the establisher dispatcher context, copy it to the
//    current dispatcher context, and return a disposition value of nested
//    unwind.
//
// Arguments:
//
//    ExceptionRecord (a0) - Supplies a pointer to an exception record.
//
//    EstablisherFrame (a1) - Supplies the frame pointer of the establisher
//       of this exception handler.
//
//    ContextRecord (a2) - Supplies a pointer to a context record.
//
//    DispatcherContext (a3) - Supplies a pointer to the dispatcher context
//       record.
//
// Return Value:
//
//    A disposition value ExceptionCollidedUnwind is returned if an unwind is
//    in progress. Otherwise, a value of ExceptionContinueSearch is returned.
//
//------------------------------------------------------------------------------
LEAF_ENTRY(RtlpUnwindHandler)
        .set noreorder
        lw      t0,ErExceptionFlags(a0) // get exception flags
        nop
        and     t0,t0,EXCEPTION_UNWIND  // check if unwind in progress
        .set reorder
        beq     zero,t0,10f             // if eq, unwind not in progress

//
// Unwind is in progress - return collided unwind disposition.
//
        L_REG   t0,CfA3 - CfA0(a1)      // get dispatcher context address
        li      v0,ExceptionCollidedUnwind // set disposition value
        lw      t1,DcControlPc(t0)      // Copy the establisher frames'
        lw      t2,DcFunctionEntry(t0)  // dispatcher context to the current
        lw      t3,DcEstablisherFrame(t0) // dispatcher context
        lw      t4,DcContextRecord(t0)  //
        sw      t1,DcControlPc(a3)      //
        sw      t2,DcFunctionEntry(a3)  //
        sw      t3,DcEstablisherFrame(a3) //
        sw      t4,DcContextRecord(a3)  //
        j       ra                      // return

//
// Unwind is not in progress - return continue search disposition.
//
10:     li      v0,ExceptionContinueSearch // set disposition value
        j       ra                      // return
        .end    RtlpUnwindHandler

