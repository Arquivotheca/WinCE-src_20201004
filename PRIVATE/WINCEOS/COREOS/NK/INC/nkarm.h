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

    nkarm.h

Abstract:

    User-mode visible ARM specific structures and constants

--*/

#ifndef _NKARM_
#define _NKARM_

// begin_ntddk begin_nthal

#if defined(ARM)

#define _cdecl


typedef struct _CPUCONTEXT {
    ULONG Psr;
    ULONG R0;
    ULONG R1;
    ULONG R2;
    ULONG R3;
    ULONG R4;
    ULONG R5;
    ULONG R6;
    ULONG R7;
    ULONG R8;
    ULONG R9;
    ULONG R10;
    ULONG R11;
    ULONG R12;
    ULONG Sp;
    ULONG Lr;
    ULONG Pc;

    // VFP registers
    ULONG Fpscr;        // floating point status register
    ULONG FpExc;        // the exception register
    ULONG S[NUM_VFP_REGS+1];    // fstmx/fldmx requires n+1 registers
    ULONG FpExtra[NUM_EXTRA_CONTROL_REGS];
} CPUCONTEXT, *PCPUCONTEXT;


#define CONTEXT_TO_PROGRAM_COUNTER(Context) ((Context)->Pc)

#define CONTEXT_TO_FRAME_POINTER(Context) ((Context)->Sp)

#define CONTEXT_TO_PARAM_1(Context) ((DWORD)(Context)->R0)
#define CONTEXT_TO_PARAM_2(Context) ((DWORD)(Context)->R1)
#define CONTEXT_TO_PARAM_3(Context) ((DWORD)(Context)->R2)
#define CONTEXT_TO_PARAM_4(Context) ((DWORD)(Context)->R3)

#define CALLEE_SAVED_REGS       (8 * sizeof (ULONG))        // (r4 - r11)
#define CONTEXT_LENGTH (sizeof(CPUCONTEXT))
#define CONTEXT_ALIGN (sizeof(ULONG))
#define CONTEXT_ROUND (CONTEXT_ALIGN - 1)

// ARM processor modes
#define USER_MODE   0x10    // 0b10000
#define FIQ_MODE    0x11    // 0b10001
#define IRQ_MODE    0x12    // 0b10010
#define SVC_MODE    0x13    // 0b10011
#define ABORT_MODE  0x17    // 0b10111
#define UNDEF_MODE  0x1b    // 0b11011
#define SYSTEM_MODE 0x1f    // 0b11111

// Other state bits in the processor status register
#define THUMB_STATE 0x20
#define FIQ_DISABLE 0x40
#define IRQ_DISABLE 0x80


/* Query & set thread's kernel vs. user mode state */
#define KERNEL_MODE     SYSTEM_MODE
#define SR_MODE_MASK    0x1f
#define GetThreadMode(pth) ((pth)->ctx.Psr & SR_MODE_MASK)
#define SetThreadMode(pth, mode) \
        ((pth)->ctx.Psr = ((pth)->ctx.Psr&~SR_MODE_MASK) | (mode))

/* Query & set kernel vs. user mode state via Context */
#define GetContextMode(pctx) ((pctx)->Psr & SR_MODE_MASK)
#define SetContextMode(pctx, mode)  \
        ((pctx)->Psr = ((pctx)->Psr&~SR_MODE_MASK) | (mode))

#define SWITCHKEY(oldval, newval) ((oldval) = (pCurThread)->aky, (pCurThread)->aky = (newval))
#define GETCURKEY() ((pCurThread)->aky)
#define SETCURKEY(newval) ((pCurThread)->aky = (newval))


// begin_nthal
//
// Define address space layout as defined by ARM memory management.
//

#define KUSEG_BASE 0x0                  // base of user segment
#define KSEG0_BASE 0x80000000           // base of cached kernel physical
#define KSEG1_BASE 0xa0000000           // base of uncached kernel physical
#define KSEG2_BASE 0xc0000000           // base of cached kernel virtual
// end_nthal


//
// Define ARM exception handling structures and function prototypes.
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

ULONG
RtlVirtualUnwind (
    IN ULONG ControlPc,
    IN PRUNTIME_FUNCTION FunctionEntry,
    IN OUT PCONTEXT ContextRecord,
    OUT PBOOLEAN InFunction,
    OUT PULONG EstablisherFrame,
    IN PPROCESS pprc
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

#define retValue ctx.R0
#define ARG0    ctx.R0
#define SetThreadIP(pth, addr) ((pth)->ctx.Pc = (ULONG)(addr))
#define GetThreadIP(pth) ((pth)->ctx.Pc)

/* Macros for handling stack shrinkage. */
#define MDTestStack(pth)    (((pth)->ctx.Sp < 0x80000000          \
        && (KSTKBOUND(pth)>>VA_PAGE) < (((pth)->ctx.Sp-32)>>VA_PAGE))  \
        ? KSTKBOUND(pth) : 0)

#define MDShrinkStack(pth)  (KSTKBOUND(pth) += PAGE_SIZE)


// Trap id values shared between C & ASM code.
#define ID_RESCHEDULE       0   // NOP used to force a reschedule
#define ID_UNDEF_INSTR      1   // undefined instruction
#define ID_SWI_INSTR        2   // SWI instruction
#define ID_PREFETCH_ABORT   3   // code page fault
#define ID_DATA_ABORT       4   // data page fault or bus error
#define ID_IRQ              5   // external h/w interrupt

// The following codes are used internally by the kernel.
#define ID_STACK_FAULT  8
#define ID_HW_BREAK     9

#include "mem_arm.h"










struct KDataStruct {
    LPDWORD lpvTls;         /* 0x000 Current thread local storage pointer */
    HANDLE  ahSys[NUM_SYS_HANDLES]; /* 0x004 If this moves, change kapi.h */
    char    bResched;       /* 0x084 reschedule flag */
    char    cNest;          /* 0x085 kernel exception nesting */
    char    bPowerOff;      /* 0x086 TRUE during "power off" processing */
    char    bProfileOn;     /* 0x087 TRUE if profiling enabled */
    ulong   unused;         /* 0x088 unused */
    ulong   rsvd2;          /* 0x08c was DiffMSec */
    PPROCESS pCurPrc;       /* 0x090 ptr to current PROCESS struct */
    PTHREAD pCurThd;        /* 0x094 ptr to current THREAD struct */
    DWORD   dwKCRes;        /* 0x098  */
    ulong   handleBase;     /* 0x09c handle table base address */
    PSECTION aSections[64]; /* 0x0a0 section table for virutal memory */
    LPEVENT alpeIntrEvents[SYSINTR_MAX_DEVICES];/* 0x1a0 */
    ulong   pAPIReturn;     /* 0x2a0 direct API return address for kernel mode */
    uchar   *pMap;          /* 0x2a4 ptr to MemoryMap array */
    DWORD   dwInDebugger;   /* 0x2a8 !0 when in debugger */
    PTHREAD pCurFPUOwner;   /* 0x2ac current FPU owner */
    PPROCESS pCpuASIDPrc;   /* 0x2b0 current ASID proc */
    long    nMemForPT;      /* 0x2b4 - Memory used for PageTables */

    DWORD   aPend1;         /* 0x2b8 - low (int 0-31) dword of interrupts pending (must be 8-byte aligned) */
    DWORD   aPend2;         /* 0x2bc - high (int 32-63) dword of interrupts pending */

    long    alPad[16];      /* 0x2c0 - padding */
    DWORD   aInfo[32];      /* 0x300 - misc. kernel info */
                            /* 0x380 - interlocked api code */
                            /* 0x400 - end */
};  /* KDataStruct */

/* High memory layout
 *
 * This structure is mapped in at the end of the 4GB virtual
 * address space.
 *
 *  0xFFFD0000 - first level page table (uncached) (2nd half is r/o)
 *  0xFFFD4000 - disabled for protection
 *  0xFFFE0000 - second level page tables (uncached)
 *  0xFFFE4000 - disabled for protection
 *  0xFFFF0000 - exception vectors
 *  0xFFFF0400 - not used (r/o)
 *  0xFFFF1000 - disabled for protection
 *  0xFFFF2000 - r/o (physical overlaps with vectors)
 *  0xFFFF2400 - Interrupt stack (1k)
 *  0xFFFF2800 - r/o (physical overlaps with Abort stack & FIQ stack)
 *  0xFFFF3000 - disabled for protection
 *  0xFFFF4000 - r/o (physical memory overlaps with vectors & intr. stack & FIQ stack)
 *  0xFFFF4900 - Abort stack (2k - 256 bytes)
 *  0xFFFF5000 - disabled for protection
 *  0xFFFF6000 - r/o (physical memory overlaps with vectors & intr. stack)
 *  0xFFFF6800 - FIQ stack (256 bytes)
 *  0xFFFF6900 - r/o (physical memory overlaps with Abort stack)
 *  0xFFFF7000 - disabled
 *  0xFFFFC000 - kernel stack
 *  0xFFFFC800 - KDataStruct
 *  0xFFFFCC00 - disabled for protection (2nd level page table for 0xFFF00000)
 */

typedef struct _PAGETBL {
    ulong   PTEs[256];
} PAGETBL;

typedef struct ARM_HIGH {
    ulong   firstPT[4096];      // 0xFFFD0000: 1st level page table
    char    reserved2[0x20000-0x4000];

    char    exVectors[0x400];   // 0xFFFF0000: exception vectors
    char    reserved3[0x2400-0x400];

    char    intrStack[0x400];   // 0xFFFF2400: interrupt stack
    char    reserved4[0x4900-0x2800];

    char    abortStack[0x700];  // 0xFFFF4900: abort stack
    char    reserved5[0x6800-0x5000];

    char    fiqStack[0x100];    // 0xFFFF6800: FIQ stack
    char    reserved6[0xC000-0x6900];

    char    kStack[0x800];      // 0xFFFFC000: kernel stack
    struct KDataStruct kdata;   // 0xFFFFC800: kernel data page
} ARM_HIGH;

// KData is already a define on ARM.  Ignore anyone's attempt to
// predefine it.
#ifdef KData
#undef KData
#endif

#define ArmHigh ((ARM_HIGH *)0xFFFD0000)
#define FirstPT (ArmHigh->firstPT)
#define KData   (ArmHigh->kdata)
#define VKData  (*(volatile struct KDataStruct *)&KData)
ERRFALSE((ulong)PUserKData==(ulong)&KData);

#define hCurThread  (KData.ahSys[SH_CURTHREAD])
#define hCurProc    (KData.ahSys[SH_CURPROC])
#define pCurThread  (KData.pCurThd)
#define pCurProc    (KData.pCurPrc)
#define ReschedFlag (KData.bResched)
#define PowerOffFlag (KData.bPowerOff)
#define ProfileFlag (KData.bProfileOn)
#define CurAKey     (pCurThread->aky)
#define SectionTable (KData.aSections)
#define MustReschedule()    (*(ushort*)&KData.bResched == 1)
#define IntrEvents  (KData.alpeIntrEvents)
#define IntrData    (KData.alpvIntrData)
#define KPlpvTls    (KData.lpvTls)
#define KInfoTable  (KData.aInfo)
#define DIRECT_RETURN (KData.pAPIReturn)
#define MemoryMap   (KData.pMap)
#define InDebugger  (KData.dwInDebugger)
#define KCResched   (KData.dwKCRes)
#define g_CurFPUOwner (KData.pCurFPUOwner)
extern DWORD CurMSec;

extern int InSysCall(void);
extern void INTERRUPTS_ON(void);
extern void INTERRUPTS_OFF(void);

#ifdef InterlockedCompareExchange
#undef InterlockedCompareExchange
#endif

#define InterlockedExchange \
        ((LONG (*)(LPLONG Target, LONG Value))(PUserKData+0x3D4))
#define InterlockedExchangeAdd \
        ((long (*)(long *target, long increment))(PUserKData+0x3C0))
#define InterlockedCompareExchange \
        ((LONG (*)(LPLONG Target, LONG Exchange, LONG Comperand))\
        (PUserKData+0x3AC))
#define InterlockedPushList \
        ((void *(*)(void *pHead, void *pItem))(PUserKData+0x398))
#define InterlockedPopList \
        ((void *(*)(void *pHead))(PUserKData+0x380))

#define InterlockedDecrement(target) (InterlockedExchangeAdd(target, -1L)-1)
#define InterlockedIncrement(target) (InterlockedExchangeAdd(target, 1L)+1)
#define InterlockedTestExchange(Target, oldValue, newValue) \
    InterlockedCompareExchange((Target), (newValue), (oldValue))


#define FIRST_INTERLOCK 0x380

//
// this is used to determine if there is an FPU
//
#define VFP_NOT_EXIST   0
#define VFP_TESTING     0x80000000
#define VFP_EXIST       1

extern DWORD vfpStat;

// Defines for CPU specific IDs.
#ifdef THUMBSUPPORT
#define THISCPUID IMAGE_FILE_MACHINE_THUMB
#else
#define THISCPUID IMAGE_FILE_MACHINE_ARM
#endif
#define PROCESSOR_ARCHITECTURE PROCESSOR_ARCHITECTURE_ARM
extern DWORD CEProcessorType;
extern WORD ProcessorLevel;
extern WORD ProcessorRevision;
extern DWORD CEInstructionSet;

#endif // defined(ARM)
// end_ntddk end_nthal

#endif // _NKARM_
