//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// UsrExceptDmp.cpp

#include <windows.h>
#include "UsrExceptDmp.h"


// TODO: use folder for ued files
#define DUMP_FILE_NAME_BASE L"Dump"
#define DUMP_FILE_NAME_EXT L".ued"
#define ABS_MAX_UDUMP_FILES (100) // This is dependent on the number of digits used for the dump file
#define DEFAULT_MAX_UDUMP_FILES (10) // Cannot be larger than ABS_MAX_UDUMP_FILES


LPCTSTR g_pszDumpFileName   = DUMP_FILE_NAME_BASE L"%02i" DUMP_FILE_NAME_EXT;
LPCTSTR g_pszDumpFileNameGen    = DUMP_FILE_NAME_BASE L"NN" DUMP_FILE_NAME_EXT;


LPCTSTR g_pszMaxUDumpFilesKey    = L"Debug";
LPCTSTR g_pszMaxUDumpFilesValue = L"uDumpMaxFiles";
LPCTSTR g_pszLastUDumpFileIndexKey    = L"Debug";
LPCTSTR g_pszLastUDumpFileIndexValue = L"uDumpFileIdx";

// Create new dump file 
HANDLE CreateDumpFile (void) 
{
    HANDLE hFile = INVALID_HANDLE_VALUE;
    BYTE bMaxDumpFiles = DEFAULT_MAX_UDUMP_FILES; // Default number of crash dump kept
    
    // Read registry for alternate max dump file
    HKEY hKey;
    LONG nRet = ::RegOpenKeyEx (HKEY_LOCAL_MACHINE, g_pszMaxUDumpFilesKey, 0, 0, &hKey);
    if (ERROR_SUCCESS == nRet)
    {
        DWORD dwTemp;
        DWORD dwRegType = REG_DWORD;
        DWORD dwRegSize = sizeof (DWORD);
        nRet = ::RegQueryValueEx (hKey, g_pszMaxUDumpFilesValue, 0, &dwRegType, (BYTE *) &dwTemp, &dwRegSize);
        if ((ERROR_SUCCESS == nRet) && (dwTemp <= ABS_MAX_UDUMP_FILES) && (REG_DWORD == dwRegType) && (sizeof (DWORD) == dwRegSize))
        {
            bMaxDumpFiles = (BYTE) dwTemp;
        }
        ::RegCloseKey (hKey);
    }

    if (bMaxDumpFiles)
    {
        BYTE bDumpFileIndex = 0;
        
        // Read registry for a current index, create if not existing
        DWORD dwDisp;
        LONG nRet = ::RegCreateKeyEx (HKEY_LOCAL_MACHINE, g_pszLastUDumpFileIndexKey, 0, L"", 0, 0, NULL, &hKey, &dwDisp);
        if (ERROR_SUCCESS == nRet)
        {
            if (REG_OPENED_EXISTING_KEY == dwDisp)
            {
                DWORD dwTemp;
                DWORD dwRegType = REG_DWORD;
                DWORD dwRegSize = sizeof (DWORD);
                nRet = ::RegQueryValueEx (hKey, g_pszLastUDumpFileIndexValue, 0, &dwRegType, (BYTE *) &dwTemp, &dwRegSize);
                if ((ERROR_SUCCESS == nRet) && (dwTemp <= ABS_MAX_UDUMP_FILES) && (REG_DWORD == dwRegType) && (sizeof (DWORD) == dwRegSize))
                {
                    bDumpFileIndex = (BYTE) (dwTemp + 1);
                }
            }
            ::RegCloseKey (hKey);
        }

        if (bDumpFileIndex >= bMaxDumpFiles) bDumpFileIndex = 0;

        WCHAR szFileName [_MAX_PATH];
        ::wsprintf (szFileName, g_pszDumpFileName, bDumpFileIndex);
#ifdef DEBUG
        DEBUGMSG (1, (L"UsrExceptDmp: szFileName=%s\r\n", szFileName));
#endif // DEBUG
        
        hFile = ::CreateFile (szFileName, 
                                        GENERIC_WRITE,
                                        FILE_SHARE_READ,
                                        NULL,
                                        CREATE_ALWAYS,
                                        FILE_ATTRIBUTE_NORMAL,
                                        0);
        if (INVALID_HANDLE_VALUE != hFile)
        {
            // Rewrite registry for current index
            LONG nRet = ::RegCreateKeyEx (HKEY_LOCAL_MACHINE, g_pszLastUDumpFileIndexKey, 0, L"", 0, 0, NULL, &hKey, &dwDisp);
            if (ERROR_SUCCESS == nRet)
            {
                DWORD dwTemp = bDumpFileIndex;
                nRet = ::RegSetValueEx (hKey, g_pszLastUDumpFileIndexValue, 0, REG_DWORD, (BYTE *) &dwTemp, sizeof (DWORD));
                ::RegCloseKey (hKey);
            }
            if (ERROR_SUCCESS != nRet)
            {
                DEBUGMSG (1, (L"UsrExceptDmp: Cannot save File Index in registry.\r\n"));
            }
        }
    }
    
    return hFile;
}


DWORD StringToDWord (LPCTSTR pszString)
{
    DWORD dwRet = 0xFFFFFFFF;

    if (::wcslen (pszString) <= 10)
    {
        if (!::_wcsnicmp (pszString, L"0x", 2))
        { // Hex
           dwRet = 0;
           for (int nIndex = 2; pszString [nIndex]; nIndex ++)
            {
                dwRet *= 16;
                if (pszString [nIndex] >= _T('0') && pszString [nIndex] <= _T('9'))
                {
                    dwRet += (pszString [nIndex] - _T('0'));
                }
                else if (pszString [nIndex] >= _T('a') && pszString [nIndex] <= _T('f'))
                {
                    dwRet += (pszString [nIndex] - _T('a') + 10);
                }
                else if (pszString [nIndex] >= _T('A') && pszString [nIndex] <= _T('F'))
                {
                    dwRet += (pszString [nIndex] - _T('A') + 10);
                }
                else 
                {
                    dwRet = 0xFFFFFFFF; // Invalid hex value
                    break;
                }
            }
        }
    }
    else
    { // Decimal case
        __int64 i64 = _ttoi64 (pszString);
        if (i64 < 0x100000000)
        {
            dwRet = (ULONG) i64;
        }
    }
    return dwRet;
}


#define EVENT_MAX_COUNT (MAX_CRASH_MODULES + 2)
#define EVENT_TIMEOUT (5000)

BOOL AttachAsAppDebugger (DWORD dwPID, DEBUG_EVENT *pDebugEvent)
{
    BOOL fRet = FALSE; // Default is failure to attach
    PROCESS *pProcess = (PROCESS *) UserKInfo [KINX_PROCARRAY];
    DWORD dwProcIdx = 0;
    for (; (dwProcIdx < MAX_PROCESSES) && !(pProcess [dwProcIdx].dwVMBase && (((DWORD) (pProcess [dwProcIdx].hProc)) == dwPID)); dwProcIdx++)
        ;

    if (dwProcIdx < MAX_PROCESSES)
    {
        //  Excepting process found: attaching to it as an app debugger
        if (::DebugActiveProcess (dwPID))
        {
            // Sanity check: this thread should be debugging thread
            if (::GetCurrentThreadId () == (DWORD) pProcess [dwProcIdx].hDbgrThrd)
            {
                //  wait for debug events.
                DEBUG_EVENT *pDebugEvent2 = pDebugEvent;
                DWORD dwEvCount = EVENT_MAX_COUNT;
                while (::WaitForDebugEvent (pDebugEvent, EVENT_TIMEOUT) && dwEvCount--)
                {
                    if ((pDebugEvent2 == pDebugEvent) && (EXCEPTION_DEBUG_EVENT == pDebugEvent->dwDebugEventCode))
                    {
                        // Check for consistency:
                        if (dwPID == pDebugEvent->dwProcessId)
                        {
                            fRet = TRUE;
                            break;
                        }
                        else
                        {
                            DEBUGMSG (1, (L"UsrExceptDmp: PID/TID does not match DebugEvent.\r\n"));
                        }
                    }
                    else
                    {
                        if (!::ContinueDebugEvent (pDebugEvent->dwProcessId, pDebugEvent->dwThreadId, DBG_EXCEPTION_NOT_HANDLED))
                        {
                            DEBUGMSG (1, (L"UsrExceptDmp : Failed to continue unhandled from non exception event.\r\n")) ;
                        }
                    }
                }          
                if (!fRet)
                {
                    DEBUGMSG (1, (L"UsrExceptDmp: Debug event not received within timeout; Abort.\r\n"));
                }
            }
            else
            {
                DEBUGMSG (1, (L"UsrExceptDmp: The debugging thread is not the current thread.\r\n"));
            }                
        }
        else
        {
            DEBUGMSG (1, (L"UsrExceptDmp: DebugActiveProcess (hProc=0x%08X) failed; Abort.\r\n", dwPID));
        }
    }
    else
    {
        DEBUGMSG (1, (L"UsrExceptDmp: Cannot find excepting process (with PID/hProc=0x%08X); Abort.\r\n", dwPID));
    }

    return fRet;    
}


LPCTSTR g_pszJitDebugKey   = L"Debug";
LPCTSTR g_pszJitDebugValue = L"JITDebugger";
LPCTSTR g_pszTargetExeName = L"UsrExceptDmp.exe";


void RegisterAsDebugger (void)
{
    HKEY hKey;
    DWORD dwDisp;
    LONG nRet = ::RegCreateKeyEx (HKEY_LOCAL_MACHINE, g_pszJitDebugKey, 0, L"", 0, 0, NULL, &hKey, &dwDisp);
    if (ERROR_SUCCESS == nRet)
    {
        nRet = ::RegSetValueEx (hKey, g_pszJitDebugValue, 0, REG_SZ, (BYTE *) g_pszTargetExeName, (::wcslen (g_pszTargetExeName) + 1) * sizeof (WCHAR));
        ::RegCloseKey (hKey);
    }
}


void Usage (void)
{
    DEBUGMSG (1, (L"UsrExceptDmp usage: \"UsrExceptDmp -RegDebugger\" will register UsrExceptDmp\r\n")) ;
    DEBUGMSG (1, (L"    as Just-In-Time user mode debugger. UsrExceptDmp will then be called\r\n")) ;
    DEBUGMSG (1, (L"    automatically from the CE on a 2nd chance exception and a micro-dump\r\n")) ;
    DEBUGMSG (1, (L"    will be generated in %s (NN in [00..09] by default - 99 max.\r\n", g_pszDumpFileNameGen)) ;
}


int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPreviousInstance, LPTSTR pszCmdLine, int nCommandShow)
{
    int iRetVal = 1;
    DWORD dwPID = 0;

    BOOL bOldKMode = ::SetKMode (TRUE);
    DWORD dwOldProcPermissions = ::SetProcPermissions (0xFFFFFFFF);

    __try
    {
        if (::wcsicmp (pszCmdLine, L""))
        { // Not empty       
            if (!::wcsicmp (pszCmdLine, L"-RegDebugger"))
            { // empty command line
                RegisterAsDebugger ();
                iRetVal = 0;
            }
            else
            {
                dwPID = StringToDWord (pszCmdLine);
                if ((0xFFFFFFFF == dwPID) || (0 == dwPID))
                {
                    DEBUGMSG (1, (L"UsrExceptDmp: Invalid PID on command line - %s\r\n", pszCmdLine));
                }
                else
                {
                    DEBUG_EVENT DebugEvent;
                    if (!AttachAsAppDebugger (dwPID, &DebugEvent))
                    {
                        DEBUGMSG (1, (L"UsrExceptDmp : Failed to attach as debugger to PID=0x%08X.\r\n", dwPID));
                    }
                    HANDLE hFile = CreateDumpFile ();
                    if (INVALID_HANDLE_VALUE != hFile)
                    {
                        if (GenerateDumpFileContent (hFile, &DebugEvent))
                        {
                            iRetVal = 0;
                        }
                        else
                        {
                            DEBUGMSG (1, (L"UsrExceptDmp : failed generating dump.\r\n"));
                        }
                        
                        ::CloseHandle (hFile);
                    }
                    else
                    {
                        DEBUGMSG (1, (L"UsrExceptDmp : No dump file created.\r\n"));
                    }
                    if (!::ContinueDebugEvent (DebugEvent.dwProcessId, DebugEvent.dwThreadId, DBG_EXCEPTION_NOT_HANDLED))
                    {
                        DEBUGMSG (1, (L"UsrExceptDmp : Failed to continue unhandled.\r\n"));
                    }
                }
            }
        }
        if (iRetVal)
        {
            Usage ();
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DEBUGMSG (1, (L"UsrExceptDmp: Unexpected exception - bailing out to prevent reentrance.\r\n"));
    }
    
    ::SetProcPermissions (dwOldProcPermissions) ;
    ::SetKMode (bOldKMode) ;

    return iRetVal;
}

