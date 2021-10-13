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
/*++ BUILD Version: 0016    Increment this if a change has global effects


Module Name:

    nkarm.h

Abstract:

    User-mode visible ARM specific structures and constants

--*/

#ifndef _NKARM_
#define _NKARM_

#ifdef __cplusplus
extern "C" {
#endif


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
    ULONG Fpscr;                // floating point status register
    ULONG FpExc;                // the exception register
    ULONG S[NUM_VFP_REGS];
    ULONG FpExtra[NUM_EXTRA_CONTROL_REGS];
} CPUCONTEXT, *PCPUCONTEXT;


#define CONTEXT_TO_PROGRAM_COUNTER(Context)     ((Context)->Pc)

#define CONTEXT_TO_STACK_POINTER(Context)       ((Context)->Sp)
#define CONTEXT_TO_RETURN_ADDRESS(Context)      ((Context)->Lr)

#define CONTEXT_TO_PARAM_1(Context) ((Context)->R0)
#define CONTEXT_TO_PARAM_2(Context) ((Context)->R1)
#define CONTEXT_TO_PARAM_3(Context) ((Context)->R2)
#define CONTEXT_TO_PARAM_4(Context) ((Context)->R3)
#define CONTEXT_TO_RETVAL(Context)  ((Context)->R0)

#define CONTEXT_LENGTH          (sizeof(CPUCONTEXT))
#define CONTEXT_ALIGN           (sizeof(ULONG))
#define CONTEXT_ROUND           (CONTEXT_ALIGN - 1)

#define CALLEE_SAVED_REGS       8       // (r4 - r11)

#define REG_R4                  0
#define REG_R5                  1
#define REG_R6                  2
#define REG_R7                  3
#define REG_R8                  4
#define REG_R9                  5
#define REG_R10                 6
#define REG_R11                 7

#define STACK_ALIGN             4           // 4-byte alignment for stack

//
// thread context translation
//
#define THRD_CTX_TO_PC(pth)         ((pth)->ctx.Pc)
#define THRD_CTX_TO_SP(pth)         ((pth)->ctx.Sp)
#define THRD_CTX_TO_PARAM_1(pth)    ((pth)->ctx.R0)
#define THRD_CTX_TO_PARAM_2(pth)    ((pth)->ctx.R1)


// ARM processor modes
#define USER_MODE   0x10    // 0b10000
#define FIQ_MODE    0x11    // 0b10001
#define IRQ_MODE    0x12    // 0b10010
#define SVC_MODE    0x13    // 0b10011
#define ABORT_MODE  0x17    // 0b10111
#define UNDEF_MODE  0x1b    // 0b11011
#define SYSTEM_MODE 0x1f    // 0b11111

// Other state bits in the processor status register
#define THUMB_STATE       0x00000020
#define FIQ_DISABLE       0x00000040
#define IRQ_DISABLE       0x00000080
#define BIG_ENDIAN_STATE  0x00000200
#define ITSTATE_MASK_LOW  0x0000FC00
#define JAZELLE_STATE     0x01000000
#define ITSTATE_MASK_HIGH 0x06000000
#define ITSTATE_MASK      (ITSTATE_MASK_HIGH | ITSTATE_MASK_LOW)
#define EXEC_STATE_MASK   (JAZELLE_STATE | ITSTATE_MASK | BIG_ENDIAN_STATE)

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

//
// BC Issue for supporting 128 API sets
//
// We have to change FIRST_METHOD in API encoding. Normally there isn't an issue when
// the call go through coredll. However, the startup code under crtw32\startup and crtw32\security
// are linked directly to EXE/DLL, where it calls TerminateProcess without going through the coredll
// thunk. We need to workaround them by re-mapping it to new encoding when we faulted with the old encoding
// of TerminateProcess.
//
#define OLD_FIRST_METHOD            0xF0010000
#define OLD_TERMINATE_PROCESS       (OLD_FIRST_METHOD - ((SH_CURPROC)<<APISET_SHIFT | (ID_PROC_TERMINATE))*APICALL_SCALE)

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

#define retValue                    ctx.R0
#define SetThreadIP(pth, addr)      ((pth)->ctx.Pc = (ULONG)(addr))
#define GetThreadIP(pth)            ((pth)->ctx.Pc)

#define COMPRESSED_PDATA        // define to enable compressed pdata format
#define IntRa                       Lr
#define IntSp                       Sp
#define INST_SIZE                   4

// Trap id values shared between C & ASM code.
#define ID_RESCHEDULE       0   // NOP used to force a reschedule
#define ID_UNDEF_INSTR      1   // undefined instruction
#define ID_SWI_INSTR        2   // SWI instruction
#define ID_PREFETCH_ABORT   3   // code page fault
#define ID_DATA_ABORT       4   // data page fault or bus error
#define ID_IRQ              5   // external h/w interrupt
#define ID_FPU_EXCEPTION    6   // VFP emulation exception

#define MD_MAX_EXCP_ID      6


//
// TTBR bit format
//
// | 31 --------------------- (14-N) |(13-N) - 5|4 - 3|2|1|0|
// |      Translation Base           |   SBZ    | RGN |P|S|C|
//
//
//   [4:3] RGN - Outer cachable attributes for page table walking:
//           b00 = Outer Noncachable
//           b01 = Outer Cachable Write-Back cached, Write Allocate
//           b10 = Outer Cachable Write-Through, No Allocate on Write
//           b11 = Outer Cachable Write-Back, No Allocate on Write.
//
//   [2] P - If ECC is supported, indicates to the memory controller whether it is 
//       enabled (P=1) or disabled (P=0).
//
//   [1] S - Indicates whether the page table walk is to Shared or Non-Shared memory.
//           0 = Non-Shared. This is the reset value.
//           1 = Shared.
//   [0] - C Indicates whether the page table walk is Inner Cachable or Inner Noncachable.
//           0 = Inner noncachable. This is the reset value.
//           1 = Inner cachable.
//
#define TTBR_CTRL_RGN_MASK          0x18
#define TTBR_CTRL_P_BIT             0x04
#define TTBR_CTRL_S_BIT             0x02
#define TTBR_CTRL_C_BIT             0x01

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
    ulong       dwArchitectureId;           // 0x2a0 architecture id
    LPVOID      pAddrMap;                   // 0x2a4 ptr to OEMAddressTable array
    DWORD       dwRead;                     // 0x2a8 - R/O, both kernel and user
    DWORD       dwWrite;                    // 0x2ac - R/W, both kernel and user
    DWORD       dwKrwUno;                   // 0x2b0 - Kernel R/W, user no access
    DWORD       dwKrwUro;                   // 0x2b4 - Kernel R/W, user no access
    DWORD       dwProtMask;                 // 0x2b8 - mask for all the permission bits
    DWORD       dwOEMInitGlobalsAddr;       // 0x2bc ptr to OAL entry point
    DWORD       dwTTBRCtrlBits;             // 0x2c0 - control bits for TTBR (see above TTBR Bit formats)
    DWORD       dwPTExtraBits;              // 0x2c4 - extra bits for PT entry
    long        alPad[14];                  // 0x2c8 - padding

    // aInfo MUST BE AT OFFSET 0x300
    DWORD       aInfo[32];      /* 0x300 - misc. kernel info */
                                /* 0x380 - interlocked api code */
                                /* 0x400 - end */
};  /* KDataStruct */

typedef struct _PCB {

#include "pcb_common.h"
    // CPU dependent part
    ulong       dwArchitectureId_unused;    // 0x2a0 architecture id
    LPVOID      pAddrMap_unused;            // 0x2a4 ptr to OEMAddressTable array
    DWORD       dwRead_unused;              // 0x2a8 - R/O, both kernel and user
    DWORD       dwWrite_unused;             // 0x2ac - R/W, both kernel and user
    DWORD       dwKrwUno_unused;            // 0x2b0 - Kernel R/W, user no access
    DWORD       dwKrwUro_unused;            // 0x2b4 - Kernel R/W, user no access
    DWORD       dwProtMask_unused;          // 0x2b8 - mask for all the permission bits
    DWORD       dwOEMInitGlobalsAddr_unused;// 0x2bc ptr to OAL entry point
    DWORD       dwTTBRCtrlBits_unused;      // 0x2c0 - control bits for TTBR (see above TTBR Bit formats)
    DWORD       dwPTExtraBits_unused;       // 0x2c4 - extra bits for PT entry
    long        alPad[14];                  // 0x2c8 - padding

    // aInfo MUST BE AT OFFSET 0x300
    DWORD       aInfo_unused[32];      /* 0x300 - misc. kernel info */
                                /* 0x380 - interlocked api code */
                                /* 0x400 - end */
} PCB, *PPCB;

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

#define ArmHigh ((ARM_HIGH *)0xFFFD0000)
ERRFALSE((ulong)PUserKData==(ulong)&ArmHigh->kdata);

__inline BOOL IsPSLBoundary (DWORD dwAddr)
{
    // user UserKData (), instead of KData directly, for this can be called in user mode
    const struct KDataStruct *pKData = (const struct KDataStruct *) PUserKData;
    return (dwAddr == SYSCALL_RETURN)
        || (dwAddr == pKData->pAPIReturn)
        || (dwAddr+INST_SIZE == SYSCALL_RETURN)
        || (dwAddr+INST_SIZE == pKData->pAPIReturn);
}

BOOL InSysCall(void);
BOOL InPrivilegeCall (void);
void INTERRUPTS_ON(void);
void INTERRUPTS_OFF(void);

void MDCheckInterlockOperation (PCONTEXT pCtx);

DWORD OEMInterruptHandler (DWORD dwEPC);
void OEMInterruptHandlerFIQ (void);

BOOL HaveVfpHardware();
BOOL HaveVfpSupport();
BOOL HaveExtendedVfp();

void *InterlockedPopList(volatile void *pHead);
void InterlockedPushList(volatile void *pHead, void *pItem);

#ifdef IN_KERNEL
#undef IsVirtualTaggedCache
#define IsVirtualTaggedCache()  (g_pKData->dwArchitectureId < ARMArchitectureV6)
#endif

#define FIRST_INTERLOCK 0x380
#define ILOCK_REGION_SIZE       (0x400 - FIRST_INTERLOCK)
#define FirstILockAddr          ((DWORD) PUserKData+FIRST_INTERLOCK)
#define IsPCInIlockAddr(pc)     ((DWORD) ((pc) - FirstILockAddr) < ILOCK_REGION_SIZE)

// Defines for CPU specific IDs.
#ifdef THUMBSUPPORT
#define THISCPUID IMAGE_FILE_MACHINE_THUMB
#else
#define THISCPUID IMAGE_FILE_MACHINE_ARM
#endif
#define PROCESSOR_ARCHITECTURE PROCESSOR_ARCHITECTURE_ARM

#endif // defined(ARM)

void InitPCBFunctions (void);
void InitInterlockedFunctions (void);

// NOTE: THE FOLLOWING 2 FUNCTIONS SHOULD ONLY BE CALLED WHEN PREEMPTION IS NOT ALLOWED. OR A 
//       THREAD CAN MIGRATE FROM ONE CPU TO ANOTHER LEAVING A STALE POINTER TO PCB
PPCB MultiCoreGetPCB (void);

FORCEINLINE PPCB GetPCB (void)
{
    return (((struct KDataStruct *)PUserKData)->nCpus > 1)
        ? MultiCoreGetPCB ()
        : (PPCB) PUserKData;
}

FORCEINLINE DWORD PcbGetCurCpu (void)
{
    return GetPCB ()->dwCpuId;
}

//
// The functions below can be called in any context.
//
DWORD MultiCorePCBGetDwordAtOffset (DWORD dwOfst);
void PCBSetDwordAtOffset (DWORD dwVal, DWORD dwOfst);
void PCBAddAtOffset (DWORD dwAddent, DWORD dwOfst);

//-------------------------------------------------------------------------
FORCEINLINE DWORD PCBGetDwordAtOffset (DWORD dwOfst)
{
    return (((struct KDataStruct *)PUserKData)->nCpus > 1)
        ? MultiCorePCBGetDwordAtOffset (dwOfst)
        : *(LPDWORD) (PUserKData + dwOfst);
}

FORCEINLINE DWORD PcbGetCurThreadId (void)
{
    return PCBGetDwordAtOffset (offsetof (PCB, CurThId));
}

FORCEINLINE DWORD PcbGetActvProcId (void)
{
    return PCBGetDwordAtOffset (offsetof (PCB, ActvProcId));
}

FORCEINLINE PTHREAD PcbGetCurThread (void)
{
    return (PTHREAD) PCBGetDwordAtOffset (offsetof (PCB, pCurThd));
}

FORCEINLINE PPROCESS PcbGetActvProc (void)
{
    return (PPROCESS) PCBGetDwordAtOffset (offsetof (PCB, pCurPrc));
}

FORCEINLINE PPROCESS PcbGetVMProc (void)
{
    return (PPROCESS) PCBGetDwordAtOffset (offsetof (PCB, pVMPrc));
}

FORCEINLINE BOOL PcbOwnSpinLock (void)
{
    return (BOOL) PCBGetDwordAtOffset (offsetof (PCB, ownspinlock));
}

//-------------------------------------------------------------------------
FORCEINLINE void PcbSetActvProcId (DWORD dwProcId)
{
    PCBSetDwordAtOffset (dwProcId, offsetof (PCB, ActvProcId));
}

FORCEINLINE void PcbSetActvProc (PPROCESS pprc)
{
    PCBSetDwordAtOffset ((DWORD) pprc, offsetof (PCB, pCurPrc));
}

FORCEINLINE void PcbSetVMProc (PPROCESS pprc)
{
    PCBSetDwordAtOffset ((DWORD) pprc, offsetof (PCB, pVMPrc));
}

FORCEINLINE void PcbSetTlsPtr (LPDWORD pTlsPtr)
{
    PCBSetDwordAtOffset ((DWORD) pTlsPtr, offsetof (PCB, lpvTls));
}

//-------------------------------------------------------------------------
FORCEINLINE void PcbIncOwnSpinlockCount (void)
{
    PCBAddAtOffset (1, offsetof (PCB, ownspinlock));
}
FORCEINLINE void PcbDecOwnSpinlockCount (void)
{
    PCBAddAtOffset ((DWORD)-1, offsetof (PCB, ownspinlock));
}

#ifdef __cplusplus
    }
#endif


// end_ntddk end_nthal

#endif // _NKARM_
