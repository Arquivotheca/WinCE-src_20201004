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

#ifdef KCOREDLL
#define WIN32_CALL(type, api, args) (*(type (*) args) g_ppfnWin32Calls[W32_ ## api])
#else
#define WIN32_CALL(type, api, args) IMPLICIT_DECL(type, SH_WIN32, W32_ ## api, args)
#endif

#define EDB 1
#include <windows.h>
#include <toolhelp.h>
#include <psapi.h>
#include <coredll.h>
#include <uiproxy.h>
#include <vmlayout.h>
#include <wdogapi.h>
#include <acctid.h>
#include <fsioctl.h>
#include <psystoken.h>
#include <cetlscommon.h>

static BOOL AutoCloseCFFMHandle (HANDLE h);

BOOL
AppVerifierIoControl (
    DWORD   dwIoControlCode,
    LPVOID  lpInBuf,
    DWORD   nInBufSize,
    LPVOID  lpOutBuf,
    DWORD   nOutBufSize,
    LPDWORD lpBytesReturned
    );

#ifdef KCOREDLL

#define BC_THRD(h)
#define BC_PROC(h)

#else  // KCOREDLL

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C" int
NKwvsprintfW(
    LPWSTR lpOut,
    LPCWSTR lpFmt,
    va_list lpParms,
    int maxchars
    );


//
// backward compatibility issue: earlier version of the OS allows user to pass a process/thread id
//      in place of process/thread handle argument. This class is to provide BC with old applications.
//
class IDConverter {
public:
    IDConverter (HANDLE h, BOOL fIsProc)
    {
        m_fIsId = !((DWORD) h & 1) && ((DWORD) h > 0x10000);
        if (m_fIsId) {
            // this is a thread id
            m_h = fIsProc
                // pass "0" for dwAccess to let OS decide the maximum allowed access for the caller
                ? OpenProcess (0, FALSE, (DWORD) h)
                : OpenThread (0, FALSE, (DWORD) h);
        } else {
            m_h = h;
        }
    }
    HANDLE GetHandle () const
    {
        return m_h;
    }
    ~IDConverter ()
    {
        if (m_fIsId) {
            CloseHandleInProc (hActiveProc, m_h);
        }
    }
private:
    HANDLE m_h;
    BOOL   m_fIsId;
};

#define BC_THRD(h)      IDConverter idc(h, FALSE); h = idc.GetHandle ();
#define BC_PROC(h)      IDConverter idc(h, TRUE); h = idc.GetHandle ();

void SetAbnormalTermination (void);

#endif // KCOREDLL

//
// Deprecated APIs: debugcheck only if the current thread is not a debugger
//
extern "C"
DWORD
xxx_GetProcAddrBits(
    HANDLE  hProc
    )
{
    RETAILMSG (1, (L"ERROR!!! Calling xxx_GetProcAddrBits\r\n"));
    DEBUGCHK(!DBGDEPRE);
    return 0;
}

extern "C"
HANDLE
xxx_GetProcFromPtr(
    LPVOID  lpv
    )
{
    RETAILMSG (1, (L"ERROR!!! Calling GetProcFromPtr\r\n"));
    DEBUGCHK(!DBGDEPRE);
    return 0;
}

extern "C"
BOOL
xxx_SetHandleOwner(
    HANDLE  h,
    HANDLE  hProc
    )
{
    RETAILMSG (1, (L"ERROR!!! Calling xxx_SetHandleOwner!!!!\r\n"));
    DEBUGCHK(!DBGDEPRE);
    return FALSE;
}

extern "C"
DWORD
xxx_GetCallerProcessIndex(
    void
    )
{
    RETAILMSG (1, (L"ERROR!!! calling GetCallerProcessIndex\r\n"));
    DEBUGCHK(!DBGDEPRE);
    return 0;
}

extern "C"
DWORD
xxx_GetProcessIndexFromID(
    HANDLE  hProc
    )
{
    RETAILMSG (1, (L"ERROR!!! calling xxx_GetProcessIndexFromID\r\n"));
    DEBUGCHK(!DBGDEPRE);
    return (DWORD)-1; // to resolve BC issues with existing MFC apps
}

extern "C"
HANDLE
xxx_GetProcessIDFromIndex(
    DWORD   dwIdx
    )
{
    RETAILMSG (1, (L"ERROR!!! calling xxx_GetProcessIDFromIndex\r\n"));
    DEBUGCHK(!DBGDEPRE);
    return 0;
}

extern "C"
DWORD
xxx_SetKMode(
    DWORD   mode
    )
{
    RETAILMSG (1, (L"ERROR!!! Calling SetKMode\r\n"));
    DEBUGCHK(!DBGDEPRE);
    return 0;
}

extern "C"
DWORD
xxx_SetProcPermissions(
    DWORD   perms
    )
{
    RETAILMSG (1, (L"ERROR!!! Calling xxx_SetProcPermissions\r\n"));
    DEBUGCHK(!DBGDEPRE);
    return 0;
}

extern "C"
DWORD
xxx_GetCurrentPermissions(
    void
    )
{
    RETAILMSG (1, (L"ERROR!!! Calling xxx_GetCurrentPermissions\r\n"));
    DEBUGCHK(!DBGDEPRE);
    return 0;
}

void ShowWarning (LPCTSTR pszAPIname)
{
    NKD (L"!!!Calling %s!!!\r\n\r\n", pszAPIname);
    NKD (L"If the pointer you're mapping is not an embedded pointer, and you don't need to\r\n");
    NKD (L"access it asynchronously (e.g. another thread will be writing to it), then there\r\n");
    NKD (L"is no need to call MapCallerPtr. The PSL interface already validated the pointer\r\n");
    NKD (L"for you. Assuming you're giving the right signature in the API signature table.\r\n\r\n");

    NKD (L"In case you need to access the pointer asynchronously, there are 3 choices\r\n");
    NKD (L"    (1) Copy-in/Copy-out yourself\r\n");
    NKD (L"    (2) VirtualCopy the pointer\r\n");
    NKD (L"    (3) Save current process id, for (process-id+pointer) uniquely idently a pointer\r\n");
    NKD (L"        and use 'SwitchToProcess (not supported yet)' API to switch to the process\r\n\r\n");
}

extern "C"
LPVOID
xxx_MapCallerPtr(
    LPVOID  ptr,
    DWORD   dwLen
    )
{
    ShowWarning (L"MapCallerPtr");
    DEBUGCHK(!DBGDEPRE);
    return ptr;
}

extern "C"
LPVOID
xxx_MapPtrToProcWithSize(
    LPVOID  ptr,
    DWORD   dwLen,
    HANDLE  hProc
    )
{
    ShowWarning (L"MapPtrToProcWithSize");
    DEBUGCHK(!DBGDEPRE);
    return ptr;
}

extern "C"
LPVOID
xxx_MapPtrToProcess(
    LPVOID  lpv,
    HANDLE  hProc
    )
{
    RETAILMSG (1, (L"!!!NOT SUPPORTED: Calling MapPtrToProcess!!!!!\r\n"));
    DEBUGCHK(!DBGDEPRE);
    return lpv;
}

extern "C"
LPVOID
xxx_MapPtrUnsecure(
    LPVOID  lpv,
    HANDLE  hProc
    )
{
    RETAILMSG (1, (L"!!!NOT SUPPORTED: Calling xxx_MapPtrUnsecure!!!!!\r\n"));
    DEBUGCHK(!DBGDEPRE);
    return lpv;
}

extern "C"
BOOL
WINAPI
xxx_CeMapArgumentArray(
    HANDLE  hProc,
    LPVOID* pArgList,
    DWORD   dwSig
    )
{
    RETAILMSG (1, (L"ERROR!!! CeMapArgumentArray is NOT not SUPPORTED\r\n"));
    DEBUGCHK(!DBGDEPRE);
    return FALSE;
}


// SDK includes

extern "C"
int
xxx_QueryAPISetID(
    char*   pName
    )
{
    int iRetVal = -1;
    DEBUGCHK(!DBGDEPRE);
    QueryAPISetID (pName, &iRetVal);
    return iRetVal;
}

extern "C"
DWORD
xxx_PerformCallBack4(
    CALLBACKINFO*   pcbi,
    LPVOID          p2,
    LPVOID          p3,
    LPVOID          p4
    )
{
    return PerformCallBack(pcbi,p2,p3,p4);
}

extern "C"
BOOL
xxx_EventModify(
    HANDLE  hEvent,
    DWORD   func
    )
{
    return EventModify(hEvent,func);
}

extern "C"
BOOL
xxx_IsNamedEventSignaled (LPCWSTR pszName, DWORD dwFlags)
{
    return IsNamedEventSignaled (pszName, dwFlags);
}


extern "C"
BOOL
xxx_CloseHandleInProc (HANDLE hProcess, HANDLE hObj)
{
    return CloseHandleInProc (hProcess, hObj);
}


extern "C"
BOOL
WINAPI
CloseHandle(
    HANDLE  hObject
    )
{
    BOOL fRet = AutoCloseCFFMHandle (hObject);
    if (!fRet) {
        fRet = CloseHandleInProc (hActiveProc, hObject);
    }
    return fRet;
}


extern "C"
HANDLE
WINAPI
xxx_CreateEventW(
    LPSECURITY_ATTRIBUTES   lpsa,
    BOOL                    bManualReset,
    BOOL                    bInitialState,
    LPCWSTR                 lpName
    )
{
    return CreateEventW (lpsa,bManualReset,bInitialState,lpName);
}


extern "C"
HANDLE
WINAPI
xxx_OpenEventW(
    DWORD   dwDesiredAccess,
    BOOL    bInheritHandle,
    LPCWSTR lpName
    )
{
    return OpenEventW(dwDesiredAccess, bInheritHandle, lpName);
}

extern "C"
DWORD
WINAPI
xxx_GetEventData(
    HANDLE  hEvent
    )
{
    return EventGetData(hEvent);
}

extern "C"
BOOL
WINAPI
xxx_SetEventData(
    HANDLE  hEvent,
    DWORD   dwData
    )
{
    return EventSetData(hEvent, dwData);
}

extern "C"
VOID
WINAPI
xxx_Sleep(
    DWORD   dwMilliseconds
    )
{
    KernelSleep(dwMilliseconds);
}

extern "C"
DWORD
WINAPI
xxx_WaitForMultipleObjects(
            DWORD   cObjects,
    CONST   HANDLE* lphObjects,
            BOOL    fWaitAll,
            DWORD   dwTimeout
    )
{
    DWORD dwRetVal = WAIT_FAILED;
    KernelWaitForMultipleObjects(cObjects, lphObjects, fWaitAll, dwTimeout, &dwRetVal);
    return dwRetVal;
}

extern "C"
DWORD
WINAPI
xxx_WaitForSingleObject(
    HANDLE  hHandle,
    DWORD   dwMilliseconds
    )
{
    DWORD dwRetVal = WAIT_FAILED;
    KernelWaitForMultipleObjects(1, &hHandle, FALSE, dwMilliseconds, &dwRetVal);
    return dwRetVal;
}

extern "C"
DWORD
WINAPI
xxx_SuspendThread(
    HANDLE  hThread
    )
{
    DWORD dwRetVal = 0xFFFFFFFF;
    BC_THRD(hThread);
    KernelSuspendThread(hThread, &dwRetVal);
    return dwRetVal;
}

extern "C"
DWORD
WINAPI
xxx_ResumeThread(
    HANDLE  hThread
    )
{
    DWORD dwRetVal = 0xFFFFFFFF;
    BC_THRD(hThread);
    ResumeThread(hThread, &dwRetVal);

    // BC workaround - allow calling ResumeThread on the thread handle returned by CreateProcess
    if (0xFFFFFFFF == dwRetVal) {
        NKResumeMainThread (hThread, &dwRetVal);
    }

    return dwRetVal;
}

extern "C"
BOOL WINAPI xxx_GetThreadContext(HANDLE hThread, LPCONTEXT lpContext)
{
    BC_THRD(hThread);
    return GetThreadContext(hThread, lpContext);
}

extern "C"
BOOL WINAPI xxx_SetThreadContext(HANDLE hThread, const CONTEXT *lpContext)
{
    BC_THRD(hThread);
    return SetThreadContext(hThread, lpContext);
}

extern "C"
BOOL
WINAPI
xxx_SetThreadPriority(
    HANDLE  hThread,
    int     nPriority
    )
{
    BC_THRD(hThread);
    return SetThreadPriority(hThread,nPriority);
}

extern "C"
int
WINAPI
xxx_GetThreadPriority(
    HANDLE  hThread
    )
{
    DWORD dwRetVal = THREAD_PRIORITY_ERROR_RETURN;
    BC_THRD(hThread);
    GetThreadPriority(hThread, &dwRetVal);
    return dwRetVal;
}

extern "C"
DWORD
WINAPI
xxx_GetThreadId (
    HANDLE  hThread
    )
{
    BC_THRD (hThread);
    return GetThreadId (hThread);
}

extern "C"
DWORD
WINAPI
xxx_GetProcessIdOfThread (
    HANDLE  hThread
    )
{
    BC_THRD (hThread);
    return GetProcessIdOfThread (hThread);
}

extern "C"
int WINAPI xxx_CeGetThreadPriority(HANDLE hThread)
{
    DWORD dwRetVal = THREAD_PRIORITY_ERROR_RETURN;
    BC_THRD(hThread);
    CeGetThreadPriority(hThread, &dwRetVal);
    return dwRetVal;
}

extern "C"
BOOL WINAPI xxx_CeSetThreadPriority(HANDLE hThread, int nPriority)
{
    BC_THRD(hThread);
    return CeSetThreadPriority(hThread,nPriority);
}

extern "C"
DWORD WINAPI xxx_CeGetThreadQuantum(HANDLE hThread)
{
    DWORD dwRetVal = MAXDWORD;
    BC_THRD(hThread);
    CeGetThreadQuantum(hThread, &dwRetVal);
    return dwRetVal;
}

extern "C"
BOOL WINAPI xxx_CeSetThreadQuantum(HANDLE hThread, DWORD dwTime)
{
    BC_THRD(hThread);
    return CeSetThreadQuantum(hThread,dwTime);
}

extern "C"
BOOL
xxx_TerminateThread(
    HANDLE  hThread,
    DWORD   dwExitcode
    )
{
    BC_THRD(hThread);
    return NKKillThread (hThread, dwExitcode, !IsExiting);
}

extern "C"
ULONG
xxx_GetThreadCallStack(
    HANDLE          hThrd,
    ULONG           dwMaxFrames,
    LPVOID          lpFrames,
    DWORD           dwFlags,
    DWORD           dwSkip
    )
{
    BC_THRD(hThrd);
    return GetThreadCallStack (hThrd, dwMaxFrames, lpFrames, dwFlags, dwSkip);
}

extern "C"
ULONG
xxx_GetCallStackSnapshot(
    ULONG            dwMaxFrames,
    CallSnapshot     lpFrames[],
    DWORD            dwFlags,
    DWORD            dwSkip
    )
{
    return xxx_GetThreadCallStack (GetCurrentThread (), dwMaxFrames, lpFrames, dwFlags, dwSkip);
}


extern "C"
BOOL
xxx_CeSetDirectCall (BOOL fKernel)
{
    return CeSetDirectCall (fKernel);
}

extern "C"
VOID
WINAPI
xxx_SetLastError(
    DWORD   dwErrCode
    )
{
    SetLastError(dwErrCode);
}

extern "C"
DWORD
WINAPI
xxx_GetLastError(
    void
    )
{
    return GetLastError();
}

extern "C"
BOOL
WINAPI
xxx_GetExitCodeThread(
    HANDLE  hThread,
    LPDWORD lpExitCode
    )
{
    BC_THRD (hThread);
    return GetExitCodeThread(hThread,lpExitCode);
}

extern "C"
BOOL WINAPI xxx_GetExitCodeProcess(HANDLE hProcess, LPDWORD lpExitCode)
{
    BC_PROC (hProcess);
    return GetExitCodeProcess(hProcess,lpExitCode);
}

extern "C"
int
WINAPI
xxx_GetProcessId (
    HANDLE  hProcess
    )
{
    BC_PROC (hProcess);
    return GetProcessId (hProcess);
}

//
// support extended TLS for user processes
//
extern "C"
BOOL
WINAPI
xxx_CeGetProcessAccount (
    HANDLE hProcess,
    PACCTID pAcctId,
    DWORD cbAcctId
    )
{
    RETAILMSG(1, (TEXT("\r\n\r\n*** CeGetProcessAccount() called - NOT SUPPORTED IN CE7.\r\n\r\n")));
    ASSERT(0);
    return FALSE;
}

extern "C"
BOOL
WINAPI
xxx_CeGetThreadAccount (
    HANDLE hThread,
    PACCTID pAcctId,
    DWORD cbAcctId
    )
{
    RETAILMSG(1, (TEXT("\r\n\r\n*** CeGetThreadAccount() called - NOT SUPPORTED IN CE7.\r\n\r\n")));
    ASSERT(0);
    return FALSE;
}

extern "C"
HANDLE
xxx_GetOwnerProcess(
    void
    )
{
    return (HANDLE) __GetUserKData (PCB_OWNER_PROCESS_OFFSET);
}

//
// support extended TLS for user processes
//
extern "C"
DWORD
xxx_TlsCall(
    DWORD   p1,
    DWORD   p2
    )
{
    DWORD fRet = 0;

    switch (p1) {

        case TLS_FUNCALLOC:
            // TlsAlloc
            fRet = TlsCall (TLS_FUNCALLOC, p2);
            break;

        case CETLS_FUNCALLOC:
            // CeTlsAlloc
            fRet = TlsCall (TLS_FUNCALLOC, p2);
            if ((fRet != TLS_OUT_OF_INDEXES) && p2)
                if (!CeTlsSetCleanupFunction (fRet, (PFNVOID)p2)) {
                    // free the slot created and fail the call
                    TlsCall (TLS_FUNCFREE, fRet);
                    fRet = TLS_OUT_OF_INDEXES;
                }
            break;

        case TLS_FUNCFREE:
            // TlsFree
            CeTlsFreeInAllThreads (p2);            
            fRet = TlsCall (p1, p2);
            break;

        case CETLS_FUNCFREE:
            // CeTlsFree
            fRet = CeTlsFreeInCurrentThread(p2);
            break;

        case CETLS_THRDEXIT:
            // Internal (called on thread exit)
            fRet = CeTlsThreadExit ();
            break;

        default:
            break;
        }

    return fRet;
}

#pragma warning(push)
#pragma warning(disable:4995)
// This is the API we exposed. Would be great if we can remove them
extern "C"
BOOL
WINAPI
xxx_IsBadCodePtr(
    FARPROC lpfn
    )
{
    return IsBadCodePtr (lpfn);
}

extern "C"
BOOL
WINAPI
xxx_IsBadReadPtr(
    CONST   VOID*   lp,
            UINT    ucb
    )
{
    return IsBadReadPtr (lp, ucb);
}

extern "C"
BOOL
WINAPI
xxx_IsBadWritePtr(
    LPVOID  lp,
    UINT    ucb
    )
{
    return IsBadWritePtr (lp, ucb);
}
#pragma warning(pop)

extern "C"
LPVOID
WINAPI
xxx_VirtualAlloc(
    LPVOID  lpAddress,
    DWORD   dwSize,
    DWORD   flAllocationType,
    DWORD   flProtect
    )
{
#ifndef KCOREDLL

    LPVOID lpRet = NULL;
    //
    // BC Workaround for allocation in ram-backed memory
    // mapped file address regions with commit option
    //
    if (IsInRAMMapSection((DWORD)lpAddress)
        && (flAllocationType & MEM_COMMIT)) {
        __try {
            DWORD dwStart = (DWORD) lpAddress;
            DWORD dwEnd = dwStart + dwSize;
            BYTE bOld;

            // page align the start address
            dwStart &= ~VM_PAGE_OFST_MASK;

            // try to touch each page
            while (dwStart < dwEnd) {
                bOld = *((LPBYTE)dwStart);
                if (PAGE_READWRITE & flProtect)
                    *((volatile BYTE*)dwStart) = bOld;
                dwStart += VM_PAGE_SIZE;
            }
            lpRet = lpAddress;
        } _except(EXCEPTION_EXECUTE_HANDLER) {
            lpRet = NULL;
        }
        return lpRet;
    }
#endif

    return VirtualAllocEx (hActiveProc, lpAddress,dwSize,flAllocationType,flProtect, NULL);
}

extern "C"
BOOL
WINAPI
xxx_VirtualFree(
    LPVOID  lpAddress,
    DWORD   dwSize,
    DWORD   dwFreeType
    )
{
    return VirtualFreeEx (hActiveProc, lpAddress,dwSize,dwFreeType, 0);
}

extern "C"
LPVOID
WINAPI
xxx_VirtualAllocEx(
    HANDLE  hProcess,
    LPVOID  lpAddress,
    DWORD   dwSize,
    DWORD   flAllocationType,
    DWORD   flProtect
    )
{
    BC_PROC (hProcess);
    return VirtualAllocEx (hProcess, lpAddress,dwSize,flAllocationType,flProtect, NULL);
}

extern "C"
LPVOID
WINAPI
xxx_VirtualAllocWithTag(
    HANDLE  hProcess,
    LPVOID  lpAddress,
    DWORD   dwSize,
    DWORD   flAllocationType,
    DWORD   flProtect,
    PDWORD  pdwTag
    )
{
    BC_PROC (hProcess);
    return VirtualAllocEx (hProcess, lpAddress,dwSize,flAllocationType,flProtect, pdwTag);
}

extern "C"
BOOL
WINAPI
xxx_VirtualFreeEx(
    HANDLE  hProcess,
    LPVOID  lpAddress,
    DWORD   dwSize,
    DWORD   dwFreeType
    )
{
    BC_PROC (hProcess);
    return VirtualFreeEx (hProcess, lpAddress,dwSize,dwFreeType, 0);
}

extern "C"
BOOL
WINAPI
xxx_VirtualFreeWithTag(
    HANDLE  hProcess,
    LPVOID  lpAddress,
    DWORD   dwSize,
    DWORD   dwFreeType,
    DWORD   dwTag
    )
{
    BC_PROC (hProcess);
    return VirtualFreeEx (hProcess, lpAddress,dwSize,dwFreeType, dwTag);
}

extern "C"
BOOL
WINAPI
xxx_VirtualProtectEx(
    HANDLE  hProcess,
    LPVOID  lpAddress,
    DWORD   dwSize,
    DWORD   flNewProtect,
    PDWORD  lpflOldProtect
    )
{
    BC_PROC (hProcess);
    // follow WIN32 spec, lpfOldProtect must not be NULL
    if (!lpflOldProtect) {
        SetLastError (ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    return VirtualProtectEx (hProcess, lpAddress,dwSize,flNewProtect,lpflOldProtect);
}

extern "C"
BOOL
WINAPI
xxx_CeLoadLibraryInProcess(
    HANDLE  hProcess,
    LPCWSTR lpwszModuleName
    )
{
    BC_PROC (hProcess);
    return CeLoadLibraryInProcess (hProcess, lpwszModuleName);
}


extern "C"
BOOL
WINAPI
xxx_VirtualProtect(
    LPVOID  lpAddress,
    DWORD   dwSize,
    DWORD   flNewProtect,
    PDWORD  lpflOldProtect
    )
{
    return xxx_VirtualProtectEx (hActiveProc, lpAddress,dwSize,flNewProtect,lpflOldProtect);
}

extern "C"
DWORD
WINAPI
xxx_VirtualQueryEx(
    HANDLE                      hProcess,
    LPCVOID                     lpAddress,
    PMEMORY_BASIC_INFORMATION   lpBuffer,
    DWORD                       dwLength
    )
{
    BC_PROC (hProcess);
    return VirtualQueryEx (hProcess, lpAddress,lpBuffer,dwLength);
}

extern "C"
DWORD
WINAPI
xxx_VirtualQuery(
    LPCVOID                     lpAddress,
    PMEMORY_BASIC_INFORMATION   lpBuffer,
    DWORD                       dwLength
    )
{
    return VirtualQueryEx (hActiveProc, lpAddress,lpBuffer,dwLength);
}

extern "C"
LPVOID
WINAPI
xxx_CeVirtualSharedAlloc(
    LPVOID  lpAddress,
    DWORD   dwSize,
    DWORD   fdwAction
    )
{
#ifdef KCOREDLL
    return CeVirtualSharedAlloc (lpAddress, dwSize, fdwAction, NULL);
#else
    SetLastError (ERROR_ACCESS_DENIED);
    return NULL;
#endif
}

extern "C"
LPVOID
WINAPI
xxx_CeVirtualSharedAllocWithTag(
    LPVOID  lpAddress,
    DWORD   dwSize,
    DWORD   fdwAction,
    PDWORD  pdwTag
    )
{
#ifdef KCOREDLL
    return CeVirtualSharedAlloc (lpAddress, dwSize, fdwAction, pdwTag);
#else
    SetLastError (ERROR_ACCESS_DENIED);
    return NULL;
#endif
}

extern "C"
BOOL
xxx_CeMapUserAddressesToVoid(
    HANDLE  hProc,
    LPVOID  lpAddress,
    DWORD   cbSize
    )
{
#ifdef KCOREDLL
    return CeMapUserAddressesToVoid (hProc, lpAddress, cbSize);
#else
    SetLastError (ERROR_ACCESS_DENIED);
    return FALSE;
#endif
}

extern "C"
BOOL
WINAPI
xxx_LockPages(
    LPVOID  lpvAddress,
    DWORD   cbSize,
    PDWORD  pPFNs,
    int     fOptions
    )
{
    return LockPagesEx ((HANDLE) GetCallerVMProcessId(), lpvAddress, cbSize, pPFNs, fOptions);
}

extern "C"
BOOL
WINAPI
xxx_UnlockPages(
    LPVOID  lpvAddress,
    DWORD   cbSize
    )
{
    return UnlockPagesEx ((HANDLE) GetCallerVMProcessId(), lpvAddress,cbSize);
}

extern "C"
LPVOID
WINAPI
xxx_AllocPhysMem(
    DWORD   cbSize,
    DWORD   fdwProtect,
    DWORD   dwAlignmentMask,
    DWORD   dwFlags,
    PULONG  pPhysicalAddress
    )
{
    return AllocPhysMemEx (hActiveProc, cbSize, fdwProtect, dwAlignmentMask, dwFlags, pPhysicalAddress);
}

extern "C"
BOOL
WINAPI
xxx_FreePhysMem(
    LPVOID  lpvAddress
    )
{
    return VirtualFreeEx (hActiveProc, lpvAddress, 0, MEM_RELEASE, 0);
}

extern "C"
void
WINAPI
xxx_SleepTillTick(
    void
    )
{
    SleepTillTick ();
}

extern "C"
BOOL
xxx_DuplicateHandle(
    HANDLE      hSrcProc,
    HANDLE      hSrcHndl,
    HANDLE      hDstProc,
    LPHANDLE    lpDstHndl,
    DWORD       dwAccess,
    BOOL        bInherit,
    DWORD       dwOptions
    )
{
#ifdef KCOREDLL
    return DuplicateHandle (hSrcProc, hSrcHndl, hDstProc, lpDstHndl, dwAccess, bInherit, dwOptions);
#else
    IDConverter idcSrcProc(hSrcProc, TRUE);
    IDConverter idcDstProc(hDstProc, TRUE);
    return DuplicateHandle (idcSrcProc.GetHandle(), hSrcHndl, idcDstProc.GetHandle(), lpDstHndl, dwAccess, bInherit, dwOptions);
#endif
}

extern "C"
BOOL
xxx_FreeModFromCurrProc(
    PDLLMAININFO    pList,
    DWORD           dwCnt
    )
{
    // should've never called
    DEBUGCHK (0);
    return FALSE;
}

extern "C"
BOOL
xxx_GetProcModList(
    PDLLMAININFO    pList,
    DWORD           dwCnt,
    BOOL            fThreadCall
    )
{
    // should've never called
    DEBUGCHK (0);
    return FALSE;
}


extern "C"
LPBYTE
WINAPI
xxx_CreateLocaleView(
    LPDWORD pdwSize
    )
{
    return CreateLocaleView (pdwSize);
}

extern "C"
DWORD
WINAPI
xxx_GetTickCount(
    void
    )
{
    return GetTickCount();
}

extern "C"
DWORD
WINAPI
xxx_GetProcessVersion(
    DWORD   dwProcessId
    )
{
    return GetProcessVersion (dwProcessId);
}

extern "C"
DWORD
WINAPI
xxx_GetModuleFileNameW(
    HMODULE hModule,
    LPWSTR  lpFilename,
    DWORD   nSize
    )
{
    HANDLE hProc = hActiveProc;
    DWORD  dwRet;

    if ((DWORD) hModule & 0x3) {
        // hModule is a process handle
        hProc = hModule;
        hModule = NULL;
    }
    {
        BC_PROC (hProc);
        dwRet = GetModuleName (hProc, hModule, lpFilename, nSize);;
    }

    return dwRet;
}

extern "C"
HMODULE
WINAPI
xxx_GetModuleHandleW(
    LPCWSTR lpModuleName
    )
{
    HMODULE hModule = (HMODULE)GetCurrentProcessId();
    if (lpModuleName) {
        hModule = GetModHandle (hActiveProc, lpModuleName, 0);
        if (!hModule)
            hModule = GetModHandle (hActiveProc, lpModuleName, LOAD_LIBRARY_AS_DATAFILE);
    }
    return hModule;
}

extern "C"
BOOL
WINAPI
xxx_QueryPerformanceCounter(
    LARGE_INTEGER*  lpPerformanceCount
    )
{
    return QueryPerformanceCounter (lpPerformanceCount);
}

extern "C"
BOOL
WINAPI
xxx_QueryPerformanceFrequency(
    LARGE_INTEGER*  lpFrequency
    )
{
    return QueryPerformanceFrequency (lpFrequency);
}

extern "C"
VOID
WINAPI
xxx_WriteDebugLED(
    WORD    wIndex,
    DWORD   dwPattern
    )
{
    WriteDebugLED (wIndex, dwPattern);
}

extern "C"
void
WINAPI
xxx_ForcePageout(
    void
    )
{
    ForcePageout ();
}

extern "C"
BOOL
WINAPI
xxx_GetThreadTimes(
    HANDLE      hThread,
    LPFILETIME  lpCreationTime,
    LPFILETIME  lpExitTime,
    LPFILETIME  lpKernelTime,
    LPFILETIME  lpUserTime
    )
{
    BC_THRD (hThread);
    return GetThreadTimes (hThread, lpCreationTime, lpExitTime, lpKernelTime, lpUserTime);
}

extern "C"
BOOL
WINAPI
xxx_GetProcessTimes(
    HANDLE      hProcess,
    LPFILETIME  lpCreationTime,
    LPFILETIME  lpExitTime,
    LPFILETIME  lpKernelTime,
    LPFILETIME  lpUserTime
    )
{
    BC_PROC (hProcess);
    return GetProcessTimes (hProcess, lpCreationTime, lpExitTime, lpKernelTime, lpUserTime);
}

extern "C"
VOID
WINAPI
xxx_OutputDebugStringW(
    LPCWSTR lpOutputString
    )
{
    OutputDebugStringW (lpOutputString);
}

extern "C"
VOID
WINAPI
xxx_GetSystemInfo(
    LPSYSTEM_INFO   lpSystemInfo
    )
{
    GetSystemInfo (lpSystemInfo);
}

extern "C"
BOOL
WINAPI
xxx_IsProcessorFeaturePresent(
    DWORD   ProcessorFeature
    )
{
    return IsProcessorFeaturePresent (ProcessorFeature);
}

extern "C"
BOOL
WINAPI
xxx_QueryInstructionSet(
    DWORD   dwInstructionSet,
    LPDWORD lpdwCurrentInstructionSet
    )
{
    return QueryInstructionSet (dwInstructionSet, lpdwCurrentInstructionSet);
}

extern "C"
VOID
WINAPI
xxx_RaiseException(
            DWORD   dwExceptionCode,
            DWORD   dwExceptionFlags,
            DWORD   nNumberOfArguments,
    CONST   DWORD*  lpArguments
    )
{
    RaiseException(dwExceptionCode,dwExceptionFlags, nNumberOfArguments, lpArguments);
}

extern "C"
BOOL
xxx_RegisterDbgZones(
    HMODULE     hMod,
    LPDBGPARAM  lpdbgparam
    )
{
    return RegisterDbgZones (hMod, lpdbgparam);
}

extern "C"
void
xxx_SetDaylightTime(
    DWORD   dst
    )
{
    SetDaylightTimeEx (dst, FALSE);
}


//------------------------------------------------------------------------------
// MEMORY-MAPPED FILES
//------------------------------------------------------------------------------

//
// CreateFileForMapping semantics are now wholly implemented inside coredll
// instead of the kernel.  We now keep a per-process CFFM handle list which
// is maintained by CreateFileForMapping, CreateFileMapping, and CloseHandle.
// The implementation is slightly different between kernel and user mode.
// In kernel mode, only CFFM handles that haven't yet been passed to CFM will
// be on the list.  In user mode, CFFM handles will remain on the list until
// the mapping is closed.  This represents a trade-off between CloseHandle
// performance in the kernel and CFFM backward-compatibility in user mode.
//
// CFFM is actually just as backward-compatible in kernel mode as it is in user
// mode, despite the different implementation.  In user mode, the application
// can pass the file handle to file operations like ReadFile and WriteFile until
// the mapping is closed.  In kernel mode, the file handle is closed as soon as
// it's passed to CreateFileMapping, but since the kernel memory-mapped file
// code still has a KERNEL reference on the file, the file handle can STILL be
// used with file calls until the mapping is closed.  Thus the kernel can
// minimize list-walking in CloseHandle by taking advantage of the fact that
// the kernel process is a special case in mapping handle ownership.
//
// Reasons to keep the CreateFileForMapping implementation inside coredll
// instead of the kernel:
//      - Coredll implementation makes it easy to route to CreateFile.  Inside
//        the kernel it's tougher to call CreateFile on behalf of the caller.
//      - Coredll makes it easier to keep per-process lists, which minimize
//        CloseHandle list walking and are only costly to those processes which
//        actually call CreateFileForMapping.
//

typedef struct _CFFMNODE {
    struct _CFFMNODE *pNext;
    HANDLE hCffm;
    HANDLE hMap;
} CFFMNODE, *PCFFMNODE;

static PCFFMNODE g_pCffmList;

static BOOL AutoCloseCFFMHandle (HANDLE h)
{
    PCFFMNODE pCurr, pPrev = NULL;
    BOOL fRet = FALSE;

    // Optimization: Skip grabbing the CS if the list is empty.
    if (g_pCffmList
        && (INVALID_HANDLE_VALUE != h) && (NULL != h)) {

        LockProcCFFM ();
        for (pCurr = g_pCffmList; pCurr; pPrev = pCurr, pCurr = pCurr->pNext) {
            // Backward-compatible behavior: After someone passes a file handle
            // to CreateFileMapping, they no longer can close the file handle
            // directly.  They must close the mapping first.
            if ((pCurr->hMap == h)
                || ((pCurr->hCffm == h) && !pCurr->hMap)) {
                // remove the node from the list
                if (pCurr == g_pCffmList) {
                    g_pCffmList = pCurr->pNext;
                } else {
                    pPrev->pNext = pCurr->pNext;
                }
                break;
            }
        }
        UnlockProcCFFM ();

        if (pCurr) {
            // auto-close the handle from CreateFileForMapping
            DEBUGMSG (1, (L"Auto-Closing CFFM handle %8.8lx for map %8.8lx\r\n", pCurr->hCffm, h));
            CloseHandleInProc (hActiveProc, pCurr->hCffm);
            fRet = (pCurr->hCffm == h);
            LocalFree (pCurr);
        }
    }

    return fRet;
}


#ifndef KCOREDLL
// AssociateCFFMHandle is only used in user mode
static BOOL AssociateCFFMHandle (HANDLE hCffm, HANDLE hMap)
{
    PCFFMNODE pCurr;

    DEBUGCHK (hMap);

    LockProcCFFM ();
    for (pCurr = g_pCffmList; pCurr; pCurr = pCurr->pNext) {
        if (pCurr->hCffm == hCffm) {
            DEBUGMSG (1, (L"Associating CFFM handle %8.8lx with map %8.8lx\r\n", pCurr->hCffm, hMap));
            pCurr->hMap = hMap;
            break;
        }
    }
    UnlockProcCFFM ();

    return NULL != pCurr;
}
#endif // KCOREDLL


static BOOL AddCFFMHandle (HANDLE hCffm)
{
    PCFFMNODE pNode;

    pNode = (PCFFMNODE) LocalAlloc (LMEM_FIXED, sizeof (CFFMNODE));
    if (pNode) {
        pNode->hCffm = hCffm;
        pNode->hMap  = NULL;

        LockProcCFFM ();
        pNode->pNext = g_pCffmList;
        g_pCffmList  = pNode;
        UnlockProcCFFM ();
    }

    return (NULL != pNode);
}


extern "C"
HANDLE
xxx_CreateFileMappingW(
    HANDLE                  hFile,
    LPSECURITY_ATTRIBUTES   lpsa,
    DWORD                   flProtect,
    DWORD                   dwMaxSizeHigh,
    DWORD                   dwMaxSizeLow,
    LPCWSTR                 lpName
    )
{
    HANDLE hMap = CreateFileMappingInProc (hActiveProc, hFile, lpsa, flProtect, dwMaxSizeHigh, dwMaxSizeLow, lpName);

    // If there's a file handle that was opened with CreateFileForMapping, it
    // must be managed properly
    if (INVALID_HANDLE_VALUE != hFile) {

#ifdef KCOREDLL

        // In the kernel, for perf reasons (not wanting to walk a CFFM list on
        // every CloseHandle call), we close the CFFM reference immediately.
        // The mapping still has a reference to the file, so it will remain
        // open until the mapping is closed.  If the mapping was not created,
        // this call will close the file immediately.
        AutoCloseCFFMHandle (hFile);

#else  // KCOREDLL

        // In user mode, for backward-compatibility reasons (allowing the file
        // handle to be usable to the caller), we hold a file handle from CFFM
        // open until the mapping is closed.
        if (hMap) {
            AssociateCFFMHandle (hFile, hMap);
        } else {
            // The mapping was not created; auto-close the file handle
            AutoCloseCFFMHandle (hFile);
        }

#endif // KCOREDLL

    }

    return hMap;
}

extern "C"
HANDLE
xxx_CreateFileForMappingW(
    LPCTSTR                 lpFileName,
    DWORD                   dwDesiredAccess,
    DWORD                   dwShareMode,
    LPSECURITY_ATTRIBUTES   lpSecurityAttributes,
    DWORD                   dwCreationDisposition,
    DWORD                   dwFlagsAndAttributes,
    HANDLE                  hTemplateFile
    )
{
    HANDLE hCffm;

    RETAILMSG (1, (L"WARNING: CreateFileForMapping is being deprecated! Use CreateFile+CloseHandle instead.\r\n"));

#ifdef KCOREDLL
    /* -------------------------------------------------------------------------
    // For performance reasons (avoiding list walking during CloseHandle) we
    // are trying to deprecate this API.  Kernel mode code should not be
    // calling it at all.  Please change your code from this:

    hFile = CreateFileForMapping(...);
    if (INVALID_HANDLE_VALUE != hFile) {
        // Never call CloseHandle on hFile
        hMap = CreateFileMapping(hFile, ...);
        // check for NULL hMap
    }

    // to this (note the new CloseHandle call):

    hFile = CreateFile(...);
    if (INVALID_HANDLE_VALUE != hFile) {
        hMap = CreateFileMapping(hFile, ...);
        CloseHandle(hFile);  // The kernel would now have its own reference to the file
        // check for NULL hMap
    }
    ------------------------------------------------------------------------- */

    DEBUGCHK (0);
#endif

    // Force minimal share mode
    if (dwShareMode & FILE_SHARE_WRITE_OVERRIDE) {
        dwShareMode = FILE_SHARE_WRITE | FILE_SHARE_READ;
    } else {
        dwShareMode = (GENERIC_READ == dwDesiredAccess) ? FILE_SHARE_READ : 0;
    }

    hCffm = CreateFileW (lpFileName,
                dwDesiredAccess,
                dwShareMode,
                lpSecurityAttributes,
                dwCreationDisposition,
                dwFlagsAndAttributes,
                hTemplateFile);

    // Track the file handle inside coredll, to provide the proper CloseHandle
    // semantics.
    if (INVALID_HANDLE_VALUE != hCffm) {
        AddCFFMHandle (hCffm);
    }

    return hCffm;
}

extern "C"
BOOL
xxx_UnmapViewOfFile(
    LPVOID lpvAddr
    )
{
    return UnmapViewInProc (hActiveProc, lpvAddr);
}

extern "C"
BOOL
xxx_UnmapViewOfFileInProcess (
    HANDLE hProcess,
    LPVOID lpvAddr
    )
{
    BC_PROC (hProcess);
    return UnmapViewInProc (hProcess, lpvAddr);
}

extern "C"
BOOL
xxx_FlushViewOfFile(
    LPCVOID lpBaseAddress,
    DWORD   dwNumberOfBytesToFlush
    )
{
    return FlushViewInProc (hActiveProc, lpBaseAddress, dwNumberOfBytesToFlush);
}

extern "C"
BOOL
xxx_FlushViewOfFileMaybe(
    LPCVOID lpBaseAddress,
    DWORD   dwNumberOfBytesToFlush
    )
{
    RETAILMSG (1, (L"!!!NOT SUPPORTED: Calling xxx_FlushViewOfFileMaybe!!!!!\r\n"));
    return TRUE;
}

extern "C"
LPVOID
xxx_MapViewOfFile(
    HANDLE  hMap,
    DWORD   fdwAccess,
    DWORD   dwOffsetHigh,
    DWORD   dwOffsetLow,
    DWORD   cbMap
    )
{
    return MapViewInProc (hActiveProc, hMap, fdwAccess, dwOffsetHigh, dwOffsetLow, cbMap);
}

extern "C"
LPVOID
xxx_MapViewOfFileInProcess (
    HANDLE  hProcess,
    HANDLE  hMap,
    DWORD   fdwAccess,
    DWORD   dwOffsetHigh,
    DWORD   dwOffsetLow,
    DWORD   cbMap
    )
{
    BC_PROC (hProcess);
    return MapViewInProc (hProcess, hMap, fdwAccess, dwOffsetHigh, dwOffsetLow, cbMap);
}

//------------------------------------------------------------------------------


extern "C"
BOOL
xxx_KernelIoControl(
    DWORD   dwIoControlCode,
    LPVOID  lpInBuf,
    DWORD   nInBufSize,
    LPVOID  lpOutBuf,
    DWORD   nOutBufSize,
    LPDWORD lpBytesReturned
    )
{

#ifndef KCOREDLL
     /* For the IOCTL_EDBG_XXX ioctls, we must package the parameters in a struct so that we
      * can pass the kernel's system call validation (validateArgs in apicall.c).
      */
    switch(dwIoControlCode){
    case IOCTL_EDBG_REGISTER_CLIENT:
    {
        EDBG_REGISTER_CLIENT_IN regClientIn;     //input parameters to register client
        UCHAR *pId = (UCHAR*)lpInBuf;  //output parameter to register client
        regClientIn.szServiceName = (LPSTR)lpOutBuf;
        regClientIn.Flags = (UCHAR)nInBufSize;
        regClientIn.WindowSize = (UCHAR)nOutBufSize;
        return KernelIoControl (dwIoControlCode, &regClientIn, sizeof(regClientIn), pId, sizeof(pId), NULL);
    }

    case IOCTL_EDBG_DEREGISTER_CLIENT:
    {
        UCHAR Id = (UCHAR)nInBufSize;  //input parameter to DEregister client
        return KernelIoControl (dwIoControlCode, &Id, sizeof(Id), NULL, 0, NULL);
    }

    case IOCTL_EDBG_REGISTER_DFLT_CLIENT:
    {
        EDBG_REGISTER_DFLT_CLIENT_IN regDfltClientIn;      //input parameters to register dflt client
        regDfltClientIn.Service= (UCHAR)nInBufSize;
        regDfltClientIn.Flags= (UCHAR)nOutBufSize;
        return KernelIoControl (dwIoControlCode, &regDfltClientIn, sizeof(regDfltClientIn), NULL, 0, NULL);
    }

    case IOCTL_EDBG_SEND:
    {
        EDBG_SEND_IN edbgSendIn;     //input parameters to edbg send
        edbgSendIn.Id = (UCHAR)nInBufSize;
        edbgSendIn.pUserData = (UCHAR *)lpInBuf;
        edbgSendIn.dwUserDataLen = nOutBufSize;
        return KernelIoControl (dwIoControlCode, &edbgSendIn, sizeof(edbgSendIn), NULL, 0, NULL);
    }

    case IOCTL_EDBG_RECV:
    {
        EDBG_RECV_IN edbgRecvIn;     //input parameters to edbg recv
        EDBG_RECV_OUT edbgRecvOut;
        edbgRecvIn.Id = (UCHAR)nInBufSize;
        edbgRecvIn.dwTimeout = nOutBufSize;
        edbgRecvOut.pRecvBuf= (UCHAR *)lpInBuf;
        edbgRecvOut.pdwLen= (DWORD *)lpOutBuf;
        return KernelIoControl (dwIoControlCode, &edbgRecvIn, sizeof(edbgRecvIn), &edbgRecvOut, sizeof(edbgRecvOut), NULL);
    }

    case IOCTL_EDBG_SET_DEBUG:
    {
        DWORD dwZoneMask = nInBufSize;  //input parameter to edbg set debug
        return KernelIoControl (dwIoControlCode, &dwZoneMask, sizeof(dwZoneMask), NULL, 0, NULL);
    }

    }
#endif

    return KernelIoControl (dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);

}

#define VERIFY_FLAG_ZERO(dwFlags)  if (dwFlags) { SetLastError (ERROR_INVALID_PARAMETER); return 0; }

//
// watchdog APIs
//
HANDLE CreateWatchDogTimer (
    LPCWSTR pszWatchDogName,
    DWORD dwPeriod,
    DWORD dwWait,
    DWORD dwDfltAction,
    DWORD dwParam,
    DWORD dwFlags)
{
    VERIFY_FLAG_ZERO (dwFlags);
    return NKWDCreate (NULL,  // lpsa, currently always NULL
        pszWatchDogName,
        dwDfltAction,
        dwParam,
        dwPeriod,
        dwWait,
        TRUE);
}

HANDLE OpenWatchDogTimer (
    LPCWSTR pszWatchDogName,
    DWORD dwFlags)
{
    VERIFY_FLAG_ZERO (dwFlags);
    return NKWDCreate (NULL,  // lpsa, currently always NULL
        pszWatchDogName,
        0,
        0,
        0,
        0,
        FALSE);
}

BOOL StartWatchDogTimer (
    HANDLE hWatchDog,
    DWORD dwFlags)
{
    VERIFY_FLAG_ZERO (dwFlags);
    return NKWDStart (hWatchDog);
}

BOOL StopWatchDogTimer (
    HANDLE hWatchDog,
    DWORD dwFlags)
{
    VERIFY_FLAG_ZERO (dwFlags);
    return NKWDStop (hWatchDog);
}

BOOL RefreshWatchDogTimer (
    HANDLE hWatchDog,
    DWORD dwFlags)
{
    VERIFY_FLAG_ZERO (dwFlags);
    return NKWDRefresh (hWatchDog);
}


//--------------------------------------------------------------------------------------------
// Backward compatibility for watchdog timer API using KernelLibIoControl
//--------------------------------------------------------------------------------------------
typedef struct _WDAPIStruct {
    LPCVOID watchdog;          // name (in create/open) or handle (in other) of the watchdog
    DWORD   dwPeriod;           // watchdog period
    DWORD   dwWait;             // wait time before default action taken
    DWORD   dwDfltAction;       // default action
    DWORD   dwParam;            // param passed to IOCTL_HAL_REBOOT if reset is the default action
    DWORD   dwFlags;            // flags
} WDAPIStruct, *PWDAPIStruct;

typedef const WDAPIStruct *PCWDAPIStruct;

//
// watchdog APIs
//
#define WDAPI_CREATE                0
#define WDAPI_OPEN                  1
#define WDAPI_START                 2
#define WDAPI_STOP                  3
#define WDAPI_REFRESH               4

static BOOL WatchDogAPI (DWORD apiCode, PCWDAPIStruct pwdas)
{
    BOOL  fRet  = FALSE;

    VERIFY_FLAG_ZERO (pwdas->dwFlags);

    switch (apiCode) {
    case WDAPI_CREATE:
    case WDAPI_OPEN:
        fRet = (BOOL) NKWDCreate (NULL,  // lpsa, currently always NULL
            (LPCWSTR) pwdas->watchdog,
            pwdas->dwDfltAction,
            pwdas->dwParam,
            pwdas->dwPeriod,
            pwdas->dwWait,
            (WDAPI_CREATE == apiCode));
        break;
    case WDAPI_START:
        fRet = NKWDStart ((HANDLE) pwdas->watchdog);
        break;
    case WDAPI_STOP:
        fRet = NKWDStop ((HANDLE) pwdas->watchdog);
        break;
    case WDAPI_REFRESH:
        fRet = NKWDRefresh ((HANDLE) pwdas->watchdog);
        break;
    default:
        SetLastError (ERROR_INVALID_PARAMETER);
        break;
    }

    return fRet;
}

extern "C"
BOOL
xxx_KernelLibIoControl(
    HANDLE  hLib,
    DWORD   dwIoControlCode,
    LPVOID  lpInBuf,
    DWORD   nInBufSize,
    LPVOID  lpOutBuf,
    DWORD   nOutBufSize,
    LPDWORD lpBytesReturned
    )
{
    BOOL fRet = FALSE;

    // watchdog support is done via KernelLibIoControl prior to Yamazaki. We need to do the same in order to keep BC.
    if (((HANDLE) KMOD_CORE == hLib)
        && (IOCTL_KLIB_WDOGAPI == dwIoControlCode)) {
        // lpInBuf: pointer to WDAPIStruct struct
        // nInBufSize: sizeof (WDAPIStruct)
        // nOutBufSize: apiCode (irregular usage)
        if (sizeof (WDAPIStruct) == nInBufSize) {
            fRet = WatchDogAPI (nOutBufSize, (PCWDAPIStruct) lpInBuf);
        } else {
            SetLastError (ERROR_INVALID_PARAMETER);
        }
    } else if (((HANDLE) KMOD_APPVERIF == hLib)
        && (IOCTL_APPVERIF_ATTACH == dwIoControlCode)
        && (NULL == lpInBuf)) {
        fRet = AppVerifierIoControl (dwIoControlCode, NULL, 0, NULL, 0, NULL);
    } else {
        fRet = KernelLibIoControl (hLib, dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);
    }

    return fRet;
}

extern "C"
LPVOID
xxx_CreateStaticMapping(
    DWORD   dwPhysBase,
    DWORD   dwSize
    )
{
#ifdef KCOREDLL
    return CreateStaticMapping (dwPhysBase, dwSize);
#else
    PVOID pStaticAddr = NULL ;
    REF_CREATE_STATIC_MAPPING createStaticMapping = { dwPhysBase, dwSize };
    if (dwPhysBase && dwSize) {
        if (!REL_UDriverProcIoControl_Trap(IOCTL_REF_CREATE_STATIC_MAPPING,&createStaticMapping,sizeof(createStaticMapping),&pStaticAddr,sizeof(pStaticAddr),NULL))
            pStaticAddr = NULL ;
    }
    else
        SetLastError (ERROR_INVALID_PARAMETER);
    return pStaticAddr ;
#endif
}

extern "C"
BOOL
xxx_DeleteStaticMapping(
    LPVOID  pVirtBase,
    DWORD   dwSize
    )
{
#ifdef KCOREDLL
    return DeleteStaticMapping (pVirtBase, dwSize);
#else
    return FALSE;
#endif
}

extern "C"
BOOL
xxx_ReleaseMutex(
    HANDLE  hMutex
    )
{
    return ReleaseMutex(hMutex);
}

extern "C"
HANDLE
xxx_CreateMutexW(
    LPSECURITY_ATTRIBUTES   lpsa,
    BOOL                    bInitialOwner,
    LPCTSTR                 lpName
    )
{
    return CreateMutexW(lpsa, bInitialOwner,lpName);
}

extern "C"
HANDLE
xxx_CreateSemaphoreW(
    LPSECURITY_ATTRIBUTES   lpsa,
    LONG                    lInitialCount,
    LONG                    lMaximumCount,
    LPCWSTR                 lpName
    )
{
    return CreateSemaphoreW(lpsa, lInitialCount, lMaximumCount, lpName);
}

extern "C"
BOOL
xxx_ReleaseSemaphore(
    HANDLE  hSemaphore,
    LONG    lReleaseCount,
    LPLONG  lpPreviousCount
    )
{
    return ReleaseSemaphore(hSemaphore, lReleaseCount, lpPreviousCount);
}

// OAK exports (pkfuncs.h)

extern "C"
BOOL xxx_AddEventAccess(HANDLE hEvent) {
    return FALSE;
}

extern "C"
HANDLE
xxx_CreateAPISet(
            char        acName[4],
            USHORT      cFunctions,
    const   PFNVOID*    ppfnMethods,
    const   ULONGLONG*  pu64Sig
    )
{
    return CreateAPISet(acName, cFunctions, ppfnMethods, pu64Sig);
}

extern "C"
BOOL
xxx_VirtualCopy(
    LPVOID  lpvDest,
    LPVOID  lpvSrc,
    DWORD   cbSize,
    DWORD   fdwProtect
    )
{
#ifdef KCOREDLL
    return VirtualCopyEx (hActiveProc, lpvDest, hActiveProc, lpvSrc, cbSize, fdwProtect);
#else
    if ((PAGE_PHYSICAL & fdwProtect)!=0) { // Mapping Phyisical Page.
        REF_VIRTUALCOPY_PARAM fnVirtualCopyParam = {
            lpvDest,lpvSrc,cbSize,fdwProtect
        };
        return REL_UDriverProcIoControl_Trap(IOCTL_REF_VIRTUAL_COPY,&fnVirtualCopyParam,sizeof(fnVirtualCopyParam),NULL,0,NULL);
    }
    else
        return VirtualCopyEx (hActiveProc, lpvDest, hActiveProc, lpvSrc, cbSize, fdwProtect);
#endif
}

extern "C"
LPVOID
xxx_VirtualAllocCopyEx (
    HANDLE hSrcProc,
    HANDLE hDstProc,
    LPVOID pAddr,
    DWORD cbSize,
    DWORD dwProtect)
{
    return VirtualAllocCopyEx (hSrcProc, hDstProc, pAddr, cbSize, dwProtect);
}


extern "C"
BOOL
xxx_VirtualCopyEx(
    HANDLE hDstProc,
    LPVOID lpvDest,
    HANDLE hSrcProc,
    LPVOID lpvSrc,
    DWORD cbSize,
    DWORD fdwProtect)
{
    return VirtualCopyEx (hDstProc, lpvDest, hSrcProc, lpvSrc, cbSize, fdwProtect);
}

extern "C"
BOOL
xxx_VirtualSetPageFlags(
    LPVOID lpvAddress,
    DWORD cbSize,
    DWORD dwFlags,
    LPDWORD lpdwOldFlags)
{
#ifdef SH4

    DWORD bits;
    BOOL  fRet;

    // translate dwFlags into bit fields
    switch (dwFlags & ~VSPF_TC) {
        case VSPF_NONE:
            bits = 0;
            break;
        case VSPF_VARIABLE:
            bits = 1;
            break;
        case VSPF_IOSPACE:
            bits = 2;
            break;
        case VSPF_IOSPACE | VSPF_16BIT:
            bits = 3;
            break;
        case VSPF_COMMON:
            bits = 4;
            break;
        case VSPF_COMMON | VSPF_16BIT:
            bits = 5;
            break;
        case VSPF_ATTRIBUTE:
            bits = 6;
            break;
        case VSPF_ATTRIBUTE | VSPF_16BIT:
            bits = 7;
            break;
        default: {
            SetLastError (ERROR_INVALID_PARAMETER);
            return FALSE;
        }
    }
    bits <<= 29;
    if (dwFlags & VSPF_TC)
       bits |= (1 << 9);

#define SH4_ATTRIB_MASK     0xE0000200  // top 3 bits, and bit 9 can be changed
    fRet = VirtualSetAttributesEx (hActiveProc, lpvAddress, cbSize, bits, SH4_ATTRIB_MASK, lpdwOldFlags);

    // translate back the old flags
    if (lpdwOldFlags) {
        bits = *lpdwOldFlags;
        switch (bits >> 29) {
                case 0:
                *lpdwOldFlags = VSPF_NONE;
                break;
            case 1:
                *lpdwOldFlags = VSPF_VARIABLE;
                break;
            case 2:
                *lpdwOldFlags = VSPF_IOSPACE;
                break;
            case 3:
                *lpdwOldFlags = VSPF_IOSPACE | VSPF_16BIT;
                break;
            case 4:
                *lpdwOldFlags = VSPF_COMMON;
                break;
            case 5:
                *lpdwOldFlags = VSPF_COMMON | VSPF_16BIT;
                break;
            case 6:
                *lpdwOldFlags = VSPF_ATTRIBUTE;
                break;
            case 7:
                *lpdwOldFlags = VSPF_ATTRIBUTE | VSPF_16BIT;
                break;
        }
        if ((bits >> 9) & 1)
            *lpdwOldFlags |= VSPF_TC;
    }
    return fRet;

#else

    return FALSE;

#endif
}

extern "C"
BOOL
xxx_SetRAMMode(
    BOOL    bEnable,
    LPVOID* lplpvAddress,
    LPDWORD lpLength
    )
{
    return SetRAMMode(bEnable, lplpvAddress, lpLength);
}

extern "C"
LPVOID
xxx_SetStoreQueueBase(
    DWORD   dwPhysPage
    )
{
    return SetStoreQueueBase(dwPhysPage);
}

extern "C"
BOOL xxx_VirtualSetAttributes (LPVOID lpvAddress, DWORD cbSize, DWORD dwNewFlags, DWORD dwMask, LPDWORD lpdwOldFlags)
{
    return VirtualSetAttributesEx (hActiveProc, lpvAddress, cbSize, dwNewFlags, dwMask, lpdwOldFlags);
}

extern "C"
BOOL xxx_VirtualSetAttributesEx (HANDLE hProc, LPVOID lpvAddress, DWORD cbSize, DWORD dwNewFlags, DWORD dwMask, LPDWORD lpdwOldFlags)
{
    return VirtualSetAttributesEx (hProc, lpvAddress, cbSize, dwNewFlags, dwMask, lpdwOldFlags);
}


extern "C"
DWORD
xxx_CeModuleJit(
    LPCWSTR lpstr1,
    LPWSTR  lpstr2,
    HANDLE* ph
    )
{
    return 0;   // no longer used
}

#define MAX_FILENAME_LEN 256

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#define _O_RDONLY       0x0000  /* open for reading only */
#define _O_WRONLY       0x0001  /* open for writing only */
#define _O_RDWR         0x0002  /* open for reading and writing */
#define _O_APPEND       0x0008  /* writes done at eof */

#define _O_CREAT        0x0100  /* create and open file */
#define _O_TRUNC        0x0200  /* open and truncate */
#define _O_EXCL         0x0400  /* open only if file doesn't already exist */

//------------------------------------------------------------------------------
// Helper Functions for ropen
//------------------------------------------------------------------------------
DWORD ConvertAccessMode(DWORD oldMode)
{
    DWORD mode;

    /* Dos Int21 func 3dh,6ch open mode */
    switch (oldMode & 3) {
        case _O_RDWR:   mode = GENERIC_READ | GENERIC_WRITE;    break;
        case _O_WRONLY: mode = GENERIC_WRITE;                   break;
        case _O_RDONLY: mode = GENERIC_READ;                    break;
        default:        mode = 0;                               break;
    }
    return mode;
}

DWORD ConvertCreateMode(DWORD oldMode)
{
    DWORD mode;

    if ((oldMode & 0xFFFF0000) == 0) {
        // Support C-Runtime constants
        switch (oldMode & 0xFF00) {
            case _O_CREAT | _O_EXCL:    mode = CREATE_NEW;          break;
            case _O_CREAT | _O_TRUNC:   mode = CREATE_ALWAYS;       break;
            case 0:                     mode = OPEN_EXISTING;       break;
            case _O_CREAT:              mode = OPEN_ALWAYS;         break;
            case _O_TRUNC:              mode = TRUNCATE_EXISTING;   break;
            default:                    mode = 0;                   break;
        }
    } else {
        /* Dos Int21 func 6ch open flag */
        switch (oldMode & 0x00090000) {
            case 0x00080000: mode = CREATE_ALWAYS;  break;
            case 0x00090000: mode = OPEN_ALWAYS;    break;
            case 0x00010000: mode = OPEN_EXISTING;  break;
            default:         mode = 0;              break;
        }
    }
    return mode;
}

DWORD ConvertShareMode(DWORD oldMode)
{
    return FILE_SHARE_READ | FILE_SHARE_WRITE;
}


extern "C"
int xxx_U_ropen(WCHAR *name, int mode) {
    WCHAR  wzBuf[MAX_PATH];
    HANDLE hFile = INVALID_HANDLE_VALUE;

    if (FAILED (StringCchPrintfW (wzBuf, MAX_PATH, L"\\Release\\%s", name))) {
        SetLastError (ERROR_INVALID_PARAMETER);
    } else {
        hFile = xxx_CreateFileW(wzBuf, ConvertAccessMode(mode), ConvertShareMode(mode),
                             NULL, ConvertCreateMode(mode), 0, NULL );
    }
    return (int) hFile;
}

extern "C"
int xxx_U_rread(int fh, BYTE *buf, int len) {
    DWORD dwNumBytesRead;
    if (!xxx_ReadFile( (HANDLE)fh, buf, len, &dwNumBytesRead, NULL)) {
        dwNumBytesRead = 0;
    }
    return (int) dwNumBytesRead;
}

extern "C"
int xxx_U_rwrite(int fh, BYTE *buf, int len) {
    DWORD dwNumBytesWritten;
    if (!xxx_WriteFile( (HANDLE)fh, buf, len, &dwNumBytesWritten, NULL)) {
        dwNumBytesWritten = 0;
    }
    return (int)dwNumBytesWritten;
}

extern "C"
int xxx_U_rlseek(int fh, int pos, int type) {
    return xxx_SetFilePointer((HANDLE)fh, pos, NULL, type);
}

extern "C"
int xxx_U_rclose(int fh) {
    return CloseHandle ((HANDLE)fh);
}

// this is the only PSL API allowed to be called when we're in power handler
// for ease of debugging
extern "C"
VOID
xxx_NKvDbgPrintfW(
            LPCWSTR lpszFmt,
            va_list lpParms
    )
{
#ifdef KCOREDLL
    // kernel mode, call direct
    NKvDbgPrintfW (lpszFmt, lpParms);
#else
    // user-mode, format the string in-proc and call OutputDebutString
    WCHAR rgchBuf[384];

    // NOTE: NKwvsprintW is inside user-mode coredll.
    NKwvsprintfW (rgchBuf, lpszFmt, lpParms, sizeof(rgchBuf)/sizeof(WCHAR));

    OutputDebugStringW (rgchBuf);
#endif
}

void WINAPIV NKDbgPrintfW(LPCWSTR lpszFmt, ...)  {
    va_list arglist;
    va_start(arglist, lpszFmt);
    xxx_NKvDbgPrintfW(lpszFmt, arglist);
    va_end(arglist);
}

extern "C"
VOID
xxx_ProfileSyscall(
    LPDWORD d1
    )
{
}

extern "C"
BOOL
xxx_GetRomFileInfo(
    DWORD               type,
    LPWIN32_FIND_DATA   lpfd,
    DWORD               count
    )
{
    return GetRomFileInfo (type, lpfd, count);
}

extern "C"
DWORD
xxx_GetRomFileBytes(
    DWORD   type,
    DWORD   count,
    DWORD   pos,
    LPVOID  buffer,
    DWORD   nBytesToRead
    )
{
    return GetRomFileBytes (type, count, pos, buffer, nBytesToRead);
}

extern "C"
void
xxx_CacheRangeFlush(
    LPVOID  pAddr,
    DWORD   dwLength,
    DWORD   dwFlags
    )
{
    if (CACHE_SYNC_WRITEBACK & dwFlags) {
        dwFlags |= CACHE_SYNC_L2_WRITEBACK;
    }

    if (CACHE_SYNC_DISCARD & dwFlags) {
        dwFlags |= CACHE_SYNC_L2_DISCARD;
    }
    CacheRangeFlush (pAddr, dwLength, dwFlags | CSF_EXTERNAL);
}

extern "C"
void xxx_CacheSync(int flags)
{
    xxx_CacheRangeFlush (0, 0, (DWORD) (flags|CACHE_SYNC_WRITEBACK));
}

extern "C"
BOOL
xxx_AddTrackedItem(
    DWORD               dwType,
    HANDLE              handle,
    TRACKER_CALLBACK    cb,
    DWORD               dwProc,
    DWORD               dwSize,
    DWORD               dw1,
    DWORD               dw2
    )
{
    return FALSE;
}

extern "C"
BOOL
xxx_DeleteTrackedItem(
    DWORD   dwType,
    HANDLE  handle
    )
{
    return FALSE;
}

extern "C"
BOOL
xxx_PrintTrackedItem(
    DWORD   dwFlag,
    DWORD   dwType,
    DWORD   dwProcID,
    HANDLE  handle
    )
{
    return FALSE;
}

extern "C"
BOOL
xxx_GetKPhys(
    void*   ptr,
    ULONG   length
    )
{
    return GetKPhys (ptr, length);
}

extern "C"
BOOL
xxx_GiveKPhys(
    void*   ptr,
    ULONG   length
    )
{
    return GiveKPhys (ptr, length);
}

extern "C"
DWORD
xxx_RegisterTrackedItem(
    LPWSTR  szName
    )
{
    return 0;
}

extern "C"
VOID
xxx_FilterTrackedItem(
    DWORD   dwFlags,
    DWORD   dwType,
    DWORD   dwProcID
    )
{
}

extern "C"
BOOL
xxx_SetKernelAlarm(
    HANDLE          hEvent,
    LPSYSTEMTIME    lpst
    )
{
    return SetKernelAlarm (hEvent, lpst);
}

extern "C"
void
xxx_RefreshKernelAlarm(
    void
    )
{
    RefreshKernelAlarm ();
}

extern "C"
VOID
xxx_SetOOMEventEx(
    HANDLE hEvtLowMemoryState, 
    HANDLE hEvtGoodMemoryState, 
    DWORD cpLowMemEvent, 
    DWORD cpLow, 
    DWORD cpCritical, 
    DWORD cpLowBlockSize, 
    DWORD cpCriticalBlockSize
    )

{
    SetOOMEventEx(hEvtLowMemoryState, hEvtGoodMemoryState, cpLowMemEvent, cpLow, cpCritical, cpLowBlockSize, cpCriticalBlockSize);
}

extern "C"
VOID
xxx_SetOOMEvent(
    HANDLE  hEvt,
    DWORD   cpLow,
    DWORD   cpCritical,
    DWORD   cpLowBlockSize,
    DWORD   cpCriticalBlockSize
    )
{
    xxx_SetOOMEventEx (hEvt, NULL, cpLow, cpLow, cpCritical, cpLowBlockSize, cpCriticalBlockSize);
}

extern "C"
DWORD
xxx_StringCompress(
    LPBYTE  bufin,
    DWORD   lenin,
    LPBYTE  bufout,
    DWORD   lenout
    )
{
    return StringCompress (bufin, lenin, bufout, lenout);
}

extern "C"
DWORD
xxx_StringDecompress(
    LPBYTE  bufin,
    DWORD   lenin,
    LPBYTE  bufout,
    DWORD   lenout
    )
{
    return StringDecompress (bufin, lenin, bufout, lenout);
}

extern "C"
DWORD
xxx_BinaryCompress(
    LPBYTE  bufin,
    DWORD   lenin,
    LPBYTE  bufout,
    DWORD   lenout
    )
{
    return BinaryCompress (bufin, lenin, bufout, lenout);
}

extern "C"
DWORD
xxx_BinaryDecompress(
    LPBYTE  bufin,
    DWORD   lenin,
    LPBYTE  bufout,
    DWORD   lenout,
    DWORD   skip
    )
{
    return BinaryDecompress (bufin, lenin, bufout, lenout, skip);
}

extern "C"
DWORD
xxx_DecompressBinaryBlock(
    LPBYTE  bufin,
    DWORD   lenin,
    LPBYTE  bufout,
    DWORD   lenout
    )
{
    return DecompressBinaryBlock (bufin, lenin, bufout, lenout);
}

extern "C"
int
xxx_InputDebugCharW(
    VOID
    )
{
    return InputDebugCharW ();
}

extern "C"
BOOL
xxx_IsBadPtr(
    DWORD   flags,
    LPBYTE  ptr,
    UINT    length
    )
{
    return IsBadPtr (flags, ptr, length);
}

extern "C"
DWORD
xxx_GetFSHeapInfo(
    void
    )
{
    return GetFSHeapInfo ();
}

extern "C"
VOID xxx_PrepareThreadExit (
    DWORD dwExitCode
    )
{
    PrepareThreadExit (dwExitCode);
}

extern "C"
HANDLE
xxx_GetCallerProcess(
    void
    )
{
#ifdef KCOREDLL
    // too verbose if turning on this messages.. Might want to consider turning it on when we're closed the "port complete"
    // DEBUGMSG (1, (L"!! WARNING !! GetCallerProcess is ambiguous. Use GetDirectCallerProcessId if asking for direct caller,\r\n"));
    // DEBUGMSG (1, (L"!! WARNING !!     or Use GetCallerVMProcessId if trying to access actual caller's VM/Handle.\r\n"));
#endif

    return (HANDLE) GetCallerProcess ();
}

extern "C"
DWORD
xxx_GetCallerVMProcessId (
    void
    )
{
    return GetCallerVMProcessId ();
}

extern "C"
DWORD
xxx_GetDirectCallerProcessId (
    void
    )
{
    return GetDirectCallerProcessId ();
}

extern "C"
DWORD
xxx_GetIdleTime(
    void
    )
{
    return GetIdleTime ();
}

extern "C"
DWORD
xxx_SetLowestScheduledPriority(
    DWORD   maxprio
    )
{
    return SetLowestScheduledPriority (maxprio);
}

extern "C"
DWORD
xxx_CeGetCurrentTrust(
    void
    )
{
    RETAILMSG(1, (TEXT("\r\n\r\n*** CeGetCurrentTrust() called - NOT SUPPORTED IN CE7.\r\n\r\n")));
    ASSERT(0);
    return OEM_CERTIFY_TRUST;
}


extern "C"
DWORD
xxx_CeGetCallerTrust(
    void
    )
{
    RETAILMSG(1, (TEXT("\r\n\r\n*** CeGetCallerTrust() called - NOT SUPPORTED IN CE7.\r\n\r\n")));
    ASSERT(0);
    return OEM_CERTIFY_TRUST;
}

extern "C"
BOOL
xxx_SetTimeZoneBias(
    DWORD   dwBias,
    DWORD   dwDaylightBias
    )
{
    return SetTimeZoneBias (dwBias, dwDaylightBias);
}

extern "C"
void
xxx_SetCleanRebootFlag(
    void
    )
{
#ifdef KCOREDLL
    KernelLibIoControl ((HANDLE) KMOD_CORE, IOCTL_KLIB_FORCECLEANBOOT, NULL, 0, NULL, 0, NULL);
#endif

    DWORD RebootFlags = RebootFlagCleanVolume;
    CeFsIoControl(TEXT("\\"), FSCTL_GET_OR_SET_REBOOT_FLAGS, &RebootFlags, sizeof(DWORD), NULL, 0, NULL, NULL);    
}

extern "C"
void
xxx_PowerOffSystem(
    void
    )
{
#ifdef KCOREDLL
    PowerOffSystem ();
#endif
}

extern "C"
BOOL
xxx_SetDbgZone(
    DWORD       pid,
    LPVOID      lpvMod,
    LPVOID      baseptr,
    DWORD       zone,
    LPDBGPARAM  lpdbgTgt
    )
{
    return SetDbgZone (pid, lpvMod, baseptr, zone, lpdbgTgt);
}

extern "C"
VOID
xxx_TurnOnProfiling(
    HANDLE  hThread
    )
{
}

extern "C"
VOID
xxx_TurnOffProfiling(
    HANDLE  hThread
    )
{
}

extern "C"
BOOL
xxx_SetHandleOwnerWorkaround (
    HANDLE  *ph,
    HANDLE  hProc
    )
{
    BOOL fRet = TRUE;
    BC_PROC (hProc);
    if (hActiveProc != hProc) {
        RETAILMSG (1, (L"!!NOTE!! Work around SetHandleOwner: Handle %8.8lx will no long be accessible by the caller.\r\n", *ph));
        RETAILMSG (1, (L"         It can only be accessed by the destination process (hProc = %8.8lx)\r\n", hProc));
        fRet = xxx_DuplicateHandle (hActiveProc, *ph, hProc, ph, 0, FALSE, DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE);
    }
    return fRet;
}

extern "C"
HANDLE
xxx_LoadIntChainHandler(
    LPCWSTR lpszFileName,
    LPCWSTR lpszFunctionName,
    BYTE    bIRQ
    )
{
#ifdef KCOREDLL
    return LoadIntChainHandler (lpszFileName, lpszFunctionName, bIRQ);
#else
    HANDLE hIISR = NULL;
    BOOL fParamOK = TRUE;
    REF_LOAD_INT_CHAIN_HANDLER intChainHandler = { {0},{0},bIRQ};
    if (lpszFileName) {
        fParamOK = SUCCEEDED(StringCchCopy(intChainHandler.szIISRDll,_countof(intChainHandler.szIISRDll),lpszFileName));
    }
    if (lpszFunctionName && fParamOK) {
        fParamOK = SUCCEEDED(StringCchCopy(intChainHandler.szIISREntry,_countof(intChainHandler.szIISREntry),lpszFunctionName));
    }
    if (fParamOK) {
        fParamOK = REL_UDriverProcIoControl_Trap(IOCTL_REF_LOAD_INT_CHAIN_HANDLER,&intChainHandler,sizeof(intChainHandler),&hIISR,sizeof(hIISR),NULL);
        if (!fParamOK)
            hIISR = NULL;
    }
    else
        SetLastError(ERROR_INVALID_PARAMETER);
    return hIISR ;
#endif
}

extern "C"
BOOL
xxx_FreeIntChainHandler(
    HANDLE  hInstance
    )
{
#ifdef KCOREDLL
    return FreeIntChainHandler (hInstance);
#else
    return REL_UDriverProcIoControl_Trap(IOCTL_REF_FREE_INT_CHAIN_HANDLER,&hInstance,sizeof(hInstance),NULL,0,NULL);
#endif
}

extern "C"
BOOL
xxx_IntChainHandlerIoControl(
    HANDLE  hLib,
    DWORD   dwIoControlCode,
    LPVOID  lpInBuf,
    DWORD   nInBufSize,
    LPVOID  lpOutBuf,
    DWORD   nOutBufSize,
    LPDWORD lpBytesReturned
    )
{
#ifdef KCOREDLL
    return xxx_KernelLibIoControl(hLib, dwIoControlCode,lpInBuf,nInBufSize,lpOutBuf,nOutBufSize, lpBytesReturned) ;
#else
    REF_INT_CHAIN_HANDLER_IOCONTROL intChainHandlerIoControl =  { hLib, dwIoControlCode, lpInBuf, nInBufSize};
    return REL_UDriverProcIoControl_Trap(IOCTL_REF_INT_CHAIN_HANDLER_IOCONTROL, &intChainHandlerIoControl, sizeof(intChainHandlerIoControl),lpOutBuf,nOutBufSize, lpBytesReturned);
#endif
}

extern "C"
HANDLE
xxx_LoadKernelLibrary(
    LPCWSTR lpszFileName
    )
{
    return LoadKernelLibrary (lpszFileName);
}

extern "C"
BOOL
xxx_InterruptInitialize(
    DWORD   idInt,
    HANDLE  hEvent,
    LPVOID  pvData,
    DWORD   cbData
    )
{
#ifdef KCOREDLL
    return InterruptInitialize (idInt, hEvent, pvData, cbData);
#else
    REF_INTERRUPTINITIALIZE_PARAM refInterruptInitParm = {idInt,hEvent, pvData,cbData } ;
    return REL_UDriverProcIoControl_Trap(IOCTL_REF_INTERRUPT_INITIALIZE,&refInterruptInitParm,sizeof(REF_INTERRUPTINITIALIZE_PARAM),NULL,0,NULL);
#endif
}

extern "C"
void
xxx_InterruptDone(
    DWORD   idInt
    )
{
#ifdef KCOREDLL
    InterruptDone (idInt);
#else
    REL_UDriverProcIoControl_Trap(IOCTL_REF_INTERRUPT_DONE,&idInt,sizeof(idInt),NULL,0,NULL);
#endif
}

extern "C"
void
xxx_InterruptDisable(
    DWORD   idInt
    )
{
#ifdef KCOREDLL
    InterruptDisable (idInt);
#else
    REL_UDriverProcIoControl_Trap(IOCTL_REF_INTERRUPT_DISABLE,&idInt,sizeof(idInt),NULL,0,NULL);
#endif
}

extern "C"
void
xxx_InterruptMask(
    DWORD   idInt,
    BOOL    fDisable
    )
{
#ifdef KCOREDLL
    InterruptMask (idInt, fDisable);
#else
    REF_INTERRUPT_MASK_PARAM refInterruptMaskParm = { idInt,fDisable };
    REL_UDriverProcIoControl_Trap(IOCTL_REF_INTERRUPT_MASK,&refInterruptMaskParm,sizeof(refInterruptMaskParm),NULL,0,NULL);
#endif
}

extern "C"
BOOL
xxx_SetPowerOffHandler(
    FARPROC pfn
    )
{
#ifdef KCOREDLL
    return SetPowerHandler (pfn, PHNDLR_DEVMGR);
#else
    return FALSE;
#endif
}

extern "C"
BOOL
xxx_SetGwesPowerHandler(
    FARPROC pfn
    )
{
#ifdef KCOREDLL
    return SetPowerHandler (pfn, PHNDLR_GWES);
#else
    return FALSE;
#endif
}

extern "C"
BOOL
AttachDebugger (LPCWSTR dbgname)
{
    return AttachDebugger_Trap (dbgname);
}


extern "C"
BOOL
xxx_ConnectDebugger(
    LPVOID  pInit
    )
{
    return TRUE;
}

extern "C"
BOOL
xxx_ConnectHdstub(
    LPVOID pInit
    )
{
    return TRUE;
}

extern "C"
BOOL
xxx_ConnectOsAxsT0(
    LPVOID pInit
    )
{
    return TRUE;
}


extern "C"
BOOL
xxx_ConnectOsAxsT1(
    LPVOID pInit
    )
{
    return TRUE;
}

extern "C"
BOOL
xxx_SetHardwareWatch(
    LPVOID  vAddr,
    DWORD   flags
    )
{
    return SetHardwareWatch (vAddr, flags);
}

extern "C" UpdateAPISetInfo (void);

extern "C"
BOOL xxx_RegisterAPISet(HANDLE hASet, DWORD dwSetID) {
    BOOL fRet = RegisterAPISet(hASet, dwSetID);
#ifdef KCOREDLL
    UpdateAPISetInfo ();
#endif
    return fRet;
}

extern "C"
HANDLE
xxx_CreateAPIHandle(
    HANDLE  hASet,
    LPVOID  pvData
    )
{
    return CreateAPIHandle(hASet, pvData);
}

extern "C"
HANDLE
xxx_CreateAPIHandleWithAccess (HANDLE hASet, LPVOID pvData, DWORD dwAccessMask, HANDLE hTargetProcess)
{
    BC_PROC(hTargetProcess);
    return CreateAPIHandleWithAccess(hASet, pvData, dwAccessMask, hTargetProcess);
}

extern "C"
LPVOID xxx_VerifyAPIHandle(HANDLE hASet, HANDLE h) {

    return VerifyAPIHandle (hASet, h);
}

extern "C"
BOOL xxx_RegisterDirectMethods (HANDLE hApiSet, const PFNVOID *ppfnDirectMethod)
{
    return RegisterDirectMethods (hApiSet, ppfnDirectMethod);
}

extern "C"
BOOL xxx_CeRegisterAccessMask (HANDLE hApiSet, const LPDWORD lprgAccessMask, DWORD cAccessMask)
{
    //Note: cAccessMask == count of # of entries in lprgAccessMask whereas API takes the total size of the array argument
    return CeRegisterAccessMask (hApiSet, lprgAccessMask, cAccessMask * sizeof(DWORD));
}

extern "C"
HANDLE xxx_LockAPIHandle (HANDLE hApiSet, HANDLE hSrcProc, HANDLE h, LPVOID *ppvObj)
{
#ifdef KCOREDLL
    return LockAPIHandle (hApiSet, hSrcProc, h, ppvObj);
#else
    HANDLE hLock = NULL;
    if (!DuplicateHandle (hSrcProc, h, hActiveProc, &hLock, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
        SetLastError (ERROR_INVALID_HANDLE);

    } else if ((NULL == (*ppvObj = VerifyAPIHandle (hApiSet, hLock)))
                && GetLastError ()) {   // last error will be zero if it's a valid handle with pvObj == NULL
        // handle not of the API set to be verified
        CloseHandle (hLock);
        hLock = NULL;
        SetLastError (ERROR_INVALID_HANDLE);
    }
    return hLock;
#endif
}

extern "C"
BOOL xxx_UnlockAPIHandle (HANDLE hApiSet, HANDLE hLock)
{
#ifdef KCOREDLL
    return UnlockAPIHandle (hApiSet, hLock);
#else
    // last error will be zero if it's a valid handle with pvObj == NULL
    return ((VerifyAPIHandle (hApiSet, hLock) || !GetLastError()) && CloseHandle (hLock));
#endif
}


extern "C"
DWORD xxx_GetHandleServerId (HANDLE h)
{
    return GetHandleServerId (h);
}

extern "C"
void
xxx_PPSHRestart(
    void
    )
{
}

extern "C"
void
xxx_DebugNotify(
    DWORD   dwFlags,
    DWORD   data
    )
{
    return DebugNotify(dwFlags, data);
}

extern "C"
BOOL
xxx_WaitForDebugEvent(
    LPDEBUG_EVENT   lpDebugEvent,
    DWORD           dwMilliseconds
    )
{
    return WaitForDebugEvent(lpDebugEvent, dwMilliseconds);
}

extern "C"
BOOL
xxx_ContinueDebugEvent(
    DWORD   dwProcessId,
    DWORD   dwThreadId,
    DWORD   dwContinueStatus
    )
{
    return ContinueDebugEvent(dwProcessId, dwThreadId, dwContinueStatus);
}

extern "C"
BOOL xxx_DebugActiveProcess(DWORD dwProcId)
{
    return DebugActiveProcess (dwProcId);
}

extern "C"
BOOL xxx_FlushInstructionCache(HANDLE hProcess, LPCVOID lpBaseAddress, DWORD dwSize)
{
    BC_PROC(hProcess);
    return FlushInstructionCache(hProcess, lpBaseAddress, dwSize);
}

extern "C"
BOOL xxx_ReadProcessMemory(HANDLE hProcess, LPCVOID lpBaseAddress, LPVOID lpBuffer, DWORD nSize, LPDWORD lpNumberOfBytesRead) {
    BC_PROC(hProcess);
    BOOL fRet = INT_ReadMemory (hProcess, lpBaseAddress, lpBuffer, nSize);
    if (fRet && lpNumberOfBytesRead) {
        *lpNumberOfBytesRead = nSize;
    }
    return fRet;
}

extern "C"
BOOL xxx_WriteProcessMemory(HANDLE hProcess, LPVOID lpBaseAddress, LPVOID lpBuffer, DWORD nSize, LPDWORD lpNumberOfBytesWritten) {
    BC_PROC(hProcess);
    BOOL fRet = INT_WriteMemory (hProcess, lpBaseAddress, lpBuffer, nSize);

    if (fRet && lpNumberOfBytesWritten) {
        *lpNumberOfBytesWritten = nSize;
    }
    return fRet;
}

extern "C"
HANDLE
xxx_OpenProcess(
    DWORD   fdwAccess,
    BOOL    fInherit,
    DWORD   IDProcess
    )
{
    return OpenProcess(fdwAccess, fInherit, IDProcess);
}

extern "C"
HANDLE
xxx_OpenThread(
    DWORD   fdwAccess,
    BOOL    fInherit,
    DWORD   IDThread
    )
{
    return OpenThread(fdwAccess, fInherit, IDThread);
}

extern "C"
BOOL WINAPI xxx_GetModuleInformation (HANDLE hProcess, HMODULE hModule, LPMODULEINFO lpmodinfo, DWORD cb)
{
    BC_PROC (hProcess);
    return CeGetModuleInfo (hProcess, hModule, MINFO_MODULE_INFO, lpmodinfo, cb);
}


extern "C"
BOOL WINAPI xxx_CeSetProcessVersion (HANDLE hProc, DWORD dwVersion)
{
    BC_PROC(hProc);
    return CeSetProcessVersion (hProc, dwVersion);
}

extern "C"
THSNAP*
xxx_THCreateSnapshot(
    DWORD   dwFlags,
    DWORD   dwProcID
    )
{
    return THCreateSnapshot (dwFlags, dwProcID);;
}


extern "C"
void
xxx_NotifyForceCleanboot(
    void
    )
{
#ifdef KCOREDLL
    KernelLibIoControl ((HANDLE) KMOD_CORE, IOCTL_KLIB_NOTIFYCLEANBOOT, NULL, 0, NULL, 0, NULL);
#else
    RETAILMSG (1, (L"NotifyForceCleanboot not supported in user mode\r\n"));
#endif
}

extern "C"
BOOL WINAPI stub_CreateProcessW(LPCWSTR lpszImageName, LPCWSTR lpszCommandLine,
    LPSECURITY_ATTRIBUTES lpsaProcess, LPSECURITY_ATTRIBUTES lpsaThread,
    BOOL fInheritHandles, DWORD fdwCreate, LPVOID lpvEnvironment,
    LPWSTR lpszCurDir, LPSTARTUPINFO lpsiStartInfo,
    LPPROCESS_INFORMATION lppiProcInfo);

extern "C"
BOOL WINAPI xxx_CreateProcessW(LPCWSTR lpszImageName, LPCWSTR lpszCommandLine,
    LPSECURITY_ATTRIBUTES lpsaProcess, LPSECURITY_ATTRIBUTES lpsaThread,
    BOOL fInheritHandles, DWORD fdwCreate, LPVOID lpvEnvironment,
    LPWSTR lpszCurDir, LPSTARTUPINFO lpsiStartInfo,
    LPPROCESS_INFORMATION lppiProcInfo) {
    return stub_CreateProcessW(lpszImageName, lpszCommandLine, lpsaProcess, lpsaThread,
                               fInheritHandles, fdwCreate, lpvEnvironment, lpszCurDir, lpsiStartInfo, lppiProcInfo);
}


extern "C"
BOOL
WINAPI
int_CreateProcessW(
    LPCWSTR                 lpszImageName,
    LPCWSTR                 lpszCommandLine,
    LPSECURITY_ATTRIBUTES   lpsaProcess,
    LPSECURITY_ATTRIBUTES   lpsaThread,
    BOOL                    fInheritHandles,
    DWORD                   fdwCreate,
    LPVOID                  lpvEnvironment,
    LPWSTR                  lpszCurDir,
    LPSTARTUPINFO           lpsiStartInfo,
    LPPROCESS_INFORMATION   lppiProcInfo
    )
{
    BOOL fRet = FALSE;

    // validation for unused parameters (rest are done in kernel)
    if (fInheritHandles     // inherit handle not supported
        || lpvEnvironment   // environment not supported
        || lpszCurDir) {    // we have no concept of "current directory"
        SetLastError (ERROR_INVALID_PARAMETER);

    } else {
        CE_PROCESS_INFORMATION CeProcInfo = {0};
        LPCE_PROCESS_INFORMATION pCeProcInfo = NULL;
        if (lppiProcInfo || fdwCreate) {
            CeProcInfo.cbSize = sizeof(CeProcInfo);
            CeProcInfo.dwFlags = fdwCreate;
            pCeProcInfo = &CeProcInfo;
        }
        fRet = CreateProcessW(lpszImageName, lpszCommandLine, pCeProcInfo);
        if (fRet && lppiProcInfo) {
            DEBUGCHK (CeProcInfo.ProcInfo.hProcess && CeProcInfo.ProcInfo.hThread);
            memcpy (lppiProcInfo, &CeProcInfo.ProcInfo, sizeof(CeProcInfo.ProcInfo));
        }
    }

    return fRet;
}

extern "C"
BOOL
WINAPI
xxx_CeCreateProcessEx(
    LPCWSTR                    lpszImageName,
    LPCWSTR                    lpszCommandLine,
    LPCE_PROCESS_INFORMATION   pProcInfo
    )
{
    return CreateProcessW(lpszImageName, lpszCommandLine, pProcInfo);
}

extern "C"
HANDLE
WINAPI
xxx_CreateThread(
    LPSECURITY_ATTRIBUTES   lpsa,
    DWORD                   cbStack,
    LPTHREAD_START_ROUTINE  lpStartAddr,
    LPVOID                  lpvThreadParm,
    DWORD                   fdwCreate,
    LPDWORD                 lpIDThread
    )
{
    return CreateThread(lpsa,cbStack,lpStartAddr,lpvThreadParm,fdwCreate,lpIDThread);
}

extern "C"
BOOL WINAPI xxx_TerminateProcess(HANDLE hProc, DWORD dwRetVal)
{
#ifndef KCOREDLL
    if (GetCurrentProcess () != hProc) {
        DWORD dwProcId = (DWORD) hProc;
        if (dwProcId & 1) {
            dwProcId = GetProcessId (hProc);
        }
        if (GetCurrentProcessId () == dwProcId) {
            SetAbnormalTermination ();
        }
    } else if (GetCurrentThreadId () == g_dwMainThId) {
        // main thread calling ExitProcess, just exit the main thread and the process will exit
        ExitThread (dwRetVal);
    }
#endif
    BC_PROC(hProc);
    return TerminateProcess(hProc, dwRetVal);
}

extern "C"
BOOL
xxx_SetStdioPathW(
    DWORD   id,
    LPCWSTR pwszPath
    )
{
    return SetStdioPathW (id, pwszPath);
}

extern "C"
BOOL
xxx_GetStdioPathW(
    DWORD   id,
    PWSTR   pwszBuf,
    LPDWORD lpdwLen
    )
{
    return GetStdioPathW (id, pwszBuf, lpdwLen);
}

extern "C"
DWORD
xxx_ReadRegistryFromOEM(
    DWORD   dwFlags,
    LPBYTE  pBuf,
    DWORD   len
    )
{
    return ReadRegistryFromOEM (dwFlags, pBuf, len);
}

extern "C"
BOOL
xxx_WriteRegistryToOEM(
    DWORD   dwFlags,
    LPBYTE  pBuf,
    DWORD   len
    )
{
    return WriteRegistryToOEM (dwFlags, pBuf, len);
}

extern "C"
__int64
xxx_CeGetRandomSeed(
    void
    )
{
    return CeGetRandomSeed ();
}

extern "C"
void
xxx_UpdateNLSInfoEx(
    DWORD   ocp,
    DWORD   acp,
    DWORD   sysloc,
    DWORD   userloc
    )
{
    UpdateNLSInfoEx (ocp, acp, sysloc, userloc);
}

extern "C"
void xxx_UpdateNLSInfo(DWORD ocp, DWORD acp, DWORD loc)
{
    UpdateNLSInfoEx (ocp, acp, loc, loc);
}

extern "C"
BOOL
xxx_PageOutModule(
    HANDLE  hModule,
    DWORD   dwFlags
    )
{
    HANDLE hProc = hActiveProc;

    if ((DWORD) hModule & 0x3) {
        // hModule is a process handle
        hProc = hModule;
        hModule = NULL;
    }

    BC_PROC (hProc);
    return PageOutModule (hProc, hModule, dwFlags);
}

extern "C"
BOOL xxx_CeGetOwnerAccount (HANDLE hTok, PACCTID pAcctId, DWORD cbAcctId)
{
    RETAILMSG(1, (TEXT("\r\n\r\n*** CeGetOwnerAccount() called - NOT SUPPORTED IN CE7.\r\n\r\n")));
    ASSERT(0);
    return FALSE;
}

extern "C"
BOOL xxx_CeGetGroupAccount (HANDLE hTok, DWORD idx, PACCTID pAcctId, DWORD cbAcctId, LPDWORD pCountGroups)
{
    RETAILMSG(1, (TEXT("\r\n\r\n*** CeGetGroupAccount() called - NOT SUPPORTED IN CE7.\r\n\r\n")));
    ASSERT(0);
    return FALSE;
}

extern "C"
BOOL xxx_CeImpersonateToken (HANDLE hToken)
{
    RETAILMSG(1, (TEXT("\r\n\r\n*** CeImpersonateToken() called - NOT SUPPORTED IN CE7.\r\n\r\n")));
    ASSERT(0);
    return FALSE;
}

extern "C"
BOOL xxx_CeDuplicateToken (HANDLE hToken, LPHANDLE lphDuplicatedToken)
{
    RETAILMSG(1, (TEXT("\r\n\r\n*** CeDuplicateToken() called - NOT SUPPORTED IN CE7.\r\n\r\n")));
    ASSERT(0);
    return FALSE;
}

extern "C"
HANDLE xxx_CeCreateToken (LPVOID psi, DWORD dwFlags)
{
    RETAILMSG(1, (TEXT("\r\n\r\n*** CeCreateToken() called - NOT SUPPORTED IN CE7.\r\n\r\n")));
    ASSERT(0);
    return NULL;
}

extern "C"
BOOL xxx_CeAccessCheck (LPVOID pSecDesc, HANDLE hSubjectToken, DWORD dwDesiredAccess)
{
    RETAILMSG(1, (TEXT("\r\n\r\n*** CeAccessCheck() called - NOT SUPPORTED IN CE7.\r\n\r\n")));
    ASSERT(0);
    return FALSE;
}

extern "C"
BOOL xxx_CeGetAccessMask (LPVOID pSecDesc, HANDLE hSubjectToken, LPDWORD pdwMaxAccessMask)
{
    RETAILMSG(1, (TEXT("\r\n\r\n*** CeGetAccessMask() called - NOT SUPPORTED IN CE7.\r\n\r\n")));
    ASSERT(0);
    return FALSE;
}

extern "C"
BOOL xxx_CePrivilegeCheck (HANDLE hSubjectToken, LPDWORD pPrivs, int nPrivs)
{
    RETAILMSG(1, (TEXT("\r\n\r\n*** CePrivilegeCheck() called - NOT SUPPORTED IN CE7.\r\n\r\n")));
    ASSERT(0);
    return FALSE;
}

extern "C"
BOOL xxx_CeSerializeTokenData (HANDLE hToken, LPVOID lpTokenData, LPDWORD lpcbTokenData)
{
    RETAILMSG(1, (TEXT("\r\n\r\n*** CeSerializeTokenData() called - NOT SUPPORTED IN CE7.\r\n\r\n")));
    ASSERT(0);
    return FALSE;
}

extern "C"
BOOL xxx_CeDeserializeTokenData (LPVOID lpTokenData, DWORD cbTokenData, LPHANDLE lphToken)
{
    RETAILMSG(1, (TEXT("\r\n\r\n*** CeDeserializeTokenData() called - NOT SUPPORTED IN CE7.\r\n\r\n")));
    ASSERT(0);
    return FALSE;
}

extern "C"
BOOL xxx_CeImpersonateCurrentProcess (void)
{
    static BOOL fFlag = FALSE;
    if (!fFlag)
    {
        /* only flag once per proc */
        RETAILMSG(1, (TEXT("\r\n\r\n*** CeImpersonateCurrentProcess() called - NOT SUPPORTED IN CE7.\r\n\r\n")));
        fFlag = TRUE;
    }
//    ASSERT(0);  TODO: Assert commented out since third party binaries still use these functions.  They must be recompiled.
    return TRUE;
}

extern "C"
BOOL   xxx_CeRevertToSelf (void)
{
    static BOOL fFlag = FALSE;
    if (!fFlag)
    {
        /* only flag once per proc */
        RETAILMSG(1, (TEXT("\r\n\r\n*** CeRevertToSelf() called - NOT SUPPORTED IN CE7.\r\n\r\n")));
        fFlag = TRUE;
    }
//    ASSERT(0);  TODO: Assert commented out since third party binaries still use these functions.  They must be recompiled.
    return TRUE;
}


// new Debug APIs
extern "C"
BOOL WINAPI xxx_DebugActiveProcessStop (DWORD dwProcId)
{
    return DebugActiveProcessStop(dwProcId);
}

extern "C"
BOOL WINAPI xxx_DebugSetProcessKillOnExit (BOOL fKillOnExit)
{
    return DebugSetProcessKillOnExit (fKillOnExit);
}

extern "C"
DWORD
xxx_CeGetProcessTrust(
    HANDLE hProc
    )
{
    RETAILMSG (1, (L"xxx_CeGetProcessTrust not implemented\r\n"));
    return OEM_CERTIFY_TRUST;
}

extern "C"
DWORD
xxx_CeGetModuleInfo (HANDLE hProc, LPVOID lpBaseAddr, DWORD infoType, LPVOID pBuf, DWORD cbBufSize)
{
    BC_PROC (hProc);
    return CeGetModuleInfo (hProc, lpBaseAddr, infoType, pBuf, cbBufSize);
}

extern "C"
BOOL xxx_CheckRemoteDebuggerPresent(HANDLE hProcess, PBOOL pbDebuggerPresent)
{
    BC_PROC (hProcess);
    return CheckRemoteDebuggerPresent (hProcess, pbDebuggerPresent);
}


extern "C"
HANDLE xxx_CreateMsgQueue(LPCWSTR lpName, LPMSGQUEUEOPTIONS lpOptions) {
    return NKCreateMsgQueue (lpName, lpOptions);
}

extern "C"
HANDLE xxx_OpenMsgQueue(HANDLE hSrcProc, HANDLE hMsgQ, LPMSGQUEUEOPTIONS lpOptions) {
    BC_PROC(hSrcProc);
    return NKOpenMsgQueueEx (hSrcProc, hMsgQ, hActiveProc, lpOptions);
}

extern "C"
BOOL xxx_ReadMsgQueue(HANDLE hMsgQ, LPVOID lpBuffer, DWORD cbBufferSize, LPDWORD lpNumberOfBytesRead, DWORD dwTimeout, DWORD *pdwFlags) {
    return NKReadMsgQueueEx (hMsgQ, lpBuffer, cbBufferSize, lpNumberOfBytesRead, dwTimeout, pdwFlags, NULL);
}

extern "C"
BOOL xxx_ReadMsgQueueEx(HANDLE hMsgQ, LPVOID lpBuffer, DWORD cbBufferSize, LPDWORD lpNumberOfBytesRead, DWORD dwTimeout, DWORD *pdwFlags, PHANDLE phTok) {
    return NKReadMsgQueueEx (hMsgQ, lpBuffer, cbBufferSize, lpNumberOfBytesRead, dwTimeout, pdwFlags, phTok);
}

extern "C"
BOOL xxx_WriteMsgQueue(HANDLE hMsgQ, LPVOID lpBuffer, DWORD cbDataSize, DWORD dwTimeout, DWORD dwFlags) {
    return NKWriteMsgQueue (hMsgQ, lpBuffer, cbDataSize, dwTimeout, dwFlags);
}

extern "C"
BOOL xxx_GetMsgQueueInfo(HANDLE hMsgQ, LPMSGQUEUEINFO lpInfo) {
    return NKGetMsgQueueInfo (hMsgQ, lpInfo);
}

extern "C"
BOOL xxx_CloseMsgQueue(HANDLE hMsgQ) {
    return CloseHandleInProc (hActiveProc, hMsgQ);
}

// remote Heap APIs
extern "C"
HLOCAL WINAPI xxx_RemoteLocalAlloc (UINT uFlags, UINT uBytes)
{
#ifdef KCOREDLL
    return RemoteLocalAlloc (uFlags, uBytes);
#else
    return NULL;
#endif
}

extern "C"
HLOCAL WINAPI xxx_RemoteLocalReAlloc (HLOCAL hMem, UINT uBytes, UINT uFlags)
{
#ifdef KCOREDLL
    return RemoteLocalReAlloc (hMem, uBytes, uFlags);
#else
    return NULL;
#endif
}

extern "C"
UINT WINAPI xxx_RemoteLocalSize( HLOCAL hMem )
{
#ifdef KCOREDLL
    return RemoteLocalSize (hMem);
#else
    return 0;
#endif
}

extern "C"
HLOCAL WINAPI xxx_RemoteLocalFree( HLOCAL hMem )
{
#ifdef KCOREDLL
    return RemoteLocalFree (hMem);
#else
    return hMem;
#endif
}


extern "C"
void xxx_PSLNotify (DWORD flags, DWORD proc, DWORD thread)
{
#ifdef KCOREDLL
    PSLNotify (flags, proc, thread);
#endif
}

extern "C"
VOID WINAPI xxx_SystemMemoryLow(void)
{
    SystemMemoryLow ();
}


extern "C"
DWORD xxx_WaitForAPIReady (DWORD dwAPISet, DWORD dwTimeout)
{
    DWORD dwRet = WaitForAPIReady (dwAPISet, dwTimeout);
#ifdef KCOREDLL
    if (WAIT_OBJECT_0 == dwRet) {
        UpdateAPISetInfo ();
    }
#endif
    return dwRet;
}


extern "C"
HANDLE xxx_CeOpenFileHandle (HMODULE hMod)
{
    HANDLE hf = INVALID_HANDLE_VALUE;
    CeGetModuleInfo (GetCurrentProcess (), hMod, MINFO_FILE_HANDLE, &hf, sizeof (hf));
    return hf;
}


extern "C"
BOOL xxx_CeCallUserProc
    (
        LPCWSTR pszDllName,
        LPCWSTR pszFuncName,
        LPVOID lpInBuffer,
        DWORD nInBufferSize,
        LPVOID lpOutBuffer,
        DWORD nOutBufferSize,
        LPDWORD lpBytesReturned
    )
{
    DWORD dwErr = 0;

#ifdef KCOREDLL

    // In kernel mode; call into the ui proxy driver
    HANDLE hDriver = INVALID_HANDLE_VALUE;

    // validate
    if (!pszDllName || !pszFuncName || !lpInBuffer || !lpBytesReturned || !lpOutBuffer) {
        dwErr = ERROR_INVALID_PARAMETER;
    } else if (INVALID_HANDLE_VALUE == (hDriver = CreateFile(UIPROXY_DRIVER_PREFIX, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL))) {
        // try to activate the device if device hasn't started
        ActivateDeviceEx_Trap(UIPROXY_DRIVER_KEY, NULL, 0, NULL);
        if (INVALID_HANDLE_VALUE == (hDriver = CreateFile(UIPROXY_DRIVER_PREFIX, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL))) {
            dwErr = GetLastError();
        }
    }

    if (INVALID_HANDLE_VALUE != hDriver) {
        BYTE* pBase = NULL;

        // compute total size to allocate
        DWORD ccbDllName = (wcslen(pszDllName) + 1) * sizeof(WCHAR);
        DWORD ccbFuncName = (wcslen(pszFuncName) + 1) * sizeof(WCHAR);

        // 8-byte aligned
        DWORD ccbInDataStart = MAKEQUADALIGN(sizeof(DRIVERUIINFO) + ccbDllName + ccbFuncName);
        DWORD ccbOutDataStart = MAKEQUADALIGN(ccbInDataStart + nInBufferSize);
        DWORD ccbSizeToAlloc = ccbOutDataStart + nOutBufferSize;

        // allocate virtual memory in kernel space to hold dll name, function name, and driver data
        // device manager will marshal this to the user mode driver process when the ioctl call is made below
        pBase = (BYTE*)VirtualAllocEx(hActiveProc, NULL, ccbSizeToAlloc, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE, NULL);
        if (!pBase) {
            dwErr = ERROR_OUTOFMEMORY;
        } else {
            *lpBytesReturned = 0;

            // copy the data size
            PDRIVERUIINFO pInfo = (PDRIVERUIINFO)pBase;
            pInfo->ccbDllName = ccbDllName;
            pInfo->ccbFuncName = ccbFuncName;
            pInfo->ccbDriverData = nInBufferSize;

            // copy the dll and function names
            pBase = (LPBYTE)(pInfo + 1);
            memcpy(pBase, pszDllName, ccbDllName);
            pBase += ccbDllName;
            memcpy(pBase, pszFuncName, ccbFuncName);

            // copy in/out data (8-byte aligned)
            pBase = (LPBYTE)pInfo + ccbInDataStart;
            memcpy(pBase, lpInBuffer, nInBufferSize);
            pBase = (LPBYTE)pInfo + ccbOutDataStart;
            memcpy(pBase, lpOutBuffer, nOutBufferSize);

            // send the message to ui proxy component
            if (!DeviceIoControl(hDriver, UIPROXY_IOCTL_DISPLAY, pInfo, ccbSizeToAlloc, pBase, nOutBufferSize, lpBytesReturned, NULL) ||
                !(*lpBytesReturned) || (*lpBytesReturned > nOutBufferSize)) {
                    dwErr = GetLastError();
            } else {
                // call successful; copy the driver data back to out parmeter
                memcpy(lpOutBuffer, pBase, *lpBytesReturned);
            }
            VirtualFreeEx(hActiveProc, pInfo, 0, MEM_RELEASE, 0);
        }
        CloseHandleInProc(hActiveProc, hDriver);
    }

#else // kcoredll

    // In user mode; load the dll in-proc
    HMODULE hMod = NULL;
    PFN_UIENTRYPOINT pfnProc = NULL;

    // valid arguments?
    if (!pszDllName || !pszFuncName || !lpInBuffer || !lpBytesReturned || !lpOutBuffer) {
        dwErr = ERROR_INVALID_PARAMETER;
    } else if (NULL == (hMod = LoadLibraryW(pszDllName))) { // valid dll load?
        dwErr = GetLastError();
    } else if (NULL == (pfnProc = (PFN_UIENTRYPOINT)GetProcAddress(hMod, pszFuncName))) { // valid entry point into the dll?
        dwErr = GetLastError();
        FreeLibrary(hMod);
    } else {
        // call the function
        *lpBytesReturned = 0;
        if (!pfnProc((LPBYTE)lpInBuffer, nInBufferSize, lpOutBuffer, nOutBufferSize, lpBytesReturned) || // function call was successful?
            (*lpBytesReturned > nOutBufferSize)) { // valid returned size
                dwErr = (GetLastError()) ? GetLastError() : ERROR_INVALID_PARAMETER;
        }
        FreeLibrary(hMod);
    }

#endif // kcoredll

    SetLastError(dwErr);
    return !dwErr;

}

extern "C"
BOOL xxx_SetAPIErrorHandler(HANDLE hApiSet, PFNAPIERRHANDLER pfnHandler)
{
    return SetAPIErrorHandler(hApiSet, pfnHandler);
}

extern "C"
BOOL CePowerOffProcessor (DWORD dwProcessor, DWORD dwHint)
{
    return NKCPUPowerFunc (dwProcessor, FALSE, dwHint);
}

extern "C"
BOOL CePowerOnProcessor (DWORD dwProcessor)
{
    return NKCPUPowerFunc (dwProcessor, TRUE, 0);
}

extern "C"
void CeResetStack (void)
{
    NKResetStack ();
}

/*
    @doc BOTH EXTERNAL
    @func DWORD | CeGetIdleTimeEx | Returns the idle time of a processor.
    @parm DWORD | dwProcessor | which processor
*/
extern "C"
BOOL WINAPI CeGetIdleTimeEx (DWORD dwProcessor, LPDWORD pdwIdleTime)
{
    BOOL fRet = ((DWORD) (dwProcessor - 1) < CeGetTotalProcessors ());
    if (fRet ) {
        *pdwIdleTime = NKGetCPUInfo (dwProcessor, CPUINFO_IDLE_TIME);
    } else {
        SetLastError (ERROR_INVALID_PARAMETER);
    }
    return fRet;
}

/*
    @doc BOTH EXTERNAL
    @func DWORD | CeGetProcessorState | Returns the state of a processor.
    @parm DWORD | dwProcessor | which processor
*/
extern "C"
DWORD WINAPI CeGetProcessorState (DWORD dwProcessor)
{
    return NKGetCPUInfo (dwProcessor, CPUINFO_POWER_STATE);
}



extern "C"
BOOL CeGetThreadAffinity (HANDLE hThread, LPDWORD pdwAffinity)
{
    BC_THRD(hThread);
    return NKGetThreadAffinity (hThread, pdwAffinity);

}

extern "C"
BOOL CeSetThreadAffinity (HANDLE hThread, DWORD dwAffinity)
{
    BC_THRD(hThread);
    return NKSetThreadAffinity (hThread, dwAffinity);
}


extern "C"
BOOL CeGetProcessAffinity (HANDLE hProcess, LPDWORD pdwAffinity)
{
    BC_PROC (hProcess);
    return NKGetProcessAffinity (hProcess, pdwAffinity);

}

extern "C"
BOOL CeSetProcessAffinity (HANDLE hProcess, DWORD dwAffinity)
{
    BC_PROC(hProcess);
    return NKSetProcessAffinity (hProcess, dwAffinity);
}

