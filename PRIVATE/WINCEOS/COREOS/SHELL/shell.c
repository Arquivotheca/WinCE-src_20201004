//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include "shellapi.h"
#include "shell.h"
#define KEEP_SYSCALLS
#include <kernel.h>
#include <celog.h>
#include <windev.h>
#include <strsafe.h>
#include "lmemdebug.h"
#include <profiler.h>

extern PSTR
MapfileLookupAddress(
	IN  PWSTR wszModuleName,
	IN  DWORD dwAddress);

// Moved this to a global since it exceeds the stack hog threshold.
TCHAR v_CmdLine[MAX_CMD_LINE];

PSHELL_EXTENSION    v_ShellExtensions;

#define INFOSIZE 8192
LPBYTE rgbInfo;
TCHAR   rgbText[512];
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

//
// Flags to disable first chance exceptions for a thread.  This allows
// the try/except wrapper to handle it instead of trapping first to the
// debugger
#define DISABLEFAULTS()	(UTlsPtr()[TLSSLOT_KERNEL] |= TLSKERN_NOFAULT)
#define ENABLEFAULTS()	(UTlsPtr()[TLSSLOT_KERNEL] &= ~TLSKERN_NOFAULT)

// Update tagCMDS in shell.h if this changes.
const CMDCLASS v_commands[] =  { 
    { 0, TEXT("exit"), DoExit},
    { 0, TEXT("start"), DoStartProcess},
    { 0, TEXT("s"), DoStartProcess},
    { 0, TEXT("gi"), DoGetInfo},
    { 0, TEXT("zo"), DoZoneOps},
    { 0, TEXT("break"), DoBreak},
    { 0, TEXT("rd"), DoResDump},
    { 0, TEXT("rf"), DoResFilter},
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
    { 0, TEXT("lmem"), DoLMem},
    { 0, TEXT("options"), DoOptions},
    { 0, TEXT("suspend"), DoSuspend},
    { 0, TEXT("prof"), DoProfile},
    { 0, TEXT("loadext"), DoLoadExt},
};

ROMChain_t *pROMChain;

typedef struct _THRD_INFO {
    struct _THRD_INFO   *pNext;
    DWORD   dwThreadID;
    DWORD   dwFlags;
#define THRD_INFO_FOUND 0x00000001
    FILETIME    ftKernThrdTime;
    FILETIME    ftUserThrdTime;
} THRD_INFO, *PTHRD_INFO;

struct {
    DWORD   dwProcid;
    DWORD   dwZone;
    DWORD   dwAccess;
    PTHRD_INFO  pThrdInfo;
} rgProcData[32];

#define MAX_MODULES 200

INT cNumProc;

struct {
    DWORD   dwModid;
    DWORD   dwZone;
} rgModData[MAX_MODULES];
INT cNumMod;

BOOL    v_fUseConsole;
BOOL    v_fBeenWarned;
BOOL    v_fTimestamp;

BOOL NumberArg(PTSTR *ppCmdLine, int *num);


#define _O_RDONLY       0x0000  /* open for reading only */
#define _O_WRONLY       0x0001  /* open for writing only */
#define _O_RDWR         0x0002  /* open for reading and writing */
#define _O_APPEND       0x0008  /* writes done at eof */

#define _O_CREAT        0x0100  /* create and open file */
#define _O_TRUNC        0x0200  /* open and truncate */
#define _O_EXCL         0x0400  /* open only if file doesn't already exist */


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
    WCHAR *name,
    int mode
    ) 
{

    WCHAR  wzBuf[MAX_PATH*2];

    _tcscpy (wzBuf, TEXT("\\Release\\"));
    _tcsncat (wzBuf, name, sizeof(wzBuf)/sizeof(WCHAR) - _tcslen(wzBuf));
    
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
    PBYTE buf,
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
    PBYTE buf,
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
    ROMChain_t *pROM;
    DWORD pfnROMBase, pfnROMEnd;

    for (pROM = pROMChain; pROM; pROM = pROM->pNext) {
        pfnROMBase = GetPFN(pROM->pTOC->physfirst);
        pfnROMEnd  = GetPFN(pROM->pTOC->physlast);

        if ((pfn >= pfnROMBase) && (pfn < pfnROMEnd)) {
            return TRUE;
        }
    }

    return FALSE;
}

void FmtPuts (TCHAR *pFmt, ...)
{
    va_list ArgList;
    TCHAR   Buffer[1024];

    va_start (ArgList, pFmt);
    wvsprintf (Buffer, pFmt, ArgList);
    Puts (Buffer);
}

void
LoadExt (LPTSTR szName, LPTSTR szDLLName)
{
    HMODULE             hMod;
    PSHELL_EXTENSION    pShellExt;
    PFN_ParseCommand    pfnParseCommand;
    
    // Ok, in theory this is a good set of data.
    hMod = LoadLibrary (szDLLName);
    if (NULL != hMod) {
        if (pfnParseCommand = (PFN_ParseCommand)GetProcAddress(hMod,
            SHELL_EXTENSION_FUNCTION)) {
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
        FmtPuts (TEXT("Shell: Error unable to load %s\r\n"), szDLLName);
    }
}

BOOL main (DWORD hInstance, DWORD hPrevInstance, TCHAR *pCmdLine, int   nShowCmd) {
    BOOL    fOK;
    TCHAR   *szPrompt = TEXT("Windows CE>");
    TCHAR   *cmdline = v_CmdLine;
    HKEY    hKey;
    DWORD   dwValNameMaxLen, dwValMaxLen;
    HANDLE  hEvent;

    DEBUGREGISTER(0);
    
    // Wait until RELFSD file system loads
    hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"ReleaseFSD"); 
    if (hEvent)
        WaitForSingleObject(hEvent, INFINITE); 
    else {
        DEBUGMSG (1, (TEXT("Shell: ReleaseFSD has not been loaded.\r\n")));
        return FALSE;
    }

    // save the ROMChain in global
    pROMChain = (ROMChain_t *) KLibGetROMChain ();

    if (!pROMChain) {
        RETAILMSG (1, (TEXT("ERROR: ROMChain not fould! (is you NK up-to-date?)\r\n")));
        return FALSE;
    }


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
        } else if (!_tcsncmp(pCmdLine, TEXT("-r"), 2)) {
            pCmdLine += 2;
            // Skip trailing spaces
            while (_istspace(*pCmdLine)) {
                pCmdLine++;
            }
            break;
        } else if (_istdigit(*pCmdLine)) {
            // Probably spawned from the init process, silently pass over the numeric parameter
            while (_istdigit(*pCmdLine)) {
                pCmdLine++;
            }
        } else {
            // Unknown/unused argument
            RETAILMSG (1, (TEXT("SHELL.EXE: Unrecognized parameter '%s'\r\n"), pCmdLine));
            // pretend there are no more arguments.
            pCmdLine = NULL;
        }
        // Skip trailing spaces
        while ((NULL != pCmdLine) && _istspace(*pCmdLine)) {
            pCmdLine++;
        }
    }
    _crtinit();
    PPSHRestart();
    if (!(rgbInfo = (LPBYTE)VirtualAlloc(0,INFOSIZE,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE))) {
        Puts(L"\r\n\r\nWindows CE Shell - unable to initialize!\r\n");
        return TRUE;
    }
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
            CloseHandle (hKey);        
        }
        DEBUGMSG (ZONE_INFO, (TEXT("Shell: main thread prio set to %d\r\n"), dwMainThdPrio));
        CeSetThreadPriority ((HANDLE)GetCurrentThreadId(), dwMainThdPrio);
    }

    // Any Shell Extensions?
    if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, TSH_EXT_REG, 0, 0, &hKey))
    {
        if (ERROR_SUCCESS == RegQueryInfoKey (hKey, NULL, NULL, NULL, NULL, NULL,
                                                NULL, NULL, &dwValNameMaxLen, &dwValMaxLen, NULL, NULL))
        {
            LPTSTR  szVal, szData;
            DWORD   dwIndex, dwValNameLen, dwValLen, dwType;
            
            RETAILMSG (1, (TEXT("ValNameLen=%d ValLen=%d\r\n"), dwValNameMaxLen, dwValMaxLen));

            // ValNameLen is in characters.
            szVal = (LPTSTR)LocalAlloc (LMEM_FIXED, (dwValNameMaxLen+1)*sizeof(TCHAR));
            // ValLen is in bytes
            szData = (LPTSTR)LocalAlloc (LMEM_FIXED, dwValMaxLen);

            dwIndex = 0;

            dwValNameLen = dwValNameMaxLen+1;
            dwValLen = dwValMaxLen;
            while (ERROR_SUCCESS == RegEnumValue (hKey, dwIndex, szVal, &dwValNameLen, NULL,
                                                  &dwType, (LPBYTE)szData, &dwValLen)) {
                RETAILMSG (1, (TEXT("Val=%s Data=%s, Type=%d\r\n"), szVal, szData, dwType));

                if (REG_SZ == dwType) {
                    LoadExt (szVal, szData);
                } else {
                    RETAILMSG (1, (TEXT("Shell: Value %s: Unexpected registry type %d\r\n"),
                                   szVal, dwType));
                }
                
                dwIndex++;
                dwValNameLen = dwValNameMaxLen+1;
                dwValLen = dwValMaxLen;
            }
        }
        CloseHandle (hKey);
    } else {
        RETAILMSG (1, (TEXT("Shell: No extension DLLs found\r\n")));
    }
    
    // Did they pass in an argument?
    if ((NULL != pCmdLine) && (TEXT('\0') != *pCmdLine)) {
        DoCommand(pCmdLine);
        goto CleanUp;
    }
    fOK = TRUE;
restart:
    PPSHRestart();
    while (1) {
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
    return TRUE;
}

BOOL DoHelp (PTSTR cmdline) {
    BOOL OldMode;
    HMODULE hLMEMDbg;
    PSHELL_EXTENSION    pShellExt;

    Puts(TEXT("\r\nEnter any of the following commands at the prompt:\r\n"));
    Puts(TEXT("break : Breaks into the debugger\r\n"));
    Puts(TEXT("s <procname> : Starts new process\r\n"));
    Puts(TEXT("gi [\"proc\",\"thrd\",\"mod\",\"all\"]* [\"<pattern>\"] : Get Information\r\n"));
    Puts(TEXT("    proc  -> Lists all processes in the system\r\n"));
    Puts(TEXT("    thrd  -> Lists all processes with their threads\r\n"));
    Puts(TEXT("    delta -> Lists only threads that have changes in CPU times\r\n"));
    Puts(TEXT("    mod   -> Lists all modules loaded\r\n"));
    Puts(TEXT("    all: Lists all of the above\r\n"));
    Puts(TEXT("mi [\"kernel\",\"full\"] : Memory information\r\n"));
    Puts(TEXT("    kernel-> Lists kernel memory detail\r\n"));
    Puts(TEXT("    full  -> Lists full memory maps\r\n"));
    Puts(TEXT("zo [\"p\",\"m\",\"h\"] [index,name,handle] [<zone>,[[\"on\",\"off\"][<zoneindex>]*]*: Zone Ops\r\n"));
    Puts(TEXT("    p/m/h -> Selects whether you operate on a process, module, or hProcess/pModule\r\n"));
    Puts(TEXT("    index -> Put the index of the proc/module, as printed by the gi command\r\n"));
    Puts(TEXT("    name  -> Put the name of the proc/module, as printed by the gi command\r\n"));
    Puts(TEXT("    handle-> Put the value of the hProcess/pModule, as printed by the gi command\r\n"));
    Puts(TEXT("          -> If no args are specified, prints cur setting with the zone names\r\n"));
    Puts(TEXT("    zone  -> Must be a number to which this proc/modl zones are to be set \r\n"));
    Puts(TEXT("             Prefix by 0x if this is specified in hex\r\n"));
    Puts(TEXT("    on/off-> Specify bits to be turned on/off relative to the current zonemask\r\n"));
    Puts(TEXT("  eg. zo p 2: Prints zone names for the proc at index 2 in the proc list \r\n"));
    Puts(TEXT("  eg. zo h 8fe773a2: Prints zone names for the proc/module with hProcess/pModule of 8fe773a2\r\n"));
    Puts(TEXT("  eg. zo m 0 0x100: Sets zone for module at index 0 to 0x100 \r\n"));
    Puts(TEXT("  eg. zo p 3 on 3 5 off 2: Sets bits 3&5 on, and bit 2 off for the proc[3]\r\n"));
    Puts(TEXT("win : Dumps the list of windows\r\n"));
    Puts(TEXT("log <CE zone> [user zone] [process zone] : Change CeLog zone settings\r\n"));
    Puts(TEXT("prof <on|off> [data_type] [storage_type] : Start or stop kernel profiler\r\n"));
    Puts(TEXT("memtrack {on, off, baseline} : Turns memtrack on or off, or generates baseline\r\n"));
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
    
    Puts(TEXT("kp <pid>: Kills process\r\n"));
    Puts(TEXT("dd <addr> [size]: dumps dwords for specified process id\r\n"));
    Puts(TEXT("df <filename> <addr> <size>: dumps address to file\r\n"));
    Puts(TEXT("hd <exe>: dumps the process heap\r\n"));
    Puts(TEXT("run <filename>: run file in batch mode\r\n"));
    Puts(TEXT("dis: Discard all discardable memory\r\n"));
    Puts(TEXT("options: Set options\r\n"));
    Puts(TEXT("     timestamp     -> Toggle timestamp on all cmds (useful for logs)\r\n"));
    Puts(TEXT("     priority [N]  -> Change the priority of the shell thread\r\n"));
    Puts(TEXT("     kernfault     -> Fault even if NOFAULT specified\r\n"));
    Puts(TEXT("     kernnofault   -> Re-enable default NOFAULT handling\r\n"));
    Puts(TEXT("suspend: Suspend the device.  Requires GWES.exe and proper OAL support\r\n"));

    if (hLMEMDbg = (HMODULE)LoadLibrary(LMEMDEBUG_DLL)) {
        Puts(TEXT("LMem <proc>|* [recent] [hist] [chk] [delta] [critsec_on] [critsec_off] [info] [<heap>|*] [breaksize <NN>] [breakcnt <NN>]\r\n"));
        Puts(TEXT("     proc   : Proc index, name, PID or * (for all processes)\r\n"));
        Puts(TEXT("     recent : Show list of recent allocations\r\n"));
        Puts(TEXT("     hist   : Show histogram of alloc size\r\n"));
        Puts(TEXT("     chk    : Checkpoint all current allocations\r\n"));
        Puts(TEXT("     delta  : Report allocations since checkpoint\r\n"));
        Puts(TEXT("     critsec_on : Report count of contentions for critsec on DeleteCriticalSection\r\n"));
        Puts(TEXT("     critsec_off : Turn off reporting of contentions for critsec on DeleteCriticalSection\r\n"));
        Puts(TEXT("     info   : Report any additional info stored with the allocations (stack etc.)\r\n"));
        Puts(TEXT("     map    : Will try to read the .map files to resolve stack addresses (implies info)\r\n"));
        Puts(TEXT("     heap   : Heap index, handle or * for all heaps\r\n"));
        Puts(TEXT("     breaksize <NN> : Break when size is NN\r\n"));
        Puts(TEXT("     breakcnt <NN> : Break when count is NN\r\n"));
        Puts(TEXT("     eg.\r\n"));
        Puts(TEXT("       'lmem *' : Print top level statistics for all heaps\r\n"));
        Puts(TEXT("       'lmem device hist' ; Show histogram of device.exe heap\r\n"));
        Puts(TEXT("       'lmem device recent' : Show recent allocations in device.exe\r\n"));
        Puts(TEXT("       'lmem * chk' : Checkpoint all current memory allocations\r\n"));
        Puts(TEXT("       'lmem * delta' : Display memory since last checkpoint\r\n"));
        Puts(TEXT("       'lmem device *' : Show all allocations in device.exe\r\n"));
        FreeLibrary(hLMEMDbg);
    }
    Puts(TEXT("loadext [[-u] DLLName] : Load shell.exe extension dll\r\n"));
    Puts(TEXT("    -u : unload extension dll\r\n"));
    Puts(TEXT("    DLLName : name of DLL to load (i.e. shellext.dll)\r\n"));
    Puts(TEXT("\r\n"));

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
#define IsWhiteSpace(c) ((c)==TEXT(' ') || (c)==TEXT('\r') || (c)==TEXT('\n'))
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
    for (j=0; j<CMDS_MAX; j++) {
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
    
    index = FindCommand(&cmdline, &szCmd);
    switch (index) {
        case CMDS_NO_COMMAND:
            break;
        case CMDS_UNKNOWN:
            // Check for extension
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
                Puts(TEXT("Unknown command.\r\n\r\n"));
            }
            break;
        default:
            if (IsValidCommand(index))
                if (v_fTimestamp) {
                    TCHAR   szOutStr[256];
                    SYSTEMTIME  SystemTime;
                    GetLocalTime (&SystemTime);
                    wsprintf (szOutStr, TEXT("%02d/%02d/%02d %d:%02d:%02d.%03d\r\n"),
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
    LPTSTR pszCommand = NULL;
    LPCTSTR pszCommandSuffix = _T(".exe");

    DEBUGCHK(pszCmdLine != NULL && pszImageName != NULL && ppi != NULL && cchImageName != 0);

    SetLastError (ERROR_INVALID_PARAMETER);
    
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
    StringCchCopy(pszImageName, cchImageName, pszCommand);
    pszImageName[cchImageName - 1] = 0;

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
        if(dwCommandLen < dwSuffixLen 
        || _tcsicmp(pszImageName + (dwCommandLen - dwSuffixLen), pszCommandSuffix) != 0) {
            iStatus = StringCchCat(pszImageName, cchImageName, pszCommandSuffix);
            if(iStatus != S_OK) {
                // string had to be truncated
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
    TCHAR szImageName[80];
    PROCESS_INFORMATION pi;

    if (cmdline == NULL) {
        Puts(TEXT("Syntax: s <procname>\r\n\r\n"));
        return TRUE;
    }

    // Check to see if the system has been started yet.
    // If GWES has not registered it's API set then assume that perhaps
    // we shouldn't be running any applications
    if (FALSE == v_fBeenWarned) {
        TCHAR   Response[256];
        int     i;
        while (FALSE == IsAPIReady (SH_WMGR)) {
            Puts (TEXT("\r\nGWES has not been started yet, run anyway (y or n)?"));
            if (Gets (Response, 256)) {
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
    
    fOk = LaunchProcess(cmdline, szImageName, sizeof(szImageName) / sizeof(szImageName[0]), &pi);
    if (!fOk)  {
        FmtPuts (TEXT("Unable to create process '%s' : Error %d\r\n"),
                 szImageName, GetLastError());
    } else {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    return TRUE;
} 

BOOL DoKillProc (PTSTR cmdline) {
    DWORD dwProcID;
    DWORD oldPerm;

    if (!cmdline || !NumberArg(&cmdline, &dwProcID) || *cmdline) {
        Puts(TEXT("Syntax: kp <pid>\r\n\r\n"));
        return TRUE;
    }
    
    if (dwProcID < 32) {
        dwProcID = rgProcData[dwProcID].dwProcid;
    }
    oldPerm = SetProcPermissions((DWORD)-1);
    TerminateProcess((HANDLE)dwProcID,0);
    SetProcPermissions(oldPerm);
    return TRUE;    
}

void DumpThisAddr(PDWORD pdw, ULONG ulLen) {
    while (ulLen) {
        ULONG ulLine = 4;
        wsprintf(rgbText, TEXT("%08X: "), (DWORD)pdw);
        Puts(rgbText);
        while (ulLen && ulLine) {
            wsprintf(rgbText, TEXT(" %08X") , *pdw);
            Puts(rgbText);
            pdw++;
            ulLen--;
            ulLine--;
        }
        Puts(TEXT("\r\n"));
    }
}

BOOL DoDumpDwords (PTSTR cmdline) {
    ULONG ulAddr;
    ULONG ulLen = 16;
    DWORD OldPerms;
    BOOL OldMode;
    
    if (cmdline == NULL || !NumberArg(&cmdline, &ulAddr)) {
        Puts(TEXT("Syntax: dd <addr> [size]\r\n"));
        return TRUE;
    }

    if (!NumberArg(&cmdline, &ulLen) && *cmdline) {
        Puts(TEXT("Syntax: dd <addr> [size]\r\n"));
        return TRUE;
    }

    if (!ulLen || (ulLen > 1024*1024)) {
        Puts(TEXT("dd: Invalid Length\r\n"));
        return TRUE;
    }

/*    
    while (*cmdline && (*cmdline == ' '))
        cmdline++;
    if (*cmdline == 0) {
        Puts(TEXT("Syntax: dd <addr> [size]\r\n"));
        return FALSE;
    }
    ulAddr = _wtol(cmdline);
    while (*cmdline && (*cmdline != ' '))
        cmdline++;
        
    while (*cmdline && (*cmdline == ' '))
        cmdline++;
    if (*cmdline != 0)
        ulLen = _wtol(cmdline);
*/
    ulAddr &= ~3;   // make DWORD aligned
    OldPerms = SetProcPermissions((DWORD)~0);
    OldMode = SetKMode(TRUE);
    if (!LockPages ((LPVOID) ulAddr, ulLen * sizeof(DWORD), 0, LOCKFLAG_READ)) {
        Puts(TEXT("dd: Invalid address!\r\n"));
    } else {
        __try {
            DumpThisAddr((PDWORD)ulAddr, ulLen);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Puts(TEXT("\r\n<Fault while dumping memory>\r\n"));
        }
        UnlockPages ((LPVOID) ulAddr, ulLen);
    }
    SetProcPermissions(OldPerms);
    SetKMode(OldMode);
    return TRUE;
}

BOOL DoDumpFile (PTSTR cmdline) {
    TCHAR *token, szImageName[80], *eos;
    TCHAR   szOutStr[256];
    ULONG ulAddress=0, ulLen=0;
    int fd;
    BOOL OldMode;
    DWORD OldPerm;
    if (cmdline == NULL) {
        Puts(TEXT("Syntax: df <filename> <addr> <size>\r\n"));
        return TRUE;
    }
    szImageName[0]='\0';
    eos = cmdline + _tcslen(cmdline);
    if ((token = _tcstok(cmdline, TEXT(" \t"))) != NULL) {
        _tcscpy(szImageName, token);
        token = _tcstok(NULL, TEXT(" \t"));
        /* Now lets put back what that one took out. Sigh. */
        if (token) {
            ulAddress = wcstoul(token,0,16);
            token = _tcstok(NULL, TEXT(" \t"));
            if (token) {
                ulLen= wcstoul(token,0,16);
            }
        }
    }
    if (szImageName[0] && ulAddress && ulLen) {
        if( -1 == (fd = UU_ropen(szImageName, 0x80001 )) ) {
            wsprintf (szOutStr, L"Unable to open input file %s\r\n", szImageName);
            Puts (szOutStr);
            return TRUE;
        }
        OldMode = SetKMode(TRUE);
        OldPerm = SetProcPermissions ((DWORD) -1);
        wsprintf (szOutStr, L"Writing address %8.8lxh len %8.8lxh...\r\n",ulAddress,ulLen);
        Puts (szOutStr);
        if((DWORD)UU_rwrite( fd, (BYTE *)ulAddress, ulLen) != ulLen) {
            wsprintf (szOutStr, L"Error reading image physical start from file.\r\n" );
            Puts (szOutStr);
        } else {
            wsprintf (szOutStr, L"Write complete.\r\n");
            Puts (szOutStr);
        }
        UU_rclose(fd);
        SetProcPermissions (OldPerm);
        SetKMode(OldMode);
    } else
        Puts(L"Error: invalid parameters.\r\n");
    return TRUE;
}


#define SHELL_GETMOD  0x0001
#define SHELL_GETPROC 0x0002
#define SHELL_GETTHRD 0x0004

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
    PHDATA phd;

    if (h) {
        phd = (PHDATA)(((ulong)h & HANDLE_ADDRESS_MASK) + 
            ((struct KDataStruct *)(UserKInfo[KINX_KDATA_ADDR]))->handleBase);
        return phd->pvObj;
    }
    return 0;
}

#define CH_DQUOTE       TEXT('"')
#define ARRAYSIZE(a)    (sizeof (a) / sizeof ((a)[0]))

//***   IsPatMatch -- does text match the pattern arg?
// NOTES
//  no REs for now (ever?), so the Is'Pat'Match is a slight misnomer
BOOL IsPatMatch(TCHAR *pszPat, TCHAR *pszText)
{
    BOOL fRet;
    TCHAR szText[64];       // 64 is plenty big enough

    if (!pszPat)
        return TRUE;

    _tcsncpy(szText, pszText, ARRAYSIZE(szText));
    szText[ARRAYSIZE(szText) - 1] = 0;
    _tcslwr(szText);
    fRet = _tcsstr(szText, pszPat) != NULL;

    return fRet;
}


//
// Check if the specified module id is in our global list
//
BOOL
IsModInList(
    DWORD dwModid
    )
{
    INT i;

    for (i = 0; i < MAX_MODULES; i++) {
        if (0 == rgModData[i].dwModid) {
            return FALSE;
        }
        if (dwModid == rgModData[i].dwModid) {
            return TRUE;
        }
    }
    return FALSE;
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


BOOL DoGetInfo (PTSTR cmdline) {
    DWORD   dwMask = 0xFFFFFFFF;
    PTSTR   szToken;
    HANDLE hSnap;
    PROCESSENTRY32 proc;
    MODULEENTRY32 mod;
    THREADENTRY32 thread;
    TCHAR   *pszArg = NULL;
    BOOL    fMatch;
    BOOL    fDelta = FALSE;
    INT     i;
    WCHAR   szLastMod[MAX_PATH];
    BOOL    fSwapped;

    // Find out if he wants to mask stuff
    if (cmdline) {
        dwMask = 0;
        while ((szToken = _tcstok(cmdline, TEXT(" \t"))) != NULL) {
            cmdline = NULL; // to make subsequent _tcstok's work
            if (!_tcscmp(szToken, TEXT("proc")))
                dwMask |= SHELL_GETPROC;
            else if (!_tcscmp(szToken, TEXT("thrd")))
                dwMask |= SHELL_GETTHRD | SHELL_GETPROC;
            else if (!_tcscmp(szToken, TEXT("mod")))
                dwMask |= SHELL_GETMOD;
            else if (!_tcscmp(szToken, TEXT("delta"))) {
                dwMask |= SHELL_GETTHRD | SHELL_GETPROC;
                fDelta = TRUE;
            } else if (!_tcscmp(szToken, TEXT("all")))
                dwMask = 0xFFFFFFFF;
            else if (!_tcscmp(szToken, TEXT("silent")))
                dwMask |= SHELL_GETMOD | SHELL_GETPROC;
            else if (szToken[0] == CH_DQUOTE && pszArg == NULL) {
                // nuke CH_DQUOTEs
                pszArg = szToken + 1;
                if (_tcslen(pszArg) && (CH_DQUOTE == pszArg[_tcslen(pszArg) - 1])) {
                    pszArg[_tcslen(pszArg) - 1] = 0;    // remove trailing quote
                }
            }
            else
                Puts(TEXT("Invalid argument.  Must be one of proc, thrd, delta, mod or all.\r\n\r\n"));
        }
    }
    if (dwMask & SHELL_GETPROC) {
        hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS | TH32CS_SNAPNOHEAPS | (dwMask & SHELL_GETTHRD ? TH32CS_SNAPTHREAD : 0),0);
        if (INVALID_HANDLE_VALUE != hSnap) {
            proc.dwSize = sizeof(proc);
            if (Process32First(hSnap,&proc)) {
                cNumProc = 0;
                wsprintf(rgbText, L"PROC: Name            hProcess: CurAKY :dwVMBase:CurZone\r\n");
                Puts(rgbText);
                if (dwMask & SHELL_GETTHRD) {
                    wsprintf(rgbText, L"THRD: State :hCurThrd:hCurProc: CurAKY :Cp :Bp :Kernel Time  User Time\r\n");
                    Puts(rgbText);
                }
                do {
                    // for now just match module; move down if want arb text
                    fMatch = IsPatMatch(pszArg, proc.szExeFile);
                    wsprintf(rgbText, L" P%2.2d: %-15s %8.8lx %8.8lx %8.8lx %8.8lx\r\n",cNumProc,
                        proc.szExeFile,proc.th32ProcessID,proc.th32AccessKey,proc.th32MemoryBase,proc.dwFlags);
                    rgProcData[cNumProc].dwProcid = proc.th32ProcessID;
                    rgProcData[cNumProc].dwAccess = proc.th32AccessKey;
                    rgProcData[cNumProc++].dwZone = proc.dwFlags;
                    if (fMatch) {
                        Puts(rgbText);
                    } else {
                        continue;
                    }
                    if (dwMask & SHELL_GETTHRD) {
                        PTHRD_INFO  pThrdInfo, pThrdInfoLast;
                        
                        thread.dwSize = sizeof(thread);

                        // First clear the used bit.
                        for (pThrdInfo=rgProcData[cNumProc-1].pThrdInfo; pThrdInfo; pThrdInfo=pThrdInfo->pNext) {
                            pThrdInfo->dwFlags &= ~(THRD_INFO_FOUND);
                        }
                        
                        if (Thread32First(hSnap,&thread)) {                            
                            DWORD dwAccess;
                            dwAccess = SetProcPermissions((DWORD)-1);
                            do {
                                if (thread.th32OwnerProcessID == proc.th32ProcessID) {
                                    FILETIME f1,f2,f3,f4;
                                    __int64 hrs, mins, secs, ms;

                                    // Find the thread info for this.
                                    for (pThrdInfo=rgProcData[cNumProc-1].pThrdInfo; pThrdInfo; pThrdInfo=pThrdInfo->pNext) {
                                        if (pThrdInfo->dwThreadID == (DWORD)thread.th32ThreadID) {
                                            break;
                                        }
                                    }
                                    if (NULL == pThrdInfo) {
                                        // Need to allocate a new one.
                                        pThrdInfo = (PTHRD_INFO)LocalAlloc (LPTR, sizeof(THRD_INFO));
                                        if (pThrdInfo) {
                                            pThrdInfo->dwThreadID = (DWORD)thread.th32ThreadID;
                                            pThrdInfo->pNext = rgProcData[cNumProc-1].pThrdInfo;
                                            rgProcData[cNumProc-1].pThrdInfo = pThrdInfo;
                                        }
                                    }
                                    if (pThrdInfo) {
                                        pThrdInfo->dwFlags |= THRD_INFO_FOUND;
                                    }
                                    
                                    wsprintf(rgbText, L" T    %6.6s %8.8lx %8.8lx %8.8lx %3d %3d",
                                        rgszStates[thread.dwFlags],thread.th32ThreadID,thread.th32CurrentProcessID,thread.th32AccessKey,
                                        thread.tpBasePri-thread.tpDeltaPri,thread.tpBasePri);
                                    if (GetThreadTimes((HANDLE)thread.th32ThreadID,&f1,&f2,&f3,&f4)) {
                                        if (pThrdInfo) {
                                            // Save them
                                            *(__int64 *)&f1 = *(__int64 *)&f3;
                                            *(__int64 *)&f2 = *(__int64 *)&f4;
                                            if (fDelta) {
                                                *(__int64 *)&f3 -= *(__int64 *)&(pThrdInfo->ftKernThrdTime);
                                                *(__int64 *)&f4 -= *(__int64 *)&(pThrdInfo->ftUserThrdTime);
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
                                        wsprintf(rgbText + lstrlen(rgbText), L" %2.2d:%2.2d:%2.2d.%3.3d", 
                                            (DWORD)hrs, (DWORD)mins, (DWORD)secs, (DWORD)ms);
                                        *(__int64 *)&f4 /= 10000;
                                        ms = *(__int64 *)&f4 % 1000;
                                        *(__int64 *)&f4 /= 1000;
                                        secs = *(__int64 *)&f4 % 60;
                                        *(__int64 *)&f4 /= 60;
                                        mins = *(__int64 *)&f4 % 60;
                                        *(__int64 *)&f4 /= 60;
                                        hrs = *(__int64 *)&f4;
                                        wsprintf(rgbText + lstrlen(rgbText), L" %2.2d:%2.2d:%2.2d.%3.3d\r\n", 
                                            (DWORD)hrs, (DWORD)mins, (DWORD)secs, (DWORD)ms);
                                    } else {
                                        wsprintf(rgbText + lstrlen(rgbText), L" No Thread Time\r\n");
                                    }
                                    Puts(rgbText);
                                }
                            } while (Thread32Next(hSnap,&thread));
                            SetProcPermissions(dwAccess);
                        }
                        // Now make sure that we remove any unused ThrdInfo structs.
                        pThrdInfoLast = NULL;
                        for (pThrdInfo=rgProcData[cNumProc-1].pThrdInfo; pThrdInfo; ) {
                            if (!(pThrdInfo->dwFlags & THRD_INFO_FOUND)) {
                                if (pThrdInfoLast) {
                                    pThrdInfoLast->pNext = pThrdInfo->pNext;
                                } else {
                                    rgProcData[cNumProc-1].pThrdInfo = pThrdInfo->pNext;
                                }
                                LocalFree (pThrdInfo);
                                if (pThrdInfoLast) {
                                    pThrdInfo = pThrdInfoLast->pNext;
                                } else {
                                    pThrdInfo = rgProcData[cNumProc-1].pThrdInfo;
                                }
                                
                            } else {
                                pThrdInfoLast = pThrdInfo;
                                pThrdInfo = pThrdInfo->pNext;
                            }
                        }

                    }
                } while (Process32Next(hSnap,&proc));
                if (dwMask & SHELL_GETMOD)
                    if (fMatch) {
                        Puts(L"\r\n");
                    }
            }
            CloseToolhelp32Snapshot(hSnap);
        } else {
            FmtPuts(L"Unable to obtain process/thread snapshot (%d)!\r\n",GetLastError());
        }
    }
    if (dwMask & SHELL_GETMOD) {
        hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_GETALLMODS,0);
        if (INVALID_HANDLE_VALUE != hSnap) {
            //
            // First sort the modules by name into rgModData[]
            //
            mod.dwSize = sizeof(mod);
            memset(rgModData, 0, sizeof(rgModData));
            cNumMod = 0;
            fSwapped = TRUE;

            while (fSwapped) {
                fSwapped = FALSE;
                szLastMod[0] = 0;
                if (Module32First(hSnap,&mod)) {
                    do {
                        if (!IsModInList(mod.th32ModuleID)) {
                            if (0 == szLastMod[0]) {
                                rgModData[cNumMod].dwModid = mod.th32ModuleID;
                                rgModData[cNumMod].dwZone = mod.dwFlags;
                                wcscpy(szLastMod, mod.szModule);
                                fSwapped = TRUE;
                            } else {
                                if (0 > wcsicmp(mod.szModule, szLastMod)) {
                                    fSwapped = TRUE;
                                    rgModData[cNumMod].dwModid = mod.th32ModuleID;
                                    rgModData[cNumMod].dwZone = mod.dwFlags;
                                    wcscpy(szLastMod, mod.szModule);
                                }
                            }
                        }
                    } while (Module32Next(hSnap,&mod));
                }

                if (fSwapped) {
                    cNumMod++;
                    if (MAX_MODULES == cNumMod) {
                        break;
                    }
                }
            }

            //
            // Print the sorted list of modules
            //
            FmtPuts(L" MOD: Name            pModule :dwInUSE :dwVMBase:CurZone\r\n");
            for (i = 0; i < cNumMod; i++) {
                if (GetModuleInfo(hSnap, &mod, rgModData[i].dwModid)) {
                    // for now just match module; move down if want arb text
                    fMatch = IsPatMatch(pszArg, mod.szModule);
                    if (fMatch) {
                        FmtPuts(L" M%2.2d: %-15s %8.8lx %8.8lx %8.8lx %8.8lx\r\n",i,
                                mod.szModule,mod.th32ModuleID,mod.ProccntUsage,
                                (DWORD)(mod.modBaseAddr),mod.dwFlags);
                    }
                }
            }

            CloseToolhelp32Snapshot(hSnap);

        } else {
            FmtPuts(L"Unable to obtain process/thread snapshot (%d)!\r\n",GetLastError());
        }
    }
    return TRUE;
}

#define RAM_PFN     1
#define OSTORE_PFN  2
#define UNKNOWN_PFN 3

int GetPfnType(ulong pfn) {
    PFREEINFO pfi, pfiEnd;
    pfi = ((MEMORYINFO*)UserKInfo[KINX_MEMINFO])->pFi;
    pfiEnd = pfi + ((MEMORYINFO*)UserKInfo[KINX_MEMINFO])->cFi;
    for ( ; pfi < pfiEnd ; ++pfi)
        if (pfn >= pfi->paStart && pfn < pfi->paEnd)
            return RAM_PFN;
    return UNKNOWN_PFN;
}

void DisplaySectionInfo(PSECTION pscn, DWORD dwVMBase, BOOL bFull) {
    MEMBLOCK *pmb;
    int ixBlock;
    int ixPage;
    int cpgCode, cpgRWData, cpgROData, cpgReserved, cpgStack;
    int cpgRAMCode;
    ulong ulPTE;
    ulong pfn;
    WCHAR achTypes[PAGES_PER_BLOCK+1];

    FmtPuts(L"Slot base %8.8lx  Section ptr %8.8lx\r\n", dwVMBase, (DWORD)pscn);

    /* Walk the process's memory section to count the pages used by the process */
    cpgCode = cpgRWData = cpgReserved = cpgStack = cpgROData = cpgRAMCode= 0;
    for (ixBlock = 0 ; ixBlock < BLOCK_MASK+1 ; ++ixBlock) {
        if ((pmb = (*pscn)[ixBlock]) != NULL_BLOCK) {
            if (pmb == RESERVED_BLOCK)
                cpgReserved += PAGES_PER_BLOCK;
            else {
                for (ixPage = 0 ; ixPage < PAGES_PER_BLOCK ; ++ixPage) {
                    ulPTE = pmb->aPages[ixPage];
                    if (ulPTE == BAD_PAGE)
                        break;
                    pfn = PFNfromEntry(ulPTE);
                    if (ulPTE == 0) {
                        ++cpgReserved;
                        achTypes[ixPage] = L'-';
                    } else if (ulPTE & PG_EXECUTE_MASK) {
                        ++cpgCode;
                        achTypes[ixPage] = L'C';
                        if (!IsPfnInRom (pfn)) {
                            achTypes[ixPage] = L'c';
                            ++cpgRAMCode;
                        }
                    } else if (ulPTE & PG_DIRTY_MASK) {
                        if (pmb->flags & MB_FLAG_AUTOCOMMIT) {
                            ++cpgStack;
                            achTypes[ixPage] = L'S';
                        } else switch (GetPfnType(pfn)) {
                        case UNKNOWN_PFN:
                            achTypes[ixPage] = L'P';
                            break;
                        case RAM_PFN:
                            ++cpgRWData;
                            achTypes[ixPage] = L'W';
                            break;
                        case OSTORE_PFN:
                            achTypes[ixPage] = L'O';
                            break;
                        default:
                            achTypes[ixPage] = L'?';
                            break;
                        }
                    } else {
                        if (!IsPfnInRom (pfn)) {
                            achTypes[ixPage] = L'r';
                            ++cpgRWData;
                        } else {
                            ++cpgROData;
                            achTypes[ixPage] = L'R';
                        }
                    }
                }
                if (bFull) {
                    achTypes[ixPage] = 0;
                    FmtPuts(L"  %8.8lx(%d): %s\r\n", 
                            dwVMBase | (ixBlock<<VA_BLOCK), pmb->cLocks, achTypes);
                }
            }
        }
    }
    TotalCpgRW += cpgRWData + cpgStack + cpgRAMCode;
    FmtPuts(L"Page summary: code=%d(%d) data r/o=%d r/w=%d stack=%d reserved=%d\r\n",
            cpgCode, cpgRAMCode, cpgROData, cpgRWData, cpgStack, cpgReserved);
}

void DisplayProcessInfo(BOOL bFull) {
    int loop;
    PPROCESS pProc;
    pProc = (PPROCESS)UserKInfo[KINX_PROCARRAY];

    // special handling for NK
    FmtPuts(L"\r\nMemory usage for Process %8.8lx: 'NK.EXE' pid %x\r\n", (DWORD)pProc, pProc->procnum);
    Puts (L"\r\nSECURE SECTION:\r\n");
    DisplaySectionInfo ((PSECTION) UserKInfo[KINX_NKSECTION], SECURE_VMBASE, bFull);
    
    for (loop = 0; loop < MAX_PROCESSES; pProc++, loop++)
        if (pProc->dwVMBase) {
            if (loop) {
                FmtPuts(L"\r\nMemory usage for Process %8.8lx: '%s' pid %x\r\n", (DWORD)pProc, 
                        pProc->lpszProcName ? pProc->lpszProcName : L"**", pProc->procnum);
            } else {
                Puts (L"\r\nROM DLL CODE SECTION:\r\n");
            }
            
            DisplaySectionInfo (((PSECTION *)UserKInfo[KINX_SECTIONS])[pProc->procnum+1], loop? pProc->dwVMBase : (1 << VA_SECTION), bFull);
        }
}

void DisplayOtherMemory(BOOL bFull) {
    int loop, ixBlock;
    MEMBLOCK *pmb;
    SECTION *pscn;
    for (loop = 0; loop < NUM_MAPPER_SECTIONS; loop++) {
        pscn = ((PSECTION *)UserKInfo[KINX_SECTIONS])[(FIRST_MAPPER_ADDRESS>>VA_SECTION)+loop];
        for (ixBlock = 0 ; ixBlock < BLOCK_MASK+1 ; ++ixBlock) {
            if ((pmb = (*pscn)[ixBlock]) != NULL_BLOCK) {
                FmtPuts(L"\r\nMapped file section %d\r\n",loop);
                DisplaySectionInfo (pscn, FIRST_MAPPER_ADDRESS + (loop << VA_SECTION), bFull);
                break;
            }
        }
    }
}

void DisplayKernelHeapInfo(void) {
    int loop;
    heapptr_t *hptr;
    ulong cbUsed, cbExtra, cbMax;
    ulong cbTotalUsed, cbTotalExtra;
    hptr = (heapptr_t *)UserKInfo[KINX_KHEAP];
    cbTotalUsed = cbTotalExtra = 0;
    Puts(L"Inx Size   Used    Max Extra  Entries Name\r\n");
    for (loop = 0 ; loop < NUMARENAS ; ++loop, ++hptr) {
        cbUsed = hptr->size * hptr->cUsed;
        cbMax = hptr->size * hptr->cMax;
        cbExtra = cbMax - cbUsed;
        cbTotalUsed += cbUsed;
        cbTotalExtra += cbExtra;
        FmtPuts(L"%2d: %4d %6ld %6ld %5ld %3d(%3d) %hs\r\n", loop, hptr->size,
                cbUsed, cbMax, cbExtra, hptr->cUsed, hptr->cMax,
                hptr->classname);
    }
    FmtPuts(L"Total Used = %ld  Total Extra = %ld  Waste = %d\r\n",
            cbTotalUsed, cbTotalExtra, UserKInfo[KINX_HEAP_WASTE]);
}

BOOL DoMemtool (PTSTR cmdline) {
    BOOL bKern = 0, bFull = 0;
    DWORD oldMode, oldPerm;
    PTSTR   szToken;
    if (cmdline) {
        while ((szToken = _tcstok(cmdline, TEXT(" \t"))) != NULL) {
            cmdline = NULL; // to make subsequent _tcstok's work
            if (!_tcscmp(szToken, TEXT("full")))
                bFull = 1;
            else if (!_tcscmp(szToken, TEXT("kernel")))
                bKern =1;
        }
    }
    Puts(L"\r\nWindows CE Kernel Memory Usage Tool 0.2\r\n");
    FmtPuts(L"Page size=%d, %d total pages, %d free pages. %d MinFree pages (%d MaxUsed bytes)\r\n",
           UserKInfo[KINX_PAGESIZE], UserKInfo[KINX_NUMPAGES],
           UserKInfo[KINX_PAGEFREE], UserKInfo[KINX_MINPAGEFREE],
           UserKInfo[KINX_PAGESIZE]*(UserKInfo[KINX_NUMPAGES]-UserKInfo[KINX_MINPAGEFREE]));
    FmtPuts(L"%d pages used by kernel, %d pages held by kernel, %d pages consumed.\r\n",
           UserKInfo[KINX_SYSPAGES],UserKInfo[KINX_KERNRESERVE],
           UserKInfo[KINX_NUMPAGES]-UserKInfo[KINX_PAGEFREE]);
    oldMode = SetKMode(TRUE);
    oldPerm = SetProcPermissions(~0ul);
    TotalCpgRW = 0;
    if (bFull || bKern)
        DisplayKernelHeapInfo();
    if (bFull || !bKern) {
        DisplayProcessInfo(bFull);
        DisplayOtherMemory(bFull);
        FmtPuts(L"\r\nTotal R/W data + stack + RAM code = %ld\r\n", TotalCpgRW);
    }
    SetProcPermissions(oldPerm);
    SetKMode(oldMode);
    return TRUE;
}


DWORD
GetIdFromHandle(
    DWORD dwHandle,
    BOOL *fMod,
    BOOL *fFound
    )
{
    DWORD i;

    *fFound = FALSE;

    //  Couldn't convert wide-string to unsigned long
    if (0==dwHandle)
        return 0xffffffff;
    
    for (i = 0; i < MAX_MODULES; i++) {
        if (0 == rgModData[i].dwModid) {
            break;
        }
        if (dwHandle == rgModData[i].dwModid) {
            *fMod = TRUE;
            *fFound = TRUE;
            return i;
        }
    }

    for (i = 0; i < 32; i++) {
        if (dwHandle == rgProcData[i].dwProcid) {
            *fMod = FALSE;
            *fFound = TRUE;
            return i;
        }
    }

    return 0xffffffff;
}


DWORD
GetIdFromName(
    PTSTR szName,
    BOOL  fMod
    )
{
    HANDLE hSnap;
    PROCESSENTRY32 proc;
    MODULEENTRY32 mod;
    DWORD dwRet;
    BOOL fFound;
    BOOL fMod1;
    
    
    if (fMod) {
        dwRet = sizeof(rgModData)/sizeof(rgModData[0]);
    
        hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_GETALLMODS,0);
        if (INVALID_HANDLE_VALUE != hSnap) {
            mod.dwSize = sizeof(mod);
            if (Module32First(hSnap,&mod)) {
                do {
                    if (IsPatMatch(szName, mod.szModule)) {
                        dwRet = GetIdFromHandle(mod.th32ModuleID, &fMod1, &fFound);
                        break;
                    }
                } while (Module32Next(hSnap,&mod));
            }
            CloseToolhelp32Snapshot(hSnap);
        }
    } else {
        dwRet = sizeof(rgProcData)/sizeof(rgProcData[0]);
        
        hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS | TH32CS_SNAPNOHEAPS ,0);
        if (INVALID_HANDLE_VALUE != hSnap) {
            proc.dwSize = sizeof(proc);
            if (Process32First(hSnap,&proc)) {
                dwRet = 0;
                do {
                    if (IsPatMatch(szName, proc.szExeFile)) {
                        break;
                    }
                    dwRet++;
                } while (Process32Next(hSnap,&proc));
            }
            CloseToolhelp32Snapshot(hSnap);
        }
    }
    
    return dwRet;
}


DWORD
GetHandleFromName(
    PTSTR szName,
    BOOL  fMod
    )
{
    HANDLE hSnap;
    PROCESSENTRY32 proc;
    MODULEENTRY32 mod;
    DWORD dwRet;
    
    
    if (fMod) {
        dwRet = sizeof(rgModData)/sizeof(rgModData[0]);
    
        hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_GETALLMODS,0);
        if (INVALID_HANDLE_VALUE != hSnap) {
            mod.dwSize = sizeof(mod);
            if (Module32First(hSnap,&mod)) {
                dwRet = 0;
                do {
                    if (IsPatMatch(szName, mod.szModule)) {
                        dwRet = mod.th32ModuleID;
                        break;
                    }
                    dwRet++;
                } while (Module32Next(hSnap,&mod));
            }
            CloseToolhelp32Snapshot(hSnap);
        }
    } else {
        dwRet = sizeof(rgProcData)/sizeof(rgProcData[0]);
        
        hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS | TH32CS_SNAPNOHEAPS ,0);
        if (INVALID_HANDLE_VALUE != hSnap) {
            proc.dwSize = sizeof(proc);
            if (Process32First(hSnap,&proc)) {
                dwRet = 0;
                do {
                    if (IsPatMatch(szName, proc.szExeFile)) {
                        dwRet = proc.th32ProcessID;
                        break;
                    }
                    dwRet++;
                } while (Process32Next(hSnap,&proc));
            }
            CloseToolhelp32Snapshot(hSnap);
        }
    }
    
    return dwRet;
}

BOOL DoZoneOps (PTSTR cmdline) {
    PTSTR   szId, szMod, szRest;
    DWORD   dwId, dwZone;
    BOOL    bRetVal=FALSE, fMod = FALSE, fHandle = FALSE, fFound = FALSE;
    DBGPARAM    dbg;
    LPDBGPARAM  lpdbg = &dbg;
    if ((szMod = _tcstok(cmdline, TEXT(" \t"))) != NULL &&
        (szId = _tcstok(NULL, TEXT(" \t"))) != NULL) {
        // get module vs proc vs handle
        if (TEXT('m')==szMod[0] || TEXT('M')==szMod[0])
            fMod = TRUE;
        else if (TEXT('p')==szMod[0] || TEXT('P')==szMod[0])
            fMod = FALSE;
        else if (TEXT('h')==szMod[0] || TEXT('H')==szMod[0])
            fHandle = TRUE;
        else {
            ERRORMSG(1, (TEXT("DoZoneOp: Invalid module string %s!\r\n"), szMod));
            goto done;
        }                    
        // parse ID
        if (TRUE==fHandle) {
            dwId = GetIdFromHandle(wcstoul(szId,NULL,16),&fMod,&fFound);
        } else if (isdigit(szId[0])) {
            dwId = _wtol(szId);
        } else {
            dwId = GetIdFromName(szId, fMod);
        }
        if ((TRUE==fHandle) && (FALSE==fFound)) {
            ERRORMSG(1, (TEXT("DoZoneOp: Invalid handle %s!\r\n"), szId));
            goto done;
        } else if (dwId >= (DWORD)(fMod?cNumMod:cNumProc)) {
            ERRORMSG(1, (TEXT("DoZoneOp: Invalid Id %s!\r\n"), szId));
            goto done;
        }
        // Initialize dwZone to the current value
        dwZone = fMod? rgModData[dwId].dwZone : rgProcData[dwId].dwZone;
        // Check what the rest of cmd line is about
        szRest = _tcstok(NULL, TEXT(" \t"));
        if (!szRest) {
            dwZone = 0xffffffff;
            // It's a get information call ...
        } else if (TEXT('0')<=szRest[0] && TEXT('9')>=szRest[0]) {
            // It's a zone set with an absolute zonemask
            dwZone = _wtol(szRest);
            DEBUGMSG(ZONE_INFO, (TEXT("Shell:DoSetZone:id:%08X;zone:%08X\r\n"),
                        dwId, dwZone));
        } else if (TEXT('o')==szRest[0] || TEXT('O')==szRest[0]) {
            BOOL    fOn;
            DWORD   bitmask;
            // go into a loop extracting further tokens
            while (szRest) {
                fOn = !_tcscmp(szRest, TEXT("on"));
                while ((szRest=_tcstok(NULL, TEXT(" \t"))) &&
                       (TEXT('o')!=szRest[0] && TEXT('O')!=szRest[0]) ) { 
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
            bRetVal = SetDbgZone(0,(LPVOID)rgModData[dwId].dwModid,0,dwZone,lpdbg);
        else
            bRetVal = SetDbgZone(rgProcData[dwId].dwProcid,0,0,dwZone,lpdbg);
        if (!bRetVal)
            goto done;
        if (dwZone == 0xffffffff)
            dwZone = lpdbg->ulZoneMask;
        if (fMod)
            rgModData[dwId].dwZone = dwZone;
        else
            rgProcData[dwId].dwZone = dwZone;
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

BOOL DoBreak (PTSTR cmdline) {
    DebugBreak();
    return TRUE;
}

BOOL DoResFilter (PTSTR cmdline) {
    PTSTR   szOption;
    int     iOn = 0;
    DWORD   dwFlags = 0, dwType = 0, dwProcID = 0;
    for (szOption=_tcstok(cmdline, TEXT(" \t")); szOption; szOption=_tcstok(NULL, TEXT(" \t"))) {
        switch (szOption[0]) {
        case L'd':
            if (szOption[1]==L'0') dwFlags |= FILTER_TYPEDEFAULTOFF;
            if (szOption[1]==L'1') dwFlags |= FILTER_TYPEDEFAULTON;
            break;
        case L'+':
            iOn = 1;
            goto parsetype;
        case L'-':
            iOn = -1;  
parsetype:
            if (szOption[1] == L't') 
            {                       
                dwType = _wtol(szOption+2);
                if (iOn == 1)
                {
                    dwFlags |= FILTER_TYPEON;
                    dwFlags &= ~FILTER_TYPEOFF;
                }
                else
                {
                    dwFlags |= FILTER_TYPEOFF;
                    dwFlags &= ~FILTER_TYPEON;
                }
            }
            else if (szOption[1] == L'p')
            {
                dwProcID = _wtol(szOption+2);
                if (!iOn) {
                    FmtPuts (TEXT("Invalid option:Must type + or - before <%s> - type ? for help.\r\n"), szOption);
                    goto ret;
                }
                dwFlags |= (iOn==1?FILTER_PROCIDON:FILTER_PROCIDOFF);
            }
            break;
      
        default:
            FmtPuts(TEXT("Invalid option %s - type ? for help.\r\n"), szOption);
            goto ret;
        }
    }
    if (dwFlags) {
        FilterTrackedItem(dwFlags, dwType, dwProcID);
    }        
    RegisterTrackedItem(NULL);
ret:    
    return TRUE;
}

BOOL DoResDump (PTSTR cmdline) {
    PTSTR   szOption;
    DWORD   dwFlags = PRINT_ALL;
    DWORD   dwType = 0, dwProcID = 0, dwHandle = 0;
    for (szOption=_tcstok(cmdline, TEXT(" \t")); szOption; szOption=_tcstok(NULL, TEXT(" \t"))) {
        switch (szOption[0]) {
        case L'i':
            dwFlags = dwFlags&~PRINT_ALL;
            dwFlags |= PRINT_RECENT;
            break;
        case L'c':
            dwFlags |= PRINT_SETCHECKPOINT;
            break;            
        case L'v':
            if (szOption[1]==0) {
                dwFlags &= 0x0000FFFF;
                dwFlags |= PRINT_DETAILS;
            } else
                dwFlags |= ((wcstoul(szOption+1, 0, 16) << 16) | PRINT_DETAILS);
            break;
        case L's':
            dwFlags |= PRINT_TRACE;
            break;
        case L't': 
            dwType = _wtol(szOption+1);
            dwFlags |= PRINT_FILTERTYPE;
            break;
        case L'p': 
            dwProcID = _wtol(szOption+1);
            dwFlags |= PRINT_FILTERPROCID;
            break;
        case L'h': 
            dwHandle = _wtol(szOption+1);
            dwFlags |= PRINT_FILTERHANDLE;
            break;
        default:
            FmtPuts(TEXT("Invalid option %s - type ? for help.\r\n"), szOption);
            goto ret;
        }
    }
    PrintTrackedItem(dwFlags, dwType, dwProcID, (HANDLE)dwHandle);
ret:    
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
    TCHAR szImageName[80];
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
#define READ_SIZE   1024
BOOL DoRunBatchFile(PTSTR cmdline) {
    PTSTR   szFile;
    int     fd;
    int     len=0;
    int     len2=0;
    int     iBOL=0;
    int     iEOL=0;
    unsigned char   cBuf[READ_SIZE];
    TCHAR wsz[256];
    if (cmdline == NULL) {
        Puts(TEXT("Syntax: run <filename>\r\n"));
        return TRUE;
    }
    szFile=_tcstok(cmdline, TEXT(" \t"));
    if( -1 == (fd = UU_ropen( szFile, 0x10000 )) ) {
        _snwprintf (wsz, ARRAYSIZE(wsz), L"Unable to open input file %s\r\n", szFile);
        wsz[ARRAYSIZE(wsz) - 1] = 0;
        Puts (wsz);
        return TRUE;
    }
    while ((len=UU_rread( fd, &cBuf[len2], READ_SIZE-len2 ))) {
        len+= len2;
        do {
            while (iEOL < len && cBuf[iEOL] && cBuf[iEOL] != '\r' && cBuf[iEOL] != '\n')
                iEOL++;
            if (iEOL < len && (cBuf[iEOL] == '\r' || cBuf[iEOL] == '\n')) {
                cBuf[iEOL]='\0';
                MultiByteToWideChar(CP_ACP, 0, &cBuf[iBOL], -1, wsz, sizeof(wsz)/sizeof(TCHAR));
                DoRunProcess(wsz);
                while (++iEOL < len && cBuf[iEOL] && (cBuf[iEOL] == '\r' || cBuf[iEOL] == '\n'));
                iBOL=iEOL;
            }
        } while (iEOL< len);
        memcpy(cBuf,&cBuf[iBOL],len-iBOL);
        cBuf[len-iBOL]='\0';
        len2=len-iBOL;
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

BOOL NumberArg(PTSTR *ppCmdLine, int *num) {
    PTSTR pCmdLine;
    BOOL bDig = FALSE, bHex = FALSE;
    int x;

    pCmdLine = *ppCmdLine;

    if (pCmdLine == NULL || *pCmdLine == 0)
        return FALSE;
        
    x = _wtol(pCmdLine);
    
    if (*pCmdLine == '-' || *pCmdLine == '+')
        pCmdLine++;
    if (pCmdLine[0] == '0' && pCmdLine[1] == 'x') {
        pCmdLine += 2;
        bHex = TRUE;
    }
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

typedef HWND (WINAPI *PFN_GetWindow) (HWND hwnd, UINT uCmd);
typedef LONG (WINAPI *PFN_GetWindowLong) (HWND hWnd, int nIndex);
typedef int  (WINAPI *PFN_GetWindowText) (HWND hWnd, LPWSTR lpString, int nMaxCount);
typedef int  (WINAPI *PFN_GetClassName) (HWND hWnd, LPWSTR lpClassName, int nMaxCount);
typedef BOOL (WINAPI *PFN_GetWindowRect) (HWND hwnd, LPRECT prc);
typedef DWORD (WINAPI *PFN_GetWindowThreadProcessId) (HWND hWnd, LPDWORD lpdwProcessId);
typedef HWND (WINAPI *PFN_GetParent) (HWND hwnd);
typedef BOOL (WINAPI *PFN_GetForegroundInfo) (GET_FOREGROUND_INFO *pgfi);

   
PFN_GetWindow       v_pfnGetWindow;
PFN_GetWindowLong   v_pfnGetWindowLong;
PFN_GetWindowText   v_pfnGetWindowText;
PFN_GetClassName    v_pfnGetClassName;
PFN_GetWindowRect   v_pfnGetWindowRect;
PFN_GetWindowThreadProcessId    v_pfnGetWindowThreadProcessId;
PFN_GetParent       v_pfnGetParent;
PFN_GetForegroundInfo   v_pfnGetForegroundInfo;

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
    DWORD style, exstyle, i;
    BOOL fPrintedExStyle;
    TCHAR   szOutStr[256];

    Indent(iIndent);
    if (v_pfnGetWindowText (hwnd, szBuf, sizeof(szBuf)/sizeof(TCHAR))) {
        wsprintf (szOutStr, TEXT("\"%s\"\r\n"), szBuf);
    } else {
        wsprintf (szOutStr, TEXT("<No name>\r\n"));
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

        
        if (!v_pfnGetWindow || !v_pfnGetWindowLong || !v_pfnGetWindowText ||
           !v_pfnGetClassName || !v_pfnGetWindowRect || !v_pfnGetWindowThreadProcessId ||
           !v_pfnGetParent || !v_pfnGetForegroundInfo) {
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


BOOL DoLog (PTSTR cmdline) {
    DWORD dwZoneUser    = 0xFFFFFFFF;
    DWORD dwZoneCE      = 0xFFFFFFFF;
    DWORD dwZoneProcess = 0xFFFFFFFF;
    DWORD dwZonesAvail;
    DWORD fIsProcNum = FALSE;

    if ((! cmdline)
        || (((*cmdline == 'p') || (*cmdline == 'P')) ? (++cmdline, (fIsProcNum = TRUE), FALSE) : FALSE)
        || (! NumberArg(&cmdline, &dwZoneCE))
        || ((NumberArg(&cmdline, &dwZoneUser)) && (NumberArg(&cmdline, &dwZoneProcess))
            && (fIsProcNum ? ((DWORD)cNumProc <= dwZoneProcess) : FALSE))) {
        Puts(TEXT("Syntax: log <hex CE zone> [hex user zone] [hex process mask]\r\n"));
        Puts(TEXT("        (omitted zones default to 0xFFFFFFFF)\r\n"));
        Puts(TEXT("Examples: log 0x63 0xFFFFFFFF 0xFFFFFFFF\r\n"));
        Puts(TEXT("          log 0x63\r\n\r\n"));
    } else {
        if (fIsProcNum && (dwZoneProcess < MAX_PROCESSES))
            dwZoneProcess = rgProcData[dwZoneProcess].dwAccess;

        CeLogSetZones (dwZoneUser, dwZoneCE, dwZoneProcess);
        CeLogReSync();
    }

    // Print result
    CeLogGetZones (&dwZoneUser, &dwZoneCE, &dwZoneProcess, &dwZonesAvail);
    FmtPuts (TEXT("Current CeLog zones:  CE=0x%X User=0x%X Process=0x%X\r\n"),
             dwZoneCE, dwZoneUser, dwZoneProcess);
    FmtPuts (TEXT("Available zones:  0x%X\r\n\r\n"), dwZonesAvail);

    return TRUE;
}

BOOL DoMemTrack (PTSTR cmdline) {
    BOOL fOn;

    if (! cmdline || (! cmdline[0])) {
        Puts(TEXT("Syntax: memtrack {on, off, baseline}\r\n\r\n"));
        return TRUE;
    }

    if (!IsCeLogEnabled()) {
        Puts(TEXT("CeLog is not currently running.  Load CeLog before using memtrack.\r\n\r\n"));
        return TRUE;
    }
    
    if (_tcsnicmp (cmdline, TEXT("on"), 2) == 0) {
        fOn = TRUE;
        cmdline += 2;
    } else if (_tcsnicmp (cmdline, TEXT("off"), 3) == 0) {
        fOn = FALSE;
        cmdline += 3;
    } else if (_tcsnicmp (cmdline, TEXT("baseline"), 8) == 0) {
        CEL_MEMTRACK_BASELINE cel;
        cel.dwReserved = 0;
        CELOGDATA(TRUE, CELID_MEMTRACK_BASELINE, &cel, sizeof(cel), 0, CELZONE_MEMTRACKING);
        return TRUE;        
    } else {
        Puts(TEXT("Syntax: memtrack {on, off, baseline}\r\n\r\n"));
        return TRUE;
    }

    if (fOn) {
        CeLogSetZones (0, CELZONE_MEMTRACKING, 0xffffffff);
        CeLogReSync();
    
    } else {
        DWORD oldMode, oldPerm;
        PPROCESS pProc;
        int loop;
        
        // Send detach messages for all processes...  There is no CeLog resync
        // that does this.

        oldMode = SetKMode(TRUE);
        oldPerm = SetProcPermissions((DWORD)-1);

        pProc = (PPROCESS)UserKInfo[KINX_PROCARRAY];
        for (loop = 0; loop < MAX_PROCESSES; pProc++, loop++) {
            if (pProc->dwVMBase) {
                CEL_MEMTRACK_DETACHP cl;
                cl.hProcess = pProc->hProc;
                CELOGDATA(TRUE, CELID_MEMTRACK_DETACHP, &cl, sizeof(CEL_MEMTRACK_DETACHP), 0, CELZONE_MEMTRACKING);
            }
        }

        SetProcPermissions(oldPerm);
        SetKMode(oldMode);
        
        CeLogSetZones (0, 0, 0);
    }


    return TRUE;
}


DWORD
GetProcessIdFromName(
    PTSTR szName
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
                if (IsPatMatch(szName, proc.szExeFile)) {
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
    TCHAR *token, szImageName[80];
    DWORD dwProc32;
    HANDLE hSnap;
    HEAPLIST32 hl;
    HEAPENTRY32 he;
    DWORD dwTotalBytes;
    DWORD dwTotalAllocs;

    if (cmdline == NULL) {
        goto dhd_syntax;
    }

    if ((token = _tcstok(cmdline, TEXT(" \t"))) == NULL) {
        goto dhd_syntax;
    }

    _tcsncpy(szImageName, token, ARRAYSIZE(szImageName)-1);
    szImageName[ARRAYSIZE(szImageName)-1]=TEXT('\0');
    _tcsncat(szImageName, TEXT(".exe"), ARRAYSIZE(szImageName) - _tcslen(szImageName) - 1);
    dwProc32 = GetProcessIdFromName(szImageName);
    if (dwProc32 == 0xffffffff) {
        Puts(TEXT("Process not found\r\n\r\n"));
        return TRUE;
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
                    do {
                        FmtPuts (TEXT("Process 0x%08x Heap 0x%08x: %5d bytes at 0x%08x\r\n"),
                                 he.th32ProcessID, he.th32HeapID, he.dwBlockSize, he.dwAddress);
                        dwTotalBytes += he.dwBlockSize;
                        dwTotalAllocs++;
                        he.dwSize = sizeof(HEAPENTRY32);
                    } while (Heap32Next(hSnap, &he));
                }

                hl.dwSize = sizeof(HEAPLIST32);
            } while (Heap32ListNext(hSnap, &hl));
            FmtPuts (TEXT("Process 0x%08x total %5d bytes in %d allocations\r\n"),
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
    Puts(TEXT("Syntax: hd <procname>\r\n\r\n"));
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
                if (NumberArg (&token, &dwPri) && (dwPri < 256)) {
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

void PrintHex (DWORD dwOffset, LPBYTE pBytes, DWORD dwLen)
{
    TCHAR szOutStr[256];
    DWORD dwStartOffset, i, j;

    for (dwStartOffset = 0; dwStartOffset < dwLen; dwStartOffset += 16) {
        wsprintf (szOutStr, TEXT("%*.*s0x%08X "), dwOffset, dwOffset, TEXT(""), dwStartOffset);
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


VOID
GetLMemInfo (HANDLE hProc, DWORD dwFlags, DWORD dwHeap)
{
    static HMODULE hLMemDebug;
    static PFN_GetProcessHeapInfo pfnGetProcessHeapInfo;
    static PFN_FreeHeapInfo pfnFreeHeapInfo;
    static PFN_GetRecentAllocs pfnGetRecentAllocs;
    static PFN_GetCurrentAllocs pfnGetCurrentAllocs;
    static PFN_SetAllAllocFlags pfnSetAllAllocFlags;
    PLMEM_HEAP_INFO     pHeap, pTemp;
    PLMEM_ALLOC_INFO    pAlloc, pTempAlloc;
    PLMEM_RECENT        pRecent;
    DWORD               i, dwIndex, dwContentions;
    DWORD               dwOldPerm;
    TCHAR               szOutStr[256];
    TCHAR               szName[256];
    DWORD               dwOffset;

    if (NULL == hLMemDebug) {
        hLMemDebug = (HMODULE)LoadDriver(LMEMDEBUG_DLL);
        if (NULL != hLMemDebug) {
            pfnGetProcessHeapInfo = (PFN_GetProcessHeapInfo)GetProcAddress (hLMemDebug, TEXT("GetProcessHeapInfo"));
            pfnFreeHeapInfo = (PFN_FreeHeapInfo)GetProcAddress (hLMemDebug, TEXT("FreeHeapInfo"));
            pfnGetCurrentAllocs = (PFN_GetCurrentAllocs)GetProcAddress (hLMemDebug, TEXT("GetCurrentAllocs"));
            pfnGetRecentAllocs = (PFN_GetRecentAllocs)GetProcAddress (hLMemDebug, TEXT("GetRecentAllocs"));
            pfnSetAllAllocFlags = (PFN_SetAllAllocFlags)GetProcAddress (hLMemDebug, TEXT("SetAllAllocFlags"));
        }
    }

    if ((NULL != hLMemDebug) && (NULL != pfnGetProcessHeapInfo) && (NULL != pfnGetCurrentAllocs) &&
        (NULL != pfnGetRecentAllocs) && (NULL != pfnFreeHeapInfo) && (NULL != pfnSetAllAllocFlags)) {
        pHeap = pfnGetProcessHeapInfo(hProc);
        
        for (i=0, pTemp = pHeap; NULL != pTemp; pTemp = pTemp->pNext, i++) {
            FmtPuts (TEXT("  Heap(%d) 0x%X TotalAlloc=%d(%d) CurrentAlloc=%d(%d) Peak=%d(%d)\r\n"), i,
                     (DWORD)(pTemp->hHeap), pTemp->dwTotalAlloc, pTemp->dwCountAlloc, pTemp->dwCurrentAlloc,
                     pTemp->dwCurrentCount, pTemp->dwMaxAlloc, pTemp->dwMaxCount);

            if (dwFlags & LMEM_OPT_HIST) {
                Puts(TEXT("    Histogram of alloc size\r\n"));
                for (dwIndex=0; dwIndex < HEAP_HIST_BUCKETS; dwIndex++) {
                    FmtPuts (TEXT("    %5d-%5d = %d\r\n"),
                             dwIndex*HEAP_HIST_SIZE, ((dwIndex+1)*HEAP_HIST_SIZE)-1, pTemp->dwHist[dwIndex]);
                }
            }
            if ((-1 == dwHeap) || (dwHeap == i) || (dwHeap == (DWORD)pTemp->hHeap)) {
                Puts(TEXT("    Show all allocations\r\n"));
                pAlloc = pfnGetCurrentAllocs(hProc, pTemp->hHeap);
                for (pTempAlloc = pAlloc; NULL != pTempAlloc; pTempAlloc = pTempAlloc->pNext) {
                    if (!(dwFlags & LMEM_OPT_DELTA) || !(pTempAlloc->wFlags & LMEM_ALLOC_CHKPOINT)) {
                        switch (pTempAlloc->dwSize) {
                        case LMEM_ALLOC_SIZE_CS :
                            dwOldPerm = SetProcPermissions(-1);
                            DISABLEFAULTS();
                            try {
                                LPCRITICAL_SECTION pCS = (LPCRITICAL_SECTION)MapPtrToProcess(pTempAlloc->lpMem, hProc);
                                dwContentions = pCS->dwContentions;
                            } __except (EXCEPTION_EXECUTE_HANDLER) {
                                dwContentions = (DWORD)-1;
                            }
                            ENABLEFAULTS();
                            SetProcPermissions(dwOldPerm);
                            wsprintf (szOutStr, TEXT("      Ptr=0x%08X %s(%d) Count=%d hThrd=0x%08X LineNum=%d File=%hs\r\n"),
                                      (DWORD)pTempAlloc, TEXT("*CritSect*"), dwContentions,
                                      pTempAlloc->dwAllocCount, (DWORD)pTempAlloc->hThread, pTempAlloc->dwLineNum,
                                      pTempAlloc->szFilename ? pTempAlloc->szFilename : "");
                            break;
                        case LMEM_ALLOC_SIZE_EVENT :
                        case LMEM_ALLOC_SIZE_FILE :
                            wsprintf (szOutStr, TEXT("      Ptr=0x%08X %s Count=%d hThrd=0x%08X LineNum=%d File=%hs\r\n"),
                                      (DWORD)pTempAlloc, (LMEM_ALLOC_SIZE_EVENT == pTempAlloc->dwSize) ? TEXT("*Event*") : TEXT("*File*"),
                                      pTempAlloc->dwAllocCount, (DWORD)pTempAlloc->hThread, pTempAlloc->dwLineNum,
                                      pTempAlloc->szFilename ? pTempAlloc->szFilename : "");
                            break;
                        default :
                            wsprintf (szOutStr, TEXT("      Ptr=0x%08X Size=%d Count=%d hThrd=0x%08X LineNum=%d File=%hs\r\n"),
                                      (DWORD)pTempAlloc, pTempAlloc->dwSize, pTempAlloc->dwAllocCount, (DWORD)pTempAlloc->hThread, pTempAlloc->dwLineNum,
                                      pTempAlloc->szFilename ? pTempAlloc->szFilename : "");
                            break;
                        }
                        
                        Puts (szOutStr);
                        if ((dwFlags & LMEM_OPT_INFO) && (pTempAlloc->wFlags & LMEM_ALLOC_ADDITIONAL)) {
                            LPBYTE pAdditional = (LPBYTE) (pTempAlloc + 1);
                            LPDWORD pDWord;
                            WORD wLen, wType, wOffset;
                            do {
                                wType = *((LPWORD)pAdditional)++;
                                wLen = *((LPWORD)pAdditional)++;
                                switch (wType) {
                                case LMEM_TYPE_END:
                                    break;
                                case LMEM_TYPE_UNICODE :
                                    FmtPuts (TEXT("        StringW='%s'\r\n"), (LPTSTR)pAdditional);
                                    break;
                                case LMEM_TYPE_ASCII :
                                    FmtPuts (TEXT("        StringA='%hs'\r\n"), (LPSTR)pAdditional);
                                    break;
                                case LMEM_TYPE_STACK :
                                    // Really just and array of DWORDs.  Print out.
                                    pDWord = (PDWORD)pAdditional;
                                    for (wOffset = 0; wOffset < wLen; wOffset += sizeof(DWORD)) {
                                        GetProcModOffset (*pDWord, szName, sizeof(szName)/sizeof(szName[0]), &dwOffset);
                                        FmtPuts (TEXT("        Stack=0x%08X %s+0x%X (%hs)\r\n"),
                                            *pDWord++, szName, dwOffset, (dwFlags & LMEM_OPT_MAP) ? 
                                            MapfileLookupAddress(szName, dwOffset) : "");
                                    }
                                    break;
                                case LMEM_TYPE_HEX :
                                    PrintHex (10, pAdditional, wLen);
                                    break;
                                }

                                pAdditional += wLen;
                            } while (LMEM_TYPE_END != wType);
                        }
                    }
                }
                if (pAlloc) {
                    pfnFreeHeapInfo(pAlloc);
                }
            }
            Puts (TEXT("\r\n"));

            if (dwFlags & LMEM_OPT_CHK) {
                pfnSetAllAllocFlags (hProc, pTemp->hHeap, LMEM_SET_ALLOC_FLAG_SET, LMEM_ALLOC_CHKPOINT);
            }
        }

        if (dwFlags & LMEM_OPT_RECENT) {
            Puts(TEXT("  Recent allocations\r\n"));
            if (pRecent = pfnGetRecentAllocs (hProc, &dwIndex)) {
                for (i=0; i < dwIndex; i++) {
                    switch (pRecent[i].dwSize) {
                    case LMEM_ALLOC_SIZE_EVENT :
                    case LMEM_ALLOC_SIZE_CS :
                    case LMEM_ALLOC_SIZE_FILE :
                        wsprintf (szOutStr, TEXT("    Heap 0x%X Ticks %d pData=0x%08X %s Line=%d File=%hs\r\n"),
                                  (DWORD)(pRecent[i].hHeap), GetTickCount() - pRecent[i].dwTickCount,
                                  (DWORD)pRecent[i].pData, (LMEM_ALLOC_SIZE_EVENT == pRecent[i].dwSize) ? TEXT("*Event*") :
                                  (LMEM_ALLOC_SIZE_CS == pRecent[i].dwSize) ? TEXT("*CritSect*") : TEXT("*File*"),
                                  pRecent[i].dwLineNum, pRecent[i].szFilename);
                        break;
                    default :
                        wsprintf (szOutStr, TEXT("    Heap 0x%X Ticks %d pData=0x%08X Size=%d Line=%d File=%hs\r\n"),
                                  (DWORD)(pRecent[i].hHeap), GetTickCount() - pRecent[i].dwTickCount,
                                  (DWORD)pRecent[i].pData, pRecent[i].dwSize, 
                                  pRecent[i].dwLineNum, pRecent[i].szFilename);
                        break;
                    }
                    Puts(szOutStr);
                }
                     
                pfnFreeHeapInfo(pRecent);
            }
            Puts (TEXT("\r\n"));
        }
        Puts (TEXT("\r\n"));
        
        if (pHeap) {
            pfnFreeHeapInfo(pHeap);
        }
    }
}

BOOL DoLMem (PTSTR cmdline)
{
    PTSTR   szProc, szTok;
    DWORD   hProc;
    DWORD   dwFlags = 0;
    BOOL    fBreakSize = FALSE;
    DWORD   dwBreakSize = 0;
    BOOL    fBreakCnt = FALSE;
    DWORD   dwBreakCnt = 0;
    DWORD   dwHeap = (DWORD)-2;
    HANDLE  hSnap;
    PROCESSENTRY32 proc;

    if (NULL == (szProc = _tcstok(cmdline, TEXT(" \t")))) {
        Puts(TEXT("LMem <proc>|* [recent] [hist] [chk] [delta] [info] [<heap>|*]\r\n"));
        return TRUE;
    }
    if (TEXT('*') == szProc[0]) {
        hProc = (DWORD)-1;
    } else if (isdigit(szProc[0])) {
        hProc = _wtol(szProc);
        if (hProc < 32) {
            // Look it up in the recent "gi proc" info
            hProc = rgProcData[hProc].dwProcid;
        }
    } else {
        hProc = GetHandleFromName (szProc, FALSE);
    }

    while (szTok = _tcstok(NULL, TEXT(" \t"))) {
        if (!_tcsicmp (szTok, TEXT("hist"))) {
            dwFlags |= LMEM_OPT_HIST;
        } else if (!_tcsicmp (szTok, TEXT("recent"))) {
            dwFlags |= LMEM_OPT_RECENT;
        } else if (!_tcsicmp (szTok, TEXT("chk"))) {
            dwFlags |= LMEM_OPT_CHK;
        } else if (!_tcsicmp (szTok, TEXT("delta"))) {
            dwFlags |= LMEM_OPT_DELTA;
            if (-2 == dwHeap) {
                dwHeap = -1;
            }
        } else if (!_tcsicmp (szTok, TEXT("info"))) {
            dwFlags |= LMEM_OPT_INFO;
        } else if (!_tcsicmp (szTok, TEXT("map"))) {
            dwFlags |= (LMEM_OPT_MAP | LMEM_OPT_INFO);
        } else if (!_tcsicmp (szTok, TEXT("critsec_on"))) {
            dwFlags |= LMEM_OPT_CSON;
        } else if (!_tcsicmp (szTok, TEXT("critsec_off"))) {
            dwFlags |= LMEM_OPT_CSOFF;
        } else if (!_tcsicmp (szTok, TEXT("breaksize"))) {
            if (NULL == (szTok = _tcstok(NULL, TEXT(" \t")))) {
                break;
            }
            fBreakSize = TRUE;
            dwBreakSize = _wtol(szTok);
        } else if (!_tcsicmp (szTok, TEXT("breakcnt"))) {
            if (NULL == (szTok = _tcstok(NULL, TEXT(" \t")))) {
                break;
            }
            fBreakCnt = TRUE;
            dwBreakCnt = _wtol(szTok);
        } else if (TEXT('*') == szTok[0]) {
            dwHeap = -1;
        } else if (isdigit(szTok[0])) {
            dwHeap = _wtol(szTok);
        } else {
            FmtPuts (TEXT("Unexpected parameter '%s', ignored\r\n"), szTok);
        }
    }
    
    if ((DWORD)-1 == hProc) {
        hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS | TH32CS_SNAPNOHEAPS ,0);
        if (INVALID_HANDLE_VALUE != hSnap) {
            proc.dwSize = sizeof(proc);
            if (Process32First(hSnap,&proc)) {
                do {
                    hProc = proc.th32ProcessID;

                    FmtPuts (TEXT("Info for process '%s' 0x%08X\r\n"), proc.szExeFile, hProc);
                    GetLMemInfo ((HANDLE)hProc, dwFlags, dwHeap);
                    if (fBreakSize || fBreakCnt) {
                        CallSetLMEMDbgOptions ((HANDLE)hProc, LMEM_SET_ALLOC_FLAG_SET, 0, dwBreakCnt, dwBreakSize);
                    }
                    if (dwFlags & (LMEM_OPT_CSON | LMEM_OPT_CSOFF)) {
                        DWORD dwOptions;
                        CallGetLMEMDbgOptions ((HANDLE)hProc, &dwOptions, &dwBreakCnt, &dwBreakSize);
                        if (dwFlags & LMEM_OPT_CSON) {
                            dwOptions |= LMEMDBG_SHOW_CS_CONTENTIONS;
                        } else {
                            dwOptions &= ~(LMEMDBG_SHOW_CS_CONTENTIONS);
                        }
                        CallSetLMEMDbgOptions ((HANDLE)hProc, LMEM_SET_ALLOC_FLAG_ASSIGN, dwOptions, dwBreakCnt, dwBreakSize);
                    }
                } while (Process32Next(hSnap,&proc));
            }
            CloseToolhelp32Snapshot(hSnap);
        }
    } else {
        GetLMemInfo ((HANDLE)hProc, dwFlags, dwHeap);
        if (fBreakSize || fBreakCnt) {
            CallSetLMEMDbgOptions ((HANDLE)hProc, LMEM_SET_ALLOC_FLAG_SET, 0, dwBreakCnt, dwBreakSize);
        }
    }
        

    return TRUE;
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


static VOID DoProfileHelp()
{
    Puts(TEXT("Syntax: prof <on|off> [data_type] [storage_type]\r\n"));
    Puts(TEXT("data_type, one of: -m or -s or -k\r\n"));
    Puts(TEXT("    -m: Gather Monte Carlo data (time-based sampling) (DEFAULT)\r\n"));
    Puts(TEXT("    -s: Gather System Call data (call-based sampling)\r\n"));
    Puts(TEXT("    -k: Gather Kernel Call data (call-based sampling)\r\n"));
    Puts(TEXT("storage_type, one of: -b or -u or -l\r\n"));
    Puts(TEXT("    -b: Buffered (DEFAULT)\r\n"));
    Puts(TEXT("    -u: Unbuffered\r\n"));
    Puts(TEXT("    -l: Send data to CeLog\r\n"));
    Puts(TEXT("Examples: prof on         - start profiler, Monte Carlo data in buffered mode\r\n"));
    Puts(TEXT("          prof on -s -u   - start profiler, SysCall data in unbuffered mode\r\n"));
    Puts(TEXT("          prof off        - stop profiler\r\n\r\n"));
}


BOOL DoProfile (PTSTR cmdline)
{
    PTSTR szToken;
    WORD  wAction;  // 0=error 1=on 2=off
    DWORD dwProfilerFlags = PROFILE_BUFFER;
    DWORD dwProfilerInterval  = 200;

    if (cmdline
        && (szToken = _tcstok(cmdline, TEXT(" \t")))
        && (wAction = (!_tcsicmp (szToken, TEXT("on")) ? 1 : (!_tcsicmp (szToken, TEXT("off")) ? 2 : 0)))
        ){

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
                } else {
                    DoProfileHelp();
                    return TRUE;
                }
            }
            
            ProfileStart(dwProfilerInterval, dwProfilerFlags);
            
        } else {
            // PROFILE OFF
            ProfileStop();
        }
        
    } else {
        DoProfileHelp();
    }

    return TRUE;
}

BOOL DoLoadExt (PTSTR cmdline)
{
    PSHELL_EXTENSION    pShellExt, pShellExtPrev;
    PTSTR               szToken;

    while (szToken = _tcstok(cmdline, TEXT(" \t"))) {
        cmdline = NULL; // Set to null for second time through

        if (!_tcscmp (szToken, TEXT("-u"))) {
            // Get the DLL Name
            if (szToken = _tcstok(cmdline, TEXT(" \t"))) {
                pShellExtPrev = NULL;
                for (pShellExt = v_ShellExtensions; NULL != pShellExt; pShellExt = pShellExt->pNext) {
                    if (!_tcsicmp(pShellExt->szDLLName, szToken)) {
                        // First remove it from the list.
                        if (pShellExtPrev) {
                            pShellExtPrev = pShellExt->pNext;
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
