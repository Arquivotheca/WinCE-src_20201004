//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++ BUILD Version: 0015    Increment this if a change has global effects


Module Name:

    ntmips.h

Abstract:

    User-mode visible Mips specific structures and constants


--*/

#ifndef _NTMIPS_
#define _NTMIPS_
#include "mipsinst.h"
#include "m16inst.h"

// begin_ntddk begin_nthal

#if defined(_MIPS_)

//
// Define system time structure.
//

typedef struct _KSYSTEM_TIME {
    ULONG LowPart;
    LONG High1Time;
    LONG High2Time;
} KSYSTEM_TIME, *PKSYSTEM_TIME;

//
// Define unsupported "keywords".
//

#define _cdecl

// begin_windbgkd

#if defined(_MIPS_)

// end_ntddk end_nthal

//
// Define MIPS specific kernel debugger information.
//
// The following structure contains machine specific data passed to
// the host system kernel debugger in a wait state change message.
//

#define DBGKD_MAXSTREAM 16

typedef struct _DBGKD_CONTROL_REPORT {
    ULONG InstructionCount;
    UCHAR InstructionStream[DBGKD_MAXSTREAM];
} DBGKD_CONTROL_REPORT, *PDBGKD_CONTROL_REPORT;

//
// The following structure contains information that the host system
// kernel debugger wants to set on every continue operation and avoids
// the need to send extra packets of information.
//

typedef ULONG DBGKD_CONTROL_SET, *PDBGKD_CONTROL_SET;

#endif                          // ntddk nthal

// end_windbgkd


//
// Define breakpoint codes.
//

#define USER_BREAKPOINT 0                   // user breakpoint
#define KERNEL_BREAKPOINT 1                 // kernel breakpoint
#define BREAKIN_BREAKPOINT 2                // break into kernel debugger
#define BRANCH_TAKEN_BREAKPOINT 3           // branch taken breakpoint
#define BRANCH_NOT_TAKEN_BREAKPOINT 4       // branch not taken breakpoint
#define SINGLE_STEP_BREAKPOINT 5            // single step breakpoint
#define DIVIDE_OVERFLOW_BREAKPOINT 6        // divide overflow breakpoint
#define DIVIDE_BY_ZERO_BREAKPOINT 7         // divide by zero breakpoint
#define RANGE_CHECK_BREAKPOINT 8            // range check breakpoint
#define STACK_OVERFLOW_BREAKPOINT 9         // MIPS code
#define MULTIPLY_OVERFLOW_BREAKPOINT 10     // multiply overflow breakpoint

#define DEBUG_PRINT_BREAKPOINT 20           // debug print breakpoint
#define DEBUG_PROMPT_BREAKPOINT 21          // debug prompt breakpoint
#define DEBUG_STOP_BREAKPOINT 22            // debug stop breakpoint
#define DEBUG_LOAD_SYMBOLS_BREAKPOINT 23    // load symbols breakpoint
#define DEBUG_UNLOAD_SYMBOLS_BREAKPOINT 24  // unload symbols breakpoint

//
// Define length of exception code dispatch vector.
//

#define XCODE_VECTOR_LENGTH 32

// begin_winnt

#if defined(_MIPS_)


//
// Integer Context Frame
//
//  This frame is used to store a limited processor context into the
// Thread structure for CPUs which have no floating point support.
//

typedef struct _ICONTEXT {
    //
    // This section is specified/returned if the ContextFlags word contains
    // the flag CONTEXT_INTEGER.
    //
    // N.B. The registers gp, sp, and ra are defined in this section, but are
    //  considered part of the control context rather than part of the integer
    //  context.
    //
    // N.B. Register zero is not stored in the frame.
    //

    ULONG BadVAddr;

    ///REG_TYPE IntZero;
    REG_TYPE IntAt;
    REG_TYPE IntV0;
    REG_TYPE IntV1;
    REG_TYPE IntA0;
    REG_TYPE IntA1;
    REG_TYPE IntA2;
    REG_TYPE IntA3;
    REG_TYPE IntT0;
    REG_TYPE IntT1;
    REG_TYPE IntT2;
    REG_TYPE IntT3;
    REG_TYPE IntT4;
    REG_TYPE IntT5;
    REG_TYPE IntT6;
    REG_TYPE IntT7;
    REG_TYPE IntS0;
    REG_TYPE IntS1;
    REG_TYPE IntS2;
    REG_TYPE IntS3;
    REG_TYPE IntS4;
    REG_TYPE IntS5;
    REG_TYPE IntS6;
    REG_TYPE IntS7;
    REG_TYPE IntT8;
    REG_TYPE IntT9;
    REG_TYPE IntK0;
    REG_TYPE IntK1;
    REG_TYPE IntGp;
    REG_TYPE IntSp;
    REG_TYPE IntS8;
    REG_TYPE IntRa;
    REG_TYPE IntLo;
    REG_TYPE IntHi;

    //
    // This section is specified/returned if the ContextFlags word contains
    // the flag CONTEXT_FLOATING_POINT.
    //

    ULONG Fsr;

    //
    // This section is specified/returned if the ContextFlags word contains
    // the flag CONTEXT_CONTROL.
    //
    // N.B. The registers gp, sp, and ra are defined in the integer section,
    //   but are considered part of the control context rather than part of
    //   the integer context.
    //

    ULONG Fir;
    ULONG Psr;

    //
    // The flags values within this flag control the contents of
    // a CONTEXT record.
    //
    // If the context record is used as an input parameter, then
    // for each portion of the context record controlled by a flag
    // whose value is set, it is assumed that that portion of the
    // context record contains valid context. If the context record
    // is being used to modify a thread's context, then only that
    // portion of the threads context will be modified.
    //
    // If the context record is used as an IN OUT parameter to capture
    // the context of a thread, then only those portions of the thread's
    // context corresponding to set flags will be returned.
    //
    // The context record is never used as an OUT only parameter.
    //

    ULONG ContextFlags;

#ifdef MIPS_HAS_FPU
    FREG_TYPE FltF0;
    FREG_TYPE FltF1;
    FREG_TYPE FltF2;
    FREG_TYPE FltF3;
    FREG_TYPE FltF4;
    FREG_TYPE FltF5;
    FREG_TYPE FltF6;
    FREG_TYPE FltF7;
    FREG_TYPE FltF8;
    FREG_TYPE FltF9;
    FREG_TYPE FltF10;
    FREG_TYPE FltF11;
    FREG_TYPE FltF12;
    FREG_TYPE FltF13;
    FREG_TYPE FltF14;
    FREG_TYPE FltF15;
    FREG_TYPE FltF16;
    FREG_TYPE FltF17;
    FREG_TYPE FltF18;
    FREG_TYPE FltF19;
    FREG_TYPE FltF20;
    FREG_TYPE FltF21;
    FREG_TYPE FltF22;
    FREG_TYPE FltF23;
    FREG_TYPE FltF24;
    FREG_TYPE FltF25;
    FREG_TYPE FltF26;
    FREG_TYPE FltF27;
    FREG_TYPE FltF28;
    FREG_TYPE FltF29;
    FREG_TYPE FltF30;
    FREG_TYPE FltF31;
 #endif

} ICONTEXT, *PICONTEXT;

#define CALLEE_SAVED_REGS       (10 * sizeof(REG_TYPE))        // (s0 - s8, gp)

#define CPUCONTEXT ICONTEXT /* only need integer context */
#define retValue ctx.IntV0
#define ARG0    ctx.IntA0
#define SetThreadIP(pth, addr) ((pth)->ctx.Fir = (ULONG)(addr))
#define GetThreadIP(pth) ((pth)->ctx.Fir)

/* Macros for handling stack shrinkage. */
#define MDTestStack(pth)    (((pth)->ctx.IntSp < 0x80000000          \
        && (KSTKBOUND(pth)>>VA_PAGE) < (((pth)->ctx.IntSp-32)>>VA_PAGE))  \
        ? KSTKBOUND(pth) : 0)

#define MDShrinkStack(pth)  (KSTKBOUND(pth) += PAGE_SIZE)

#include "mem_mips.h"

#endif // MIPS

// end_winnt


#define CONTEXT_TO_PROGRAM_COUNTER(Context) ((Context)->Fir)

#define CONTEXT_LENGTH (sizeof(CONTEXT))
#define CONTEXT_ALIGN (sizeof(ULONG))
#define CONTEXT_ROUND (CONTEXT_ALIGN - 1)

// begin_nthal
//
// Define R4000 system coprocessor registers.
//
// Define index register fields.
//

typedef struct _INDEX {
    ULONG INDEX : 6;
    ULONG X1 : 25;
    ULONG P : 1;
} INDEX;

//
// Define random register fields.
//

typedef struct _RANDOM {
    ULONG INDEX : 6;
    ULONG X1 : 26;
} RANDOM;

//
// Define TB entry low register fields.
//

typedef struct _ENTRYLO {
    ULONG G : 1;
    ULONG V : 1;
    ULONG D : 1;
    ULONG C : 3;
    ULONG PFN : 24;
    ULONG X1 : 2;
} ENTRYLO, *PENTRYLO;

//
// Define R4000 PTE format for memory management.
//
// N.B. This must map exactly over the entrylo register.
//

typedef struct _HARDWARE_PTE {
    ULONG Global : 1;
    ULONG Valid : 1;
    ULONG Dirty : 1;
    ULONG CachePolicy : 3;
    ULONG PageFrameNumber : 24;
    ULONG Write : 1;
    ULONG CopyOnWrite : 1;
} HARDWARE_PTE, *PHARDWARE_PTE;

//
// Define R4000 macro to initialize page directory table base.
//

#define INITIALIZE_DIRECTORY_TABLE_BASE(dirbase, pfn) \
     ((HARDWARE_PTE *)(dirbase))->PageFrameNumber = pfn; \
     ((HARDWARE_PTE *)(dirbase))->Global = 0; \
     ((HARDWARE_PTE *)(dirbase))->Valid = 1; \
     ((HARDWARE_PTE *)(dirbase))->Dirty = 1; \
     ((HARDWARE_PTE *)(dirbase))->CachePolicy = PCR->CachePolicy

//
// Define page mask register fields.
//

typedef struct _PAGEMASK {
    ULONG X1 : 13;
    ULONG PAGEMASK : 12;
    ULONG X2 : 7;
} PAGEMASK, *PPAGEMASK;

//
// Define wired register fields.
//

typedef struct _WIRED {
    ULONG NUMBER : 6;
    ULONG X1 : 26;
} WIRED;

//
// Define TB entry high register fields.
//

typedef struct _ENTRYHI {
    ULONG PID : 8;
    ULONG X1 : 5;
    ULONG VPN2 : 19;
} ENTRYHI, *PENTRYHI;

//
// Define processor status register fields.
//

typedef struct _PSR {
    ULONG IE : 1;
    ULONG EXL : 1;
    ULONG ERL : 1;
    ULONG KSU : 2;
    ULONG UX : 1;
    ULONG SX : 1;
    ULONG KX : 1;
    ULONG INTMASK : 8;
    ULONG DE : 1;
    ULONG CE : 1;
    ULONG CH : 1;
    ULONG X1 : 1;
    ULONG SR : 1;
    ULONG TS : 1;
    ULONG BEV : 1;
    ULONG X2 : 2;
    ULONG RE : 1;
    ULONG FR : 1;
    ULONG RP : 1;
    ULONG CU0 : 1;
    ULONG CU1 : 1;
    ULONG CU2 : 1;
    ULONG CU3 : 1;
} PSR, *PPSR;

//
// Define configuration register fields.
//

typedef struct _CONFIG {
    ULONG K0 : 3;
    ULONG CU : 1;
    ULONG DB : 1;
    ULONG IB : 1;
    ULONG DC : 3;
    ULONG IC : 3;
    ULONG X1 : 1;
    ULONG EB : 1;
    ULONG EM : 1;
    ULONG BE : 1;
    ULONG SM : 1;
    ULONG SC : 1;
    ULONG EW : 2;
    ULONG SW : 1;
    ULONG SS : 1;
    ULONG SB : 2;
    ULONG EP : 4;
    ULONG EC : 3;
    ULONG CM : 1;
} CONFIG;

//
// Define ECC register fields.
//

typedef struct _ECC {
    ULONG ECC : 8;
    ULONG X1 : 24;
} ECC;

//
// Define cache error register fields.
//

typedef struct _CACHEERR {
    ULONG PIDX : 3;
    ULONG SIDX : 19;
    ULONG X1 : 2;
    ULONG EI : 1;
    ULONG EB : 1;
    ULONG EE : 1;
    ULONG ES : 1;
    ULONG ET : 1;
    ULONG ED : 1;
    ULONG EC : 1;
    ULONG ER : 1;
} CACHEERR;

//
// Define R4000 cause register fields.
//

typedef struct _CAUSE {
    ULONG X1 : 2;
    ULONG XCODE : 5;
    ULONG X2 : 1;
    ULONG INTPEND : 8;
    ULONG X3 : 12;
    ULONG CE : 2;
    ULONG X4 : 1;
    ULONG BD : 1;
} CAUSE;

//
// Define R4000 processor id register fields.
//

typedef struct _PRID {
    ULONG REV : 8;
    ULONG IMPL : 8;
    ULONG X1 : 16;
} PRID;

// end_nthal

// begin_nthal
//
// Define R4000 floating status register field definitions.
//

typedef struct _FSR {
    ULONG RM : 2;
    ULONG SI : 1;
    ULONG SU : 1;
    ULONG SO : 1;
    ULONG SZ : 1;
    ULONG SV : 1;
    ULONG EI : 1;
    ULONG EU : 1;
    ULONG EO : 1;
    ULONG EZ : 1;
    ULONG EV : 1;
    ULONG XI : 1;
    ULONG XU : 1;
    ULONG XO : 1;
    ULONG XZ : 1;
    ULONG XV : 1;
    ULONG XE : 1;
    ULONG X1 : 5;
    ULONG CC : 1;
    ULONG FS : 1;
    ULONG X2 : 7;
} FSR, *PFSR;

// end_nthal

// begin_nthal
//
// Define address space layout as defined by MIPS memory management.
//

#define KUSEG_BASE 0x0                  // base of user segment
#define KSEG0_BASE 0x80000000           // base of cached kernel physical
#define KSEG1_BASE 0xa0000000           // base of uncached kernel physical
#define KSEG2_BASE 0xc0000000           // base of cached kernel virtual
// end_nthal


//
// Define MIPS exception handling structures and function prototypes.
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

PRUNTIME_FUNCTION
RtlLookupFunctionEntry (
    IN ULONG ControlPc
    );

ULONG
RtlVirtualUnwind (
    IN ULONG ControlPc,
    IN PRUNTIME_FUNCTION FunctionEntry,
    IN OUT PCONTEXT ContextRecord,
    OUT PBOOLEAN InFunction,
    OUT PULONG EstablisherFrame
    );

//
// Define C structured exception handing function prototypes.
//

struct _EXCEPTION_POINTERS;

typedef
LONG
(*EXCEPTION_FILTER) (
    struct _EXCEPTION_POINTERS *ExceptionPointers
    );

typedef
VOID
(*TERMINATION_HANDLER) (
    BOOLEAN is_abnormal
    );

// begin_winnt

#if defined(_MIPS_)

VOID
__jump_unwind (
    PVOID Fp,
    PVOID TargetPc
    );

#endif // MIPS

// end_winnt










struct KDataStruct {
    LPDWORD lpvTls;         /* 0x000 Current thread local storage pointer */
    HANDLE ahSys[NUM_SYS_HANDLES]; /* 0x004 If this moves, change kapi.h */
    char    bResched;       /* 0x084 reschedule flag */
    char    cNest;          /* 0x085 kernel exception nesting */
    char    pad;            /* 0x086 alignment padding */
    char    bPowerOff;      /* 0x087 - power off flag */
    ulong   asm_only1;      /* 0x088 kernel temp saved T0 (8 bytes) : only from asm */
    ulong   asm_only2;      /* 0x08C kernel temp saved T0 (8 bytes) : only from asm */
    ulong   basePSR;        /* 0x090 */
    ulong   cMsec;          /* 0x094 # of milliseconds since boot */
    ulong   cDMsec;         /* 0x098 # of mSec since last TimerCallBack */
    ACCESSKEY akyCur;       /* 0x09c current access key */
    PFNKDBG pfnKDbg;        /* 0x0a0 kernel debugger entry point */
    DWORD   isrFalse;       /* 0x0a4 false interrupt service routine */
    DWORD   adwIsrTable[6]; /* 0x0a8 first level intr service routines */   

    PSECTION aSections[64]; /* 0x0c0 section table for virutal memory */
    LPEVENT alpeIntrEvents[SYSINTR_MAX_DEVICES];/* 0x1c0 */
    LPVOID  alpvIntrData[SYSINTR_MAX_DEVICES];  /* 0x240 */
    PTHREAD pCurThd;        /* 0x2c0: ptr to current THREAD struct */
    PPROCESS pCurPrc;       /* 0x2c4 ptr to current PROCESS struct */
    ulong   handleBase;     /* 0x2c8: handle table base address */
    ulong   pAPIReturn;     /* 0x2cc direct API return address for kernel mode */
    DWORD   dwKCRes;        /* 0x2d0 */
    DWORD   dwInDebugger;   /* 0x2d4 !0 if in debugger */
    DWORD   dwPfnShift;     /* 0x2d8: PFN_SHIFT */
    BOOL    fMIPS16Sup;     /* 0x2dc: if MIPS16 instruction is supported */
    DWORD   dwPfnIncr;      // 0x2e0: PFN_INCR */
    BYTE    bPadding[20];   /* 0x2e4 */
    PTHREAD g_CurFPUOwner;  /* 0x2f8 Current FPU owner thread */
    long    nMemForPT;      /* 0x2fc - Memory used for PageTables */
    long    aInfo[32];      /* 0x300 - misc. kernel info */
                            /* 0x380 - end */
};  /* KData */

#define KData  (*(struct KDataStruct *)(KPAGE_BASE+0x1800))
#define VKData  (*(volatile struct KDataStruct *)(KPAGE_BASE+0x1800))

#define hCurThread ((HANDLE)KData.ahSys[SH_CURTHREAD])
#define hCurProc ((HANDLE)KData.ahSys[SH_CURPROC])
#define pCurThread (KData.pCurThd)
#define pCurProc    (KData.pCurPrc)
#define ReschedFlag (KData.bResched)
#define PowerOffFlag (KData.bPowerOff)
#define CurAKey     (KData.akyCur)
#define DbgEntry    (KData.pfnKDbg)
#define SectionTable (KData.aSections)
#define InSysCall() (KData.cNest != 1)
#define ISRTable    (KData.adwIsrTable)
#define IntrEvents  (KData.alpeIntrEvents)
#define IntrData    (KData.alpvIntrData)
#define KPlpvTls    (KData.lpvTls)
#define KInfoTable  (KData.aInfo)
#define bIntrIndexLow (KData.bIntrIndexLow)
#define bIntrIndexHigh (KData.bIntrIndexHigh)
#define bIntrNumber (KData.bIntrNumber)
#define DIRECT_RETURN (KData.pAPIReturn)
#define g_CurFPUOwner (KData.g_CurFPUOwner)
#define KCResched   (KData.dwKCRes)
#define InDebugger  (KData.dwInDebugger)
#define IsMIPS16Supported   (KData.fMIPS16Sup)

ERRFALSE(AddrCurMSec == offsetof(struct KDataStruct, cMsec)+KPAGE_BASE+0x1800);
#undef CurMSec
#define CurMSec (KData.cMsec)

#define VR5432_BP_BUG 1

#if defined(VR5432_BP_BUG)
// use the hand-written assembly
extern void INTERRUPTS_ON(void);
extern void INTERRUPTS_OFF(void);
#else
#define INTERRUPTS_ON() _enable()
#define INTERRUPTS_OFF() _disable()
#endif

extern void *InterlockedPopList(void *pHead);
extern void *InterlockedPushList(void *pHead, void *pItem);


// Defines for CPU specific IDs.
#if defined(MIPSIV)
#define THISCPUID IMAGE_FILE_MACHINE_MIPSFPU
#else
#define THISCPUID IMAGE_FILE_MACHINE_R4000
#endif

#define PROCESSOR_ARCHITECTURE PROCESSOR_ARCHITECTURE_MIPS

// Note that R4000 is defined for all MIPS at or above R4000, so they
// need to be defined at the top of the chain, or else they will get
// superceded by the R4000 switch 
#if defined(R5000) || defined(MIPSIV)
  #define CEProcessorType PROCESSOR_MIPS_R5000
  #define ProcessorLevel 5
#elif defined(R4000)
  #define CEProcessorType PROCESSOR_MIPS_R4000
  #define ProcessorLevel 4
#endif
extern WORD ProcessorRevision;
extern DWORD CEInstructionSet;

#endif // defined(_MIPS_)
// end_ntddk end_nthal

#endif // _NTMIPS_
