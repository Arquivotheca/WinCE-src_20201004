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

/** Address translation tables */

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

extern struct KDataStruct *g_pKData;

#define dwCurThId       ((DWORD) g_pKData->ahSys[SH_CURTHREAD])
#define dwActvProcId    ((DWORD) g_pKData->ahSys[SH_CURPROC])
#define pCurThread      (g_pKData->pCurThd)
#define pActvProc       (g_pKData->pCurPrc)
#define pVMProc         (g_pKData->pVMPrc)
#define ReschedFlag     (g_pKData->bResched)
#define KCResched       (g_pKData->dwKCRes)
#define PowerOffFlag    (g_pKData->bPowerOff)
#define ProfileFlag     (g_pKData->bProfileOn)
#define IntrEvents      (g_pKData->alpeIntrEvents)
#define KPlpvTls        (g_pKData->lpvTls)
#define KInfoTable      (g_pKData->aInfo)
#define DIRECT_RETURN   (g_pKData->pAPIReturn)
#define InDebugger      (g_pKData->dwInDebugger)
#define g_CurFPUOwner   (g_pKData->pCurFPUOwner)
#define PendEvents1     (g_pKData->aPend1)
#define PendEvents2     (g_pKData->aPend2)
#define MemForPT        (g_pKData->nMemForPT)
#define dwCeLogTLBMiss  (g_pKData->dwTlbMissCnt)


BOOL INTERRUPTS_ENABLE (BOOL fEnable);
BOOL NKReboot (BOOL fClean);

ERRFALSE(SYSHANDLE_OFFSET == offsetof(struct KDataStruct, ahSys));
ERRFALSE(CURTLSPTR_OFFSET == offsetof(struct KDataStruct, lpvTls));
ERRFALSE(KINFO_OFFSET == offsetof(struct KDataStruct, aInfo));

#include "thread.h"

extern RunList_t RunList;
extern void (*MTBFf)(LPVOID, ulong, ulong, ulong, ulong);

extern LPVOID pExitThread;
extern LPVOID pUsrExcpExitThread;
extern LPVOID pKrnExcpExitThread;
extern LPVOID pCaptureDumpFileOnDevice;
extern LPVOID pKCaptureDumpFileOnDevice;

__inline BOOL IsInRange (DWORD x, DWORD start, DWORD end)
{
    return (x >= start) && (x < end);
}


extern DWORD PagedInCount;

#define KERN_TRUST_FULL 2
#define KERN_TRUST_RUN 1

#if 0
#define TRUSTED_API(apiName, retval)    \
                                {   if (KERN_TRUST_FULL != pCurProc->bTrustLevel) { \
                                        ERRORMSG(1, (L"%s failed due to insufficient trust\r\n", apiName)); \
                                        KSetLastError (pCurThread, ERROR_ACCESS_DENIED); \
                                        return (retval); \
                                    }   \
                                }

#define TRUSTED_API_VOID(apiName)   \
                                {   if (KERN_TRUST_FULL != pCurProc->bTrustLevel) { \
                                        ERRORMSG(1, (L"%s failed due to insufficient trust\r\n", apiName)); \
                                        KSetLastError (pCurThread, ERROR_ACCESS_DENIED); \
                                        return; \
                                    }   \
                                }

#endif
#define TRUSTED_API(apiName, retval)    
#define TRUSTED_API_VOID(apiName)       


struct _TOKENLIST {
    struct _TOKENLIST  *pNext;  // next entry
    PTOKENINFO          pTok;   // the token
    PCALLSTACK          pcstk;  // record PSL entries where impersonation occurs, such that
                                // we don't have privilege leaks, and "RevertToSelf" doesn't
                                // go beyond PSL boundary.
};
//
// CleanupTokenList: called on thread exit. cleaning up left-over token list of the thread.
// 
void CleanupTokenList (PCALLSTACK pcstk);

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

PHDATA LockThrdToken (PTHREAD pth);

// SimplePrivilegeCheck - check privilege against the token of current thread
BOOL SimplePrivilegeCheck (DWORD dwPriv);

DWORD GetContiguousPages(DWORD dwPages, DWORD dwAlignment, DWORD dwFlags);
void GuardCommit(ulong addr);
LPVOID GetHelperStack(void);
void FreeHelperStack(LPVOID);
void CloseMappedFileHandles();
void FreeIntrFromEvent(PEVENT lpe);
void CreateNewProc(ulong nextfunc, ulong param);
void KernelInit2(void);
void AlarmThread(LPVOID param);
DWORD WINAPI RunApps(LPVOID param);
extern LPCWSTR FindModuleNameAndOffset (DWORD dwPC, LPDWORD pdwOffset);
#if x86
void InitNPXHPHandler(LPVOID);
void InitializeEmx87(void);
void InitializeFPU(void);
extern DWORD dwFPType;
extern PVOID g_pCoredllFPEmul;
extern PVOID g_pKCoredllFPEmul;
void SetFPEmul (PTHREAD pth, DWORD dwMode);
#endif

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


//
// machine dependent functions for exception handling
//
#ifdef DEBUG
int MDDbgPrintExceptionInfo (PEXCEPTION_RECORD pExr, PCALLSTACK pcstk);
#endif

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
BOOL SetThreadBasePrio(PTHREAD pth, DWORD nPriority);
PTHREAD EventModIntr(PEVENT lpe, DWORD type);
VOID MakeRunIfNeeded(PTHREAD pth);

void DumpDwords (const DWORD *pdw, int len);

extern void (*TBFf)(LPVOID, ulong);
extern void (*KTBFf)(LPVOID, ulong);
extern void (*MTBFf)(LPVOID, ulong, ulong, ulong, ulong);
extern void (*pSignalStarted)(DWORD);
extern DWORD (WINAPI *pfnKrnGetHeapSnapshot) (THSNAP *pSnap);
extern DWORD (WINAPI *pfnUsrGetHeapSnapshot) (THSNAP *pSnap);


void KernelFindMemory(void);


BOOL FindROMFile(DWORD dwType, DWORD dwFileIndex, LPVOID* ppAddr, DWORD*  pLen);
extern ROMChain_t *ROMChain;

DWORD CECompress(LPBYTE  lpbSrc, DWORD cbSrc, LPBYTE lpbDest, DWORD cbDest, WORD wStep, DWORD dwPagesize);
DWORD CEDecompress(LPBYTE  lpbSrc, DWORD cbSrc, LPBYTE  lpbDest, DWORD cbDest, DWORD dwSkip, WORD wStep, DWORD dwPagesize);
DWORD CEDecompressROM(LPBYTE  lpbSrc, DWORD cbSrc, LPBYTE  lpbDest, DWORD cbDest, DWORD dwSkip, WORD wStep, DWORD dwPagesize);
extern BOOL g_fCompressionSupported;

DWORD DecompressROM(LPBYTE BufIn, DWORD InSize, LPBYTE BufOut, DWORD OutSize, DWORD skip);

extern HANDLE hAlarmThreadWakeup;
DWORD ThreadSuspend(PTHREAD hTh, BOOL fLateSuspend);

int ropen(WCHAR *name, int mode);
int rread(int fd, char *buf, int cnt);
int rreadseek(int fd, char *buf, int cnt, int off);
int rlseek(int fd, int off, int mode);
int rclose(int fd);
int rwrite(int fd, char *buf, int cnt);

LPVOID memset(LPVOID dest, int c, unsigned int count);
LPVOID memcpy(LPVOID dest, const void *src, unsigned int count);


#if defined(WIN32_CALL) && !defined(KEEP_SYSCALLS)
#undef InputDebugCharW
int WINAPI InputDebugCharW(void);
#undef OutputDebugStringW
VOID WINAPI OutputDebugStringW(LPCWSTR str);
#undef NKvDbgPrintfW
VOID NKvDbgPrintfW(LPCWSTR lpszFmt, va_list lpParms);
#endif

void DumpStringToSerial(LPCWSTR);
void InitMUILanguages(void);

BOOL ChangeMapFlushing(LPCVOID lpBaseAddress, DWORD dwFlags);
DWORD DoGetModuleFileName (HMODULE hModule, LPWSTR lpFilename, DWORD nSize);
BOOL OwnAnyKernelCs (void);

DWORD OEMNotifyIntrOccurs (DWORD dwSysIntr);

#ifndef SH4
#define NKSetRAMMode NotSupported
#define NKSetStoreQueueBase NotSupported
#endif

#ifdef IN_KERNEL
#define SetNKCallOut(pth)       (KTHRDINFO(pth) |= UTLS_NKCALLOUT)
#define ClearNKCallOut(pth)     (KTHRDINFO(pth) &= ~UTLS_NKCALLOUT)


typedef HANDLE (* PFN_FindResource) (HMODULE hModule, LPCWSTR lpszName, LPCWSTR lpszType);
extern PFN_FindResource g_pfnFindResource;

#undef TakeCritSec
#define TakeCritSec CRITEnter
#undef LeaveCritSec
#define LeaveCritSec CRITLeave
#undef CreateCrit
#define CreateCrit CRITCreate
#undef DeleteCrit
#define DeleteCrit CRITDelete
#undef GetProcAddressA
#define GetProcAddressA (* g_pfnGetProcAddrA)
#undef GetProcAddressW
#define GetProcAddressW (* g_pfnGetProcAddrW)
#undef WaitForMultipleObjects
#define WaitForMultipleObjects NKWaitForMultipleObjects
#undef WaitForSingleObject
#define WaitForSingleObject NKWaitForSingleObject
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

#endif  // IN_KERNEL

/* Kernel zones */

extern DBGPARAM dpCurSettings;

#define ZONE_SCHEDULE   DEBUGZONE(0)    /* 0x0001 */
#define ZONE_MEMORY     DEBUGZONE(1)    /* 0x0002 */
#define ZONE_OBJDISP    DEBUGZONE(2)    /* 0x0004 */
#define ZONE_DEBUGGER   DEBUGZONE(3)    /* 0x0008 */
#define ZONE_NEXTTHREAD DEBUGZONE(4)    /* 0x0010 */
#define ZONE_LOADER1    DEBUGZONE(5)    /* 0x0020 */
#define ZONE_VIRTMEM    DEBUGZONE(6)    /* 0x0040 */
#define ZONE_LOADER2    (DEBUGZONE(7)&&!pCurThread->bDbgCnt)    /* 0x0080 */
#define ZONE_DEBUG      DEBUGZONE(8)    /* 0x0100 */
#define ZONE_MAPFILE    DEBUGZONE(9)    /* 0x0200 */
#define ZONE_PHYSMEM    DEBUGZONE(10)   /* 0x0400 */
#define ZONE_SEH        (DEBUGZONE(11)&&!pCurThread->bDbgCnt)   /* 0x0800 */
#define ZONE_OPENEXE    DEBUGZONE(12)   /* 0x1000 */
#define ZONE_MEMTRACKER DEBUGZONE(13)   /* 0x2000 */
#define ZONE_PAGING     DEBUGZONE(14)   /* 0x4000 */
#define ZONE_ENTRY      (DEBUGZONE(15)&&!pCurThread->bDbgCnt)   /* 0x8000 */

/* Kernel Debugger interfaces */
extern BOOL (*g_pKdInit)( /* interface left undeclared */ );
extern BOOL (*KDSanitize)(BYTE* pbClean, VOID* pvAddrMem, ULONG nSize, BOOL fAlwaysCopy);
extern void (*KDReboot)(BOOL);

/* HDSTUB Interfaces */
extern BOOL (*g_pHdInit) (struct _HDSTUB_INIT *);
extern ULONG (*HDException) (EXCEPTION_RECORD*, CONTEXT*, DWORD);
extern void (*HDPageIn) (DWORD, BOOL);
extern void (*HDModLoad) (DWORD);
extern void (*HDModUnload) (DWORD);
extern void *pvHDNotifyExdi;
extern ULONG *pulHDEventFilter;
extern BOOL (*HDConnectClient) (BOOL (*)(struct _HDSTUB_DATA *, void *), void *);

#define HD_NOTIFY_MARGIN    20

#define HD_EXCEPTION_PHASE_FIRSTCHANCE  0
#define HD_EXCEPTION_PHASE_SECONDCHANCE 1
#define HD_EXCEPTION_PHASE_DUMPONLY     2

/* OsAxs Interfaces */
extern BOOL (*g_pOsAxsT0Init) (struct _HDSTUB_DATA *, void *);
extern BOOL (*g_pOsAxsT1Init) (struct _HDSTUB_DATA *, void *);

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

DWORD WireBuffer(LPBYTE buf, DWORD len);
#define UnWireBuffer(buf,len) (0)

#define PageFreeCount ((long)KInfoTable[KINX_PAGEFREE])

#define HOST_TRANSCFG_NUM_REGKEYS 8 // Number of keys to preload from the desktop
int rRegGet(DWORD hKey, const CHAR *szName, LPDWORD lpdwType, LPBYTE lpbData, LPDWORD lpdwSize);
int rRegOpen(DWORD hKey, const CHAR *szName, LPDWORD lphKey);
int rRegClose(DWORD hKey);

// Force a reschedule when KCall returns.
#define SetReschedule() (KCResched = 1)

#ifndef COREDLL
#define INVALID_PRIO 256
#endif


#ifdef x86
#define NCRPTR(pStk,cbStk)          ((NK_PCR*)((ulong)pStk + cbStk - sizeof(NK_PCR)))
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
       ((InDebugger)?1:(DebugBreak())), \
       0  \
   )))


#endif  // DEBUG
#endif  // SHIP_BUILD

#endif  // IN_KERNEL


//
// From corecrt.h
//
#ifndef DEFAULT_SECURITY_COOKIE
#define DEFAULT_SECURITY_COOKIE 0x0000B064
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




#endif

