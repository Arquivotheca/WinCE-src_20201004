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

Module:

    crtsupp.cpp

Description:

    Provide COREDLL-specific implementations of CRT support routines

--*/
#include <corecrt.h>
#include <corecrtstorage.h>
#include <errorrep.h>
#include <stdlib.h>
#include <coredll.h>
#include <string.h>
#include <console.h>

#define MSVCRT_DLL   L"msvcrt.dll"

extern "C"
{
// CRT functions from msvcrt.dll
typedef int (*PFNCRT_SWSCANF_S) (const wchar_t *, const wchar_t *, ...);
typedef int (*PFNCRT_SWPRINTF_S)(wchar_t *, size_t, const wchar_t *, ...);
typedef int (*PFNCRT_SWPRINTF)(wchar_t *, const wchar_t *, ...);
typedef int (*PFNCRT_VSNPRINTF)(char *, size_t ,const char *, va_list ap);
typedef int (*PFNCRT_VSNWPRINTF)(wchar_t *, size_t ,const wchar_t *, va_list ap);
typedef wchar_t *(*PFNCRT_WSETLOCALE)(int, const wchar_t *);
typedef void (*PFNCRT_SETMBCP)(int);

HINSTANCE m_hCRT;
PFNCRT_SWPRINTF_S mpfn_CRTswprintf_s;
PFNCRT_SWSCANF_S mpfn_CRTswscanf_s;
PFNCRT_SWPRINTF mpfn_CRT_swprintf;
PFNCRT_VSNPRINTF mpfn_CRT_vsnprintf;
PFNCRT_VSNWPRINTF mpfn_CRT_vsnwprintf;
PFNCRT_WSETLOCALE mpfn_CRT_wsetlocale;
PFNCRT_SETMBCP mpfn_CRT_setmbcp;

CRITICAL_SECTION csLowioInitLock;
BOOL LowioInited = FALSE;

void LowioTerm();

int _mtinitlocks ();
void _mtdeletelocks();
BOOL _heap_init();
int _cinit(BOOL);
void _cexit();
void _heap_term();

HMODULE LoadCRT (void)
{
    HMODULE hCRT = LoadLibrary (MSVCRT_DLL);

    if ( !hCRT ) {
        RETAILMSG (1, (L"LoadCRT: CRT LoadLibrary failed(pid = %8.8lx)\r\n", GetCurrentProcessId ()));
    } else {
    
        mpfn_CRTswprintf_s = (PFNCRT_SWPRINTF_S)GetProcAddress(hCRT, TEXT("swprintf_s"));
        mpfn_CRTswscanf_s = (PFNCRT_SWSCANF_S)GetProcAddress(hCRT, TEXT("swscanf_s"));
        mpfn_CRT_swprintf = (PFNCRT_SWPRINTF)GetProcAddress(hCRT, TEXT("_swprintf"));
        mpfn_CRT_vsnprintf = (PFNCRT_VSNPRINTF)GetProcAddress(hCRT, TEXT("_vsnprintf"));
        mpfn_CRT_vsnwprintf = (PFNCRT_VSNWPRINTF)GetProcAddress(hCRT, TEXT("_vsnwprintf"));
        mpfn_CRT_wsetlocale = (PFNCRT_WSETLOCALE)GetProcAddress(hCRT, TEXT("_wsetlocale"));
        mpfn_CRT_setmbcp = (PFNCRT_SETMBCP)GetProcAddress(hCRT, TEXT("_setmbcp"));

        if (   !mpfn_CRTswprintf_s
            || !mpfn_CRT_swprintf
            || !mpfn_CRTswscanf_s
            || !mpfn_CRT_setmbcp
            || !mpfn_CRT_vsnprintf
            || !mpfn_CRT_vsnwprintf
            || !mpfn_CRT_wsetlocale) {
            RETAILMSG (1, (L"LoadCRT: CRT GetProcAddress Failed(pid = %8.8lx)\r\n", GetCurrentProcessId ()));
        } else {
            m_hCRT = hCRT;
        }
    }
    
    return m_hCRT;
}

int CrtEntry(DWORD dwReason)
{
    if (DLL_PROCESS_ATTACH == dwReason)
    {
#ifndef KCOREDLL
        // Set CRT locale to system locale. For Kernel we do this after
        // filesystem is initialised in ReInitLocale().
        mpfn_CRT_wsetlocale(0, L"");
#endif
        
        InitializeCriticalSection(&csLowioInitLock);
    }
    else
    {
        ASSERT (DLL_PROCESS_DETACH == dwReason);
        LowioTerm();
        DeleteCriticalSection(&csLowioInitLock);
        
    }
    return 0;
}


__declspec(noreturn)
void
__cdecl
__crt_fatal(
    CONTEXT * pContextRecord,
    DWORD ExceptionCode
    )
/*++

Description:

    Create a fake exception record using the given context and report a fatal
    error via Windows Error Reporting.  Then either end the current process
    or, if in kernel mode, reboot the device.

Arguments:

    pContextRecord - Supplies a pointer to the context reflecting CPU state at
        the time of the error.

    ExceptionCode - Supplies the exception code reported and the argument to
        ExitProcess in user mode.

--*/
{

    EXCEPTION_RECORD ExceptionRecord;
    EXCEPTION_POINTERS ExceptionPointers;

    ZeroMemory(&ExceptionRecord, sizeof(ExceptionRecord));

    ExceptionRecord.ExceptionCode = ExceptionCode;
    ExceptionRecord.ExceptionAddress = (PVOID)CONTEXT_TO_PROGRAM_COUNTER(pContextRecord);

    ExceptionPointers.ExceptionRecord = &ExceptionRecord;
    ExceptionPointers.ContextRecord = pContextRecord;

    ReportFault(&ExceptionPointers, 0);

    // Notify the debugger if connected.

    DebugBreak();

#if defined(KCOREDLL)

    // Give the OAL a chance to restart or shutdown

    KernelIoControl(IOCTL_HAL_REBOOT, NULL, 0, NULL, 0, NULL);
    KernelIoControl(IOCTL_HAL_HALT, NULL, 0, NULL, 0, NULL);

#else

    // Terminate the current process as soon as possible

    ExitProcess(ExceptionCode);

#endif

    // Do not return to the caller under any circumstances

    for (;;)
    {
        Sleep(INFINITE);
    }
}

DWORD *
__crt_get_kernel_flags()
/*++

Description:

    Return a pointer to the kernel's user-mode TLS slot.

--*/
{
    return &UTlsPtr()[TLSSLOT_KERNEL];
}


void
__cdecl
__crt_unrecoverable_error(
    const wchar_t *pszExpression,
    const wchar_t *pszFunction,
    const wchar_t *pszFile,
    unsigned int nLine,
    uintptr_t pReserved
    )
/*++

Description:

    Log the fact that an unrecoverable error occurred

--*/
{
    // Parameters are ignored but may be useful when debugging.

    (pszExpression);
    (pszFunction);
    (pszFile);
    (nLine);
    (pReserved);

    // Capture the context for error reporting.

    CONTEXT ContextRecord;

    _CRT_CAPTURE_CONTEXT(&ContextRecord);

    __crt_fatal(&ContextRecord, (DWORD)STATUS_INVALID_PARAMETER);
}


BOOL
__crtIsDebuggerPresent(void)
{
    return IsDebuggerPresent();
}

immGlob_t *
GetIMMStorage()
{
    return &(((runtimeGlob_t *)TlsGetValue(TLSSLOT_RUNTIME))->immdata);
}

DWORD *
__imm_get_storage_flags()
/*++

Description:

    Return a pointer to a the CRT flags.

--*/
{
    return &GetIMMStorage()->dwFlags;
}

DWORD
GetIMMFlags(
    void
    )
{
    return *__imm_get_storage_flags();
}


void
SetIMMFlag(
    DWORD dwFlag
    )
{
    *__imm_get_storage_flags() |= dwFlag;
}


void
ClearIMMFlag(
    DWORD dwFlag
    )
{
    *__imm_get_storage_flags() &= ~dwFlag;
}


/*
 * STDIO
 */

// Special value interpreted by output functions to redirect to OutputDebugStringW
#define _CRT_DBGOUT_HANDLE ((HANDLE)(-2))

/*
 * Console globals. Initialized on access to the console handles stdin/out/err
 * (if they havn't been redirected). Access to these is protected by the
 * csLowioInitLock both at Init and later in OpenStdConsole & LowioTerm
 */
static HANDLE hConsoleDevice;
static int    iConsoleNumber;
HANDLE hCOMPort = INVALID_HANDLE_VALUE;

/*
 * Value of regkey we read to decide whether to redirect console to COMx: or
 * debugout
 */
static int iConsoleRedir;
static DWORD dwCOMSpeed;

void __cdecl ConsoleRedirInit(void);
BOOL __cdecl LowioInit(void);
void __cdecl LowioTerm(void);
HANDLE xxx_ActivateDeviceEx(LPCWSTR lpszDevKey, LPCVOID lpRegEnts, DWORD cRegEnts, LPVOID lpvParam);
BOOL xxx_DeactivateDevice(HANDLE hDevice);
BOOL xxx_GetDeviceInformationByDeviceHandle(HANDLE hDevice, PDEVMGR_DEVICE_INFORMATION pdi);

#include <winbase.h>

BOOL __cdecl MarkConsoleForDereg(void)
{
    // NOTE: This IOCTL is here to avoid a deadlock between (a) us being in our
    // DllMain calling DeregisterDevice (and hence trying to acquire the device
    // CS *after* the loader CS and (b) Various other devices which acquire the
    // device CS *before* the Loader CS (by calling LoadLib, FreeLib etc inside
    // the device fns etc). See bug#4165.
    
    if (hConsoleDevice)
    {
        // So far the code in device manager doesn't call into loader while
        // holding the device CS; for now enabling deactivation here since
        // auto-deregister works only for devices loaded with RegisterDevice.
        xxx_DeactivateDevice(hConsoleDevice);

        hConsoleDevice = NULL;
        iConsoleNumber = -1;

        return TRUE;
    }

    return FALSE;
}

BOOL __cdecl LowioInit(void)
{
    // We are inside the csInitStdioLock critsec
    // Read the console-redir registry key
    ConsoleRedirInit();

    // but don't do anything else with the console YET
    hConsoleDevice = NULL;
    iConsoleNumber = -1;
    hCOMPort = INVALID_HANDLE_VALUE;
    return TRUE;
}



void __cdecl LowioTerm(void)
{
    EnterCriticalSection(&csLowioInitLock);
    if (LowioInited)
        MarkConsoleForDereg();
    LeaveCriticalSection(&csLowioInitLock);
}

/*------------------------------------------------------------
 * This function is called from LowioInit() to just read the
 * Console redirect registry key. No action is taken based on
 * this, as yet.
 -------------------------------------------------------------*/

// constant regkey names
const WCHAR wszKey[] = L"Drivers\\Console";
const WCHAR wszValName[] = L"OutputTo";
const WCHAR wszSpeedValName[] = L"COMSpeed";

void __cdecl ConsoleRedirInit(void)
{
    HKEY hkey = NULL;
    DWORD dwType, dwLen;
    int iRedir;

    // We are inside the csInitStdioLock critsec
    iConsoleRedir = 0;

    if (ERROR_SUCCESS != RegOpenKeyExW(HKEY_LOCAL_MACHINE, (LPCWSTR)wszKey, 0, KEY_READ, &hkey))
    {
        goto noredir;
    }

    dwLen = sizeof(iRedir);
    if (ERROR_SUCCESS != RegQueryValueExW(hkey, (LPCWSTR)wszValName, NULL, &dwType, (PBYTE)&iRedir, &dwLen))
    {
        goto noredir;
    }

    if ((dwType != REG_DWORD) ||
        (dwLen != sizeof(DWORD)) ||
        (iRedir < -1) ||
        (iRedir == 0) ||
        (iRedir >= 10))
    {
        goto noredir;
    }

    if (iRedir > 0)
    {
        dwLen = sizeof(dwCOMSpeed);

        if (ERROR_SUCCESS != RegQueryValueExW(hkey, (LPCWSTR)wszSpeedValName, NULL, &dwType, (PBYTE)&dwCOMSpeed, &dwLen))
        {
            dwCOMSpeed = CBR_9600;
        }
    }

    iConsoleRedir = iRedir;

noredir:
    if (hkey) RegCloseKey(hkey);
    return;
}


/*----------------------------------------------------------------
 * This function is called from _get_osfhandle when someone tries to access
 * file handles 0, 1 or 2, but there's no OS file handle behind it.
 * We first figure out where we want the console I/O to go
 *
 * If to debugport then this function returns a special value
 * (_CRT_DBGOUT_HANDLE) which is interpreted by My{Read|Write}File macros
 *
 * If to COM port then we try to open the specifid COM port
 *
 * If to Console, then first try to Register the console device if not already
 * done, and then try to open the device
 *----------------------------------------------------------------*/
// macro used to map 0, 1 and 2 to right value for call to GetStdHandle
#define stdaccess(fh)   ((fh==0) ? GENERIC_READ : GENERIC_WRITE)
#define stdshare(fh)    (FILE_SHARE_READ | FILE_SHARE_WRITE)
#define stdcreat(fh, d) (((d) || (fh==0)) ? OPEN_EXISTING : OPEN_ALWAYS)

HANDLE __cdecl OpenStdConsole(int fh, PBOOL is_dev)
{
    WCHAR szDevName[] = L"COXX:";
    WCHAR szTempPath[MAX_PATH];
    DWORD dwLen = MAX_PATH;
    HANDLE hFile = INVALID_HANDLE_VALUE;

    ASSERT(fh>=0 && fh<=2);

    EnterCriticalSection(&csLowioInitLock);
    if (!LowioInited)
        LowioInited = LowioInit();
    LeaveCriticalSection(&csLowioInitLock);

    if (!LowioInited)
        return hFile;

    *is_dev = TRUE;

    // This function is NOT called on init, so we are NOT inside any global critsecs
    // However we should be inside the lock for the *current* FILEX struct

    // First try to get inherited stdin/out/err "handle" if any
    if (GetStdioPathW(fh, (PWSTR)szTempPath, &dwLen) && dwLen && szTempPath[0])
    {
        // check if it's a device or file
        BOOL fDev = (lstrlen((LPCTSTR)szTempPath)==5 && szTempPath[4]==':');

        // open with approp modes based on fh & fDev
        hFile = CreateFileW(szTempPath, stdaccess(fh), stdshare(fh), NULL, stdcreat(fh, fDev), 0, NULL);

        if (INVALID_HANDLE_VALUE != hFile)
        {
            // set up new inheritance
            SetStdioPathW(fh, (LPCWSTR)szTempPath);

            // if not a device, turn off FDEV bit
            if (!fDev)
            {
                *is_dev = FALSE;
                if (fh != 0)    // Write at the end, but read at the beginning.
                    SetFilePointer (hFile, 0, NULL, FILE_END);
            }

            goto done;
        }
        // else fall thru
    }
    
    // Didnt inherit. Check for redir & open new console if neccesary
    if (iConsoleRedir == 0)
    {
        // Not redirected
        if (!hConsoleDevice)
        {
            // Try to register a unique console device
            int i;

            EnterCriticalSection(&csLowioInitLock); // get the init critsec
            for (i=1; i<10; i++)
            {
                hConsoleDevice = xxx_ActivateDeviceEx(L"Drivers\\Console", NULL, 0, NULL);

                if (hConsoleDevice)
                {
                    DEVMGR_DEVICE_INFORMATION di;
                    memset(&di, 0, sizeof(di));
                    di.dwSize = sizeof(di);
                    if (xxx_GetDeviceInformationByDeviceHandle(hConsoleDevice, &di))
                    {
                        iConsoleNumber = di.szLegacyName[3] - '0';
                        break;
                    }
                    else
                    {
                        xxx_DeactivateDevice(hConsoleDevice);
                        hConsoleDevice = NULL;
                    }
                }
            }
            LeaveCriticalSection(&csLowioInitLock);
        }

        if (hConsoleDevice)
        {
            szDevName[2] = L'N';
            szDevName[3] = (L'0' + iConsoleNumber);
            hFile = CreateFileW(szDevName, stdaccess(fh), stdshare(fh), NULL, OPEN_EXISTING, 0, NULL);

            // set up new inheritance
            if(INVALID_HANDLE_VALUE != hFile)
            {
                SetStdioPathW(fh, (LPCWSTR)szDevName);
            }
        }
    }
    else if (iConsoleRedir == (-1)) // Debug port
    {
        hFile = _CRT_DBGOUT_HANDLE;
    }
    else // COM port
    {
        // The Serial driver doesn't like to opened multiply even if in
        // mutually compatible R/W modes (for stdin & stdout).
        // So we open it just once with R & W access and cache the handle.
        if (INVALID_HANDLE_VALUE!=hCOMPort)
        {
            hFile = hCOMPort;
        }
        else
        {
            szDevName[2] = L'M';
            szDevName[3] = (L'0' + iConsoleRedir);
            hFile = CreateFileW(szDevName, GENERIC_READ|GENERIC_WRITE, stdshare(fh), NULL, OPEN_EXISTING, 0, NULL);

            // cache the handle & set up new inheritance
            // also set Comm settings
            if (INVALID_HANDLE_VALUE != hFile)
            {
                COMMTIMEOUTS CommTimeouts;
                DCB dcb;

                hCOMPort = hFile;
                SetStdioPathW(fh, (LPCWSTR)szDevName);

                if (GetCommTimeouts(hFile, &CommTimeouts))
                {
                    CommTimeouts.ReadIntervalTimeout = 100;          // Want SMALL number here
                    CommTimeouts.ReadTotalTimeoutConstant = 1000000; // Want LARGE number here
                    SetCommTimeouts(hFile, &CommTimeouts);
                }
                if (GetCommState(hFile, &dcb))
                {
                    dcb.BaudRate = dwCOMSpeed;
                    SetCommState(hFile, &dcb);
                }
            }
        }
    }

done:
    return hFile;
}

HANDLE __cdecl GetCOMPortHandle(void)
{
	return hCOMPort;
}

int _purecall(
	void
	)
{
    return 0;
}

} // extern "C"

