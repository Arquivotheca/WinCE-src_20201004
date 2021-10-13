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

#ifdef __cplusplus
extern "C" {
#endif

// begin_ntddk begin_nthal

#if defined(_MIPS_)

//
// Define unsupported "keywords".
//

#define _cdecl

//
// Define breakpoint codes.
//

#define USER_BREAKPOINT                 0   // user breakpoint
#define KERNEL_BREAKPOINT               1   // kernel breakpoint
#define BREAKIN_BREAKPOINT              2   // break into kernel debugger
#define BRANCH_TAKEN_BREAKPOINT         3   // branch taken breakpoint
#define BRANCH_NOT_TAKEN_BREAKPOINT     4   // branch not taken breakpoint
#define SINGLE_STEP_BREAKPOINT          5   // single step breakpoint
#define DIVIDE_OVERFLOW_BREAKPOINT      6   // divide overflow breakpoint
#define DIVIDE_BY_ZERO_BREAKPOINT       7   // divide by zero breakpoint
#define RANGE_CHECK_BREAKPOINT          8   // range check breakpoint
#define STACK_OVERFLOW_BREAKPOINT       9   // MIPS code
#define MULTIPLY_OVERFLOW_BREAKPOINT    10  // multiply overflow breakpoint

//
// define exception code
//
#define XID_TLB_MOD                     1   // TLB modification
#define XID_TLB_LOAD                    2   // TLB load
#define XID_TLB_STORE                   3   // TLB store
#define XID_ADDR_LOAD                   4   // address alignment error (load)
#define XID_ADDR_STORE                  5   // address alignment error (store)
#define XID_BUS_ERR_INST                6   // bus error (instruction)
#define XID_BUS_ERR_DATA                7   // bus error (data)
#define XID_SYSCALL                     8   // syscall (unused in CE)
#define XID_BREAK_POINT                 9   // break point
#define XID_DIVIDE_BY_ZERO              9   // XID_BREAK_POINT overloaded; see MDHandleHardwareException() in mdsched.c
#define XID_RESV_INST                   10  // reserved instruction
#define XID_COPROCESSOR_UNUSABLE        11  // co-proc unuseable
#define XID_INT_OVERFLOW                12  // integer overflow
#define XID_TRAP                        13  // trap (unused in CE)
#define XID_VIRTUAL_INSTRUCTION         14  // virtual coherency (instruction)
#define XID_FPU_EXCEPTION               15  // floating point exception
    // 16-22 reserved
#define XID_WATCHPOINT                  23  // watch point
    // 23-29 reserved
#define XID_STACK_OVERFLOW              30  // stack overflow (software)
#define XID_VIRTUAL_DATA                31  // virtual coherency (data)

#define MD_MAX_EXCP_ID                  15  // id above 15 is treated as unknown exception in exception printout

//
// Define length of exception code dispatch vector.
//

#define XCODE_VECTOR_LENGTH 32

//
// BC Issue for supporting 128 API sets
//
// We have to change FIRST_METHOD in API encoding. Normally there isn't an issue when
// the call go through coredll. However, the startup code under crtw32\startup and crtw32\security
// are linked directly to EXE/DLL, where it calls TerminateProcess without going through the coredll
// thunk. We need to workaround them by re-mapping it to new encoding when we faulted with the old encoding
// of TerminateProcess.
//
#define OLD_FIRST_METHOD            0xFFFFFC02
#define OLD_TERMINATE_PROCESS       (OLD_FIRST_METHOD - ((SH_CURPROC)<<APISET_SHIFT | (ID_PROC_TERMINATE))*APICALL_SCALE)

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


#define CPUCONTEXT                  ICONTEXT        
#define retValue                    ctx.IntV0

#define COMBINED_PDATA              // define to enable compbined pdata format
#define INST_SIZE                   4


#define SetThreadIP(pth, addr)  ((pth)->ctx.Fir = (ULONG)(addr))
#define GetThreadIP(pth)        ((pth)->ctx.Fir)

#endif // MIPS

// end_winnt


#define CONTEXT_TO_PROGRAM_COUNTER(Context)     ((Context)->Fir)

#define CONTEXT_TO_STACK_POINTER(Context)       ((Context)->IntSp)
#define CONTEXT_TO_RETURN_ADDRESS(Context)      ((LONG) ((Context)->IntRa))

#define CONTEXT_TO_PARAM_1(Context)             ((Context)->IntA0)
#define CONTEXT_TO_PARAM_2(Context)             ((Context)->IntA1)
#define CONTEXT_TO_PARAM_3(Context)             ((Context)->IntA2)
#define CONTEXT_TO_PARAM_4(Context)             ((Context)->IntA3)

#define CONTEXT_TO_RETVAL(Context)              ((Context)->IntV0)

#define CONTEXT_LENGTH                          (sizeof(CONTEXT))
#define CONTEXT_ALIGN                           (sizeof(REGTYPE))
#define CONTEXT_ROUND                           (CONTEXT_ALIGN - 1)

#define STACK_ALIGN                             8           // 4-byte alignment for stack

#define CALLEE_SAVED_REGS                       10          // (s0 - s8, gp)
#define REG_S0                                  0
#define REG_S1                                  1
#define REG_S2                                  2
#define REG_S3                                  3
#define REG_S4                                  4
#define REG_S5                                  5
#define REG_S6                                  6
#define REG_S7                                  7
#define REG_S8                                  8
#define REG_GP                                  9

//
// thread context translation
//
#define THRD_CTX_TO_PC(pth)         ((pth)->ctx.Fir)
#define THRD_CTX_TO_SP(pth)         ((pth)->ctx.IntSp)
#define THRD_CTX_TO_PARAM_1(pth)    ((pth)->ctx.IntA0)
#define THRD_CTX_TO_PARAM_2(pth)    ((pth)->ctx.IntA1)

/* Query & set thread's kernel vs. user mode state */
#define GetThreadMode(pth)          ((pth)->ctx.Psr & MODE_MASK)
#define SetThreadMode(pth, mode)    ((pth)->ctx.Psr = (mode))

/* Query & set kernel vs. user mode state via Context */
#define GetContextMode(pctx)        ((pctx)->Psr & MODE_MASK)
#define SetContextMode(pctx, mode)  ((pctx)->Psr = (mode))


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
    union {
        struct {
            ULONG X1 : 2;
            ULONG XCODE : 5;
            ULONG X2 : 1;
            ULONG INTPEND : 8;
            ULONG X3 : 12;
            ULONG CE : 2;
            ULONG X4 : 1;
            ULONG BD : 1;
        };
        ULONG AsUlong;
    };
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

/* Kernel data area
 *
 * This area is always mapped into two well known locations of
 * the virtual memory space. One in user space which is read-only
 * and one in the kernel space which is read-write.
 *
 * NOTE: currently must be in sync with kapi.h for sharing with apis.c
 */

struct KDataStruct {

#include "kdata_common.h"

    // CPU Dependent part uses offset 0x2a0 - 0x300, and after aInfo
    ULONGLONG   _saveT0_pcb;        /* 0x2a0 (ASM ONLY), to save T0 */
    ULONGLONG   _saveK0_pcb;        /* 0x2a8 (ASM ONLY), to save T0 */
    ULONGLONG   _saveK1_pcb;        /* 0x2b0 (ASM ONLY), to save T0 */
    ulong       basePSR;        /* 0x2b8 (ASM ONLY), Base PSR */
    DWORD       isrFalse;       /* 0x2bc false interrupt service routine */
    DWORD       adwIsrTable[6]; /* 0x2c0 first level intr service routines */
    DWORD       dwPfnShift;     /* 0x2d8: PFN_SHIFT */
    DWORD       dwPfnIncr;      /* 0x2dc: PFN_INCR */
    BOOL        dwArchFlags;    /* 0x2e0: architecture flags */
    DWORD       dwCoProcBits;   /* 0x2e4: co-proc enable bits */
    DWORD       dwPadding[6];   /* 0x2e8: padding */

    // aInfo MUST BE AT OFFSET 0x300
    long        aInfo[32];      /* 0x300 - misc. kernel info */
                                /* 0x380 - end */
};  /* KData */

typedef struct _PCB {

#include "pcb_common.h"
    // CPU dependent part
    ULONGLONG   _saveT0;        /* 0x2a0 (ASM ONLY), to save T0 */
    ULONGLONG   _saveK0;        /* 0x2a8 (ASM ONLY), to save T0 */
    ULONGLONG   _saveK1;        /* 0x2b0 (ASM ONLY), to save T0 */
    ulong       basePSR_KData;        /* 0x2b8 (ASM ONLY), Base PSR */
    DWORD       isrFalse_KData;       /* 0x2bc false interrupt service routine */
    DWORD       adwIsrTable_KData[6]; /* 0x2c0 first level intr service routines */
    DWORD       dwPfnShift_KData;     /* 0x2d8: PFN_SHIFT */
    DWORD       dwPfnIncr_KData;      /* 0x2dc: PFN_INCR */
    BOOL        dwArchFlags;    /* 0x2e0: architecture flags (replicated in each pcb)*/
    DWORD       dwCoProcBits;   /* 0x2e4: co-proc enable bits (replicated in each pcb) */
    DWORD       dwPadding[6];   /* 0x2e8: padding */

    // aInfo MUST BE AT OFFSET 0x300
    long        aInfo_unused[32];      /* 0x300 - misc. kernel info */
                                /* 0x380 - end */
} PCB, *PPCB;


__inline BOOL IsPSLBoundary (DWORD dwAddr)
{
    // user UserKData (), instead of KData directly, for this can be called in user mode
    struct KDataStruct *pKData = (struct KDataStruct *) PUserKData;
    return (dwAddr == SYSCALL_RETURN)
        || (dwAddr == pKData->pAPIReturn)
        || (dwAddr+INST_SIZE == SYSCALL_RETURN)
        || (dwAddr+INST_SIZE == pKData->pAPIReturn);
}


//
// TLB is hardwired for PCB at PCB_ADDRESS on each core. i.e. we can always use PCB_ADDRESS to reference
// PCB regardless which core we run on.
//
#ifdef IN_KERNEL
#define PCB_ADDRESS                 0xffffd800
#else
#define PCB_ADDRESS                 0x0000d800
#endif



#define InPrivilegeCall()           (((PPCB) PCB_ADDRESS)->cNest != 1)
#define InSysCall()                 (InPrivilegeCall() || PcbOwnSpinLock())
#define ISRTable                    (g_pKData->adwIsrTable)
#define IsMIPS16Supported           (g_pKData->dwArchFlags & MIPS_FLAG_MIPS16)

void MDCheckInterlockOperation (PCONTEXT pCtx);
void InitInterlockedFunctions (void);


// NOTE: THE FOLLOWING 2 FUNCTIONS SHOULD ONLY BE CALLED WHEN PREEMPTION IS NOT ALLOWED. OR A 
//       THREAD CAN MIGRATE FROM ONE CPU TO ANOTHER LEAVING A STALE POINTER TO PCB
FORCEINLINE PPCB GetPCB (void)
{
    return (PPCB) PCB_ADDRESS;
}
FORCEINLINE DWORD PcbGetCurCpu (void)
{
    return ((PPCB) PCB_ADDRESS)->dwCpuId;
}

//
// The functions below can be called in any context.
//
FORCEINLINE DWORD PCBGetDwordAtOffset (DWORD dwOfst)
{
    return *(LPDWORD) ((DWORD) PCB_ADDRESS+dwOfst);
}

//-------------------------------------------------------------------------
FORCEINLINE DWORD PcbGetCurThreadId (void)
{
    return ((PPCB) PCB_ADDRESS)->CurThId;
}

FORCEINLINE DWORD PcbGetActvProcId (void)
{
    return ((PPCB) PCB_ADDRESS)->ActvProcId;
}

FORCEINLINE PTHREAD PcbGetCurThread (void)
{
    return ((PPCB) PCB_ADDRESS)->pCurThd;
}

FORCEINLINE PPROCESS PcbGetActvProc (void)
{
    return ((PPCB) PCB_ADDRESS)->pCurPrc;
}

FORCEINLINE PPROCESS PcbGetVMProc (void)
{
    return ((PPCB) PCB_ADDRESS)->pVMPrc;
}

FORCEINLINE BOOL PcbOwnSpinLock (void)
{
    return ((PPCB) PCB_ADDRESS)->ownspinlock;
}

//-------------------------------------------------------------------------
FORCEINLINE void PcbSetActvProcId (DWORD dwProcId)
{
    ((PPCB) PCB_ADDRESS)->ActvProcId = dwProcId;
}

FORCEINLINE void PcbSetActvProc (PPROCESS pprc)
{
    ((PPCB) PCB_ADDRESS)->pCurPrc = pprc;
}

FORCEINLINE void PcbSetVMProc (PPROCESS pprc)
{
    ((PPCB) PCB_ADDRESS)->pVMPrc = pprc;
}

FORCEINLINE void PcbSetTlsPtr (LPDWORD pTlsPtr)
{
    ((PPCB) PCB_ADDRESS)->lpvTls = pTlsPtr;
}

//-------------------------------------------------------------------------
FORCEINLINE void PcbIncOwnSpinlockCount (void)
{
    ((PPCB) PCB_ADDRESS)->ownspinlock ++;
}
FORCEINLINE void PcbDecOwnSpinlockCount (void)
{
    ((PPCB) PCB_ADDRESS)->ownspinlock --;
}


#define VR5432_BP_BUG 1

#if defined(VR5432_BP_BUG)
// use the hand-written assembly
extern void INTERRUPTS_ON(void);
extern void INTERRUPTS_OFF(void);
#else
#define INTERRUPTS_ON()         _enable()
#define INTERRUPTS_OFF()        _disable()
#endif

void InterlockedPushList(volatile void *pHead, void *pItem);
void *InterlockedPopList(volatile void *pHead);


// Defines for CPU specific IDs.
#if defined(MIPSIV)
#define THISCPUID IMAGE_FILE_MACHINE_MIPSFPU
#else
#define THISCPUID IMAGE_FILE_MACHINE_R4000
#endif

#define PROCESSOR_ARCHITECTURE PROCESSOR_ARCHITECTURE_MIPS

#endif // defined(_MIPS_)
// end_ntddk end_nthal


#ifdef __cplusplus
}
#endif

#endif // _NTMIPS_
