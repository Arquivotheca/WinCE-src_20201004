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
#include <windows.h>


#include "shellapi.h"
#include "shell.h"
#define KEEP_SYSCALLS
#include <kernel.h>
#include <celog.h>
#include <windev.h>
#include <strsafe.h>
#include <profiler.h>
#include <pfsioctl.h>
#include <acctid.h>
#include <intsafe.h>

// for ARM, page protection flags is stored in KData, for v6 or later uses a different
// set of flags. Since we look at page table directly for 'mi' command, we need to
// re-define KData to use UserKData.
#ifdef  g_pKData
#undef  g_pKData
#endif
#define g_pKData       ((struct KDataStruct *)PUserKData)

#define ARRAYSIZE(a)    (sizeof (a) / sizeof ((a)[0]))

#define MAX_PRIORITY_LEVELS 256
#define _4M                 (VM_PAGE_SIZE * 1024)               // 4M per-section

extern PSTR
MapfileLookupAddress(
    IN  PWSTR wszModuleName,
    IN  DWORD dwAddress);

// Moved this to a global since it exceeds the stack hog threshold.
TCHAR v_CmdLine[MAX_CMD_LINE];

OBJSTORE_RANGE      v_ObjstoreRange;
PSHELL_EXTENSION    v_ShellExtensions;

#define IsObjectStore(addr)     ((DWORD) (addr) - (DWORD) v_ObjstoreRange.pBase < v_ObjstoreRange.cbMaxSize)

#define INFOSIZE 8192
LPBYTE rgbInfo;
TCHAR   rgbText[512];
UINT    ccbrgbText = sizeof(rgbText);
DWORD TotalCpgRW;

#ifdef DEBUG
DBGPARAM dpCurSettings = {
    TEXT("Shell"), {
    TEXT("Info"),TEXT("n/a"),TEXT("n/a"),TEXT("n/a"),
    TEXT("n/a"),TEXT("n/a"),TEXT("n/a"),TEXT("n/a"),
    TEXT("n/a"),TEXT("n/a"),TEXT("n/a"),TEXT("n/a"),
    TEXT("n/a"),TEXT("n/a"),TEXT("n/a"),TEXT("n/a") },
    0x00000000
};
#define ZONE_INFO DEBUGZONE(0)
#endif

#define IsWhiteSpace(c) ((c)==TEXT(' ') || (c)==TEXT('\r') || (c)==TEXT('\n'))

//
// Event used to find out if an image has GWES support or not
// If OpenEvent() on this event name fails, then GWES is not
// going to come up. If OpenEvent() on this event name succeeds,
// then clients can use WaitForSingleObject(hEvent, 0) to wait
// for GWES support to come on-line.
const WCHAR* g_pwszGwesEventName = L"SYSTEM/GweApiSetReady";
HANDLE g_hGwesEvent = NULL;

//
// Flags to disable first chance exceptions for a thread.  This allows
// the try/except wrapper to handle it instead of trapping first to the
// debugger
#define DISABLEFAULTS()    (UTlsPtr()[TLSSLOT_KERNEL] |= TLSKERN_NOFAULT)
#define ENABLEFAULTS()    (UTlsPtr()[TLSSLOT_KERNEL] &= ~TLSKERN_NOFAULT)

static VOID DoLoadExtHelp();
static VOID DoKillProcHelp();

// Update tagCMDS in shell.h if this changes.
const CMDCLASS v_commands[] =  {
    { 0, TEXT("exit"), DoExit},
    { 0, TEXT("start"), DoStartProcess},
    { 0, TEXT("s"), DoStartProcess},
    { 0, TEXT("gi"), DoGetInfo},
    { 0, TEXT("zo"), DoZoneOps},
    { 0, TEXT("break"), DoBreak},
    { 0, TEXT("kp"), DoKillProc},
    { 0, TEXT("?"), DoHelp},
    { 0, TEXT("dd"), DoDumpDwords},
    { 0, TEXT("df"), DoDumpFile},
    { 0, TEXT("mi"), DoMemtool},
    { 0, TEXT("run"), DoRunBatchFile},
    { 0, TEXT("dis"), DoDiscard},
    { 0, TEXT("win"), DoWinList},
    { 0, TEXT("log"), DoLog},
    { 0, TEXT("memtrack"), DoMemTrack},    
    { 0, TEXT("hd"), DoHeapDump},
    { 0, TEXT("options"), DoOptions},
    { 0, TEXT("suspend"), DoSuspend},
    { 0, TEXT("prof"), DoProfile},
    { 0, TEXT("loadext"), DoLoadExt},
    { 0, TEXT("tp"), DoThreadPrio},
    { 0, TEXT("fi"), DoFindFile},
    { 0, TEXT("dev"), DoDeviceInfo},
};

ROMChain_t *pROMChain;

typedef struct _THRD_INFO {
    struct _THRD_INFO   *pNext;
    DWORD   dwThreadID;
    // keep FILE_TIME 8-byte aligned.
    FILETIME    ftKernThrdTime;
    FILETIME    ftUserThrdTime;
    DWORD   dwFlags;
} THRD_INFO, *PTHRD_INFO;

typedef struct _PROC_DATA {
    struct _PROC_DATA   *pNext;
    DWORD   dwProcid;
    DWORD   dwZone;
    DWORD   dwAccess;
    PTHRD_INFO  pThrdInfo;
    TCHAR   szExeFile[MAX_PATH];
} PROC_DATA, *PPROC_DATA;

PPROC_DATA  v_pProcData;

typedef struct _PAGE_USAGE_INFO {
    DWORD cpROMCode;        // # of of XIP code pages
    DWORD cpRAMCode;        // # of RAM code pages
    DWORD cpROData;         // # of R/O data pages
    DWORD cpRWData;         // # of R/W data pages
    DWORD cpStack;          // # of stack pages
    DWORD cpReserved;       // # of reserved pages
} PAGE_USAGE_INFO, *PPAGE_USAGE_INFO;

UINT cNumProc;

#define MAX_MODULES 200

typedef struct _MOD_DATA {
    struct _MOD_DATA *pNext;
    DWORD   dwModid;
    DWORD   dwZone;
    DWORD   dwProcUsage;
    DWORD   dwBaseAddr;
    DWORD   dwFlags;
    TCHAR   szModule[MAX_PATH];
} MOD_DATA, *PMOD_DATA;

PMOD_DATA v_pModData;

UINT cNumMod;

BOOL    v_fUseConsole;
BOOL    v_fUseDebugOut;
BOOL    v_fBeenWarned;
BOOL    v_fTimestamp;

ROMRAMINFO v_physInfo;

BOOL IntArg(PTSTR *ppCmdLine, int *num);
BOOL DwordArg(PTSTR *ppCmdLine, DWORD *dw);
BOOL UlongArg(PTSTR *ppCmdLine, ULONG *ul);
BOOL FileNameArg(PTSTR *ppCmdLine, PTSTR pszFileName, UINT cchFileName);

#define _O_RDONLY       0x0000  /* open for reading only */
#define _O_WRONLY       0x0001  /* open for writing only */
#define _O_RDWR         0x0002  /* open for reading and writing */
#define _O_APPEND       0x0008  /* writes done at eof */

#define _O_CREAT        0x0100  /* create and open file */
#define _O_TRUNC        0x0200  /* open and truncate */
#define _O_EXCL         0x0400  /* open only if file doesn't already exist */

BOOL IsPfnInRanges (const PFNRANGE *ppfr, DWORD cRanges, DWORD dwPfn)
{
    for ( ; cRanges; ppfr ++, cRanges --) {
        if (dwPfn - ppfr->pfnBase < ppfr->pfnLength) {
            break;
        }
    }
    return cRanges;
}

FORCEINLINE BOOL IsPfnInROM (DWORD dwPfn)
{
    return IsPfnInRanges (&v_physInfo.roms[0], v_physInfo.cRoms, dwPfn);
}

FORCEINLINE BOOL IsPfnInRAM (DWORD dwPfn)
{
    return IsPfnInRanges (&v_physInfo.rams[0], v_physInfo.cRams, dwPfn);
}

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

//------------------------------------------------------------------------------
// Helper Functions for ropen
//------------------------------------------------------------------------------

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

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int
UU_ropen(
    LPCWSTR name,
    int mode
    )
{

    WCHAR  wzBuf[MAX_PATH*2];

    _tcscpy (wzBuf, TEXT("\\Release\\"));
    _tcsncat (wzBuf, name, ARRAYSIZE (wzBuf) - _tcslen(wzBuf) - 1);

    return (int) CreateFileW(wzBuf, ConvertAccessMode(mode), ConvertShareMode(mode),
                             NULL, ConvertCreateMode(mode), 0, NULL );

}

int
UU_rclose(
    int fd
    )
{
    return CloseHandle ((HANDLE)fd);
}


int
UU_rread(
    int fd,
    void *buf,
    int cnt
    )
{
    DWORD dwNumBytesRead;
    if (!ReadFile( (HANDLE)fd, buf, cnt, &dwNumBytesRead, NULL)) {
            dwNumBytesRead = 0;
    }
    return (int)dwNumBytesRead;
}

int
UU_rwrite(
    int fd,
    const void *buf,
    int cnt
    )
{

    DWORD dwNumBytesWritten;
    if (!WriteFile( (HANDLE)fd, buf, cnt, &dwNumBytesWritten, NULL)) {
        dwNumBytesWritten = 0;
    }
    return (int)dwNumBytesWritten;
}

// NOTE: This function can only be called from KMode
BOOL IsPfnInRom (DWORD pfn)
{
#if 0
    ROMChain_t *pROM;
    DWORD pfnROMBase, pfnROMEnd;

    for (pROM = pROMChain; pROM; pROM = pROM->pNext) {
        pfnROMBase = GetPFN(pROM->pTOC->physfirst);
        pfnROMEnd  = GetPFN(pROM->pTOC->physlast);

        if ((pfn >= pfnROMBase) && (pfn < pfnROMEnd)) {
            return TRUE;
        }
    }
#endif
    return FALSE;
}

void FmtPuts (const TCHAR *pFmt, ...)
{
    va_list ArgList;
    TCHAR   Buffer[1024];

    va_start (ArgList, pFmt);
    wvsprintf (Buffer, pFmt, ArgList);
    Puts (Buffer);
}


// If fAskUser=TRUE, will ask the user whether CeLog should be loaded.
// If fAskUser=FALSE, will just load CeLog without asking.
// Returns TRUE if CeLog was loaded and FALSE otherwise.
static BOOL LoadCeLog(BOOL fAskUser)
{
    HANDLE hLib;
    TCHAR  Response[MAX_PATH];
    int    i;

    if (fAskUser) {
        Puts (TEXT("\r\nCeLog has not been loaded yet, load it now?  (y or n at prompt below)\r\nWindows CE>"));
        if (!Gets (Response, MAX_PATH)) {
            return FALSE;
        }
        for (i = 0; IsWhiteSpace(Response[i]); i++)
            ;
        if ((TEXT('Y') != Response[i]) && (TEXT('y') != Response[i])) {
            // If not Y then don't do anything
            Puts (TEXT("CeLog is not loaded, command aborted.\r\n\r\n"));
            return FALSE;
        }
    }

    hLib = LoadKernelLibrary(TEXT("CeLog.dll"));  // Handle will never be closed
    if (hLib == NULL) {
        Puts(TEXT("CeLog could not be loaded.  Make sure CeLog.dll is present.\r\n\r\n"));
        return FALSE;
    }

    Puts(TEXT("CeLog is now loaded.\r\n*** You should probably start a flush application like CeLogFlush.exe.\r\n\r\n"));

    return TRUE;
}

int
CompareFilenames (const TCHAR *pFilename1, const TCHAR *pFilename2)
{
    TCHAR szFilename1 [MAX_PATH];
    TCHAR szFilename2 [MAX_PATH];
    PTCHAR p1;
    PTCHAR p2;

    // Find the filename portion.
    p1 = _tcsrchr (pFilename1, _T('\\'));
    p2 = _tcsrchr (pFilename2, _T('\\'));

    // Make a local copy.
    StringCchCopy (szFilename1, MAX_PATH, p1 ? p1 + 1 : pFilename1);
    StringCchCopy (szFilename2, MAX_PATH, p2 ? p2 + 1 : pFilename2);

    // Ignore the extension (or, lack of).
    p1 = _tcsrchr (szFilename1, _T('.'));
    p2 = _tcsrchr (szFilename2, _T('.'));

    if (p1) {
        *p1 = _T('\0');
    }
    if (p2) {
        *p2 = _T('\0');
    }

    return _tcsicmp (szFilename1, szFilename2);
}

void
LoadExt (LPCTSTR szName, LPCTSTR szDLLName)
{
    HMODULE             hMod;
    PSHELL_EXTENSION    pShellExt;
    PFN_ParseCommand    pfnParseCommand;

    // First, make sure this extension is not already loaded.
    for (pShellExt = v_ShellExtensions; NULL != pShellExt; pShellExt = pShellExt->pNext) {
        if (!CompareFilenames (pShellExt->szDLLName, szDLLName))
        {
            // This extension is already loaded.
            return;
        }
    }

    // Ok, in theory this is a good set of data.
    hMod = LoadLibrary (szDLLName);
    if (NULL != hMod) {
        pfnParseCommand = (PFN_ParseCommand)GetProcAddress(hMod,
            SHELL_EXTENSION_FUNCTION);
        if(pfnParseCommand != NULL) {
            // Ok, have all of the data.  Let's burn some memory
            pShellExt = (PSHELL_EXTENSION)LocalAlloc (LPTR, sizeof(SHELL_EXTENSION));
            if (pShellExt) {
                pShellExt->szName = LocalAlloc (LMEM_FIXED,
                                                sizeof(TCHAR)*(_tcslen(szName)+1));
                pShellExt->szDLLName = LocalAlloc (LMEM_FIXED,
                    sizeof(TCHAR)*(_tcslen(szDLLName)+1));
                if (pShellExt->szName && pShellExt->szDLLName) {
                    _tcscpy (pShellExt->szName, szName);
                    _tcscpy (pShellExt->szDLLName, szDLLName);
                    pShellExt->hModule = hMod;
                    pShellExt->pfnParseCommand = pfnParseCommand;
                    pShellExt->pNext = v_ShellExtensions;
                    v_ShellExtensions = pShellExt;
                } else {
                    if (pShellExt->szName) {
                        LocalFree (pShellExt->szName);
                    }
                    if (pShellExt->szDLLName) {
                        LocalFree (pShellExt->szDLLName);
                    }
                    LocalFree (pShellExt);
                    FreeLibrary (hMod);
                }
            } else {
                FmtPuts (TEXT("Unable to allocate extension block\r\n"));
                FreeLibrary (hMod);
            }
        } else {
            FmtPuts (TEXT("Shell: Error unable to find %s in %s\r\n"),
                           SHELL_EXTENSION_FUNCTION, szDLLName);
            FreeLibrary (hMod);
        }
    } else {
        FmtPuts (TEXT("Shell: Error unable to load %s : Error %d\r\n"), szDLLName, GetLastError());
    }
}

void
LoadExtensions (void)
{
    HKEY hKey;
    DWORD   dwValNameMaxLen, dwValMaxLen;

    // Any Shell Extensions?
    if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, TSH_EXT_REG, 0, 0, &hKey))
    {
        if (ERROR_SUCCESS == RegQueryInfoKey (hKey, NULL, NULL, NULL, NULL, NULL,
                                                NULL, NULL, &dwValNameMaxLen, &dwValMaxLen, NULL, NULL))
        {
            LPTSTR  szVal, szData;
            DWORD   dwIndex, dwValNameLen, dwValLen, dwType;
            DWORD   dwValNameMaxSizeBytes;
            DWORD   dwValNameMaxLenPlusNull;
            HRESULT hr;

            hr = DWordAdd(dwValNameMaxLen, 1, &dwValNameMaxLenPlusNull);
            if (SUCCEEDED(hr))
            {
                hr = DWordMult(dwValNameMaxLenPlusNull, sizeof(TCHAR), &dwValNameMaxSizeBytes);
                if (SUCCEEDED(hr))
                {
                    // ValNameLen is in characters.
                    szVal = (LPTSTR)LocalAlloc (LMEM_FIXED, dwValNameMaxSizeBytes);
                    // ValLen is in bytes
                    szData = (LPTSTR)LocalAlloc (LMEM_FIXED, dwValMaxLen);

                    dwIndex = 0;

                    dwValNameLen = dwValNameMaxLenPlusNull;
                    dwValLen = dwValMaxLen;
                    while (ERROR_SUCCESS == RegEnumValue (hKey, dwIndex, szVal, &dwValNameLen, NULL,
                                                          &dwType, (LPBYTE)szData, &dwValLen)) {

                        if (REG_SZ == dwType) {
                            LoadExt (szVal, szData);
                        } else {
                            RETAILMSG (1, (TEXT("Shell: Value %s: Unexpected registry type %d\r\n"),
                                           szVal, dwType));
                        }

                        dwIndex++;
                        dwValNameLen = dwValNameMaxLenPlusNull;
                        dwValLen = dwValMaxLen;
                    }
                }
            }
        }
        RegCloseKey (hKey);
    }
}

int WINAPI
WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR pCmdLine, int nShowCmd) {
    BOOL    fOK;
    const TCHAR *szPrompt = TEXT("Windows CE>");
    TCHAR   *cmdline = v_CmdLine;
    HKEY hKey;
    HANDLE  hEvent;
    DWORD dwSignalStartedID = 0;

    DEBUGREGISTER(0);

    // get the range for object store
    CeFsIoControl (NULL, FSCTL_GET_OBJECT_STORE_RANGE, NULL, 0, &v_ObjstoreRange, sizeof (v_ObjstoreRange), NULL, NULL);

    // Check for arg "-c" at start of command line
    while ((NULL != pCmdLine) && (TEXT('\0') != *pCmdLine)) {
        // Skip whitespace
        while (_istspace(*pCmdLine)) {
            pCmdLine++;
        }
        //
        if (!_tcsncmp(pCmdLine, TEXT("-c"), 2)) {
            v_fUseConsole = TRUE;
            pCmdLine += 2;
        } else if (!_tcsncmp(pCmdLine, TEXT("-d"), 2)) {
            v_fUseConsole = TRUE;
            v_fUseDebugOut = TRUE;
            pCmdLine += 2;
        } else if (!_tcsncmp(pCmdLine, TEXT("-r"), 2)) {
            pCmdLine += 2;
            // Skip trailing spaces
            while (_istspace(*pCmdLine)) {
                pCmdLine++;
            }
            break;
        } else if (_istdigit(*pCmdLine)) {
            // Probably spawned from the init process, silently pass over the numeric parameter
            dwSignalStartedID =_wtol(pCmdLine);
            while (_istdigit(*pCmdLine)) {
                pCmdLine++;
            }
        } else {
            // Unknown/unused argument
            RETAILMSG (1, (TEXT("SHELL.EXE: Unrecognized parameter '%s', will attempt to execute\r\n"), pCmdLine));
            // Skip out of loop, later passed down into doCommand
            break;
        }
        // Skip trailing spaces
        while ((NULL != pCmdLine) && _istspace(*pCmdLine)) {
            pCmdLine++;
        }
    }

    // If we were launched by init process, signal that our initialization is complete.
    if (dwSignalStartedID) {
        SignalStarted (dwSignalStartedID);
    }

    _crtinit();
    if (!v_fUseConsole) {
        // Wait until RELFSD file system loads
        hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"ReleaseFSD");
        if (hEvent)
            WaitForSingleObject(hEvent, INFINITE);
        else {
            DEBUGMSG (1, (TEXT("Shell: ReleaseFSD has not been loaded.\r\n")));
            return FALSE;
        }
        PPSHRestart();
    }

    rgbInfo = VirtualAlloc(0,INFOSIZE,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if (rgbInfo == NULL) {
        Puts(L"\r\n\r\nWindows CE Shell - unable to initialize!\r\n");
        return TRUE;
    }

    // initialize phsymem info
    VERIFY (KernelLibIoControl ((HANDLE) KMOD_CORE, IOCTL_KLIB_GETROMRAMINFO,
            NULL, 0, &v_physInfo, sizeof (v_physInfo), NULL));
    
    Puts(L"\r\n\r\nWelcome to the Windows CE Shell. Type ? for help.\r\n");

    // Set Main Thread priority
    {
        DWORD dwMainThdPrio = 130;

        if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, TXTSHELL_REG, 0, 0, &hKey))
        {
            DWORD dwSize = sizeof (dwMainThdPrio);
            DWORD dwPrioTemp = 0;
            DWORD dwType;

            if (ERROR_SUCCESS == RegQueryValueEx (hKey,  MAINTHDPRIO_RGVAL, NULL, &dwType, (LPBYTE) &dwPrioTemp, &dwSize) &&
                (REG_DWORD == dwType))
            {
                dwMainThdPrio = dwPrioTemp;
            }
            RegCloseKey (hKey);
        }
        DEBUGMSG (ZONE_INFO, (TEXT("Shell: main thread prio set to %d\r\n"), dwMainThdPrio));
        CeSetThreadPriority (GetCurrentThread(), dwMainThdPrio);
    }

    // Load extensions.
    LoadExtensions ();

    // Did they pass in an argument?
    if ((NULL != pCmdLine) && (TEXT('\0') != *pCmdLine)) {
        DoCommand(pCmdLine);
        goto CleanUp;
    }

    // check if gwes support is enabled in the image
    g_hGwesEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, g_pwszGwesEventName);

    // fUseDebugOut only makes sense for cmdline variables.
    v_fUseDebugOut = FALSE;
    fOK = TRUE;

restart:
    if (!v_fUseConsole)
        PPSHRestart();

    for(;;) {
        Puts(szPrompt);
        if (Gets(cmdline, MAX_CMD_LINE)) {
            fOK = TRUE;
            if (!DoCommand(cmdline)) {
                // User must have typed exit
                fOK = FALSE;
                break;
            }
        } else {
            break;
        }
    }
    if (fOK) {
        fOK = FALSE;
        Sleep(1000);
        _crtinit();
        Puts(L"\r\n\r\nWelcome to the Windows CE Shell. Type ? for help.\r\n");
        goto restart;
    }
CleanUp:
    VirtualFree(rgbInfo,0,MEM_RELEASE);

    if (g_hGwesEvent)
        CloseHandle(g_hGwesEvent);

    return TRUE;
}

BOOL DoHelp (PTSTR cmdline) {
    PSHELL_EXTENSION    pShellExt;

    Puts(TEXT("\r\nEnter any of the following commands at the prompt:\r\n"));
    
    Puts(TEXT("break : Breaks into the debugger\r\n"));
    
    Puts(TEXT("s <procname> : Starts new process\r\n"));
    
    Puts(TEXT("gi [\"proc\",\"thrd\",\"mod\",\"all\"]* [\"<pattern>\"] : Get Information\r\n"));
    Puts(TEXT("    proc   -> Lists all processes in the system\r\n"));
    Puts(TEXT("    thrd   -> Lists all processes with their threads\r\n"));
    Puts(TEXT("    delta  -> Lists only threads that have changes in CPU times\r\n"));
    Puts(TEXT("    mod    -> Lists all modules loaded\r\n"));
    Puts(TEXT("    moduse -> Lists module usage data\r\n"));
    Puts(TEXT("    modloaded -> List the processees that have a partular module loaded\r\n"));
    Puts(TEXT("    system -> Lists processor architecture and OS version\r\n"));
    Puts(TEXT("    all    -> Lists all of the above\r\n"));
    
    Puts(TEXT("mi [\"kernel\",\"full\"] : Memory information\r\n"));
    Puts(TEXT("    kernel-> Lists kernel memory detail\r\n"));
    Puts(TEXT("    full  -> Lists full memory maps\r\n"));

    Puts(TEXT("zo [\"p\",\"m\",\"h\"] [index,name,handle] [<zone>,[[\"on\",\"off\"][<zoneindex>]*]*: Zone Ops\r\n"));
    Puts(TEXT("    p/m/h -> Selects whether you operate on a process, module, or PID/pModule\r\n"));
    Puts(TEXT("    index -> Put the index of the proc/module, as printed by the gi command\r\n"));
    Puts(TEXT("    name  -> Put the name of the proc/module, as printed by the gi command\r\n"));
    Puts(TEXT("    handle-> Put the value of the PID/pModule, as printed by the gi command\r\n"));
    Puts(TEXT("          -> If no args are specified, prints cur setting with the zone names\r\n"));
    Puts(TEXT("    zone  -> Must be a number to which this proc/modl zones are to be set \r\n"));
    Puts(TEXT("             Prefix by 0x if this is specified in hex\r\n"));
    Puts(TEXT("    on/off-> Specify bits to be turned on/off relative to the current zonemask\r\n"));
    Puts(TEXT("  eg. zo p 2: Prints zone names for the proc at index 2 in the proc list \r\n"));
    Puts(TEXT("  eg. zo h 8fe773a2: Prints zone names for the proc/module with PID/pModule of 8fe773a2\r\n"));
    Puts(TEXT("  eg. zo m 0 0x100: Sets zone for module at index 0 to 0x100 \r\n"));
    Puts(TEXT("  eg. zo p 3 on 3 5 off 2: Sets bits 3&5 on, and bit 2 off for the proc[3]\r\n"));
    
    Puts(TEXT("win : Dumps the list of windows\r\n"));
    
    Puts(TEXT("log <CE zone> [user zone] [process zone] : Change CeLog zone settings\r\n"));
    
    Puts(TEXT("prof <on|off> [data_type] [storage_type] : Start or stop kernel profiler\r\n"));
    Puts(TEXT("acct [optional: <acctid>] : Display info about a speific account or all accounts in system)\r\n"));    
    Puts(TEXT("memtrack (deprecated; use Application Verifier instead)\r\n"));

#if 0    
    OldMode = SetKMode(TRUE);

    if (((ROMHDR *)UserKInfo[KINX_PTOC])->ulTrackingLen) {
        Puts(TEXT("rd [i] [c] [v[xxxx]] [t<typeid>] [p<procid>] [h<handle>]: Dump resources\r\n"));
        Puts(TEXT("    i     -> Does an incremental dump from the last checkpoint\r\n"));
        Puts(TEXT("    c     -> Checkpoints the current state of resources\r\n"));
        Puts(TEXT("    v     -> Sets verbosity level. Default is 0\r\n"));
        Puts(TEXT("          -> xxxx is a four digit hex number.\r\n"));
        Puts(TEXT("             its meaning is determined by\r\n"));
        Puts(TEXT("             the tracking callback that was\r\n"));
        Puts(TEXT("             registered with the resource type.\r\n"));
        Puts(TEXT("    s     -> Causes stack traces to be printed in the form:\r\n"));
        Puts(TEXT("             <module name>!(base address)\r\n"));
        Puts(TEXT("             where base address + rva is equal to the number\r\n"));
        Puts(TEXT("             in the right hand column of the map file for the module.\r\n"));
        Puts(TEXT("    t     -> Only prints resources of the type <typeid>\r\n"));
        Puts(TEXT("    p     -> Only prints resources for process <procid>\r\n"));
        Puts(TEXT("    h     -> Only prints resources with handle <handle> \r\n"));
        Puts(TEXT("rf [d<0,1>] [<+,-> t<typeid>] [<+,-> p<procid>]: Filter tracked resources\r\n"));
        Puts(TEXT("    d     -> Changes the default state for all resource types.\r\n"));
        Puts(TEXT("          -> 0 : Causes no resource types to be tracked\r\n"));
        Puts(TEXT("          -> 1 : Causes all resource types to be tracked\r\n"));
        Puts(TEXT("    t     -> + : Track resources of the type <typeid> \r\n"));
        Puts(TEXT("          -> - : Don't track resources of the type <typeid> \r\n"));
        Puts(TEXT("    p     -> + : Track resources for process <procid>\r\n"));
        Puts(TEXT("          -> - : Dont filter on any process (undo the effect of a +) \r\n"));
    }
    SetKMode(OldMode);
#endif

    DoKillProcHelp();

    Puts(TEXT("dd <process id> <addr> [<#dwords>]: dumps dwords for specified process id\r\n"));
    Puts(TEXT("df <filename> <process id> <addr> <size>: dumps address of a process to file\r\n"));
    Puts(TEXT("hd <exe>: dumps the process heap\r\n"));
    Puts(TEXT("run <filename>: run file in batch mode\r\n"));
    Puts(TEXT("dis: Discard all discardable memory\r\n"));
    Puts(TEXT("options: Set options\r\n"));
    Puts(TEXT("     timestamp     -> Toggle timestamp on all cmds (useful for logs)\r\n"));
    Puts(TEXT("     priority [N]  -> Change the priority of the shell thread\r\n"));
    Puts(TEXT("     kernfault     -> Fault even if NOFAULT specified\r\n"));
    Puts(TEXT("     kernnofault   -> Re-enable default NOFAULT handling\r\n"));
    Puts(TEXT("suspend: Suspend the device.  Requires GWES.exe and proper OAL support\r\n"));
    
    DoLoadExtHelp();

    Puts(TEXT("tp <tid> [prio] : Sets/queries thread priority\r\n"));
    Puts(TEXT("    tid  : can be either a thread id, 'kitlintr' or 'kitltimer'\r\n"));
    Puts(TEXT("    prio : thread priority\r\n"));
    Puts(TEXT("           -1 or omitted : query current thread priority\r\n"));
    Puts(TEXT("           0 - 255       : set thread priority\r\n"));
    Puts(TEXT("dev : Display information about installed devices\r\n"));
    Puts(TEXT("\r\n"));

    // Refresh the loaded extensions.
    LoadExtensions ();

    for (pShellExt = v_ShellExtensions; NULL != pShellExt; pShellExt = pShellExt->pNext) {
        FmtPuts (TEXT("Extension: %s\r\n"), pShellExt->szDLLName);
        __try {
            pShellExt->pfnParseCommand (TEXT("?"), TEXT(""), FmtPuts, Gets);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            FmtPuts (TEXT("Exception calling handler %s\r\n"), pShellExt->szDLLName);
        }
    }

    return TRUE;
}

CMDS FindCommand(PTSTR *ppchCmdLine, PTSTR *ppchCmd) {
    int j;
    size_t i;
    TCHAR *token;
    // strip white space at the start
    while (IsWhiteSpace(**ppchCmdLine))
        (*ppchCmdLine)++;
    // strip white space at the end
    i = _tcslen(*ppchCmdLine);
    while (IsWhiteSpace(*(*ppchCmdLine+i-1))) {
        *(*ppchCmdLine+i-1) = TEXT('\0');
        i--;
    }
    *ppchCmd = token = _tcstok(*ppchCmdLine, TEXT(" "));
    if ((token == NULL) || (*token == '\0'))
        return CMDS_NO_COMMAND;
    // Check if we have anything left on the line
    if (i == _tcslen(token))
        *ppchCmdLine = NULL;
    else // Move ahead of the token & the NULL inserted by _tcstok
        *ppchCmdLine += _tcslen(token)+1;
    for (j=0; j<_countof(v_commands); j++) {
        DEBUGMSG(0,(TEXT("SHELL: Token=%s, Cmd=%s\r\n"), token, v_commands[j].szCommand));
        if (!_tcscmp(token, v_commands[j].szCommand))
            return (CMDS)j;
    }
    return CMDS_UNKNOWN;
}

BOOL DoCommand(PTSTR cmdline) {
    CMDS index;
    LPTSTR szCmd;
    PSHELL_EXTENSION    pShellExt;
    BOOL    fRet;
    BOOL fRetry;

    index = FindCommand(&cmdline, &szCmd);
    switch (index) {
        case CMDS_NO_COMMAND:
            break;
        case CMDS_UNKNOWN:
            // Check for extension
            fRetry = FALSE;
            retry:
            for (pShellExt = v_ShellExtensions; NULL != pShellExt; pShellExt = pShellExt->pNext) {
                __try {
                    fRet = pShellExt->pfnParseCommand (szCmd, cmdline, FmtPuts, Gets);
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    FmtPuts (TEXT("Exception calling handler %s\r\n"), pShellExt->szDLLName);
                    fRet = FALSE;
                }

                if (fRet) {
                    break;
                }
            }
            if (NULL == pShellExt) {
                if (!fRetry) {
                    // Refresh the loaded shell extensions, and try one more time.
                    LoadExtensions ();
                    fRetry = TRUE;
                    goto retry;
                }
                else {
                    Puts(TEXT("Unknown command.\r\n\r\n"));
                }
            }
            break;
        default:
            if (IsValidCommand(index))
                if (v_fTimestamp) {
                    TCHAR   szOutStr[MAX_PATH];
                    SYSTEMTIME  SystemTime;
                    GetLocalTime (&SystemTime);
                    StringCbPrintf (szOutStr, sizeof(szOutStr), TEXT("%02d/%02d/%02d %d:%02d:%02d.%03d\r\n"),
                              SystemTime.wMonth, SystemTime.wDay, SystemTime.wYear,
                              SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond, SystemTime.wMilliseconds);
                    Puts(szOutStr);
                }
                return (v_commands[index].lpfnCmdProc)(cmdline);
            break;
    }
    return TRUE;
}

BOOL DoExit (PTSTR cmdline) {
    return FALSE;
}

//
// Create a process.  The parameters are:
//  pszCmdLine - the command line to be parsed and executed (IN).
//  pszImageName - the name of the process to be executed as parsed out of
//              the command line (filled in by this routine).
//  cchImageName - number of characters in the pszImageName buffer (IN).
//  ppi - pointer to a PROCESS_INFORMATION structure describing the created
//              process (filled in by this routine).
BOOL LaunchProcess(LPTSTR pszCmdLine, LPTSTR pszImageName, DWORD cchImageName, LPPROCESS_INFORMATION ppi)
{
    BOOL fOk = FALSE;
    LPCTSTR pszCommand = NULL;
    LPCTSTR pszCommandSuffix = _T(".exe");

    if (pszCmdLine == NULL || pszImageName == NULL || cchImageName == 0)
    {
        SetLastError (ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // skip leading whitespace
    while(*pszCmdLine && _istspace(*pszCmdLine)) {
        pszCmdLine++;
    }

    // is the program name quoted?
    if(*pszCmdLine == _T('"')) {
        // yes, look for the matching quote
        pszCmdLine++;
        pszCommand = pszCmdLine;
        while(*pszCmdLine && *pszCmdLine != _T('"')) {
            pszCmdLine++;
        }

        // did we find the matching quote?
        if(*pszCmdLine) {
            // yes, null terminate and skip it
            *pszCmdLine = 0;
            pszCmdLine++;

            // validly formatted program name
            fOk = TRUE;
        }
    } else {
        // no quote, look for the end of the first whitespace-separated token
        pszCommand = pszCmdLine;
        while(*pszCmdLine && !_istspace(*pszCmdLine)) {
            pszCmdLine++;
        }

        // did we get to the end of the string?
        if(*pszCmdLine) {
            *pszCmdLine = 0;
            pszCmdLine++;
        }

        // validly formatted program name
        fOk = TRUE;
    }

    // save the program name to pass back to the caller
    DEBUGCHK(pszCommand != NULL);
    if (S_OK != StringCchCopy(pszImageName, cchImageName, pszCommand)) {
        fOk = FALSE;
    }

    // did we get a good program name?
    if(fOk) {
        DWORD dwCommandLen, dwSuffixLen;
        int iStatus;

        // yes, skip leading whitespace on the command line
        while(*pszCmdLine && _istspace(*pszCmdLine)) {
            pszCmdLine++;
        }

        // determine whether we need to append a .exe to the command name
        dwCommandLen = _tcslen(pszImageName);
        dwSuffixLen = _tcslen(pszCommandSuffix);
        if(dwCommandLen < dwSuffixLen ||
           _tcsicmp(pszImageName + (dwCommandLen - dwSuffixLen), pszCommandSuffix) != 0)
        {
            iStatus = StringCchCat(pszImageName, cchImageName, pszCommandSuffix);
            if(iStatus != S_OK)
            { // string had to be truncated
                fOk = FALSE;
                SetLastError (ERROR_INVALID_PARAMETER);
            }
        }
    }

    // everything ok so far?
    if(fOk) {
        // yes, now invoke the program
        fOk = CreateProcessW (pszImageName, pszCmdLine ? pszCmdLine : TEXT(""),
            NULL, NULL, FALSE, 0, NULL, NULL, NULL, ppi);
    }

    return fOk;
}

BOOL DoStartProcess (PTSTR cmdline) {
    BOOL fOk;
    TCHAR szImageName[MAX_PATH];

    if (cmdline == NULL) {
        Puts(TEXT("Syntax: s <procname>\r\n\r\n"));
        return TRUE;
    }

    // check to make sure gwes is started if present; if not present, ignore
    if ((FALSE == v_fBeenWarned) && (g_hGwesEvent)) {
        TCHAR   Response[MAX_PATH];
        int     i;
        while (WAIT_OBJECT_0 != WaitForSingleObject(g_hGwesEvent, 0)) {
            Puts (TEXT("\r\nGWES has not been started yet, run anyway (y or n)?"));
            if (Gets (Response, MAX_PATH)) {
                for (i=0; IsWhiteSpace (Response[i]); i++) {
                    ;
                }
                RETAILMSG (1, (TEXT("Response='%c'\r\n"), Response[i]));
                if (('Y' == Response[i]) || ('y' == Response[i])) {
                    v_fBeenWarned = TRUE;
                    break;
                } else if (('N' == Response[i]) || ('n' == Response[i])) {
                    // Don't do it.
                    return TRUE;
                }
                // If not Y or N then loop around again.
                Puts (TEXT("Enter Y or N\r\n"));
            } else {
                return TRUE;
            }
        }
    }

    fOk = LaunchProcess(cmdline, szImageName, sizeof(szImageName) / sizeof(szImageName[0]), NULL);
    if (!fOk)  {
        FmtPuts (TEXT("Unable to create process '%s' : Error 0x%X\r\n"),
                 szImageName, GetLastError());
    }
    return TRUE;
}

VOID DoKillProcHelp()
{
    Puts(TEXT("kp [-r (automatically refresh process list)] <proc> [<proc2> <proc3> ...]: Kills process(es)\r\n"));
    Puts(TEXT("   r       : Automatically refreshes the process list.  No need to run \"gi proc\" if you do this.\r\n"));
    Puts(TEXT("   <proc>  : Can either be PROC or process name.  Use quotes for names with white spaces.\r\n"));
    Puts(TEXT("Note:  kp uses TerminateProcess to shut down the application.\r\n"));
}

VOID DoKillProcByID(DWORD dwProcID)
{
    DWORD lasterr;

    if (dwProcID)
    {
        HANDLE hProc = OpenProcess (PROCESS_ALL_ACCESS, FALSE, dwProcID);
        FmtPuts(TEXT("Attempting to kill process of id %08x ..."), dwProcID);

        if (hProc) {        
            if (TerminateProcess (hProc, 0))
            {
                Puts(TEXT("Succeeded\r\n\r\n"));
            }
            else
            {
                lasterr = GetLastError();
                FmtPuts(TEXT("Failed TerminateProcess(), lasterr=%d\r\n\r\n"), lasterr);
            }
            CloseHandle (hProc);
        } 
        else 
        {
            lasterr = GetLastError();
            FmtPuts(TEXT("Unable to open process %d, lasterr=%d\r\n\r\n"), dwProcID, lasterr);
        }
    }
}

BOOL DoKillProc(PTSTR cmdline)
{
    DWORD dwProcID = 0;
    DWORD i;
    PPROC_DATA pProcData;
    WCHAR szProcessName[MAX_PATH];
    
    BOOL PrintSyntax = TRUE;
    BOOL IsProcID = TRUE;
    BOOL fRefreshProcList = FALSE;

    if (cmdline && _tcslen(cmdline) > 3 && !_tcsncmp(cmdline, L"-r ", 3))
    {
        fRefreshProcList = TRUE;
        cmdline += 3;
    }

    if (0 == cNumProc || TRUE == fRefreshProcList)
    {
        WCHAR szProc [] = L"proc";
        DoGetInfo(szProc); // Initialize the process list
    }

    // Terminate all the processes specified on the command line
    while (cmdline)
    {
        IsProcID = DwordArg(&cmdline, &dwProcID); // First treat the argument as an ID and see if we come back with something
        if (!IsProcID)
        { // User did not specify ID; treat as process name before bailing out
            dwProcID = 0;
            if (FileNameArg(&cmdline, szProcessName, ARRAYSIZE(szProcessName)))
            { // It seems like a valid name; check whether the process is running
                for (pProcData = v_pProcData; pProcData != NULL; pProcData = pProcData->pNext) 
                {
                    if (!CompareFilenames(pProcData->szExeFile, szProcessName))
                    { // We found a running process with a matching name
                        dwProcID = pProcData->dwProcid;
                        DoKillProcByID(dwProcID);
                    }
                }

                if (!dwProcID)
                {
                    Puts(TEXT("Could not find specified process name in list of running processes.\r\n\r\n"));
                }

                PrintSyntax = FALSE;
            }
            else
            {
                break;
            }
        }
        else if (dwProcID < cNumProc)
        {   // Shortcut, you can tell us the number from the previous GI command and we'll look up the ProcID for you
            // Hopefully we'll never have a ProcID that's a low integer (or at least not less then the number of processes running).
            for (i = 0, pProcData = v_pProcData; (i < dwProcID) && (pProcData != NULL); i++,pProcData = pProcData->pNext) 
            {
            }
    
            if (NULL != pProcData) 
            {
                dwProcID = pProcData->dwProcid;
            }

            DoKillProcByID(dwProcID);
            PrintSyntax = FALSE;
        }
        else
        {
            Puts(TEXT("Unable to get to PID based on arguments\r\n\r\n"));
        }
    }

    if (PrintSyntax)
    {
        DoKillProcHelp();
    }

    return TRUE;
}

void DumpLine (DWORD dwAddr, const DWORD *pdw)
{
    StringCbPrintf(rgbText, ccbrgbText, TEXT("%08X: %08X %08X %08X %08X\r\n"),
        dwAddr, pdw[0], pdw[1], pdw[2], pdw[3]);
    Puts (rgbText);

}

BOOL DumpThisAddr (int fd, DWORD dwProcId, DWORD dwAddr, ULONG ulLen) {

    HANDLE hProc = OpenProcess (PROCESS_VM_READ, 0, dwProcId);
    BOOL   fRet = (NULL != hProc);

    if (hProc) {
        DWORD buf[256];     // read 256 dword at at time
        DWORD cbToRead, cbRead;
        const DWORD *pdw;
        
        while ((int) ulLen > 0) {
            cbToRead = ((ulLen > 1024)? 1024 : ulLen);
            if (!ReadProcessMemory (hProc, (LPVOID) dwAddr, buf, cbToRead, &cbRead)
                || (cbToRead != cbRead)) {
                Puts (TEXT("Unable to read process memory\r\n"));
                fRet = FALSE;
                break;
            }

            ulLen -= cbRead;

            if (-1 == fd) {
                for (pdw = buf; (int) cbRead > 0; pdw += 4, dwAddr += 16) {
                    DumpLine (dwAddr, pdw);
                    cbRead -= 16;
                }
            } else if ((DWORD) UU_rwrite (fd, (LPBYTE)buf, cbRead) != cbRead) {
                Puts (TEXT("Error Writing file\r\n"));
                fRet = FALSE;
                break;
            }
        }
        CloseHandle (hProc);
    }
    return fRet;
}

BOOL DoDumpDwords (PTSTR cmdline) {
    ULONG ulAddr;
    ULONG ulLen = 16;
    DWORD dwProcId;

    if (cmdline == NULL || !DwordArg(&cmdline, &dwProcId)) {
        Puts(TEXT("Syntax: dd <process id> <addr> [<#dwords>]\r\n"));
        return TRUE;
    }

    if (!UlongArg(&cmdline, &ulAddr)) {
        Puts(TEXT("Syntax: dd <process id> <addr> [<#dwords>]\r\n"));
        return TRUE;
    }

    if (!UlongArg(&cmdline, &ulLen) && *cmdline) {
        Puts(TEXT("Syntax: dd <process id> <addr> [<#dwords>]\r\n"));
        return TRUE;
    }

    if (!ulLen || (ulLen > 1024*1024)) {
        Puts(TEXT("dd: Invalid Length\r\n"));
        return TRUE;
    }

    DumpThisAddr(-1, dwProcId, ulAddr & ~3, ulLen * sizeof (DWORD));

    return TRUE;
}

BOOL DoDumpFile (PTSTR cmdline) {
    TCHAR szImageName[MAX_PATH];
    TCHAR szOutStr[MAX_PATH];
    LPCTSTR token, eos;
    ULONG ulAddress=0, ulLen=0;
    int fd;
    DWORD dwProcId = 0;

    if (cmdline == NULL) {
        Puts(TEXT("Syntax: df <filename> <process id> <addr> <size>\r\n"));
        return TRUE;
    }
    szImageName[0]='\0';
    eos = cmdline + _tcslen(cmdline);
    token = _tcstok(cmdline, TEXT(" \t"));
    if(token != NULL) {
        StringCbCopy(szImageName, sizeof(szImageName), token);

        // get process id
        token = _tcstok(NULL, TEXT(" \t"));
        if(token == NULL) {
            Puts(TEXT("Syntax: df <filename> <process id> <addr> <size>\r\n"));
            return TRUE;
        }
        dwProcId = wcstoul(token,0,16);

        // get address
        token = _tcstok(NULL, TEXT(" \t"));
        if(token == NULL) {
            Puts(TEXT("Syntax: df <filename> <process id> <addr> <size>\r\n"));
            return TRUE;
        }
        ulAddress = wcstoul(token,0,16);

        // get size
        token = _tcstok(NULL, TEXT(" \t"));
        if(token == NULL) {
            Puts(TEXT("Syntax: df <filename> <process id> <addr> <size>\r\n"));
            return TRUE;
        }
        ulLen= wcstoul(token,0,16);

    }
    
    if (szImageName[0] && dwProcId && ulAddress && ulLen) {
        
        if (-1 == (fd = UU_ropen (szImageName, 0x80001))) {
            StringCbPrintf (szOutStr, sizeof(szOutStr), L"Unable to open input file %s\r\n", szImageName);
            Puts (szOutStr);
        } else {

            if (DumpThisAddr (fd, dwProcId, ulAddress, ulLen)) {
                Puts (L"Write complete.\r\n");
            } else {
                Puts (L"Error reading image physical start from file.\r\n" );
            }

            UU_rclose(fd);
        }
        
    } else {
        Puts(L"Error: invalid parameters.\r\n");
    }
    return TRUE;
}


#define SHELL_GETMOD  0x0001
#define SHELL_GETPROC 0x0002
#define SHELL_GETTHRD 0x0004
#define SHELL_GETSYSTEM 0x0008
#define SHELL_GETREFCNT 0x0010

#define NUM_THRDSTATES 8
const TCHAR rgszStates[NUM_THRDSTATES][16] = {
    TEXT("Runing "), // 0
    TEXT("Runabl "), // 1
    TEXT("Blockd "), // 2
    TEXT("Suspnd "), // 3
    TEXT("Sleepg "), // 4
    TEXT("Sl/Blk "), // 5
    TEXT("Sl/Spd "), // 6
    TEXT("S/S/Bk "), // 7
};

/******************************************************************************
GetObjectPtr - copied from NK to follow thread chain

******************************************************************************/
PVOID GetObjectPtr(HANDLE h)
{
#if 0
    PHDATA phd;

    if (h) {
        phd = (PHDATA)(((ulong)h & HANDLE_ADDRESS_MASK) +
            ((struct KDataStruct *)(UserKInfo[KINX_KDATA_ADDR]))->handleBase);
        return phd->pvObj;
    }
#endif
    return 0;
}

#define CH_DQUOTE       TEXT('"')

//***   IsPatMatch -- does text match the pattern arg?
// NOTES
//  no REs for now (ever?), so the Is'Pat'Match is a slight misnomer
BOOL IsPatMatch(LPCTSTR pszPat, LPCTSTR pszText)
{
    BOOL fRet;
    TCHAR szText[MAX_PATH];

    if (!pszPat)
        return TRUE;

    _tcsncpy(szText, pszText, ARRAYSIZE(szText) - 1);
    szText[ARRAYSIZE(szText) - 1] = 0;
    _tcslwr(szText);
    fRet = _tcsstr(szText, pszPat) != NULL;

    return fRet;
}


//
// Check whether a component name matches a file name.
// i.e. pszName=="dhcp" will match pszText=="dhcp.dll", but not "dhcpv6l.dll"
//
BOOL
IsNameMatch (LPCTSTR pszName, LPCTSTR pszText)
{
    TCHAR szText[MAX_PATH];
    TCHAR szName[MAX_PATH];
    DWORD NameLen;
    DWORD TextLen;
    DWORD i;

    if ((NULL == pszName) || (NULL == pszText)) {
        return FALSE;
    }

    NameLen = _tcslen(pszName);
    TextLen = _tcslen(pszText);

    if (TextLen >= MAX_PATH) {
        return FALSE;
    }

    //
    // pszName can't be a substring of pszText if it is longer.
    //
    if (NameLen > TextLen) {
        return FALSE;
    }

    //
    // Canonicalize pszText into szText
    //
    _tcsncpy(szText, pszText, MAX_PATH-1);
    szText[MAX_PATH-1] = 0;
    _tcslwr(szText);

    //
    // Canonicalize pszName into szName
    //
    _tcsncpy(szName, pszName, MAX_PATH-1);
    szName[MAX_PATH-1] = 0;
    _tcslwr(szName);


    //
    // Find szName
    //
    for (i = 0; i < NameLen; i++) {
        if (szName[i] != szText[i]) {
            return FALSE;
        }
    }

    return !szText[NameLen] || ((TCHAR)'.' == szText[NameLen]);
}


//
// Return the MODULEENTRY32 info for a specified module id
//
BOOL
GetModuleInfo(
    HANDLE hSnap,
    MODULEENTRY32 * mod,
    DWORD dwModid
    )
{
    mod->dwSize = sizeof(*mod);
    if (Module32First(hSnap,mod)) {
        do {
            if (dwModid == mod->th32ModuleID) {
                return TRUE;
            }
        } while (Module32Next(hSnap,mod));
    }
    return FALSE;
}

int IsModuleLoadedInProcess (LPPROCESSENTRY32 lppe, LPCTSTR pszModName)
{
    HANDLE hSnap = CreateToolhelp32Snapshot (TH32CS_SNAPMODULE, lppe->th32ProcessID);
    MODULEENTRY32 me32;
    int found = 0;
    me32.dwSize = sizeof(me32);
    if (Module32First (hSnap, &me32)) {
        do {
            if (IsNameMatch (pszModName, me32.szModule)) {
                StringCbPrintf(rgbText, ccbrgbText, L"    ID:0x%8.8lx, '%s'\r\n", lppe->th32ProcessID, lppe->szExeFile);
                Puts (rgbText);
                found = 1;
                break;
            }
        } while (Module32Next (hSnap, &me32));
    }
    CloseToolhelp32Snapshot (hSnap);

    return found;
}

void DoModLoaded (LPCTSTR pszModName)
{
    if (!pszModName) {
        Puts(TEXT("Syntax: gi modloaded <DllName>\r\n\r\n"));
    } else {
        HANDLE hSnap = CreateToolhelp32Snapshot (TH32CS_SNAPPROCESS | TH32CS_SNAPNOHEAPS, 0);
        PROCESSENTRY32 pe;
        int nProcs = 0;
        StringCbPrintf(rgbText, ccbrgbText, L"Module '%s' is loaded in the following processes:\r\n", pszModName);
        Puts (rgbText);
        pe.dwSize = sizeof(pe);
        if (Process32First (hSnap, &pe)) {
            do {
                nProcs += IsModuleLoadedInProcess (&pe, pszModName);
            } while (Process32Next (hSnap, &pe));
        }

        CloseToolhelp32Snapshot (hSnap);

        if (!nProcs) {
            Puts (TEXT("    <None>\r\n"));
        }
    }
}

void SortModuleList(HANDLE hSnap)
{
    MODULEENTRY32 me32;
    PMOD_DATA   pModData, pLastModData;

    // Free the old data.
    for (pModData = v_pModData; NULL != pModData; pModData = pLastModData) {
        pLastModData = pModData->pNext;
        LocalFree (pModData);
    }
    v_pModData = NULL;
    
    me32.dwSize = sizeof(me32);
    cNumMod = 0;

    
    // Get all entries into the list
    if (Module32First(hSnap, &me32))
    {
        do
        {
            pModData = LocalAlloc (LPTR, sizeof(MOD_DATA));
            if (pModData) {
                pModData->dwModid = me32.th32ModuleID;
                pModData->dwZone = me32.dwFlags;
                pModData->dwProcUsage = me32.ProccntUsage;
                pModData->dwBaseAddr = (DWORD)(me32.modBaseAddr);
                pModData->dwFlags = me32.dwFlags;
                memcpy (pModData->szModule, me32.szModule, sizeof(pModData->szModule));

                // Insert alphabetically
                if (NULL == v_pModData) {
                    v_pModData = pModData;
                } else {
                    if (wcscmp(pModData->szModule, v_pModData->szModule) < 0) {
                        pModData->pNext = v_pModData;
                        v_pModData = pModData;
                    } else {
                        for (pLastModData = v_pModData; NULL != pLastModData; pLastModData = pLastModData->pNext) {
                            if ((NULL == pLastModData->pNext) || (wcscmp(pModData->szModule, pLastModData->pNext->szModule) < 0)) {
                                pModData->pNext = pLastModData->pNext;
                                pLastModData->pNext = pModData;
                                break;
                            }
                        }
                    }
                }
            }
            cNumMod++;
        }
        while(Module32Next(hSnap, &me32));
    }
}
BOOL DoGetInfo (PTSTR cmdline) {
    // dwMask represents the information gi is going to print. dwTHFlags
    // represents the information gi needs to request from toolhelp. These
    // aren't the same thing!
    DWORD dwProcessId = 0;
    DWORD   dwMask = 0xFFFFFFFF;
    DWORD   dwTHFlags = TH32CS_SNAPPROCESS | TH32CS_SNAPNOHEAPS | TH32CS_SNAPTHREAD | TH32CS_SNAPMODULE | TH32CS_GETALLMODS;
    PTSTR   szToken;
    HANDLE  hSnap = INVALID_HANDLE_VALUE;
    PROCESSENTRY32 proc;
    THREADENTRY32 thread;
    TCHAR   *pszArg = NULL;
    BOOL    fMatch;
    BOOL    fDelta = FALSE;
    UINT    i;
    int len = 0;
    PPROC_DATA pProcData, pNextProcData, pSavedProcList, pOldProcData;
    PTHRD_INFO  pThrdInfo, pThrdInfoLast;
    PMOD_DATA pModData;

    // Find out if he wants to mask stuff
    if (cmdline) {
        dwMask = 0;
        dwTHFlags = 0;
        while ((szToken = _tcstok(cmdline, TEXT(" \t"))) != NULL) {
            cmdline = NULL; // to make subsequent _tcstok's work
            if (!_tcscmp(szToken, TEXT("proc"))) {
                dwMask |= SHELL_GETPROC;
                dwTHFlags |= TH32CS_SNAPPROCESS | TH32CS_SNAPNOHEAPS;
            } else if (!_tcscmp(szToken, TEXT("thrd"))) {
                dwMask |= SHELL_GETTHRD | SHELL_GETPROC;
                dwTHFlags |= TH32CS_SNAPPROCESS | TH32CS_SNAPNOHEAPS | TH32CS_SNAPTHREAD;
            } else if (!_tcscmp(szToken, TEXT("mod"))) {
                dwMask |= SHELL_GETMOD;
                dwTHFlags |= TH32CS_SNAPMODULE | TH32CS_GETALLMODS;
            } else if (!_tcscmp(szToken, TEXT("modloaded"))) {
                DoModLoaded (_tcstok(cmdline, TEXT(" \t")));
                return TRUE;
            } else if (!_tcscmp(szToken, TEXT("moduse"))) {
                dwMask |= SHELL_GETMOD;
                dwTHFlags |= TH32CS_SNAPMODULE;
                if ((szToken = _tcstok(cmdline, TEXT(" \t"))) != NULL) {
                    dwProcessId = _wtol (szToken); // next token is a process # listed from "gi proc" command
                    if (dwProcessId < cNumProc)
                    {
                        for (i = 0, pProcData = v_pProcData; (i < dwProcessId) && (pProcData != NULL); i++,pProcData = pProcData->pNext) {
                        }
                
                        if (NULL != pProcData) {
                            dwProcessId = pProcData->dwProcid;
                        } else {
                            Puts(TEXT("Invalid argument for proc #.  Use process index returned by gi proc command.\r\n\r\n"));
                            return TRUE;
                        }
                    } else {
                        Puts(TEXT("Invalid argument for proc #.  Use process index returned by gi proc command.\r\n\r\n"));
                        return TRUE;
                    }
                    
                } else {
                    dwProcessId = GetCurrentProcessId();
                }
            } else if (!_tcscmp(szToken, TEXT("delta"))) {
                dwMask |= SHELL_GETTHRD | SHELL_GETPROC;
                fDelta = TRUE;
                dwTHFlags |= TH32CS_SNAPPROCESS | TH32CS_SNAPNOHEAPS | TH32CS_SNAPTHREAD;
            } else if (!_tcscmp(szToken, TEXT("all"))) {
                dwMask = 0xFFFFFFFF;
                dwTHFlags |= TH32CS_SNAPPROCESS | TH32CS_SNAPNOHEAPS | TH32CS_SNAPTHREAD | TH32CS_SNAPMODULE | TH32CS_GETALLMODS;
            } else if (!_tcscmp(szToken, TEXT("silent"))) {
                dwMask |= SHELL_GETMOD | SHELL_GETPROC;
                dwTHFlags |= TH32CS_SNAPPROCESS | TH32CS_SNAPNOHEAPS | TH32CS_SNAPMODULE | TH32CS_GETALLMODS;
            } else if (szToken[0] == CH_DQUOTE && pszArg == NULL) {
                // remove CH_DQUOTEs
                pszArg = szToken + 1;
                if (_tcslen(pszArg) && (CH_DQUOTE == pszArg[_tcslen(pszArg) - 1])) {
                    pszArg[_tcslen(pszArg) - 1] = 0;    // remove trailing quote
                }
            }
            else if (!_tcscmp(szToken, TEXT("system")))
                dwMask |= SHELL_GETSYSTEM;
            else
                Puts(TEXT("Invalid argument.  Must be one of proc, thrd, delta, mod, moduse, system or all.\r\n\r\n"));
        }
    }

    if (dwTHFlags != 0)
    {
        hSnap = CreateToolhelp32Snapshot(dwTHFlags, dwProcessId);

        if (dwTHFlags & TH32CS_SNAPMODULE)
            SortModuleList(hSnap);
    }

    if (dwMask & SHELL_GETPROC) {
        if (INVALID_HANDLE_VALUE != hSnap) {
            proc.dwSize = sizeof(proc);
            
            // First save away previous pProcData
            pSavedProcList = v_pProcData;
            v_pProcData = pProcData = NULL;
            
            if (Process32First(hSnap,&proc)) {
                cNumProc = 0;
                StringCbPrintf(rgbText, ccbrgbText, L"PROC: Name            PID      AcctId   VMBase   CurZone\r\n");
                Puts(rgbText);
                if (dwMask & SHELL_GETTHRD) {
                    StringCbPrintf(rgbText, ccbrgbText, L"THRD: State  TID      PID      CurAKY   Cp  Bp   Kernel Time  User Time\r\n");
                    Puts(rgbText);
                }
                do {
                    // for now just match module; move down if want arb text
                    fMatch = IsPatMatch(pszArg, proc.szExeFile);
                    StringCbPrintf(rgbText, ccbrgbText, L" P%2.2d: %-15s %8.8lx %8.8lx %8.8lx %8.8lx\r\n",cNumProc,
                        proc.szExeFile,proc.th32ProcessID,proc.th32AccessKey,proc.th32MemoryBase,proc.dwFlags);

                    if (NULL == (pNextProcData = LocalAlloc (LPTR, sizeof(PROC_DATA)))) {
                        Puts (TEXT("Shell: Error unable allocate PROCDATA structure\r\n"));
                        break;
                    }
                    if (NULL != pProcData) {
                        pProcData->pNext = pNextProcData;
                    } else {
                        v_pProcData = pNextProcData;
                    }
                    pProcData = pNextProcData;
                    pProcData->dwProcid = proc.th32ProcessID;
                    pProcData->dwAccess = proc.th32AccessKey;
                    pProcData->dwZone = proc.dwFlags;
                    memcpy (pProcData->szExeFile, proc.szExeFile, sizeof(pProcData->szExeFile));
                    cNumProc++;
                    
                    if (fMatch) {
                        Puts(rgbText);
                    } else {
                        continue;
                    }
                    if (dwMask & SHELL_GETTHRD) {

                        thread.dwSize = sizeof(thread);

                        if (Thread32First(hSnap,&thread)) {
                            HANDLE hThrd;
                            BOOL fGotTime;
                            do {
                                if (thread.th32OwnerProcessID == proc.th32ProcessID) {
                                    FILETIME f1,f2,f3,f4;
                                    __int64 hrs, mins, secs, ms;

                                    pThrdInfo = (PTHRD_INFO)LocalAlloc (LPTR, sizeof(THRD_INFO));
                                    if (pThrdInfo) {
                                        pThrdInfo->dwThreadID = (DWORD)thread.th32ThreadID;
                                        pThrdInfo->pNext = pProcData->pThrdInfo;
                                        pProcData->pThrdInfo = pThrdInfo;
                                    }
                                    
                                    pThrdInfoLast = NULL;
                                    
                                    // Search for the old proc data
                                    for (pOldProcData = pSavedProcList; NULL != pOldProcData; pOldProcData = pOldProcData->pNext) {
                                        if (pOldProcData->dwProcid == pProcData->dwProcid) {
                                            // Now search for the thread info
                                            for (pThrdInfoLast = pOldProcData->pThrdInfo; NULL != pThrdInfoLast; pThrdInfoLast = pThrdInfoLast->pNext) {
                                                if (pThrdInfoLast->dwThreadID == (DWORD)thread.th32ThreadID) {
                                                    break;
                                                }
                                            }
                                            break;
                                        }
                                    }

                                    StringCbPrintf(rgbText, ccbrgbText, L" T    %6.6s %8.8lx %8.8lx %8.8lx %3d %3d",
                                        rgszStates[thread.dwFlags],thread.th32ThreadID,thread.th32CurrentProcessID,thread.th32AccessKey,
                                        thread.tpBasePri-thread.tpDeltaPri,thread.tpBasePri);
                                    
                                    hThrd = OpenThread (0, 0, thread.th32ThreadID);
                                    fGotTime = GetThreadTimes(hThrd,&f1,&f2,&f3,&f4);
                                    CloseHandle (hThrd);

                                    if (fGotTime) {
                                        if (pThrdInfo) {
                                            // Save them
                                            *(__int64 *)&f1 = *(__int64 *)&f3;
                                            *(__int64 *)&f2 = *(__int64 *)&f4;
                                            if (fDelta && pThrdInfoLast) {
                                                *(__int64 *)&f3 -= *(__int64 *)&(pThrdInfoLast->ftKernThrdTime);
                                                *(__int64 *)&f4 -= *(__int64 *)&(pThrdInfoLast->ftUserThrdTime);
                                            }

                                            // Save away the current time.
                                            *(__int64 *)&(pThrdInfo->ftKernThrdTime) = *(__int64 *)&f1;
                                            *(__int64 *)&(pThrdInfo->ftUserThrdTime) = *(__int64 *)&f2;
                                            if (fDelta && (0 == *(__int64 *)&f3) && (0 == *(__int64 *)&f4)) {
                                                // If no time has been used, skip this thread
                                                continue;
                                            }
                                        }
                                        *(__int64 *)&f3 /= 10000;
                                        ms = *(__int64 *)&f3 % 1000;
                                        *(__int64 *)&f3 /= 1000;
                                        secs = *(__int64 *)&f3 % 60;
                                        *(__int64 *)&f3 /= 60;
                                        mins = *(__int64 *)&f3 % 60;
                                        *(__int64 *)&f3 /= 60;
                                        hrs = *(__int64 *)&f3;
                                        len = lstrlen(rgbText);
                                        if (len*sizeof(TCHAR) < ccbrgbText)
                                        {
                                            StringCbPrintf(rgbText + len, ccbrgbText - len*sizeof(TCHAR), L" %2.2d:%2.2d:%2.2d.%3.3d",
                                                (DWORD)hrs, (DWORD)mins, (DWORD)secs, (DWORD)ms);
                                        }
                                        *(__int64 *)&f4 /= 10000;
                                        ms = *(__int64 *)&f4 % 1000;
                                        *(__int64 *)&f4 /= 1000;
                                        secs = *(__int64 *)&f4 % 60;
                                        *(__int64 *)&f4 /= 60;
                                        mins = *(__int64 *)&f4 % 60;
                                        *(__int64 *)&f4 /= 60;
                                        hrs = *(__int64 *)&f4;
                                        len = lstrlen(rgbText);
                                        if (len*sizeof(TCHAR) < ccbrgbText)
                                        {
                                            StringCbPrintf(rgbText + len, ccbrgbText - len*sizeof(TCHAR), L" %2.2d:%2.2d:%2.2d.%3.3d\r\n",
                                                (DWORD)hrs, (DWORD)mins, (DWORD)secs, (DWORD)ms);
                                        }
                                    } else {
                                        len = lstrlen(rgbText);
                                        if (len*sizeof(TCHAR) < ccbrgbText)
                                        {
                                            StringCbPrintf(rgbText + len, ccbrgbText - len*sizeof(TCHAR), L" No Thread Time\r\n");
                                        }
                                    }
                                    Puts(rgbText);
                                }
                            } while (Thread32Next(hSnap,&thread));
                        }
                    }
                } while (Process32Next(hSnap,&proc));
                if (dwMask & SHELL_GETMOD)
                    if (fMatch) {
                        Puts(L"\r\n");
                    }
            }
            // Free the previous version of the data
            for (pProcData = pSavedProcList; NULL != pProcData; pProcData = pNextProcData) {
                // Free the old Thread Info
                for (pThrdInfo = pProcData->pThrdInfo; NULL != pThrdInfo; pThrdInfo = pThrdInfoLast) {
                    pThrdInfoLast = pThrdInfo->pNext;
                    LocalFree (pThrdInfo);
                }

                // Now free the ProcData
                pNextProcData = pProcData->pNext;
                LocalFree (pProcData);
            }
            
        } else {
            FmtPuts(L"Unable to obtain process/thread snapshot (%d)!\r\n",GetLastError());
        }
    }
    if (dwMask & SHELL_GETMOD) {
        if (INVALID_HANDLE_VALUE != hSnap) {
            //
            // Print the sorted list of modules
            //
            FmtPuts(L" MOD: Name            pModule :dwInUSE :dwVMBase:CurZone\r\n");
            for (i = 0,pModData = v_pModData; NULL != pModData; i++,pModData = pModData->pNext) {
                fMatch = IsPatMatch(pszArg, pModData->szModule);
                if (fMatch) {
                    // for now just match module; move down if want arb text
                    FmtPuts(L" M%2.2d: %-15s %8.8lx %8.8lx %8.8lx %8.8lx\r\n",i,
                            pModData->szModule, pModData->dwModid, pModData->dwProcUsage,
                            pModData->dwBaseAddr, pModData->dwFlags);
                }
            }
        } else {
            FmtPuts(L"Unable to obtain process/thread snapshot (%d)!\r\n",GetLastError());
        }
    }
    if (dwMask & SHELL_GETSYSTEM) {
        // return the CPU type info and OS major and minor version info

        DWORD dwInstructionSet;
        SYSTEM_INFO si;
        OSVERSIONINFO osv;

        osv.dwOSVersionInfoSize  = sizeof(OSVERSIONINFO);
        if (GetVersionEx(&osv)) {
            GetSystemInfo(&si);
            dwInstructionSet = 0;
            if (QueryInstructionSet(PROCESSOR_QUERY_INSTRUCTION, &dwInstructionSet)) {
                if (PROCESSOR_ARM_V4I_INSTRUCTION   == dwInstructionSet ||
                    PROCESSOR_ARM_V4IFP_INSTRUCTION == dwInstructionSet) {

                    // this could be ARMV4I|FP, ARMV6|FP or ARMV7|FP - we need to dig a bit further...
                    
                    // QueryInstructionSet requires this special handling to 
                    // maintain ARMV4I back compat. Look at NKQueryInstructionSet 
                    // (private\winceos\coreos\nk\kernel\kmisc.c) and the assignment
                    // to CEInstructionSet in private\winceos\coreos\nk\kernel\arm\mdarm.c)
                    // This is from mdarm.c:
                    // 
                    //   CEProcessorType     = (dwCpuCap >> 4) & 0xFFF;
                    //   CEProcessorLevel    = 4;
                    //   CEProcessorRevision = (WORD) dwCpuCap & 0x0f;
                    //   CEInstructionSet    = PROCESSOR_ARM_V4I_INSTRUCTION
                    // 
                    // That's why this extra code is required. 
                    // In order for VSD CoreCon over an app-level KITL connectlon
                    // to correctly identify that the device is an ARMV6 
                    // (and to make it deploy ARMV6 binaries), "gi system" needs 
                    // to tell the truth (as the app-level KITL bootstrap - 
                    // which is what reports the CPU type - just calls 
                    // "gi system" via shell.exe and parses the returned text). 

                    DWORD dwDummyInstSet = 0;
                    if (QueryInstructionSet(PROCESSOR_ARM_V7_INSTRUCTION, &dwDummyInstSet)) {
                        // this is ARMV7
                        dwInstructionSet = PROCESSOR_ARM_V7_INSTRUCTION;
                    }
                    else if (QueryInstructionSet(PROCESSOR_ARM_V6FP_INSTRUCTION, &dwDummyInstSet)) {
                        // this is ARMV6FP
                        dwInstructionSet = PROCESSOR_ARM_V6FP_INSTRUCTION;
                    }
                    else if (QueryInstructionSet(PROCESSOR_ARM_V6_INSTRUCTION, &dwDummyInstSet)) {
                        // this is ARMV6
                        dwInstructionSet = PROCESSOR_ARM_V6_INSTRUCTION;
                    }
                    // else this is ARMV4/5 - leave dwInstructionSet alone
                }
            }
            StringCbPrintf(rgbText, ccbrgbText, L"Architecture CpuType    OSMajor    OSMinor    InstructionSet NumberOfCpu\r\n"
                           L"0x%8.8lx   0x%8.8lx 0x%8.8lx 0x%8.8lx 0x%8.8lx     0x%8.8lx\r\n",
                           si.wProcessorArchitecture, si.dwProcessorType, osv.dwMajorVersion,
                           osv.dwMinorVersion, dwInstructionSet, si.dwNumberOfProcessors);
            Puts(rgbText);
        }
    }

    if (hSnap != INVALID_HANDLE_VALUE)
        CloseToolhelp32Snapshot(hSnap);

    return TRUE;
}
                                        
void DisplayOneBlock (const DWORD *ptes, DWORD dwStartAddr, PPAGE_USAGE_INFO ppui, DWORD dwExeEnd, BOOL bFull)
{
    int   idx       = 0;
    BOOL  fPageType = 0;
    WCHAR achTypes[VM_PAGES_PER_BLOCK+1];
    DWORD dwEntry;
    
    if (   (dwStartAddr  && (dwStartAddr  <  dwExeEnd))                             // EXE
        || ((dwStartAddr >= g_pKData->dwKDllFirst) && (dwStartAddr < g_pKData->dwKDllEnd))      // k-mode DLL
        || ((dwStartAddr >= VM_DLL_BASE)  && (dwStartAddr < VM_RAM_MAP_BASE))) {    // u-mode DLL
        fPageType = VM_PAGER_LOADER;
    }

    for (idx = 0; idx < VM_PAGES_PER_BLOCK; idx ++) {
        dwEntry = ptes[idx];
        if(dwEntry == 0) {
            break;
        }

        if (IsPageCommitted (dwEntry)) {
            if (IsObjectStore (dwStartAddr)) {
                achTypes[idx] = 'P';
            } else if (IsPageWritable (dwEntry)) {

                if (VM_PAGER_AUTO == fPageType) {
                    achTypes[idx] = 'S';
                    ppui->cpStack ++;
                } else if (IsPfnInRAM (PFNfromEntry (dwEntry))) {
                    achTypes[idx] = 'W';
                    ppui->cpRWData ++;
                } else {
                    achTypes[idx] = 'P';
                }

            } else if (VM_PAGER_LOADER == fPageType) {

                if (IsPfnInROM (PFNfromEntry (dwEntry))) {
                    achTypes[idx] = 'C';
                    ppui->cpROMCode ++;
                } else {
                    achTypes[idx] = 'c';
                    ppui->cpRAMCode ++;
                }
                
            } else {
                achTypes[idx] = IsPfnInROM (PFNfromEntry (dwEntry))? 'R' : 'r';
                ppui->cpROData ++;
            }
                
        } else {
            achTypes[idx] = '-';
            ppui->cpReserved ++;
            if (!fPageType) {
                fPageType = GetPagerType (dwEntry);
            }
        }
    }
    achTypes[idx] = 0;   // set EOS

    // print the line if 'full' is specified
    if (idx && bFull) {
        FmtPuts(L"  %8.8lx: %s\r\n", dwStartAddr, achTypes);
    }
}

void DisplayVMInfo (PPAGETABLE pptbl, DWORD dwStartAddr, PPAGE_USAGE_INFO ppui, DWORD dwExeEnd, BOOL bFull)
{
    int idx;
    
    for (idx = 0; idx < VM_NUM_PT_ENTRIES; idx += VM_PAGES_PER_BLOCK) {
        DisplayOneBlock (&pptbl->pte[idx], dwStartAddr, ppui, dwExeEnd, bFull);
        dwStartAddr += VM_BLOCK_SIZE;
    }
}

void DisplayProcessInfo (PPROC_DATA pProcData, BOOL bFull)
{
    PROCMEMINFO     pmi = {0};
    PAGE_USAGE_INFO pui = {0};
    
    PPAGETABLE ppdirMap = (PPAGETABLE) KernelLibIoControl ((HANDLE) KMOD_CORE, IOCTL_KLIB_MAPPD,
            &pProcData->dwProcid, sizeof (DWORD),
            &pmi, sizeof (pmi),
            NULL);

    if (!ppdirMap) {
        FmtPuts(TEXT("Unable to get memory info for process '%s', id = 0x%x.\r\n\r\n"),
            pProcData->szExeFile, pProcData->dwProcid);
    } else {

        PPAGETABLE pptbl  = ppdirMap;
        const BYTE *pdbits = pmi.pdbits;
        DWORD dwStartAddr = 0;
        DWORD dwEndAddr   = VM_SHARED_HEAP_BASE;
        
        if (pmi.fIsKernel) {

            dwStartAddr = VM_SHARED_HEAP_BASE;
            
            Puts(L"\r\nMemory usage for Shared Heap:\r\n");

            for ( ; dwStartAddr < VM_KMODE_BASE; pptbl ++, pdbits ++, dwStartAddr += _4M) {
                if (*pdbits) {
                    DisplayVMInfo (pptbl, dwStartAddr, &pui, pmi.dwExeEnd, bFull);
                }
            }

            dwEndAddr   = VM_CPU_SPECIFIC_BASE;

        }

        FmtPuts(L"\r\nMemory usage for Process '%s' pid %x\r\n",
            pProcData->szExeFile, pProcData->dwProcid);
        
        // DisplayVMInfo (ppdirMap, &pmi, bFull);
        for ( ; dwStartAddr < dwEndAddr; pptbl ++, pdbits ++, dwStartAddr += _4M) {
            if (*pdbits) {
                DisplayVMInfo (pptbl, dwStartAddr, &pui, pmi.dwExeEnd, bFull);
            }
        }

        // unmap page directory
        VERIFY (KernelLibIoControl ((HANDLE) KMOD_CORE, IOCTL_KLIB_UNMAPPD,
                ppdirMap, 0, NULL, 0, NULL));

        TotalCpgRW += pui.cpRWData + pui.cpStack;

        FmtPuts(L"Page summary: code=%d(%d) data r/o=%d r/w=%d stack=%d reserved=%d\r\n",
            pui.cpROMCode, pui.cpRAMCode, pui.cpROData, pui.cpRWData, pui.cpStack, pui.cpReserved);
        
    }
}

void DisplayKernelHeapInfo(void) {
    LPBYTE    buf[sizeof (heapptr_t) + MAX_PATH];   // need room for heap class name
    heapptr_t *hptr = (heapptr_t *) buf;
    DWORD     cbRet;
    int       loop;
    ulong     cbUsed, cbExtra, cbMax;
    ulong     cbTotalUsed = 0, cbTotalExtra = 0;

    Puts(L"Inx Size   Used    Max Extra  Entries Name\r\n");
    for (loop = 0 ; loop < NUMARENAS ; ++loop) {
        if (!KernelLibIoControl ((HANDLE) KMOD_CORE, IOCTL_KLIB_GETKHEAPINFO,
            NULL, loop, buf, sizeof (buf), &cbRet)) {
            DEBUGCHK (0);
        } else {
            cbUsed = hptr->size * hptr->cUsed;
            cbMax = hptr->size * hptr->cMax;
            cbExtra = cbMax - cbUsed;
            cbTotalUsed += cbUsed;
            cbTotalExtra += cbExtra;
            FmtPuts(L"%2d: %4d %6ld %6ld %5ld %3d(%3d) %hs\r\n", loop, hptr->size,
                    cbUsed, cbMax, cbExtra, hptr->cUsed, hptr->cMax, hptr->classname);
        }
    }
    FmtPuts(L"Total Used = %ld  Total Extra = %ld  Waste = %d\r\n",
            cbTotalUsed, cbTotalExtra, UserKInfo[KINX_HEAP_WASTE]);
}

BOOL DoMemtool (PTSTR cmdline) {
    BOOL bKern = 0, bFull = 0;
#if 0
    DWORD oldMode, oldPerm;
#endif
    PCTSTR szToken;
    if (cmdline) {
        while ((szToken = _tcstok(cmdline, TEXT(" \t"))) != NULL) {
            cmdline = NULL; // to make subsequent _tcstok's work
            if (!_tcscmp(szToken, TEXT("full")))
                bFull = 1;
            else if (!_tcscmp(szToken, TEXT("kernel")))
                bKern = 1;
        }
    }
    Puts(L"\r\nWindows CE Kernel Memory Usage Tool 0.2\r\n");
    FmtPuts(L"Page size=%d, %d total pages, %d free pages. %d MinFree pages (%d current bytes, %d MaxUsed bytes)\r\n",
           UserKInfo[KINX_PAGESIZE], UserKInfo[KINX_NUMPAGES],
           UserKInfo[KINX_PAGEFREE], UserKInfo[KINX_MINPAGEFREE],
           UserKInfo[KINX_PAGESIZE]*(UserKInfo[KINX_NUMPAGES]-UserKInfo[KINX_PAGEFREE]),            
           UserKInfo[KINX_PAGESIZE]*(UserKInfo[KINX_NUMPAGES]-UserKInfo[KINX_MINPAGEFREE]));
    FmtPuts(L"%d pages used by kernel, %d pages held by kernel, %d pages consumed.\r\n",
           UserKInfo[KINX_SYSPAGES],UserKInfo[KINX_KERNRESERVE],
           UserKInfo[KINX_NUMPAGES]-UserKInfo[KINX_PAGEFREE]);

    TotalCpgRW = 0;
    if (bFull || bKern)
        DisplayKernelHeapInfo();
    
    if (bFull || !bKern) {

        PPROC_DATA pProcData;
        WCHAR szProc[] = L"proc";

        // refresh process list (can fail if OOM, and will use old proc list in that case)        
        DoGetInfo (szProc);
        
        for (pProcData = v_pProcData; pProcData; pProcData = pProcData->pNext) {
            DisplayProcessInfo (pProcData, bFull);
        }
        
        FmtPuts(L"\r\nTotal R/W data + stack = %ld\r\n", TotalCpgRW);
    }

    return TRUE;
}

LPVOID 
GetDataFromHandle (
    DWORD dwHandle,
    BOOL *fMod
    )
{

    //  Couldn't convert wide-string to unsigned long
    if (0==dwHandle)
        return NULL;

    *fMod = !(dwHandle & 3);
    if(*fMod != 0) {

        PMOD_DATA pModData;

        for (pModData = v_pModData; pModData; pModData = pModData->pNext) {
            if (dwHandle == pModData->dwModid) {
                return pModData;
            }
        }
        
    } else {

        PPROC_DATA pProcData;
        
        for (pProcData = v_pProcData; pProcData; pProcData = pProcData->pNext) {
            if (dwHandle == pProcData->dwProcid) {
                return pProcData;
            }
        }
    }

    return NULL;
}

LPVOID
GetDataFromName (
    PCTSTR szName,
    BOOL  fMod
    )
{
    if (fMod) {
        PMOD_DATA pModData;
        for (pModData = v_pModData; pModData; pModData = pModData->pNext) {
            if (IsNameMatch(szName, pModData->szModule)) {
                return pModData;
            }
        }
    } else {
        PPROC_DATA pProcData;
        for (pProcData = v_pProcData; pProcData; pProcData = pProcData->pNext) {
            if (IsNameMatch(szName, pProcData->szExeFile)) {
                return pProcData;
            }
        }
    }

    return NULL;
}

LPVOID
GetDataFromOrdinal (
    DWORD dwOrdinal,
    BOOL  fMod 
    )
{
    DWORD dwIdx = 0;
    if (fMod) {
        PMOD_DATA pModData;
        for (pModData = v_pModData; pModData && (dwIdx < dwOrdinal); pModData = pModData->pNext, dwIdx ++) {
            // empty body
        }
        return pModData;
    } else {
        PPROC_DATA pProcData;
        for (pProcData = v_pProcData; pProcData && (dwIdx < dwOrdinal); pProcData = pProcData->pNext, dwIdx ++) {
            // empty body
        }
        return pProcData;
    }
}


void SetAllZones(BOOL fMod, DWORD dwZone)
{
    PMOD_DATA pModData;
    PPROC_DATA pProcData;

    if (fMod) {
        for (pModData = v_pModData; NULL != pModData; pModData = pModData->pNext) {
            if (pModData->dwModid) {
                SetDbgZone(0,(LPVOID)pModData->dwModid,0,dwZone,NULL);
            }
        }
    } else {
        for (pProcData = v_pProcData; NULL != pProcData; pProcData = pProcData->pNext) {
            if (pProcData->dwProcid) {
                SetDbgZone(pProcData->dwProcid,0,0,dwZone,NULL);
            }
        }
    }
}


void InitProcOrModList(BOOL fMod, PTSTR cmdline, PCTSTR original)
{
    WCHAR szInfo[5] = L"proc";

    if (fMod) {
        StringCchCopy(szInfo, 5, L"mod");
    }

    // Initialize the module or process list
    DoGetInfo(szInfo);

    // Restore strtok state
    _tcsncpy(cmdline, original, MAX_PATH);
    cmdline[MAX_PATH-1] = 0;

    _tcstok(cmdline, TEXT(" \t"));
    _tcstok(NULL, TEXT(" \t"));
}


BOOL DoZoneOps (PTSTR cmdline) {

    BOOL    fMod = FALSE, fHandle = FALSE, bRetVal = FALSE;
    PTSTR   szId;
    LPCTSTR szMod, szRest;
    DWORD   dwZone, dwId;
    LPVOID  pModProcData = NULL;
    DBGPARAM    dbg;
    LPDBGPARAM  lpdbg = &dbg;
    TCHAR   original[MAX_PATH];

    if (!cmdline) {
        ERRORMSG (1, (TEXT("DoZoneOp: Invalid module string (NULL)\r\n")));
        return TRUE;
    }

    if ((0 == cNumMod) || (0 == cNumProc)) {
        // Save original command line in case one of the lists needs to be initialized.
        _tcsncpy(original, cmdline, ARRAYSIZE(original) - 1);
        original[ARRAYSIZE(original) - 1] = 0;     // force null termination
    }

    if ((szMod = _tcstok(cmdline, TEXT(" \t"))) != NULL &&
        (szId = _tcstok(NULL, TEXT(" \t"))) != NULL) {

        // get module vs proc vs handle
        if (TEXT('m')==szMod[0] || TEXT('M')==szMod[0]) {
            fMod = TRUE;
            if (0 == cNumMod) {
                InitProcOrModList(fMod, cmdline, original);
            }
        } else if (TEXT('p')==szMod[0] || TEXT('P')==szMod[0]) {
            fMod = FALSE;
            if (0 == cNumProc) {
                InitProcOrModList(fMod, cmdline, original);
            }
        } else if (TEXT('h')==szMod[0] || TEXT('H')==szMod[0]) {
            fHandle = TRUE;
        } else {
            ERRORMSG(1, (TEXT("DoZoneOp: Invalid module string %s!\r\n"), szMod));
            goto done;
        }

        // parse ID
        if (0 == _tcscmp(TEXT("alloff"), szId)) {
            SetAllZones(fMod, 0);
            return TRUE;
        } else if (0 == _tcscmp(TEXT("allon"), szId)) {
            SetAllZones(fMod, 0xffff);
            return TRUE;
        } else if (TRUE==fHandle) {
            pModProcData = GetDataFromHandle (wcstoul(szId,NULL,16), &fMod);
        } else if (isdigit(szId[0])) {
            pModProcData = GetDataFromOrdinal (_wtol(szId), fMod);
        } else {
            pModProcData = GetDataFromName (szId, fMod);
        }

        if (!pModProcData) {
            ERRORMSG(1, (TEXT("DoZoneOp: Invalid handle/id %s!\r\n"), szId));
            goto done;
        }

        // Initialize dwZone/dwId to the current value
        if (fMod) {
            dwZone = ((PMOD_DATA) pModProcData)->dwZone;
            dwId   = ((PMOD_DATA) pModProcData)->dwModid;
        } else {
            dwZone = ((PPROC_DATA) pModProcData)->dwZone;
            dwId   = ((PPROC_DATA) pModProcData)->dwProcid;
        }
        // Check what the rest of cmd line is about
        szRest = _tcstok(NULL, TEXT(" \t"));
        if (!szRest) {
            dwZone = 0xffffffff;
            // It's a get information call ...
        } else if (TEXT('0')<=szRest[0] && TEXT('9')>=szRest[0]) {
            // It's a zone set with an absolute zonemask
            dwZone = wcstol(szRest, NULL, 16);
            DEBUGMSG(ZONE_INFO, (TEXT("Shell:DoSetZone:id:%08X;zone:%08X\r\n"),
                        dwId, dwZone));
        } else if (TEXT('o')==szRest[0] || TEXT('O')==szRest[0]) {
            BOOL    fOn;
            DWORD   bitmask;
            // go into a loop extracting further tokens
            while (szRest) {
                fOn = !_tcscmp(szRest, TEXT("on"));
                for(;;) {
                    szRest = _tcstok(NULL, TEXT(" \t"));
                    if(szRest == NULL ||
                        szRest[0] == TEXT('o') ||
                        szRest[0] == TEXT('O')) {
                        break;
                    }
                    bitmask = 1<<_wtol(szRest);
                    dwZone = fOn? dwZone|bitmask: dwZone&~bitmask;
                }
            }
        } else {
            ERRORMSG(1, (TEXT("DoZoneOp: Invalid Command %s!\r\n"), szRest));
            goto done;
        }
        
        // Make the actual call ...
        if (fMod)
            bRetVal = SetDbgZone (0, (LPVOID)dwId, 0, dwZone, lpdbg);
        else
            bRetVal = SetDbgZone (dwId, 0, 0, dwZone, lpdbg);
        
        if (!bRetVal)
            goto done;
        
        if (dwZone == 0xffffffff)
            dwZone = lpdbg->ulZoneMask;

        if (fMod)
            ((PMOD_DATA) pModProcData)->dwZone = dwZone;
        else
            ((PPROC_DATA) pModProcData)->dwZone = dwZone;
        
#define Mark(i) ((dwZone&(1<<i))?TEXT('*'):TEXT(' '))
        FmtPuts (TEXT("Registered Name:%s   CurZone:%08X\r\n"),
                lpdbg->lpszName, dwZone);
        Puts(TEXT("Zone Names - Prefixed with bit number and * if currently on\r\n"));
        FmtPuts (TEXT(" 0%c%-16s: 1%c%-16s: 2%c%-16s: 3%c%-16s\r\n"),
              Mark(0), lpdbg->rglpszZones[0], Mark(1), lpdbg->rglpszZones[1],
              Mark(2), lpdbg->rglpszZones[2], Mark(3), lpdbg->rglpszZones[3]);
        FmtPuts(TEXT(" 4%c%-16s: 5%c%-16s: 6%c%-16s: 7%c%-16s\r\n"),
              Mark(4), lpdbg->rglpszZones[4], Mark(5), lpdbg->rglpszZones[5],
              Mark(6), lpdbg->rglpszZones[6], Mark(7), lpdbg->rglpszZones[7]);
        FmtPuts(TEXT(" 8%c%-16s: 9%c%-16s:10%c%-16s:11%c%-16s\r\n"),
              Mark(8), lpdbg->rglpszZones[8], Mark(9), lpdbg->rglpszZones[9],
              Mark(10), lpdbg->rglpszZones[10], Mark(11), lpdbg->rglpszZones[11]);
        FmtPuts(TEXT("12%c%-16s:13%c%-16s:14%c%-16s:15%c%-16s\r\n"),
              Mark(12), lpdbg->rglpszZones[12], Mark(13), lpdbg->rglpszZones[13],
              Mark(14), lpdbg->rglpszZones[14], Mark(15), lpdbg->rglpszZones[15]);
    }

done:
    if (!bRetVal)
        Puts(TEXT("Zone Operation failed! May be an invalid param - type ? for help.\r\n"));
    return TRUE;
}


#include "..\..\..\..\public\common\oak\dbgpub\dbgbrk.c" // For DoDebugBreak()


BOOL DoBreak (PTSTR cmdline) {
    DoDebugBreak ();
    return TRUE;
}


//  DoRunProcess - create process and wait for termination
//
//  input - cmdline - command line to pass to create process
//
//  returns -   TRUE process created and terminated normally
//              FALSE unable to create the process
BOOL DoRunProcess (PTSTR cmdline) {
    BOOL fOk;
    TCHAR szImageName[MAX_PATH];
    PROCESS_INFORMATION pi;

    if (cmdline == NULL) {
        Puts(TEXT("Run process requires an argument\r\n"));
        return TRUE;
    }

    fOk = LaunchProcess(cmdline, szImageName, sizeof(szImageName) / sizeof(szImageName[0]), &pi);
    if (!fOk)  {
        FmtPuts (TEXT("Unable to create process '%s' : Error %d\r\n"),
                 szImageName, GetLastError());
    } else {
        Puts(TEXT("Running "));
        Puts(szImageName);
        do {
            Puts(TEXT("."));
        } while (WAIT_TIMEOUT == WaitForSingleObject(pi.hProcess,3000));
        Puts(TEXT("done.\r\n"));
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    return TRUE;
}

//  DoRunBatchFile - runs a batch file
//
//  input - cmdline contains the name of the batch file to open
//
//  returns TRUE batch file opened and commands executed
//          TRUE unable to open batch file
#define READ_SIZE (1024)
#define TRUNC_SIZE (20)
BOOL DoRunBatchFile(PTSTR cmdline) {
    PCTSTR   szFile;
    int     fd;
    int     len=0;
    int     len2=0;
    int     iBOL=0;
    int     iEOL=0;
    DWORD dwFileSize = 0;
    int     cLine = 1;
    char   cBuf [READ_SIZE + 1];
    TCHAR wsz[MAX_PATH];
    if (cmdline == NULL) {
        Puts(TEXT("Syntax: run <filename>\r\n"));
        return TRUE;
    }
    szFile=_tcstok(cmdline, TEXT(" \t"));
    if( -1 == (fd = UU_ropen( szFile, 0x10000 )) ) {
        _snwprintf (wsz, ARRAYSIZE(wsz) - 1, L"Unable to open input file %s\r\n", szFile);
        wsz[ARRAYSIZE(wsz) - 1] = 0;
        Puts (wsz);
        return TRUE;
    }
    dwFileSize = GetFileSize ((HANDLE) fd, NULL) ;
    for(;;)
    {
        len = UU_rread (fd, &cBuf[len2], READ_SIZE - len2);
        if(len == 0) {
            break;
        }
        dwFileSize -= len;
        len += len2;
        do
        {
            while (iEOL < len && cBuf[iEOL] && cBuf[iEOL] != '\r' && cBuf[iEOL] != '\n')
            {
                iEOL++;
            }
            if (((iEOL < len) && ((cBuf[iEOL] == '\r') || (cBuf[iEOL] == '\n'))) || !dwFileSize)
            { // Line complete
                if (iEOL != iBOL)
                { // Line not empty
                    cBuf [iEOL] = '\0';
                    MultiByteToWideChar(CP_ACP, 0, &cBuf[iBOL], -1, wsz, sizeof(wsz)/sizeof(TCHAR));
                    DoRunProcess(wsz);
                }
                while ((++iEOL < len) && cBuf[iEOL] && (cBuf[iEOL] == '\r' || cBuf[iEOL] == '\n'));
                ++cLine;
                iBOL = iEOL;
            }
            else
            {
                if (!iBOL)
                {
                    cBuf [TRUNC_SIZE] = '\0';
                    FmtPuts (TEXT ("Ignoring line %i ('%S...'), line is too long (max is %i characters).\r\n"),
                                    cLine, cBuf, READ_SIZE);
                    iEOL = 0;
                    do
                    {
                        len = UU_rread (fd, cBuf, READ_SIZE);
                        dwFileSize -= len;
                        while ((iEOL < len) && cBuf [iEOL] && (cBuf [iEOL] != '\r') && (cBuf [iEOL] != '\n'))
                        {
                            iEOL++;
                        }
                    }
                    while ((iEOL == len) && (cBuf [iEOL] != '\r') && (cBuf [iEOL] != '\n') && dwFileSize);
                    iBOL = iEOL;
                }
            }
        }
        while (iEOL < len);
        if (len > iBOL)
        {
            len2 = len - iBOL;
            memcpy (cBuf, &cBuf [iBOL], len2);
            cBuf [len2] = '\0';
        }
        else
        {
            len2 = 0;
        }
        iEOL=iBOL=0;
    }
    UU_rclose(fd);
    return TRUE;
}

BOOL DoDiscard(PTSTR cmdline) {
    ForcePageout();
    return TRUE;
}

// parses the argument in the command line - will only handle numbers
// returns: TRUE - next argument is a number - value stored in *num, pcmdline updated
//          FALSE - next argument is not a number - *num not changed, pcmdline not changed
BOOL IntArg(PTSTR *ppCmdLine, int *num) {
    PTSTR pCmdLine;
    BOOL bDig = FALSE, bHex = FALSE;
    int x;

    pCmdLine = *ppCmdLine;

    if (pCmdLine == NULL || *pCmdLine == 0)
        return FALSE;

    if (pCmdLine[0] == '0' && pCmdLine[1] == 'x') {
        x = wcstol(pCmdLine, NULL, 16);
        pCmdLine += 2;
        bHex = TRUE;
    }
    else
        x = _wtol(pCmdLine);

    if (*pCmdLine == '-' || *pCmdLine == '+')
        pCmdLine++;
    while (*pCmdLine && ((*pCmdLine >= '0' && *pCmdLine <= '9') ||
                         (*pCmdLine >= 'a' && *pCmdLine <= 'f' && bHex) ||
                         (*pCmdLine >= 'A' && *pCmdLine <= 'F' && bHex))) {
        bDig = TRUE;
        pCmdLine++;
    }
    if (!bDig || (*pCmdLine && *pCmdLine != ' '))
        return FALSE;
    while (*pCmdLine && (*pCmdLine == ' '))
        pCmdLine++;
    *ppCmdLine = pCmdLine;
    *num = x;
    return TRUE;
}

BOOL DwordArg(PTSTR *ppCmdLine, DWORD *dw)
{
    return IntArg(ppCmdLine, (int *)dw);
}

BOOL UlongArg(PTSTR *ppCmdLine, ULONG *ul)
{
    return IntArg(ppCmdLine, (int *)ul);
}

// Parses the argument in the command line - handles strings and will deal with quotes for names with spaces
// returns:  TRUE  - next argument is a string - value stored in pszFileName, pcmdline updated
//           FALSE - next argument is not a string or not enough room in the pszFileName - params not updated
BOOL FileNameArg(PTSTR *ppCmdLine, PTSTR pszFileName, UINT cchFileName)
{
    PTSTR pCmdLine = NULL, pLocalFileName = NULL;
    BOOL fValidInput = TRUE, fBreakOut = FALSE;
    UINT uiIndexIntoFileName = 0;

    pCmdLine = *ppCmdLine;
    if (NULL == pCmdLine || 0 == *pCmdLine)
    {
        return FALSE;
    }

    pLocalFileName = pszFileName;
    if (NULL == pLocalFileName || 0 == cchFileName)
    {
        return FALSE;
    }

    while (*pCmdLine && _istspace(*pCmdLine))
    { // Skip leading whitespaces
        pCmdLine++;
    }

    if ('"' == *pCmdLine)
    {
        fValidInput = FALSE;
        pCmdLine++;
    }

    while (*pCmdLine)
    {
        if (FALSE == fValidInput && ('"' == *pCmdLine))
        { // End of quotes; make sure there is a whitespace after this
            fValidInput = TRUE;

            pCmdLine++;
            if (*pCmdLine && !_istspace(*pCmdLine))
            { // Malformed; let's mark and bail out
                fValidInput = FALSE;
            }

            fBreakOut = TRUE;
            break;
        }
        else if (_istspace(*pCmdLine) && TRUE == fValidInput)
        {
            fBreakOut = TRUE;
        }
        else
        {
            *pLocalFileName++ = *pCmdLine;
            pCmdLine++;
            uiIndexIntoFileName++;
            if (uiIndexIntoFileName >= cchFileName)
            {
                fValidInput = FALSE;
                fBreakOut = TRUE;
            }
        }

        if (TRUE == fBreakOut)
        {
            break;
        }
    }

    if (TRUE == fValidInput)
    {
        while (*pCmdLine && _istspace(*pCmdLine))
        { // Ignore trailing whitespaces
            pCmdLine++;
        }

        *pLocalFileName++ = '\0';

        *ppCmdLine = pCmdLine;
        return TRUE;
    }

    *pszFileName = '\0';
    return FALSE;    
}


typedef HWND (WINAPI *PFN_GetWindow) (HWND hwnd, UINT uCmd);
typedef LONG (WINAPI *PFN_GetWindowLong) (HWND hWnd, int nIndex);
typedef int  (WINAPI *PFN_GetWindowText) (HWND hWnd, LPWSTR lpString, int nMaxCount);
typedef int  (WINAPI *PFN_GetClassName) (HWND hWnd, LPWSTR lpClassName, int nMaxCount);
typedef BOOL (WINAPI *PFN_GetWindowRect) (HWND hwnd, LPRECT prc);
typedef DWORD (WINAPI *PFN_GetWindowThreadProcessId) (HWND hWnd, LPDWORD lpdwProcessId);
typedef HWND (WINAPI *PFN_GetParent) (HWND hwnd);
typedef BOOL (WINAPI *PFN_GetForegroundInfo) (GET_FOREGROUND_INFO *pgfi);
typedef DWORD (WINAPI *PFN_GetWindowCompositionFlags) (HWND hWnd);

PFN_GetWindow       v_pfnGetWindow;
PFN_GetWindowLong   v_pfnGetWindowLong;
PFN_GetWindowText   v_pfnGetWindowText;
PFN_GetClassName    v_pfnGetClassName;
PFN_GetWindowRect   v_pfnGetWindowRect;
PFN_GetWindowThreadProcessId    v_pfnGetWindowThreadProcessId;
PFN_GetParent       v_pfnGetParent;
PFN_GetForegroundInfo   v_pfnGetForegroundInfo;
PFN_GetWindowCompositionFlags v_pfnGetWindowCompositionFlags;

typedef struct STYLEENTRY
{
    DWORD style;
    LPTSTR sz;
} STYLEENTRY;

STYLEENTRY v_rgse[] = {
    {WS_CAPTION, TEXT("WS_CAPTION")},
    {WS_BORDER , TEXT("WS_BORDER")},
    {WS_DLGFRAME,TEXT("WS_DLGFRAME")},
    {WS_CHILD,   TEXT("WS_CHILD")},
    {WS_VISIBLE,   TEXT("WS_VISIBLE")},
    {WS_DISABLED,   TEXT("WS_DISABLED")},
    {WS_CLIPSIBLINGS,   TEXT("WS_CLIPSIBLINGS")},
    {WS_CLIPCHILDREN,   TEXT("WS_CLIPCHILDREN")},
    {WS_VSCROLL,   TEXT("WS_VSCROLL")},
    {WS_HSCROLL,   TEXT("WS_HSCROLL")},
    {WS_SYSMENU,   TEXT("WS_SYSMENU")},
    {WS_GROUP,   TEXT("WS_GROUP")},
    {WS_TABSTOP,   TEXT("WS_TABSTOP")},
    {WS_POPUP,   TEXT("WS_POPUP")},
};

STYLEENTRY v_rgseEx[] = {
    {WS_EX_DLGMODALFRAME,   TEXT("WS_EX_DLGMODALFRAME")},
    {WS_EX_TOPMOST,   TEXT("WS_EX_TOPMOST")},
    {WS_EX_WINDOWEDGE,   TEXT("WS_WINDOWEDGE")},
    {WS_EX_CLIENTEDGE,   TEXT("WS_CLIENTEDGE")},
    {WS_EX_CONTEXTHELP,   TEXT("WS_CONTEXTHELP")},
    {WS_EX_STATICEDGE,   TEXT("WS_STATICEDGE")},
    {WS_EX_OVERLAPPEDWINDOW,   TEXT("WS_OVERLAPPEDWINDOW")},
    {WS_EX_CAPTIONOKBTN,   TEXT("WS_CAPTIONOKBTN")},
    {WS_EX_NODRAG,   TEXT("WS_EX_NODRAG")},
    {WS_EX_ABOVESTARTUP,   TEXT("WS_ABOVESTARTUP")},
    {WS_EX_INK,   TEXT("WS_EX_INK")},
    {WS_EX_NOANIMATION,   TEXT("WS_EX_NOANIMATION")},
    {WS_EX_NOACTIVATE,   TEXT("WS_EX_NOACTIVATE")},
};

STYLEENTRY v_rgseWCF[] = {
    {WCF_ALPHATRANSPARENCY, TEXT("WCF_ALPHATRANSPARENCY")},
    {WCF_TRUECOLOR,         TEXT("WCF_TRUECOLOR")},
    {WCF_OVERLAYTARGET,     TEXT("WCF_OVERLAYTARGET")},
};

void Indent (int iIndent)
{
    int i;
    for (i=0; i < iIndent; i++)
        Puts (TEXT("    "));
}


void PrintWindow (HWND hwnd, int iIndent)
{
    TCHAR szBuf[100];
    RECT rc;
    DWORD hprc, hthd;
    DWORD style, exstyle, i, wcf;
    BOOL fPrintedExStyle, fPrintedWCF;
    TCHAR   szOutStr[MAX_PATH];

    Indent(iIndent);
    if (v_pfnGetWindowText (hwnd, szBuf, sizeof(szBuf)/sizeof(TCHAR))) {
     _snwprintf(szOutStr, ARRAYSIZE(szOutStr) - 1, TEXT("\"%s\"\r\n"), szBuf);
     szOutStr[ARRAYSIZE(szOutStr) - 1] = 0;
    } else {
        StringCbPrintf (szOutStr, sizeof(szOutStr), TEXT("<No name>\r\n"));
    }
    Puts (szOutStr);

    if (!v_pfnGetClassName (hwnd, szBuf, sizeof(szBuf)/sizeof(TCHAR))) {
        _tcscpy (szBuf, TEXT("Unknown class"));
        Puts (szOutStr);
    }
    v_pfnGetWindowRect (hwnd, &rc);
    hthd = v_pfnGetWindowThreadProcessId (hwnd, &hprc);
    Indent(iIndent);

    FmtPuts (TEXT("hwnd=%08x Class=%s parent=%08x thread=%08x process=%08x\r\n"),
             (DWORD)hwnd, szBuf, (DWORD)(v_pfnGetParent(hwnd)), hthd, hprc);

    Indent(iIndent);
    FmtPuts (TEXT("x=%d y=%d width=%d height=%d\r\n"), rc.left, rc.top,
             rc.right - rc.left, rc.bottom - rc.top);

    style = v_pfnGetWindowLong (hwnd, GWL_STYLE);
    Indent(iIndent);
    Puts (TEXT("Style="));
    for (i=0; i < sizeof(v_rgse) / sizeof(v_rgse[0]); i++) {
        if (style & v_rgse[i].style) {
            style &= ~v_rgse[i].style;
            FmtPuts (TEXT("%s "), v_rgse[i].sz);
        }
    }
    if (style) {
        FmtPuts (TEXT("%08x "), style);
    }
    Puts (TEXT("\r\n"));

    exstyle = v_pfnGetWindowLong (hwnd, GWL_EXSTYLE);
    fPrintedExStyle = FALSE;
    for (i=0; i < sizeof(v_rgseEx) / sizeof(v_rgseEx[0]); i++) {
        if (exstyle & v_rgseEx[i].style) {
            exstyle &= ~v_rgseEx[i].style;
            if (!fPrintedExStyle) {
                Indent(iIndent);
                Puts (TEXT("exstyle="));
                fPrintedExStyle = TRUE;
            }
            FmtPuts (TEXT("%s "), v_rgseEx[i].sz);
        }
    }
    if (exstyle) {
            if (!fPrintedExStyle) {
                Indent(iIndent);
                Puts (TEXT("exstyle="));
                fPrintedExStyle = TRUE;
            }
            FmtPuts (TEXT("%08x "), exstyle);
    }
    if (fPrintedExStyle) {
        Puts (TEXT("\r\n"));
    }

    wcf = v_pfnGetWindowCompositionFlags (hwnd);
    fPrintedWCF = FALSE;
    for (i=0; i < sizeof(v_rgseWCF) / sizeof(v_rgseWCF[0]); i++) {
        if (wcf & v_rgseWCF[i].style) {
            wcf &= ~v_rgseWCF[i].style;
            if (!fPrintedWCF) {
                Indent(iIndent);
                Puts (TEXT("WCF="));
                fPrintedWCF = TRUE;
            }
            FmtPuts (TEXT("%s "), v_rgseWCF[i].sz);
        }
    }
    if (wcf) {
            if (!fPrintedWCF) {
                Indent(iIndent);
                Puts (TEXT("WCF="));
                fPrintedWCF = TRUE;
            }
            FmtPuts (TEXT("%08x "), wcf);
    }
    if (fPrintedWCF) {
        Puts (TEXT("\r\n"));
    }

    Puts (TEXT("\r\n"));
}

void WinList (HWND hwnd, int iIndent)
{
    if (hwnd) {
        PrintWindow (hwnd, iIndent);
    }
    hwnd = v_pfnGetWindow (hwnd, GW_CHILD);

    while (hwnd) {
        WinList (hwnd, iIndent + 1);
        hwnd = v_pfnGetWindow (hwnd, GW_HWNDNEXT);
    }
}

BOOL DoWinList (PTSTR cmdline)
{
    TCHAR   szText[100];
    TCHAR   szClass[100];
    GET_FOREGROUND_INFO gfi;

    Puts(TEXT("\r\n\r\n"));
    if (NULL == v_pfnGetWindow) {
        HMODULE hCoreDLL;

        hCoreDLL = (HMODULE)LoadLibrary(TEXT("COREDLL.DLL"));
        if (NULL == hCoreDLL) {
            Puts (TEXT("Unable to LoadLibrary(Coredll.dll)\r\n"));
            return TRUE;
        }

        // Try to find the pointer to GetWindow
        v_pfnGetWindow = (PFN_GetWindow)GetProcAddress (hCoreDLL,
            TEXT("GetWindow"));
        v_pfnGetWindowLong = (PFN_GetWindowLong)GetProcAddress (hCoreDLL,
            TEXT("GetWindowLongW"));
        v_pfnGetWindowText = (PFN_GetWindowText)GetProcAddress (hCoreDLL,
            TEXT("GetWindowTextW"));
        v_pfnGetClassName = (PFN_GetClassName)GetProcAddress (hCoreDLL,
            TEXT("GetClassNameW"));
        v_pfnGetWindowRect = (PFN_GetWindowRect)GetProcAddress (hCoreDLL,
            TEXT("GetWindowRect"));
        v_pfnGetWindowThreadProcessId = (PFN_GetWindowThreadProcessId)GetProcAddress (hCoreDLL,
            TEXT("GetWindowThreadProcessId"));
        v_pfnGetParent = (PFN_GetParent)GetProcAddress (hCoreDLL,
            TEXT("GetParent"));
        v_pfnGetForegroundInfo = (PFN_GetForegroundInfo)GetProcAddress (hCoreDLL,
            TEXT("GetForegroundInfo"));
        v_pfnGetWindowCompositionFlags = (PFN_GetWindowCompositionFlags)GetProcAddress (hCoreDLL,
            TEXT("GetWindowCompositionFlags"));


        if (!v_pfnGetWindow || !v_pfnGetWindowLong || !v_pfnGetWindowText ||
           !v_pfnGetClassName || !v_pfnGetWindowRect || !v_pfnGetWindowThreadProcessId ||
           !v_pfnGetParent || !v_pfnGetForegroundInfo || !v_pfnGetWindowCompositionFlags) {
            // Make sure I try again next time....
            v_pfnGetWindow = NULL;
            Puts (TEXT("Unable to GetProcAddress of something\r\n"));
            FreeLibrary (hCoreDLL);
            Puts (TEXT("Unable to get function pointers, assuming missing components\r\n\r\n"));
            return TRUE;
        }
        // Free the library anyway.  Shell.exe already has coredll loaded
        FreeLibrary (hCoreDLL);
    }

    v_pfnGetForegroundInfo (&gfi);

    Puts (TEXT("\r\nFOREGROUND INFO:\r\n\r\n"));

    if (gfi.hwndActive) {
        v_pfnGetWindowText (gfi.hwndActive, szText, sizeof(szText)/sizeof(TCHAR));
        v_pfnGetClassName (gfi.hwndActive, szClass, sizeof(szClass)/sizeof(TCHAR));
        FmtPuts (TEXT("Active:     0x%X,  %s,  %s\r\n"), (DWORD)gfi.hwndActive,
                 szText, szClass);
    } else {
        Puts (TEXT("Active:     NULL\r\n\r\n"));
    }

    if (gfi.hwndFocus) {
        v_pfnGetWindowText (gfi.hwndFocus, szText, sizeof(szText)/sizeof(TCHAR));
        v_pfnGetClassName (gfi.hwndFocus, szClass, sizeof(szClass)/sizeof(TCHAR));
        FmtPuts (TEXT("Focus:      0x%X,  %s,  %s\r\n"), (DWORD)gfi.hwndFocus,
                 szText, szClass);
    } else {
        Puts (TEXT("Focus:      NULL\r\n"));
    }

    if (gfi.hwndMenu) {
        v_pfnGetWindowText (gfi.hwndMenu, szText, sizeof(szText)/sizeof(TCHAR));
        v_pfnGetClassName (gfi.hwndMenu, szClass, sizeof(szClass)/sizeof(TCHAR));
        FmtPuts (TEXT("Menu:       0x%X,  %s,  %s\r\n"), (DWORD)gfi.hwndMenu,
                 szText, szClass);
    } else {
        Puts (TEXT("Menu:       NULL\r\n"));
    }

    if (gfi.hwndKeyboardDest) {
        v_pfnGetWindowText (gfi.hwndKeyboardDest, szText, sizeof(szText)/sizeof(TCHAR));
        v_pfnGetClassName (gfi.hwndKeyboardDest, szClass, sizeof(szClass)/sizeof(TCHAR));
        FmtPuts (TEXT("KeybdDest:  0x%X,  %s,  %s\r\n"), (DWORD)gfi.hwndKeyboardDest,
                 szText, szClass);
    } else {
        Puts (TEXT("KeybdDest:  NULL\r\n"));
    }
    FmtPuts (TEXT("IME open:   %i\r\n"), gfi.fOpen);
    FmtPuts (TEXT("IME Conv:   0x%X\r\n"), gfi.fdwConversion);
    FmtPuts (TEXT("IME Sent:   0x%X\r\n"), gfi.fdwSentence);

    Puts(TEXT("\r\n\r\n"));

    WinList ((HWND)0, -1);
    Puts(TEXT("\r\n\r\n"));
    return TRUE;
}


//------------------------------------------------------------------------------
// LOG command
//------------------------------------------------------------------------------

typedef enum {
    LOGACT_NONE,        // No action needed
    LOGACT_FLUSH,       // Flush and then discard data from the log
    LOGACT_CLEAR,       // Discard data without flushing
    LOGACT_SETBUFSIZE,  // Set log buffer size
    LOGACT_STOPFLUSH,   // Signal that flush app should stop running
} LogAction;

// Handy for local use
typedef struct {
    TCHAR     szFileName[MAX_PATH];
    LogAction act;
    DWORD     dwBufSize;
} CeLogParams;

BOOL DoLogAction(
    LogAction act,      // Desired action
    DWORD dwParam       // Use varies by action type:
                        //   FLUSH        lpszFileName
                        //   CLEAR        <unused>
                        //   SETBUFSIZE   dwBufSize
                        //   STOPFLUSH    <unused>
    )
{
    static HANDLE  s_hHelperLib = NULL;
    static FARPROC s_pfnFlushOnce = NULL;
    static FARPROC s_pfnSetCeLogBufSize = NULL;
    static FARPROC s_pfnStopFlush = NULL;

    // Hold the handle open forever after we have it
    if (!s_hHelperLib) {
        s_hHelperLib = LoadLibrary(TEXT("ShellCeLog.dll"));
        if (!s_hHelperLib) {
            Puts(TEXT("Unable to open ShellCeLog.dll\r\n"));
            return FALSE;
        }

        // Initialize the function pointers that shell.exe uses
        s_pfnSetCeLogBufSize = GetProcAddress(s_hHelperLib, TEXT("SetCeLogBufSize"));
        s_pfnFlushOnce       = GetProcAddress(s_hHelperLib, TEXT("FlushOnce"));
        s_pfnStopFlush       = GetProcAddress(s_hHelperLib, TEXT("StopFlush"));
    }

    switch (act) {
    case LOGACT_SETBUFSIZE:
        if (s_pfnSetCeLogBufSize) {
            if (s_pfnSetCeLogBufSize(dwParam)) {
                FmtPuts(TEXT("CeLog buffer size set to %ukB in registry.\r\n\r\n"),
                        (dwParam >> 10));
                return TRUE;
            } else if (IsCeLogStatus(CELOGSTATUS_ENABLED_ANY)) {
                // Already running; it won't make a difference
                Puts(TEXT("CeLog is already running, buffer size could not be changed.\r\n\r\n"));
            }
        }
        break;

    case LOGACT_STOPFLUSH:
        // celogflush and oscapture exit on request
        if (s_pfnStopFlush) {
            if (s_pfnStopFlush ()) {
                Puts(TEXT("Signalled flush app to exit.\r\n"));
                return TRUE;
            } else {
                Puts(TEXT("Flush app is not running.\r\n"));
            }
        }
        break;

    case LOGACT_FLUSH:
    case LOGACT_CLEAR:
        if (s_pfnFlushOnce) {
            // FlushOnce always clears but it may just discard instead of flushing
            return s_pfnFlushOnce((act == LOGACT_FLUSH) ? ((LPWSTR) dwParam) : NULL);
        }
        break;

    default:
        break;
    }

    return FALSE;
}

VOID DoLogHelp()
{
    Puts(TEXT("Syntax:   log [log_options]  [[CE zone] [user zone] [process mask]]\r\n"));

    Puts(TEXT("log_options:\r\n"));
    Puts(TEXT("    -buf <size>            Set size of CeLog data buffer, if CeLog is\r\n"));
    Puts(TEXT("                           not yet running (hex or decimal)\r\n"));
    Puts(TEXT("    -clear                 Discard CeLog data buffer contents\r\n"));
    Puts(TEXT("    -flush [filename.clg]  Flush buffered data to log file\r\n"));
    Puts(TEXT("                           (default file \\Release\\celog.clg)\r\n"));
    Puts(TEXT("    -stopflush             Signal flush application to exit\r\n"));

    Puts(TEXT("Zones:  CeLog zones (hex or decimal).  Omitted zones default to 0xFFFFFFFF.\r\n"));

    Puts(TEXT("\r\nExamples:\r\n"));
    Puts(TEXT("    log 0x63 0xFFFFFFFF 0xFFFFFFFF    Get data from zone 0x63\r\n"));
    Puts(TEXT("    log 0x63                          (same as previous)\r\n"));
    Puts(TEXT("    log -buf 0x00100000               Set CeLog buffer size to 1MB without\r\n"));
    Puts(TEXT("                                      changing zones\r\n"));
    Puts(TEXT("    log -buf 1048576                  (same as previous)\r\n"));
    Puts(TEXT("    log -buf 1048576 0x63             Set buffer size to 1MB and set zones\r\n"));
    Puts(TEXT("    log -flush \\release\\myfile.clg    Flush CeLog buffer contents to file\r\n\r\n"));
}


BOOL DoLog(PTSTR cmdline)
{
    PTSTR szToken;
    DWORD rgdwZones[4];   // 0=CE, 1=user, 2=process, 3=available
    int   nCurZone;
    CeLogParams celog;

    memset(rgdwZones, 0xFF, sizeof(rgdwZones));  // default to all on
    nCurZone = 0;

    celog.act = LOGACT_NONE;
    celog.dwBufSize = 0;
    celog.szFileName[0] = 0;

    if (!cmdline) {
        DoLogHelp();
    } else {
        szToken = _tcstok(cmdline, TEXT(" \t"));
        while (szToken && (szToken[0])) {
            if (!_tcscmp(szToken, TEXT("-buf"))) {
                // Buffer size must follow
                szToken = _tcstok(NULL, TEXT(" \t"));
                if (!DwordArg(&szToken, &celog.dwBufSize)) {
                    DoLogHelp();
                    break;
                }

                // Set the buffer size in the registry
                DoLogAction(LOGACT_SETBUFSIZE, celog.dwBufSize);

            } else if (!_tcscmp(szToken, TEXT("-clear"))) {
                if (celog.act == LOGACT_NONE) {  // Stopflush & flush take precedence
                    celog.act = LOGACT_CLEAR;
                }

            } else if (!_tcscmp(szToken, TEXT("-flush"))) {
                PCTSTR pNextTok;

                if (!IsCeLogStatus(CELOGSTATUS_ENABLED_ANY)) {
                    FmtPuts (TEXT("CeLog is not loaded, no data to flush!\r\n\r\n"));
                    return TRUE;
                }

                if (celog.act != LOGACT_STOPFLUSH) {  // Stopflush takes precedence
                    celog.act = LOGACT_FLUSH;
                }

                // Filename is optional
                pNextTok = _tcstok(NULL, TEXT("\""));
                if (pNextTok) {
                    _tcsncpy(celog.szFileName, pNextTok, MAX_PATH);
                    celog.szFileName[MAX_PATH-1] = 0;
                } else {
                    _tcscpy(celog.szFileName, TEXT("\\release\\celog.clg"));
                }


            } else if (!_tcscmp(szToken, TEXT("-stopflush"))) {
                celog.act = LOGACT_STOPFLUSH;

            } else if (DwordArg(&szToken, &rgdwZones[nCurZone])) {
                nCurZone++;
            } else {
                DoLogHelp();
                break;
            }

            szToken = _tcstok(NULL, TEXT(" \t"));
        }
    }

    // Load CeLog if necessary
    if (((nCurZone != 0) || (celog.act != LOGACT_NONE))
        && !IsCeLogStatus(CELOGSTATUS_ENABLED_ANY) && !LoadCeLog(TRUE)) {
        return TRUE;
    }

    // Perform log clear/flush
    if ((celog.act != LOGACT_NONE)
        && !DoLogAction(celog.act, (DWORD) celog.szFileName)) {
        return TRUE;
    }

    // Set zones if any were read in
    if (nCurZone != 0) {
        CeLogSetZones(rgdwZones[1], rgdwZones[0], rgdwZones[2]);  // note order
        CeLogReSync();
    }

    // Print zones if they were just set or if the user was getting info
    if ((nCurZone != 0) || ((celog.dwBufSize == 0) && (celog.act == LOGACT_NONE))) {
        CeLogGetZones(&rgdwZones[1], &rgdwZones[0], &rgdwZones[2], &rgdwZones[3]);  // note order
        if (IsCeLogStatus(CELOGSTATUS_ENABLED_ANY)) {
            FmtPuts(TEXT("\r\nCurrent CeLog zones:  CE=0x%08X User=0x%08X Process=0x%08X\r\n"),
                    rgdwZones[0], rgdwZones[1], rgdwZones[2]);
        }
        FmtPuts(TEXT("Available zones:  0x%08X\r\n\r\n"), rgdwZones[3]);
    }

    return TRUE;
}

//------------------------------------------------------------------------------
// MEMTRACK command
//------------------------------------------------------------------------------

BOOL DoMemTrack(PTSTR cmdline)
{
    Puts(TEXT("memtrack is no longer supported. Use Application Verifier to track memory.\r\n\r\n"));
    return TRUE;
}


//------------------------------------------------------------------------------


DWORD
GetProcessIdFromName(
    PCTSTR szName
    )
{
    HANDLE hSnap;
    PROCESSENTRY32 proc;
    DWORD dwRet;


    hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS | TH32CS_SNAPNOHEAPS ,0);
    dwRet = 0xffffffff;
    if (INVALID_HANDLE_VALUE != hSnap) {
        proc.dwSize = sizeof(proc);
        if (Process32First(hSnap,&proc)) {
            do {
                if (IsNameMatch(szName, proc.szExeFile)) {
                    dwRet = proc.th32ProcessID;
                    break;
                }
            } while (Process32Next(hSnap,&proc));
        }
        CloseToolhelp32Snapshot(hSnap);
    }
    return dwRet;
}

BOOL
GetProcModOffset (DWORD dwAddr, LPTSTR szName, DWORD dwNameLen, LPDWORD pdwOffset)
{
    HANDLE hSnap;
    PROCESSENTRY32 proc;
    MODULEENTRY32 mod;
    BOOL fRet = FALSE;

#define GETSLOT(a) ((((DWORD)a)&0x3E000000)>>25)
#define MASKSLOT(a) (((DWORD)a)&0x01FFFFFF)

    if ((dwNameLen > MAX_PATH) || (dwNameLen < 1)) {
        return fRet;
    }

    szName[0] = TEXT('\0');
    *pdwOffset = dwAddr;

    hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_GETALLMODS,0);
    if (INVALID_HANDLE_VALUE != hSnap) {
        mod.dwSize = sizeof(mod);
        if (Module32First(hSnap,&mod)) {
            do {
                // First check for matching slot addresses (only slot 1?)
                if ((GETSLOT(dwAddr) == GETSLOT(mod.modBaseAddr)) &&
                    (MASKSLOT(dwAddr) > MASKSLOT(mod.modBaseAddr)))  {
                    if ((MASKSLOT(dwAddr) - MASKSLOT(mod.modBaseAddr)) < *pdwOffset) {
                        *pdwOffset = MASKSLOT(dwAddr) - MASKSLOT(mod.modBaseAddr);
                        _tcsncpy (szName, mod.szModule, dwNameLen);
                    }
                } else if ((1 != GETSLOT(dwAddr)) && (1 != GETSLOT(mod.modBaseAddr))) {
                    if ((MASKSLOT(dwAddr) - MASKSLOT(mod.modBaseAddr)) < *pdwOffset) {
                        *pdwOffset = MASKSLOT(dwAddr) - MASKSLOT(mod.modBaseAddr);
                        _tcsncpy (szName, mod.szModule, dwNameLen);
                    }
                }
            } while (Module32Next(hSnap, &mod));
        }
        CloseToolhelp32Snapshot(hSnap);
    }

    if (TEXT('\0') == szName[0]) {
        hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS | TH32CS_SNAPNOHEAPS ,0);
        if (INVALID_HANDLE_VALUE != hSnap) {
            proc.dwSize = sizeof(proc);
            if (Process32First(hSnap,&proc)) {
                do {
                    if ((dwAddr - proc.th32MemoryBase) < *pdwOffset) {
                        *pdwOffset = dwAddr - proc.th32MemoryBase;
                        _tcsncpy (szName, proc.szExeFile, dwNameLen);
                    }
                } while (Process32Next(hSnap,&proc));
            }
            CloseToolhelp32Snapshot(hSnap);
        }
    }

    if (TEXT('\0') == szName[0]) {
        fRet = TRUE;
    }
    szName[dwNameLen-1] = TEXT('\0');

    return fRet;
}


BOOL
DoHeapDump(
    PTSTR cmdline
    )
{
    TCHAR *token;
    DWORD dwProc32 = (DWORD)-1, dwProcID = 0;
    HANDLE hSnap;
    HEAPLIST32 hl;
    HEAPENTRY32 he;
    DWORD dwHeapBytes;
    DWORD dwHeapAllocs;
    DWORD dwTotalBytes;
    DWORD dwTotalAllocs;
    BOOL bIncludeFreed = FALSE;
    BOOL IsProcID = TRUE;
    PPROC_DATA pProcData;
    UINT32 i;

    if (cmdline == NULL) {
        goto dhd_syntax;
    }

    if ((token = _tcstok(cmdline, TEXT(" \t"))) == NULL) {
        goto dhd_syntax;
    }

    IsProcID = DwordArg(&token, &dwProcID); // First treat the argument as an ID and see if we come back with something
    if (IsProcID)
    {
        for (i = 0, pProcData = v_pProcData; (i < dwProcID) && (pProcData != NULL); i++,pProcData = pProcData->pNext) 
        {
        }
    
        if (NULL != pProcData) 
        {
            dwProc32 = pProcData->dwProcid;
        }
    }
    else
        dwProc32 = GetProcessIdFromName(token);
   
    if (dwProc32 == 0xffffffff) {
        Puts(TEXT("Process not found\r\n\r\n"));
        return TRUE;
    }

    token = _tcstok(NULL, TEXT(" \t"));
    if(token != NULL) {
        if (0 == _tcscmp(TEXT("freed"), token)) {
            bIncludeFreed = TRUE;
        }
    }

    dwTotalBytes = 0;
    dwTotalAllocs = 0;
    hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPHEAPLIST, dwProc32);
    if (INVALID_HANDLE_VALUE != hSnap) {
        hl.dwSize = sizeof(HEAPLIST32);
        if (Heap32ListFirst(hSnap, &hl)) {
            do {
                he.dwSize = sizeof(HEAPENTRY32);
                if (Heap32First(hSnap, &he, dwProc32, hl.th32HeapID)) {

                    dwHeapBytes = 0;
                    dwHeapAllocs = 0;

                    do {
                        if ((FALSE == bIncludeFreed) && (he.dwFlags & LF32_FREE)) {
                            continue;
                        }
                        FmtPuts (TEXT("Process 0x%08x Heap 0x%08x: %5d bytes at 0x%08x %s\r\n"),
                                 he.th32ProcessID, he.th32HeapID, he.dwBlockSize, he.dwAddress,
                                 he.dwFlags & LF32_FREE ? L"(free)" : L"");
                        dwHeapBytes += he.dwBlockSize;
                        dwHeapAllocs++;
                        he.dwSize = sizeof(HEAPENTRY32);
                    } while (Heap32Next(hSnap, &he));

                    FmtPuts (TEXT("Process 0x%08x Heap 0x%08x: %5d total bytes in %5d allocations\r\n\r\n"),
                             he.th32ProcessID, he.th32HeapID, dwHeapBytes, dwHeapAllocs);

                    dwTotalBytes  += dwHeapBytes;
                    dwTotalAllocs += dwHeapAllocs;
                }


                hl.dwSize = sizeof(HEAPLIST32);
            } while (Heap32ListNext(hSnap, &hl));
            FmtPuts (TEXT("Process 0x%08x: Total %5d bytes in %d allocations\r\n"),
                     hl.th32ProcessID, dwTotalBytes, dwTotalAllocs);
        } else {
            if (ERROR_NO_MORE_FILES==GetLastError())
                Puts(TEXT("No heap list exists for this process.\r\n\r\n"));
            else
                Puts(TEXT("Heap32ListFirst failed\r\n"));
        }
        CloseToolhelp32Snapshot(hSnap);
        return TRUE;
    } else {
        Puts(TEXT("Heap snapshot failed for Process\r\n\r\n"));
        return TRUE;
    }

dhd_syntax:
    Puts(TEXT("Syntax: hd <procid> or hd <procname> \r\n\r\n"));
    return TRUE;
}

BOOL
DoOptions(
    PTSTR cmdline
    )
{
    TCHAR   *token;
    DWORD   dwPri;

    if ((NULL == cmdline) || (TEXT('\0') == *cmdline)) {
        Puts (TEXT("Options:\r\n"));
        FmtPuts (TEXT("     Timestamp : %s\r\n"), v_fTimestamp ? TEXT("On") : TEXT("Off"));
        Puts (TEXT("\r\n"));
        return TRUE;
    }
    for (token = _tcstok(cmdline, TEXT(" \t")); token; token=_tcstok(NULL, TEXT(" \t"))) {
        if (!_tcsicmp(token, TEXT("timestamp"))) {
            // Toggle timestamp flag
            v_fTimestamp ^= 1;
            FmtPuts (TEXT("Timestamp : %s\r\n"), v_fTimestamp ? TEXT("On") : TEXT("Off"));
        } else if (!_tcsicmp(token, TEXT("priority"))) {
            token = _tcstok (NULL, TEXT(" \t"));
            if (token) {
                if (DwordArg (&token, &dwPri) && (dwPri < MAX_CE_PRIORITY_LEVELS)) {
                    CeSetThreadPriority ((HANDLE)GetCurrentThreadId(), dwPri);
                    FmtPuts (TEXT("Priority set to %d\r\n"), dwPri);
                } else {
                    Puts (TEXT("Invalid priority value\r\n"));
                }
            } else {
                dwPri = CeGetThreadPriority ((HANDLE)GetCurrentThreadId());
                FmtPuts (TEXT("Current priority=%d\r\n"), dwPri);
            }
        } else if (!_tcsicmp(token, TEXT("kernfault"))) {
            KLibSetIgnoreNoFault (TRUE);
        } else if (!_tcsicmp(token, TEXT("kernnofault"))) {
            KLibSetIgnoreNoFault (FALSE);
        } else {
            FmtPuts (TEXT("Unexpected option '%s'\r\n"), token);
        }
    }
    return TRUE;
}

#define LMEM_OPT_HIST   0x0001
#define LMEM_OPT_RECENT 0x0002
#define LMEM_OPT_CHK    0x0004
#define LMEM_OPT_DELTA  0x0008
#define LMEM_OPT_INFO   0x0010
#define LMEM_OPT_CSON   0x0020
#define LMEM_OPT_CSOFF  0x0040
#define LMEM_OPT_MAP    0x0080

void PrintHex (DWORD dwOffset, const BYTE *pBytes, DWORD dwLen)
{
    TCHAR szOutStr[MAX_PATH];
    DWORD dwStartOffset, i, j;

    for (dwStartOffset = 0; dwStartOffset < dwLen; dwStartOffset += 16) {
        StringCbPrintf (szOutStr, sizeof(szOutStr), TEXT("%*.*s0x%08X "), dwOffset, dwOffset, TEXT(""), dwStartOffset);
        i = _tcslen(szOutStr);

        // Print them out in HEX
        for (j=0; j < 16; j++) {
            if ((dwStartOffset + j) < dwLen) {
#define HEX2CHAR(x) (((x) < 0x0a) ? (x) + '0' : ((x)-0xa) + 'A')
                szOutStr[i++] = HEX2CHAR(pBytes[dwStartOffset+j]>>4);
                szOutStr[i++] = HEX2CHAR(pBytes[dwStartOffset+j]&0x0F);
            } else {
                szOutStr[i++] = TEXT(' ');
                szOutStr[i++] = TEXT(' ');
            }
            szOutStr[i++] = TEXT(' ');
            if (7 == j) {
                szOutStr[i++] = TEXT('-');
                szOutStr[i++] = TEXT(' ');
            }

        }

        // A little space
        for (j=0; j < 5; j++) {
            szOutStr[i++] = TEXT(' ');
        }
        szOutStr[i++] = TEXT(':');

        for (j=0; j < 16; j++) {
            if ((dwStartOffset + j) < dwLen) {
                if ((pBytes[dwStartOffset+j] < ' ') || (pBytes[dwStartOffset+j] >= 0x7f)) {
                    szOutStr[i++] = TEXT('.');
                } else {
                    szOutStr[i++] = pBytes[dwStartOffset+j];
                }
            } else {
                szOutStr[i++] = TEXT(' ');
            }
        }
        szOutStr[i++] = TEXT(':');
        szOutStr[i++] = TEXT('\r');
        szOutStr[i++] = TEXT('\n');
        szOutStr[i++] = TEXT('\0');
        Puts (szOutStr);
    }

}


BOOL DoSuspend(PTSTR cmdline)
{
    typedef void (*PFN_GwesPowerOffSystem) (void);
    PFN_GwesPowerOffSystem pfnPowerOff;
    HMODULE hCoreDLL;

    hCoreDLL = (HMODULE)LoadLibrary(TEXT("COREDLL.DLL"));
    if (NULL == hCoreDLL) {
        Puts (TEXT("Unable to LoadLibrary(Coredll.dll)\r\n"));
        return TRUE;
    }
    if (NULL == (pfnPowerOff = (PFN_GwesPowerOffSystem)GetProcAddress(hCoreDLL, TEXT("GwesPowerOffSystem")))) {
        Puts (TEXT("Unable to find GwesPowerOffSystem() api in coredll.dll\r\n"));
        FreeLibrary(hCoreDLL);
        return TRUE;
    }

    // Just suspend.
    pfnPowerOff();
    FreeLibrary(hCoreDLL);
    return TRUE;

}


//------------------------------------------------------------------------------
// PROF command
//------------------------------------------------------------------------------

static VOID DoProfileHelp()
{
    Puts(TEXT("Syntax: prof <on|off> [data_type] [storage_type] [other_options]\r\n"));
    
    Puts(TEXT("data_type, one of: -m or -s or -k\r\n"));
    Puts(TEXT("    -m: Gather Monte Carlo data (time-based sampling) (DEFAULT)\r\n"));
    Puts(TEXT("    -s: Gather System Call data (call-based sampling)\r\n"));
    Puts(TEXT("    -k: Gather Kernel Call data (call-based sampling)\r\n"));

    Puts(TEXT("storage_type, one of: -b or -u or -l\r\n"));
    Puts(TEXT("    -b: Buffered (DEFAULT)\r\n"));
    Puts(TEXT("    -u: Unbuffered\r\n"));
    Puts(TEXT("    -l: Send data to CeLog, additional options available:\r\n"));
    
    Puts(TEXT("other_options:\r\n"));
    Puts(TEXT("    -i <interval>          Set the sampling interval, in microseconds\r\n"));
    Puts(TEXT("                           (hex or decimal)\r\n"));
    Puts(TEXT("    -t                     Profile without using OAL profiler support\r\n"));
    Puts(TEXT("                           by sampling on tick interrupts (SYSINTR_NOP)\r\n"));
    Puts(TEXT("The following options are only available when using CeLog (-l):\r\n"));
    Puts(TEXT("    -buf <size>            Set size of CeLog data buffer, if CeLog is\r\n"));
    Puts(TEXT("                           not yet running (hex or decimal)\r\n"));
    Puts(TEXT("    -clear                 (\"prof on\" only) Clear CeLog buffer, then\r\n"));
    Puts(TEXT("                           start profiler\r\n"));
    Puts(TEXT("    -flush [filename.clg]  (\"prof off\" only) Stop profiler, then\r\n"));
    Puts(TEXT("                           flush CeLog buffer to log file\r\n"));
    Puts(TEXT("                           (default file \\Release\\celog.clg)\r\n"));
#if 0  // Profiler callstack option is unsupported right now, so hide it
    Puts(TEXT("    -stack [inproc]        Capture callstacks while profiling.  If\r\n"));
    Puts(TEXT("                           inproc flag is specified, only saves the\r\n"));
    Puts(TEXT("                           stack within the thread's owner process.\r\n"));
    Puts(TEXT("                           (No PSL boundary crossing.)\r\n"));
#endif

    Puts(TEXT("\r\nExamples:\r\n"));
    Puts(TEXT("    prof on                   Start profiler, Monte Carlo data,\r\n"));
    Puts(TEXT("                              buffered mode\r\n"));
    Puts(TEXT("    prof on -s -u             Start profiler, SysCall data,\r\n"));
    Puts(TEXT("                              unbuffered mode\r\n"));
    Puts(TEXT("    prof off                  Stop profiler\r\n\r\n"));
    Puts(TEXT("    prof on -l                Start profiler, Monte Carlo data, CeLog mode\r\n"));
    Puts(TEXT("    prof on -l -buf 0x100000  Start profiler, Monte Carlo data, CeLog mode\r\n"));
    Puts(TEXT("                              with 1MB CeLog buffer\r\n"));
    Puts(TEXT("    prof on -l -buf 1048576   (same as previous)\r\n"));
    Puts(TEXT("    prof on -l -clear         Clear CeLog buffer, then start profiler,\r\n"));
    Puts(TEXT("                              Monte Carlo data in CeLog mode\r\n"));
    Puts(TEXT("    prof off -flush           Stop profiler and flush CeLog buffer to file\r\n"));
#if 0
    Puts(TEXT("    prof on -l -stack         Start profiler, Monte Carlo data, CeLog mode\r\n"));
    Puts(TEXT("                              with callstacks\r\n"));
#endif
    Puts(TEXT("\r\n"));
}


BOOL DoProfile(PTSTR cmdline)
{
    PTSTR szToken;
    WORD  wAction;  // 0=error 1=on 2=off
    DWORD dwProfilerFlags = PROFILE_BUFFER;
    DWORD dwProfilerInterval  = 200;
    CeLogParams celog;
    static BOOL s_fLogging = FALSE;

    celog.act = LOGACT_NONE;
    celog.dwBufSize = 0;
    celog.szFileName[0] = 0;

    if(cmdline == NULL) {
        DoProfileHelp();
        return TRUE;
    }
    szToken = _tcstok(cmdline, TEXT(" \t"));
    if(szToken == NULL) {
        DoProfileHelp();
        return TRUE;
    }
    if(_tcsicmp(szToken, TEXT("on")) == 0) {
        wAction = 1;
    } else if(_tcsicmp(szToken, TEXT("off")) == 0) {
        wAction = 2;
    } else {
        DoProfileHelp();
        return TRUE;
    }

    if (wAction == 1) {
        // PROFILE ON

        for (szToken = _tcstok(NULL, TEXT(" \t")); szToken; szToken = _tcstok(NULL, TEXT(" \t"))) {
            if (!_tcscmp(szToken, TEXT("-m"))) {
                dwProfilerFlags &= ~(PROFILE_OBJCALL | PROFILE_KCALL);
            } else if (!_tcscmp(szToken, TEXT("-s"))) {
                dwProfilerFlags |= PROFILE_OBJCALL;
                dwProfilerFlags &= ~PROFILE_KCALL;
            } else if (!_tcscmp(szToken, TEXT("-k"))) {
                dwProfilerFlags |= PROFILE_KCALL;
                dwProfilerFlags &= ~PROFILE_OBJCALL;

            } else if (!_tcscmp(szToken, TEXT("-b"))) {
                dwProfilerFlags |= PROFILE_BUFFER;
                dwProfilerFlags &= ~PROFILE_CELOG;
                dwProfilerInterval = 200;
            } else if (!_tcscmp(szToken, TEXT("-u"))) {
                dwProfilerFlags &= ~(PROFILE_BUFFER | PROFILE_CELOG);
                dwProfilerInterval = 1000;
            } else if (!_tcscmp(szToken, TEXT("-l"))) {
                dwProfilerFlags |= PROFILE_CELOG;
                dwProfilerFlags &= ~PROFILE_BUFFER;
                dwProfilerInterval = 200;

            } else if (!_tcscmp(szToken, TEXT("-i"))) {
                // Profile interval must follow
                szToken = _tcstok(NULL, TEXT(" \t"));
                if (!DwordArg(&szToken, &dwProfilerInterval)) {
                    DoProfileHelp();
                    return TRUE;
                }
            } else if (!_tcscmp(szToken, TEXT("-t"))) {
                dwProfilerFlags |= PROFILE_TICK;

            } else if (!_tcscmp(szToken, TEXT("-buf"))) {
                // Buffer size must follow
                szToken = _tcstok(NULL, TEXT(" \t"));
                if (!DwordArg(&szToken, &celog.dwBufSize)) {
                    DoProfileHelp();
                    return TRUE;
                }
            } else if (!_tcscmp(szToken, TEXT("-clear"))) {
                celog.act = LOGACT_CLEAR;
            } else if (!_tcscmp(szToken, TEXT("-stack"))) {
                dwProfilerFlags |= PROFILE_CELOG | PROFILE_CALLSTACK;
                dwProfilerFlags &= ~PROFILE_BUFFER;
                dwProfilerInterval = 200;
                // "inproc" flag may follow...
            } else if (!_tcscmp(szToken, TEXT("inproc"))) {
                // Only valid if after stack flag
                if (dwProfilerFlags & PROFILE_CALLSTACK) {
                    dwProfilerFlags |= PROFILE_CALLSTACK_INPROC;
                }

            } else {
                DoProfileHelp();
                return TRUE;
            }
        }

        if (dwProfilerFlags & PROFILE_CELOG) {
            // Set the buffer size in the registry
            if (celog.dwBufSize) {
                DoLogAction(LOGACT_SETBUFSIZE, celog.dwBufSize);
            }

            // Load CeLog if necessary
            if (!IsCeLogStatus(CELOGSTATUS_ENABLED_ANY)
                && !LoadCeLog(TRUE)) {
                return TRUE;
            }

            if ((celog.act != LOGACT_NONE)
                && !DoLogAction(celog.act, (DWORD) celog.szFileName)) {
                return TRUE;
            }

            s_fLogging = TRUE;

        } else if (celog.dwBufSize != 0) {
            Puts(TEXT("Buffer size ignored, only used with CeLog\r\n"));
        } else if (celog.act != LOGACT_NONE) {
            Puts(TEXT("\"-clear\" option ignored, only used with CeLog\r\n"));
        }

        ProfileStart(dwProfilerInterval, dwProfilerFlags);

    } else {
        // PROFILE OFF
        ProfileStop();

        // Flush log if necessary
        if (s_fLogging) {
            szToken = _tcstok(NULL, TEXT(" \t"));
            if(szToken != NULL && !_tcscmp(szToken, TEXT("-flush"))) {
                // Filename is optional
                PCTSTR pNextTok = _tcstok(NULL, TEXT("\""));
                if (pNextTok) {
                    _tcsncpy(celog.szFileName, pNextTok, MAX_PATH);
                    celog.szFileName[MAX_PATH-1] = 0;
                } else {
                    _tcscpy(celog.szFileName, TEXT("\\release\\celog.clg"));
                }
                celog.act = LOGACT_FLUSH;
            }
        }

        if ((celog.act != LOGACT_NONE)
            && !DoLogAction(celog.act, (DWORD) celog.szFileName)) {
            return TRUE;
        }

        s_fLogging = FALSE;
    }

    return TRUE;
}

static VOID DoLoadExtHelp()
{
    Puts(TEXT("loadext [[-u] DLLName] : Load shell.exe extension dll\r\n"));
    Puts(TEXT("    -u : unload extension dll\r\n"));
    Puts(TEXT("    DLLName : name of DLL to load (i.e. shellext.dll)\r\n"));
}

BOOL DoLoadExt (PTSTR cmdline)
{
    PSHELL_EXTENSION    pShellExt, pShellExtPrev;
    PCTSTR              szToken;

    if (!cmdline) {
        DoLoadExtHelp();
        return TRUE;
    }

    for(;;) {
        szToken = _tcstok(cmdline, TEXT(" \t"));
        if(szToken == NULL) {
            break;
        }
        cmdline = NULL; // Set to null for second time through

        if (!_tcscmp (szToken, TEXT("-u"))) {
            // Get the DLL Name
            szToken = _tcstok(cmdline, TEXT(" \t"));
            if(szToken != NULL) {
                pShellExtPrev = NULL;
                for (pShellExt = v_ShellExtensions; NULL != pShellExt; pShellExt = pShellExt->pNext) {
                    if (!_tcsicmp(pShellExt->szDLLName, szToken)) {
                        // First remove it from the list.
                        if (pShellExtPrev) {
                            pShellExtPrev->pNext = pShellExt->pNext;
                        } else {
                            v_ShellExtensions = pShellExt->pNext;
                        }
                        // Make it all go away
                        LocalFree (pShellExt->szDLLName);
                        LocalFree (pShellExt->szName);
                        FreeLibrary (pShellExt->hModule);
                        LocalFree (pShellExt);
                        break;
                    }
                    pShellExtPrev = pShellExt;
                }
            }
        } else {
            LoadExt (szToken, szToken);
        }
    }

    for (pShellExt = v_ShellExtensions; NULL != pShellExt; pShellExt = pShellExt->pNext) {
        FmtPuts (TEXT("  DLL=%s, Name=%s\r\n"), pShellExt->szDLLName, pShellExt->szName);
    }

    return TRUE;
}

static DWORD GetThrdIdFromEvt (LPCWSTR pszName)
{
    HANDLE hEvt = OpenEvent (EVENT_ALL_ACCESS, FALSE, pszName);
    DWORD  dwThrdId = 0;
    if (hEvt) {
        dwThrdId = GetEventData (hEvt);
        CloseHandle (hEvt);
    }
    return dwThrdId;
}

BOOL DoThreadPrio (PTSTR cmdline)
{
    PCTSTR szToken;
    DWORD  dwThrdId = 0;
    // nPriority = -1 is to query the current thread priority
    // nPriority = -2 is an error in the user input
    // nPriority = 0-255, valid priority
    INT nPriority = -1;
    BOOL fRet;

    for(;;) {
        szToken = _tcstok (cmdline, TEXT(" \t"));
        if(szToken == NULL) {
            break;
        }
        cmdline = NULL; // Set to null for second time through

        if (!_tcscmp (szToken, TEXT("kitlintr"))) {
            // Special token for kitl interrupt thread
            dwThrdId = g_pKData->dwIdKitlIST;
        }
        else if (!_tcscmp (szToken, TEXT("kitltimer"))) {
            // Special token for kitl timer thread
            dwThrdId = g_pKData->dwIdKitlTimer;
        }
        else if (!dwThrdId) {
            // This token must be the thread id
            dwThrdId = _wtol (szToken);
        }
        else {
            // This token must be the new thread priority
            nPriority = _wtoi (szToken);
            // _wtoi returns 0 on error so check if szToken is actually zero 
            // or if this is an error; error if len > 1 and wtoi returns 0
            if (nPriority == 0 && (szToken[0] != _T('0') || _tcslen(szToken) > 1))
                nPriority = -2;
        }
    }

    if (dwThrdId) {
        if (nPriority <= -2 || nPriority >= MAX_CE_PRIORITY_LEVELS)
        {
            FmtPuts (TEXT("Invalid priority for Thread id 0x%8.8lx.\r\n"), dwThrdId);
        } else {
            HANDLE hThread = OpenThread (THREAD_ALL_ACCESS, FALSE, dwThrdId);
  
            if (hThread) {
                if (nPriority >= 0) {
                    fRet = CeSetThreadPriority (hThread, nPriority);
                    FmtPuts (TEXT("Thread 0x%08x priority --> %d %s\r\n"), hThread, nPriority, fRet ? _T("OK") : _T("FAILED"));
                } 
                else {
                    FmtPuts (TEXT("Thread 0x%08x current priority: %d\r\n"), hThread, CeGetThreadPriority (hThread));                
                }
                
                CloseHandle(hThread);
            } else {
                FmtPuts (TEXT("Invalid Thread id 0x%8.8lx\r\n"), dwThrdId);
            }
        }
    } else {
        FmtPuts (TEXT("Invalid parameter. ThreadId: 0x%08x, Priority: %d\r\n"), dwThrdId, nPriority);
    }


    return TRUE;
}


BOOL DoFindFile (PCTSTR cmdline)
{
    WIN32_FIND_DATA wfd = {0};
    HANDLE hFind = INVALID_HANDLE_VALUE;

    // get the file info
    hFind = FindFirstFile(cmdline, &wfd);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        // return the file info
        StringCbPrintf(rgbText, ccbrgbText, L"Attributes    FileSize(b)   CreateTime(L) CreateTime(H) AccessTime(L) AccessTime(H) WriteTime(L)  WriteTime(H)\r\n"
                          L"0x%8.8lx    0x%8.8lx    0x%8.8lx    0x%8.8lx    0x%8.8lx    0x%8.8lx    0x%8.8lx    0x%8.8lx\r\n",
                          wfd.dwFileAttributes, ((wfd.nFileSizeHigh * MAXDWORD) + wfd.nFileSizeLow),
              wfd.ftCreationTime.dwLowDateTime, wfd.ftCreationTime.dwHighDateTime,
              wfd.ftLastAccessTime.dwLowDateTime, wfd.ftLastAccessTime.dwHighDateTime,
              wfd.ftLastWriteTime.dwLowDateTime, wfd.ftLastWriteTime.dwHighDateTime);
        FindClose( hFind);
    }
    else
    {
    // file not found
        StringCbPrintf(rgbText, ccbrgbText, L"Attributes    FileSize(b)   CreateTime(L) CreateTime(H) AccessTime(L) AccessTime(H) WriteTime(L)  WriteTime(H)\r\n"
              L"0x%8.8lx    0x%8.8lx    0x%8.8lx    0x%8.8lx    0x%8.8lx    0x%8.8lx    0x%8.8lx    0x%8.8lx\r\n",
              (DWORD) -1, 0, 0, 0, 0, 0, 0, 0);
    }
    Puts(rgbText);

    return TRUE;
}

// HANDLE FindFirstDevice(DeviceSearchType searchType, LPCVOID pvSearchParam, __out PDEVMGR_DEVICE_INFORMATION pdi);
// BOOL FindNextDevice(HANDLE h, __out PDEVMGR_DEVICE_INFORMATION pdi);
// WINBASEAPI BOOL WINAPI FindClose(HANDLE hFindFile);

typedef HANDLE  (WINAPI *PFN_FindFirstDevice) (DeviceSearchType searchType, LPCVOID pvSearchParam, __out PDEVMGR_DEVICE_INFORMATION pdi);
typedef BOOL (WINAPI *PFN_FindNextDevice) (HANDLE h, __out PDEVMGR_DEVICE_INFORMATION pdi);
typedef BOOL (WINAPI *PFN_FindClose) (HANDLE hFindFile);

PFN_FindFirstDevice v_pfnFindFirstDevice;
PFN_FindNextDevice v_pfnFindNextDevice;
PFN_FindClose v_pfnFindClose;


BOOL DoDeviceInfo (PCTSTR cmdline) {
    HANDLE hSearchDevice;
    DEVMGR_DEVICE_INFORMATION di;
    BOOL fRet;

    if (NULL == v_pfnFindFirstDevice) {
        HMODULE hCoreDLL;

        hCoreDLL = (HMODULE)LoadLibrary(TEXT("COREDLL.DLL"));
        if (NULL == hCoreDLL) {
            Puts (TEXT("Unable to LoadLibrary(Coredll.dll)\r\n"));
            return TRUE;
        }
        v_pfnFindFirstDevice = (PFN_FindFirstDevice)GetProcAddress(hCoreDLL, TEXT("FindFirstDevice"));
        v_pfnFindNextDevice = (PFN_FindNextDevice)GetProcAddress(hCoreDLL, TEXT("FindNextDevice"));
        v_pfnFindClose = (PFN_FindClose)GetProcAddress(hCoreDLL, TEXT("FindClose"));
        
        // Free the library anyway.  Shell.exe already has coredll loaded
        FreeLibrary (hCoreDLL);

        if ((NULL == v_pfnFindFirstDevice) || (NULL == v_pfnFindNextDevice) ||
           (NULL == v_pfnFindClose)) {
            Puts (TEXT("Unable to GetProcAddress of FindFirstDevice, FindNextDevice or FindClose\r\n"));
            return TRUE;
        }
    }
    
    memset (&di, 0, sizeof(di));
    di.dwSize = sizeof(di);
    FmtPuts(L"%-10s %-10s %-5s %s.%s.%s\r\n",
            L"DevHand", L"Parent", L"Legcy", L"Name", L"Bus", L"Key");
    hSearchDevice = v_pfnFindFirstDevice (DeviceSearchByBusName, L"*", &di);
    if (hSearchDevice != INVALID_HANDLE_VALUE) {
        do {
            FmtPuts(L"0x%08X 0x%08X %-5s %s.%s.%s\r\n",
                    di.hDevice, di.hParentDevice, di.szLegacyName,
                    di.szDeviceName, di.szBusName, di.szDeviceKey);
            fRet = v_pfnFindNextDevice (hSearchDevice, &di);
        } while (fRet == TRUE);
        v_pfnFindClose (hSearchDevice);
    }
    return TRUE;
}

