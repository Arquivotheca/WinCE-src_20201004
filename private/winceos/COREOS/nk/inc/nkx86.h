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
/*++ BUILD Version: 0002    // Increment this if a change has global effects


Module Name:

    nkx86.h

Abstract:

    User-mode visible x86 specific x86 structures and constants


--*/

#ifndef _NTx86_
#define _NTx86_


// begin_ntddk begin_nthal begin_winnt begin_ntminiport

#ifdef x86

//
// Disable these two pramas that evaluate to "sti" "cli" on x86 so that driver
// writers to not leave them inadvertantly in their code.
//

#if !defined(MIDL_PASS)
#if !defined(RC_INVOKED)

#pragma warning(disable:4164)   // disable C4164 warning so that apps that
                                // build with /Od don't get weird errors !
#ifdef _M_IX86
#if defined(__cplusplus)
extern "C" void _enable(void);
extern "C" void _disable(void);
#else
extern void _enable(void);
extern void _disable(void);
#endif
#pragma intrinsic(_enable)
#pragma intrinsic(_disable)
#endif

#pragma warning(default:4164)   // reenable C4164 warning

#endif
#endif

extern void OEMNMIHandler (void);

// end_ntddk end_nthal end_winnt end_ntminiport

//
//  Values put in ExceptionRecord.ExceptionInformation[0]
//  First parameter is always in ExceptionInformation[1],
//  Second parameter is always in ExceptionInformation[2]
//

#define EFLAGS_IF_BIT               9                   // bit 9 of EFLAGS is IF bit

#define EMX87_DATA_SIZE 40
#define SIZE_OF_FX_REGISTERS        128
#define SIZE_OF_FXSAVE_AREA         528

// CPUID_ flags are in pkfuncs.h
#define CPUIDEX_3DNOW               (1 << 31)
#define CPUIDEX_3DNOWEX             (1 << 30)

#define CR4_FXSR                    0x00000200      // CR4 fxsr enable bit
#define FXRESTOR_EAX    __asm {_emit 0fh} __asm {_emit 0aeh} __asm {_emit 08h}
#define FXSAVE_EAX      __asm {_emit 0fh} __asm {_emit 0aeh} __asm {_emit 00h}
#define FXSAVE_ECX      __asm {_emit 0fh} __asm {_emit 0aeh} __asm {_emit 01h}
#define MOV_EDX_CR4     __asm {_emit 0fh} __asm {_emit 020h} __asm {_emit 0e2h}
#define MOV_CR4_EDX     __asm {_emit 0fh} __asm {_emit 022h} __asm {_emit 0e2h}

typedef struct _FXSAVE_AREA {       // FXSAVE *has* to operate on a 16 byte
    USHORT  ControlWord;            // aligned buffer
    USHORT  StatusWord;
    USHORT  TagWord;
    USHORT  ErrorOpcode;
    ULONG   ErrorOffset;
    ULONG   ErrorSelector;
    ULONG   DataOffset;
    ULONG   DataSelector;
    ULONG   MXCsr;
    ULONG   Reserved2;
    UCHAR   RegisterArea[SIZE_OF_FX_REGISTERS];
    UCHAR   Reserved3[SIZE_OF_FX_REGISTERS];
    UCHAR   Reserved4[224];
    UCHAR   Align16Bytes[16];
} FXSAVE_AREA, *PFXSAVE_AREA;

typedef struct _NK_PCR {
    DWORD   ExceptionList;
    DWORD   InitialStack;
    DWORD   StackLimit;
    union {
        DWORD   Emx87Data[EMX87_DATA_SIZE];
        FLOATING_SAVE_AREA tcxFPU;
        FXSAVE_AREA tcxExtended;
    };
    DWORD   pretls[PRETLS_RESERVED];
    DWORD   tls[TLS_MINIMUM_AVAILABLE];
} NK_PCR;

#define FS_LIMIT    (12+4*PRETLS_RESERVED+SIZE_OF_FXSAVE_AREA-1) // PCR visble thru FS:

ERRFALSE(sizeof(FLOATING_SAVE_AREA) < SIZE_OF_FXSAVE_AREA);

// Note: To enforce 16 byte alignment, FLTSAVE_BACKOFF must be ANDed with 0xF0
#define FLTSAVE_BACKOFF (SIZE_OF_FXSAVE_AREA-16+PRETLS_RESERVED*4)
#define PTH_TO_FLTSAVEAREAPTR(pth) ((FLOATING_SAVE_AREA *)((((DWORD)(pth->tlsSecure))-FLTSAVE_BACKOFF) & 0xfffffff0))

#define TLS2PCR(tlsPtr)             ((NK_PCR*) ((char *)tlsPtr - offsetof(NK_PCR, tls)))

void UpdateRegistrationPtr (NK_PCR * pcr);

#define CALLEE_SAVED_REGS       6       // fs:0, ebx, edi, esi, ebp, esp

#define REG_EXCPLIST    0
#define REG_EBX         1
#define REG_ESI         2
#define REG_EDI         3
#define REG_EBP         4
#define REG_ESP         5   // needed only for SEH handler (need ESP at the point of API call)

#define REG_OFST_EXCPLIST   (REG_EXCPLIST*REGSIZE)
#define REG_OFST_EBX        (REG_EBX*REGSIZE)
#define REG_OFST_ESI        (REG_ESI*REGSIZE)
#define REG_OFST_EDI        (REG_EDI*REGSIZE)
#define REG_OFST_EBP        (REG_EBP*REGSIZE)
#define REG_OFST_ESP        (REG_ESP*REGSIZE)

//
// Call frame record definition.
//
// There is no standard call frame for NT/x86, but there is a linked
// list structure used to register exception handlers, this is it.
//

// begin_nthal
//
// Exception Registration structure
//

typedef struct _EXCEPTION_REGISTRATION_RECORD {
    struct _EXCEPTION_REGISTRATION_RECORD *Next;
    PEXCEPTION_ROUTINE Handler;
} EXCEPTION_REGISTRATION_RECORD;

typedef EXCEPTION_REGISTRATION_RECORD *PEXCEPTION_REGISTRATION_RECORD;

#define REGISTRATION_RECORD_PSL_BOUNDARY    (-2)

//
// Define constants for system IDTs
//

#define MAXIMUM_IDTVECTOR   0xff
#define PRIMARY_VECTOR_BASE 0x30        // 0-2f are x86 trap vectors

// begin_ntddk

// end_ntddk end_nthal end_winnt end_ntminiport

#define CONTEXT_TO_PROGRAM_COUNTER(Context) ((Context)->Eip)
#define CONTEXT_TO_STACK_POINTER(Context) ((Context)->Esp)

#define CONTEXT_TO_PARAM_1(Context) (*(DWORD*) (CONTEXT_TO_STACK_POINTER(Context) + sizeof(DWORD)))
#define CONTEXT_TO_PARAM_2(Context) (*(DWORD*) (CONTEXT_TO_STACK_POINTER(Context) + 2*sizeof(DWORD)))
#define CONTEXT_TO_PARAM_3(Context) (*(DWORD*) (CONTEXT_TO_STACK_POINTER(Context) + 3*sizeof(DWORD)))
#define CONTEXT_TO_PARAM_4(Context) (*(DWORD*) (CONTEXT_TO_STACK_POINTER(Context) + 4*sizeof(DWORD)))

#define CONTEXT_TO_RETVAL(Context)  ((Context)->Eax)

#define CONTEXT_LENGTH  (sizeof(CONTEXT))
#define CONTEXT_ALIGN   (sizeof(ULONG))
#define CONTEXT_ROUND   (CONTEXT_ALIGN - 1)

#define STACK_ALIGN         4           // 4-byte alignment for stack
#define INST_SIZE           0

//
// hardware exception id
//
#define ID_DIVIDE_BY_ZERO           0
#define ID_SINGLE_STEP              1
#define ID_THREAD_BREAK_POINT       2
#define ID_BREAK_POINT              3
// 4-5 not used
#define ID_ILLEGAL_INSTRUCTION      6
#define ID_FPU_NOT_PRESENT          7
#define ID_DOUBLE_FAULT             8
// 9-12 not used
#define ID_PROTECTION_FAULT         13
#define ID_PAGE_FAULT               14
// 15 not used
#define ID_FPU_EXCEPTION            16

#define MD_MAX_EXCP_ID              16

//
// thread context translation
//
#define THRD_CTX_TO_PC(pth)         ((pth)->ctx.TcxEip)
#define THRD_CTX_TO_SP(pth)         ((pth)->ctx.TcxEsp)
#define THRD_CTX_TO_RETVAL(pth)     ((pth)->ctx.TcxEax)
#define THRD_CTX_TO_PARAM_1(pth)    (((LPDWORD) ((pth)->ctx.TcxEsp))[1])
#define THRD_CTX_TO_PARAM_2(pth)    (((LPDWORD) ((pth)->ctx.TcxEsp))[2])


//
//  GDT selectors - These defines are R0 selector numbers, which means
//                  they happen to match the byte offset relative to
//                  the base of the GDT.
//

#define KGDT_NULL           0x0000
#define KGDT_R0_CODE        0x0008
#define KGDT_R0_DATA        0x0010
#define KGDT_R1_CODE        0x0018
#define KGDT_R1_DATA        0x0020
#define KGDT_R3_CODE        0x0038
#define KGDT_R3_DATA        0x0040
#define KGDT_MAIN_TSS       0x0048
#define KGDT_NMI_TSS        0x0050
#define KGDT_DOUBLE_TSS     0x0058
#define KGDT_PCR            0x0060
#define KGDT_EMX87          0x0068
#define KGDT_UPCB           0x0070
#define KGDT_KPCB           0x0078

//
// Process Ldt Information
//  NtQueryInformationProcess using ProcessLdtInformation
//

typedef struct _LDT_INFORMATION {
    ULONG Start;
    ULONG Length;
    LDT_ENTRY LdtEntries[1];
} PROCESS_LDT_INFORMATION, *PPROCESS_LDT_INFORMATION;

//
// Process Ldt Size
//  NtSetInformationProcess using ProcessLdtSize
//

typedef struct _LDT_SIZE {
    ULONG Length;
} PROCESS_LDT_SIZE, *PPROCESS_LDT_SIZE;

//
// Thread Descriptor Table Entry
//  NtQueryInformationThread using ThreadDescriptorTableEntry
//

// begin_windbgkd

typedef struct _DESCRIPTOR_TABLE_ENTRY {
    ULONG Selector;
    LDT_ENTRY Descriptor;
} DESCRIPTOR_TABLE_ENTRY, *PDESCRIPTOR_TABLE_ENTRY;

// end_windbgkd

// soft interrupt used to handle API call and KCall
#define SYSCALL_INT         0x20
#define KCALL_INT           0x22
#define RESCHED_INT         0x24


typedef struct TContext CPUCONTEXT, *PCPUCONTEXT;
struct TContext {
    ULONG   TcxGs;
    ULONG   TcxFs;
    ULONG   TcxEs;
    ULONG   TcxDs;
    ULONG   TcxEdi;
    ULONG   TcxEsi;
    ULONG   TcxEbp;
    ULONG   TcxNotEsp;
    ULONG   TcxEbx;
    ULONG   TcxEdx;
    ULONG   TcxEcx;
    ULONG   TcxEax;
    ULONG   TcxError;
    ULONG   TcxEip;
    ULONG   TcxCs;
    ULONG   TcxEFlags;
    ULONG   TcxEsp;
    ULONG   TcxSs;
};

#define retValue ctx.TcxEax
#define SetThreadIP(pth, addr) ((pth)->ctx.TcxEip = (ULONG)(addr))
#define GetThreadIP(pth) ((pth)->ctx.TcxEip)

/* Query & set thread's kernel vs. user mode state */
#define KERNEL_MODE     0
#define USER_MODE       1

#define GetThreadMode(pth) ((WORD) (pth)->ctx.TcxCs == (KGDT_R3_CODE|3))
#define SetThreadMode(pth, mode) ((mode) ?  \
            ((pth)->ctx.TcxCs = (KGDT_R3_CODE|3),   \
            (pth)->ctx.TcxSs = (KGDT_R3_DATA|3))    \
        :   ((pth)->ctx.TcxCs = (KGDT_R1_CODE|1),   \
            (pth)->ctx.TcxSs = (KGDT_R1_DATA|1)) )

/* Query & set kernel vs. user mode state via Context */
#define GetContextMode(pctx) ((WORD) (pctx)->SegCs == (KGDT_R3_CODE|3))
#define SetContextMode(pctx, mode)  ((mode) ?   \
            ((pctx)->SegCs = (KGDT_R3_CODE|3),  \
            (pctx)->SegSs = (KGDT_R3_DATA|3))   \
        :   ((pctx)->SegCs = (KGDT_R1_CODE|1),  \
            (pctx)->SegSs = (KGDT_R1_DATA|1)) )

#define INTERRUPTS_ON()     _enable()
#define INTERRUPTS_OFF()    _disable()

#define MDCheckInterlockOperation(x)
#define InitInterlockedFunctions()

#pragma warning(disable:4035)               // re-enable below

void *InterlockedPopList(volatile void *pHead);
void InterlockedPushList(volatile void *pHead, void *pItem);

#if 0
__inline void *InterlockedPopList(void *pHead)
{
    void *ret;

    _disable();
    if ((ret = *(void **)pHead) != 0)
        *(void **)pHead = *(void **)ret;
    _enable();
    return ret;
}


__inline void *InterlockedPushList(volatile void *pHead, void *pItem)
{
    __asm {
        mov     ecx, dword ptr [pHead]  // (ecx) = pHead
        mov     edx, dword ptr [pItem]  // (edx) = pItem
        mov     eax, dword ptr [ecx]    // (eax) = *pHead
    x:  mov     dword ptr [edx], eax    // pItem->next = *pHead
        cmpxchg dword ptr [ecx], edx    // ICMPXG (*pHead, pItem) (i.e. *pHead = pItem)
        jnz     x                       // someone chagned phead? retry if yes
    }
}
#endif

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
PVOID __inline 
GetRegistrationHead(void)
{
    __asm mov eax, dword ptr fs:[0];
}
        

#pragma warning(default:4035)

#pragma warning(disable:4733)
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void __inline 
StoreRegistrationPointer(
    void *RegistrationPointer
    )
{
    __asm {
        mov eax, [RegistrationPointer]
        mov dword ptr fs:[0], eax
    }
}
#pragma warning(default:4733)




// Defines for CPU specific IDs.
#define THISCPUID IMAGE_FILE_MACHINE_I386
#define PROCESSOR_ARCHITECTURE PROCESSOR_ARCHITECTURE_INTEL
extern DWORD ProcessorFeatures;
extern DWORD ProcessorFeaturesEx;
#define FN_BITS_PER_TAGWORD         16
#define FN_TAG_EMPTY                0x3
#define FN_TAG_MASK                 0x3
#define FX_TAG_VALID                0x1
#define NUMBER_OF_FP_REGISTERS      8
#define BYTES_PER_FP_REGISTER       10
#define BYTES_PER_FX_REGISTER       16
#define TS_MASK                     0x00000008
#define FPTYPE_HARDWARE 1
#define FPTYPE_SOFTWARE 2
extern DWORD dwFPType;

// begin_ntddk begin_nthal
#endif // x86
// end_ntddk end_nthal

//
// Library function prototypes.
//

VOID
RtlCaptureContext (
    OUT PCONTEXT ContextRecord
    );

//
// Additional information supplied in QuerySectionInformation for images.
//

#define SECTION_ADDITIONAL_INFO_USED 0

//
// GDT Entry
//

typedef struct _KGDTENTRY {
    USHORT  LimitLow;
    USHORT  BaseLow;
    union {
        struct {
            UCHAR   BaseMid;
            UCHAR   Flags1;     // Declare as bytes to avoid alignment
            UCHAR   Flags2;     // Problems.
            UCHAR   BaseHi;
        } Bytes;
        struct {
            ULONG   BaseMid : 8;
            ULONG   Type : 5;
            ULONG   Dpl : 2;
            ULONG   Pres : 1;
            ULONG   LimitHi : 4;
            ULONG   Sys : 1;
            ULONG   Reserved_0 : 1;
            ULONG   Default_Big : 1;
            ULONG   Granularity : 1;
            ULONG   BaseHi : 8;
        } Bits;
    } HighWord;
} KGDTENTRY, *PKGDTENTRY;

#define TYPE_CODE   0x10  // 11010 = Code, Readable, NOT Conforming, Accessed
#define TYPE_DATA   0x12  // 10010 = Data, ReadWrite, NOT Expanddown, Accessed
#define TYPE_TSS    0x09  // 01001 = NonBusy 486 TSS
#define TYPE_LDT    0x02  // 00010 = LDT

#define DPL_USER    3
#define DPL_SYSTEM  0

#define GRAN_BYTE   0
#define GRAN_PAGE   1

#define SELECTOR_TABLE_INDEX 0x04

//
// Entry of Interrupt Descriptor Table (IDTENTRY)
//

typedef struct _KIDTENTRY {
   USHORT Offset;
   USHORT Selector;
   USHORT Access;
   USHORT ExtendedOffset;
} KIDTENTRY;

typedef KIDTENTRY *PKIDTENTRY;

//
// Access types for IDT entries
//
#define TRAP_GATE       0x8F00
#define RING1_TRAP_GATE 0xAF00
#define RING3_TRAP_GATE 0xEF00
#define INTERRUPT_GATE  0x8E00
#define RING1_INT_GATE  0xAE00
#define RING3_INT_GATE  0xEE00
#define TASK_GATE       0x8500


//
// TSS (Task switch segment) NT only uses to control stack switches.
//
//  The only fields we use are Esp0, Ss0, the IoMapBase
//  and the IoAccessMaps themselves.
//
//
//  Size of TSS must be <= 0xDFFF
//


typedef struct _KTSS {

    USHORT  Backlink;
    USHORT  Reserved0;

    ULONG   Esp0;
    USHORT  Ss0;
    USHORT  Reserved1;

    ULONG   Esp1;
    USHORT  Ss1;
    USHORT  Reserved2;

    ULONG   Esp2;
    USHORT  Ss2;
    USHORT  Reserved3;

    ULONG   CR3;

    ULONG   Eip;
    ULONG   Eflags;

    ULONG   Eax;
    ULONG   Ecx;
    ULONG   Edx;
    ULONG   Ebx;
    ULONG   Esp;
    ULONG   Ebp;
    ULONG   Esi;
    ULONG   Edi;

    USHORT  Es;
    USHORT  Reserved4;

    USHORT  Cs;
    USHORT  Reserved5;

    USHORT  Ss;
    USHORT  Reserved6;

    USHORT  Ds;
    USHORT  Reserved7;

    USHORT  Fs;
    USHORT  Reserved8;

    USHORT  Gs;
    USHORT  Reserved9;

    USHORT  LDT;
    USHORT  Reserved10;

    USHORT  Flags;

    USHORT  IoMapBase;
} KTSS, *PKTSS;

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

    // CPU dependent part
    ULONGLONG * pGDT_unused;            /* 0x2a0 ptr to global descriptor table */
    DWORD       dwCpuCap;               /* 0x2a4 - CPU capability bits */
    DWORD       dwOEMInitGlobalsAddr;   /* 0x2a8 ptr to OAL entry point */
    PADDRMAP    pAddrMap;               /* 0x2ac - ptr to OEMAddressTable */
    PKIDTENTRY  pIDT_unused;            /* 0x2b0 - ptr to interrupt descriptor table */
    KTSS      * pTSS_unused;            /* 0x2b4 - ptr to per-CPU TSS */
    long        alPad[18];              /* 0x2bc - padding */

    // aInfo MUST BE AT OFFSET 0x300
    DWORD       aInfo[32];              /* 0x300 - misc. kernel info */
                                        /* 0x380-0x400 reserved */
                                        /* 0x400 - end */
}; /* KDataStruct */


typedef struct _PCB {

#include "pcb_common.h"
    // CPU dependent part
    PKGDTENTRY  pGDT;                   /* 0x2a0 ptr to global descriptor table */
    DWORD       dwCpuCap_unused;        /* 0x2a4 - CPU capability bits */
    DWORD       dwOEMInitGlobalsAddr_unused;/* 0x2a8 ptr to OAL entry point */
    PADDRMAP    pAddrMap_unused;        /* 0x2ac - ptr to OEMAddressTable */
    PKIDTENTRY  pIDT;                   /* 0x2b0 - ptr to interrupt descriptor table */
    KTSS      * pTSS;                   /* 0x2b4 - ptr to per-CPU TSS */
    long        alPad[18];              /* 0x2b8 - padding */

    // aInfo MUST BE AT OFFSET 0x300
    DWORD       aInfo_unused[32];       /* 0x300 - misc. kernel info */
                                        /* 0x380-0x400 reserved */
                                        /* 0x400 - end */
} PCB, *PPCB;

__inline BOOL IsPSLBoundary (DWORD dwAddr)
{
    // user UserKData (), instead of KData directly, for this can be called in user mode
    struct KDataStruct *pKData = (struct KDataStruct *) PUserKData;
    return (dwAddr == SYSCALL_RETURN)
        || (dwAddr == pKData->pAPIReturn);
}

// NOTE: THE FOLLOWING 2 FUNCTIONS SHOULD ONLY BE CALLED WHEN PREEMPTION IS NOT ALLOWED. OR A 
//       THREAD CAN MIGRATE FROM ONE CPU TO ANOTHER LEAVING A STALE POINTER TO PCB
FORCEINLINE PPCB GetPCB (void)
{
    _asm { 
        mov eax, gs:[0] PCB.pSelf 
    }
}

FORCEINLINE DWORD PcbGetCurCpu (void)
{
    _asm { mov eax, gs:[0] PCB.dwCpuId }
}


//
// The functions below can be called in any context.
//
FORCEINLINE DWORD PcbGetCurThreadId (void)
{
    _asm {
        mov eax, gs:[0] PCB.CurThId
    }
}

FORCEINLINE DWORD PcbGetActvProcId (void)
{
    _asm {
        mov eax, gs:[0] PCB.ActvProcId
    }
}

FORCEINLINE PTHREAD PcbGetCurThread (void)
{
    _asm {
        mov eax, gs:[0] PCB.pCurThd
    }
}
FORCEINLINE PPROCESS PcbGetActvProc (void)
{
    _asm {
        mov eax, gs:[0] PCB.pCurPrc
    }
}
FORCEINLINE PPROCESS PcbGetVMProc (void)
{
    _asm {
        mov eax, gs:[0] PCB.pVMPrc
    }
}

FORCEINLINE DWORD PCBGetDwordAtOffset (DWORD dwOfst)
{
    _asm { 
        mov ecx, dword ptr [dwOfst]
        mov eax, gs:[ecx]
    }
}

FORCEINLINE void PcbSetActvProcId (DWORD dwProcId)
{
    _asm {
        mov eax, [dwProcId]
        mov gs:[0] PCB.ActvProcId, eax
    }
}

FORCEINLINE void PcbSetActvProc (PPROCESS pprc)
{
    _asm {
        mov eax, [pprc]
        mov gs:[0] PCB.pCurPrc, eax
    }
}
FORCEINLINE void PcbSetVMProc (PPROCESS pprc)
{
    _asm {
        mov eax, [pprc]
        mov gs:[0] PCB.pVMPrc, eax
    }
}
FORCEINLINE void PcbSetTlsPtr (LPDWORD pTlsPtr)
{
    _asm {
        mov eax, [pTlsPtr]
        mov gs:[0] PCB.lpvTls, eax
    }
}

FORCEINLINE void PcbIncOwnSpinlockCount (void)
{
    _asm {
        inc dword ptr gs:[0] PCB.ownspinlock  // increment the # of owned spinlock - no reschedule from here
    }
}

FORCEINLINE void PcbDecOwnSpinlockCount (void)
{
    _asm {
        dec dword ptr gs:[0] PCB.ownspinlock  // decrement the # of owned spinlock - reschedule can happen from here
    }
}

FORCEINLINE BOOL InPrivilegeCall (void)
{
    _asm {
        xor eax, eax
        cmp byte ptr gs:[0] PCB.cNest, 1       // (GetPCB()->cNest == 1)?
        je  short __return__
        inc eax
    __return__:
    }
}

FORCEINLINE BOOL InSysCall (void)
{
    _asm {
        mov eax, dword ptr gs:[0] PCB.ownspinlock
        cmp byte ptr gs:[0] PCB.cNest, 1       // (GetPCB()->cNest == 1)?
        je  short __return__
        inc eax
    __return__:
    }
}


extern struct KDataStruct  *g_pKData;

void InitializeEmx87(void);
void InitializeFPU(void);
void InitFPUIDTs (FARPROC NPXNPHandler);
extern DWORD dwFPType;

#pragma pack(push, 1)           // We want this structure packed exactly as declared!

typedef struct {
    USHORT Size;
    PVOID Base;
} FWORDPtr;

#pragma pack(pop)

#endif // _NTx86_
