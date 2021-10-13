//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

 
#include "windows.h"
#include "nkintr.h"
#include "bldver.h"
#include "kernel.h"

#undef bIntrIndexHigh
#undef bIntrNumber

DWORD IsExiting;
DWORD fIsDying;

#include "cscode.c"

extern HANDLE hInstCoreDll;
BOOL Imm_DllEntry(HANDLE hinstDll, DWORD dwReason, LPVOID lpvReserved);
BOOL WINAPI xxx_CloseHandle(HANDLE hObject);

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

// To be called by init.exe when it's done.
VOID WINAPI SystemStarted(void) {
	ERRORMSG(CeGetCurrentTrust() != OEM_CERTIFY_TRUST,(L"SystemStarted failed due to insufficient trust\r\n"));
	if (CeGetCurrentTrust() == OEM_CERTIFY_TRUST)
		PSLNotify(DLL_SYSTEM_STARTED,0,0);
}

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
    DWORD i = 0;
    DWORD dwReason = DLL_THREAD_DETACH;
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
        ProcessDetachAllDLLs();
        PSLNotify(DLL_PROCESS_DETACH,GetCurrentProcessId(),GetCurrentThreadId());
        DebugNotify(DLL_PROCESS_DETACH,dwExitCode);
        CloseProcOE(1);
        CloseAllHandles();
    } else {
        if (!IsExiting) {
            ThreadDetachAllDLLs();
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
typedef DWORD (*mainthreadfunctype)(ulong,ulong,ulong,ulong);
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

    if ((GetCurrentThreadId () == (DWORD) hMainThread) && !(UTlsPtr()[TLSSLOT_KERNEL] & TLSKERN_TRYINGTODIE)) {
        // main thread faulted, show error bux
        ErrInfo errInfo = {dwExcpCode, dwExcpAddr};
        HANDLE hThrd = CreateThread (NULL, 0, ShowErrThrd, &errInfo, 0, NULL);

        if (!hThrd) {
            // if we can't create a thread, don't bother showing the error box since it's likely the system is
            // in a really stressed condition.
            RETAILMSG (1, (L"Main thread in proc %8.8lx faulted, Excpetion code = %8.8lx, Exception Address = %8.8x!\r\n",
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



void
MainThreadBaseFunc(
	LPVOID	pfn,
	ulong	param1,
	ulong	param2,
	ulong	param3,
	ulong	param4
	)
{
	DWORD retval = 0;

	Imm_DllEntry(hInstCoreDll, DLL_PROCESS_ATTACH, 0);

	RegisterDlgClass();

	DebugNotify(DLL_PROCESS_ATTACH,(DWORD)pfn);

	hMainThread = (HANDLE)GetCurrentThreadId();

	PSLNotify(DLL_PROCESS_ATTACH,GetCurrentProcessId(),GetCurrentThreadId());

	retval = ((mainthreadfunctype)pfn) (param1, param2, param3, param4);

	/* ExitThread stops execution of the current thread */
	ExitThread(retval);

}

void ThreadBaseFunc(LPVOID pfn, ulong param)
{
    DWORD retval = 0;
    if (GetCurrentProcessIndex()) {
        Imm_DllEntry (hInstCoreDll, DLL_THREAD_ATTACH, 0);
    }
    DebugNotify(DLL_THREAD_ATTACH,(DWORD)pfn);
    PSLNotify(DLL_THREAD_ATTACH,GetCurrentProcessId(),GetCurrentThreadId());
    ThreadAttachAllDLLs();
    retval = ((threadfunctype)pfn)(param);
    ExitThread(retval);
    /* ExitThread stops execution of the current thread */
}

#if defined(ARM) || defined (x86)
// QFE for .NETCF on ARM class processors - don't access kernel structure directly so .NETCF apps can run untrusted
void ComThreadBaseFunc(LPVOID pfn, ulong param1, ulong param2, ulong param3, ulong dwExeBase, ulong dwRva14, ulong dwSize14)
#else
void ComThreadBaseFunc(LPVOID pfn, ulong param1, ulong param2, ulong param3, ulong param4)
#endif
{
    DWORD retval = 0;
#if !defined(ARM) && !defined (x86)
    DWORD dwExeBase, dwRva14, dwSize14;
    BOOL oldMode;
    PPROCESS pProc;
#endif
    Imm_DllEntry (hInstCoreDll, DLL_PROCESS_ATTACH, 0);
    RegisterDlgClass ();
    hMainThread = (HANDLE)GetCurrentThreadId();
    PSLNotify(DLL_PROCESS_ATTACH,GetCurrentProcessId(),GetCurrentThreadId());
    DebugNotify(DLL_PROCESS_ATTACH,(DWORD)pfn);
#if !defined(ARM) && !defined (x86)
    oldMode = SetKMode(TRUE);
    pProc = (PPROCESS)param4;
    dwExeBase = (DWORD)pProc->BasePtr;
    dwRva14 = pProc->e32.e32_sect14rva;
    dwSize14 = pProc->e32.e32_sect14size;
    SetKMode(oldMode);
#endif
    retval = ((comthreadfunctype)pfn)(param1,param2,param3,SW_SHOW,dwExeBase,dwRva14,dwSize14);
    ExitThread(retval);
    /* ExitThread stops execution of the current thread */
}

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

BOOL SetInterruptEvent(DWORD idInt) {
    long mask;
    long pend;
    long *ptrPend;

    if ((idInt < SYSINTR_DEVICES) || (idInt >= SYSINTR_MAXIMUM))
        return FALSE;
    idInt -= SYSINTR_DEVICES;
    mask = 1 << idInt;
    ptrPend = (long*)(UserKInfo[KINX_KDATA_ADDR]+KINFO_OFFSET) + KINX_PENDEVENTS;
    do {
        pend = *ptrPend;
        if (pend & mask)
            return TRUE;    // The bit is already set, so all done.
    } while (InterlockedTestExchange(ptrPend, pend, pend|mask) != pend);
    return TRUE;
}

//
// THIS FUNCTION SHOULD ONLY BE USED IN POWER-ON HANDLER.
//
// In order to allow setting an non-interrupt event during power-on, we need to:
// (1) NOT making any API calls (trapping here kernel will kill the system)
// (2) Chain the events to be set together and leave
// (3) Upon return from all the power handler, kernel will set the events one by one
//
// NOTE: There is not much checking here since we'll fault if we're not in KMode. And the caller 
//       can do whatever they want in KMode anyway.
//

#define HANDLEPTR(h)    ((PHDATA) (((DWORD) (h) & HANDLE_ADDRESS_MASK) + 0x80000000))
#define NEXTEVT(h)      (HANDLEPTR(h)->dwInfo)

BOOL CeSetPowerOnEvent (HANDLE hEvt)
{
    HANDLE *ptrEvts = (HANDLE*)(UserKInfo[KINX_KDATA_ADDR]+KINFO_OFFSET) + KINX_PWR_EVTS;
    HANDLE hEvtCurr;

    if (!(hEvtCurr = *ptrEvts)) {
        // first in the list
        *ptrEvts = hEvt;
        NEXTEVT(hEvt) = 0;
    } else {
        HANDLE hEvtPrev = NULL;
        // need to prevent circular list
        do {
            if (hEvtCurr == hEvt) {
                // already in the list, do nothing
                return FALSE;
            }
            hEvtPrev = hEvtCurr;
            hEvtCurr = (HANDLE) NEXTEVT (hEvtCurr);
        } while (hEvtCurr);
        // DEBUGCHK (hEvtPrev);
        NEXTEVT(hEvt) = 0;
        NEXTEVT(hEvtPrev) = (DWORD) hEvt;
    }

    return TRUE;
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

BOOL GetFileAttributesExW(LPCWSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation) {
	LPCWSTR pTrav;
	HANDLE hFind;
	WIN32_FIND_DATA w32fd;
	for (pTrav = lpFileName; *pTrav; pTrav++)
		if (*pTrav == '*' || *pTrav == '?') {
			SetLastError(ERROR_INVALID_NAME);
			return FALSE;
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
