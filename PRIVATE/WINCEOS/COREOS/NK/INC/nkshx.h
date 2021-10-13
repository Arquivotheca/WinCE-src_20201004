//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++ BUILD Version: 0016    Increment this if a change has global effects


Module Name:

    nkshx.h

Abstract:

    User-mode visible SH3 specific structures and constants

--*/

#ifndef _NKSHX_
#define _NKSHX_

// begin_ntddk begin_nthal

#if defined(SHx)

extern DWORD OEMNMI (void);

#define _cdecl

// begin_ntddk begin_nthal
//
// Define size of kernel mode stack.
//

#define KERNEL_STACK_SIZE 2048

//
// Define length of exception code dispatch vector.
//

#define XCODE_VECTOR_LENGTH 32

#define COMPRESSED_PDATA        // define to enable compressed pdata format
#define IntRa                       PR
#define IntSp                       R15
#define INST_SIZE                   2

//
// Define length of interrupt vector table.
//
#define MAXIMUM_VECTOR 32

#define CALLEE_SAVED_REGS       8           // 7 regs, (r8 - r14) add 1 to make 8 byte alignment
#define REG_R8                                  0
#define REG_R9                                  1
#define REG_R10                                 2
#define REG_R11                                 3
#define REG_R12                                 4
#define REG_R13                                 5
#define REG_R14                                 6

#define CONTEXT_TO_PROGRAM_COUNTER(Context)     ((Context)->Fir)

#define CONTEXT_TO_STACK_POINTER(Context)       ((Context)->R15)
#define CONTEXT_TO_RETURN_ADDRESS(Context)      ((Context)->IntRa)

#define CONTEXT_TO_PARAM_1(Context) ((Context)->R4)
#define CONTEXT_TO_PARAM_2(Context) ((Context)->R5)
#define CONTEXT_TO_PARAM_3(Context) ((Context)->R6)
#define CONTEXT_TO_PARAM_4(Context) ((Context)->R7)

#define CONTEXT_TO_RETVAL(Context)              ((Context)->R0)

#define CONTEXT_LENGTH (sizeof(CPUCONTEXT))
#define CONTEXT_ALIGN (sizeof(ULONG))
#define CONTEXT_ROUND (CONTEXT_ALIGN - 1)

#define STACK_ALIGN         4           // 4-byte alignment for stack

/* Query & set thread's kernel vs. user mode state */
#define KERNEL_MODE     0x40
#define USER_MODE       0x00
#define SR_MODE_BITS    24

//
// thread context translation
//
#define THRD_CTX_TO_PC(pth)         ((pth)->ctx.Fir)
#define THRD_CTX_TO_SP(pth)         ((pth)->ctx.R15)
#define THRD_CTX_TO_PARAM_1(pth)    ((pth)->ctx.R4)
#define THRD_CTX_TO_PARAM_2(pth)    ((pth)->ctx.R5)

/* Query & set kernel vs. user mode state via Thread Context */
#define GetThreadMode(pth) (((pth)->ctx.Psr >> SR_MODE_BITS) & 0x40)
#define SetThreadMode(pth, mode) \
        ((pth)->ctx.Psr = ((pth)->ctx.Psr & 0x3fffffff) | ((mode)<<SR_MODE_BITS))

/* Query & set kernel vs. user mode state via Context */
#define GetContextMode(pctx) (((pctx)->Psr >> SR_MODE_BITS) & 0x40)
#define SetContextMode(pctx, mode)  \
        ((pctx)->Psr = ((pctx)->Psr & 0x3fffffff) | ((mode)<<SR_MODE_BITS))


//
// hardware exception id
//
#define ID_TLB_MISS_LOAD            2
#define ID_TLB_MISS_STORE           3
#define ID_TLB_MODIFICATION         4
#define ID_TLB_PROTECTION_LOAD      5
#define ID_TLB_PROTECTION_STORE     6
#define ID_ADDRESS_ERROR_LOAD       7
#define ID_ADDRESS_ERROR_STORE      8
#define ID_FPU_EXCEPTION            9
// 10 unused
#define ID_BREAK_POINT              11
#define ID_RESERVED_INSTR           12
#define ID_ILLEGAL_INSTRUCTION      13
// 14 unused
#define ID_HARDWARE_BREAK           15

#define MD_MAX_EXCP_ID              15

        
// begin_nthal
//
// Define address space layout as defined by SHx memory management.
//

#define KUSEG_BASE 0x0                  // base of user segment
#define KSEG0_BASE 0x80000000           // base of cached kernel physical
#define KSEG1_BASE 0xa0000000           // base of uncached kernel physical
#define KSEG2_BASE 0xc0000000           // base of cached kernel virtual
// end_nthal


//
// Define SHx exception handling structures and function prototypes.
//
// Function table entry structure definition.
//

typedef struct _RUNTIME_FUNCTION {
    ULONG BeginAddress;
    ULONG EndAddress;
    PEXCEPTION_ROUTINE ExceptionHandler;
    PVOID HandlerData;
    ULONG PrologEndAddress;
} RUNTIME_FUNCTION, *PRUNTIME_FUNCTION;

//
// Scope table structure definition.
//

typedef struct _SCOPE_TABLE {
    ULONG Count;
    struct
    {
        ULONG BeginAddress;
        ULONG EndAddress;
        ULONG HandlerAddress;
        ULONG JumpTarget;
    } ScopeRecord[1];
} SCOPE_TABLE, *PSCOPE_TABLE;

//
// Runtime Library function prototypes.
//

VOID
RtlCaptureContext (
    OUT PCONTEXT ContextRecord
    );

//
// Define C structured exception handing function prototypes.
//

struct _EXCEPTION_POINTERS;

typedef
LONG
(*EXCEPTION_FILTER) (
    ULONG EstablisherFrame,
    struct _EXCEPTION_POINTERS *ExceptionPointers
    );

typedef
VOID
(*TERMINATION_HANDLER) (
    ULONG EstablisherFrame,
    BOOLEAN is_abnormal
    );

// begin_ntddk begin_nthal

#define CPUCONTEXT CONTEXT /* only need integer context */
#define retValue ctx.R0
#define ARG0    ctx.R4
#define SetThreadIP(pth, addr) ((pth)->ctx.Fir = (ULONG)(addr))
#define GetThreadIP(pth) ((pth)->ctx.Fir)


/* Kernel data area
 *
 * This area is always mapped into two well known locations of
 * the virtual memory space. One in user space which is read-only
 * and one in the kernel space which is read-write.
 *
 *BUGBUG: currently must be in sync with kapi.h for sharing with apis.c
 */

struct KDataStruct {

#include "kdata_common.h"

    // CPU Dependent part uses offset 0x2a0 - 0x300, and after aInfo
    DWORD       dwPadding[24];   /* 0x2a0: padding */

    // aInfo MUST BE AT OFFSET 0x300
    DWORD   aInfo[32];      /* 0x300 - misc. kernel info */
                            /* 0x380 - interlocked api code */
                            /* 0x400 - end */
};  /* KDataStruct */

__inline IsPSLBoundary (DWORD dwAddr)
{
    // user UserKData (), instead of KData directly, for this can be called in user mode
    struct KDataStruct *pKData = (struct KDataStruct *) PUserKData;
    return (dwAddr == SYSCALL_RETURN)
        || (dwAddr == pKData->pAPIReturn)
        || (dwAddr+INST_SIZE == SYSCALL_RETURN)
        || (dwAddr+INST_SIZE == pKData->pAPIReturn);
}

/* Osaccess and Kdstub need to have KData redefined.  Moved the
 * definition out. */
#ifndef KData
extern struct KDataStruct KData;
#endif

extern BYTE IntrPrio[];
extern DWORD InterruptTable[];
#define InSysCall()         (g_pKData->cNest != 1)

extern void INTERRUPTS_ON(void);
extern void INTERRUPTS_OFF(void);

#ifdef InterlockedCompareExchange
#undef InterlockedCompareExchange
#endif

#define InterlockedDecrement(lpAddend) \
        (InterlockedExchangeAdd((lpAddend), -1) - 1)
#define InterlockedExchangeAdd \
        ((LONG (*)(LONG volatile *lpAddend, LONG value))(PUserKData+0x3f0))
#define InterlockedIncrement \
        ((LONG (*)(LONG volatile *lpAddend))(PUserKData+0x3e0))
#define InterlockedTestExchange(Target, oldValue, newValue) \
    InterlockedCompareExchange((Target), (newValue), (oldValue))
#define InterlockedCompareExchange \
        ((LONG (*)(LONG volatile *Target, LONG Exchange, LONG Comperand))\
        (PUserKData+0x3d0))
#define InterlockedExchange \
        ((LONG (*)(LONG volatile *Target, LONG Value))(PUserKData+0x3c0))

#define InterlockedPushList \
        ((void *(*)(void *pHead, void *pItem))(PUserKData+0x3b0))
#define InterlockedPopList \
        ((void *(*)(void *pHead))(PUserKData+0x390))

#define FIRST_INTERLOCK 0x380

// Defines for CPU specific IDs.
#define PROCESSOR_ARCHITECTURE PROCESSOR_ARCHITECTURE_SHX
#define THISCPUID IMAGE_FILE_MACHINE_SH4

#endif // defined(SHx)
// end_ntddk end_nthal

#endif // _NKSHX_
