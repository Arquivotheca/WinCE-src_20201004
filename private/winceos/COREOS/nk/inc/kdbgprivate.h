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
/*++

Module Name:

    hdstub.h

Abstract:

    Defines and types for Hdstub.

Environment:

    CE Kernel

--*/

#pragma once

#include "hdstub_common.h"

/* Return value indicates whether exception processing needs to continue or not.
   pfHandled is the return value that is returned back to the OS. */
typedef BOOL (*KDBG_EXCEPTIONFN)(PEXCEPTION_RECORD pex, CONTEXT *pContext, BOOL b2ndChance, BOOL *pfIgnoreExceptionOnContinue);
typedef BOOL (*KDBG_VMPAGEINFN)(DWORD dwPageAddr, DWORD dwNumPages, BOOL bWriteable);
typedef BOOL (*KDBG_VMPAGEOUTFN)(DWORD dwPageAddr, DWORD dwNumPages);
typedef BOOL (*KDBG_MODLOADFN)(DWORD dwStructAddr);
typedef BOOL (*KDBG_MODUNLOADFN)(DWORD dwStructAddr);

// Hdstub client record
enum
{
    KDBG_EH_CODEBP = 0,
    KDBG_EH_DATABP= 1,
    KDBG_EH_THREADAFFINITY = 2,
    KDBG_EH_SSTEP = 3,
    KDBG_EH_ACTION = 4,
    KDBG_EH_FILTER = 5,
    KDBG_EH_DEBUGGER = 6,
    KDBG_EH_WATSON = 7,
    KDBG_EH_MAX
};
typedef struct KDBG_EVENT_HANDLER
{
    KDBG_EXCEPTIONFN pfnException;
    KDBG_VMPAGEINFN  pfnVmPageIn;
    KDBG_VMPAGEOUTFN  pfnVmPageOut;
    KDBG_MODLOADFN   pfnModLoad;
    KDBG_MODUNLOADFN pfnModUnload;
    DWORD dwFilter;
} KDBG_EVENT_HANDLER;

typedef struct _STRING // Counted String
{
    USHORT Length;
    USHORT MaximumLength;
    // [size_is(MaximumLength), length_is(Length) ]
    PCHAR Buffer;
} STRING, *PSTRING;

enum
{
    EXCEPTION_STACK_UNUSED = -1,
    EXCEPTION_STACK_SIZE = 3
};
// Stack Size = 3 because:
// 0 -> System
// 1 -> Exception in Debugger, or Break in OsAxs while Watson is capturing System
// 2 -> Exception in Debugger while there is a Break in OsAxs while Watson is capturing System
typedef struct _EXCEPTION_INFO
{
    EXCEPTION_RECORD *exception;
    CONTEXT *context;
    BOOL secondChance;
} EXCEPTION_INFO;


// status Constants for Packet waiting
// TODO: remove this since we use KITL
#define KDP_PACKET_RECEIVED    0x0000
#define KDP_PACKET_RESEND      0x0001
#define KDP_PACKET_UNEXPECTED  0x0002
#define KDP_PACKET_NONE        0xFFFF

enum VMBreakpointFlags
{
    BPF_16BIT = 1 << 0,
};

enum MAPADDR_OP
{
    MA_MAPIN  = 1,          // Return an pointer that is accessible. (May actually temporarily map in a page)
    MA_MAPOUT = 2,          // Releases a pointer. (May actually map out the page).
    MA_TRANSLATE = 3,       // Return the physical address (or kva.  Should be unique)
};

enum MEMADDR_FLAGS
{
    MF_TYPE_DEBUG    = 0,               // Debug memory. (No mapping required)
    MF_TYPE_VIRTUAL  = 1,               // Process virtual address (mapping req)
    MF_TYPE_PHYSICAL = 2,               // Physical address
    MF_TYPE_IO       = 3,               // IO space.
    MF_TYPE_MASK     = (1 << 4) - 1,    // First 4 bits for memory type.
    MF_WRITE         = 1 << 5,          // Write intent for this mem address.
};
typedef struct MEMADDR
{
    ULONG flags;
    PPROCESS vm;
    void *addr;
} MEMADDR;

void __inline SetVirtAddr(MEMADDR *ma, PROCESS *vmp, void *adr)
{
    ma->flags = MF_TYPE_VIRTUAL;
    ma->vm = vmp;
    ma->addr = adr;
}

void __inline SetPhysAddr(MEMADDR *ma, void *adr)
{
    ma->flags = MF_TYPE_PHYSICAL;
    ma->vm = NULL;
    ma->addr = adr;
}

void __inline SetDebugAddr(MEMADDR *ma, void *adr)
{
    ma->flags = MF_TYPE_DEBUG;
    ma->vm = NULL;
    ma->addr = adr;
}

#define AddrType(ma) ((ma)->flags & MF_TYPE_MASK)
#define AddrIsType(ma, type) (AddrType(ma) == (type))
#define AddrIsVirtual(ma)  AddrIsType(ma, MF_TYPE_VIRTUAL)
#define AddrIsPhysical(ma)  AddrIsType(ma, MF_TYPE_PHYSICAL)
#define AddrIsDebug(ma)  AddrIsType(ma, MF_TYPE_DEBUG)
BOOL __inline AddrEq(MEMADDR const *a1, MEMADDR const *a2)
{
    return AddrType(a1) == AddrType(a2) && // Type equal
            a1->addr == a2->addr &&     // Address equal
            (!AddrIsVirtual(a1) || a1->vm == a2->vm); // Not virt addr or vm equal
}

enum BreakpointState
{
    BPS_INACTIVE,
    BPS_ACTIVE,
    BPS_PAGEDOUT
};

typedef struct Breakpoint
{
    MEMADDR Address;
    ULONG unused_hProc;                 // Process affinity.
    ULONG hTh;                          // Thread associated with breakpoint
    ULONG BreakInstr;
    ULONG LastInstr;                    // Instruction that was last there
    BYTE InstrSize;
    BYTE fRomBP : 1;
    BYTE fWritten : 1;
    BYTE fDisabled : 1;
} Breakpoint;

// This structure is hosted in HD.DLL and contains a table of pointers to
// functionality that each debugger DLL exports to others.  This should
// cut down on stack overhead of calling into pfnCallClientIoctl (which
// will be kept temporarily around to make portability easier.)
typedef struct DEBUGGER_DATA
{
    DWORD sig0;      // Version check based on the size of this structure
    DWORD sig1;      // and the size of structure up to start of Hdstub API
    DWORD sig2;      // and the size of structure up to start of OsAxs API
    DWORD sig3;      // and the size of structure up to the start of the KD api

    KDBG_EVENT_HANDLER eventHandlers[KDBG_EH_MAX];
    EXCEPTION_INFO exceptionInfo[EXCEPTION_STACK_SIZE];
    int exceptionIdx;

    BOOL fIgnoredModuleNotifications;

    // hdstub api
    void (*pfnEnableHDEvents) (BOOL);
    void (*pfnDebugPrintf)(LPCWSTR, ...);
    void (*pfnFreezeSystem)(void);
    void (*pfnThawSystem)(void);
    HRESULT (*pfnAddVMBreakpoint)(MEMADDR *, ULONG, BOOL, Breakpoint **);
    HRESULT (*pfnDeleteVMBreakpoint)(Breakpoint *);
    HRESULT (*pfnDisableVMBreakpoint)(Breakpoint *);
    HRESULT (*pfnEnableVMBreakpoint)(Breakpoint *);
    HRESULT (*pfnFindVMBreakpoint)(MEMADDR *, Breakpoint *, Breakpoint **);
    ULONG (*pfnMoveMemory0)(MEMADDR const *, MEMADDR const *, ULONG);
    void *(*pfnMapAddr)(MEMADDR const *addr);
    void (*pfnUnMapAddr)(MEMADDR const *addr);
    PPROCESS (*pfnGetProperVM)(PPROCESS, void *);
    DWORD (*pfnGetPysicalAddress)(PPROCESS, void *);
    DWORD (*pfnAddrToPA)(const MEMADDR *);
    BOOL (*pfnOnEmbeddedBreakpoint)();
    BOOL (*pfnCurrently16Bit)();
    BOOL (*pfnIoControl)(DWORD, void *, DWORD);
    HRESULT (*pfnAddDataBreakpoint)(PPROCESS pVM, DWORD dwAddress, DWORD dwBytes, BOOL fHardware, BOOL fReadDBP, BOOL fWriteDBP, DWORD *dwBPHandle);
    HRESULT (*pfnDeleteDataBreakpoint)(DWORD dwHandle);
    DWORD (*pfnFindDataBreakpoint)(PPROCESS pVM, DWORD dwAddress, BOOL fWrite);
    DWORD (*pfnEnableDataBreakpointsOnPage)(PPROCESS pVM, DWORD dwAddress);
    DWORD (*pfnDisableDataBreakpointsOnPage)(PPROCESS pVM, DWORD dwAddress);
    DWORD (*pfnEnableAllDataBreakpoints)();
    DWORD (*pfnDisableAllDataBreakpoints)();
    BOOL  (*pfnIsValidProcPtr)(PPROCESS);
    void  (*pfnUpdateModLoadNotif)(void);
    void  (*pfnSetDisableAllBPOnHalt)(BOOL);
    BOOL  (*pfnDisableAllBPOnHalt)();
    HRESULT (*pfnSetBPState)(Breakpoint *, enum BreakpointState);
    HRESULT (*pfnGetBPState)(Breakpoint *, enum BreakpointState *);
    void  (*pfnSanitize)(void *, MEMADDR const *, ULONG);
    HRESULT (*pfnEnableAllBP)(void);

    // osaxs api
    void (*pfnCaptureExtraContext)(void);
    HRESULT (*pfnGetCpuContext)(DWORD, CONTEXT *, size_t);
    HRESULT (*pfnSetCpuContext)(DWORD, CONTEXT *, size_t);
    BOOL (*pfnWasCaptureDumpFileOnDeviceCalled)(EXCEPTION_RECORD *);
    PPROCESS (*pfnHandleToProcess)(HANDLE);
    PTHREAD (*pfnHandleToThread)(HANDLE);
    void (*pfnHandleOsAxsApi)(STRING *);
    void (*pfnPushExceptionState)(EXCEPTION_RECORD *, CONTEXT *, BOOL);
    void (*pfnPopExceptionState)(void);
    HRESULT (*pfnSingleStepThread)();
    HRESULT (*pfnSingleStepCBP)(BOOL);
    HRESULT (*pfnSingleStepDBP)(BOOL);
    HRESULT (*pfnGetNextPC)(ULONG *);
    ULONG (*pfnTranslateOffset)(ULONG);
    void (*pfnSimulateDebugBreakExecution)(void);
    ULONG (*pfnGetCallStack) (PTHREAD, PCONTEXT, ULONG, LPVOID, DWORD, DWORD);

    // kdbg api
    BYTE *kdbgBuffer;
    DWORD kdbgBufferSize;
    
    BOOL (*pfnHandleKdApi)(STRING *, BOOL *);
    void (*pfnKDBGSend)(WORD, GUID, STRING *, STRING *);
    WORD (*pfnKDBGRecv)(STRING *, DWORD *, GUID *);
    void (*pfnKDBGDebugMessage)(CHAR*, DWORD);

    // kernel API
#include "kdbgimports.h"
} DEBUGGER_DATA;

extern struct DEBUGGER_DATA *g_pDebuggerData;

enum
{
    DEBUGGER_DATA_SIG0 = sizeof(DEBUGGER_DATA),
    DEBUGGER_DATA_SIG1 = offsetof(DEBUGGER_DATA, pfnDebugPrintf),
    DEBUGGER_DATA_SIG2 = offsetof(DEBUGGER_DATA, pfnGetCpuContext),
    DEBUGGER_DATA_SIG3 = offsetof(DEBUGGER_DATA, pfnHandleKdApi),
};

#define VALID_DEBUGGER_DATA(dd) (           \
    (dd)->sig0 == DEBUGGER_DATA_SIG0 &&     \
    (dd)->sig1 == DEBUGGER_DATA_SIG1 &&     \
    (dd)->sig2 == DEBUGGER_DATA_SIG2 &&     \
    (dd)->sig3 == DEBUGGER_DATA_SIG3        \
    )

// Kernel imports
#define g_pKData (g_pDebuggerData->pKData)
#define g_pprcNK (g_pDebuggerData->pprcNK)
#define g_ppcbs (g_pDebuggerData->rgpPCBs)
#define dwCEInstructionSet (g_pDebuggerData->dwCEInstructionSet)
#define ppCaptureDumpFileOnDevice (g_pDebuggerData->ppCaptureDumpFileOnDevice)
#define ppKCaptureDumpFileOnDevice (g_pDebuggerData->ppKCaptureDumpFileOnDevice)
#define g_kitlLock (*(g_pDebuggerData->pKitlSpinLock))
#define g_schedLock (*(g_pDebuggerData->pSchedLock))
#define g_physLock (*(g_pDebuggerData->pPhysLock))
#define g_oalLock (*(g_pDebuggerData->pOalLock))
#define SystemAPISets (g_pDebuggerData->pSystemAPISets)
#define pTOC (g_pDebuggerData->pRomHdr)
#define LogPtr (g_pDebuggerData->pLogPtr)
#define OEMAddressTable (g_pDebuggerData->pOEMAddressTable)
#define ProcessorFeatures (*(g_pDebuggerData->pdwProcessorFeatures))
#define NKvDbgPrintfW (g_pDebuggerData->pfnNKvDbgPrintfW)

#define hKCoreDll (*(g_pDebuggerData->phKCoreDll))

#define g_pKHeapBase ((g_pDebuggerData->ppKHeapBase != NULL)? *(g_pDebuggerData->ppKHeapBase) : NULL)
#define g_pCurKHeapFree ((g_pDebuggerData->ppCurKHeapFree != NULL)? *(g_pDebuggerData->ppCurKHeapFree) : NULL)

#define fIntrusivePaging (*g_pDebuggerData->pfIntrusivePaging)
#define pulHDEventFilter (g_pDebuggerData->pulHDEventFilter)

#define INTERRUPTS_ENABLE (g_pDebuggerData->pfnINTERRUPTS_ENABLE)
#define AcquireSpinLock (g_pDebuggerData->pfnAcquireSpinLock)
#define ReleaseSpinLock (g_pDebuggerData->pfnReleaseSpinLock)
#define DeleteCriticalSection (g_pDebuggerData->pfnDeleteCriticalSection)
#define EnterCriticalSection (g_pDebuggerData->pfnEnterCriticalSection)
#define InitializeCriticalSection (g_pDebuggerData->pfnInitializeCriticalSection)
#define LeaveCriticalSection (g_pDebuggerData->pfnLeaveCriticalSection)

#define SCHL_DoThreadGetContext (g_pDebuggerData->pfnDoThreadGetContext)
#define MDCaptureFPUContext (g_pDebuggerData->pfnMDCaptureFPUContext)

#define EventModify (g_pDebuggerData->pfnEventModify)
#define EVNTModify (g_pDebuggerData->pfnEVNTModify)

#define GetPageTable (g_pDebuggerData->pfnGetPageTable)
#define GetPFNOfProcess (g_pDebuggerData->pfnGetPFNOfProcess)
#define VMCreateKernelPageMapping (g_pDebuggerData->pfnVMCreateKernelPageMapping)
#define VMRemoveKernelPageMapping (g_pDebuggerData->pfnVMRemoveKernelPageMapping)
#define GetProcPtr (g_pDebuggerData->pfnGetProcPtr)
#define h2pHDATA (g_pDebuggerData->pfnh2pHDATA)

#define HDCleanup (g_pDebuggerData->pfnHDCleanup)
#define KDCleanup (g_pDebuggerData->pKDCleanup)

#define HwTrap (g_pDebuggerData->pfnHwTrap)

#ifdef ARM
#define InSysCall (g_pDebuggerData->pfnInSysCall)
#endif

#ifndef x86
#define InterlockedDecrement (g_pDebuggerData->pfnInterlockedDecrement)
#define InterlockedIncrement (g_pDebuggerData->pfnInterlockedIncrement)
#endif

#define IsDesktopDbgrExist (g_pDebuggerData->pfnIsDesktopDbgrExist)
#define KCall (g_pDebuggerData->pfnKCall)
#define kdpInvalidateRange (g_pDebuggerData->pfnKdpInvalidateRange)
#define kdpIsROM (g_pDebuggerData->pfnKdpIsROM)
#define KITLIoctl (g_pDebuggerData->pfnKITLIoctl)
#define MakeWritableEntry (g_pDebuggerData->pfnMakeWritableEntry)
#define OEMGetTickCount (g_pDebuggerData->pfnOEMGetTickCount)
#define NKCacheRangeFlush (g_pDebuggerData->pfnNKCacheRangeFlush)
#define NKRegisterDbgZones (g_pDebuggerData->pfnNKRegisterDbgZones)
#define NKGetThreadCallStack (g_pDebuggerData->pfnNKGetThreadCallStack)
#define NKIsProcessorFeaturePresent (g_pDebuggerData->pfnNKIsProcessorFeaturePresent)
#define NKKernelLibIoControl (g_pDebuggerData->pfnNKKernelLibIoControl)
#define NKLoadKernelLibrary (g_pDebuggerData->pfnNKLoadKernelLibrary)
#define GetProcAddressA (g_pDebuggerData->pfnGetProcAddressA)
#define NKwvsprintfW (g_pDebuggerData->pfnNKwvsprintfW)
#define OEMGetRegDesc (g_pDebuggerData->pfnOEMGetRegDesc)
#define OEMReadRegs (g_pDebuggerData->pfnOEMReadRegs)
#define OEMWriteRegs (g_pDebuggerData->pfnOEMWriteRegs)
#define OEMKDIoControl (g_pDebuggerData->pfnOEMKDIoControl)

#define ResumeAllOtherCPUs (g_pDebuggerData->pfnResumeAllOtherCPUs)
#define StopAllOtherCPUs (g_pDebuggerData->pfnStopAllOtherCPUs)

#define SCHL_SetThreadBasePrio (g_pDebuggerData->pfnSCHL_SetThreadBasePrio)
#define SCHL_SetThreadQuantum (g_pDebuggerData->pfnSCHL_SetThreadQuantum)

#define SwitchActiveProcess (g_pDebuggerData->pfnSwitchActiveProcess)
#define SwitchVM (g_pDebuggerData->pfnSwitchVM)

#define VMAlloc (g_pDebuggerData->pfnVMAlloc)
#define VMFreeAndRelease (g_pDebuggerData->pfnVMFreeAndRelease)

#define AcquireKitlSpinLock() \
if(&g_kitlLock != NULL) \
{ \
    AcquireSpinLock(&g_kitlLock); \
} \

#define ReleaseKitlSpinLock() \
if(&g_kitlLock != NULL) \
{ \
    ReleaseSpinLock(&g_kitlLock); \
} \


// Inline definitions to satisfy other inline definitions in kernel headers.
__inline LPVOID Pfn2Virt(DWORD dwPfn)
{
    return g_pDebuggerData->pfnPfn2Virt(dwPfn);
}


#define DD_eventHandlers (g_pDebuggerData->eventHandlers)
#define DD_fIgnoredModuleNotifications (g_pDebuggerData->fIgnoredModuleNotifications)

// Hdstub functions
#define DD_EnableHDEvents (g_pDebuggerData->pfnEnableHDEvents)
#define DD_DebugPrintf (g_pDebuggerData->pfnDebugPrintf)
#define DD_FreezeSystem (g_pDebuggerData->pfnFreezeSystem)
#define DD_ThawSystem (g_pDebuggerData->pfnThawSystem)
#define DD_AddVMBreakpoint (g_pDebuggerData->pfnAddVMBreakpoint)
#define DD_DeleteVMBreakpoint (g_pDebuggerData->pfnDeleteVMBreakpoint)
#define DD_DisableVMBreakpoint (g_pDebuggerData->pfnDisableVMBreakpoint)
#define DD_EnableVMBreakpoint (g_pDebuggerData->pfnEnableVMBreakpoint)
#define DD_FindVMBreakpoint (g_pDebuggerData->pfnFindVMBreakpoint)
#define DD_MoveMemory0 (g_pDebuggerData->pfnMoveMemory0)
#define DD_MapAddr (g_pDebuggerData->pfnMapAddr)
#define DD_UnMapAddr (g_pDebuggerData->pfnUnMapAddr)
#define DD_GetProperVM (g_pDebuggerData->pfnGetProperVM)
#define DD_GetPhysicalAddress (g_pDebuggerData->pfnGetPysicalAddress)
#define DD_OnEmbeddedBreakpoint (g_pDebuggerData->pfnOnEmbeddedBreakpoint)
#define DD_Currently16Bit (g_pDebuggerData->pfnCurrently16Bit)
#define DD_IoControl (g_pDebuggerData->pfnIoControl)
#define DD_AddDataBreakpoint (g_pDebuggerData->pfnAddDataBreakpoint)
#define DD_DeleteDataBreakpoint (g_pDebuggerData->pfnDeleteDataBreakpoint)
#define DD_FindDataBreakpoint (g_pDebuggerData->pfnFindDataBreakpoint)
#define DD_EnableDataBreakpointsOnPage (g_pDebuggerData->pfnEnableDataBreakpointsOnPage)
#define DD_DisableDataBreakpointsOnPage (g_pDebuggerData->pfnDisableDataBreakpointsOnPage)
#define DD_EnableAllDataBreakpoints (g_pDebuggerData->pfnEnableAllDataBreakpoints)
#define DD_DisableAllDataBreakpoints (g_pDebuggerData->pfnDisableAllDataBreakpoints)
#define DD_AddrToPA (g_pDebuggerData->pfnAddrToPA)
#define DD_IsValidProcPtr (g_pDebuggerData->pfnIsValidProcPtr)
#define DD_UpdateModLoadNotif (g_pDebuggerData->pfnUpdateModLoadNotif)
#define DD_SetDisableAllBPOnHalt (g_pDebuggerData->pfnSetDisableAllBPOnHalt)
#define DD_DisableAllBPOnHalt (g_pDebuggerData->pfnDisableAllBPOnHalt)
#define DD_SetBPState (g_pDebuggerData->pfnSetBPState)
#define DD_GetBPState (g_pDebuggerData->pfnGetBPState)
#define DD_Sanitize (g_pDebuggerData->pfnSanitize)
#define DD_EnableAllBP (g_pDebuggerData->pfnEnableAllBP)

// Osaxs functions
#define DD_GetCpuContext (g_pDebuggerData->pfnGetCpuContext)
#define DD_SetCpuContext (g_pDebuggerData->pfnSetCpuContext)
#define DD_CaptureExtraContext (g_pDebuggerData->pfnCaptureExtraContext)
#define DD_WasCaptureDumpFileOnDeviceCalled (g_pDebuggerData->pfnWasCaptureDumpFileOnDeviceCalled)
#define DD_HandleToProcess (g_pDebuggerData->pfnHandleToProcess)
#define DD_HandleToThread (g_pDebuggerData->pfnHandleToThread)
#define DD_HandleOsAxsApi (g_pDebuggerData->pfnHandleOsAxsApi)
#define DD_PushExceptionState (g_pDebuggerData->pfnPushExceptionState)
#define DD_PopExceptionState (g_pDebuggerData->pfnPopExceptionState)
#define DD_ExceptionState (g_pDebuggerData->exceptionInfo[g_pDebuggerData->exceptionIdx])
#define DD_SingleStepThread (g_pDebuggerData->pfnSingleStepThread)
#define DD_SingleStepCBP (g_pDebuggerData->pfnSingleStepCBP)
#define DD_SingleStepDBP (g_pDebuggerData->pfnSingleStepDBP)
#define DD_GetNextPC (g_pDebuggerData->pfnGetNextPC)
#define DD_TranslateOffset (g_pDebuggerData->pfnTranslateOffset)
#define DD_GetCallStack (g_pDebuggerData->pfnGetCallStack)
#define DD_SimulateDebugBreakExecution (g_pDebuggerData->pfnSimulateDebugBreakExecution)
#define DD_kdbgBuffer (g_pDebuggerData->kdbgBuffer)
#define DD_kdbgBufferSize (g_pDebuggerData->kdbgBufferSize)
#define DD_HandleKdApi (g_pDebuggerData->pfnHandleKdApi)
#define DD_KDBGSend (g_pDebuggerData->pfnKDBGSend)
#define DD_KDBGRecv (g_pDebuggerData->pfnKDBGRecv)
#define DD_KDBGDebugMessage (g_pDebuggerData->pfnKDBGDebugMessage)

// Serial Debug message support
#ifdef DEBUG
#   ifdef SHIP_BUILD
#       undef SHIP_BUILD
#   endif
#endif

#ifdef SHIP_BUILD
#   define DEBUGGERMSG(cond, printf_exp) ((void)0)
#   define DBGRETAILMSG(cond, printf_exp) ((void)0)
#   define REGISTERDBGZONES(mod) ((void)0)
#else
#   ifdef DEBUG
#       define DEBUGGERMSG(cond, printf_exp) ((void)((cond)? (DD_DebugPrintf printf_exp), 1:0))
#   else
#       define DEBUGGERMSG(cond, printf_exp) ((void)0)
#   endif
#   define DBGRETAILMSG(cond, printf_exp) ((void)((cond)? (DD_DebugPrintf printf_exp), 1:0))
#   define REGISTERDBGZONES(mod) NKRegisterDbgZones(mod, &dpCurSettings)
#endif

#define KDD DD_DebugPrintf
#define KDLOG(x) KDD(TEXT("%s=0x%08x\r\n"), TEXT(#x), x)
#define KDLOGS(x) KDD(TEXT("%s=%s\r\n"), TEXT(#x), x)
#define KDASSERT(x) DEBUGGERMSG(!(x), (TEXT("ASSERT: %s, %d ") TEXT(#x) TEXT("\r\n"), TEXT(__FILE__), __LINE__))
#define KD_ASSERT KDASSERT

#ifdef DEBUG
#define KDVERIFY(x) DEBUGGERMSG(!(x), (TEXT("VERIFY: %s, %d ") TEXT(#x) TEXT("\r\n"), TEXT(__FILE__), __LINE__))
#else
#define KDVERIFY(x) x
#endif


// Support for exception handlers
#ifdef x86
#define DECL_EXCEPTION_HANDLER()
#define IMPL_EXCEPTION_HANDLER() \
    EXCEPTION_DISPOSITION __cdecl _except_handler3(PEXCEPTION_RECORD XRec, void *Reg, PCONTEXT Ctx, PDISPATCHER_CONTEXT Disp) \
    { return g_pDebuggerData->p_except_handler3(XRec, Reg, Ctx, Disp); } \
    BOOL __abnormal_termination(void) \
    { return g_pDebuggerData->p__abnormal_termination(); }
#else
#define DECL_EXCEPTION_HANDLER()
#define IMPL_EXCEPTION_HANDLER() \
    EXCEPTION_DISPOSITION __C_specific_handler(PEXCEPTION_RECORD Rec, void *Frame, PCONTEXT Ctx, PDISPATCHER_CONTEXT Disp) \
    { return g_pDebuggerData->p__C_specific_handler(Rec, Frame, Ctx, Disp); }
#endif

//Invalid data breakpoint handle.
#define INVALID_DBP_HANDLE 0

#define SET_BIT0 0x1
#define CLEAR_BIT0 (0xFFFFFFFE)


// Probing for hardware features
#if defined (SHx)
#define IsFPUPresent() NKIsProcessorFeaturePresent (PF_SHX_FPU)
#define IsFPUPresentEx() FALSE
#elif defined (MIPS)
#define IsFPUPresent() NKIsProcessorFeaturePresent (PF_MIPS_FPU)
#define IsFPUPresentEx() FALSE
#elif defined (ARM)
#define IsFPUPresent() NKIsProcessorFeaturePresent (PF_ARM_VFP_SUPPORT)
#define IsFPUPresentEx() NKIsProcessorFeaturePresent(PF_ARM_VFP_EXTENDED_REGISTERS)
#elif defined (x86)
#define IsFPUPresent() (!NKIsProcessorFeaturePresent (PF_FLOATING_POINT_EMULATED))
#define IsFPUPresentEx() FALSE
#endif

#define AV_EXCEPTION_PARAMETERS 2
#define AV_EXCEPTION_ACCESS 0 //0 = read, 1 = write.
#define AV_EXCEPTION_ADDRESS 1
