//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include <kernel.h>
#include <psapi.h>

// memory usage watermark, borrowed from physmem.c
#define PAGEOUT_LOW         (( 68*1024)/PAGE_SIZE)  // do pageout once below this mark, reset when above high
#define PAGEOUT_HIGH        ((132*1024)/PAGE_SIZE)

extern CRITICAL_SECTION rtccs, LLcs, NameCS;
extern LPVOID pGwesHandler;
extern long PageOutTrigger;    // Threshold level to start page out.
extern long PageOutLevel;      // Threshold to stop page out.
DWORD curridlelow, curridlehigh, idleconv;
Name *pDebugger, *pPath, *pInjectDLLs;
BOOL bDbgOnFirstChance = FALSE;
LPVOID pGwesHandler = SC_Nop;

// Default registry values, may be overwritten with actual desktop zone settings
DWORD g_dwKeys[HOST_TRANSCFG_NUM_REGKEYS] = {
    (DWORD)-1,          // nk
    (DWORD)-1,          // filesys
    (DWORD)-1,          // fsdmgr
    (DWORD)-1,          // relfsd
    (DWORD)-1,          // device
    CELOG_CE_MASK,      // CeLogZoneCE
    CELOG_USER_MASK,    // CeLogZoneUser
    CELOG_PROCESS_MASK, // CeLogZoneProcess
};
WCHAR* g_keyNames[] = {L"Nk", L"FileSys", L"FSDMGR", L"ReleaseFSD", L"Device", L"CeLogZoneCE", L"CeLogZoneUser", L"CeLogZoneProcess"};

BOOL bAllKMode;
void UpdateCallerInfo (PTHREAD pth, BOOL fInKMode);

PFN_OEMKDIoControl pKDIoControl = NULL;

//
// old stubs from nocefnk.dll
//
int __cdecl InitializeJit( PFNOPEN pfOpen, PFNCLOSE pfClose )
{
RETAILMSG(1,(TEXT("InitializeJit\r\n")));
    return 0;
}


DWORD __cdecl ModuleJit( LPCWSTR source, LPWSTR target, HANDLE* hIn )
{
    return 0;
}

/* xref ref regset
; NK references two registry settings:
;   HKLM\Debug\JITDebugger - for just in time debugging
; and
;   HKLM\Loader\SystemPath - for alternative loader path

[HKEY_LOCAL_MACHINE\Debug]
   ; update the JITDebugger field to be the JIT debugger of your platform
   ;"JITDebugger"="My Debugger Name"

[HKEY_LOCAL_MACHINE\Loader]
   ; specifies all the alternative paths for loader to search when loading an app/module
   ; Note that every string must end with "\\" as the loader uses direct concantenation
   ; to obtain the path of the exe/dll.
   ;"SystemPath"=multi_sz:"AltPath1\\","AltPath2\\"
[HKEY_LOCAL_MACHINE\KERNEL]
   ; specifies all DLLs file name to be injected into process
   ;"InjectDLL"=multi_sz:"INJECT1.DLL","INJECT2.DLL"
*/

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_CeSetExtendedPdata(
    LPVOID pData
    ) 
{
#ifdef x86
    KSetLastError(pCurThread,ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
#else
    TRUSTED_API (L"SC_CeSetExtendedPdata", FALSE);
    pCurProc->pExtPdata = pData;
    return TRUE;
#endif
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
VOID 
SC_GetSystemInfo(
    LPSYSTEM_INFO lpSystemInfo
    ) 
{
    DEBUGMSG(ZONE_ENTRY,(L"SC_GetSystemInfo entry: %8.8lx\r\n",lpSystemInfo));
    if (SC_MapPtrWithSize(lpSystemInfo, sizeof (SYSTEM_INFO), hCurProc)) {
        lpSystemInfo->wProcessorArchitecture = PROCESSOR_ARCHITECTURE;
        lpSystemInfo->wReserved = 0;
        lpSystemInfo->dwPageSize = PAGE_SIZE;
        lpSystemInfo->lpMinimumApplicationAddress = (LPVOID)0x10000;
        lpSystemInfo->lpMaximumApplicationAddress = (LPVOID)0x7fffffff;
        lpSystemInfo->dwActiveProcessorMask = 1;
        lpSystemInfo->dwNumberOfProcessors = 1;
        lpSystemInfo->dwProcessorType = CEProcessorType;
        lpSystemInfo->wProcessorLevel = ProcessorLevel;
        lpSystemInfo->wProcessorRevision = ProcessorRevision;
        lpSystemInfo->dwAllocationGranularity = 0x10000;
    }
    DEBUGMSG(ZONE_ENTRY,(L"SC_GetSystemInfo exit\r\n"));
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL SC_QueryInstructionSet(DWORD dwInstSet, LPDWORD lpdwOSInstSet)
{
    BOOL fRet = TRUE;

    DEBUGMSG(ZONE_ENTRY,(L"SC_QueryInstructionSet entry: %8.8lx %8.8lx\r\n",dwInstSet, lpdwOSInstSet));
    if (!SC_MapPtrWithSize (lpdwOSInstSet, sizeof (DWORD), hCurProc)) {
        KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
        fRet = FALSE;
        
    } else if (dwInstSet != CEInstructionSet) {
    
        switch (dwInstSet) {
        case PROCESSOR_QUERY_INSTRUCTION:
            // query always okay
            break;
        case PROCESSOR_MIPS_MIPSII_INSTRUCTION:
            // MIPSII can run on MIPS16
            fRet = (PROCESSOR_MIPS_MIPS16_INSTRUCTION == CEInstructionSet);
            break;

        // OS never build THUMB anymore
        case PROCESSOR_ARM_V4IFP_INSTRUCTION:
        case PROCESSOR_ARM_V4TFP_INSTRUCTION:
            fRet = (PROCESSOR_ARM_V4IFP_INSTRUCTION == CEInstructionSet);
            break;
        case PROCESSOR_ARM_V4T_INSTRUCTION:
        case PROCESSOR_ARM_V4I_INSTRUCTION:
        case PROCESSOR_ARM_V4_INSTRUCTION:
            fRet = (PROCESSOR_ARM_V4I_INSTRUCTION == CEInstructionSet)
                || (PROCESSOR_ARM_V4IFP_INSTRUCTION == CEInstructionSet);
            break;

        default:
            // MIPS use different calling convention between HW FP and FP emulator
            // so don't support FP emulator binary on HW FP OS.
            fRet = ((CEInstructionSet >> 24) != PROCESSOR_ARCHITECTURE_MIPS)
                && ((dwInstSet | PROCESSOR_FEATURE_FP) == CEInstructionSet);
            break;
        }
    }

    __try {
        *lpdwOSInstSet = CEInstructionSet;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
        fRet = FALSE;
    }

    DEBUGMSG(ZONE_ENTRY,(L"SC_QueryInstructionSet exit %d\r\n", fRet));
    return fRet;
}

BOOL (* pOEMIsProcessorFeaturePresent) (DWORD dwProcessorFeature);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL
SC_IsProcessorFeaturePresent(
    DWORD dwProcessorFeature
    )
{
    BOOL fRet = FALSE;

    DEBUGMSG(ZONE_ENTRY,(L"SC_IsProcessorFeaturePresent entry: %8.8lx\r\n",dwProcessorFeature));
    switch( dwProcessorFeature )
    {
#ifdef _X86_
//
// x86 Processor Features
//
    case PF_FLOATING_POINT_PRECISION_ERRATA:
        fRet = ProcessorFeatures;
        break;
    case PF_MMX_INSTRUCTIONS_AVAILABLE:
        fRet = ProcessorFeatures & CPUID_MMX;
        break;
    case PF_3DNOW_INSTRUCTIONS_AVAILABLE:
        fRet = ProcessorFeaturesEx & CPUIDEX_3DNOW;
        break;
    case PF_RDTSC_INSTRUCTION_AVAILABLE:
        fRet = ProcessorFeatures & CPUID_TSC;
        break;
    case PF_FLOATING_POINT_EMULATED:
        fRet = (dwFPType == FPTYPE_SOFTWARE);
        break;
    // Not implemented/supported on CE
    case PF_COMPARE_EXCHANGE_DOUBLE:
    case PF_XMMI_INSTRUCTIONS_AVAILABLE:
        break;

#elif defined (ARM)
//
// ARM Processor Features
//
    case PF_ARM_VFP10:
        fRet = vfpStat & VFP_EXIST;
        break;
        
    case PF_ARM_V4:
#if defined(ARMV4I) || defined(ARMV4T)
    case PF_ARM_THUMB:
#endif
        fRet = TRUE;
        break;

#elif defined (MIPS)
//
// MIPS Processor Features
//
    case PF_MIPS_MIPS16:
        fRet = IsMIPS16Supported;
        break;
    case PF_MIPS_MIPSII:
#ifdef MIPSIV        
    case PF_MIPS_MIPSIV:
#endif
#ifdef MIPS_HAS_FPU
    case PF_MIPS_FPU:
#endif
        fRet = TRUE;
        break;


#elif defined (SHx)
//
// SHx Processor Features
//

#ifdef SH3
    case PF_SHX_SH3:
#elif defined (SH4)
    case PF_SHX_SH4:
    case PF_SHX_FPU:
#else
    #error Unknown CPU
#endif
        fRet = TRUE;
        break;
        
#endif

        
    default:
        // call down to OEM for everything we don't know
        if (pOEMIsProcessorFeaturePresent) {
            fRet = pOEMIsProcessorFeaturePresent (dwProcessorFeature);
        }
        break;
    }
    DEBUGMSG(ZONE_ENTRY,(L"SC_IsProcessorFeaturePresent exit %d\r\n", fRet));
    return fRet;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_IsBadPtr(
    DWORD flag,
    LPBYTE lpv,
    DWORD len
    ) 
{
    DWORD dwBeg = (DWORD) lpv;
    DWORD dwEnd = (DWORD) lpv+len-1;
    
    DEBUGMSG(ZONE_ENTRY,(L"SC_IsBadPtr entry: %8.8lx %8.8lx %8.8lx\r\n",flag,lpv,len));
    // return false if zero length
    if ((int) len <= 0) {
        DEBUGMSG(ZONE_ENTRY,(L"SC_IsBadPtr exit: %8.8lx zero or negative length)\r\n", len != 0));
        return len != 0;
    }
    // cannot span across Kernel/User address
    if ((dwBeg & 0x80000000) != (dwEnd & 0x80000000)) {
        KSetLastError(pCurThread,ERROR_INVALID_ADDRESS);
        DEBUGMSG(ZONE_ENTRY,(L"SC_IsBadPtr exit: TRUE (ptr bad)\r\n"));
        return TRUE;
    }

    if (dwBeg & 0x80000000) {
        // must be in KMode if it's a kernel address
        if (!bAllKMode && !(pCurThread->tlsPtr[PRETLS_THRDINFO] & UTLS_INKMODE)) {
            KSetLastError(pCurThread,ERROR_INVALID_ADDRESS);
            return TRUE;
        }
        // return if the range is not completely within Secure Section
        if (!IsSecureVa (dwBeg) || !IsSecureVa (dwEnd)) {
            return (dwEnd >= (SECURE_SECTION << VA_SECTION))
                && (dwBeg < ((SECURE_SECTION+1) << VA_SECTION));
        }
    } else {
    
        DWORD aky, i;
        DEBUGMSG (ZONE_ENTRY, (L"Testing Access, pCurThread->aky = %8.8lx\r\n", pCurThread->aky));
        // not kmode, check access right
        for (i = dwBeg >> VA_SECTION; (i <= MAX_PROCESSES) && (i <= (dwEnd >> VA_SECTION)); i ++) {
            if (i) {
                aky = 1 << (i - 1);
                if (!TestAccess (&pCurThread->aky, &aky)) {
                    KSetLastError (pCurThread,ERROR_INVALID_ADDRESS);
                    DEBUGMSG(ZONE_ENTRY,(L"SC_IsBadPtr exit: TRUE (ptr bad)\r\n"));
                    return TRUE;
                }
            }
            
        }
    }
    
    if (!LockPages(lpv, len, 0, flag==VERIFY_WRITE_FLAG ?
            (LOCKFLAG_QUERY_ONLY | LOCKFLAG_WRITE) : (LOCKFLAG_QUERY_ONLY | LOCKFLAG_READ))) {
        KSetLastError(pCurThread,ERROR_INVALID_ADDRESS);
        DEBUGMSG(ZONE_ENTRY,(L"SC_IsBadPtr exit: TRUE (ptr bad)\r\n"));
        return TRUE;
    }
    DEBUGMSG(ZONE_ENTRY,(L"SC_IsBadPtr exit: FALSE (ptr OK)\r\n"));
    return FALSE;
}




//------------------------------------------------------------------------------
// @func LPVOID | MapPtrUnsecure | Maps an unmapped pointer to a mapped pointer in a process
// @parm LPVOID | lpv | pointer to map
// @parm HANDLE | hProc | process to map into
// @rdesc  Returns a mapped version of the pointer, or 0 for failure
// @comm If the pointer is already mapped, the original pointer is returned.  If the pointer
//       is unmapped, it first maps it, then returns the mapped pointer.  No access validation is performed.
//       This function should be called to map pointers which are passed to a PSL where the pointer is not
//       a parameter directly, but obtained from a structure, and needs to be adjusted for the address space.
// @xref <f MapPtrUnsecure>
//------------------------------------------------------------------------------
LPVOID 
SC_MapPtrUnsecure(
    LPVOID lpv,
    HANDLE hProc
    ) 
{
    LPVOID retval;
    PPROCESS pProc;
    DEBUGMSG(ZONE_ENTRY,(L"SC_MapPtrUnsecure entry: %8.8lx %8.8lx\r\n",lpv,hProc));
    if (!hProc || (hProc == GetCurrentProcess()))
        hProc = hCurProc;
    if (!(pProc = HandleToProc(hProc))) {
        KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
        retval = 0;
    } else
        retval = MapPtrProc(lpv,pProc);
    DEBUGMSG(ZONE_ENTRY,(L"SC_MapPtrUnsecure exit: %8.8lx\r\n",retval));
    return retval;
}




//------------------------------------------------------------------------------
// @func HANDLE | GetProcFromPtr | Returns the process id which owns the pointer passed in
// @parm LPVOID | ptr | pointer from which to find a process
// @rdesc  Returns the process id of the owning process
// @comm Returns the owner process of the pointer, or NULL if the pointer is not valid.
//------------------------------------------------------------------------------
HANDLE 
SC_GetProcFromPtr(
    LPVOID lpv
    ) 
{
    HANDLE hRet = hCurProc;
    DEBUGMSG(ZONE_ENTRY,(L"SC_GetProcFromPtr entry: %8.8lx\r\n",lpv));
    if (ZeroPtr(lpv) == (DWORD)lpv) {
        if (IsModCodeAddr (lpv)) {
            HANDLE hCaller = SC_GetCallerProcess ();
            hRet = hCaller? hCaller : hCurProc;
        }
    } else if (IsSecureVa(lpv)) {
        hRet = ProcArray[0].hProc;
    } else {
        int idx = ((DWORD)lpv>>VA_SECTION)-1;
        hRet = ((idx >= MAX_PROCESSES) || !ProcArray[idx].dwVMBase)
            ? 0 : ProcArray[idx].hProc;
    }
    DEBUGMSG(ZONE_ENTRY,(L"SC_GetProcFromPtr exit :%8.8lx\r\n", hRet));
    return hRet;
}




//------------------------------------------------------------------------------
// @func LPVOID | MapPtrToProcess | Maps an unmapped pointer to a mapped pointer in a process
// @parm LPVOID | lpv | pointer to map
// @parm HANDLE | hProc | process to map into
// @rdesc  Returns a mapped version of the pointer, or 0 for failure
// @comm If the pointer is already mapped, the original pointer is returned if the caller
//       has access to dereference that pointer, else 0 is returned.  If the pointer is
//       unmapped, it first maps it, then returns the mapped pointer if the caller can access
//       it, else 0.  This function should be called to map pointers which are passed to a PSL where the pointer is not
//       a parameter directly, but obtained from a structure, and needs to be adjusted for the address space.
// @xref <f MapPtrToProcess>
//------------------------------------------------------------------------------
LPVOID 
SC_MapPtrToProcess(
    LPVOID lpv,
    HANDLE hProc
    ) 
{
    PPROCESS pProc;
    DEBUGMSG(ZONE_ENTRY,(L"SC_MapPtrToProcess entry: %8.8lx %8.8lx\r\n",lpv,hProc));
    if (!(pProc = HandleToProc(hProc))) {
        KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
        lpv = 0;
    } else if ((DWORD)lpv>>VA_SECTION != 0) {
        if (!IsAccessOK(lpv,CurAKey)) {
            KSetLastError(pCurThread,ERROR_ACCESS_DENIED);
            lpv = 0;
        }
    } else if ((DWORD)lpv > 0x10000)
        lpv = MapPtrProc(lpv,pProc);
    DEBUGMSG(ZONE_ENTRY,(L"SC_MapPtrToProcess exit: %8.8lx\r\n",lpv));
    return lpv;
}

LPVOID SC_MapPtrWithSize (LPVOID ptr, DWORD dwLen, HANDLE hProc)
{
    DWORD dwAddr = (DWORD) SC_MapPtrToProcess (ptr, hProc);
    DWORD dwEnd;
    PPROCESS pProc;

    // fail if we don't have a caller process
    if (!(pProc = HandleToProc(hProc))) {
        KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
        return 0;
    }

    // dwLen == 0 is treated as dwLen == 1
    if (!dwLen)
        dwLen = 1;

    dwEnd = dwAddr + dwLen - 1;

    if ((KERN_TRUST_FULL != pProc->bTrustLevel) && !(KTHRDINFO(pCurThread) & UTLS_NKCALLOUT)) {
        ACCESSKEY aky = pProc->aky | ProcArray[0].aky;

        // not valid if it overlapped with any KMode only address
        if ((dwEnd < dwAddr)                        // address wrap around
            || ((int) dwAddr < 0x10000)             // invalid start address
            || ((int) dwEnd < 0x10000)              // invalid end address
            || !IsAccessOK ((LPVOID) dwAddr, aky)
            || !IsAccessOK ((LPVOID) dwEnd, aky)) {

            KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
            return 0;
        }
    }

    return (LPVOID) dwAddr;
}

LPVOID SC_MapCallerPtr (LPVOID ptr, DWORD dwLen)
{
    return SC_MapPtrWithSize (ptr, dwLen, SC_GetCallerProcess ());
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
SC_GetProcAddrBits(
    HANDLE hproc
    ) 
{
    PPROCESS pproc;
    DEBUGMSG(ZONE_ENTRY,(L"SC_GetProcAddrBits entry: %8.8lx\r\n",hproc));
    if (!(pproc = HandleToProc(hproc))) {
        KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
        DEBUGMSG(ZONE_ENTRY,(L"SC_GetProcAddrBits exit: %8.8lx\r\n",0));
        return 0;
    }
    DEBUGMSG(ZONE_ENTRY,(L"SC_GetProcAddrBits exit: %8.8lx\r\n",pproc->dwVMBase));
    return pproc->dwVMBase;
}




//------------------------------------------------------------------------------
// @func DWORD | GetFSHeapInfo | Gets info on the physical space reserved for the file system
// @comm Retrieves the start of the physical memory reserved for the file system
//------------------------------------------------------------------------------
DWORD 
SC_GetFSHeapInfo(void) 
{
    DEBUGMSG(ZONE_ENTRY,(L"SC_GetFSHeapInfo entry\r\n"));
    DEBUGMSG(ZONE_ENTRY,(L"SC_GetFSHeapInfo exit: %8.8lx\r\n",PAGEALIGN_UP(pTOC->ulRAMFree + MemForPT)));
    return PAGEALIGN_UP(pTOC->ulRAMFree+MemForPT);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SC_UpdateNLSInfo(
    DWORD ocp,
    DWORD acp,
    DWORD sysloc,
    DWORD userloc
    ) 
{

    KInfoTable[KINX_NLS_CP] = (DWORD)(((WORD)ocp << 16) + (WORD)acp);
    KInfoTable[KINX_NLS_SYSLOC] = sysloc;
    KInfoTable[KINX_NLS_USERLOC] = userloc;
}

DWORD randdw1, randdw2;



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
__int64 
SC_CeGetRandomSeed() 
{
    return (((__int64)randdw1)<<32) | (__int64)randdw2;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
SC_GetIdleTime(void) 
{
    DWORD result;
    __int64 temp;
    DEBUGMSG(ZONE_ENTRY,(L"SC_GetIdleTime entry\r\n"));
    temp = curridlehigh;
    if (idleconv) {
        temp = (temp * 0x100000000) + curridlelow;
        result = (DWORD)(temp/idleconv);
    } else
        result = 0xffffffff;
    DEBUGMSG(ZONE_ENTRY,(L"SC_GetIdleTime exit: %8.8lx\r\n",result));
    return result;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPCWSTR 
SC_GetProcName(void) 
{
    LPWSTR retval;
    DEBUGMSG(ZONE_ENTRY,(L"SC_ProcGetName entry\r\n"));
    retval = MapPtr(pCurProc->lpszProcName);
    DEBUGMSG(ZONE_ENTRY,(L"SC_ProcGetName exit: %8.8lx\r\n",retval));
    return retval;
}




//------------------------------------------------------------------------------
// @func HANDLE | GetOwnerProcess | Returns the process id which owns the current thread
// @rdesc Returns the process id of the process which spawned the current thread
// @comm Returns the process id of the process which spawned the current thread
//------------------------------------------------------------------------------
HANDLE 
SC_GetOwnerProcess(void) 
{
    DEBUGMSG(ZONE_ENTRY,(L"SC_GetOwnerProcess entry\r\n"));
    DEBUGMSG(ZONE_ENTRY,(L"SC_GetOwnerProcess exit: %8.8lx\r\n",pCurThread->pOwnerProc->hProc));
    return pCurThread->pOwnerProc->hProc;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPWSTR 
SC_GetCommandLineW(void) 
{
    DEBUGMSG(ZONE_ENTRY,(L"SC_GetCommandLineW entry\r\n"));
    DEBUGMSG(ZONE_ENTRY,(L"SC_GetCommandLineW exit: %8.8lx\r\n",pCurThread->pOwnerProc->pcmdline));
    return (LPWSTR)pCurThread->pOwnerProc->pcmdline;
}



//------------------------------------------------------------------------------
// @func HANDLE | GetCallerProcess | Returns the process id which called the currently running PSL
// @rdesc Returns the process id of the process which called the currently running PSL
// @comm Returns the process id of the process which called the currently running PSL
//------------------------------------------------------------------------------
HANDLE 
SC_GetCallerProcess(void) 
{
    PCALLSTACK pcstk = pCurThread->pcstkTop;
    DEBUGMSG(ZONE_ENTRY,(L"SC_GetCallerProcess entry\r\n"));
    while (pcstk && (pcstk->dwPrcInfo & CST_IN_KERNEL) && (pcstk->pprcLast != ProcArray)) {
        pcstk = pcstk->pcstkNext;
    }
    DEBUGMSG(ZONE_ENTRY,(L"SC_GetCallerProcess exit: %8.8lx\r\n",pcstk? pcstk->pprcLast->hProc : 0));
    return pcstk? pcstk->pprcLast->hProc : 0;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
SC_CeGetCurrentTrust(void) 
{
    DWORD retval;
    DEBUGMSG(ZONE_ENTRY,(L"SC_CeGetCurrentTrust entry\r\n"));
    retval = pCurProc->bTrustLevel;
    DEBUGMSG(ZONE_ENTRY,(L"SC_CeGetCurrentTrust exit: %8.8lx\r\n",retval));
    return retval;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
SC_CeGetCallerTrust(void) 
{
    HANDLE hval = SC_GetCallerProcess();
    DWORD retval;
    PPROCESS pproc;
    DEBUGMSG(ZONE_ENTRY,(L"SC_CeGetCallerTrust entry\r\n"));
    if (hval) {
        pproc = HandleToProc(hval);
        DEBUGCHK(pproc);
        retval = pproc->bTrustLevel;
    } else
        retval = pCurProc->bTrustLevel;
    DEBUGMSG(ZONE_ENTRY,(L"SC_CeGetCallerTrust exit: %8.8lx\r\n",retval));
    return retval;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
SC_GetCallerIndex(void) 
{
    HANDLE hval = SC_GetCallerProcess();
    DWORD retval;
    PPROCESS pproc;
    DEBUGMSG(ZONE_ENTRY,(L"SC_GetCallerIndex entry\r\n"));
    if (hval) {
        pproc = HandleToProc(hval);
        DEBUGCHK(pproc);
        retval = pproc->procnum;
    } else
        retval = (DWORD)-1;
    DEBUGMSG(ZONE_ENTRY,(L"SC_GetCallerIndex exit: %8.8lx\r\n",retval));
    return retval;
}

BOOL fJitIsPresent;

// Must be kept in sync with filesys\main\fsmain.c
#define MAX_APPSTART_KEYNAME 128

DWORD g_bAllowSystemAccess = 0;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
RunApps(
    ulong param
    ) 
{
    
    WCHAR  pName[MAX_APPSTART_KEYNAME];
    BYTE   pProp[MAX_PATH*sizeof(WCHAR)];
    DWORD  size, type;
    HANDLE hFSReady;
    DWORD cBytes, cPages;
    BOOL   fFileSysStarted;
    DWORD  dwData;

    // Start filesys and wait for the registry to become available
    hFSReady = CreateEvent(NULL, TRUE, FALSE, TEXT("SYSTEM/FSReady"));
    DEBUGCHK(hFSReady);
    if (IsCeLogEnabled()) {
        CELOG_LaunchingFilesys();
    }
    if (CreateProcess(L"filesys.exe", 0,0,0,0,0x80000000,0,0,0,0) && hFSReady) {
        WaitForMultipleObjects(1, &hFSReady, 0, INFINITE);
        CloseHandle(hFSReady);
    }


    //
    // Final kernel configuration requiring the registry
    //

    fJitIsPresent = InitializeJit(JitOpenFile, JitCloseFile);

    // Initialize MUI-Resource loader (requires registry)
    InitMUILanguages();

    fFileSysStarted = (SystemAPISets[SH_FILESYS_APIS] != NULL);

    // Now that filesys is ready, the registry is available
    size = sizeof(pName);
    if (fFileSysStarted && (RegQueryValueExW(HKEY_LOCAL_MACHINE, L"JITDebugger", (LPDWORD)L"Debug",
                          &type, (LPBYTE)pName, &size) == ERROR_SUCCESS)
        && (type == REG_SZ) && (size < sizeof(pProp))
        && (pDebugger = AllocName((strlenW(pName)+1) * sizeof(WCHAR)))) {
        kstrcpyW(pDebugger->name, pName);
    }

    size = sizeof(dwData);
    if (fFileSysStarted && (RegQueryValueExW(HKEY_LOCAL_MACHINE, L"LaunchJITOnFirstChance", (LPDWORD)L"Debug",
                          &type, (LPBYTE)&dwData, &size) == ERROR_SUCCESS)
                          && (type == REG_DWORD) && (size == sizeof(dwData))) {
        bDbgOnFirstChance = (dwData == 1) ? TRUE : FALSE;
    }

    size = sizeof(pProp);
    if (fFileSysStarted && (RegQueryValueExW(HKEY_LOCAL_MACHINE, L"SystemPath", (LPDWORD)L"Loader",
                          &type, (LPBYTE)pProp, &size) == ERROR_SUCCESS)
        && (type == REG_MULTI_SZ) && (size < sizeof(pProp))
        && (pPath = AllocName(size)))
        memcpy(pPath->name, pProp, size);

    // set default value for PageOutTrigger and PageOutLevel
    if (!fFileSysStarted || RegQueryValueEx(HKEY_LOCAL_MACHINE, L"cbLow", (LPDWORD)L"SYSTEM\\OOM",
                        &type, (LPBYTE)&cBytes, &size) != ERROR_SUCCESS)
        cBytes = 0;
    if (!fFileSysStarted || RegQueryValueEx(HKEY_LOCAL_MACHINE, L"cpLow", (LPDWORD)L"SYSTEM\\OOM",
                        &type, (LPBYTE)&cPages, &size) != ERROR_SUCCESS)
        cPages = 0;
    if (cBytes || cPages) {
        cBytes = (cBytes + PAGE_SIZE -1) / PAGE_SIZE;   // convert to pages
        cPages = (cBytes>cPages)?cBytes:cPages;
    }
    PageOutTrigger = cPages + PAGEOUT_LOW;
    PageOutLevel = cPages + PAGEOUT_HIGH;

    // get Inject Dlls from registry
    if (fFileSysStarted && (RegQueryValueEx(HKEY_LOCAL_MACHINE, L"InjectDLL", (LPDWORD)L"SYSTEM\\KERNEL",
                         &type, NULL, &size) == ERROR_SUCCESS)
         && (type == REG_MULTI_SZ) && (size != 0)
         && (pInjectDLLs = AllocName(size))) {
         if (RegQueryValueEx(HKEY_LOCAL_MACHINE, L"InjectDLL", (LPDWORD)L"SYSTEM\\KERNEL",
                         &type, (LPBYTE)(pInjectDLLs->name), &size) != ERROR_SUCCESS) {
            FreeName(pInjectDLLs);
            pInjectDLLs = NULL;
         }
    }
    if (RegQueryValueEx(HKEY_LOCAL_MACHINE, L"AllowSystemAccess", (LPDWORD)L"System\\ObjectStore", NULL, (LPBYTE)&g_bAllowSystemAccess, &size) != ERROR_SUCCESS) {
        g_bAllowSystemAccess = 0;        
    }  

    // Tell filesys that the remaining kernel work is done
    if (fFileSysStarted) {
        pSignalStarted(0);
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_SetDbgZone(
    DWORD dwProcid,
    LPVOID lpvMod,
    LPVOID BasePtr,
    DWORD zone,
    LPDBGPARAM lpdbgTgt
    ) 
{
    DWORD retval = FALSE;
    PMODULE pMod;
    PPROCESS pProc;
    LPDBGPARAM lpdbgSrc = 0;
    ACCESSKEY ulOldKey;

    // make sure the parameter is okay
    if (lpdbgTgt && !SC_MapPtrWithSize (lpdbgTgt, sizeof (DBGPARAM), hCurProc)) {
        return FALSE;
    }
    
    SWITCHKEY(ulOldKey,0xffffffff);

    // Cannot turn all 32 on -- zone 0xFFFFFFFF is used to query

    if (dwProcid) {
        if (!lpvMod && (pProc = HandleToProc((HANDLE)dwProcid)) && pProc->ZonePtr) {
            if (zone != 0xffffffff)
                pProc->ZonePtr->ulZoneMask=zone;
            lpdbgSrc = pProc->ZonePtr;
            retval = TRUE;
        }
    } else if (lpvMod) {
        pMod = (PMODULE)lpvMod;
        if (IsValidModule(pMod) && pMod->ZonePtr) {
            DWORD dwUseMask = pMod->inuse | 0x1;    // Add the kernel's bit
            DWORD dwZeroAddr = (DWORD)(ZeroPtr((LPVOID)(pMod->ZonePtr)));
            INT i=0;
            // set source for copying names: use kernel's version since it's guaranteed to be there !
            lpdbgSrc = (LPDBGPARAM)(MapPtrProc(dwZeroAddr,&ProcArray[0]));
            if (zone != 0xffffffff) {
                for (i = 0; dwUseMask; i++, dwUseMask >>= 1) {
                    if (dwUseMask & 0x1)
                        ((LPDBGPARAM)MapPtrProc(dwZeroAddr,&ProcArray[i]))->ulZoneMask = zone;
                }
            }
            retval = TRUE;
        }
    }
    // Copy information if required
    if (lpdbgSrc && lpdbgTgt)
        memcpy(lpdbgTgt, lpdbgSrc, sizeof(DBGPARAM));
    // restore access rights
    SETCURKEY(ulOldKey);
    return retval;
}

extern o32_lite *FindOptr (o32_lite *optr, int nCnt, DWORD dwAddr);


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_RegisterDbgZones(
    HANDLE hMod,
    LPDBGPARAM lpdbgparam
    ) 
{
    DWORD hKeyPeg=0, hKeyZones=0;
    DWORD dwType, dwData, dwSize;
    CHAR szName[16];
    PMODULE pMod = (PMODULE)hMod;
    HKEY hZones;
    BOOL bTryLocal;
    int i;
    
    lpdbgparam = MapPtr(lpdbgparam);
    if (pMod) {
        o32_lite *optr;
        if (!IsValidModule(pMod))
            return FALSE;
        if (pMod->ZonePtr)
            return TRUE;

        // verify lpdbgparam is in RO section of this module
        if (!(optr = FindOptr (pMod->o32_ptr, pMod->e32.e32_objcnt, ZeroPtr (lpdbgparam)))
            || !(optr->o32_flags & IMAGE_SCN_MEM_WRITE)) {
            DEBUGMSG (1, (L"SC_RegisterDbgZones failed with invalide DBGPARAM (%8.8lx)\r\n", lpdbgparam));
            return FALSE;
        }
        pMod->ZonePtr = (LPDBGPARAM)MapPtrProc(ZeroPtr(lpdbgparam),&ProcArray[0]);
	} else {
		if (!SC_MapPtrWithSize (lpdbgparam, sizeof(DBGPARAM), hCurProc)) {
            DEBUGMSG (1, (L"SC_RegisterDbgZones failed with invalide DBGPARAM (%8.8lx)\r\n", lpdbgparam));
            return FALSE;
		}
        pCurThread->pOwnerProc->ZonePtr = lpdbgparam;
	}

    bTryLocal = TRUE;
    //
    // See if the debug zone mask is specified in the desktop PC's registry
    //
    rRegOpen((DWORD)HKEY_CURRENT_USER, "Pegasus", &hKeyPeg);
    if (hKeyPeg) {
        rRegOpen(hKeyPeg, "Zones", &hKeyZones);
        if (hKeyZones) {
            KUnicodeToAscii(szName, lpdbgparam->lpszName, sizeof(szName));
            rRegGet(hKeyZones, szName, &dwType, (LPBYTE)&dwData, &dwSize);
            if (dwType == REG_DWORD) {
                if (pMod)
                    SC_SetDbgZone(0,pMod,0,dwData,0);
                else
                    SC_SetDbgZone((DWORD)hCurProc,0,0,dwData,0);
                bTryLocal = FALSE;
            }
            rRegClose(hKeyZones);
        }
        rRegClose(hKeyPeg);
    } else {
        dwData = -1;
        for (i = 0; i < HOST_TRANSCFG_NUM_REGKEYS; i++) {
            if (strcmpW (lpdbgparam->lpszName, g_keyNames[i]) == 0) {
                dwData = g_dwKeys[i];
                if (dwData != -1) {
                    if (pMod)
                        SC_SetDbgZone(0,pMod,0,dwData,0);
                    else
                        SC_SetDbgZone((DWORD)hCurProc,0,0,dwData,0);
                    bTryLocal = FALSE;
                }
                break;
            }
        }
    }

    //
    // Can only check the local registry if the filesys API is registered.
    //
    if (bTryLocal && (UserKInfo[KINX_API_MASK] & (1 << SH_FILESYS_APIS))) {
        //
        // See if the debug zone mask is specified in the Windows CE registry
        // (in HLM\DebugZones:<module name>)
        //
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("DebugZones"), 0, 0,
                        &hZones) == ERROR_SUCCESS) {
            dwSize = sizeof(DWORD);
            if (RegQueryValueEx(hZones, lpdbgparam->lpszName, 0, &dwType,
                        (PUCHAR)&dwData, &dwSize) == ERROR_SUCCESS) {
                if (dwType == REG_DWORD) {
                    if (pMod)
                        SC_SetDbgZone(0,pMod,0,dwData,0);
                    else
                        SC_SetDbgZone((DWORD)hCurProc,0,0,dwData,0);
                }
            }
            RegCloseKey(hZones);
        }
    }
    return TRUE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
PTHREAD 
ZeroAllThreadsTLS(
    PTHREAD pCur,
    DWORD slot
    ) 
{
    KCALLPROFON(65);
    if (pCur == (PTHREAD)1)
        pCur = pCurProc->pTh;
    else if (HandleToThread(pCur->hTh) != pCur)
        return (PTHREAD)1;
    pCur->tlsPtr[slot] = 0;
    KCALLPROFOFF(65);
    return pCur->pNextInProc;
}



//------------------------------------------------------------------------------
/* Thread local storage APIs */
//------------------------------------------------------------------------------
DWORD 
SC_TlsCall(
    DWORD type,
    DWORD slot
    ) 
{
    int loop, mask;
    DWORD used;
    PTHREAD pth;
    DEBUGMSG(ZONE_ENTRY,(L"SC_TlsCall entry: %8.8lx %8.8lx\r\n",type,slot));

    // We need to remove the debugchk for most DLL does TlsAlloc/TlsFree at PROCESS_ATTACH/PROCESS_DETACH.
    // It'll be okay as long as the thread doesn't perform TlsGet/TlsSet in other process. The DEBUGCHK
    // is now done in TlsGet/TlsSet.
    // PSL should NOT call TlsAlloc/TlsFree on caller thread's stack. Fail the call if it does.
    //if (pCurThread->pOwnerProc != pCurProc) {
    //    DEBUGCHK (0);
    //    KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
    //    return 0xffffffff;
    //}
    switch (type) {
        case TLS_FUNCALLOC:
            do {
                used = pCurProc->tlsLowUsed;
                for (loop = 0, mask = 1; loop < 32; loop++, mask <<= 1)
                    if (!(used & mask))
                        break;
            } while ((loop != 32) && ((DWORD)InterlockedTestExchange(&pCurProc->tlsLowUsed,used,used | mask) != used));
            if (loop == 32) {
                do {
                    used = pCurProc->tlsHighUsed;
                    for (mask = 1; loop < 64; loop++, mask <<= 1)
                        if (!(used & mask))
                            break;
                } while ((loop != 64) && ((DWORD)InterlockedTestExchange(&pCurProc->tlsHighUsed,used,used|mask) != used));
                if (loop == 64) {
                    DEBUGMSG(ZONE_ENTRY,(L"SC_TlsCall exit: %8.8lx\r\n",0xffffffff));
                    return 0xffffffff;
                }
            }
            DEBUGMSG(ZONE_ENTRY,(L"SC_TlsCall exit: %8.8lx\r\n",loop));
            return loop;
        case TLS_FUNCFREE:
            if (slot >= 64) {
                DEBUGMSG(ZONE_ENTRY,(L"SC_TlsCall exit: %8.8lx\r\n",0));
                return 0;
            }
            pth = (PTHREAD)1;
            while (pth = (PTHREAD)KCall((PKFN)ZeroAllThreadsTLS,pth,slot))
                ;
            if (slot < 32) {
                do {
                    used = pCurProc->tlsLowUsed;
                } while ((DWORD)InterlockedTestExchange(&pCurProc->tlsLowUsed,used,used & ~(1<<slot)) != used);
            } else {
                do {
                    used = pCurProc->tlsHighUsed;
                } while ((DWORD)InterlockedTestExchange(&pCurProc->tlsHighUsed,used,used & ~(1<<(slot-32))) != used);
            }
            DEBUGMSG(ZONE_ENTRY,(L"SC_TlsCall exit: %8.8lx\r\n",1));
            return 1;
        default:
            DEBUGCHK(0);
            DEBUGMSG(ZONE_ENTRY,(L"SC_TlsCall exit: %8.8lx\r\n",0));
            return 0;
    }
}



//------------------------------------------------------------------------------
//  @func DWORD | SetProcPermissions | Sets the kernel's internal thread permissions
//  @rdesc Returns the new permission dword
//  @parm DWORD | newpermissions | the new permissions bitmask
//  @comm Sets the internal permissions bitmask for the current thread
//------------------------------------------------------------------------------
DWORD 
SC_SetProcPermissions(
    DWORD newperms
    ) 
{
    DWORD retval = 0;
    PCALLSTACK pcstk = pCurThread->pcstkTop;
    
    TRUSTED_API (L"SC_SetProcPermissions", 0);

    DEBUGMSG(ZONE_ENTRY,(L"SC_SetProcPermissions entry: %8.8lx\r\n",newperms));
    if (!newperms)
        newperms = 0xffffffff;
    else {
        // prevent user from removing kernel access
        AddAccess(&newperms, ProcArray[0].aky);
        // prevent user from removing access to his stack
        AddAccess(&newperms, pCurThread->pOwnerProc->aky);
    }

    if (!pcstk || !(pcstk->dwPrcInfo & CST_IN_KERNEL)) {
        // calling from kMode
        SWITCHKEY(retval, newperms);
    } else {
        // update pcstkTop->aky, which will be set when returning from kernel
        retval = pcstk->akyLast;
        pcstk->akyLast = newperms;
    }
    DEBUGMSG(ZONE_ENTRY,(L"SC_SetProcPermissions exit: %8.8lx\r\n",retval));
    return retval;
}



//------------------------------------------------------------------------------
//  @func DWORD | GetCurrentPermissions | Obtains the kernel's internal thread permissions dword
//  @rdesc Returns the thread permissions dword
//  @comm Obtains the internal permissions bitmask for the current thread
//------------------------------------------------------------------------------
DWORD 
SC_GetCurrentPermissions(void) 
{
    DEBUGMSG(ZONE_ENTRY,(L"SC_GetCurrentPermissions entry\r\n"));
    DEBUGMSG(ZONE_ENTRY,(L"SC_GetCurrentPermissions exit: %8.8lx\r\n",pCurThread->aky));
    return pCurThread->aky;
}

DWORD NormalBias, DaylightBias, InDaylight;



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SC_SetDaylightTime(
    DWORD dst
    ) 
{
    DEBUGMSG(ZONE_ENTRY,(L"SC_SetDaylightTime entry: %8.8lx\r\n",dst));
    InDaylight = dst;
    KInfoTable[KINX_TIMEZONEBIAS] = (InDaylight ? DaylightBias : NormalBias);
    DEBUGMSG(ZONE_ENTRY,(L"SC_SetDaylightTime exit\r\n"));
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SC_SetTimeZoneBias(
    DWORD dwBias,
    DWORD dwDaylightBias
    ) 
{
    DEBUGMSG(ZONE_ENTRY,(L"SC_SetTimeZoneBias entry: %8.8lx %8.8lx\r\n",dwBias,dwDaylightBias));
    NormalBias = dwBias;
    DaylightBias = dwDaylightBias;
    KInfoTable[KINX_TIMEZONEBIAS] = (InDaylight ? DaylightBias : NormalBias);
    DEBUGMSG(ZONE_ENTRY,(L"SC_SetTimeZoneBias exit\r\n"));
}

/* Set thread last error */



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SC_SetLastError(
    DWORD dwError
    ) 
{
    DEBUGMSG(ZONE_ENTRY,(L"SC_SetLastError entry: %8.8lx\r\n",dwError));
    pCurThread->dwLastError = dwError;
    DEBUGMSG(ZONE_ENTRY,(L"SC_SetLastError exit\r\n"));
}

/* Get thread last error */



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
SC_GetLastError(void) 
{
    DEBUGMSG(ZONE_ENTRY,(L"SC_GetLastError entry\r\n"));
    DEBUGMSG(ZONE_ENTRY,(L"SC_GetLastError exit: %8.8lx\r\n",pCurThread->dwLastError));
    return pCurThread->dwLastError;
}

/* Get thread return code */



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_ThreadGetCode(
    HANDLE hTh,
    LPDWORD lpExit
    ) 
{
    BOOL fRet;
    DEBUGMSG(ZONE_ENTRY,(L"SC_ThreadGetCode entry: %8.8lx %8.8lx\r\n",hTh,lpExit));
    if (fRet = (BOOL) SC_MapPtrWithSize (lpExit, sizeof(DWORD), hCurProc)) {
        *lpExit = GetUserInfo(hTh);
    } else {
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
    }
    DEBUGMSG(ZONE_ENTRY,(L"SC_ThreadGetCode exit: %8.8lx\r\n",TRUE));
    return fRet;
}

/* Get process return code */



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_ProcGetCode(
    HANDLE hProc,
    LPDWORD lpExit
    ) 
{
    BOOL fRet;
    DEBUGMSG(ZONE_ENTRY,(L"SC_ProcGetCode entry: %8.8lx %8.8lx\r\n",hProc,lpExit));
    if (fRet = (BOOL) SC_MapPtrWithSize (lpExit, sizeof(DWORD), hCurProc)) {
        *lpExit = GetUserInfo(hProc);
    } else {
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
    }
    DEBUGMSG(ZONE_ENTRY,(L"SC_ProcGetCode exit: %8.8lx\r\n",TRUE));
    return fRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
SC_ProcGetIndex(
    HANDLE hProc
    ) 
{
    PPROCESS pproc;
    DEBUGMSG(ZONE_ENTRY,(L"SC_ProcGetIndex entry: %8.8lx\r\n",hProc));
    if (!(pproc = HandleToProc(hProc))) {
        DEBUGMSG(ZONE_ENTRY,(L"SC_ProcGetIndex exit: %8.8lx\r\n",0xffffffff));
        return 0xffffffff;
    }
    DEBUGMSG(ZONE_ENTRY,(L"SC_ProcGetIndex exit: %8.8lx\r\n",pproc->procnum));
    return pproc->procnum;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
HANDLE
SC_ProcGetIDFromIndex(
    DWORD dwIdx
    ) 
{
    return ((dwIdx < MAX_PROCESSES) && ProcArray[dwIdx].dwVMBase)? ProcArray[dwIdx].hProc : NULL;

}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_ProcFlushICache(
    HANDLE hProc,
    LPCVOID lpBaseAddress,
    DWORD dwSize
    ) 
{
    PPROCESS pprc;
    if (!hProc || (hProc == GetCurrentProcess()))
        hProc = hCurProc;

    pprc = HandleToProc (hProc);
    if (!pprc)
        return FALSE;
    OEMCacheRangeFlush (MapPtrProc (lpBaseAddress, pprc), dwSize, CACHE_SYNC_INSTRUCTIONS|CACHE_SYNC_WRITEBACK);
    return TRUE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_ProcReadMemory(
    HANDLE hProcess,
    LPCVOID lpBaseAddress,
    LPVOID lpBuffer,
    DWORD nSize,
    LPDWORD lpNumberOfBytesRead
    ) 
{
    BOOL retval = FALSE;
    LPBYTE ptr;
    PPROCESS pproc;

    TRUSTED_API (L"SC_ProcReadMemory", FALSE);

    if (lpNumberOfBytesRead)
        *lpNumberOfBytesRead = 0;
    if (pproc = HandleToProc(hProcess)) {
        ACCESSKEY ulOldKey;
        SWITCHKEY(ulOldKey,0xffffffff);
        ptr = MapPtrProc(lpBaseAddress,pproc);
        if (!SC_IsBadPtr (0, ptr, nSize) || IsSecureVa (ptr) || IsValidKPtr (ptr)) {
            __try {
                memcpy(lpBuffer,ptr,nSize);
                if (lpNumberOfBytesRead)
                    *lpNumberOfBytesRead = nSize;
                retval = TRUE;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                SetLastError(ERROR_NOACCESS);
            }
        }
        SETCURKEY(ulOldKey);
    }
    return retval;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_ProcWriteMemory(
    HANDLE hProcess,
    LPVOID lpBaseAddress,
    LPVOID lpBuffer,
    DWORD nSize,
    LPDWORD lpNumberOfBytesWritten
    ) 
{
    BOOL retval = FALSE;
    LPBYTE ptr, ptr2, ptrend;
    PPROCESS pproc;
    DWORD bytesleft, bytestocopy;

    TRUSTED_API (L"SC_ProcWriteMemory", FALSE);

    if (lpNumberOfBytesWritten)
        *lpNumberOfBytesWritten = 0;
    if (pproc = HandleToProc(hProcess)) {
        ACCESSKEY ulOldKey;
        SWITCHKEY(ulOldKey,0xffffffff);
        lpBaseAddress = ptr = MapPtrProc(lpBaseAddress,pproc);
        if (!IsROM (ptr, nSize)) {
            __try {
                bytesleft = nSize;
                while (bytesleft) {
                    BOOL bUnlock;
                    bytestocopy = 0x10000 - ((DWORD)ptr & 0xffff);
                    if (bytestocopy > bytesleft)
                        bytestocopy = bytesleft;
                    bUnlock = LockPages(ptr,bytestocopy,0,LOCKFLAG_READ);
                    ptr2 = ptr;
                    ptrend = ptr2 + bytestocopy;
                    while (ptr2 < ptrend) {
                        MEMORY_BASIC_INFORMATION mbi;
                        if (!VirtualQuery(ptr2,&mbi,sizeof(mbi)))
                            bUnlock = FALSE;
                        else if ((mbi.Protect != PAGE_READWRITE) && (mbi.Protect != PAGE_EXECUTE_READWRITE)) {
                            DWORD dwProt;
                            dwProt = ((mbi.Protect == PAGE_EXECUTE) || (mbi.Protect == PAGE_EXECUTE_READ)) ?
                                PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
                            VirtualProtect(ptr2,PAGE_SIZE - ((DWORD)ptr2 & (PAGE_SIZE-1)), dwProt, &dwProt);
                            bUnlock = FALSE;
                        }
                        ptr2 += PAGE_SIZE - ((DWORD)ptr2 & (PAGE_SIZE-1));
                    }
                    if (bUnlock)
                        UnlockPages(ptr,bytestocopy);
                    memcpy(ptr,lpBuffer,bytestocopy);
                    ptr += bytestocopy;
                    lpBuffer = (LPVOID)((LPBYTE)lpBuffer + bytestocopy);
                    bytesleft -= bytestocopy;
                }
                if (lpNumberOfBytesWritten)
                    *lpNumberOfBytesWritten = nSize;
                SC_CacheRangeFlush (lpBaseAddress, nSize, CACHE_SYNC_DISCARD | CACHE_SYNC_INSTRUCTIONS);
                retval = TRUE;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                SetLastError(ERROR_NOACCESS);
            }
        }
        SETCURKEY(ulOldKey);
    }
    return retval;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL SC_ProcGetModInfo (HANDLE hProc, HMODULE hModule, LPMODULEINFO lpmodinfo, DWORD cb)
{
    PMODULE  pMod = (PMODULE) hModule;
    PPROCESS pprc;
    BOOL     fRet = FALSE;

    DEBUGMSG(ZONE_ENTRY,(L"SC_ProcGetModInfo entry: %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
                    hProc, hModule, lpmodinfo, cb));
    if (!hProc || (hProc == GetCurrentProcess()))
        hProc = hCurProc;

    __try {
        // validate parameters
        if ((sizeof (MODULEINFO) != cb)
            || !SC_MapPtrWithSize (lpmodinfo, cb, hCurProc)
            || (pMod && !IsValidModule (pMod))
            || !(pprc = HandleToProc (hProc))
            || (pMod && !(pMod->inuse & (1 << pprc->procnum)))) {
            KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
        } else {
            if (pMod) {
                // pMod is not NULL, return the information of the module
                lpmodinfo->lpBaseOfDll = (LPVOID) ZeroPtr (pMod->BasePtr);
                lpmodinfo->SizeOfImage = pMod->e32.e32_vsize;
                lpmodinfo->EntryPoint = (LPVOID) pMod->startip;
            } else {
                // pMod is NULL, just return the process information
                lpmodinfo->lpBaseOfDll = pprc->BasePtr;
                lpmodinfo->SizeOfImage = pprc->e32.e32_vsize;
                lpmodinfo->EntryPoint = (LPVOID) (pprc->e32.e32_sect14rva ? CTBFf : MTBFf);
            }
            fRet = TRUE;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
    }
    DEBUGMSG(ZONE_ENTRY,(L"SC_ProcGetModInfo exit: %8.8lx\r\n", fRet));
    return fRet;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL SC_ProcSetVer (HANDLE hProc, DWORD dwVersion)
{
    BOOL fRet = TRUE;
    PPROCESS pproc;
    DEBUGMSG(ZONE_ENTRY,(L"SC_ProcSetVer entry: %8.8lx %8.8lx\r\n", hProc, dwVersion));
    if (!hProc || (hProc == GetCurrentProcess()))
        hProc = hCurProc;

    if ((pproc = HandleToProc(hProc))                       // valid process
        && TestAccess (&pCurThread->aky, &pproc->aky)) {    // has permission to the process
                                                            // (need to test this because we might get here
                                                            //  when called directly from KMODE)
        pproc->e32.e32_cevermajor = (BYTE) HIWORD (dwVersion);
        pproc->e32.e32_ceverminor = (BYTE) LOWORD (dwVersion);
    } else {
        fRet = FALSE;
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
    }
    DEBUGMSG(ZONE_ENTRY,(L"SC_ProcSetVer exit: %8.8lx\r\n", fRet));
    return fRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
HANDLE 
SC_OpenProcess(
    DWORD fdwAccess,
    BOOL fInherit,
    DWORD IDProcess
    ) 
{
    if (!fInherit && (GetHandleType((HANDLE)IDProcess) == SH_CURPROC) && IncRef((HANDLE)IDProcess, pCurProc))
        return (HANDLE)IDProcess;
    SetLastError(ERROR_INVALID_PARAMETER);
    return NULL;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPBYTE 
SC_THGrow(
    THSNAP *pSnap,
    DWORD dwSize
    ) 
{
    LPBYTE lpRet = pSnap->pNextFree;

    if ((KERN_TRUST_FULL != pCurProc->bTrustLevel)
        && !SC_MapPtrWithSize (pSnap, sizeof (THSNAP), hCurProc)) {
        return NULL;
    }
    pSnap->pNextFree += dwSize;
    if (pSnap->pNextFree > pSnap->pHighReserve) {
        ERRORMSG(1,(L"THGrow: Not enough reservation for toolhelp!\r\n"));
        return 0;
    }
    if (pSnap->pNextFree > pSnap->pHighCommit) {
        DWORD cbCommit = (pSnap->pNextFree - pSnap->pHighCommit + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        if ((KERN_TRUST_FULL != pCurProc->bTrustLevel)
            && !SC_MapPtrWithSize (pSnap->pHighCommit, cbCommit, hCurProc)) {
            return NULL;
        }
        if (!VirtualAlloc(pSnap->pHighCommit, cbCommit, MEM_COMMIT, PAGE_READWRITE))
            return 0;
        pSnap->pHighCommit = (LPBYTE)((DWORD)(pSnap->pNextFree + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));
    }
    return lpRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
THGetProcs(
    THSNAP *pSnap,
    BOOL bGetHeap
    ) 
{
    TH32PROC **ppNext;
    PTHREAD pTh;
    DWORD loop;
    LPBYTE pNextFree;
    ppNext = &pSnap->pProc;
    for (loop = 0; loop < MAX_PROCESSES; loop++) {
        if (ProcArray[loop].dwVMBase && (!loop || ProcStarted(&ProcArray[loop]))) {
            if (!(*ppNext = (TH32PROC *)THGrow(pSnap,sizeof(TH32PROC))))
                return FALSE;
            (*ppNext)->procentry.dwSize = sizeof(PROCESSENTRY32);
            (*ppNext)->procentry.cntUsage = 1;
            (*ppNext)->procentry.th32ProcessID = (DWORD)ProcArray[loop].hProc;
            (*ppNext)->procentry.th32ModuleID = 0;
            (*ppNext)->procentry.cntThreads = 0;
            for (pTh = ProcArray[loop].pTh; pTh; pTh = pTh->pNextInProc)
                (*ppNext)->procentry.cntThreads++;
            (*ppNext)->procentry.th32ParentProcessID = 0;
            (*ppNext)->procentry.pcPriClassBase = THREAD_PRIORITY_NORMAL;
            (*ppNext)->procentry.dwFlags = ProcArray[loop].ZonePtr ? ProcArray[loop].ZonePtr->ulZoneMask : 0;
            (*ppNext)->procentry.th32MemoryBase = ProcArray[loop].dwVMBase;
            (*ppNext)->procentry.th32AccessKey = ProcArray[loop].aky;
            kstrcpyW((*ppNext)->procentry.szExeFile,ProcArray[loop].lpszProcName);
            if (loop && bGetHeap) {
                pNextFree = pSnap->pNextFree;
                __try {

                    if (!pGetHeapSnapshot (pSnap, 1, &(*ppNext)->pMainHeapEntry, ProcArray[loop].hProc)) {
                        (*ppNext)->procentry.th32DefaultHeapID = 0;
                        (*ppNext)->pMainHeapEntry = 0;
                        pSnap->pNextFree = pNextFree;
                    } else {
                        (*ppNext)->procentry.th32DefaultHeapID = (*ppNext)->pMainHeapEntry->heapentry.th32HeapID;
                    }
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    (*ppNext)->procentry.th32DefaultHeapID = 0;
                    (*ppNext)->pMainHeapEntry = 0;
                    pSnap->pNextFree = pNextFree;
                }
            } else {
                (*ppNext)->procentry.th32DefaultHeapID = 0;
                (*ppNext)->pMainHeapEntry = 0;
            }
            ppNext = &(*ppNext)->pNext;
        }
    }
    *ppNext = 0;
    return TRUE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
THGetMods(
    THSNAP *pSnap,
    DWORD dwProcID
    ) 
{
    TH32MOD **ppNext;
    PMODULE pMod;
    PPROCESS pProc;
    DWORD loop;
    ppNext = &pSnap->pMod;
    if (dwProcID && !(pProc = HandleToProc((HANDLE)dwProcID)))
        return FALSE;
    for (pMod = pModList; pMod; pMod = pMod->pMod) {
        if (dwProcID && !HasModRefProcPtr(pMod,pProc))
            continue;
        if (!(*ppNext = (TH32MOD *)THGrow(pSnap,sizeof(TH32MOD))))
            return FALSE;
        (*ppNext)->modentry.dwSize = sizeof(MODULEENTRY32);
        (*ppNext)->modentry.th32ModuleID = (DWORD)pMod;
        (*ppNext)->modentry.th32ProcessID = dwProcID;
        (*ppNext)->modentry.GlblcntUsage = 0;
        for (loop = 0; loop < MAX_PROCESSES; loop++)
            (*ppNext)->modentry.GlblcntUsage += pMod->refcnt[loop];
        (*ppNext)->modentry.ProccntUsage = dwProcID ? pMod->refcnt[pProc->procnum] : pMod->inuse;
        (*ppNext)->modentry.modBaseAddr = (LPBYTE)ZeroPtr(pMod->BasePtr);
        (*ppNext)->modentry.modBaseSize = pMod->e32.e32_vsize;
        (*ppNext)->modentry.hModule = (HMODULE)pMod;
        (*ppNext)->modentry.dwFlags = pMod->ZonePtr ? pMod->ZonePtr->ulZoneMask : 0;
        kstrcpyW((*ppNext)->modentry.szModule,pMod->lpszModName);
        (*ppNext)->modentry.szExePath[0] = 0;
        ppNext = &(*ppNext)->pNext;
    }
    *ppNext = 0;
    return TRUE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
THGetThreads(
    THSNAP *pSnap,
    ACCESSKEY dwSavedCurThdAky
    ) 
{
    TH32THREAD **ppNext;
    PTHREAD pTh;
    DWORD loop, idxProc;
    ppNext = &pSnap->pThread;
    for (loop = 0; loop < MAX_PROCESSES; loop++) {
        if (ProcArray[loop].dwVMBase && (!loop || ProcStarted(&ProcArray[loop]))) {
            for (pTh = ProcArray[loop].pTh; pTh; pTh = pTh->pNextInProc) {
                if (!(*ppNext = (TH32THREAD *)THGrow(pSnap,sizeof(TH32THREAD))))
                    return FALSE;

                // test if the thread handle is valid
                if (HandleToThread (pTh->hTh) == pTh) {
                    (*ppNext)->threadentry.dwSize = sizeof(THREADENTRY32);
                    (*ppNext)->threadentry.cntUsage = 1;
                    (*ppNext)->threadentry.th32ThreadID = (DWORD)pTh->hTh;
                    (*ppNext)->threadentry.th32OwnerProcessID = (DWORD)ProcArray[loop].hProc;
                    (*ppNext)->threadentry.tpBasePri = GET_BPRIO(pTh);
                    (*ppNext)->threadentry.tpDeltaPri = GET_BPRIO(pTh) - GET_CPRIO(pTh);
                    (*ppNext)->threadentry.dwFlags = GET_SLEEPING(pTh) ? 4 + (pTh->lpProxy != 0) + (pTh->bSuspendCnt != 0) * 2 : GET_RUNSTATE(pTh);
                    idxProc = ((DWORD) pTh->pProc - (DWORD) &ProcArray[0]) / sizeof (ProcArray[0]);
                    (*ppNext)->threadentry.th32CurrentProcessID = (idxProc < MAX_PROCESSES)? (DWORD)ProcArray[idxProc].hProc : 0;
                    (*ppNext)->threadentry.th32AccessKey = (pTh == pCurThread) ? dwSavedCurThdAky : pTh->aky;
                    ppNext = &(*ppNext)->pNext;
                }
            }
        }
    }
    *ppNext = 0;
    return TRUE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
THGetHeaps(
    THSNAP *pSnap,
    DWORD dwProcID
    ) 
{
    LPBYTE pNextFree = pSnap->pNextFree;
    if ((HANDLE)dwProcID == ProcArray[0].hProc)
        pSnap->pHeap = 0;
    else {
        __try {
            if (!pGetHeapSnapshot (pSnap, 0, &pSnap->pHeap, (HANDLE) dwProcID))
                return FALSE;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            pSnap->pNextFree = pNextFree;
            pSnap->pHeap = 0;
        }
    }
    return TRUE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
THSNAP *
SC_THCreateSnapshot(
    DWORD dwFlags,
    DWORD dwProcID
    ) 
{
    ACCESSKEY ulOldKey;
    THSNAP *pSnap;
    if (!dwProcID || (dwProcID == (DWORD)GetCurrentProcess()))
        dwProcID = (DWORD)hCurProc;
    else if (GetHandleType((HANDLE)dwProcID) != SH_CURPROC) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }
    if (!(pSnap = VirtualAlloc(0, THSNAP_RESERVE, MEM_RESERVE, PAGE_NOACCESS))) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return INVALID_HANDLE_VALUE;
    }
    pSnap = MapPtr(pSnap);
    if (!VirtualAlloc(pSnap,PAGE_SIZE, MEM_COMMIT, PAGE_READWRITE)) {
        VirtualFree(pSnap,0,MEM_RELEASE);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return INVALID_HANDLE_VALUE;
    }
    pSnap->pNextFree = (LPBYTE)pSnap + sizeof(THSNAP);
    pSnap->pHighCommit = (LPBYTE)pSnap + PAGE_SIZE;
    pSnap->pHighReserve = (LPBYTE)pSnap + THSNAP_RESERVE;
    SWITCHKEY(ulOldKey,0xffffffff);
    EnterCriticalSection(&LLcs);
    if (((dwFlags & TH32CS_SNAPPROCESS) && !THGetProcs(pSnap,(dwFlags & TH32CS_SNAPNOHEAPS ? 0 : 1))) ||
        ((dwFlags & TH32CS_SNAPMODULE) && !THGetMods(pSnap,(dwFlags & TH32CS_GETALLMODS) ? 0 : dwProcID)) ||
        ((dwFlags & TH32CS_SNAPTHREAD) && !THGetThreads(pSnap, ulOldKey)) ||
        ((dwFlags & TH32CS_SNAPHEAPLIST) && !THGetHeaps(pSnap,dwProcID))) {
        VirtualFree(pSnap,THSNAP_RESERVE,MEM_DECOMMIT);
        VirtualFree(pSnap,0,MEM_RELEASE);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        pSnap = INVALID_HANDLE_VALUE;
    }
    LeaveCriticalSection(&LLcs);
    SETCURKEY(ulOldKey);
    return pSnap;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
SC_GetProcessVersion(
    DWORD dwProcessId
    ) 
{
    PPROCESS pProc;
    if (!dwProcessId)
        pProc = pCurProc;
    else if (!(pProc = HandleToProc((HANDLE)dwProcessId))) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    return MAKELONG(pProc->e32.e32_ceverminor,pProc->e32.e32_cevermajor);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_SetStdioPathW(
    DWORD id,
    LPCWSTR pwszPath
    ) 
{
    LPName pName, pName2;
    DWORD len;
    if ((id >= 3) || (pwszPath && ((len = strlenW(pwszPath)) > MAX_PATH-1))) {
        KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (pwszPath) {
        if (!(pName = AllocName(sizeof(WCHAR)*(len+1)))) {
            KSetLastError(pCurThread,ERROR_OUTOFMEMORY);
            return FALSE;
        }
        memcpy(pName->name,pwszPath,sizeof(WCHAR)*(len+1));
    } else
        pName = 0;
    EnterCriticalSection(&NameCS);
    pName2 = pCurProc->pStdNames[id];
    pCurProc->pStdNames[id] = pName;
    LeaveCriticalSection(&NameCS);
    if (pName2)
        FreeName(pName2);
    return TRUE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_GetStdioPathW(
    DWORD id,
    PWSTR pwszBuf,
    LPDWORD lpdwLen
    ) 
{
    DWORD len;
    Name name;
    if ((id >= 3)
        || !lpdwLen
        || ((KERN_TRUST_FULL != pCurProc->bTrustLevel) && !SC_MapPtrWithSize (pwszBuf, *lpdwLen, hCurProc))) {
        KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    LockPages(&name,sizeof(name),0,LOCKFLAG_WRITE);
    EnterCriticalSection(&NameCS);
    if (!(pCurProc->pStdNames[id]))
        len = 0;
    else {
        len = strlenW(pCurProc->pStdNames[id]->name)+1;
        memcpy(name.name,pCurProc->pStdNames[id]->name,sizeof(WCHAR)*len);
    }
    LeaveCriticalSection(&NameCS);
    UnlockPages(&name,sizeof(name));
    if (*lpdwLen < len) {
        *lpdwLen = len;
        KSetLastError(pCurThread,ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }
    *lpdwLen = len;
    memcpy(pwszBuf,name.name,sizeof(WCHAR)*len);
    return TRUE;
}

BOOL (*pQueryPerformanceCounter)(LARGE_INTEGER *lpPerformanceCount);
BOOL (*pQueryPerformanceFrequency)(LARGE_INTEGER *lpPerformanceFrequency);



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_QueryPerformanceCounter(
    LARGE_INTEGER *lpPerformanceCount
    ) 
{
    if ((KERN_TRUST_FULL != pCurProc->bTrustLevel)
        && !SC_MapPtrWithSize (lpPerformanceCount, sizeof (LARGE_INTEGER), hCurProc)) {
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (pQueryPerformanceCounter)
        return pQueryPerformanceCounter(lpPerformanceCount);
    lpPerformanceCount->HighPart = 0;
    lpPerformanceCount->LowPart = CurMSec;
    return TRUE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_QueryPerformanceFrequency(
    LARGE_INTEGER *lpFrequency
    ) 
{
    if ((KERN_TRUST_FULL != pCurProc->bTrustLevel)
        && !SC_MapPtrWithSize (lpFrequency, sizeof (LARGE_INTEGER), hCurProc)) {
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (pQueryPerformanceFrequency)
        return pQueryPerformanceFrequency(lpFrequency);
    lpFrequency->HighPart = 0;
    lpFrequency->LowPart = 1000;
    return TRUE;
}

DWORD (*pReadRegistryFromOEM)(DWORD dwFlags, LPBYTE pBuf, DWORD len);
BOOL (*pWriteRegistryToOEM)(DWORD dwFlags, LPBYTE pBuf, DWORD len);



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
SC_ReadRegistryFromOEM(
    DWORD dwFlags,
    LPBYTE pBuf,
    DWORD len
    ) 
{
    TRUSTED_API (L"SC_ReadRegistryFromOEM", 0);
    if (pReadRegistryFromOEM)
        return pReadRegistryFromOEM(dwFlags, pBuf, len);
    return 0;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_WriteRegistryToOEM(
    DWORD dwFlags,
    LPBYTE pBuf,
    DWORD len
    ) 
{
    TRUSTED_API (L"SC_WriteRegistryToOEM", FALSE);
    if (dwFlags == REG_WRITE_BYTES_PROBE)
        return (pWriteRegistryToOEM ? TRUE : FALSE);
    if (pWriteRegistryToOEM)
        return pWriteRegistryToOEM(dwFlags, pBuf, len);
    return 0;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD DoGetModuleFileName (HMODULE hModule, LPWSTR lpFilename, DWORD nSize)
{
    PPROCESS pProc;
    openexe_t *oe;
    CEOIDINFOEX ceoi;
    LPWSTR pstr;
    int loop;

    if (!hModule)
        oe = &pCurProc->oe;
    else if (pProc = HandleToProc((HANDLE)hModule))
        oe = &pProc->oe;
    else if (IsValidModule((PMODULE)hModule))
        oe = &(((LPMODULE)hModule)->oe);
    else {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    if (!GetNameFromOE(&ceoi, oe)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    for (pstr = ceoi.infFile.szFileName, loop = 1; (loop < (int)nSize) && *pstr; loop++)
        *lpFilename++ = *pstr++;
    *lpFilename = 0;

    return loop-1;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
SC_GetModuleFileNameW(
    HMODULE hModule,
    LPWSTR lpFilename,
    DWORD nSize
    ) 
{

    if (!nSize
        || ((KERN_TRUST_FULL != pCurProc->bTrustLevel) && !SC_MapPtrWithSize (lpFilename, nSize, hCurProc))) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    return DoGetModuleFileName (hModule, lpFilename, nSize);
}



void SC_CacheRangeFlush (LPVOID pAddr, DWORD dwLength, DWORD dwFlags)
{
    OEMCacheRangeFlush (MapPtrProc (pAddr, pCurProc), dwLength, dwFlags);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SC_RegisterGwesHandler(
    LPVOID pfn
    ) 
{
    TRUSTED_API_VOID (L"SC_RegisterGwesHandler");
    pGwesHandler = pfn;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
VOID 
xxx_RaiseException(
    DWORD dwExceptionCode,
    DWORD dwExceptionFlags,
    DWORD cArgs,
    CONST DWORD *lpArgs
    ) 
{
    RaiseException(dwExceptionCode, dwExceptionFlags, cArgs, lpArgs);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
DoMapPtr(
    LPVOID *P
    ) 
{
    if (((DWORD)*P >= 0x10000) && !((DWORD)*P >> VA_SECTION))
        *P = (LPVOID)((DWORD)*P | (DWORD)pCurProc->dwVMBase);
}
#define MapArgPtr(P) DoMapPtr((LPVOID *)&(P))



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Special SwitchToProc to switch into NK
void
SwitchToKernel(
    PCALLSTACK pcstk
    )
{
    pcstk->pprcLast = pCurProc;
    pcstk->akyLast = CurAKey;
    pcstk->retAddr = 0;
    pcstk->dwPrevSP = 0;
    pcstk->pcstkNext = pCurThread->pcstkTop;
    pcstk->dwPrcInfo = CST_IN_KERNEL;
    pCurThread->pcstkTop = pcstk;
    pCurThread->pProc = &ProcArray[0];
    SetCPUASID(pCurThread);
    UpdateCallerInfo (pCurThread, TRUE);
    randdw1 = ((randdw1<<5) | (randdw1>>27)) ^ (CurMSec & 0x1f);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
const CINFO *
SwitchToProc(
    PCALLSTACK pcstk,
    DWORD dwAPISet
    ) 
{
    const CINFO *pci;
    if (!(pci = SystemAPISets[dwAPISet]))
        RaiseException(STATUS_INVALID_SYSTEM_SERVICE, EXCEPTION_NONCONTINUABLE, 0, 0);
    DEBUGCHK(pci->pServer); // Should use SwitchToKernel instead
    pcstk->pprcLast = pCurProc;
    pcstk->akyLast = CurAKey;
    pcstk->retAddr = 0;
    pcstk->dwPrevSP = 0;
    pcstk->pcstkNext = pCurThread->pcstkTop;
    pcstk->dwPrcInfo = 0;
    pCurThread->pcstkTop = pcstk;
    pCurThread->pProc = pci->pServer;
    AddAccess(&pCurThread->aky, pci->pServer->aky);
    SetCPUASID(pCurThread);
    UpdateCallerInfo (pCurThread, TRUE);
    randdw1 = ((randdw1<<5) | (randdw1>>27)) ^ (CurMSec & 0x1f);
    SetNKCallOut (pCurThread);
    return pci;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
const CINFO *
SwitchToProcHandle(
    PCALLSTACK pcstk,
    HANDLE *ph,
    DWORD dwType
    ) 
{
    const CINFO *pci;
    const HDATA *phd;
    phd = HandleToPointer(*ph);
    if (!phd || !TestAccess(&phd->lock, &pCurThread->aky))
        return 0;
    pci = phd->pci;
    if (pci->type != dwType)
        return 0;
    *ph = (HANDLE)phd->pvObj;
    pcstk->pprcLast = pCurProc;
    pcstk->akyLast = CurAKey;
    pcstk->retAddr = 0;
    pcstk->dwPrevSP = 0;
    pcstk->dwPrcInfo = 0;
    pcstk->pcstkNext = pCurThread->pcstkTop;
    pCurThread->pcstkTop = pcstk;
    pCurThread->pProc = pci->pServer;
    AddAccess(&pCurThread->aky, pci->pServer->aky);
    SetCPUASID(pCurThread);
    UpdateCallerInfo (pCurThread, TRUE);
    randdw1 = ((randdw1<<5) | (randdw1>>27)) ^ (CurMSec & 0x1f);
    SetNKCallOut (pCurThread);
    return pci;
}

extern BOOL TryCloseMappedHandle (HANDLE h);
extern BOOL DoCloseAPIHandle (HANDLE h);


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
const CINFO *
SwitchToProcHandleClosing(
    PCALLSTACK pcstk,
    HANDLE hObj,
    LPBOOL bRet
    ) 
{
    const CINFO *pci;
    const HDATA *phd;
    phd = HandleToPointer(hObj);
    if (!phd)
        return 0;
    pci = phd->pci;
    if (!TestAccess(&phd->lock, &pCurProc->aky)) {
        if ((phd->lock == 1) && (phd->ref.count == 1)) {
            *bRet = TryCloseMappedHandle(hObj);
        }
        return 0;
    }
    if (pci->disp == DISPATCH_KERNEL_PSL) {
        *bRet = (*(BOOL (*)(HANDLE))(pci->ppfnMethods[0]))(hObj);
        return 0;
    }

    pcstk->pprcLast = pCurProc;
    pcstk->akyLast = CurAKey;
    pcstk->retAddr = 0;
    pcstk->dwPrevSP = 0;
    pcstk->pcstkNext = pCurThread->pcstkTop;
    pcstk->dwPrcInfo = 0;
    pCurThread->pcstkTop = pcstk;
    pCurThread->pProc = pci->pServer;
    AddAccess(&pCurThread->aky, pci->pServer->aky);
    SetCPUASID(pCurThread);
    UpdateCallerInfo (pCurThread, TRUE);
    randdw1 = ((randdw1<<5) | (randdw1>>27)) ^ (CurMSec & 0x1f);
    SetNKCallOut (pCurThread);
    return pci;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SwitchBack() 
{
    PCALLSTACK pcstk;
    pcstk = pCurThread->pcstkTop;
    pCurThread->pcstkTop = pcstk->pcstkNext;
    SETCURKEY(pcstk->akyLast);
    pCurThread->pProc = pcstk->pprcLast;
    SetCPUASID(pCurThread);
    UpdateCallerInfo (pCurThread, TRUE);
    ClearNKCallOut (pCurThread);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
PerformCallBack4Int(
    CALLBACKINFO *pcbi,
    LPVOID p2,
    LPVOID p3,
    LPVOID p4
    ) 
{
    CALLSTACK cstk, *pcstk;
    DWORD dwRet, dwFault = 0;
    PPROCESS pProc;
    if (!(pProc = HandleToProc(pcbi->hProc))) {
        RaiseException(STATUS_INVALID_PARAMETER, EXCEPTION_NONCONTINUABLE, 0, 0);
        return 0;
    }

    //DEBUGCHK (bAllKMode || (pProc == &ProcArray[0]));
    cstk.pprcLast = pCurProc;
    cstk.akyLast = CurAKey;
    cstk.retAddr = 0;
    cstk.pcstkNext = pCurThread->pcstkTop;
    cstk.dwPrevSP = 0;
    cstk.dwPrcInfo = 0;
    pCurThread->pProc = pProc;
    pCurThread->pcstkTop = &cstk;

    if (pProc != &ProcArray[0]) {
        pcstk = &cstk;
        while ((pcstk = (PCALLSTACK)((ulong)pcstk->pcstkNext&~1)) && (pcstk->pprcLast != pProc))
            ;
        if (pcstk) {
            pCurThread->aky = pcstk->akyLast;
        } else {
            pCurThread->aky = pCurThread->pOwnerProc->aky;
            AddAccess(&pCurThread->aky, ProcArray[0].aky);
            AddAccess(&pCurThread->aky, pProc->aky);
        }
    }
    randdw1 = ((randdw1<<5) | (randdw1>>27)) ^ (CurMSec & 0x1f);
    SetCPUASID(pCurThread);

    UpdateCallerInfo (pCurThread, TRUE);    // update caller info

    try {
        dwRet = (*(DWORD (*)(LPVOID, LPVOID, LPVOID, LPVOID))(ZeroPtr(pcbi->pfn)))(pcbi->pvArg0,p2,p3,p4);
    } except (EXCEPTION_EXECUTE_HANDLER) {
        dwFault = 1;
        dwRet = 0; // set return value
    }

    SwitchBack();
    if (dwFault)
        RaiseException(STATUS_INVALID_PARAMETER, EXCEPTION_NONCONTINUABLE, 0, 0);
    return dwRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
SC_PerformCallBack4(
    CALLBACKINFO *pcbi,
    LPVOID p2,
    LPVOID p3,
    LPVOID p4
    ) 
{
    if (pCurProc->bTrustLevel == KERN_TRUST_FULL)
        return PerformCallBack4Int(pcbi,p2,p3,p4);
    RaiseException(STATUS_INVALID_PARAMETER, EXCEPTION_NONCONTINUABLE, 0, 0);
    return 0;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
HGDIOBJ 
SC_SelectObject(
    HANDLE hDC,
    HANDLE hObj
    ) 
{
    CALLSTACK cstk;
    HGDIOBJ hRet;
    const CINFO *pci;
    pci = SwitchToProc(&cstk,SH_GDI);
    try {
        hRet = (*(HGDIOBJ (*)(HANDLE,HANDLE))(pci->ppfnMethods[SELECTOBJECT]))(hDC,hObj);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        hRet = 0;
    }
    SwitchBack();
    return hRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_PatBlt(
    HDC hdcDest,
    int nXLeft,
    int nYLeft,
    int nWidth,
    int nHeight,
    DWORD dwRop
    ) 
{
    CALLSTACK cstk;
    BOOL bRet = 0;
    const CINFO *pci;
    pci = SwitchToProc(&cstk,SH_GDI);
    try {
        bRet = (*(BOOL (*)(HDC,int,int,int,int,DWORD))(pci->ppfnMethods[PATBLT]))(hdcDest,nXLeft,nYLeft,nWidth,nHeight,dwRop);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        bRet = 0;
    }
    SwitchBack();
    return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_GetTextExtentExPointW(
    HDC hdc,
    LPCWSTR lpszStr,
    int cchString,
    int nMaxExtent,
    LPINT lpnFit,
    LPINT alpDx,
    LPSIZE lpSize
    ) 
{
    CALLSTACK cstk;
    BOOL bRet;
    const CINFO *pci;
    MapArgPtr(lpszStr);
    MapArgPtr(lpnFit);
    MapArgPtr(alpDx);
    MapArgPtr(lpSize);
    pci = SwitchToProc(&cstk,SH_GDI);
    try {
        bRet = (*(BOOL (*)(HDC,LPCWSTR,int,int,LPINT,LPINT,LPSIZE))(pci->ppfnMethods[GETTEXTEXTENTEXPOINTW]))(hdc,lpszStr,cchString,nMaxExtent,lpnFit,alpDx,lpSize);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        bRet = 0;
    }
    SwitchBack();
    return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
HBRUSH 
SC_GetSysColorBrush(
    int nIndex
    ) 
{
    CALLSTACK cstk;
    HBRUSH hRet ;
    const CINFO *pci;
    pci = SwitchToProc(&cstk,SH_GDI);
    try {
        hRet = (*(HBRUSH (*)(int))(pci->ppfnMethods[GETSYSCOLORBRUSH]))(nIndex);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        hRet = 0;
    }
    SwitchBack();
    return hRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
COLORREF 
SC_SetBkColor(
    HDC hDC,
    COLORREF dwColor
    ) 
{
    CALLSTACK cstk;
    COLORREF cr ;
    const CINFO *pci;
    pci = SwitchToProc(&cstk,SH_GDI);
    try {
        cr = (*(COLORREF (*)(HDC,COLORREF))(pci->ppfnMethods[SETBKCOLOR]))(hDC,dwColor);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        cr = 0;
    }
    SwitchBack();
    return cr;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
HWND 
SC_GetParent(
    HWND hwnd
    ) 
{
    CALLSTACK cstk;
    HWND hRet ;
    const CINFO *pci;
    pci = SwitchToProc(&cstk,SH_WMGR);
    try {
        hRet = (*(HWND (*)(HWND))(pci->ppfnMethods[MID_GetParent]))(hwnd);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        hRet = 0;
    }
    SwitchBack();
    return hRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_ClientToScreen(
    HWND hwnd,
    LPPOINT lpPoint
    ) 
{
    CALLSTACK cstk;
    BOOL bRet ;
    const CINFO *pci;
    MapArgPtr(lpPoint);
    pci = SwitchToProc(&cstk,SH_WMGR);
    try {
        bRet = (*(BOOL (*)(HWND, LPPOINT))(pci->ppfnMethods[MID_ClientToScreen]))(hwnd,lpPoint);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        bRet = 0;
    }
    SwitchBack();
    return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LRESULT 
SC_DefWindowProcW(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam
    ) 
{
    CALLSTACK cstk;
    LRESULT lRet ;
    const CINFO *pci;
    pci = SwitchToProc(&cstk,SH_WMGR);
    try {
        lRet = (*(LRESULT (*)(HWND, UINT, WPARAM, LPARAM))(pci->ppfnMethods[MID_DefWindowProcW]))(hwnd,msg,wParam,lParam);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        lRet = 0;
    }
    SwitchBack();
    return lRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_GetClipCursor(
    LPRECT lpRect
    ) 
{
    CALLSTACK cstk;
    BOOL bRet;
    const CINFO *pci;
    MapArgPtr(lpRect);
    pci = SwitchToProc(&cstk,SH_WMGR);
    try {
        bRet = (*(BOOL (*)(LPRECT))(pci->ppfnMethods[MID_GetClipCursor]))(lpRect);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        bRet = 0;
    }
    SwitchBack();
    return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
HDC 
SC_GetDC (
    HWND hwnd
    ) 
{
    CALLSTACK cstk;
    HDC hRet;
    const CINFO *pci;
    pci = SwitchToProc(&cstk,SH_WMGR);
    try {
        hRet = (*(HDC (*)(HWND))(pci->ppfnMethods[MID_GetDC]))(hwnd);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        hRet = 0;
    }
    SwitchBack();
    return hRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
HWND 
SC_GetFocus() 
{
    CALLSTACK cstk;
    HWND hRet;
    const CINFO *pci;
    pci = SwitchToProc(&cstk,SH_WMGR);
    try {
        hRet = (*(HWND (*)())(pci->ppfnMethods[MID_GetFocus]))();
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        hRet = 0;
    }
    SwitchBack();
    return hRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_GetMessageW(
    PMSG pMsgr,
    HWND hwnd,
    UINT wMsgFilterMin,
    UINT wMsgFilterMax
    ) 
{
    CALLSTACK cstk;
    BOOL bRet;
    const CINFO *pci;
    MapArgPtr(pMsgr);
    pci = SwitchToProc(&cstk,SH_WMGR);
    try {
        bRet = (*(BOOL (*)(PMSG,HWND,UINT,UINT))(pci->ppfnMethods[MID_GetMessageW]))(pMsgr,hwnd,wMsgFilterMin,wMsgFilterMax);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        bRet = 0;
    }
    SwitchBack();
    return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
HWND 
SC_GetWindow(
    HWND hwnd,
    UINT uCmd
    ) 
{
    CALLSTACK cstk;
    HWND hRet;
    const CINFO *pci;
    pci = SwitchToProc(&cstk,SH_WMGR);
    try {
        hRet = (*(HWND (*)(HWND,UINT))(pci->ppfnMethods[MID_GetWindow]))(hwnd,uCmd);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        hRet = 0;
    }
    SwitchBack();
    return hRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_PeekMessageW(
    PMSG pMsg,
    HWND hWnd,
    UINT wMsgFilterMin,
    UINT wMsgFilterMax,
    UINT wRemoveMsg
    ) 
{
    CALLSTACK cstk;
    BOOL bRet;
    const CINFO *pci;
    MapArgPtr(pMsg);
    pci = SwitchToProc(&cstk,SH_WMGR);
    try {
        bRet = (*(BOOL (*)(PMSG,HWND,UINT,UINT,UINT))(pci->ppfnMethods[MID_PeekMessageW]))(pMsg,hWnd,wMsgFilterMin,wMsgFilterMax,wRemoveMsg);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        bRet = 0;
    }
    SwitchBack();
    return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_ReleaseDC(
    HWND hwnd,
    HDC hdc
    ) 
{
    CALLSTACK cstk;
    BOOL bRet;
    const CINFO *pci;
    pci = SwitchToProc(&cstk,SH_WMGR);
    try {
        bRet = (*(BOOL (*)(HWND,HDC))(pci->ppfnMethods[MID_ReleaseDC]))(hwnd,hdc);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        bRet = 0;
    }
    SwitchBack();
    return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LRESULT 
SC_SendMessageW(
    HWND hwnd,
    UINT Msg,
    WPARAM wParam,
    LPARAM lParam
    ) 
{
    CALLSTACK cstk;
    LRESULT lRet;
    const CINFO *pci;
    pci = SwitchToProc(&cstk,SH_WMGR);
    try {
        lRet = (*(LRESULT (*)(HWND,UINT,WPARAM,LPARAM))(pci->ppfnMethods[MID_SendMessageW]))(hwnd,Msg,wParam,lParam);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        lRet = 0;
    }
    SwitchBack();
    return lRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
SC_SetScrollInfo(
    HWND hwnd,
    int fnBar,
    LPCSCROLLINFO lpsi,
    BOOL fRedraw
    ) 
{
    CALLSTACK cstk;
    int iRet;
    const CINFO *pci;
    MapArgPtr(lpsi);
    pci = SwitchToProc(&cstk,SH_WMGR);
    try {
        iRet = (*(int (*)(HWND,int,LPCSCROLLINFO,BOOL))(pci->ppfnMethods[MID_SetScrollInfo]))(hwnd,fnBar,lpsi,fRedraw);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        iRet = 0;
    }
    SwitchBack();
    return iRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LONG 
SC_SetWindowLongW(
    HWND hwnd,
    int nIndex,
    LONG lNewLong
    ) 
{
    CALLSTACK cstk;
    LONG lRet;
    const CINFO *pci;
    pci = SwitchToProc(&cstk,SH_WMGR);
    try {
        lRet = (*(LONG (*)(HWND,int,LONG))(pci->ppfnMethods[MID_SetWindowLongW]))(hwnd,nIndex,lNewLong);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        lRet = 0;
    }
    SwitchBack();
    return lRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_SetWindowPos(
    HWND hwnd,
    HWND hwndInsertAfter,
    int x,
    int y,
    int dx,
    int dy,
    UINT fuFlags
    ) 
{
    CALLSTACK cstk;
    BOOL bRet;
    const CINFO *pci;
    pci = SwitchToProc(&cstk,SH_WMGR);
    try {
        bRet = (*(BOOL (*)(HWND,HWND,int,int,int,int,UINT))(pci->ppfnMethods[MID_SetWindowPos]))(hwnd,hwndInsertAfter,x,y,dx,dy,fuFlags);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        bRet = 0;
    }
    SwitchBack();
    return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
HBRUSH 
SC_CreateSolidBrush(
    COLORREF crColor
    ) 
{
    CALLSTACK cstk;
    HBRUSH hRet;
    const CINFO *pci;
    pci = SwitchToProc(&cstk,SH_GDI);
    try {
        hRet = (*(HBRUSH (*)(COLORREF))(pci->ppfnMethods[CREATESOLIDBRUSH]))(crColor);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        hRet = 0;
    }
    SwitchBack();
    return hRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_DeleteMenu(
    HMENU hmenu,
    UINT uPosition,
    UINT uFlags
    ) 
{
    CALLSTACK cstk;
    BOOL bRet;
    const CINFO *pci;
    pci = SwitchToProc(&cstk,SH_WMGR);
    try {
        bRet = (*(BOOL (*)(HMENU, UINT, UINT))(pci->ppfnMethods[MID_DeleteMenu]))(hmenu,uPosition,uFlags);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        bRet = 0;
    }
    SwitchBack();
    return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_DeleteObject(
    HGDIOBJ hObject
    ) 
{
    CALLSTACK cstk;
    BOOL bRet;
    const CINFO *pci;
    pci = SwitchToProc(&cstk,SH_GDI);
    try {
        bRet = (*(BOOL (*)(HGDIOBJ))(pci->ppfnMethods[DELETEOBJECT]))(hObject);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        bRet = 0;
    }
    SwitchBack();
    return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
SC_DrawTextW(
    HDC hdc,
    LPCWSTR lpszStr,
    int cchStr,
    RECT *lprc,
    UINT wFormat
    ) 
{
    CALLSTACK cstk;
    int iRet;
    const CINFO *pci;
    MapArgPtr(lpszStr);
    MapArgPtr(lprc);
    pci = SwitchToProc(&cstk,SH_GDI);
    try {
        iRet = (*(int (*)(HDC,LPCWSTR,int,RECT *,UINT))(pci->ppfnMethods[DRAWTEXTW]))(hdc,lpszStr,cchStr,lprc,wFormat);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        iRet = 0;
    }
    SwitchBack();
    return iRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_ExtTextOutW(
    HDC hdc,
    int X,
    int Y,
    UINT fuOptions,
    CONST RECT *lprc,
    LPCWSTR lpszString,
    UINT cbCount,
    CONST INT *lpDx
    ) 
{
    CALLSTACK cstk;
    BOOL bRet;
    const CINFO *pci;
    MapArgPtr(lprc);
    MapArgPtr(lpszString);
    MapArgPtr(lpDx);
    pci = SwitchToProc(&cstk,SH_GDI);
    try {
        bRet = (*(BOOL (*)(HDC,int,int,UINT,CONST RECT *, LPCWSTR, UINT, CONST INT *))(pci->ppfnMethods[EXTTEXTOUTW]))(hdc,X,Y,fuOptions,lprc,lpszString,cbCount,lpDx);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        bRet = 0;
    }
    SwitchBack();
    return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
SC_FillRect(
    HDC hdc,
    CONST RECT *lprc,
    HBRUSH hbr
    ) 
{
    CALLSTACK cstk;
    int iRet;
    const CINFO *pci;
    MapArgPtr(lprc);
    pci = SwitchToProc(&cstk,SH_GDI);
    try {
        iRet = (*(int (*)(HDC,CONST RECT *, HBRUSH))(pci->ppfnMethods[FILLRECT]))(hdc,lprc,hbr);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        iRet = 0;
    }
    SwitchBack();
    return iRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
SHORT 
SC_GetAsyncKeyState(
    INT vKey
    ) 
{
    CALLSTACK cstk;
    SHORT sRet;
    const CINFO *pci;
    pci = SwitchToProc(&cstk,SH_WMGR);
    try {
        sRet = (*(SHORT (*)(INT))(pci->ppfnMethods[MID_GetAsyncKeyState]))(vKey);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        sRet = 0;
    }
    SwitchBack();
    return sRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
SC_GetDlgCtrlID(
    HWND hWnd
    ) 
{
    CALLSTACK cstk;
    int iRet;
    const CINFO *pci;
    pci = SwitchToProc(&cstk,SH_WMGR);
    try {
        iRet = (*(int (*)(HWND))(pci->ppfnMethods[MID_GetDlgCtrlID]))(hWnd);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        iRet = 0;
    }
    SwitchBack();
    return iRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
HGDIOBJ 
SC_GetStockObject(
    int fnObject
    ) 
{
    CALLSTACK cstk;
    HGDIOBJ hRet;
    const CINFO *pci;
    pci = SwitchToProc(&cstk,SH_GDI);
    try {
        hRet = (*(HGDIOBJ (*)(int))(pci->ppfnMethods[GETSTOCKOBJECT]))(fnObject);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        hRet = 0;
    }
    SwitchBack();
    return hRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
SC_GetSystemMetrics(
    int nIndex
    ) 
{
    CALLSTACK cstk;
    int iRet;
    const CINFO *pci;
    pci = SwitchToProc(&cstk,SH_WMGR);
    try {
        iRet = (*(int (*)(int))(pci->ppfnMethods[MID_GetSystemMetrics]))(nIndex);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        iRet = 0;
    }
    SwitchBack();
    return iRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
ATOM 
SC_RegisterClassWStub(
    CONST WNDCLASSW *lpWndClass,
    LPCWSTR lpszClassName,
    HANDLE hprcWndProc
    ) 
{
    CALLSTACK cstk;
    ATOM aRet;
    const CINFO *pci;
    MapArgPtr(lpWndClass);
    MapArgPtr(lpszClassName);
    pci = SwitchToProc(&cstk,SH_WMGR);
    try {
        aRet = (*(ATOM (*)(CONST WNDCLASSW *, LPCWSTR, HANDLE))(pci->ppfnMethods[MID_RegisterClassWApiSetEntry]))(lpWndClass,lpszClassName,hprcWndProc);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        aRet = 0;
    }
    SwitchBack();
    return aRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
UINT 
SC_RegisterClipboardFormatW(
    LPCWSTR lpszFormat
    ) 
{
    CALLSTACK cstk;
    UINT uRet;
    const CINFO *pci;
    MapArgPtr(lpszFormat);
    pci = SwitchToProc(&cstk,SH_WMGR);
    try {
        uRet = (*(UINT (*)(LPCWSTR))(pci->ppfnMethods[MID_RegisterClipboardFormatW]))(lpszFormat);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        uRet = 0;
    }
    SwitchBack();
    return uRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
SC_SetBkMode(
    HDC hdc,
    int iBkMode
    ) 
{
    CALLSTACK cstk;
    int iRet;
    const CINFO *pci;
    pci = SwitchToProc(&cstk,SH_GDI);
    try {
        iRet = (*(int (*)(HDC,int))(pci->ppfnMethods[SETBKMODE]))(hdc,iBkMode);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        iRet = 0;
    }
    SwitchBack();
    return iRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
COLORREF 
SC_SetTextColor(
    HDC hdc,
    COLORREF crColor
    ) 
{
    CALLSTACK cstk;
    COLORREF cRet ;
    const CINFO *pci;
    pci = SwitchToProc(&cstk,SH_GDI);
    try {
        cRet = (*(COLORREF (*)(HDC,COLORREF))(pci->ppfnMethods[SETTEXTCOLOR]))(hdc,crColor);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        cRet = 0;
    }
    SwitchBack();
    return cRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_InvalidateRect(
    HWND hwnd,
    LPCRECT prc,
    BOOL fErase
    ) 
{
    CALLSTACK cstk;
    BOOL bRet;
    const CINFO *pci;
    MapArgPtr(prc);
    pci = SwitchToProc(&cstk,SH_WMGR);
    try {
        bRet = (*(BOOL (*)(HWND, LPCRECT, BOOL))(pci->ppfnMethods[MID_InvalidateRect]))(hwnd,prc,fErase);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        bRet = 0;
    }
    SwitchBack();
    return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_TransparentImage(
    HDC hdcDest,
    int nXDest,
    int nYDest,
    int nWidthDest,
    int nHeightDest,
    HANDLE hImgSrc,
    int nXSrc,
    int nYSrc,
    int nWidthSrc,
    int nHeightSrc,
    COLORREF crTransparentColor
    ) 
{
    CALLSTACK cstk;
    BOOL bRet;
    const CINFO *pci;
    pci = SwitchToProc(&cstk,SH_GDI);
    try {
        bRet = (*(BOOL (*)(HDC,int,int,int,int,HANDLE,int,int,int,int,COLORREF))(pci->ppfnMethods[TRANSPARENTIMAGE]))
            (hdcDest, nXDest, nYDest, nWidthDest, nHeightDest, hImgSrc, nXSrc, nYSrc, nWidthSrc, nHeightSrc, crTransparentColor);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        bRet = 0;
    }
    SwitchBack();
    return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_IsDialogMessageW(
    HWND hDlg,
    LPMSG lpMsg
    ) 
{
    CALLSTACK cstk;
    BOOL bRet;
    const CINFO *pci;
    MapArgPtr(lpMsg);
    pci = SwitchToProc(&cstk,SH_WMGR);
    try {
        bRet = (*(BOOL (*)(HWND,LPMSG))(pci->ppfnMethods[MID_IsDialogMessageW]))(hDlg,lpMsg);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        bRet = 0;
    }
    SwitchBack();
    return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_PostMessageW(
    HWND hWnd,
    UINT Msg,
    WPARAM wParam,
    LPARAM lParam
    ) 
{
    CALLSTACK cstk;
    BOOL bRet;
    const CINFO *pci;
    pci = SwitchToProc(&cstk,SH_WMGR);
    try {
        bRet = (*(BOOL (*)(HWND,UINT,WPARAM,LPARAM))(pci->ppfnMethods[MID_PostMessageW]))(hWnd,Msg,wParam,lParam);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        bRet = 0;
    }
    SwitchBack();
    return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_IsWindowVisible(
    HWND hWnd
    ) 
{
    CALLSTACK cstk;
    BOOL bRet;
    const CINFO *pci;
    pci = SwitchToProc(&cstk,SH_WMGR);
    try {
        bRet = (*(BOOL (*)(HWND))(pci->ppfnMethods[MID_IsWindowVisible]))(hWnd);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        bRet = 0;
    }
    SwitchBack();
    return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
SHORT 
SC_GetKeyState(
    int nVirtKey
    )
{
    CALLSTACK cstk;
    SHORT sRet;
    const CINFO *pci;
    pci = SwitchToProc(&cstk,SH_WMGR);
    try {
        sRet = (*(SHORT (*)(int))(pci->ppfnMethods[MID_GetKeyState]))(nVirtKey);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        sRet = 0;
    }
    SwitchBack();
    return sRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
HDC 
SC_BeginPaint(
    HWND hwnd,
    LPPAINTSTRUCT pps
    ) 
{
    CALLSTACK cstk;
    HDC hRet;
    const CINFO *pci;
    MapArgPtr(pps);
    pci = SwitchToProc(&cstk,SH_WMGR);
    try {
        hRet = (*(HDC (*)(HWND,LPPAINTSTRUCT))(pci->ppfnMethods[MID_BeginPaint]))(hwnd,pps);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        hRet = 0;
    }
    SwitchBack();
    return hRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_EndPaint(
    HWND hwnd,
    LPPAINTSTRUCT pps
    ) 
{
    CALLSTACK cstk;
    BOOL bRet;
    const CINFO *pci;
    MapArgPtr(pps);
    pci = SwitchToProc(&cstk,SH_WMGR);
    try {
        bRet = (*(BOOL (*)(HWND,LPPAINTSTRUCT))(pci->ppfnMethods[MID_EndPaint]))(hwnd,pps);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        (*(void (*)(void))pGwesHandler)();
        bRet = 0;
    }
    SwitchBack();
    return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LONG 
NKRegOpenKeyExW(
    HKEY hKey,
    LPCWSTR lpSubKey,
    DWORD ulOptions,
    REGSAM samDesired,
    PHKEY phkResult
    ) 
{
    CALLSTACK cstk;
    LONG lRet ;
    const CINFO *pci;
    MapArgPtr(lpSubKey);
    MapArgPtr(phkResult);
    pci = SwitchToProc(&cstk,SH_FILESYS_APIS);
    lRet = (*(LONG (*)(HKEY,LPCWSTR,DWORD,REGSAM,PHKEY))(pci->ppfnMethods[23]))(hKey,lpSubKey,ulOptions,samDesired,phkResult);
    SwitchBack();
    return lRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LONG 
NKRegCloseKey(
    HKEY hKey
    ) 
{
    CALLSTACK cstk;
    LONG lRet ;
    const CINFO *pci;
    pci = SwitchToProc(&cstk,SH_FILESYS_APIS);
    lRet = (*(LONG (*)(HKEY))(pci->ppfnMethods[17]))(hKey);
    SwitchBack();
    return lRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LONG 
NKRegQueryValueExW(
    HKEY hKey,
    LPCWSTR lpValueName,
    LPDWORD lpReserved,
    LPDWORD lpType,
    LPBYTE lpData,
    LPDWORD lpcbData
    ) 
{
    CALLSTACK cstk;
    LONG lRet ;
    const CINFO *pci;
    MapArgPtr(lpValueName);
    MapArgPtr(lpReserved);
    MapArgPtr(lpType);
    MapArgPtr(lpData);
    MapArgPtr(lpcbData);
    pci = SwitchToProc(&cstk,SH_FILESYS_APIS);
    lRet = (*(LONG (*)(HKEY,LPCWSTR,LPDWORD,LPDWORD,LPBYTE,LPDWORD))(pci->ppfnMethods[25]))(hKey,lpValueName,lpReserved,lpType,lpData,lpcbData);
    SwitchBack();
    return lRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LONG 
NKRegSetValueExW(
    HKEY    hKey,
    LPCWSTR lpValueName,
    DWORD   dwReserved,
    DWORD   dwType,
    LPBYTE  lpData,
    DWORD   cbData
    ) 
{
    CALLSTACK cstk;
    LONG lRet ;
    const CINFO *pci;
    MapArgPtr(lpValueName);
    MapArgPtr(lpData);
    pci = SwitchToProc(&cstk,SH_FILESYS_APIS);
    lRet = (*(LONG (*)(HKEY,LPCWSTR,DWORD,DWORD,LPBYTE,DWORD))(pci->ppfnMethods[26]))(hKey,lpValueName,dwReserved,dwType,lpData,cbData);
    SwitchBack();
    return lRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LONG 
NKRegCreateKeyExW(
    HKEY hKey,
    LPCWSTR lpSubKey,
    DWORD Reserved,
    LPWSTR lpClass,
    DWORD dwOptions,
    REGSAM samDesired,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    PHKEY phkResult,
    LPDWORD lpdwDisposition
    ) 
{
    CALLSTACK cstk;
    LONG lRet ;
    const CINFO *pci;
    MapArgPtr(lpSubKey);
    MapArgPtr(lpClass);
    MapArgPtr(lpSecurityAttributes);
    MapArgPtr(phkResult);
    MapArgPtr(lpdwDisposition);
    pci = SwitchToProc(&cstk,SH_FILESYS_APIS);
    lRet = (*(LONG (*)(HKEY,LPCWSTR,DWORD,LPWSTR,DWORD,REGSAM,LPSECURITY_ATTRIBUTES,PHKEY,LPDWORD))(pci->ppfnMethods[18]))(hKey,lpSubKey,Reserved,lpClass,dwOptions,samDesired,lpSecurityAttributes,phkResult,lpdwDisposition);
    SwitchBack();
    return lRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_IsSystemFile(
    LPCWSTR lpFileName
    )
{
    CALLSTACK cstk;
    BOOL fRet;
    const CINFO *pci;
    MapArgPtr(lpFileName);
    pci = SwitchToProc(&cstk,SH_FILESYS_APIS);
    fRet = (*(BOOL (*)(LPCWSTR))(pci->ppfnMethods[55]))(lpFileName);
    SwitchBack();
    return fRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
HANDLE 
SC_CreateFileW(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
    )
{
    CALLSTACK cstk;
    HANDLE h;
    const CINFO *pci;
    MapArgPtr(lpFileName);
    MapArgPtr(lpSecurityAttributes);
    pci = SwitchToProc(&cstk,SH_FILESYS_APIS);
    h = (*(HANDLE (*)(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE))(pci->ppfnMethods[9]))(lpFileName,dwDesiredAccess,dwShareMode,lpSecurityAttributes,dwCreationDisposition,dwFlagsAndAttributes,hTemplateFile);
    SwitchBack();
    return h;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_ReadFile(
    HANDLE hFile,
    LPVOID lpBuffer,
    DWORD nNumberOfBytesToRead,
    LPDWORD lpNumberOfBytesRead,
    LPOVERLAPPED lpOverlapped
    ) 
{
    CALLSTACK cstk;
    BOOL b = FALSE;
    const CINFO *pci;
    MapArgPtr(lpBuffer);
    MapArgPtr(lpNumberOfBytesRead);
    MapArgPtr(lpOverlapped);
    if (!(pci = SwitchToProcHandle(&cstk,&hFile,HT_FILE)))
        SetLastError(ERROR_INVALID_HANDLE);
    else {
        __try {
            b = (*(BOOL (*)(HANDLE,LPVOID,DWORD,LPDWORD,LPOVERLAPPED))(pci->ppfnMethods[2]))(hFile,lpBuffer,nNumberOfBytesToRead,lpNumberOfBytesRead,lpOverlapped);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_INVALID_PARAMETER);
        }
        SwitchBack();
    }
    return b;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
CEOID 
SC_CeWriteRecordProps(
    HANDLE hDbase,
    CEOID oidRecord,
    WORD cPropID,
    CEPROPVAL *rgPropVal
    ) 
{
    CALLSTACK cstk;
    CEOID oid = 0;
    const CINFO *pci;
    MapArgPtr(rgPropVal);
    if (!(pci = SwitchToProcHandle(&cstk,&hDbase,HT_DBFILE)))
        SetLastError(ERROR_INVALID_HANDLE);
    else {
        __try {
            oid = (*(CEOID (*)(HANDLE,CEOID,WORD,CEPROPVAL *))(pci->ppfnMethods[5]))(hDbase,oidRecord,cPropID,rgPropVal);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_INVALID_PARAMETER);
        }
        SwitchBack();
    }
    return oid;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_ReadFileWithSeek(
    HANDLE hFile,
    LPVOID lpBuffer,
    DWORD nNumberOfBytesToRead,
    LPDWORD lpNumberOfBytesRead,
    LPOVERLAPPED lpOverlapped,
    DWORD dwLowOffset,
    DWORD dwHighOffset
    ) 
{
    CALLSTACK cstk;
    BOOL b = 0;
    const CINFO *pci;
    MapArgPtr(lpBuffer);
    MapArgPtr(lpNumberOfBytesRead);
    MapArgPtr(lpOverlapped);
    if (!(pci = SwitchToProcHandle(&cstk,&hFile,HT_FILE)))
        SetLastError(ERROR_INVALID_HANDLE);
    else {
        __try {
            b = (*(BOOL (*)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED, DWORD, DWORD))(pci->ppfnMethods[12]))
                (hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped, dwLowOffset, dwHighOffset);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_INVALID_PARAMETER);
        }
        SwitchBack();
    }
    return b;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_WriteFileWithSeek(
    HANDLE hFile,
    LPCVOID lpBuffer,
    DWORD nNumberOfBytesToWrite,
    LPDWORD lpNumberOfBytesWritten,
    LPOVERLAPPED lpOverlapped,
    DWORD dwLowOffset,
    DWORD dwHighOffset
    ) 
{
    CALLSTACK cstk;
    BOOL b = 0;
    const CINFO *pci;
    MapArgPtr(lpBuffer);
    MapArgPtr(lpNumberOfBytesWritten);
    MapArgPtr(lpOverlapped);
    if (!(pci = SwitchToProcHandle(&cstk,&hFile,HT_FILE)))
        SetLastError(ERROR_INVALID_HANDLE);
    else {
        __try {
            b = (*(BOOL (*)(HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED, DWORD, DWORD))(pci->ppfnMethods[13]))
                (hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten, lpOverlapped, dwLowOffset, dwHighOffset);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_INVALID_PARAMETER);
        }
        SwitchBack();
    }
    return b;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_CloseHandle(
    HANDLE hObj
    ) 
{
    CALLSTACK cstk;
    BOOL b = FALSE;
    const CINFO *pci;
    if (!(pci = SwitchToProcHandleClosing (&cstk, hObj, &b))) {
        if (!b)
            SetLastError(ERROR_INVALID_HANDLE);
    } else {
        b = DoCloseAPIHandle (hObj);
        SwitchBack();
    }
    return b;
}


BOOL SC_DuplicateHandle (HANDLE hSrcProc, HANDLE hSrcHndl, HANDLE hDstProc, LPHANDLE lpDstHndl, DWORD dwAccess, BOOL bInherit, DWORD dwOptions)
{
    PPROCESS pSrcProc, pDstProc;
    PHDATA   phd;
    const CINFO *pci;

    // conver pseudo process handles to real handles
    if (GetCurrentProcess() == hSrcProc)
        hSrcProc = hCurProc;
    if (GetCurrentProcess() == hDstProc)
        hDstProc = hCurProc;

    // validate the parameters
    if (bInherit    // bInHerit must be false
        || !(DUPLICATE_SAME_ACCESS & dwOptions)         // option must contain DUPLICATE_SAME_ACCESS
        || (~(DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE) & dwOptions)  // can't have anything other than these two
        || !lpDstHndl                                   // lpDstHndl can't be NULL
        || !(pSrcProc = HandleToProc (hSrcProc))        // must be valid process handle
        || !(pDstProc = HandleToProc(hDstProc))         // must be valid process handle
        || !(phd = (PHDATA) HandleToPointer(hSrcHndl))  // must be a valid handle
        || !(pci = phd->pci)                            // must have a server
        || !TestAccess (&phd->lock, &pSrcProc->aky)     // src proc must have access to source handle
        ) {
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // verify permission if not fully trusted
    if ((KERN_TRUST_FULL != pCurProc->bTrustLevel)
        && (!SC_MapPtrWithSize (lpDstHndl, sizeof (HANDLE), hCurProc)
            || !TestAccess (&pDstProc->aky, &pCurThread->aky)
            || !TestAccess (&pSrcProc->aky, &pCurThread->aky))) {
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    switch (pci->type) {
    case HT_EVENT:
    case HT_MUTEX:
    case HT_SEMAPHORE:
        break;
    default:
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // increment the reference count of the handle
    IncRef (hSrcHndl, pDstProc);
    *lpDstHndl = hSrcHndl;

    // close source handle if specified
    if (DUPLICATE_CLOSE_SOURCE & dwOptions) {
        DecRef (hSrcHndl, pSrcProc, FALSE);
    }

    return TRUE;
}
 
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_DeviceIoControl(
    HANDLE hDevice,
    DWORD dwIoControlCode,
    LPVOID lpInBuf,
    DWORD nInBufSize,
    LPVOID lpOutBuf,
    DWORD nOutBufSize,
    LPDWORD lpBytesReturned,
    LPOVERLAPPED lpOverlapped
    ) 
{
    CALLSTACK cstk;
    BOOL b = FALSE;
    const CINFO *pci;
    MapArgPtr(lpInBuf);
    MapArgPtr(lpOutBuf);
    MapArgPtr(lpBytesReturned);
    MapArgPtr(lpOverlapped);
    if (!(pci = SwitchToProcHandle(&cstk,&hDevice,HT_FILE)))
        SetLastError(ERROR_INVALID_HANDLE);
    else {
        __try {
            b = (*(BOOL (*)(HANDLE,DWORD,LPVOID,DWORD,LPVOID,DWORD,LPDWORD,LPOVERLAPPED))(pci->ppfnMethods[11]))(hDevice,dwIoControlCode,lpInBuf,nInBufSize,lpOutBuf,nOutBufSize,lpBytesReturned,lpOverlapped);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_INVALID_PARAMETER);
        }
        SwitchBack();
    }
    return b;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
HANDLE 
SC_OpenDatabaseEx(
    PCEGUID pguid,
    PCEOID poid,
    LPWSTR lpszName,
    CEPROPID propid,
    DWORD dwFlags,
    CENOTIFYREQUEST *pReq
    ) 
{
    CALLSTACK cstk;
    HANDLE h;
    const CINFO *pci;
    MapArgPtr(pguid);
    MapArgPtr(poid);
    MapArgPtr(lpszName);
    MapArgPtr(pReq);
    pci = SwitchToProc(&cstk,SH_FILESYS_APIS);
    h = (*(HANDLE (*)(PCEGUID,PCEOID,LPWSTR,CEPROPID,DWORD,CENOTIFYREQUEST *))(pci->ppfnMethods[16]))(pguid,poid,lpszName,propid,dwFlags,pReq);
    SwitchBack();
    return h;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
CEOID 
SC_SeekDatabase(
    HANDLE hDatabase,
    DWORD dwSeekType,
    DWORD dwValue,
    WORD wNumVals,
    LPDWORD lpdwIndex
    ) 
{
    CALLSTACK cstk;
    CEOID oid = 0;
    const CINFO *pci;
    MapArgPtr(lpdwIndex);
    if (!(pci = SwitchToProcHandle(&cstk,&hDatabase,HT_DBFILE)))
        SetLastError(ERROR_INVALID_HANDLE);
    else {
        __try {
            oid = (*(CEOID (*)(HANDLE,DWORD,DWORD,WORD,LPDWORD))(pci->ppfnMethods[2]))(hDatabase,dwSeekType,dwValue,wNumVals,lpdwIndex);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_INVALID_PARAMETER);
        }
        SwitchBack();
    }
    return oid;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
CEOID 
SC_ReadRecordPropsEx (
    HANDLE hDbase,
    DWORD dwFlags,
    LPWORD lpcPropID,
    CEPROPID *rgPropID,
    LPBYTE *lplpBuffer,
    LPDWORD lpcbBuffer,
    HANDLE hHeap
    ) 
{
    CALLSTACK cstk;
    CEOID oid = 0;
    const CINFO *pci;
    MapArgPtr(lpcPropID);
    MapArgPtr(rgPropID);
    MapArgPtr(lplpBuffer);
    MapArgPtr(lpcbBuffer);
    if (!(pci = SwitchToProcHandle(&cstk,&hDbase,HT_DBFILE)))
        SetLastError(ERROR_INVALID_HANDLE);
    else {
        __try {
            oid = (*(CEOID (*)(HANDLE,DWORD,LPWORD,CEPROPID *,LPBYTE *,LPDWORD,HANDLE))(pci->ppfnMethods[4]))(hDbase,dwFlags,lpcPropID,rgPropID,lplpBuffer,lpcbBuffer,hHeap);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_INVALID_PARAMETER);
        }
        SwitchBack();
    }
    return oid;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPBYTE 
SC_CreateLocaleView(
    BOOL bFirst
    ) 
{
    HANDLE hNLS, hNLSMap;
    LPBYTE lpNLSView;

    if (bFirst) {
        TRUSTED_API (L"SC_CreateLocaleView", NULL);
        hNLS = CreateFileForMapping(TEXT("\\windows\\wince.nls"), GENERIC_READ,
                                    0, 0, OPEN_EXISTING, 0, 0);
        DEBUGCHK(hNLS != INVALID_HANDLE_VALUE);
        hNLSMap = CreateFileMapping(hNLS, 0, PAGE_READONLY, 0, 0,
                                    TEXT("NLSFILE"));
    } else {
        hNLSMap = CreateFileMapping(INVALID_HANDLE_VALUE, 0, PAGE_READONLY, 0, 0,
                                    TEXT("NLSFILE"));
    }
    DEBUGCHK(hNLSMap);
    lpNLSView = MapViewOfFile(hNLSMap, FILE_MAP_READ, 0, 0, 0);
    DEBUGCHK(lpNLSView);

    return lpNLSView;
}

