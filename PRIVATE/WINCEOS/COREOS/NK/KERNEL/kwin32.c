//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/**     TITLE("Kernel Win32 Handle")
 *++
 *
 *
 * Module Name:
 *
 *    KWin32.c
 *
 * Abstract:
 *
 *  This file contains the definition of the Win32 system API handle.
 *
 *--
 */
#include "kernel.h"
#include "kdstub.h"
#include "hdstub.h"
#include "osaxs.h"
#include <kitlpriv.h>

BOOL KernelIoctl (DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned);





#define SECURE_WORKAROUND
#ifdef SECURE_WORKAROUND
DWORD SC_CallForward (PCALLBACKINFO pcbi, DWORD arg1, DWORD arg2, DWORD arg3, DWORD arg4, DWORD arg5, DWORD arg6, DWORD arg7)
{
    if (!bAllKMode)
        RETAILMSG (1, (L"!!!! Work around Security violation (call forward), pth = %8.8lx, proc = '%s' !!!!\n", pCurThread, pCurProc->lpszProcName));
    return PerformCallBack (pcbi, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
}
#endif 

static BOOL CheckAccessToVM (LPVOID lpvAddress, DWORD cbSize)
{
    // always okay if calling from kernel itself
    if (ProcArray == pCurProc)
        return TRUE;

    // kernel address is invalid
    if ((int) lpvAddress >= 0) {
        
        DWORD dwSlot = ((DWORD) lpvAddress >> VA_SECTION);
        DWORD dwZeroAddr = (DWORD) ZeroPtr (lpvAddress);

        switch (dwSlot) {
            
        case MODULE_SECTION:
        case RESOURCE_SECTION:
        case SHARED_SECTION:
            // resource, shared, and module section are invalid            
            break;
            
        case 0:
            // slot 0 address, map it current process
            dwSlot = pCurProc->dwVMBase >> VA_SECTION;

            // fall through
        default:
            if (dwSlot <= MAX_PROCESSES) {
                // non-trusted app cannot change VM beyond DllLoadBase
                if ((KERN_TRUST_FULL != pCurProc->bTrustLevel)
                    && ((dwZeroAddr + (cbSize? cbSize : 1)) > (DWORD) DllLoadBase)) {
                    break;
                }
                // slot 1-32: check process aky
                if (pCurThread->aky & (1 << (dwSlot-1))) {
                    return TRUE;
                }
            } else {
                // memory mapped area, check alk of 1st MEMBLOCK
                MEMBLOCK *pmb = (*SectionTable[dwSlot])[0];
                if (pmb && (RESERVED_BLOCK != pmb) && (pCurThread->aky & pmb->alk))
                    return TRUE;
            }
            break;
        }
    }
    KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
    return FALSE;
}

static LPVOID EXT_VirtualAlloc (LPVOID lpvAddress, DWORD cbSize, DWORD fdwAllocationType, DWORD fdwProtect)
{
    DWORD dwErr = 0;
    
    // make sure the thread has access to the address
    if (!CheckAccessToVM (lpvAddress, cbSize)) {
        return NULL;
    }

    // MEM_TOP_DOWN is not support when calling from outside kernel
    fdwAllocationType &= ~MEM_TOP_DOWN;
    return DoVirtualAlloc (lpvAddress, cbSize, fdwAllocationType, fdwProtect, 0, 0);
}

static BOOL EXT_VirtualFree (LPVOID lpvAddress, DWORD cbSize, DWORD fdwFreeType)
{
    // only trusted apps can free shared section
    if (IsInSharedSection (lpvAddress)) {
        TRUSTED_API ("CeVirtualFree", FALSE);
        
    // make sure the thread has access to the address
    } else if (!CheckAccessToVM (lpvAddress, cbSize)) {
        return FALSE;
    }

    return SC_VirtualFree (lpvAddress, cbSize, fdwFreeType);
}

static BOOL EXT_VirtualProtect (LPVOID lpvAddress, DWORD cbSize, DWORD fdwNewProtect, PDWORD pfdwOldProtect)
{
    // make sure the thread has access to the address
    if (!CheckAccessToVM (lpvAddress, cbSize)) {
        return FALSE;
    }

    // verify user pointer
    if ((KERN_TRUST_FULL != pCurProc->bTrustLevel) 
        && pfdwOldProtect
        && !SC_MapPtrWithSize (pfdwOldProtect, sizeof (DWORD), hCurProc)) {
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // verify if trying to make ROM R/W
    if ((fdwNewProtect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE)) 
        && IsROM (lpvAddress, cbSize)) {
        KSetLastError (pCurThread, ERROR_ACCESS_DENIED);
        return FALSE;
    }
    
    return SC_VirtualProtect (lpvAddress, cbSize, fdwNewProtect, pfdwOldProtect);
}

static DWORD EXT_VirtualQuery (LPVOID lpvAddress, PMEMORY_BASIC_INFORMATION pmbiBuffer, DWORD cbLength)
{
    if ((KERN_TRUST_FULL != pCurProc->bTrustLevel)
        && !SC_MapPtrWithSize (pmbiBuffer, sizeof (MEMORY_BASIC_INFORMATION), hCurProc)) {
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
        return 0;
    }
    return SC_VirtualQuery (lpvAddress, pmbiBuffer, cbLength);
}

static BOOL SC_GetRealTime (LPSYSTEMTIME lpst)
{
    if (!SC_MapPtrWithSize (lpst, sizeof (SYSTEMTIME), hCurProc))
        return FALSE;
    return OEMGetRealTime (lpst);
}

static BOOL SC_SetRealTime (LPSYSTEMTIME lpst)
{
    BOOL fRet = OEMSetRealTime (lpst);
    // indicate time has changed
    KInfoTable[KINX_TIMECHANGECOUNT] ++;
    return fRet;
}

BOOL NKDeleteStaticMapping (LPVOID pAddr, DWORD cbSize)
{
    // not implemented, always succeed
    return TRUE;
}

static BOOL SC_PageOutModule (HANDLE hModule, DWORD dwFlags);

const PFNVOID Win32Methods[] = {
    (PFNVOID)SC_Nop,
    (PFNVOID)SC_NotSupported,
    (PFNVOID)SC_CreateAPISet,               //  2
    (PFNVOID)EXT_VirtualAlloc,              //  3
    (PFNVOID)EXT_VirtualFree,               //  4
    (PFNVOID)EXT_VirtualProtect,            //  5
    (PFNVOID)EXT_VirtualQuery,              //  6
    (PFNVOID)SC_VirtualCopy,                //  7
    (PFNVOID)0,                             //  8 Was SC_LoadLibraryW
    (PFNVOID)SC_FreeLibrary,                //  9
    (PFNVOID)SC_GetProcAddressW,            // 10
    (PFNVOID)SC_ThreadAttachOrDetach,       // 11 Was SC_ThreadAttachAllDLLs
    (PFNVOID)0,                             // 12 Was SC_ThreadDetachAllDLLs
    (PFNVOID)SC_GetTickCount,               // 13
    (PFNVOID)OutputDebugStringW,            // 14
    (PFNVOID)SC_TlsCall,                    // 15
    (PFNVOID)SC_GetSystemInfo,              // 16
    (PFNVOID)0,                             // 17  Was ropen
    (PFNVOID)0,                             // 18  Was rread
    (PFNVOID)0,                             // 19  Was rwrite
    (PFNVOID)0,                             // 20  Was rlseek
    (PFNVOID)0,                             // 21  Was rclose
    (PFNVOID)SC_RegisterDbgZones,           // 22
    (PFNVOID)NKvDbgPrintfW,                 // 23
    (PFNVOID)SC_ProfileSyscall,             // 24
    (PFNVOID)SC_FindResource,               // 25
    (PFNVOID)SC_LoadResource,               // 26
    (PFNVOID)SC_SizeofResource,             // 27
    (PFNVOID)SC_GetRealTime,                // 28
    (PFNVOID)SC_SetRealTime,                // 29
    (PFNVOID)SC_ProcessDetachAllDLLs,       // 30
    (PFNVOID)SC_ExtractResource,            // 31
    (PFNVOID)SC_GetRomFileInfo,             // 32
    (PFNVOID)SC_GetRomFileBytes,            // 33
    (PFNVOID)SC_CacheRangeFlush,            // 34
    (PFNVOID)SC_Nop,                        // 35
    (PFNVOID)SC_Nop,                        // 36
    (PFNVOID)SC_Nop,                        // 37
    (PFNVOID)SC_GetKPhys,                   // 38
    (PFNVOID)SC_GiveKPhys,                  // 39
    (PFNVOID)SC_SetExceptionHandler,        // 40
    (PFNVOID)SC_Nop,                        // 41
    (PFNVOID)SC_Nop,                        // 42
    (PFNVOID)SC_SetKernelAlarm,             // 43
    (PFNVOID)SC_RefreshKernelAlarm,         // 44
    (PFNVOID)SC_CeGetRandomSeed,            // 45
    (PFNVOID)SC_CloseProcOE,                // 46
    (PFNVOID)SC_SetGwesOOMEvent,            // 47
    (PFNVOID)SC_FSStringCompress,           // 48
    (PFNVOID)SC_FSStringDecompress,         // 49
    (PFNVOID)SC_FSBinaryCompress,           // 50
    (PFNVOID)SC_FSBinaryDecompress,         // 51
    (PFNVOID)SC_CreateEvent,                // 52
    (PFNVOID)SC_CreateProc,                 // 53
    (PFNVOID)SC_CreateThread,               // 54
    (PFNVOID)InputDebugCharW,               // 55
    (PFNVOID)UB_TakeCritSec,                // 56
    (PFNVOID)SC_LeaveCritSec,               // 57
    (PFNVOID)UB_WaitForMultiple,            // 58
    (PFNVOID)SC_MapPtrToProcess,            // 59
    (PFNVOID)SC_MapPtrUnsecure,             // 60
    (PFNVOID)SC_GetProcFromPtr,             // 61
    (PFNVOID)SC_IsBadPtr,                   // 62
    (PFNVOID)SC_GetProcAddrBits,            // 63
    (PFNVOID)SC_GetFSHeapInfo,              // 64
    (PFNVOID)SC_OtherThreadsRunning,        // 65
    (PFNVOID)SC_KillAllOtherThreads,        // 66
    (PFNVOID)SC_GetOwnerProcess,            // 67
    (PFNVOID)SC_GetCallerProcess,           // 68
    (PFNVOID)SC_GetIdleTime,                // 69
    (PFNVOID)SC_SetLowestScheduledPriority, // 70
    (PFNVOID)SC_IsPrimaryThread,            // 71
    (PFNVOID)SC_SetProcPermissions,         // 72
    (PFNVOID)SC_GetCurrentPermissions,      // 73
    (PFNVOID)0,                             // 74
    (PFNVOID)SC_SetDaylightTime,            // 75
    (PFNVOID)SC_SetTimeZoneBias,            // 76
    (PFNVOID)SC_SetCleanRebootFlag,         // 77
    (PFNVOID)SC_CreateCrit,                 // 78
    (PFNVOID)SC_PowerOffSystem,             // 79
    (PFNVOID)SC_CreateMutex,                // 80
    (PFNVOID)SC_SetDbgZone,                 // 81
    (PFNVOID)UB_Sleep,                      // 82
    (PFNVOID)SC_TurnOnProfiling,            // 83
    (PFNVOID)SC_TurnOffProfiling,           // 84
    (PFNVOID)SC_CeGetCurrentTrust,          // 85
    (PFNVOID)SC_CeGetCallerTrust,           // 86
    (PFNVOID)SC_NKTerminateThread,          // 87
    (PFNVOID)SC_SetLastError,               // 88
    (PFNVOID)SC_GetLastError,               // 89
    (PFNVOID)SC_GetProcName,                // 90
    (PFNVOID)SC_TerminateSelf,              // 91
    (PFNVOID)SC_CloseAllHandles,            // 92
    (PFNVOID)SC_SetHandleOwner,             // 93
    (PFNVOID)0,                             // 94 Was SC_LoadDriver
    (PFNVOID)SC_CreateFileMapping,          // 95
    (PFNVOID)SC_UnmapViewOfFile,            // 96
    (PFNVOID)SC_FlushViewOfFile,            // 97
    (PFNVOID)SC_CreateFileForMapping,       // 98
    (PFNVOID)KernelIoctl,                   // 99
    (PFNVOID)SC_GetThreadCallStack,         // 100
    (PFNVOID)SC_Nop,                        // 101
    (PFNVOID)0,                             // 102
    (PFNVOID)SC_UpdateNLSInfo,              // 103
    (PFNVOID)SC_ConnectDebugger,            // 104
    (PFNVOID)SC_InterruptInitialize,        // 105
    (PFNVOID)SC_InterruptDone,              // 106
    (PFNVOID)SC_InterruptDisable,           // 107
    (PFNVOID)SC_SetKMode,                   // 108
    (PFNVOID)SC_SetPowerOffHandler,         // 109
    (PFNVOID)SC_SetGwesPowerHandler,        // 110
    (PFNVOID)SC_SetHardwareWatch,           // 111
    (PFNVOID)SC_QueryAPISetID,              // 112
#ifdef SECURE_WORKAROUND
    (PFNVOID)SC_CallForward,
#else
    (PFNVOID)-1,                            // 113 (PerformCallBack)
#endif
    (PFNVOID)CaptureContext,                // 114 (RaiseException)
    (PFNVOID)SC_GetCallerIndex,             // 115
    (PFNVOID)SC_WaitForDebugEvent,          // 116
    (PFNVOID)SC_ContinueDebugEvent,         // 117
    (PFNVOID)SC_DebugNotify,                // 118
    (PFNVOID)SC_OpenProcess,                // 119
    (PFNVOID)SC_THCreateSnapshot,           // 120
    (PFNVOID)SC_THGrow,                     // 121
    (PFNVOID)SC_NotifyForceCleanboot,       // 122
    (PFNVOID)SC_DumpKCallProfile,           // 123
    (PFNVOID)SC_GetProcessVersion,          // 124
    (PFNVOID)SC_GetModuleFileNameW,         // 125
    (PFNVOID)SC_QueryPerformanceCounter,    // 126
    (PFNVOID)SC_QueryPerformanceFrequency,  // 127
    (PFNVOID)SC_KernExtractIcons,           // 128
    (PFNVOID)SC_ForcePageout,               // 129
    (PFNVOID)SC_GetThreadTimes,             // 130
    (PFNVOID)SC_GetModuleHandleW,           // 131
    (PFNVOID)0,                                         // 132
    (PFNVOID)SC_SetStdioPathW,              // 133
    (PFNVOID)SC_GetStdioPathW,              // 134
    (PFNVOID)SC_ReadRegistryFromOEM,        // 135
    (PFNVOID)SC_WriteRegistryToOEM,         // 136
    (PFNVOID)SC_WriteDebugLED,              // 137
    (PFNVOID)SC_LockPages,                  // 138
    (PFNVOID)SC_UnlockPages,                // 139
    (PFNVOID)NKVirtualSetAttributes,        // 140
#ifdef SH4
    (PFNVOID)SC_SetRAMMode,                 // 141
    (PFNVOID)SC_SetStoreQueueBase,          // 142
#else
    (PFNVOID)0,                             // 141
    (PFNVOID)0,                             // 142
#endif
    (PFNVOID)SC_FlushViewOfFileMaybe,       // 143
    (PFNVOID)SC_GetProcAddressA,            // 144
    (PFNVOID)SC_GetCommandLineW,            // 145
    (PFNVOID)SC_DisableThreadLibraryCalls,  // 146
    (PFNVOID)SC_CreateSemaphore,            // 147
    (PFNVOID)SC_LoadLibraryExW,             // 148
#ifdef SECURE_WORKAROUND
    (PFNVOID)SC_CallForward,
#else
    (PFNVOID)-2,                            // 149 (PerformCallForward)
#endif
    (PFNVOID)SC_CeMapArgumentArray,         // 150
    (PFNVOID)SC_KillThreadIfNeeded,         // 151
    (PFNVOID)SC_ProcGetIndex,               // 152
    (PFNVOID)SC_RegisterGwesHandler,        // 153
    (PFNVOID)SC_GetProfileBaseAddress,      // 154
    (PFNVOID)SC_SetProfilePortAddress,      // 155
    (PFNVOID)SC_CeLogData,                  // 156
    (PFNVOID)SC_CeLogSetZones,              // 157
    (PFNVOID)NULL,                          // 158 (was ModuleJit)
    (PFNVOID)SC_CeSetExtendedPdata,         // 159
    (PFNVOID)SC_VerQueryValueW,             // 160
    (PFNVOID)SC_GetFileVersionInfoSizeW,    // 161
    (PFNVOID)SC_GetFileVersionInfoW,        // 162
    (PFNVOID)SC_CreateLocaleView,           // 163
    (PFNVOID)SC_CeLogReSync,                // 164
    (PFNVOID)SC_LoadIntChainHandler,        // 165
    (PFNVOID)SC_FreeIntChainHandler,        // 166
    (PFNVOID)SC_LoadKernelLibrary,          // 167
    (PFNVOID)SC_AllocPhysMem,               // 168
    (PFNVOID)SC_FreePhysMem,                // 169
    (PFNVOID)SC_KernelLibIoControl,         // 170
    (PFNVOID)SC_OpenEvent,                  // 171
    (PFNVOID)UB_SleepTillTick,              // 172
    (PFNVOID)SC_DuplicateHandle,            // 173
    (PFNVOID)SC_CreateStaticMapping,        // 174
    (PFNVOID)SC_MapCallerPtr,               // 175
    (PFNVOID)SC_MapPtrWithSize,             // 176
    (PFNVOID)SC_LoadStringW,                // 177
    (PFNVOID)SC_QueryInstructionSet,        // 178
    (PFNVOID)SC_CeLogGetZones,              // 179
    (PFNVOID)SC_ProcGetIDFromIndex,         // 180
    (PFNVOID)SC_IsProcessorFeaturePresent,  // 181
    (PFNVOID)SC_DecompressBinaryBlock,      // 182
    (PFNVOID)SC_PageOutModule,              // 183
    (PFNVOID)SC_InterruptMask,              // 184
    (PFNVOID)SC_GetProcModList,             // 185
    (PFNVOID)SC_FreeModFromCurrProc,        // 186
    (PFNVOID)SC_CeVirtualSharedAlloc,       // 187
    (PFNVOID)NKDeleteStaticMapping,         // 188
    (PFNVOID)SC_CreateToken,                // 189
    (PFNVOID)SC_RevertToSelf,               // 190
    (PFNVOID)SC_CeImpersonateCurrProc,      // 191
    (PFNVOID)SC_CeDuplicateToken,           // 192
    (PFNVOID)SC_ConnectHdstub,              // 193 
    (PFNVOID)SC_ConnectOsAxsT0,             // 194 
    (PFNVOID)SC_IsNamedEventSignaled,       // 195
    (PFNVOID)SC_ConnectOsAxsT1,             // 196
};

const CINFO cinfWin32 = {
    "Wn32",
    DISPATCH_I_KPSL,
    0,
    sizeof(Win32Methods)/sizeof(Win32Methods[0]),
    Win32Methods,
};

extern const CINFO cinfThread;
extern const CINFO cinfProc;
extern const CINFO cinfMutex;
extern const CINFO cinfSem;
extern const CINFO cinfEvent;
extern const CINFO cinfAPISet;
extern const CINFO cinfMap;
extern const CINFO cinfToken;
const CINFO CinfFile = { "FILE", DISPATCH_KERNEL, HT_FILE, 0, 0 };
const CINFO CinfFind = { "FIND", DISPATCH_KERNEL, HT_FIND, 0, 0 };
const CINFO CinfDBFile = { "DFIL", DISPATCH_KERNEL, HT_DBFILE, 0, 0 };
const CINFO CinfDBFind = { "DFND", DISPATCH_KERNEL, HT_DBFIND, 0, 0 };
const CINFO CinfSocket = { "SKT", DISPATCH_KERNEL, HT_SOCKET, 0, 0 };
const CINFO CinfWnetEnum = { "ENUM", DISPATCH_KERNEL, HT_WNETENUM, 0, 0 };

extern void InitMemoryPool(void);
extern void ProfInit(void);
extern void HeapInit(void);
extern void SchedInit(void);
extern void ProcInit(void);
extern CRITICAL_SECTION VAcs, RFBcs, PhysCS, LLcs, ModListcs, ODScs, CompCS, MapCS, NameCS, EventCS, MutexCS, SemCS,
            DbgApiCS, PagerCS, WriterCS, MapNameCS, ppfcs, PageOutCS, IntChainCS, DirtyPageCS, WDcs;

extern BOOL fNoDebugger;
extern BOOL fDebuggerLoaded;

#ifdef SHx
extern void MD_CBRtn(void);
#if defined(SH4)
extern void FPUFlushContext(void);
#elif !defined(SH3e)
extern void DSPFlushContext(void);
#if defined(SH3)
extern unsigned int SH3DSP;
#endif
#endif
#elif  MIPS
extern void MD_CBRtn(void);
#if defined(MIPS_HAS_FPU)
extern void FPUFlushContext(void);
#endif
#elif  ARM
extern void MD_CBRtn(void);
extern void DetectVFP(void);
extern void FPUFlushContext(void);
extern DWORD vfpStat;
#elif  x86
extern BOOL (* __abnormal_termination)(VOID);
extern void FPUFlushContext(void);
extern DWORD MD_CBRtn;
extern PTHREAD g_CurFPUOwner;
#endif

BOOL ReadyForStrings;
PPROCESS PowerProc;
FARPROC PowerFunc;
ACCESSKEY PowerKey;
PPROCESS GwesPowerProc;
FARPROC GwesPowerFunc;
ACCESSKEY GwesPowerKey;

extern void MemTrackInit(void);

void  (* lpWriteDebugStringFunc)(unsigned short *str) = OEMWriteDebugString;
int   (* lpReadDebugByteFunc)(void)                   = OEMReadDebugByte;
void  (* lpWriteDebugByteFunc)(BYTE ch)               = OEMWriteDebugByte;

// Ethernet debug functions, pointers set only if platform supports ether debug.
PFN_KITLRegisterDfltClient pKITLRegisterDfltClient;
PFN_KITLInitializeInterrupt pKITLInitializeInterrupt;
PFN_KITLSend pKITLSend;
PFN_KITLRecv pKITLRecv;

DWORD (* pKITLIoCtl) (DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned);


BOOL fSysStarted;

BOOL (*pfnIsDesktopDbgrExist) ();

//------------------------------------------------------------------------------
// Placeholder for when debugger is not present
//------------------------------------------------------------------------------
static BOOL
FakeKDSanitize(
    BYTE* pbClean,
    VOID* pvMem,
    ULONG nSize,
    BOOL  fAlwaysCopy
    )
{
    if (fAlwaysCopy) memcpy(pbClean, pvMem, nSize);
    return FALSE;
}

//------------------------------------------------------------------------------
// Placeholder for when debugger is not present
//------------------------------------------------------------------------------
static void 
FakeKDReboot(
    BOOL fReboot
    ) 
{
    return;
}

/* Kernel Debugger interface pointers */
BOOL  (*g_pKdInit)(KERNDATA* pKernData) = NULL;
BOOL  (*KDSanitize)(BYTE* pbClean, VOID* pvAddrMem, ULONG nSize, BOOL fAlwaysCopy) = FakeKDSanitize;
VOID  (*KDReboot)(BOOL fReboot) = FakeKDReboot;

//------------------------------------------------------------------------------
// This function is called when kdstub gets Terminate Message from the debugger
// It simply reverts back KDSanitize and KDReboot function 
// pointers to FAKE functions.
//------------------------------------------------------------------------------
BOOL
KDCleanup(void)
{
    g_pKdInit       = NULL;
    KDSanitize      = FakeKDSanitize;
    KDReboot        = FakeKDReboot;
    fDebuggerLoaded = FALSE;
    return TRUE;
}

//------------------------------------------------------------------------------
// HwTrap - This was HdstubNotify.  Added here to give OsAccess capability to set
// HW breakpoint before the kernel is booted into memory.
//------------------------------------------------------------------------------

int g_iHwTrapCount=0;

void HwTrap (void)
{
    DebugBreak ();
    g_iHwTrapCount++; // This make this function unique enough to prevent ICF optimization (with DoDebugBreak())
}

void FakeNKHaltSystem (void)
{
    OutputDebugStringW (_T("Halting system\r\n"));
    for (;;)
        ;
}

void (*lpNKHaltSystem)(void) = FakeNKHaltSystem;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

// RestorePrio & KDEnableInt are used by KD to boost thread priority and
// enable and disable interrupts
int g_cInterruptsOff = 0;

static void RestorePrio (SAVED_THREAD_STATE *psvdThread)
{
    // doesn't do KCall profiling here since this is part
    // of the debugger

    if (!psvdThread) return;
    if (!psvdThread->fSaved)
    {
        DEBUGMSG (1, (L"Calling KDEnableInt(TRUE, ...) with invalid psvdThread!!!\r\n"));
        return;
    }    
    
    pCurThread->dwQuantum = psvdThread->dwQuantum;
    psvdThread->fSaved = FALSE;

    SetThreadBasePrio (hCurThread, psvdThread->bBPrio);
}

static void KDEnableInt (BOOL fEnable, SAVED_THREAD_STATE *psvdThread)
{        
    if (!pCurThread)
    {
        DEBUGMSG (1, (L"KDEnableInt with pCurThread == 0!!!\r\n"));
        return;
    }
    
    if (fEnable)
    {
        if (psvdThread)
        {
            if (g_cInterruptsOff < 1)
            {
                DEBUGMSG (1, (L"Calling KDEnableInt(TRUE, ...) without first calling KDEnableInt(FALSE, ...)!!!\r\n"));
            }
            else
            {
                --g_cInterruptsOff;
            }
            KCall ((FARPROC) RestorePrio, psvdThread);
        }
        INTERRUPTS_ENABLE(TRUE); // KCall should turn interrupts on for us, this may not be necessary
    } 
    else
    {

        if (psvdThread)
        {
            INTERRUPTS_ENABLE(FALSE); // Do not use INTERRUPTS_OFF, it will cause problems with MIPIV FP registers
            if (g_cInterruptsOff)
            {
                DEBUGMSG (1, (L"Recursively calling KDEnableInt(FALSE, psvdThread != NULL) %i time(s). This OK if KdStub stumbling on its own BP.\r\n", g_cInterruptsOff));
            }
            psvdThread->bCPrio = (BYTE)GET_CPRIO (pCurThread);
            psvdThread->bBPrio = (BYTE)GET_BPRIO (pCurThread);
            psvdThread->dwQuantum = pCurThread->dwQuantum;
            psvdThread->fSaved = TRUE;

            pCurThread->dwQuantum = 0;
            SET_CPRIO (pCurThread, 0);
            SET_BPRIO (pCurThread, 0);
            ++g_cInterruptsOff;
        } else {
            if (!g_cInterruptsOff) {
                DEBUGMSG (1, (L"Calling KDEnableInt (FALSE, NULL) without previously calling KDEnableInt (FALSE, p)\r\n"));;
            } else {
                INTERRUPTS_ENABLE(FALSE); // Do not use INTERRUPTS_OFF, it will cause problems with MIPIV FP registers
            }
        }
        //INTERRUPTS_ON();      // un-comment this line if we want KD running with interrupt on
    }
}


void kdpInvalidateRange (    PVOID pvAddr,  ULONG ulSize)
{
    InvalidateRange (pvAddr, ulSize);
}


extern BOOL (*pfnOEMIsRom) (LPVOID pvAddr, DWORD cbSize);


BOOL kdpIsROM (LPVOID pvAddr, DWORD cbSize)
{
    BOOL fRet = FALSE;
    if (IsKernelVa (pvAddr))
    {
        fRet = pfnOEMIsRom (pvAddr, cbSize);
    }
    return fRet;
}


extern BOOL g_fForcedPaging;


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_ConnectDebugger(
    LPVOID pInit  // ignored
    ) 
{
    BOOL fRet;

    KERNDATA kd =
    {
        sizeof(KERNDATA),

        // OUT params (pass default values, so that Kd can ignore partially some of them)
        KDSanitize,
        KDReboot,

        // IN params
        pTOC,
        ROMChain,
        ProcArray,
        (HDATA *) &HandleList,
        &KData,
        &VAcs,
        &NullSection,
        &NKSection,
        &hCoreDll,
        KDCleanup,
        KDEnableInt,
        &pCaptureDumpFileOnDevice,
        pfnIsDesktopDbgrExist,
        NKwvsprintfW,
        NKDbgPrintfW,
        KCall,
        DbgVerify,
        kdpInvalidateRange,
        DoVirtualCopy,
        kdpIsROM,
        DBG_CallCheck,
        NULL, // MD_CBRtn
        INTERRUPTS_OFF,
        INTERRUPTS_ENABLE,
        pKDIoControl,        
        pKITLIoCtl,
        GetObjectPtrByType,
        InitializeCriticalSection,
        DeleteCriticalSection,
        EnterCriticalSection,
        LeaveCriticalSection,
        SC_CloseHandle,
        SC_VirtualFree,
        FALSE, // fFPUPresent
        FALSE, // fDSPPresent
        &g_fForcedPaging,
        SC_CacheRangeFlush,
        &g_ulHDEventFilter,
        HwTrap,
        // ...
    };

    TRUSTED_API (L"SC_ConnectDebugger", FALSE);

    DEBUGMSG(ZONE_DEBUGGER, (TEXT("Entering ConnectDebugger\r\n")));

#ifdef SHx
    kd.pMD_CBRtn               = MD_CBRtn;
    kd.p__C_specific_handler   = __C_specific_handler;
#if defined(SH4)
    kd.FPUFlushContext         = FPUFlushContext;
    kd.fFPUPresent             = TRUE;
#elif !defined(SH3e)
    kd.DSPFlushContext         = DSPFlushContext;
#if defined(SH3)
    kd.fDSPPresent             = (1 == SH3DSP);
#endif
#endif

#elif  MIPS
    kd.pInterlockedDecrement   = InterlockedDecrement;
    kd.pInterlockedIncrement   = InterlockedIncrement;
    kd.pMD_CBRtn               = MD_CBRtn;
    kd.p__C_specific_handler   = __C_specific_handler;
#if defined(MIPS_HAS_FPU)
    kd.FPUFlushContext         = FPUFlushContext;
    kd.fFPUPresent             = TRUE;
#endif

#elif  ARM
    kd.pMD_CBRtn               = MD_CBRtn;
    kd.p__C_specific_handler   = __C_specific_handler;
    kd.pInSysCall              = InSysCall;
    kd.FPUFlushContext         = FPUFlushContext;
    kd.fFPUPresent             = (1 == vfpStat);

#elif  x86
    kd.p__abnormal_termination = __abnormal_termination;
    kd.p_except_handler3       = _except_handler3;
    kd.FPUFlushContext         = FPUFlushContext;
    kd.pMD_CBRtn               = (VOID (*)(VOID))&MD_CBRtn;
    kd.ppCurFPUOwner           = &g_CurFPUOwner;
    kd.pdwProcessorFeatures    = &ProcessorFeatures;
    kd.fFPUPresent             = TRUE;
#else
    #pragma message("ERROR: ConnectDebugger is not supported on this CPU!")
    ERRORMSG(1, (TEXT("ConnectDebugger is not supported on this CPU!\r\n")));
    return FALSE;
#endif

    if (!fDebuggerLoaded)
    {
        CALLSTACK *pcstk = NULL, cstk;
        if (ProcArray != pCurProc) {
            SwitchToKernel (pcstk = &cstk);
        }

        EnterCriticalSection (&LLcs);
        if (fRet = (!fNoDebugger && g_pKdInit && HDConnectClient &&
            HDConnectClient ((HDSTUB_CLINIT_FUNC)g_pKdInit, &kd)))
        {
            KDSanitize      = kd.pKdSanitize;
            KDReboot        = kd.pKdReboot;
            HDModLoad ((DWORD)&ProcArray[0]);
            ReadyForStrings = TRUE;
            fDebuggerLoaded = TRUE;
        } else {
            ERRORMSG(1, (TEXT("ConnectDebugger failed\r\n")));
        }
        LeaveCriticalSection(&LLcs);
        if (pcstk) {
            SwitchBack ();
        }
    }
    else
    {
        DEBUGMSG(ZONE_DEBUGGER, (TEXT("  Nk!SC_ConnectDebugger: Debugger is already connected, ignoring connect request.\r\n")));
        fRet = TRUE;
    }

    return fRet;
}


//------------------------------------------------------------------------------
// HDStub Fake interface (Runs when hd.dll is not loaded.)
//------------------------------------------------------------------------------
ULONG 
FakeHDException(
    PEXCEPTION_RECORD ExceptionRecord,
    CONTEXT *ContextRecord,
    BOOLEAN SecondChance
    ) 
{
    return FALSE;
}

static void FakeHDPageIn (DWORD dw, BOOL f)
{
}

static void FakeHDModLoad (DWORD dw)
{
}

static void FakeHDModUnload (DWORD dw)
{
}

//------------------------------------------------------------------------------
// HDStub interface
//------------------------------------------------------------------------------
static BOOL s_fHdConnected = FALSE;
BOOL  (*g_pHdInit) (HDSTUB_INIT *) = NULL;
ULONG (*HDException) (EXCEPTION_RECORD *, CONTEXT *, BOOLEAN) = FakeHDException;
void  (*HDPageIn) (DWORD, BOOL) = FakeHDPageIn;
void  (*HDModLoad) (DWORD) = FakeHDModLoad;
void  (*HDModUnload) (DWORD) = FakeHDModUnload;
void  *pvHDNotifyExdi = (void *)HwTrap;
DWORD  *pdwHDTaintedModuleCount = NULL;
BOOL  (*HDConnectClient) (HDSTUB_CLINIT_FUNC, void *) = NULL;
HDSTUB_EVENT *g_pHdEvent = NULL;
// Hardware event filter.
ULONG g_ulHDEventFilter = 0;


//------------------------------------------------------------------------------
// OsAccess synchronization block - Aligned on a 64byte boundary.  Makes it easier
// to find.
//------------------------------------------------------------------------------
_declspec(align(64)) OSAXS_KERN_POINTERS_2 OsAxsDataBlock_2 =
{
    SIGNATURE_OSAXS_KERN_POINTERS_2,
    sizeof (OsAxsDataBlock_2),
    (DWORD)&KData,
    (DWORD)&ProcArray[0],
    (DWORD)&pvHDNotifyExdi,
    (DWORD)&g_pHdEvent,
    (DWORD)&pdwHDTaintedModuleCount,
    (DWORD)&g_ulHDEventFilter,
    (DWORD)&MD_CBRtn,                   // Needs to be dereferenced manually
    (DWORD)&SystemAPISets[0],
    (DWORD)&MemoryInfo,
    (DWORD)&pTOC,
    FALSE
};

// Compatibility with older versions of PB
_declspec(align(64)) OSAXS_KERN_POINTERS_1 OsAxsDataBlock =
{
    SIGNATURE_OSAXS_KERN_POINTERS,
    sizeof(OsAxsDataBlock),
    VERSION_OSAXS_KERN_POINTERS_1,
    0,
    (DWORD)&KData,
    (DWORD)&ProcArray[0],
    (DWORD)&pvHDNotifyExdi,
    (DWORD)&g_pHdEvent,
    (DWORD)&pdwHDTaintedModuleCount,
    (DWORD)&g_ulHDEventFilter,
    (DWORD)&MD_CBRtn,                   // Needs to be dereferenced manually
    (DWORD)&SystemAPISets[0],
    (DWORD)&MemoryInfo,
    (DWORD)&pTOC
};


void HDCleanup ()
{
    g_pHdInit = NULL;
    HDException = FakeHDException;
    HDPageIn = FakeHDPageIn;
    HDModLoad = FakeHDModLoad;
    HDModUnload = FakeHDModUnload;
    pvHDNotifyExdi = NULL;
    pdwHDTaintedModuleCount = NULL;
    HDConnectClient = NULL;
    g_pHdEvent = NULL;
    s_fHdConnected = FALSE;
}

BOOL SC_ConnectHdstub (void *pvUnused)
{
    BOOL fRet;
    HDSTUB_INIT hd = {
        sizeof(HDSTUB_INIT),
        InitializeCriticalSection,
        DeleteCriticalSection,
        EnterCriticalSection,
        LeaveCriticalSection,
        INTERRUPTS_ENABLE,
#ifdef MIPS
        InterlockedDecrement,
        InterlockedIncrement,
#endif
#ifdef  ARM
        InSysCall,
#endif        
        HDCleanup,
        pKITLIoCtl,
        NKwvsprintfW,
        &KData,
        &g_ulHDEventFilter,
        HwTrap,
        SC_CacheRangeFlush,
    };

    TRUSTED_API (L"SC_ConnectHdstub", FALSE);

    if (!s_fHdConnected)
    {
        CALLSTACK *pcstk = NULL, cstk;
        if (ProcArray != pCurProc) {
            SwitchToKernel (pcstk = &cstk);
        }

        fRet = g_pHdInit && g_pHdInit (&hd);

        if (pcstk) {
            SwitchBack ();
        }

        if (fRet) {
            HDException = hd.pfnException;
            HDPageIn = hd.pfnVmPageIn;
            HDModLoad = hd.pfnModLoad;
            HDModUnload = hd.pfnModUnload;
            pdwHDTaintedModuleCount = hd.pdwTaintedModuleCount;
            g_pHdEvent = hd.pEvent;
            HDConnectClient = hd.pfnConnectClient;

            s_fHdConnected = TRUE;
            
            // Update the signature block so that OsAccess on host side can find it.
            OsAxsDataBlock_2.fHdstubLoaded = TRUE;
        }
    }
    else
    {
        DEBUGMSG(ZONE_DEBUGGER, (TEXT("  Nk!SC_ConnectHdstub: HD is already connected, ignoring connect request\r\n")));
        fRet = TRUE;
    }
    return fRet;
}

//------------------------------------------------------------------------------
// OsAxs common Interface
//------------------------------------------------------------------------------

BOOL s_fOsAxsT0Connected = FALSE;
BOOL (*g_pOsAxsT0Init) (struct _HDSTUB_DATA *, void *);
BOOL s_fOsAxsT1Connected = FALSE;

void OsaxsT0Cleanup ()
{
    g_pOsAxsT0Init = NULL;
    s_fOsAxsT0Connected = FALSE;
}

static BOOL ConnectOsAxs(BOOL (*pInitFunc)(struct _HDSTUB_DATA *, void *))
{
    BOOL fRet;

    OSAXS_DATA osaxs = {
        sizeof (OSAXS_DATA),
        &KData,
        &VAcs,
        &NullSection,
        &NKSection,
        &hCoreDll,
        &pCaptureDumpFileOnDevice,
        SystemAPISets,
        ProcArray,
        pTOC,
        LogPtr,
        pKDIoControl,
        NKKernelLibIoControl,
        GetObjectPtrByType,
        SC_CacheRangeFlush,
        kdpIsROM,
        DbgVerify,
        DoThreadGetContext,
        NKGetThreadCallStack,
        SC_EventModify,
        SC_GetSystemInfo,
        SC_GetLastError,
        SC_SetLastError,
        INTERRUPTS_ENABLE,
#if defined(MIPS)
        InterlockedDecrement,
        InterlockedIncrement,
#endif
#ifdef x86
        _except_handler3,
        __abnormal_termination,
#else
        __C_specific_handler,
#endif
        MD_CBRtn,
        NULL,                   // OEMGetRegDesc,
        NULL,                   // OEMReadRegs,
        NULL,                   // OEMWriteRegs

        KCall,
        NULL,
        NULL,
        NULL,
        NULL,
        pKITLIoCtl,
        NKwvsprintfW
    };

    CALLSTACK *pcstk = NULL, cstk;
    if (ProcArray != pCurProc) {
        SwitchToKernel (pcstk = &cstk);
    }


#if defined (SHx) && !defined (SH4) && !defined (SH3e)
        osaxs.DSPFlushContext = DSPFlushContext;
#endif

#if defined (SH4) || defined (ARM) || defined (MIPS_HAS_FPU) || defined (x86)
        osaxs.FPUFlushContext = FPUFlushContext;
#endif

#ifdef x86
        osaxs.ppCurFPUOwner = &g_CurFPUOwner;
        osaxs.pdwProcessorFeatures = &ProcessorFeatures;
#endif

    fRet = pInitFunc && HDConnectClient &&
        HDConnectClient ((HDSTUB_CLINIT_FUNC)pInitFunc, &osaxs);

    if (pcstk) {
        SwitchBack ();
    }

    return fRet;
}


BOOL SC_ConnectOsAxsT0(void *pvUnused)
{
    BOOL fRet = FALSE;

    TRUSTED_API (L"SC_ConnectOsAxsT0", FALSE);

    if (!s_fOsAxsT0Connected)
    {
        fRet = ConnectOsAxs(g_pOsAxsT0Init);
        if (fRet)
        {
            s_fOsAxsT0Connected = TRUE;
        }
    }
    else
    {
        DEBUGMSG(ZONE_DEBUGGER, (TEXT("  Nk!SC_ConnectOsAxsT0: OsAxsT0 is already attached, ignoring connect request.\r\n")));
        fRet = TRUE;
    }

    return fRet;
}


//------------------------------------------------------------------------------
// OsAxsT1 Interface
//------------------------------------------------------------------------------

BOOL (*g_pOsAxsT1Init)(struct _HDSTUB_DATA *, void *);

void OsaxsT1Cleanup()
{
    g_pOsAxsT1Init = NULL;
    s_fOsAxsT1Connected = FALSE;
}


BOOL SC_ConnectOsAxsT1(void *pvUnused)
{
    BOOL fRet = FALSE;

    TRUSTED_API (L"SC_ConnectOsAxsT1", FALSE);

    if (!s_fOsAxsT1Connected)
    {
        CALLSTACK *pcstk = NULL, cstk;
        if (ProcArray != pCurProc) {
            SwitchToKernel (pcstk = &cstk);
        }

        fRet = ConnectOsAxs(g_pOsAxsT1Init);

        if (pcstk) {
            SwitchBack ();
        }

        if (fRet)
        {
            s_fOsAxsT1Connected = TRUE;
        }
    }
    else
    {
        DEBUGMSG(ZONE_DEBUGGER, (TEXT("  Nk!SC_ConnectOsAxsT1: OsAxsT1 is already attached, ignoring connect request.\r\n")));
        fRet = TRUE;
    }
    return fRet;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
KernelInit(void) 
{
#ifdef DEBUG
    lpWriteDebugStringFunc(TEXT("Windows CE KernelInit\r\n"));
#endif
    /* setup the well known system API sets & API types */
    SystemAPISets[SH_WIN32] = &cinfWin32;
    SystemAPISets[SH_CURTHREAD] = &cinfThread;
    SystemAPISets[SH_CURPROC] = &cinfProc;
    SystemAPISets[SH_CURTOKEN] = &cinfToken;
    SystemAPISets[HT_EVENT] = &cinfEvent;
    SystemAPISets[HT_MUTEX] = &cinfMutex;
    SystemAPISets[HT_SEMAPHORE] = &cinfSem;
    SystemAPISets[HT_APISET] = &cinfAPISet;
    SystemAPISets[HT_FILE] = &CinfFile;
    SystemAPISets[HT_FIND] = &CinfFind;
    SystemAPISets[HT_DBFILE] = &CinfDBFile;
    SystemAPISets[HT_DBFIND] = &CinfDBFind;
    SystemAPISets[HT_SOCKET] = &CinfSocket;
    SystemAPISets[HT_FSMAP] = &cinfMap;
    SystemAPISets[HT_WNETENUM] = &CinfWnetEnum;
    KInfoTable[KINX_APISETS] = (DWORD)SystemAPISets;
    HeapInit();
    InitMemoryPool();
    ProcInit();
    SchedInit();
#ifdef DEBUG
    lpWriteDebugStringFunc(TEXT("Scheduling the first thread.\r\n"));
#endif
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
KernelInit2(void) 
{
    HANDLE hKdLib;
    ULONG  nName;
    LPCWSTR rgpszKdLibNames[] =
    {
        TEXT("kd.dll")
        // more can be added in order of descending preference
    };

    fSysStarted = TRUE;

    DEBUGMSG(1, (TEXT("KernelInit2: pCurThread=%8.8lx hCurThread=%8.8lx hCurProc=%8.8lx, KernelInit = %8.8lx\r\n"),
            pCurThread, hCurThread, hCurProc, KernelInit));

#ifdef  ARM
    // Determine if ARM VFP hardware present (Will set vfpStat = 1)
    DetectVFP ();
#endif    

    /* Bring up HdStub if present */
    if (LoadKernelLibrary (TEXT ("hd.dll"))) {
        if (g_pHdInit) {
            DEBUGMSG(ZONE_DEBUGGER, (TEXT("Hdstub loaded\r\n")));
            SC_ConnectHdstub ((void*)g_pHdInit);
        }
    }

    /* Bring up OsAxsT0 if present */
    if (LoadKernelLibrary (TEXT ("osaxst0.dll"))) {
        if (g_pOsAxsT0Init) {
            DEBUGMSG(ZONE_DEBUGGER, (TEXT("OsaxsT0 loaded\r\n")));
            SC_ConnectOsAxsT0 (NULL);
        }
    }

    /* Bring up OsAxsT1 if present */
    if (LoadKernelLibrary (TEXT ("osaxst1.dll"))) {
        if (g_pOsAxsT1Init) {
            DEBUGMSG (ZONE_DEBUGGER, (TEXT("OsAxsT1 loaded\r\n")));
            SC_ConnectOsAxsT1 (NULL);
        }
    }

    /* initialize kernel debugger subsystem if present. */
    for (nName = 0; nName < (sizeof(rgpszKdLibNames)/sizeof(LPCWSTR)); nName++) {
        if (hKdLib = LoadKernelLibrary(rgpszKdLibNames[nName])) {
            DEBUGMSG(ZONE_DEBUGGER, (TEXT("Debugger '%s' loaded\r\n"), rgpszKdLibNames[nName]));
            if (g_pKdInit) {
                SC_ConnectDebugger((VOID*)g_pKdInit);
            } else {
                ERRORMSG(1, (TEXT("'%s' is not a debugger DLL\r\n"), rgpszKdLibNames[nName]));
            }
        }
    }

    InitializeCriticalSection(&ODScs);
    InitializeCriticalSection(&CompCS);
    InitializeCriticalSection(&PhysCS);
    InitializeCriticalSection(&VAcs);
    InitializeCriticalSection(&LLcs);
    InitializeCriticalSection(&ModListcs);
    InitializeCriticalSection(&RFBcs);
    InitializeCriticalSection(&MapCS);
    InitializeCriticalSection(&NameCS);
    InitializeCriticalSection(&WDcs);
    InitializeCriticalSection(&EventCS);
    InitializeCriticalSection(&MutexCS);
    InitializeCriticalSection(&SemCS);
    InitializeCriticalSection(&MapNameCS);
    InitializeCriticalSection(&DbgApiCS);
    InitializeCriticalSection(&PagerCS);
    InitializeCriticalSection(&PageOutCS);
    InitializeCriticalSection(&WriterCS);
    InitializeCriticalSection(&IntChainCS);
    InitializeCriticalSection(&DirtyPageCS);


    // create the "Module section"
    CreateSection ((LPVOID) MODULE_BASE_ADDRESS, FALSE);
    CreateSection ((LPVOID) SHARED_BASE_ADDRESS, FALSE);
    CreateSection ((LPVOID) RESOURCE_BASE_ADDRESS, FALSE);
    DEBUGCHK (NULL_SECTION != SectionTable[MODULE_SECTION]);
    DEBUGCHK (NULL_SECTION != SectionTable[SHARED_SECTION]);
    DEBUGCHK (NULL_SECTION != SectionTable[RESOURCE_SECTION]);

    // Load code-coverage buffer allocator from ROM if present
    LoadKernelLibrary(TEXT("kcover.dll"));

    // initialized profiler
    ProfInit();
    
    // Auto-load logger from ROM if present
    LoadKernelLibrary(TEXT("CeLog.dll"));
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void SC_SetExceptionHandler(
    PEXCEPTION_ROUTINE per
    ) 
{
    TRUSTED_API_VOID (L"SC_SetExceptionHandler");

    pCurProc->pfnEH = per;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_Nop() 
{
    return TRUE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_NotSupported() 
{
    KSetLastError(pCurThread,ERROR_NOT_SUPPORTED);
    return FALSE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_SetHardwareWatch(
    LPVOID vAddr,
    DWORD flags
    )
{
    LPVOID pAddr = 0;
    extern SetCPUHardwareWatch(LPVOID,DWORD);

    TRUSTED_API ("SC_SetHardwareWatch", FALSE);
    
#ifdef MIPS
    vAddr = MapPtrProc(vAddr, pCurProc);
    pAddr = VerifyAccess(vAddr, VERIFY_KERNEL_OK|VERIFY_READ_FLAG, 0xffffffff);
    SetCPUHardwareWatch(pAddr, flags);
    return TRUE;
#elif x86
    // Make sure this runs without preemption
    vAddr = MapPtrProc(vAddr, pCurProc);
    SetCPUHardwareWatch(vAddr, flags);
    return TRUE;
#else
    return FALSE;
#endif
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_SetKMode(
    BOOL newMode
    )
{
    PCALLSTACK pcstk;
    BOOL bRet;
    // Retrive the previous mode from the callstack structure because the thread is now
    // in kernel mode because it is running here. Then update the callstack to switch to
    // the desired mode when we return.
    if (bAllKMode)
        bRet = 1;
    else {
        TRUSTED_API (L"SC_SetKMode", FALSE);
        
        pcstk = pCurThread->pcstkTop;
        bRet = !((DWORD)pcstk->dwPrcInfo & CST_MODE_FROM_USER);

        // can't set kmode to false if our base mode is kmode
        if (!pcstk->pcstkNext || (pcstk->pcstkNext->dwPrcInfo & CST_MODE_TO_USER)) {
            if (newMode)
                pcstk->dwPrcInfo &= ~CST_MODE_FROM_USER;     // to KMode --> clear the bit
            else
                pcstk->dwPrcInfo |= CST_MODE_FROM_USER;      // to UMode --> set the bit
        } else {
            DEBUGCHK (bRet);    // we must be in KMode here
        }
    }
    return bRet;
}

void UpdateCallerInfo (PTHREAD pth, BOOL fInKMode);
void SwitchBack();
void InitClock(void);

PPROCESS SwitchToProcPtr(
	CALLSTACK*	pcstk,
	PPROCESS    pprc
	)
{
	pcstk->pprcLast = pCurProc;
	pcstk->akyLast = CurAKey;
	pcstk->retAddr = 0;
	pcstk->dwPrevSP = 0;
	pcstk->pcstkNext = pCurThread->pcstkTop;
	pcstk->dwPrcInfo = 0;
	pCurThread->pcstkTop = pcstk;
	pCurThread->pProc = pprc;
	AddAccess(&pCurThread->aky, pprc->aky);
	SetCPUASID(pCurThread);
	UpdateCallerInfo (pCurThread, TRUE);

	return pcstk->pprcLast;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_SetPowerOffHandler(
    FARPROC pfn
    )
{
    TRUSTED_API (L"SC_SetPowerOffHandler", FALSE);
    
    PowerKey = GETCURKEY();
    PowerFunc = pfn;
    PowerProc = pCurProc;
    return TRUE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_SetGwesPowerHandler(
    FARPROC pfn
    )
{
    TRUSTED_API (L"SC_SetGwesPowerHandler", FALSE);
    GwesPowerKey = GETCURKEY();
    GwesPowerFunc = pfn;
    GwesPowerProc = pCurProc;
    return TRUE;
}



//------------------------------------------------------------------------------
//
//  Invoke power on/off handler in the registered process space. Note that the
//  current thread is used to set the access key and process address space information
//  but that the thread is not actually scheduled in this state. Since a reschedule
//  will always occur after a power off, it is not necessary to call SetCPUASID to
//  restore the address space information when we are done.
//
//------------------------------------------------------------------------------
void CallPowerProc(
        PPROCESS    pprc,
        FARPROC     pfn,
        BOOL        bOff,
        LPCWSTR     pszMsg
    )
{

    if (pfn) {
        CALLSTACK cstk;
        RETAILMSG(1, (pszMsg));

        SwitchToProcPtr (&cstk, pprc);
        (*pfn)(bOff);       /* set power off state */
        SwitchBack ();
    }
}

void CallOEMPowerOff (void)
{
    INTERRUPTS_OFF ();
#if SHx
    OEMCacheRangeFlush (0, 0, CACHE_SYNC_DISCARD | CACHE_SYNC_INSTRUCTIONS);
#endif
    OEMPowerOff ();
#if SHx
    OEMCacheRangeFlush (0, 0, CACHE_SYNC_DISCARD | CACHE_SYNC_INSTRUCTIONS);
#endif
    InitClock ();
    INTERRUPTS_ON ();
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void SC_PowerOffSystem(void) 
{
    extern DWORD bLastIdle;
    extern HANDLE hEvtPwrHndlr;
    DWORD  bPrio = GET_BPRIO(pCurThread);
    DWORD  dwQuantum = pCurThread->dwQuantum;

    TRUSTED_API_VOID (L"SC_PowerOffSystem");
    bLastIdle = 0;
    
    SC_CeSetThreadPriority (hCurThread, 0);
    SC_CeSetThreadQuantum (hCurThread, 0);

    // this retail message is required to make sure no other thread holds the 
    // CS for debug transport. 
    // NOTE: must be done after we change the priority and before setting 
    //       PowerOffFlag
    RETAILMSG(1, (TEXT("Powering Off system:\r\n")));

    PowerOffFlag = TRUE;

    SetEvent (hEvtPwrHndlr);

    /* Tell GWES and FileSys that we are turning power off */
    CallPowerProc (GwesPowerProc, GwesPowerFunc, TRUE, TEXT("  Calling GWES power proc.\r\n"));
    CallPowerProc (PowerProc, PowerFunc, TRUE, TEXT("  Calling device manager power proc.\r\n"));

    RETAILMSG(1, (TEXT("  Calling OEMPowerOff...\r\n")));

    KCall ((FARPROC) CallOEMPowerOff);

    KInfoTable[KINX_TIMECHANGECOUNT] ++;    // indicate time changed since tick won't run while powered off
    RETAILMSG(1, (TEXT("Back from OEMPowerOff\r\n")));

    /* Tell GWES and Device manager that we are turning power back on */
    CallPowerProc (PowerProc, PowerFunc, FALSE, TEXT("  Calling device manager power proc.\r\n"));
    CallPowerProc (GwesPowerProc, GwesPowerFunc, FALSE, TEXT("  Calling GWES power proc.\r\n"));

    RETAILMSG(1, (TEXT("  Returning to normally scheduled programming.\r\n")));

    // If platform supports debug Ethernet, turn interrupts back on
    if (pKITLInitializeInterrupt)
        pKITLInitializeInterrupt();

    PowerOffFlag = FALSE;

    SC_CeSetThreadQuantum (hCurThread, dwQuantum);
    SC_CeSetThreadPriority (hCurThread, bPrio);
}




VOID (*pWriteDebugLED)(WORD wIndex, DWORD dwPattern);


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
VOID 
SC_WriteDebugLED(
    WORD wIndex,
    DWORD dwPattern
    ) 
{
   if( pWriteDebugLED )
       pWriteDebugLED( wIndex, dwPattern );
}



//------------------------------------------------------------------------------
// Print debug message using debug Ethernet card
//------------------------------------------------------------------------------
void
KITLWriteDebugString(
    unsigned short *str
    ) 
{
    UCHAR FmtBuf[256+sizeof(KITL_DBGMSG_INFO)];
    KITL_DBGMSG_INFO *pDbgInfo = (KITL_DBGMSG_INFO *)FmtBuf;
    // Prepend a header so that application on the other side can get timing
    // and thread info.
    pDbgInfo->dwLen      = sizeof(KITL_DBGMSG_INFO);
    pDbgInfo->dwThreadId = (DWORD)hCurThread;
    pDbgInfo->dwProcId   = (DWORD)hCurProc;
    pDbgInfo->dwTimeStamp = CurMSec;
    KUnicodeToAscii(FmtBuf+sizeof(KITL_DBGMSG_INFO),str,256);
    pKITLSend(KITL_SVC_DBGMSG, FmtBuf, sizeof(KITL_DBGMSG_INFO)+strlen(FmtBuf+sizeof(KITL_DBGMSG_INFO))+1);
}

// kernelIoctl: route it through KITL before calling OEMIoControl
BOOL KernelIoctl (DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned)
{
    DWORD dwRet = ERROR_NOT_SUPPORTED;
    BOOL fRet;

    switch (dwIoControlCode) {
    // allow non-trusted apps to get device id/info
    case IOCTL_HAL_GET_CACHE_INFO:
        if (sizeof (CacheInfo) != nOutBufSize) {
            KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
            return FALSE;
        }
        // fall through
    case IOCTL_HAL_GET_DEVICE_INFO:
    case IOCTL_HAL_GET_DEVICEID:
        // validate parameters if caller is not fully trusted
        if ((KERN_TRUST_FULL != pCurProc->bTrustLevel)
            && ((lpInBuf && !SC_MapPtrWithSize (lpInBuf, nInBufSize, hCurProc))
                || !lpOutBuf
                || !SC_MapPtrWithSize (lpOutBuf, nOutBufSize, hCurProc)
                || (lpBytesReturned && !SC_MapPtrWithSize (lpBytesReturned, sizeof (DWORD), hCurProc)))) {
            KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
            return FALSE;
        }
        break;
    default:
        TRUSTED_API (L"KernelIoctl", FALSE);
        if (pKITLIoCtl) {
            dwRet =  pKITLIoCtl (dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);
        }
        break;
    }

    if (ERROR_NOT_SUPPORTED == dwRet){
        if (IOCTL_HAL_REBOOT == dwIoControlCode){
            KDReboot(TRUE);     // Inform kdstub we are about to reboot
        }
        fRet = OEMIoControl (dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);
        if (IOCTL_HAL_REBOOT == dwIoControlCode){
            Sleep(1000);        // Allow time for hardware to reset        
            KDReboot(FALSE);    // Inform kdstub reboot failed
        }
    }
    else {
        fRet = (BOOL) dwRet;
    }
    return fRet;
}


//------------------------------------------------------------------------------
//
//  Routine to start DBGMSG and PPSH KITL service.
//
//------------------------------------------------------------------------------
BOOL
SetKernelCommDev (
    UCHAR Service,
    UCHAR CommDevice
    )
{
    UCHAR *PpfsEthBuf;

    // only support KERNEL_COMM_ETHER
    if (KERNEL_COMM_ETHER != CommDevice) {
        return FALSE;
    }
    switch (Service) {
    case KERNEL_SVC_DBGMSG:
        if (pKITLRegisterDfltClient &&
            pKITLRegisterDfltClient(KITL_SVC_DBGMSG,0,NULL,NULL)) {
            lpWriteDebugStringFunc = KITLWriteDebugString;
            return TRUE;
        }
        break;

    case KERNEL_SVC_PPSH:
        if (pKITLRegisterDfltClient &&
            pKITLRegisterDfltClient(KITL_SVC_PPSH,0,&PpfsEthBuf,&PpfsEthBuf)) {
            return TRUE;
        }
        break;
    default:
        break;
    }
    return FALSE;
}

void UnloadExe(PPROCESS pProc);
void UnloadMod(PMODULE pMod);

static DWORD UnloadDepends (PPROCESS pprc, DWORD dwFlags)
{
    PMODULE pMod;
    DWORD   dwErr = 0;
    EnterCriticalSection (&ModListcs);
    __try {
        for (pMod = pModList; pMod; pMod = pMod->pMod) {
            if ((pMod->inuse & pprc->aky)               // the module is loaded by the process
                && PageAble (&pMod->oe)                 // the module is pageable
                && ((PAGE_OUT_ALL_DEPENDENT_DLL == dwFlags)     // either the flags say to page out all
                    || !(pMod->inuse & ~pprc->aky))) {          // or we're the only only process using this moudle
                UnloadMod (pMod);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        dwErr = ERROR_INVALID_PARAMETER;
    }
    LeaveCriticalSection (&ModListcs);
    return dwErr;
}

//------------------------------------------------------------------------------
// Page out a particular module
//------------------------------------------------------------------------------
static BOOL SC_PageOutModule (HANDLE hModule, DWORD dwFlags)
{
    PPROCESS pprc = NULL;
    PMODULE  pMod = NULL;
    DWORD    dwErr = 0;
    ACCESSKEY oldaky;


    TRUSTED_API (L"SC_PageOutModule", FALSE);

    EnterCriticalSection (&PageOutCS);      // criticalsection ordering require pageoutCS to 
    EnterCriticalSection (&LLcs);           // be held before Loader CS

    switch (dwFlags) {
    case PAGE_OUT_PROCESS_ONLY:
    case PAGE_OUT_DLL_USED_ONLY_BY_THISPROC:
    case PAGE_OUT_ALL_DEPENDENT_DLL:
        if (!hModule
            || (hModule == ProcArray[0].hProc)          // cannot page out NK
            || (GetCurrentProcess() == hModule)         // page out current process - not a good idea
            || (hCurProc == hModule)
            || (!(pprc = HandleToProc (hModule))                    // has to be valid process
                && !IsValidModule (pMod = (PMODULE) hModule))) {    // or a valid module
            dwErr = ERROR_INVALID_HANDLE;
            break;
        }

        SWITCHKEY (oldaky, 0xFFFFFFFF);
        __try {
            if (pprc) {
                // paging out a process
                UnloadExe (pprc);

                // pageout dependent modules based on flags
                if  (PAGE_OUT_PROCESS_ONLY != dwFlags)
                    dwErr = UnloadDepends (pprc, dwFlags);
                
            } else if (PageAble (&pMod->oe)) {
                UnloadMod (pMod);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            dwErr = ERROR_INVALID_PARAMETER;
        }
        SETCURKEY (oldaky);
        break;
    default:
        dwErr = ERROR_INVALID_PARAMETER;
    }
    
    LeaveCriticalSection (&LLcs);
    LeaveCriticalSection (&PageOutCS);

    if (dwErr)
        KSetLastError (pCurThread, dwErr);

    return !dwErr;
}

