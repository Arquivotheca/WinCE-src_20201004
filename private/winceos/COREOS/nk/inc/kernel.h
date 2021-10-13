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

/*
    Kernel.h - internal kernel data

    This file contains the kernel's internal data structures, function prototypes,
    macros, etc. It is not intended for use outside of the kernel.
*/

#ifndef __KERNEL_H__
#define __KERNEL_H__

#define NO_BCOEMGLOBAL  // avoid using legacy mappings to 5.0 shared variables in OEMGLOBAL.H


#include "windows.h"
#include "kcalls.h"

#include "patcher.h"
#include "handle.h"

#define C_ONLY



#define IN
#define OUT
#define OPTIONAL


#include "vm.h"
#include "dbgrapi.h"

#include "schedule.h"

#include "kwin32.h"

extern DWORD randdw1,randdw2;
extern HMODULE hCoreDll, hKCoreDll;



BOOL ReturnTrue();
BOOL NotSupported();
BOOL ReturnFalse (void);
DWORD ReturnNegOne (void);
DWORD ReturnSelf (DWORD id);

#include "nkintr.h"
#include "intrapi.h"

#define MAX_PCB_OFST            0x84        // actual KData starts at offset 0x84 in KPAGE

#if defined(MIPS)
#include "nkmips.h"
#elif defined(SHx)
#include "nkshx.h"
#elif defined(x86)
#include "nkx86.h"
#elif defined(ARM)
#include "nkarm.h"
#else
#pragma error("No CPU Defined")
#endif

#ifdef ARM
void DataMemoryBarrier (void);
void DataSyncBarrier (void);
#else
#define DataMemoryBarrier()
#define DataSyncBarrier()
#endif

#define MAX_CPU 254
extern PPCB    g_ppcbs[MAX_CPU];

#define dwCurThId               PcbGetCurThreadId()
#define dwActvProcId            PcbGetActvProcId()
#define pCurThread              PcbGetCurThread()
#define pActvProc               PcbGetActvProc()
#define pVMProc                 PcbGetVMProc()
#define PendEvents1(ppcb)       ((ppcb)->aPend1)
#define PendEvents2(ppcb)       ((ppcb)->aPend2)

#define IntrEvents              (g_pKData->alpeIntrEvents)
#define KInfoTable              (g_pKData->aInfo)
#define DIRECT_RETURN           (g_pKData->pAPIReturn)
#define InDebugger              (g_pKData->dwInDebugger)
#define MemForPT                (g_pKData->nMemForPT)

// DO NOT USE KInfoTable[KINX_PAGEFREE] for free count 
// of pages inside kernel. That count includes # pages 
// held in page pool above target.
#define PageFreeCount           (g_pKData->PageFreeCount)
#define PageFreeCount_Pool      ((long)KInfoTable[KINX_PAGEFREE])

BOOL INTERRUPTS_ENABLE (BOOL fEnable);
BOOL NKReboot (BOOL fClean);

ERRFALSE((SYSHANDLE_OFFSET + (SH_CURTHREAD * sizeof(HANDLE))) == offsetof(PCB, CurThId));
ERRFALSE((SYSHANDLE_OFFSET + (SH_CURPROC   * sizeof(HANDLE))) == offsetof(PCB, ActvProcId));
ERRFALSE(CURTLSPTR_OFFSET == offsetof(PCB, lpvTls));
ERRFALSE(KINFO_OFFSET == offsetof(struct KDataStruct, aInfo));

#include "thread.h"

extern FARPROC pExitThread;
extern FARPROC pUsrExcpExitThread;
extern FARPROC pKrnExcpExitThread;
extern FARPROC pCaptureDumpFileOnDevice;
extern FARPROC pKCaptureDumpFileOnDevice;

__inline BOOL IsInRange (DWORD x, DWORD start, DWORD end)
{
    return (x >= start) && (x < end);
}

#define KERN_TRUST_FULL 2
#define KERN_TRUST_RUN 1

#define TRUSTED_API(apiName, retval)    
#define TRUSTED_API_VOID(apiName)       

#include "process.h"
#include "handle.h"

#include "mapfile.h"
#include "watchdog.h"

#include "fscall.h"

#include <nkglobal.h>

#define CurMSec         (g_pNKGlobal->dwCurMSec)
#define dwReschedTime   (g_pNKGlobal->dwNextReschedTime)


// dwFlags for MODULES
// #define DONT_RESOLVE_DLL_REFERENCES     0x00000001   // defined in winbase.h
// #define LOAD_LIBRARY_AS_DATAFILE        0x00000002   // defined in winbase.h
// #define LOAD_WITH_ALTERED_SEARCH_PATH   0x00000008   // defined in winbase.h

DWORD GetContiguousPages(DWORD dwPages, DWORD dwAlignment, DWORD dwFlags);
void CreateNewProc(ulong nextfunc, ulong param);
void KernelInit2(void);
DWORD WINAPI RunApps(LPVOID param);
extern LPCWSTR FindModuleNameAndOffset (DWORD dwPC, LPDWORD pdwOffset);

void SetCPUASID(PTHREAD pth);
#ifdef SHx
void SetCPUGlobals(void);
#endif

#define PSDFromLPName(pName)     ((PSECDESCHDR) &pName->name[1])

void NKUnicodeToAscii(LPCHAR, LPCWSTR, int);
void NKAsciiToUnicode(LPWSTR wchptr, LPCSTR chptr, int maxlen);

// this is a common function used to reserve stack while handling hardware exception
void UpdateCallerInfo (PTHREAD pth, BOOL fInKMode);
BOOL ExceptionDispatch (PEXCEPTION_RECORD pExr, PCONTEXT pCtx);

ERRFALSE (sizeof(EXCEPTION_RECORD) == 0x50);

void StopAllOtherCPUs (void);
void ResumeAllOtherCPUs (void);
void NKIdle (DWORD dwIdleParam);

//
// machine dependent functions for exception handling
//
#ifdef DEBUG
int MDDbgPrintExceptionInfo (PEXCEPTION_RECORD pExr, PCALLSTACK pcstk);
#endif

void MDCaptureFPUContext (PCONTEXT pCtx);
BOOL MDHandleHardwareException (PCALLSTACK pcstk, PEXCEPTION_RECORD pExr, PCONTEXT pCtx, LPDWORD pdwExcpId);
LPDWORD MDGetRaisedExceptionInfo (PEXCEPTION_RECORD pExr, PCONTEXT pCtx, PCALLSTACK pcstk);
void MDSkipBreakPoint (PEXCEPTION_RECORD pExr, PCONTEXT pCtx);
BOOL MDDumpFrame (PTHREAD pth, DWORD dwExcpId, PEXCEPTION_RECORD pExr, PCONTEXT pCtx, DWORD dwLevel);
BOOL MDIsPageFault (DWORD dwExcpId);
void MDRestoreCalleeSavedRegisters (PCALLSTACK pcstk, PCONTEXT pCtx);
BOOL MDDumpThreadContext (PTHREAD pth, DWORD arg1, DWORD arg2, DWORD arg3, DWORD dwLevel);
void MDSetupExcpInfo (PTHREAD pth, PCALLSTACK pcstk, DWORD arg1, DWORD arg2, DWORD arg3);
DWORD ResetThreadStack (PTHREAD pth);
void MDClearVolatileRegs (PCONTEXT pCtx);

extern FARPROC g_pfnKrnRtlDispExcp;
extern FARPROC g_pfnUsrRtlDispExcp;


LPWSTR SC_GetCommandLineW(VOID);
PFNVOID DBG_CallCheck(PTHREAD pth, DWORD dwJumpAddress, PCONTEXT pCtx);
#define DBG_ReturnCheck(pth) ((pth)->pcstkTop ? (pth)->pcstkTop->retAddr : 0)
BOOL UserDbgTrap(EXCEPTION_RECORD *er, PCONTEXT pctx, BOOL bFirst);
void SurrenderCritSecs(void);

void DumpDwords (const DWORD *pdw, int len);

extern FARPROC TBFf;
extern FARPROC KTBFf;
extern FARPROC MTBFf;
extern DWORD (WINAPI *pfnKrnGetHeapSnapshot) (THSNAP *pSnap);
extern DWORD (WINAPI *pfnUsrGetHeapSnapshot) (THSNAP *pSnap);


void KernelFindMemory(void);
void InitAllOtherCPUs (void);
void StartAllCPUs (void);


BOOL FindROMFile(DWORD dwType, DWORD dwFileIndex, LPVOID* ppAddr, DWORD*  pLen);
extern ROMChain_t *ROMChain;

DWORD CECompress(LPBYTE  lpbSrc, DWORD cbSrc, LPBYTE lpbDest, DWORD cbDest, WORD wStep, DWORD dwPagesize);
DWORD CEDecompress(LPBYTE  lpbSrc, DWORD cbSrc, LPBYTE  lpbDest, DWORD cbDest, DWORD dwSkip, WORD wStep, DWORD dwPagesize);
DWORD CEDecompressROM(LPBYTE  lpbSrc, DWORD cbSrc, LPBYTE  lpbDest, DWORD cbDest, DWORD dwSkip, WORD wStep, DWORD dwPagesize);
extern BOOL g_fCompressionSupported;

DWORD DecompressROM(LPBYTE BufIn, DWORD InSize, LPBYTE BufOut, DWORD OutSize, DWORD skip);

extern PHDATA phdAlarmThreadWakeup;
extern PHDATA phdPslNotifyEvent;

int ropen(WCHAR *name, int mode);
int rread(int fd, char *buf, int cnt);
int rreadseek(int fd, char *buf, int cnt, int off);
int rlseek(int fd, int off, int mode);
int rclose(int fd);
int rwrite(int fd, char *buf, int cnt);

LPVOID memset(LPVOID dest, int c, unsigned int count);
LPVOID memcpy(LPVOID dest, const void *src, unsigned int count);

// IPI Commnads
#define IPI_STOP_CPU            1
#define IPI_RESCHEDULE          2
#define IPI_SUSPEND_THREAD      3
#define IPI_INVALIDATE_VM       4
#define IPI_PREPARE_POWEROFF    5
#define IPI_TERMINATE_THREAD    7

void SendIPI (DWORD dwCommand, DWORD dwData);


#if defined(WIN32_CALL) && !defined(KEEP_SYSCALLS)
#undef InputDebugCharW
int WINAPI InputDebugCharW(void);
#undef OutputDebugStringW
VOID WINAPI OutputDebugStringW(LPCWSTR str);
#undef NKvDbgPrintfW
VOID NKvDbgPrintfW(LPCWSTR lpszFmt, va_list lpParms);
#endif

#define DUMMY_THREAD_ID     0x16
extern THREAD dummyThread;

void NKOutputDebugString (LPCWSTR str);

void InitMUILanguages(void);

DWORD OEMNotifyIntrOccurs (DWORD dwSysIntr);

#ifndef SH4
#define NKSetRAMMode NotSupported
#define NKSetStoreQueueBase NotSupported
#endif

#ifdef IN_KERNEL

typedef HANDLE (* PFN_FindResource) (HMODULE hModule, LPCWSTR lpszName, LPCWSTR lpszType);
extern PFN_FindResource g_pfnFindResource;

#undef GetProcAddressA
#define GetProcAddressA (* g_pfnGetProcAddrA)
#undef GetProcAddressW
#define GetProcAddressW (* g_pfnGetProcAddrW)
#undef WaitForMultipleObjects
#define WaitForMultipleObjects ++ DO NOT USE ++
#undef WaitForSingleObject
#define WaitForSingleObject ++ DO NOT USE ++
#undef CreateEventW
#define CreateEventW NKCreateEvent
#undef OpenEventW
#define OpenEventW NKOpenEvent
#undef Sleep
#define Sleep NKSleep

#undef SetLastError
#define SetLastError(err) (pCurThread->dwLastError = err)
#undef GetLastError
#define GetLastError() (pCurThread->dwLastError)
#undef FreeLibrary
#define FreeLibrary NKFreeLibrary
#undef LoadKernelLibrary
#define LoadKernelLibrary NKLoadKernelLibrary

extern DWORD g_dwNXFlags;
    
#endif  // IN_KERNEL

/* Kernel zones */

extern DBGPARAM dpCurSettings;

#define ZONE_SCHEDULE   DEBUGZONE(0)    /* 0x0001 */
#define ZONE_MEMORY     DEBUGZONE(1)    /* 0x0002 */
#define ZONE_OBJDISP    DEBUGZONE(2)    /* 0x0004 */
#define ZONE_DEBUGGER   DEBUGZONE(3)    /* 0x0008 */
#define ZONE_SECURITY   DEBUGZONE(4)    /* 0x0010 */
#define ZONE_LOADER1    DEBUGZONE(5)    /* 0x0020 */
#define ZONE_VIRTMEM    DEBUGZONE(6)    /* 0x0040 */
#define ZONE_LOADER2    (DEBUGZONE(7)&&!pCurThread->bDbgCnt)    /* 0x0080 */
#define ZONE_DEBUG      DEBUGZONE(8)    /* 0x0100 */
#define ZONE_MAPFILE    DEBUGZONE(9)    /* 0x0200 */
#define ZONE_PHYSMEM    DEBUGZONE(10)   /* 0x0400 */
#define ZONE_SEH        (DEBUGZONE(11)&&!pCurThread->bDbgCnt)   /* 0x0800 */
#define ZONE_OPENEXE    DEBUGZONE(12)   /* 0x1000 */
#ifdef ZONE_ERROR
#undef ZONE_ERROR
#define ZONE_ERROR      DEBUGZONE(13)   /* 0x2000 */ /* was ZONE_MEMTRACKER */
#endif
#define ZONE_PAGING     DEBUGZONE(14)   /* 0x4000 */
#define ZONE_ENTRY      (DEBUGZONE(15)&&!pCurThread->bDbgCnt)   /* 0x8000 */

#pragma warning(disable:4115) // C4115: named type definition in parentheses, re-enable below

/* Kernel Debugger interfaces */
extern BOOL (*g_pKdInit)(struct DEBUGGER_DATA *, void *);
extern BOOL (*KDSanitize)(BYTE* pbClean, VOID* pvAddrMem, ULONG nSize, BOOL fAlwaysCopy);
extern BOOL (*KDIgnoreIllegalInstruction)(DWORD);
extern void (*KDReboot)(BOOL);

/* HDSTUB Interfaces */
extern BOOL (*g_pHdInit) (struct KDBG_INIT *);
extern BOOL (*HDException) (PEXCEPTION_RECORD, CONTEXT*, BOOLEAN);
extern void (*HDPageIn) (DWORD, BOOL);
extern void (*HDPageOut) (DWORD, DWORD);
extern void (*HDModLoad) (DWORD, BOOL);
extern void (*HDModUnload) (DWORD, BOOL);
extern BOOL (*HDIsDataBreakpoint) (DWORD, BOOL);
extern void (*HDStopAllCpuCB) (void);
extern void *pvHDNotifyExdi;
extern BOOL (*HDConnectClient) (BOOL (*)(struct DEBUGGER_DATA *, void *), void *);

#pragma warning(default:4115) // C4115: named type definition in parentheses

#define HD_NOTIFY_MARGIN    20

/* OsAxs Interfaces */
extern BOOL (*g_pOsAxsT0Init) (struct DEBUGGER_DATA *, void *);
extern BOOL (*g_pOsAxsT1Init) (struct DEBUGGER_DATA *, void *);

BOOLEAN NKDispatchException(PTHREAD pth, PEXCEPTION_RECORD ExceptionRecord, PCONTEXT ContextRecord);
ULONG NKGetThreadCallStack (PTHREAD pth, ULONG dwMaxFrames, LPVOID lpFrames, DWORD dwFlags, DWORD dwSkip, PCONTEXT pCtx);


// macros to deal with stack reservation
#define STKMSK              (STACK_ALIGN-1)        // mask to get stack aligned
#define ALIGNSTK(x)         (((x)+STKMSK)&~STKMSK)  // minimum needed to keep stack aligned
#define IsStkAligned(x)     (!((x) & STKMSK))       // if x is aligned properly for stack address


// # of bytes needed for PRETLS block
#define SIZE_PRETLS     ALIGNSTK(PRETLS_RESERVED * sizeof(DWORD))

//
//  Useful
//
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif // !ARRAY_SIZE
#define LAST_ELEMENT(x) (&x[ARRAY_SIZE(x)-1])
#define CCHSIZEOF(sz)   (sizeof(sz)/sizeof(TCHAR))

#define HOST_TRANSCFG_NUM_REGKEYS 8 // Number of keys to preload from the desktop

// Force a reschedule when KCall returns.
#define SetReschedule(ppcb)     ((ppcb)->dwKCRes = 1)
#define GetReschedule(ppcb)     ((ppcb)->dwKCRes != 0)

#ifndef COREDLL
#define INVALID_PRIO 256
#endif


#ifdef x86
#define NCRPTR(pStk,cbStk)          ((NK_PCR*)((ulong)pStk + cbStk - sizeof(NK_PCR)))
extern FARPROC g_pCoredllFPEmul;
extern FARPROC g_pKCoredllFPEmul;
void SetFPEmul (PTHREAD pth, DWORD dwMode);
#define CONTEXT_SEH                 CONTEXT_FULL
#else
#define CONTEXT_SEH                 (CONTEXT_CONTROL|CONTEXT_INTEGER)
#endif

#define TLSPTR(pStk,cbStk)          ((LPDWORD)((ulong)pStk + cbStk - (TLS_MINIMUM_AVAILABLE*4)))
#define INITIAL_SP(pStk, cbStk)     ((LPBYTE) TLSPTR (pStk, cbStk) - SECURESTK_RESERVE)


typedef void (* PFN_LogPageIn) (ULONG uAddr, BOOL bWrite);
extern PFN_LogPageIn pfnNKLogPageIn;

// the real tick is dependent on whether variable tick scheduling is enabled.
#define GETCURRTICK()   ((DWORD) (g_pOemGlobal->pfnUpdateReschedTime? OEMGetTickCount () : CurMSec))

extern BOOL fDisableNoFault;

#define IsNoFaultSet(lptls)      (!fDisableNoFault && ((lptls)[TLSSLOT_KERNEL] & TLSKERN_NOFAULT))
#define IsNoFaultMsgSet(lptls)   (!fDisableNoFault && (((lptls)[TLSSLOT_KERNEL] & (TLSKERN_NOFAULT | TLSKERN_NOFAULTMSG)) == (TLSKERN_NOFAULT | TLSKERN_NOFAULTMSG)))

#include "heap.h"

//
// Page Table Entry from OEMAddressTable
//
struct _ADDRMAP {
    DWORD dwVA;
    DWORD dwPA;
    DWORD dwSize;
};


#include "celognk.h"


#ifdef  IN_KERNEL   // if we are in the kernel

#ifndef SHIP_BUILD  // if not SHIP_BUILD
#ifdef  DEBUG       // and is DEBUG

#ifdef  DBGCHK      // if this is already defined
#undef  DBGCHK      // then undef it
#endif  // DBGCHK

#define DBGCHK(module,exp) \
   ((void)((exp)?1:(          \
       NKDbgPrintfW ( TEXT("%s: DEBUGCHK failed in file %s at line %d \r\n"), \
                 (LPWSTR)module, TEXT(__FILE__) ,__LINE__ ),    \
       ((InDebugger)?1:(DebugBreak(),0)), \
       0  \
   )))


#endif  // DEBUG
#endif  // SHIP_BUILD

#endif  // IN_KERNEL


//
// From corecrt.h
//
#ifndef DEFAULT_SECURITY_COOKIE
#define DEFAULT_SECURITY_COOKIE 0xA205B064
#endif

//
// friendly exception names
//
#define IDSTR_INVALID_EXCEPTION         "<Invalid>"

#define IDSTR_RESCHEDULE                "Reschedule"
#define IDSTR_UNDEF_INSTR               "Undefined Instruction"
#define IDSTR_SWI_INSTR                 "SWI"
#define IDSTR_PREFETCH_ABORT            "Prefetch Abort"
#define IDSTR_DATA_ABORT                "Data Abort"
#define IDSTR_IRQ                       "IRQ"
#define IDSTR_DIVIDE_BY_ZERO            "Divide By Zero"
#define IDSTR_ILLEGAL_INSTRUCTION       "Illegal Instruction"
#define IDSTR_ACCESS_VIOLATION          "Access Violation"
#define IDSTR_ALIGNMENT_ERROR           "Alignment Error"
#define IDSTR_FPU_EXCEPTION             "FPU Exception"
#define IDSTR_INTEGER_OVERFLOW          "Integer Overflow"
#define IDSTR_HARDWARE_BREAK            "Hardware Break"

#define IDSTR_PROTECTION_FAULT          IDSTR_ACCESS_VIOLATION
#define IDSTR_PAGE_FAULT                IDSTR_ACCESS_VIOLATION

LPCSTR GetExceptionString (DWORD dwExcpId);

//
// Default page out trigger and level settings
//
#define PAGEOUT_LOW  ((1*1024*1024) / VM_PAGE_SIZE) // 1MB (in pages)
#define PAGEOUT_HIGH ((3*1024*1024) / VM_PAGE_SIZE) // 3MB (in pages)

//
// internal only CPU State, MUST NOT CONFLICT WITH CE_PROCESSOR_STATE_XXX
//
#define PROCESSOR_STATE_POWERING_OFF        0x1001
#define PROCESSOR_STATE_READY_TO_POWER_OFF  0x1002
#define PROCESSOR_STATE_POWERING_ON         0x1003

//
// Kernel object types
//
#define KERNEL_EVENT            0
#define KERNEL_MUTEX            1
#define KERNEL_SEMAPHORE        2
#define KERNEL_FSMAP            3
#define KERNEL_MSGQ             4
#define KERNEL_WDOG             5
#define KERNEL_MAXOBJECT        KERNEL_WDOG

#ifdef KERN_CORE
#include <psystoken.h>
#endif

#endif

