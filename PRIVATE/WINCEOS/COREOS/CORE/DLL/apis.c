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

#define EDB 1
#include "windows.h"
#include "nkintr.h"
#include "bldver.h"
#include "kernel.h"
#include "edbaccess.h"
#include "cnnclpth.h"
#include "errorrep.h"

#undef bIntrIndexHigh
#undef bIntrNumber

extern PEDBACCESS pEDBAccess;


DWORD IsExiting;
DWORD fIsDying;

#include "cscode.c"

extern HANDLE hInstCoreDll;
BOOL Imm_DllEntry(HANDLE hinstDll, DWORD dwReason, LPVOID lpvReserved);
BOOL WINAPI xxx_CloseHandle(HANDLE hObject);
BOOL WINAPI CoreDllInit (HANDLE  hinstDLL, DWORD fdwReason, LPVOID lpvReserved);

CRITICAL_SECTION ProcCS;

static void ReleaseProcCS ()
{
    HANDLE hCurrentThread = (HANDLE) GetCurrentThreadId ();

    if (hCurrentThread == ProcCS.OwnerThread) {
        // if we reach here, we're being terminated while calling DllMain
        // need to clean up the module list
        GetProcModList (NULL, 0);

        do {
            LeaveCriticalSection (&ProcCS);
        } while (hCurrentThread && (hCurrentThread == ProcCS.OwnerThread));
    }
}

typedef BOOL (*comentry_t)(HANDLE,DWORD,LPVOID,LPVOID,DWORD,DWORD);
typedef BOOL (*dllntry_t)(HANDLE,DWORD,LPVOID);

BOOL CallEntry (PDLLMAININFO pInfo, DWORD dwReason)
{
    return pInfo->dwSect14rva
        ? ((comentry_t)pInfo->pDllMain) (pInfo->hLib, dwReason, (LPVOID)IsExiting, pInfo->pBasePtr, pInfo->dwSect14rva, pInfo->dwSect14size)
        : ((dllntry_t)pInfo->pDllMain) (pInfo->hLib, dwReason, (LPVOID)IsExiting);
}

DWORD _CallDllMains (PDLLMAININFO pList, DWORD dwCnt, DWORD dwReason)
{
    // call all the DllMains one by one
    DWORD           dwErr = 0;
    PDLLMAININFO    pTrav;
    DEBUGCHK ((HANDLE) GetCurrentThreadId () == ProcCS.OwnerThread);
    for (pTrav = pList ; dwCnt --; pTrav ++) {
        __try {
            if (!CallEntry (pTrav, dwReason)) {
                dwErr = ERROR_DLL_INIT_FAILED;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            dwErr = ERROR_DLL_INIT_FAILED;
        }

        // don't continue process-attach if error
        if (dwErr && (DLL_PROCESS_ATTACH == dwReason)) {
            // fail to load libries, call DLL_PROCESS_DETACH on the modules
            // that we've called so far in reverse order
            while (pTrav -- > pList) {
                __try {
                    CallEntry (pTrav, DLL_PROCESS_DETACH);
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                }
            }

            break;
        }
    }
    return dwErr;
}

DWORD CallDllMains (DWORD dwCnt, DWORD dwReason)
{
    DEBUGCHK ((HANDLE) GetCurrentThreadId () == ProcCS.OwnerThread);
    if (dwCnt) {
        DWORD dwErr = ERROR_DLL_INIT_FAILED;
        PDLLMAININFO pList;
        DEBUGCHK ((DWORD) ProcCS.OwnerThread == GetCurrentThreadId ());
        if ((pList = (PDLLMAININFO) _alloca (dwCnt * sizeof (DLLMAININFO)))
            && GetProcModList (pList, dwCnt)) {
            dwErr = _CallDllMains (pList, dwCnt, dwReason);

            // call all the DllMains one by one
            if (DLL_PROCESS_DETACH == dwReason) {
                FreeModFromCurrProc (pList, dwCnt);
            }
        }
        return dwErr;
    }
    return 0;
}

/*
    @doc BOTH EXTERNAL
    
    @func BOOL | IsAPIReady | Tells whether the specified API set has been registered
    @parm DWORD | hAPI | The predefined system handle of the desired API set (from syscall.h)
    @comm During system initialization, some of the components may rely on other
          components that are not yet loaded.  IsAPIReady can be used to check
          if the desired API set is available and thus avoid taking an exception.
*/
BOOL
IsAPIReady(
    DWORD hAPI
    )
{
    if (hAPI > NUM_SYS_HANDLES)
        return FALSE;
    return (UserKInfo[KINX_API_MASK] & (1 << hAPI)) != 0;
}

BOOL CeSafeCopyMemory (LPVOID pDst, LPCVOID pSrc, DWORD cbSize)
{
    __try {
        memcpy (pDst, pSrc, cbSize);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return FALSE;
    }
    return TRUE;
}

#if defined(x86)
void DoPslFuncCall(DWORD flags, DWORD proc, DWORD thread, DWORD index) {
    IMPLICIT_DECL(void, index, 0, (DWORD,DWORD,DWORD))(flags,proc,thread);
}
#endif

void PSLNotify(DWORD flags, DWORD proc, DWORD thread) {
    DWORD loop = NUM_SYS_HANDLES;
    while (--loop >= SH_LAST_NOTIFY)
        if (UserKInfo[KINX_API_MASK] & (1 << loop)) {
#if defined(x86)
            DoPslFuncCall(flags,proc,thread,loop);
#else
            IMPLICIT_DECL(void, loop, 0, (DWORD,DWORD,DWORD))(flags,proc,thread);
#endif
        }
}

// @func int | GetAPIAddress | Find API function address
// @rdesc Returns the function address of the requested API (0 if not valid)
// @parm int | setId | API Set index (via QueryAPISetID)
// @parm int | iMethod | method # within the API Set
// @comm Returns the kernel trap address used to invoke the given method within
// the given API Set.

FARPROC GetAPIAddress(int setId, int iMethod)
{
    if (!IsAPIReady((DWORD)setId) || (iMethod > METHOD_MASK))
        return 0;
    return (FARPROC)IMPLICIT_CALL((DWORD)setId, (DWORD)iMethod);
}


/* Support functions which do simple things and then (usually) trap into the kernel */

/* Zero's out critical section info */

/*
    @doc BOTH EXTERNAL
    
    @func BOOL | GetVersionEx | Returns version information for the OS.
    @parm LPOSVERSIONINFO | lpver | address of structure to fill in.

    @comm Follows the Win32 reference description without restrictions or modifications.
*/
BOOL GetVersionEx(LPOSVERSIONINFO lpver) {
    DEBUGCHK(lpver->dwOSVersionInfoSize >= sizeof(OSVERSIONINFO));
    lpver->dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    lpver->dwMajorVersion = CE_MAJOR_VER;
    lpver->dwMinorVersion = CE_MINOR_VER;
    lpver->dwBuildNumber = CE_BUILD_VER;
    lpver->dwPlatformId = VER_PLATFORM_WIN32_CE;
    lpver->szCSDVersion[0] = '\0';
    return TRUE;
}

extern BOOL xxx_SystemParametersInfo_GWE(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni);

BOOL SystemParametersInfoW(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni) {
    BOOL retval = FALSE;
    DWORD bytesused;
    switch (uiAction) {
        case SPI_GETPLATFORMTYPE:
        case SPI_GETOEMINFO:
        case SPI_GETPLATFORMVERSION:
            __try  {
                retval = KernelIoControl(IOCTL_HAL_GET_DEVICE_INFO,&uiAction,4,pvParam,uiParam,&bytesused);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                ;
            }
            break;
        default:
            if (IsAPIReady(SH_WMGR))
                retval = xxx_SystemParametersInfo_GWE(uiAction, uiParam, pvParam, fWinIni);
            break;
    }
    return retval;
}

/* Alloc memory for stack, and then create thread (via kernel call) */

/*
    @doc BOTH EXTERNAL
    
    @func HANDLE | CreateThread | Creates a thread to execute within the address space of
    the calling process.

    @parm LPSECURITY_ATTRIBUTES | lpThreadAttributes | address of thread (<p must be NULL>.)
    security attributes.
    @parm DWORD | dwStackSize |  initial thread stack size, in bytes. (<p must be 0>)
    @parm LPTHREAD_START_ROUTINE | lpStartAddress | address of thread function
    @parm LPVOID | lpParameter | argument for new thread
    @parm DWORD | dwCreationFlags | creation flags
    @parm LPDWORD | lpThreadId | address of returned thread identifier


    @comm Follows the Win32 reference description with these restrictions:
    Certain parameters must be set as indicated above.

*/

/* Create a process.  Also creates an event to block on to synchronize the actual
   loading of the process so we can get the return value correctly */

/*
    @doc BOTH EXTERNAL
    
    @func BOOL | CreateProcess | Creates a new process and its primary thread.
    The new process executes the specified executable file.
    @parm LPCTSTR | lpApplicationName | pointer to name of executable module
    @parm LPTSTR | lpCommandLine | pointer to command line string
    @parm LPSECURITY_ATTRIBUTES | lpProcessAttributes | <p Must be NULL> Pointer to process security attributes
    @parm LPSECURITY_ATTRIBUTES | lpThreadAttributes | <p Must be NULL> Pointer to thread security attributes
    @parm BOOL | bInheritHandles | <p Must be FALSE> Handle inheritance flag
    @parm DWORD | dwCreationFlags | creation flag. <p See restrictions below>
    @parm LPVOID | lpEnvironment | <p Must be NULL> Pointer to new environment block
    @parm LPCTSTR | lpCurrentDirectory | <p Must be NULL> Pointer to current directory name
    @parm LPSTARTUPINFO | lpStartupInfo | <p Must be NULL> Pointer to STARTUPINFO
    @parm LPPROCESS_INFORMATION | lpProcessInformation | pointer to PROCESS_INFORMATION

    @comm Follows the Win32 reference description with these restrictions:
    Certain parameters must be set as indicated above.
    The loader does not search a path. Only the following <p dwCreationFlags> parameters are
    supported: CREATE_SUSPENDED.  The command line cannot contain parameters and the parameter
    list cannot contain the name of the executable as the first element.  The parameters and
    command line must be in the two variables, not combined in either one.
    
*/

/*
    @doc BOTH EXTERNAL
    
    @func BOOL | TerminateProcess | Terminates a process
    @parm HANDLE | hProc | handle of process (ProcessID)
    @parm DWORD | dwRetVal | return value of process (ignored)
    @comm You cannot terminate a PSL.
          You may not be able to terminate a process wedged inside a PSL.

*/


VOID WINAPI SystemMemoryLow(void) {
    PSLNotify(DLL_MEMORY_LOW,0,0);
}

/* Terminate thread routine */

/*
    @doc BOTH EXTERNAL
    
    @func VOID | ExitThread | Ends a thread.
    @parm DWORD | dwExitCode | exit code for this thread
    
    @comm Follows the Win32 reference description with the following restriction:
            If the primary thread calls ExitThread, the application will exit

*/
VOID WINAPI ExitThread (DWORD dwExitCode)
{
    // if hInstCoreDll is NULL, the process hasn't even started when it got terminated.
    // don't bother notifying PSL or calling DllMain.
    if (hInstCoreDll) {
        DWORD i = 0;
        DWORD dwReason = DLL_THREAD_DETACH;

        ReleaseProcCS ();

        SetLastError(dwExitCode);
        CloseProcOE(2);
        if (IsPrimaryThread()) {
            IsExiting = 1;
            dwReason = DLL_PROCESS_DETACH;
            KillAllOtherThreads();
            while (OtherThreadsRunning()) {
                if (i < 2) {
                    if (++i == 2)
                        PSLNotify(DLL_PROCESS_EXITING,GetCurrentProcessId(),GetCurrentThreadId());
                }
                KillAllOtherThreads();
                if (i > 1)
                    Sleep(250*(i-1));
            }
            EnterCriticalSection (&ProcCS);
            i = ProcessDetachAllDLLs ();
            CallDllMains (i, dwReason);
            CoreDllInit (hInstCoreDll, dwReason, 0);

            LeaveCriticalSection (&ProcCS);
            DebugNotify(DLL_PROCESS_DETACH,dwExitCode);

            CloseProcOE(1);
            CloseAllHandles();
            PSLNotify(DLL_PROCESS_DETACH,GetCurrentProcessId(),GetCurrentThreadId());
        } else {
            if (!IsExiting && GetCurrentProcessIndex()) {
                EnterCriticalSection (&ProcCS);
                i = ThreadAttachOrDetach ();
                CallDllMains (i, dwReason);
                LeaveCriticalSection (&ProcCS);
                PSLNotify(DLL_THREAD_DETACH,GetCurrentProcessId(),GetCurrentThreadId());
                DebugNotify(DLL_THREAD_DETACH,dwExitCode);
            }
            CloseProcOE(0);
        }
        if (GetCurrentProcessIndex()) {
            LPVOID pBuf;
            Imm_DllEntry (hInstCoreDll, dwReason, 0);
            if ((pBuf = TlsGetValue(TLSSLOT_RUNTIME)) && ((DWORD)pBuf >= 0x10000)) {
                LocalFree((LPVOID)ZeroPtr(pBuf));
            }
        }
    }
    NKTerminateThread(dwExitCode);
}

/* Thread local storage routines (just like Win32) */

/*
    @doc BOTH EXTERNAL
    
    @func LPVOID | TlsGetValue | Retrieves the value in the calling thread's thread local
    storage (TLS) slot for a specified TLS index. Each thread of a process has its own slot
    for each TLS index.
    @parm DWORD | dwTlsIndex | TLS index to retrieve value for

    @comm Follows the Win32 reference description without restrictions or modifications.
*/
LPVOID WINAPI TlsGetValue(DWORD slot) {
    LPDWORD tlsptr = UTlsPtr ();
    LPVOID lpRet = NULL;
    if (tlsptr && (slot < TLS_MINIMUM_AVAILABLE)) {
        DEBUGCHK ((slot < TLSSLOT_NUMRES) || ((HANDLE) GetCurrentProcessId () == GetOwnerProcess ()));
        // from SDK help:
        // Note  The data stored in a TLS slot can have a value of zero. In this case,
        // the return value is zero and GetLastError returns NO_ERROR.

        // fail TlsGetValue in non-ship build if current process != owner process
#ifndef SHIP_BUILD
        if ((slot >= TLSSLOT_NUMRES) && ((HANDLE) GetCurrentProcessId () != GetOwnerProcess ()))
            SetLastError (ERROR_INVALID_PARAMETER);
        else
#endif
        if (!(lpRet = (LPVOID) tlsptr[slot])) {
            SetLastError (NO_ERROR);
        }
    } else {
        SetLastError (ERROR_INVALID_PARAMETER);
    }
    return lpRet;
}
    
/*
    @doc BOTH EXTERNAL
    
    @func BOOL | TlsSetValue | Stores a value in the calling thread's thread local storage
    (TLS) slot for a specified TLS index. Each thread of a process has its own slot for each
    TLS index.
    @parm DWORD | dwTlsIndex | TLS index to set value for
    @parm LPVOID | lpvTlsValue | value to be stored

    @comm Follows the Win32 reference description without restrictions or modifications.
*/
BOOL WINAPI TlsSetValue(DWORD slot, LPVOID value) {
    LPDWORD tlsptr = UTlsPtr ();
    if (tlsptr && (slot < TLS_MINIMUM_AVAILABLE)) {
        DEBUGCHK ((slot < TLSSLOT_NUMRES) || ((HANDLE) GetCurrentProcessId () == GetOwnerProcess ()));

        // fail TlsSetValue in non-ship build if current process != owner process
#ifndef SHIP_BUILD
        if ((slot < TLSSLOT_NUMRES) || ((HANDLE) GetCurrentProcessId () == GetOwnerProcess ()))
#endif
        {
            tlsptr[slot] = (DWORD)value;
            return TRUE;
        }
    }
    SetLastError (ERROR_INVALID_PARAMETER);
    return FALSE;
}

BOOL IsProcessDying() {
    return (UTlsPtr()[TLSSLOT_KERNEL] & TLSKERN_TRYINGTODIE) ? 1 : 0;
}

typedef DWORD (*threadfunctype)(ulong);
typedef DWORD (*comthreadfunctype)(ulong,ulong,ulong,ulong,ulong,ulong,ulong);

// Dupe of structure in showerr.c, change both or move to shared header file
typedef struct _ErrInfo {
    DWORD dwExcpCode;
    DWORD dwExcpAddr;
} ErrInfo, *PErrInfo;

DWORD WINAPI ShowErrThrd (LPVOID lpParam);

HANDLE hMainThread;

VOID WINAPI ThreadExceptionExit (DWORD dwExcpCode, DWORD dwExcpAddr)
{

    // for safety measure, on excpetion, don't call anything in KMode directly
    // or we might hit another exception.
    UTlsPtr ()[PRETLS_THRDINFO] &= ~UTLS_INKMODE;

    // we should've try-excepted all code that can generate an exception while
    // hold ProcCS.
    DEBUGCHK ((HANDLE) GetCurrentThreadId () != ProcCS.OwnerThread);
    ReleaseProcCS ();

    if ((GetCurrentThreadId () == (DWORD) hMainThread) && !(UTlsPtr()[TLSSLOT_KERNEL] & TLSKERN_TRYINGTODIE)) {
        // main thread faulted, show error bux
        ErrInfo errInfo = {dwExcpCode, dwExcpAddr};
        HANDLE hThrd = CreateThread (NULL, 0, ShowErrThrd, &errInfo, 0, NULL);

        if (!hThrd) {
            // if we can't create a thread, don't bother showing the error box since it's likely the system is
            // in a really stressed condition.
            RETAILMSG (1, (L"Main thread in proc %8.8lx faulted, Exception code = %8.8lx, Exception Address = %8.8x!\r\n",
                GetCurrentProcessId(), dwExcpCode, dwExcpAddr));
            RETAILMSG (1, (L"Main thread in proc %8.8lx faulted - cleaning up!\r\n", GetCurrentProcessId()));
        } else {
            WaitForSingleObject (hThrd, INFINITE);
            xxx_CloseHandle (hThrd);
        }

    } else {
        // secondary thread faulted (or stack is completely messed up)
        LPCWSTR pname;
        pname = GetProcName();
        // don't terminate GWES.EXE or DEVICE.EXE
        RETAILMSG(1,(L"%s thread in proc %8.8lx (%s) faulted!\r\n",
                (GetCurrentThreadId () == (DWORD) hMainThread)? L"Main" : L"Secondary",
                GetCurrentProcessId(),pname));
        if (wcsicmp(pname,L"device.exe") && wcsicmp(pname,L"gwes.exe") && wcsicmp(pname,L"services.exe") && wcsicmp(pname,L"filesys.exe")) {
            RETAILMSG(1,(L"Terminating process %8.8lx (%s)!\r\n",GetCurrentProcessId(),pname));
            TerminateThread(hMainThread, dwExcpCode);
        }
    }
    ExitThread (dwExcpCode);
}

void
RegisterDlgClass(
    void
    );


DWORD MainThreadInit (DWORD dwModCnt)
{
    PDLLMAININFO pList = NULL;
    DWORD dwErr = 0;

    if (dwModCnt && !(pList = (PDLLMAININFO) _alloca (dwModCnt * sizeof(DLLMAININFO)))) {
        return ERROR_OUTOFMEMORY;
    }
    EnterCriticalSection (&ProcCS);
    if (dwModCnt) {
        // we need to retrieve the module list before calling CoreDllInit because
        // we might load lmemdebug and destroy the list.
        GetProcModList (pList, dwModCnt);
    }
    CoreDllInit (hInstCoreDll, DLL_PROCESS_ATTACH, 0);
    if (dwModCnt) {
        dwErr = _CallDllMains (pList, dwModCnt, DLL_PROCESS_ATTACH);
    }
    LeaveCriticalSection (&ProcCS);
    return dwErr;
}

#if defined(x86)
// Turn off FPO optimization for base functions so that debugger can correctly unwind retail x86 call stacks
#pragma optimize("y",off)
#endif

void
MainThreadBaseFunc(
    LPVOID      pfn,
    ulong       param1,
    DWORD       dwModCnt,
    ulong       param3,
    HINSTANCE   hCoreDll,
    DWORD       dwRva14,
    DWORD       dwSize14,
    DWORD       dwExeBase
    )
{
    DWORD dwErr;
    extern BOOL InitSysTime (void);

    hMainThread = (HANDLE)GetCurrentThreadId();
    hInstCoreDll = hCoreDll;
    InitSysTime ();

    InitializeCriticalSection (&ProcCS);

    DebugNotify (DLL_PROCESS_ATTACH, (DWORD)pfn);

    // purposely make MainThreadInit a function so the stack used by _alloc can be reused.
    dwErr = MainThreadInit (dwModCnt);

    if (!dwErr) {
        Imm_DllEntry(hInstCoreDll, DLL_PROCESS_ATTACH, 0);

        RegisterDlgClass();

        PSLNotify (DLL_PROCESS_ATTACH, GetCurrentProcessId(), (DWORD) hMainThread);

        dwErr = ((comthreadfunctype)pfn) (param1, 0, param3, SW_SHOW, dwExeBase, dwRva14, dwSize14);
    }
    /* ExitThread stops execution of the current thread */
    ExitThread (dwErr);

}

void ThreadBaseFunc(LPVOID pfn, ulong param)
{
    DWORD retval = 0;
    if (GetCurrentProcessIndex()) {
        Imm_DllEntry (hInstCoreDll, DLL_THREAD_ATTACH, 0);
        DebugNotify(DLL_THREAD_ATTACH,(DWORD)pfn);
        PSLNotify(DLL_THREAD_ATTACH,GetCurrentProcessId(),GetCurrentThreadId());

        // Don't bother calling DLL_THREAD_ATTACH if we're exiting due to exception.
        // Or we might run into deadlock if THREAD_ATTACH calls Heap API.
        if ((LPVOID)ShowErrThrd != pfn) {
            EnterCriticalSection (&ProcCS);
            retval = ThreadAttachOrDetach ();
            CallDllMains (retval, DLL_THREAD_ATTACH);
            LeaveCriticalSection (&ProcCS);
        }
    }

    retval = ((threadfunctype)pfn)(param);
    ExitThread(retval);
    /* ExitThread stops execution of the current thread */
}

#if defined(x86)
// Re-Enable optimization
#pragma optimize("",on)
#endif

/*
    @doc BOTH EXTERNAL
    
    @func VOID | GlobalMemoryStatus | Gets information on the physical and virtual memory of the system
    @parm LPMEMORYSTATUS | lpmst | pointer to structure to receive information
    @comm Follows the Win32 reference description without restrictions or modifications.
*/

VOID WINAPI GlobalMemoryStatus(LPMEMORYSTATUS lpmst) {
    DWORD addr;
    MEMORY_BASIC_INFORMATION mbi;
    lpmst->dwLength = sizeof(MEMORYSTATUS);
    lpmst->dwMemoryLoad = 100 - ((UserKInfo[KINX_PAGEFREE]*100) / UserKInfo[KINX_NUMPAGES]);
    lpmst->dwTotalPhys = UserKInfo[KINX_NUMPAGES]*UserKInfo[KINX_PAGESIZE];
    lpmst->dwAvailPhys = UserKInfo[KINX_PAGEFREE]*UserKInfo[KINX_PAGESIZE];
    lpmst->dwTotalPageFile = 0;
    lpmst->dwAvailPageFile = 0;
    lpmst->dwTotalVirtual = 32*1024*1024;
    lpmst->dwAvailVirtual = 0;
    for (addr = 0x10000; addr < 32*1024*1024; addr += (DWORD)mbi.RegionSize) {
        if (!VirtualQuery((LPCVOID)addr,&mbi,sizeof(mbi)))
            break;
        if (mbi.State == MEM_FREE)
            lpmst->dwAvailVirtual += (mbi.RegionSize - ((~(DWORD)mbi.BaseAddress+1)&0xffff)) & 0xffff0000;
    }
}

BOOL AttachDebugger(LPCWSTR dbgname) {
    if (CeGetCurrentTrust() != OEM_CERTIFY_TRUST) {
        ERRORMSG(1,(L"AttachDebugger failed due to insufficient trust\r\n"));
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    if (!LoadKernelLibrary(dbgname)) {
        SetLastError(ERROR_FILE_NOT_FOUND);
        return FALSE;
    }
    if (!ConnectDebugger(NULL)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    return TRUE;
}

BOOL AttachHdstub (LPCWSTR dbgname) {
    if (CeGetCurrentTrust () != OEM_CERTIFY_TRUST) {
        ERRORMSG (1,(L"AttachHdstub failed due to insufficient trust\r\n"));
        SetLastError (ERROR_ACCESS_DENIED);
        return FALSE;
    }
    if (!LoadKernelLibrary (dbgname)) {
        SetLastError (ERROR_FILE_NOT_FOUND);
        return FALSE;
    }
    if (!ConnectHdstub (NULL)) {
        SetLastError (ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    return TRUE;
}


BOOL AttachOsAxsT0 (LPCWSTR dbgname) {
    if (CeGetCurrentTrust () != OEM_CERTIFY_TRUST) {
        ERRORMSG (1,(L"AttachOsAxsT0 failed due to insufficient trust\r\n"));
        SetLastError (ERROR_ACCESS_DENIED);
        return FALSE;
    }
    if (!LoadKernelLibrary (dbgname)) {
        SetLastError (ERROR_FILE_NOT_FOUND);
        return FALSE;
    }
    if (!ConnectOsAxsT0 (NULL)) {
        SetLastError (ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    return TRUE;
}

BOOL AttachOsAxsT1 (LPCWSTR dbgname) {
    if (CeGetCurrentTrust () != OEM_CERTIFY_TRUST) {
        ERRORMSG (1,(L"AttachOsAxsT1 failed due to insufficient trust\r\n"));
        SetLastError (ERROR_ACCESS_DENIED);
        return FALSE;
    }
    if (!LoadKernelLibrary (dbgname)) {
        SetLastError (ERROR_FILE_NOT_FOUND);
        return FALSE;
    }
    if (!ConnectOsAxsT1 (NULL)) {
        SetLastError (ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    return TRUE;
}


#if defined(x86)
// Turn off FPO optimization for CaptureDumpFileOnDevice function so that Watson can correctly unwind retail x86 call stacks
#pragma optimize("y",off)
#endif

BOOL CaptureDumpFileOnDevice(DWORD dwProcessId, DWORD dwThreadId, LPCWSTR pwzExtraFilesPath)
{
    BOOL fHandled = FALSE;
    DWORD dwArguments[5];
    WCHAR wzCanonicalExtraFilesPath[MAX_PATH];
    BOOL  fReportFault = (dwProcessId == (-1)) && (dwThreadId == (-1));
    DWORD dwArg2 = 0;

    if (!fReportFault)
    {
        if (pwzExtraFilesPath)
        {
            if (!CeGetCanonicalPathNameW(pwzExtraFilesPath, wzCanonicalExtraFilesPath, ARRAY_SIZE(wzCanonicalExtraFilesPath), 0))
            {
                fHandled = FALSE;
                SetLastError(ERROR_BAD_PATHNAME);
                goto Exit;
            }
            dwArg2 = (DWORD)wzCanonicalExtraFilesPath;
        }
    }
    else
    {
        // For ReportFault this is actually the pointer to the exception
        dwArg2 = (DWORD)pwzExtraFilesPath;
    }
    
    dwArguments[0] = dwProcessId;
    dwArguments[1] = dwThreadId;
    dwArguments[2] = dwArg2;

    // We pass in the CurrentTrust as an extra safety check in DwDmpGen.cpp
    // DwDmpGen.cpp will do additional trust level checking.
    dwArguments[3] = CeGetCurrentTrust();  
    dwArguments[4] = (DWORD)&CaptureDumpFileOnDevice;
    
    __try 
    {
        // This exception will be handled by OsAxsT0.dll if
        // we succesfully generate a dump file. The RaisException 
        // will return if handled, otherwise it will caught by the 
        // the try catch block.
        RaiseException(STATUS_CRASH_DUMP,0,5,&dwArguments[0]);
        fHandled = TRUE;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        // We end up here if no dump was captured, in which case we return FALSE
        fHandled = FALSE;
        if (ERROR_SUCCESS == GetLastError())
        {
            SetLastError(ERROR_NOT_SUPPORTED);
        }
    }

Exit:
    
    return fHandled;
}

#if defined(x86)
// Re-Enable optimization
#pragma optimize("",on)
#endif

EFaultRepRetVal APIENTRY ReportFault(LPEXCEPTION_POINTERS pep, DWORD dwMode)
{
    if (!CaptureDumpFileOnDevice(-1,-1,(LPCWSTR)pep))
    {
        return frrvErrNoDW;
    }
    return frrvOk;
}

BOOL SetInterruptEvent(DWORD idInt) {
    long mask;
    long pend;
    long *ptrPend;

    if ((idInt < SYSINTR_DEVICES) || (idInt >= SYSINTR_MAXIMUM))
        return FALSE;
    idInt -= SYSINTR_DEVICES;
    ptrPend = (long *) UserKInfo[KINX_PENDEVENTS];
    
    // calculate which DWORD based on idInt
    ptrPend += idInt >> 5;  // idInt / 32
    idInt   &= 0x1f;        // idInt % 32

    mask = 1 << idInt;
    do {
        pend = *ptrPend;
        if (pend & mask)
            return TRUE;    // The bit is already set, so all done.
    } while (InterlockedTestExchange(ptrPend, pend, pend|mask) != pend);
    return TRUE;
}

// Macallan: Power Handler can call SetEvent directly
BOOL CeSetPowerOnEvent (HANDLE hEvt)
{
    return SetEvent (hEvt);
}


VOID FreeLibraryAndExitThread(HMODULE hLibModule, DWORD dwExitCode) {
    FreeLibrary(hLibModule);
    ExitThread(dwExitCode);
}

static CONST WCHAR szHex[] = L"0123456789ABCDEF";

UINT GetTempFileNameW(LPCWSTR lpPathName, LPCWSTR lpPrefixString, UINT uUnique, LPWSTR lpTempFileName) {
    DWORD Length, Length2, PassCount, dwAttr;
    UINT uMyUnique;
    HANDLE hFile;
    Length = wcslen(lpPathName);
    if (!Length || (Length >= MAX_PATH)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    memcpy(lpTempFileName,lpPathName,Length*sizeof(WCHAR));
    if (lpTempFileName[Length-1] != (WCHAR)'\\')
        Length++;
    lpTempFileName[Length-1] = 0;
    dwAttr = GetFileAttributesW(lpTempFileName);
    if ((dwAttr == 0xFFFFFFFF) || !(dwAttr & FILE_ATTRIBUTE_DIRECTORY)) {
        SetLastError(ERROR_DIRECTORY);
        return 0;
    }
    lpTempFileName[Length-1] = L'\\';
    PassCount = 0;
    Length2 = wcslen(lpPrefixString);
    if (Length2 > 3)
        Length2 = 3;
    memcpy(&lpTempFileName[Length],lpPrefixString,Length2*sizeof(WCHAR));
    Length += Length2;
    uUnique &= 0x0000ffff;
    if ((Length + 9) > MAX_PATH) { // 4 hex digits, .tmp,  and a null
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    lpTempFileName[Length+4] = '.';
    lpTempFileName[Length+5] = 't';
    lpTempFileName[Length+6] = 'm';
    lpTempFileName[Length+7] = 'p';
try_again:
    if (!uUnique) {
        if (!(uMyUnique = (UINT)Random() & 0x0000ffff)) {
            if (!(++PassCount & 0xffff0000))
                goto try_again;
            SetLastError(ERROR_RETRY);
            return 0;
        }
    } else
        uMyUnique = uUnique;
    lpTempFileName[Length] = szHex[(uMyUnique >> 12) & 0xf];
    lpTempFileName[Length+1] = szHex[(uMyUnique >> 8) & 0xf];
    lpTempFileName[Length+2] = szHex[(uMyUnique >> 4) & 0xf];
    lpTempFileName[Length+3] = szHex[uMyUnique & 0xf];
    lpTempFileName[Length+8] = 0;
    if (!uUnique) {
        if ((hFile = CreateFileW(lpTempFileName, GENERIC_READ, 0, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0)) == INVALID_HANDLE_VALUE) {
            switch (GetLastError()) {
                case ERROR_FILE_EXISTS:
                case ERROR_ALREADY_EXISTS:
                    if (!(++PassCount & 0xffff0000))
                        goto try_again;
                    break;
            }
            return 0;
        } else
            xxx_CloseHandle(hFile);
    }
    return uMyUnique;
}

// On success, return length of lpCanonicalPathName.  Otherwise, return 0.
DWORD
CeGetCanonicalPathNameW(
    LPCWSTR lpPathName,
    LPWSTR lpCanonicalPathName,
    DWORD cchCanonicalPathName,
    DWORD dwReserved
    )
{
    ULONG cchPathName;           // length of lpPathName; number of remaining characters
    ULONG ulPathIndex;           // current index in path
    ULONG ulNextDirectoryIndex;  // index(next directory relative to uiPathIndex)
    ULONG ulJumpOffset;          // skip extraneous '\\'-s or '/'-s
    PATH Path;                   // path object
    PATH_NODE PathNode;          // current path node
    PATH_NODE_TYPE PathNodeType; // type of current path node
    HRESULT hResult;             // StringCchLength return

    // Note: lpCanonicalPathName can be NULL
    if (!lpPathName) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    // Calculate length of lpPathName; StringCchLength succeeds if lpPathName <= MAX_PATH
    hResult = StringCchLength(lpPathName, MAX_PATH, &cchPathName);
    if (FAILED(hResult)) {
        SetLastError(ERROR_FILENAME_EXCED_RANGE);
        return 0;
    }

    // Initialize path object
    InitPath(&Path);
    // Determine path type
    GetPathType(lpPathName, cchPathName, &Path.PathType);
    // Jump to first path node; Subtract jump offset from cchPathName
    ulPathIndex = GetIndexOfNextPathNodeInPath(lpPathName, &cchPathName);

    // Process path name
    while (cchPathName > 0) {
        // Reset path node type
        PathNodeType = PNT_UNKNOWN;
        ulNextDirectoryIndex = GetIndexOfNextDirectoryInPath(&lpPathName[ulPathIndex], cchPathName);
        if (ulNextDirectoryIndex > 0) {
            // Directory
            GetPathNodeType(
                &lpPathName[ulPathIndex], ulNextDirectoryIndex, &PathNodeType);
            switch (PathNodeType) {
            case PNT_SELF:
                break;
            case PNT_PARENT:
                PopPathNode(&Path);
                break;
            case PNT_FILE:
                PathNode.ulPathIndex = ulPathIndex;
                PathNode.ulNameLength = ulNextDirectoryIndex;
                PathNode.PathNodeType = PathNodeType;
                PushPathNode(&Path, PathNode);
                break;
            default:
                // This should never happen
                DEBUGCHK(0);
            }
            // Jump over path node
            cchPathName -= (1 + ulNextDirectoryIndex);
            ulPathIndex += (1 + ulNextDirectoryIndex);
            // Ignore extraneous '\\'-s or '/'-s; Subtract jump offset from cchPathName
            ulJumpOffset = GetIndexOfNextPathNodeInPath(&lpPathName[ulPathIndex], &cchPathName);
            ulPathIndex += ulJumpOffset;
        }
        else {
            // File; Last node in path
            GetPathNodeType(&lpPathName[ulPathIndex], cchPathName, &PathNodeType);
            switch (PathNodeType) {
            case PNT_SELF:
                break;
            case PNT_PARENT:
                PopPathNode(&Path);
                break;
            case PNT_FILE:
                PathNode.ulPathIndex = ulPathIndex;
                PathNode.ulNameLength = cchPathName;
                PathNode.PathNodeType = PathNodeType;
                PushPathNode(&Path, PathNode);
                break;
            default:
                // This should never happen
                DEBUGCHK(0);
            }
            cchPathName = 0;
        }
    }

    // Calculate (cache) length of canonical path name
    cchPathName = GetPathLength(&Path);

    // If lpCanonicalPathName == NULL, then caller is only interested in the
    // length of the canonicalized version of lpPathName; Ignore cchCanonicalPathName
    if (lpCanonicalPathName == NULL) {
        return cchPathName;
    }

    // lpCanonicalPathName must be large enough to contain canonical path name
    if (cchPathName > cchCanonicalPathName) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return 0;
    }

    // Pop canonical path from stack into lpCanonicalPathName
    return PopPath(&Path, lpPathName, lpCanonicalPathName);
}

#define FS_COPYBLOCK_SIZE             (64*1024) // Perfect size for a VirtualAlloc
#define COPYFILEEX_FAIL_IF_DST_EXISTS (1 << 0)
#define COPYFILEEX_SRC_READ_ONLY      (1 << 1)
#define COPYFILEEX_QUIET              (1 << 2)
#define COPYFILEEX_RET                (1 << 3)
#define COPYFILEEX_DELETE_DST         (1 << 4)

BOOL
CopyFileExW(
    LPCWSTR lpExistingFileName,
    LPCWSTR lpNewFileName,
    LPPROGRESS_ROUTINE lpProgressRoutine,
    LPVOID lpData,
    LPBOOL pbCancel,
    DWORD dwCopyFlags // NT ignores COPY_FILE_RESTARTABLE
    )
{
    // File support
    HANDLE hExistingFile, hNewFile;        // Source and destination file handles
    DWORD dwFileAttributes;                // Source file attributes
    LARGE_INTEGER liExistingFileSize;      // Source file size
    FILETIME ftCreation, ftLastAccess, ftLastWrite; // Source file times
    WCHAR lpszCExisting[MAX_PATH];         // Canonical version of lpszExisting
    WCHAR lpszCNew[MAX_PATH];              // Canonical version of lpszNew
    BOOL fNewFileExists = FALSE;           // Destination file exists

    // Copy support
    LPBYTE lpbCopyChunk = NULL;            // Copy buffer
    DWORD dwBytesRead, dwBytesWritten;     // Copy bytes read and written
    DWORD dwCopyProgressResult;            // CopyProgressRoutine result
    LARGE_INTEGER liTotalBytesTransferred; // Bytes transferred

    // Result support
    DWORD dwError;                         // Capture last error of nested calls

    // Status flags
    DWORD dwFlags = COPYFILEEX_DELETE_DST; // bit 0=fFailIfDestinationExists;
                                           // bit 1=fDestinationFileExists
                                           // bit 2=fSourceFileReadyOnly
                                           // bit 3=fQuiet (Call CopyProgressRoutine)
                                           // bit 4=fRet
                                           // bit 5=fDeleteDestinationFile

    // Canonicalize lpExistingFileName and lpNewFileName
    if (!CeGetCanonicalPathName(lpNewFileName, lpszCNew, MAX_PATH, 0)) {
        return FALSE;
    }
    if (!CeGetCanonicalPathName(lpExistingFileName, lpszCExisting, MAX_PATH, 0)) {
        return FALSE;
    }

    if (dwCopyFlags & COPY_FILE_FAIL_IF_EXISTS) dwFlags |= COPYFILEEX_FAIL_IF_DST_EXISTS;

    // Open source file
    hExistingFile = CreateFile(lpszCExisting, GENERIC_READ,
        FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (INVALID_HANDLE_VALUE == hExistingFile) {
        return FALSE;
    }

    if (!(dwFlags & COPYFILEEX_FAIL_IF_DST_EXISTS) && 
        (0 == _wcsnicmp(lpszCExisting, lpszCNew, MAX_PATH))) {
        // Cannot copy a file onto itself
        VERIFY(CloseHandle(hExistingFile));
        SetLastError(ERROR_SHARING_VIOLATION);
        return FALSE;
    }

    // Get source file's attributes
    dwFileAttributes = GetFileAttributes(lpszCExisting);
    if (0xFFFFFFFF == dwFileAttributes) {
        // This shouldn't happen
        ERRORMSG(1, (_T(
            "CopyFileEx: Failed to get attributes of open file %s\r\n"
            ), lpszCExisting));
        VERIFY(CloseHandle(hExistingFile));
        return FALSE;
    }
    else {
        // Remove ROM flag to/and make sure the destination file can be written to
        dwFileAttributes &= ~(FILE_ATTRIBUTE_INROM);
        if (dwFileAttributes & FILE_ATTRIBUTE_READONLY) dwFlags |= COPYFILEEX_SRC_READ_ONLY;
        dwFileAttributes = dwFileAttributes & ~FILE_ATTRIBUTE_READONLY;
    }

    // TODO: Since we move the destination file's file pointer, should we allow shared access?
    // Attempt to open destination file
    hNewFile = CreateFile(
        lpszCNew,
        GENERIC_WRITE,
        FILE_SHARE_READ|FILE_SHARE_WRITE,
        0,
        CREATE_NEW,
        dwFileAttributes,
        0);
    if (INVALID_HANDLE_VALUE == hNewFile) {
        // The destination file already exists
        fNewFileExists = TRUE;
        if (dwFlags & COPYFILEEX_FAIL_IF_DST_EXISTS) {
            VERIFY(CloseHandle(hExistingFile));
            return FALSE;
        }
        // Open the destination file
        hNewFile = CreateFile(
            lpszCNew,
            GENERIC_WRITE,
            FILE_SHARE_READ|FILE_SHARE_WRITE,
            0,
            OPEN_ALWAYS,
            dwFileAttributes,
            0);
        if (INVALID_HANDLE_VALUE == hNewFile) {
            VERIFY(CloseHandle(hExistingFile));
            return FALSE;
        }
        // Destination file exists, so only delete destination file if copy
        // cancelled through CopyProgressRoutine, as per CopyFileEx specification
        dwFlags &= ~(COPYFILEEX_DELETE_DST);
    }

    // If destination file exists and is hidden or read-only, fail
    dwFileAttributes = GetFileAttributes(lpszCNew);
    if ((dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_READONLY)) &&
        fNewFileExists
    ) {
        SetLastError(ERROR_ACCESS_DENIED);
        goto cleanUp;
    }

    // Allocate copy chunk
    lpbCopyChunk = VirtualAlloc(0, FS_COPYBLOCK_SIZE, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
    if (!lpbCopyChunk) {
        SetLastError(ERROR_OUTOFMEMORY);
        goto cleanUp;
    }

    // Get size of source file
    liExistingFileSize.LowPart = GetFileSize(hExistingFile, (PDWORD)&liExistingFileSize.HighPart);
    if (0xFFFFFFFF == liExistingFileSize.LowPart) {
        goto cleanUp;
    }

    // Expand destination file
    if (0xFFFFFFFF == SetFilePointer(
        hNewFile,
        (LONG) liExistingFileSize.LowPart,
        &liExistingFileSize.HighPart,
        FILE_BEGIN
    )) {
        // 0xFFFFFFFF is also a valid file pointer, confirm with GetLastError
        if (NO_ERROR != GetLastError()) goto cleanUp;
    }
    // NT pre-allocates the destination file.  If this fails, then behavior
    // deviates from NT, but correctness is preserved.
    SetEndOfFile(hNewFile);
    // Reset destination file pointer
    if (0xFFFFFFFF == SetFilePointer(hNewFile, 0, NULL, FILE_BEGIN)) {
        goto cleanUp;
    }

    // NT will call CopyProgressRoutine for initial stream switch regardless
    // of the value of *pbCancel
    if (lpProgressRoutine) {
        liTotalBytesTransferred.QuadPart = 0;
        // File streams not supported; Always report stream 1
        dwCopyProgressResult = (*lpProgressRoutine)(
            liExistingFileSize,
            liTotalBytesTransferred,
            liExistingFileSize,
            liTotalBytesTransferred,
            1,
            CALLBACK_STREAM_SWITCH,
            hExistingFile,
            hNewFile,
            lpData);
        switch (dwCopyProgressResult) {
        case PROGRESS_CONTINUE:
            break;
        case PROGRESS_CANCEL:
            dwFlags |= COPYFILEEX_DELETE_DST;
            SetLastError(ERROR_REQUEST_ABORTED);
            goto cleanUp;
        case PROGRESS_STOP:
            SetLastError(ERROR_REQUEST_ABORTED);
            goto cleanUp;
        case PROGRESS_QUIET:
            dwFlags |= COPYFILEEX_QUIET;
            break;
        default:
            ERRORMSG(1,(_T(
                "CopyFileEx: Error: Unknown CopyProgressRoutine result\r\n"
                )));
            goto cleanUp;
        }
    }

    // Perform copy
    while (1) {
        // Has the copy been cancelled?
        if (pbCancel && *pbCancel) {
            // Although the CopyFileEx specification is unclear as to whether
            // the destination file is deleted when *pbCancel is TRUE, NT
            // deletes the destination file when *pbCancel is TRUE, regardless
            // of whether the file existed before CopyFileEx was invoked
            dwFlags |= COPYFILEEX_DELETE_DST;
            goto cleanUp;
        }
        if (!ReadFile(hExistingFile, lpbCopyChunk, FS_COPYBLOCK_SIZE, &dwBytesRead, 0)) {
            goto cleanUp;
        }
        else if (
            dwBytesRead &&
            (!WriteFile(hNewFile, lpbCopyChunk, dwBytesRead, &dwBytesWritten, 0) ||
            (dwBytesWritten != dwBytesRead)
        )) {
            goto cleanUp;
        }

        // Is copy complete?
        if (dwBytesRead == 0) break;

        // Update copy progress
        liTotalBytesTransferred.QuadPart += (LONGLONG) dwBytesWritten;

        // Call CopyProgressRoutine
        if (!(dwFlags & COPYFILEEX_QUIET) && lpProgressRoutine) {
            // File streams not supported; Always report stream 1
            dwCopyProgressResult = (*lpProgressRoutine)(
                liExistingFileSize,
                liTotalBytesTransferred,
                liExistingFileSize,
                liTotalBytesTransferred,
                1,
                CALLBACK_CHUNK_FINISHED,
                hExistingFile,
                hNewFile,
                lpData);
            switch (dwCopyProgressResult) {
            case PROGRESS_CONTINUE:
                break;
            case PROGRESS_CANCEL:
                dwFlags |= COPYFILEEX_DELETE_DST;
                SetLastError(ERROR_REQUEST_ABORTED);
                goto cleanUp;
            case PROGRESS_STOP:
                SetLastError(ERROR_REQUEST_ABORTED);
                goto cleanUp;
            case PROGRESS_QUIET:
                dwFlags |= COPYFILEEX_QUIET;
                break;
            default:
                ERRORMSG(1,(_T(
                    "CopyFileEx: Error: Unknown CopyProgressRoutine result\r\n"
                    )));
                goto cleanUp;
            }
        }
    }

    // The file was copied successfully
    dwFlags |= COPYFILEEX_RET;         // Mark success
    dwFlags &= ~COPYFILEEX_DELETE_DST; // Unmark deletion of destination file

cleanUp:;

    VirtualFree(lpbCopyChunk, 0, MEM_RELEASE);

    if (dwFlags & COPYFILEEX_RET) {
        // Set destination file time
        GetFileTime(hExistingFile, &ftCreation, &ftLastAccess, &ftLastWrite);
        SetFileTime(hNewFile, &ftCreation, &ftLastAccess, &ftLastWrite);
    }

    VERIFY(CloseHandle(hExistingFile));
    VERIFY(CloseHandle(hNewFile));

    if ((dwFlags & COPYFILEEX_RET) &&
        (dwFlags & COPYFILEEX_SRC_READ_ONLY)) {
        // If source file read-only, make destination file read-only
        SetFileAttributes(lpszCNew, dwFileAttributes|FILE_ATTRIBUTE_READONLY);
    }

    // DeleteFile deletes a file if no open handles exist
    if (dwFlags & COPYFILEEX_DELETE_DST) {
        // Preserve LastError
        dwError = GetLastError();
        DeleteFile(lpszCNew);
        SetLastError(dwError);
    }

    return (dwFlags & COPYFILEEX_RET);
}

BOOL GetFileAttributesExW(LPCWSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation) {

    LPCWSTR pTrav;
    HANDLE hFind;
    WIN32_FIND_DATA w32fd;

    for (pTrav = lpFileName; *pTrav; pTrav++) {
        if (*pTrav == '*' || *pTrav == '?') {
            SetLastError(ERROR_INVALID_NAME);
            return FALSE;
        }
    }
    if (fInfoLevelId != GetFileExInfoStandard) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if ((hFind = FindFirstFile(lpFileName,&w32fd)) == INVALID_HANDLE_VALUE) {
        SetLastError(ERROR_FILE_NOT_FOUND);
        return FALSE;
    }
    xxx_CloseHandle(hFind);
    ((WIN32_FILE_ATTRIBUTE_DATA *)lpFileInformation)->dwFileAttributes = w32fd.dwFileAttributes;
    ((WIN32_FILE_ATTRIBUTE_DATA *)lpFileInformation)->ftCreationTime = w32fd.ftCreationTime;
    ((WIN32_FILE_ATTRIBUTE_DATA *)lpFileInformation)->ftLastAccessTime = w32fd.ftLastAccessTime;
    ((WIN32_FILE_ATTRIBUTE_DATA *)lpFileInformation)->ftLastWriteTime = w32fd.ftLastWriteTime;
    ((WIN32_FILE_ATTRIBUTE_DATA *)lpFileInformation)->nFileSizeHigh = w32fd.nFileSizeHigh;
    ((WIN32_FILE_ATTRIBUTE_DATA *)lpFileInformation)->nFileSizeLow = w32fd.nFileSizeLow;
    return TRUE;
}

LPVOID WINAPI CeZeroPointer (LPVOID ptr)
{
    return (LPVOID) ZeroPtr (ptr);
}

// Determine whether <pchTarget> matches <pchWildcardMask>; '?' and '*' are
// valid wildcards.
// The '*' quantifier is a 0 to <infinity> character wildcard.
// The '?' quantifier is a 0 or 1 character wildcard.
BOOL
MatchesWildcardMask(
    DWORD lenWildcardMask,
    PCWSTR pchWildcardMask,
    DWORD lenTarget,
    PCWSTR pchTarget
    )
{
    while (lenWildcardMask && lenTarget) {
        if (*pchWildcardMask == L'?') {
            // skip current target character
            lenTarget--;
            pchTarget++;
            lenWildcardMask--;
            pchWildcardMask++;
            continue;
        }
        if (*pchWildcardMask == L'*') {
            pchWildcardMask++;
            if (--lenWildcardMask) {
                while (lenTarget) {
                    if (MatchesWildcardMask(lenWildcardMask, pchWildcardMask, lenTarget--, pchTarget++)) {
                        return TRUE;
                    }
                }
                return FALSE;
            }
            return TRUE;
        }
        // test for case-insensitive equality
        else if (_wcsnicmp(pchWildcardMask, pchTarget, 1)) {
            return FALSE;
        }
        lenWildcardMask--;
        pchWildcardMask++;
        lenTarget--;
        pchTarget++;
    }
    // target matches wildcard mask, succeed
    if (!lenWildcardMask && !lenTarget) {
        return TRUE;
    }
    // wildcard mask has been spent and target has characters remaining, fail
    if (!lenWildcardMask) {
        return FALSE;
    }
    // target has been spent; succeed only if wildcard characters remain
    while (lenWildcardMask--) {
        if (*pchWildcardMask != L'*' && *pchWildcardMask != L'?') {
            return FALSE;
        }
        pchWildcardMask++;
    }
    return TRUE;
}


static	BOOL	s_ForcePixelDoubling;
static	BOOL	s_ForcePixelDoublingIsSet;
static	BOOL	s_ForcePixelDoublingIsChecked;


DWORD
WINAPI
ForcePixelDoubling(
	BOOL	torf
	)
{
	if ( s_ForcePixelDoublingIsSet ||
		 s_ForcePixelDoublingIsChecked )
		{
		//	OK if setting the same value.
		if ( s_ForcePixelDoubling == torf )
			{
			return ERROR_SUCCESS;
			}

		//	Too late
		if ( s_ForcePixelDoublingIsChecked )
			{
			return ERROR_ALREADY_REGISTERED;
			}
		//	Different value
		return ERROR_ALREADY_INITIALIZED;
		}

	s_ForcePixelDoubling = torf;
	s_ForcePixelDoublingIsSet = TRUE;
	return ERROR_SUCCESS;
}




int
WINAPI
IsForcePixelDoubling(
	void
	)
{
	if ( GetCurrentProcessId() != (DWORD)GetOwnerProcess() )
		{
		int	IsDouble = -1;

		CALLBACKINFO    cbi;
		cbi.hProc	= GetOwnerProcess();
		cbi.pfn		= (FARPROC)&IsForcePixelDoubling;
		__try
			{
			IsDouble = PerformCallBack(&cbi);
			}
		__except (EXCEPTION_EXECUTE_HANDLER)
			{
			}
		return IsDouble;
		}

	s_ForcePixelDoublingIsChecked = TRUE;
	if ( !s_ForcePixelDoublingIsSet )
		{
		return -1;
		}
	return s_ForcePixelDoubling;
}




