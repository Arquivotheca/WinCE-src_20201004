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
/***
* stioinit.c - Init that is ONLY pulled in if CORSTIOA or CORESTIOW
*       components are selected
*
*Purpose:
*       Init code for all the STDIO functionality in CE
*
*******************************************************************************/

#include <cruntime.h>
#include <crtmisc.h>
#include <internal.h>
#include <malloc.h>
#include <mtdll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <crttchar.h>
#include <dbgint.h>
#include <msdos.h>
#include <coredll.h>
#include <console.h>

extern FARPROC pfnTermStdio;

/*
 * This data should NOT be pulled in in the STR-only version
 * Other Locks
 */
CRITICAL_SECTION csIobScanLock;   // used to lock entire __piob array

/*
 * This is the basic structure of the STDIO/buffered IO portion
 * of the library. The FILE handle apps get are actualy FILEX ptrs
 * This array grows dynamically in increments of NSTREAM_INCREMENT
 */
FILEX** __piob; // pointer to an array of ptrs to FILEX*
int _nstream;   // Current length of the __piob array.

/*
 * Console globals. Initialized on access to the console handles stdin/out/err
 * (if they havn't been redirected). Access to these is protected by the
 * csStdioInitLock both at Init and later in OpenStdConsole & LowioTerm
 */
static HANDLE hConsoleDevice;
static int    iConsoleNumber;
HANDLE hCOMPort = INVALID_HANDLE_VALUE;

tGetCommState g_pfnGetCommState = NULL;
tSetCommState g_pfnSetCommState = NULL;
tSetCommTimeouts g_pfnSetCommTimeouts = NULL;
tGetCommTimeouts g_pfnGetCommTimeouts = NULL;

static tActivateDeviceEx g_pfnActivateDeviceEx = NULL;
static tDeactivateDevice g_pfnDeactivateDevice = NULL;
static tGetDeviceInformationByDeviceHandle g_pfnGetDeviceInformationByDeviceHandle = NULL;

/*
 * Value of regkey we read to decide whether to redirect console to COMx: or
 * debugout
 */
static int iConsoleRedir;
static DWORD dwCOMSpeed;


static
BOOL
LoadCoredllApi(FARPROC * ppfn, LPCSTR pszApiName)
{
    FARPROC pfn;

    _ASSERTE(ppfn != NULL);
    _ASSERTE(pszApiName != NULL);

    if (*ppfn != NULL)
    {
        // Already loaded
        return TRUE;
    }

    DEBUGCHK(hInstCoreDll != NULL);

    if (hInstCoreDll)
    {
        pfn = (FARPROC)GetProcAddressA(hInstCoreDll, pszApiName);
        if (pfn != NULL)
        {
            *ppfn = pfn;
            return TRUE;
        }
    }

    return FALSE;
}


/*
 * Functions declared in this file
 */
void __cdecl ConsoleRedirInit(void);
int  __cdecl TermStdio(void);
BOOL __cdecl InitPiob(void);
void __cdecl TermPiob(void);
void __cdecl MarkConsoleForDereg(void);
BOOL __cdecl LowioInit(void);
void __cdecl LowioTerm(void);
int  __cdecl _real_filbuf2(FILEX *str, int fWide);
int  __cdecl _real_flsbuf2(int ch,FILEX *str, int fWide);

int __cdecl _InitStdioLib(void)
{
    // already inside csStdioInitLock
    DEBUGMSG (DBGSTDIO, (TEXT("Stdio: STDIO is being INITED\r\n")));

    // Init the rest of the locks
    InitializeCriticalSection(&csIobScanLock);

    if (!LowioInit())
        return FALSE;

    // setup __piob array & init stdin/out/err FILEX structs. Must be last
    if(!InitPiob())
        return FALSE;

    // Init for the str-only library
    _pfnfilbuf2 = _real_filbuf2;
    _pfnflsbuf2 = _real_flsbuf2;

    // setup cleanup function pointer
    pfnTermStdio = TermStdio;
    return TRUE;
}

/*
 * Cleanup
 */

int __cdecl TermStdio(void)
{
    EnterCriticalSection(&csStdioInitLock);
    if(!fStdioInited) goto done;

    // we can't do a real deregister due to possibel deadlock
    // so we've added this IOCTL hack. Do this before TermPiob
    MarkConsoleForDereg();

    TermPiob();  // flush & close all stream & free __piob. Must be called *before* LowioTerm
    LowioTerm(); // DeRegister our console device if any

    DeleteCriticalSection(&csIobScanLock);
    fStdioInited = FALSE;

done:
    LeaveCriticalSection(&csStdioInitLock);
    return 0;
}

/***
* InitPiob
*   Create and initialize the __piob array.
*******************************************************************************/

BOOL __cdecl InitPiob(void)
{
    int i;

    // We are inside the csInitStdioLock critsec
    _nstream = INITIAL_NSTREAM;
    __piob = (FILEX**)_malloc_crt(_nstream * sizeof(FILEX*));
    if (!__piob)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
    memset(__piob, 0, (_nstream * sizeof(FILEX*)));

    // Initialize the stdin/out/err handles
    for (i = 0 ; i < 3 ; i++)
    {
        // allocate a new _FILEX, set _piob[i] to it and return a pointer to it.
        __piob[i] = _malloc_crt(sizeof(_FILEX));
        if (!__piob[i])
        {
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return FALSE;
        }
        memset(__piob[i], 0, sizeof(_FILEX));
        InitializeCriticalSection( &(__piob[i]->lock) );
        __piob[i]->fd = i;
        __piob[i]->osfhnd = INVALID_HANDLE_VALUE;
        __piob[i]->peekch = '\n';

        // Initialize stdio with invalid handles. When anyone tries actual
        // IO on these handles we'll catch them & open the console then.
        // However these must be set like it's all A-OK.
        __piob[i]->osfile = (BYTE)(FTEXT|FDEV|FOPEN);
        __piob[i]->_flag = ((i == 0) ? _IOREAD : _IOWRT);
    }
    return TRUE;
}

extern void *_stdbuf[2];    // _sftbuf.c
int _deletestream(int);     // fflush.c

/***
* TermPiob
*
*   (1) Flush all streams.
*
*   (2) If returning to caller, close all streams.  This is
*   not necessary if the exe is terminating because the OS will
*   close the files for us (much more efficiently, too).
*
*******************************************************************************/

void __cdecl TermPiob(void)
{
    // We are inside the csInitStdioLock critsec

    int i;

    // flush all streams
    _flushall();

    // close and free all streams
    EnterCriticalSection(&csIobScanLock);

    for (i = 0; i < _nstream; ++i)
    {
        if (__piob[i] != NULL)
        {
            _deletestream(i);
        }
    }

    _nstream = 0;

    // free the __piob array
    _free_crt(__piob);
    __piob = NULL;

    // free the temporary stdout/err buffers
    for (i = 0; i < _countof(_stdbuf); ++i)
    {
        if (_stdbuf[i] != NULL)
        {
            _free_crt(_stdbuf[i]);
            _stdbuf[i] = NULL;
        }
    }

    LeaveCriticalSection(&csIobScanLock);
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

void __cdecl MarkConsoleForDereg(void)
{
    int i;
    // NOTE: This IOCTL is here to avoid a deadlock between (a) us being in our
    // DllMain calling DeregisterDevice (and hence trying to acquire the device
    // CS *after* the loader CS and (b) Various other devices which acquire the
    // device CS *before* the Loader CS (by calling LoadLib, FreeLib etc inside
    // the device fns etc). See bug#4165.

    // IOCTL call has to be on a *file* handle, not a device handle, so we try
    // all 3 of stdin, out & err
    if (hConsoleDevice)
    {
        for (i=0; i<3; i++)
        {
            if (__piob[i]->osfhnd != INVALID_HANDLE_VALUE)
                CloseHandle(__piob[i]->osfhnd);
        }

        // So far the code in device manager doesn't call into loader while
        // holding the device CS; for now enabling deactivation here since
        // auto-deregister works only for devices loaded with RegisterDevice.
        if (g_pfnDeactivateDevice)
        {
            (*g_pfnDeactivateDevice)(hConsoleDevice);
        }

        hConsoleDevice = NULL;
        iConsoleNumber = -1;
    }
}

void __cdecl LowioTerm(void)
{
    // does nothing now
}

/*------------------------------------------------------------
 * This function is called from LowioInit() to just read the
 * Console redirect registry key. No action is taken based on
 * this, as yet.
 -------------------------------------------------------------*/

// constant regkey names
const WCHAR wszKey[] = TEXT("Drivers\\Console");
const WCHAR wszValName[] = TEXT("OutputTo");
const WCHAR wszSpeedValName[] = TEXT("COMSpeed");

void __cdecl ConsoleRedirInit(void)
{
    HKEY hkey = NULL;
    DWORD dwType, dwLen;
    int iRedir;

    // We are inside the csInitStdioLock critsec
    iConsoleRedir = 0;

    if (ERROR_SUCCESS != RegOpenKeyExW(HKEY_LOCAL_MACHINE, wszKey, 0, KEY_READ, &hkey))
    {
        goto noredir;
    }

    dwLen = sizeof(iRedir);
    if (ERROR_SUCCESS != RegQueryValueExW(hkey, wszValName, NULL, &dwType, (PBYTE)&iRedir, &dwLen))
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

        if (ERROR_SUCCESS != RegQueryValueExW(hkey, wszSpeedValName, NULL, &dwType, (PBYTE)&dwCOMSpeed, &dwLen))
        {
            dwCOMSpeed = CBR_9600;
        }
    }

    iConsoleRedir = iRedir;

    if (iRedir == -1)
    {
        DEBUGMSG(DBGSTDIO, (TEXT("Console redirected to DEBUG\r\n")));
    }
    else
    {
        DEBUGMSG(DBGSTDIO, (TEXT("Console redirected to COM%d\r\n"), iConsoleRedir));
    }

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

HANDLE __cdecl OpenStdConsole(int fh)
{
    WCHAR szDevName[] = TEXT("COXX:");
    WCHAR szTempPath[MAX_PATH];
    DWORD dwLen = MAX_PATH;
    HANDLE hFile = INVALID_HANDLE_VALUE;

    DEBUGMSG (DBGSTDIO, (TEXT("Stdio: OpenStdConsole(%d)\r\n"), fh));

    _ASSERTE(fh>=0 && fh<=2);

    // This function is NOT called on init, so we are NOT inside any global critsecs
    // However we should be inside the lock for the *current* FILEX struct

    // First try to get inherited stdin/out/err "handle" if any
    if (GetStdioPathW(fh, szTempPath, &dwLen) && dwLen && szTempPath[0])
    {
        // check if it's a device or file
        BOOL fDev = (lstrlen(szTempPath)==5 && szTempPath[4]==':');

        DEBUGMSG (DBGSTDIO, (TEXT("Stdio: OpenStdConsole(%d)--inherited path(%s) l=%d dev=%d\r\n"), fh, szTempPath, dwLen, fDev));

        // open with approp modes based on fh & fDev
        hFile = CreateFileW(szTempPath, stdaccess(fh), stdshare(fh), NULL, stdcreat(fh, fDev), 0, NULL);

        if (INVALID_HANDLE_VALUE != hFile)
        {
            // set up new inheritance
            SetStdioPathW(fh, szTempPath);

            // if not a device, turn off FDEV bit
            if (!fDev)
            {
                _osfile(fh) &= (char)(~FDEV);
                if (fh != 0)    // Write at the end, but read at the beginning.
                    SetFilePointer (hFile, 0, NULL, FILE_END);
            }

            goto done;
        }
        // else fall thru
    }
    DEBUGMSG (DBGSTDIO, (TEXT("Stdio: OpenStdConsole(%d)--FAILED to inherit path\r\n"), fh));

    // Didnt inherit. Check for redir & open new console if neccesary
    if (iConsoleRedir == 0)
    {
        // Not redirected
        if (!hConsoleDevice)
        {
            // Try to register a unique console device
            int i;

            LoadCoredllApi((FARPROC*)&g_pfnActivateDeviceEx, "ActivateDeviceEx");
            LoadCoredllApi((FARPROC*)&g_pfnDeactivateDevice, "DeactivateDevice");
            LoadCoredllApi((FARPROC*)&g_pfnGetDeviceInformationByDeviceHandle, "GetDeviceInformationByDeviceHandle");

            EnterCriticalSection(&csStdioInitLock); // get the init critsec
            for (i=1; i<10; i++)
            {
                if (g_pfnActivateDeviceEx)
                {
                   hConsoleDevice = (*g_pfnActivateDeviceEx)(L"Drivers\\Console", NULL, 0, NULL);
                }

                if (hConsoleDevice)
                {
                    DEVMGR_DEVICE_INFORMATION di;
                    DEBUGMSG (DBGSTDIO, (TEXT("Stdio: OpenStdConsole(%d)--Registered Console %d\r\n"), fh, i));
                    memset(&di, 0, sizeof(di));
                    di.dwSize = sizeof(di);
                    if (g_pfnGetDeviceInformationByDeviceHandle &&
                        (*g_pfnGetDeviceInformationByDeviceHandle)(hConsoleDevice, &di))
                    {
                        iConsoleNumber = di.szLegacyName[3] - '0';
                        break;
                    }
                    else
                    {
                        if (g_pfnDeactivateDevice)
                        {
                            (*g_pfnDeactivateDevice)(hConsoleDevice);
                        }
                        hConsoleDevice = NULL;
                    }
                }
            }
            LeaveCriticalSection(&csStdioInitLock);
        }

        if (hConsoleDevice)
        {
            DEBUGMSG (DBGSTDIO, (TEXT("Stdio: OpenStdConsole(%d)--Opening Console %d\r\n"), fh, iConsoleNumber));
            szDevName[2] = L'N';
            szDevName[3] = (WCHAR)(L'0' + iConsoleNumber);
            hFile = CreateFileW(szDevName, stdaccess(fh), stdshare(fh), NULL, OPEN_EXISTING, 0, NULL);

            // set up new inheritance
            if(INVALID_HANDLE_VALUE != hFile)
            {
                SetStdioPathW(fh, szDevName);
            }
        }
    }
    else if (iConsoleRedir == (-1)) // Debug port
    {
        DEBUGMSG (DBGSTDIO, (TEXT("Stdio: OpenStdConsole(%d)--Redirected to DEBUG\r\n"), fh));
        hFile = _CRT_DBGOUT_HANDLE;
    }
    else // COM port
    {
        LoadCoredllApi((FARPROC*)&g_pfnGetCommTimeouts, "GetCommTimeouts");
        LoadCoredllApi((FARPROC*)&g_pfnGetCommState,    "GetCommState");
        LoadCoredllApi((FARPROC*)&g_pfnSetCommTimeouts, "SetCommTimeouts");
        LoadCoredllApi((FARPROC*)&g_pfnSetCommState,    "SetCommState");

        // The Serial driver doesn't like to opened multiply even if in
        // mutually compatible R/W modes (for stdin & stdout).
        // So we open it just once with R & W access and cache the handle.
        if (INVALID_HANDLE_VALUE!=hCOMPort)
        {
            DEBUGMSG (DBGSTDIO, (TEXT("Stdio: OpenStdConsole(%d)--RE-USING COM %d\r\n"), fh, iConsoleRedir));
            hFile = hCOMPort;
        }
        else if (g_pfnGetCommTimeouts && g_pfnSetCommTimeouts && g_pfnGetCommState && g_pfnSetCommState)
        {
            DEBUGMSG (DBGSTDIO, (TEXT("Stdio: OpenStdConsole(%d)--Opening COM %d\r\n"), fh, iConsoleRedir));
            szDevName[2] = L'M';
            szDevName[3] = (WCHAR)(L'0' + iConsoleRedir);
            hFile = CreateFileW(szDevName, GENERIC_READ|GENERIC_WRITE, stdshare(fh), NULL, OPEN_EXISTING, 0, NULL);

            // cache the handle & set up new inheritance
            // also set Comm settings
            if (INVALID_HANDLE_VALUE != hFile)
            {
                COMMTIMEOUTS CommTimeouts;
                DCB dcb;

                hCOMPort = hFile;
                SetStdioPathW(fh, szDevName);

                if (g_pfnGetCommTimeouts(hFile, &CommTimeouts))
                {
                    CommTimeouts.ReadIntervalTimeout = 100;          // Want SMALL number here
                    CommTimeouts.ReadTotalTimeoutConstant = 1000000; // Want LARGE number here
                    g_pfnSetCommTimeouts(hFile, &CommTimeouts);
                }
                if (g_pfnGetCommState(hFile, &dcb))
                {
                    dcb.BaudRate = dwCOMSpeed;
                    g_pfnSetCommState(hFile, &dcb);
                }
            }
        }
    }

done:
    _osfhnd(fh) = hFile;
    DEBUGMSG (DBGSTDIO, (TEXT("Stdio: OpenStdConsole(%d)--returning(%x)\r\n"), fh, hFile));
    return _osfhnd(fh);
}

/*
 * This functions has nothing to do with init, but it's in need of a good home
 * It translates the stdin/stdout/stderr external macros to FILE handles
 */
FILE* __cdecl _getstdfilex(int i)
{
    if(!CheckStdioInit())
        return NULL;

    return (FILE*)__piob[i];
}

