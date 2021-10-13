//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/***
* stioinit.c - Init that is ONLY pulled in if CORSTIOA or CORESTIOW 
*       components are selected
*
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

// This data should NOT be pulled in in the STR-only version
// Other Locks
CRITICAL_SECTION csIobScanLock;   // used to lock entire __piob array

// This is the basic structure of the STDIO/buffered IO portion
// of the library. The FILE handle apps get are actualy FILEX ptrs
// This array grows dynamically in increments of NSTREAM_INCREMENT
FILEX** __piob; // pointer to an array of ptrs to FILEX*
int _nstream;   // Current length of the __piob array.

// Console globals. Initialized on access to the console handles stdin/out/err
// (if they havn't been redirected). Access to these is protected by the 
// csStdioInitLock both at Init and later in OpenStdConsole & LowioTerm
HANDLE hConsoleDevice;
int    iConsoleNumber;
HANDLE hCOMPort = INVALID_HANDLE_VALUE;
HMODULE hCoreDll = NULL;

tGetCommState fGetCommState = NULL;
tSetCommState fSetCommState = NULL;
tSetCommTimeouts fSetCommTimeouts = NULL;
tGetCommTimeouts fGetCommTimeouts = NULL;

// Value of regkey we read to decide whether to redirect console to COMx: or debugout
int iConsoleRedir;
DWORD dwCOMSpeed;

// functions decalred in this file
void __cdecl ConsoleRedirInit(void);
int __cdecl TermStdio(void);
BOOL __cdecl InitPiob(void);
void __cdecl TermPiob(void);
void __cdecl MarkConsoleForDereg(void);
BOOL __cdecl LowioInit(void);
void __cdecl LowioTerm(void);
int __cdecl _real_filbuf2(FILEX *str, int fWide);
int __cdecl _real_flsbuf2(int ch,FILEX *str, int fWide);

int __cdecl _InitStdioLib(void)
{
    // already inside csStdioInitLock
    DEBUGMSG (DBGSTDIO, (TEXT("Stdio: STDIO is being INITED\r\n")));
    
    // Init the rest of the locks
    InitializeCriticalSection(&csIobScanLock);
    
    if(!LowioInit())
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
    if(!(__piob = (FILEX**)_malloc_crt(_nstream * sizeof(FILEX*))))
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
    memset(__piob, 0, (_nstream * sizeof(FILEX*)));

    // Initialize the stdin/out/err handles
    for ( i = 0 ; i < 3 ; i++ ) 
    {
        // allocate a new _FILEX, set _piob[i] to it and return a pointer to it.
        if(!(__piob[i] = _malloc_crt(sizeof(_FILEX))))
        {
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return FALSE;
        }
        memset(__piob[i], 0, sizeof(_FILEX));
        InitializeCriticalSection( &(__piob[i]->lock) );
        __piob[i]->fd = i;
        __piob[i]->osfhnd = INVALID_HANDLE_VALUE;
        __piob[i]->peekch = '\n';

        // These have been initialize them with invalid handles. When anyone tries actual 
        // IO to these handles we'll catch them & open the console then
        // however these must be set like it's all A-OK
        __piob[i]->osfile = (BYTE)(FTEXT|FDEV|FOPEN); 
        __piob[i]->_flag = ((i==0) ? _IOREAD : _IOWRT);
    }
    return TRUE;
}

/***
* TermPiob
*
*   (1) Flush all streams.  (Do this even if we're going to
*   call fcloseall since that routine won't do anything to the
*   std streams.)
*
*   (2) If returning to caller, close all streams.  This is
*   not necessary if the exe is terminating because the OS will
*   close the files for us (much more efficiently, too).
*
*******************************************************************************/

void __cdecl TermPiob(void)
{
    // We are inside the csInitStdioLock critsec
    
    // flush all streams 
    _flushall();
    
    _fcloseall();

    

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
    DWORD dwDummy;
    // NOTE: This IOCTL is here to avoid a deadlock between (a) us being in our 
    // DllMain calling DeregisterDevice (and hence trying to acquire the device 
    // CS *after* the loader CS and (b) Various other devices which acquire the 
    // device CS *before* the Loader CS (by calling LoadLib, FreeLib etc inside
    // the device fns etc). See #4165. 

    // IOCTL call has to be on a *file* handle, not a device handle, so we try
    // all 3 of stdin, out & err
    if(hConsoleDevice)
    {
        for(i=0; i<3; i++)
        {
            if(__piob[i]->osfhnd != INVALID_HANDLE_VALUE)
                DeviceIoControl(__piob[i]->osfhnd, IOCTL_DEVICE_AUTODEREGISTER, NULL, 0, NULL, 0, &dwDummy, NULL);
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
    
    if(ERROR_SUCCESS != RegOpenKeyExW(HKEY_LOCAL_MACHINE, wszKey, 0, KEY_READ, &hkey))
        goto noredir;

    dwLen=sizeof(iRedir);
    if(ERROR_SUCCESS != RegQueryValueExW(hkey, wszValName, NULL, &dwType, (PBYTE)&iRedir, &dwLen))
        goto noredir;

    if(dwType!=REG_DWORD || dwLen!=sizeof(DWORD) || iRedir<(-1) || iRedir==0 || iRedir>=10)
        goto noredir;

    if(iRedir>0)
    {
        dwLen=sizeof(dwCOMSpeed);
        if(ERROR_SUCCESS != RegQueryValueExW(hkey, wszSpeedValName, NULL, &dwType, (PBYTE)&dwCOMSpeed, &dwLen))
            dwCOMSpeed = 19600;
    }
    iConsoleRedir = iRedir;

#ifdef DEBUG
    if(iConsoleRedir==(-1))
        DEBUGMSG(1, (TEXT("Console redirected to DEBUG for process 0x%X\r\n"), GetCurrentProcessId()));
    else
        DEBUGMSG(1, (TEXT("Console redirected to COM%d for process 0x%X\r\n"), iConsoleRedir, GetCurrentProcessId()));
#endif
    if(hkey) RegCloseKey(hkey);
    return;

noredir:
    DEBUGMSG(1, (TEXT("Console NOT redirected for process 0x%X\r\n"), GetCurrentProcess()));
    if(hkey) RegCloseKey(hkey);
    return;
}



/*----------------------------------------------------------------
 * This function is called from _get_osfhandle when someone tries to access 
 * file handles 0, 1 or 2, but there's no OS file handle behind it.
 * We first figure out where we want the console I/O to go
 *
 * If to debugport then //NYI//this function always returns a special value of (-2)
 * which is interpreted by our MyReadFile & MyWriteFile etc macros//NYI//
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
    if(GetStdioPathW(fh, szTempPath, &dwLen) && dwLen && szTempPath[0])
    {
        // check if it's a device or file
        BOOL fDev = (lstrlen(szTempPath)==5 && szTempPath[4]==':');
        
        DEBUGMSG (DBGSTDIO, (TEXT("Stdio: OpenStdConsole(%d)--inherited path(%s) l=%d dev=%d\r\n"), fh, szTempPath, dwLen, fDev));
        
        // open with approp modes based on fh & fDev
        hFile = CreateFile(szTempPath, stdaccess(fh), stdshare(fh), NULL, stdcreat(fh, fDev), 0, NULL);

        if(INVALID_HANDLE_VALUE != hFile)
        {
            // set up new inheritance
            SetStdioPathW(fh, szTempPath);
            
            // if not a device, turn off FDEV bit
            if(!fDev)
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
    if(iConsoleRedir==0)
    {
        // Not redirected
        if(!hConsoleDevice)
        {
            // Try to register a unique console device
            int i;
            EnterCriticalSection(&csStdioInitLock); // get the init critsec
            for(i=1; i<10; i++)
            {
                if(hConsoleDevice = RegisterDevice_Trap(L"CON", i, L"console.dll", 0))
                {
                    DEBUGMSG (DBGSTDIO, (TEXT("Stdio: OpenStdConsole(%d)--Registered Console %d\r\n"), fh, i));
                    iConsoleNumber = i;
                    break;
                }
            }
            LeaveCriticalSection(&csStdioInitLock);
        }
            
        if(hConsoleDevice)
        {
            DEBUGMSG (DBGSTDIO, (TEXT("Stdio: OpenStdConsole(%d)--Opening Console %d\r\n"), fh, iConsoleNumber));
            szDevName[2] = 'N';
            szDevName[3] = '0'+iConsoleNumber;
            hFile = CreateFile(szDevName, stdaccess(fh), stdshare(fh), NULL, OPEN_EXISTING, 0, NULL);

            // set up new inheritance
            if(INVALID_HANDLE_VALUE != hFile)
                SetStdioPathW(fh, szDevName);
        }
    }
    else if(iConsoleRedir == (-1)) // Debug port
    {
        DEBUGMSG (DBGSTDIO, (TEXT("Stdio: OpenStdConsole(%d)--Redirected to DEBUG\r\n"), fh));
        hFile = (HANDLE)(-2);
    }
    else // COM port
    {
        if (! hCoreDll)
            hCoreDll = LoadLibrary (L"coredll.dll");

        if (hCoreDll)
        {
            if (! fGetCommTimeouts)
                fGetCommTimeouts = (tGetCommTimeouts)GetProcAddress (hCoreDll, L"GetCommTimeouts");

            if (! fSetCommTimeouts)
                fSetCommTimeouts = (tSetCommTimeouts)GetProcAddress (hCoreDll, L"SetCommTimeouts");

            if (! fGetCommState)
                fGetCommState = (tGetCommState)GetProcAddress (hCoreDll, L"GetCommState");

            if (! fSetCommState)
                fSetCommState = (tSetCommState)GetProcAddress (hCoreDll, L"SetCommState");
        }

        // The Serial driver doesn't like to opened multiply even if in 
        // mutually compatible R/W modes (for stdin & stdout). 
        // So we open it just once with R & W access and cache the handle.
        if(INVALID_HANDLE_VALUE!=hCOMPort)
        {
            DEBUGMSG (DBGSTDIO, (TEXT("Stdio: OpenStdConsole(%d)--RE-USING COM %d\r\n"), fh, iConsoleRedir));
            hFile = hCOMPort;
        }
        else if (fGetCommTimeouts && fSetCommTimeouts && fGetCommState && fSetCommState)
        {
            DEBUGMSG (DBGSTDIO, (TEXT("Stdio: OpenStdConsole(%d)--Opening COM %d\r\n"), fh, iConsoleRedir));
            szDevName[2] = 'M';
            szDevName[3] = '0'+iConsoleRedir;
            hFile = CreateFile(szDevName, GENERIC_READ|GENERIC_WRITE, stdshare(fh), NULL, OPEN_EXISTING, 0, NULL);
                    
            // cache the handle & set up new inheritance
            // also set Comm settings
            if(INVALID_HANDLE_VALUE != hFile)
            {
                COMMTIMEOUTS CommTimeouts;
                DCB dcb;

                hCOMPort = hFile;
                SetStdioPathW(fh, szDevName);

                if(fGetCommTimeouts(hFile, &CommTimeouts))
                {
                    CommTimeouts.ReadIntervalTimeout = 100;          // Want SMALL number here
                    CommTimeouts.ReadTotalTimeoutConstant = 1000000; // Want LARGE number here
                    fSetCommTimeouts(hFile, &CommTimeouts);
                }
                if(fGetCommState(hFile, &dcb))
                {
                    dcb.BaudRate = dwCOMSpeed;
                    fSetCommState(hFile, &dcb);
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
 *
 */
FILE* __cdecl _getstdfilex(int i)
{
    if(!CheckStdioInit())
        return NULL;

    return (FILE*)__piob[i];
}

