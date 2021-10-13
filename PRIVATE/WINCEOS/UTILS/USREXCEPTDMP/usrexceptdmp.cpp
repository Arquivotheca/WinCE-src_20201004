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
#include "DwPublic.h"

#define ARRAYSIZE(x) (sizeof(x)/sizeof(x[0]))

HRESULT DirectorySize(TCHAR *szDirectory, DWORD64 *pdw64TotalSize)
{
    HRESULT hRes = E_FAIL;
    DWORD dwError;

    WIN32_FIND_DATA *pFindData = NULL;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    TCHAR *szSearchDirectory = NULL;
    TCHAR *pszWildCard = NULL;

    ASSERT((NULL != szDirectory) && (NULL != pdw64TotalSize));
    if ((NULL == szDirectory) || (NULL == pdw64TotalSize))
    {
        DEBUGMSG (1, (L"  UsrExceptDmp!DirectorySize: Bad parameter\r\n"));
        goto Exit;
    }

    pFindData = new WIN32_FIND_DATA;
    ASSERT(NULL != pFindData);
    if (NULL == pFindData)
    {
        DEBUGMSG (1, (L"  UsrExceptDmp!DirectorySize: New WIN32_FIND_DATA failed.\r\n"));
        goto Exit;
    }

    szSearchDirectory = new TCHAR[MAX_PATH];
    ASSERT(NULL != szSearchDirectory);
    if (NULL == szSearchDirectory)
    {
        DEBUGMSG (1, (L"  UsrExceptDmp!DirectorySize: New TCHAR[MAX_PATH] failed.\r\n"));
        goto Exit;
    }

    hRes = StringCchCopy(szSearchDirectory, MAX_PATH, szDirectory);
    if (FAILED(hRes))
    {
        DEBUGMSG (1, (L"  UsrExceptDmp!DirectorySize: StringCchCopy failed, error=0x%08X\r\n", hRes));
        goto Exit;
    }

    // Append backslash if needed
    if (szSearchDirectory[lstrlen(szSearchDirectory)-1] != TEXT('\\'))
    {
        hRes = ::StringCchCat (szSearchDirectory, MAX_PATH, TEXT("\\"));
        if (FAILED(hRes))
        {
            DEBUGMSG (1, (L"  UsrExceptDmp!DirectorySize: StringCchCat failed adding '\\' to search directory, error=0x%08X\r\n", hRes));
            goto Exit;
        }
    }

    pszWildCard = szSearchDirectory+lstrlen(szSearchDirectory);
    
    hRes = ::StringCchCat (szSearchDirectory, MAX_PATH, TEXT("*.*"));
    if (FAILED(hRes))
    {
        DEBUGMSG (1, (L"  UsrExceptDmp!DirectorySize: StringCchCat failed adding '*.*' to search directory, error=0x%08X\r\n", hRes));
        goto Exit;
    }

    *pdw64TotalSize = 0;
    // Find all files in directory
    hFind = FindFirstFile(szSearchDirectory, pFindData);

    // Check if any files found
    if (hFind != INVALID_HANDLE_VALUE)
    {
        // Loop through all files to get total size
        do
        {
            if (!(pFindData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                *pdw64TotalSize += (DWORD64(pFindData->nFileSizeHigh) * MAXDWORD + pFindData->nFileSizeLow);
            }
            else
            {
                if (memcmp(TEXT("."), pFindData->cFileName, sizeof(TCHAR)) && memcmp(TEXT(".."), pFindData->cFileName, sizeof(TCHAR)*2))
                {
                    // Recurse into subdirectories
                    DWORD64 dw64DirSize;
                    hRes = StringCchCopy(pszWildCard, MAX_PATH - (pszWildCard-szSearchDirectory), pFindData->cFileName);
                    if (FAILED(hRes))
                    {
                        DEBUGMSG (1, (L"  UsrExceptDmp!DirectorySize: StringCchCat failed adding directory to search directory, error=0x%08X\r\n", hRes));
                        goto Exit;
                    }

                    hRes = DirectorySize(szSearchDirectory, &dw64DirSize);
                    if (FAILED(hRes))
                    {
                        DEBUGMSG (1, (L"  UsrExceptDmp!DirectorySize: DirectorySize failed, error=0x%08X\r\n", hRes));
                        goto Exit;
                    }
                    *pdw64TotalSize += dw64DirSize;
                }
            }
        } while (FindNextFile(hFind, pFindData));
        
        dwError = GetLastError();
        if (dwError != ERROR_NO_MORE_FILES)
        {
            DEBUGMSG (1, (L"  UsrExceptDmp!DirectorySize: FindNextFile failed, error=0x%08X\r\n", dwError));
            hRes = E_FAIL;
            goto Exit;
        }
    }
    else
    {
        dwError = GetLastError();
        if (dwError != ERROR_NO_MORE_FILES)
        {
            DEBUGMSG (1, (L"  UsrExceptDmp!DirectorySize: FindFirstFile failed, error=0x%08X\r\n", dwError));
            hRes = E_FAIL;
            goto Exit;
        }
    }

    hRes = S_OK;
    
Exit:
    if (hFind != INVALID_HANDLE_VALUE )
    {
        FindClose(hFind);
    }

    if (NULL != pFindData)
    {
        delete pFindData;
    }

    if (NULL != szSearchDirectory)
    {
        delete [] szSearchDirectory;
    }
    
    return hRes;
}

/*----------------------------------------------------------------------------
    FValidateAndCreatePath

    Tries to create a path with validation checking.
    The top most directory will have the Watson required folder attributes set.
----------------------------------------------------------------------------*/
HRESULT FValidateAndCreatePath(LPWSTR pwzPathName, BOOL fTopMost=TRUE)
{
    HRESULT hRes = E_FAIL;
    DWORD   dwError;
    DWORD   cchPath;
    DWORD   FileAttribute;
    LPWSTR  PathDelimit;

    ASSERT(NULL != pwzPathName);
    if (NULL == pwzPathName)
    {
        DEBUGMSG (1, (L"  UsrExceptDmp!FValidateAndCreatePath: Bad Parameter.\r\n"));
        hRes = E_INVALIDARG;
        goto Exit;
    }

    // First check to see if we are trying to create a root folder.
    // On Windows CE devices the root folder identifies
    // the storage device. We want to make sure we are 
    // not creating a root folder, otherwise the storage device will 
    // have the incorrect name when it comes online.
    cchPath = wcslen(pwzPathName);
    PathDelimit = wcschr(pwzPathName, L'\\');
    if (PathDelimit != pwzPathName)
    {
        DEBUGMSG (1, (L"  UsrExceptDmp!FValidateAndCreatePath: Start of path (%s) should have delimiter '\\'.\r\n", pwzPathName));
        hRes = E_INVALIDARG;
        goto Exit;
    }

    PathDelimit = wcschr(pwzPathName+1, L'\\');
    if ((PathDelimit <= pwzPathName + 1) || (PathDelimit >= pwzPathName+cchPath))
    {
        DEBUGMSG (1, (L"  UsrExceptDmp!FValidateAndCreatePath: Cannot create a root path (%s).\r\n", pwzPathName));
        hRes = E_INVALIDARG;
        goto Exit;
    }

    //  check to see if the path specified is there.
    FileAttribute = GetFileAttributesW( pwzPathName );
    if( FileAttribute != 0xFFFFFFFF )
    {
        //  check to see if the attribute says it is not a dir.
        if( !(FileAttribute & FILE_ATTRIBUTE_DIRECTORY) )
        {
            DEBUGMSG (1, (L"  UsrExceptDmp!FValidateAndCreatePath: Path exists, but it is not a directory (Path=%s)\r\n", pwzPathName));  
            hRes = E_INVALIDARG;
            goto Exit;
        }

        //  We found the file and it is a dir.
        hRes = S_OK;
        goto Exit;
    }

    dwError = GetLastError();
    if( (dwError != ERROR_FILE_NOT_FOUND) &&
        (dwError != ERROR_PATH_NOT_FOUND) )
    {
        DEBUGMSG (1, (L"  UsrExceptDmp!FValidateAndCreatePath: GetFileAttributesW failed for existing path, dwError=0x%08X\r\n",dwError));
        hRes = E_FAIL;
        goto Exit;
    }

    //  we did not find the path, so create it.
    if(CreateDirectoryW (pwzPathName, NULL))
    {
        //  done.
        hRes = S_OK;
        goto Exit;
    }

    dwError = GetLastError();
    if( dwError != ERROR_PATH_NOT_FOUND )
    {
        DEBUGMSG (1, (L"  UsrExceptDmp!FValidateAndCreatePath: CreateDirectoryW failed, dwError=0x%08X\r\n",dwError));
        hRes = E_FAIL;
        goto Exit;
    }

    //  sub-path is not found, create it first.
    if (cchPath < 4)
    {
        DEBUGMSG (1, (L"  UsrExceptDmp!FValidateAndCreatePath: Invalid path name (Path=%s)\r\n", pwzPathName));
        hRes = E_INVALIDARG;
        goto Exit;
    }

    PathDelimit = pwzPathName+cchPath -1 ;
    //  step back from the trailing backslash
    if( *PathDelimit == L'\\' )
        PathDelimit--;

    //  find the last path delimiter.
    while( PathDelimit >  pwzPathName )
    {
        if( *PathDelimit == L'\\' )
            break;
        PathDelimit--;
    }

    if( PathDelimit == pwzPathName )
    {
        DEBUGMSG (1, (L"  UsrExceptDmp!FValidateAndCreatePath: Can't find path delimiter '\\' (Path=%s)\r\n", pwzPathName));
        hRes = E_INVALIDARG;
        goto Exit;
    }

    *PathDelimit = L'\0';

    //  validate sub-path now.
    hRes = FValidateAndCreatePath(pwzPathName, FALSE);

    //  replace the connect char (before checking the hRes)
    *PathDelimit = L'\\';

    if( FAILED(hRes))
    {
        goto Exit;
    }

    //  try to create one more time.
    if(CreateDirectoryW (pwzPathName, NULL))
    {
        //  done.
        hRes = S_OK;
        goto Exit;
    }

    dwError = GetLastError();

    DEBUGMSG (1, (L"  UsrExceptDmp!FValidateAndCreatePath: CreateDirectoryW failed, dwError=0x%08X\r\n", dwError));

    hRes = E_FAIL;

Exit:

    // If we created a directory, and it is the Top Most, make sure attributes set correctly
    if (SUCCEEDED(hRes) && fTopMost)
    {
        FileAttribute = GetFileAttributesW(pwzPathName);
        if(0xFFFFFFFF == FileAttribute)
        {
            dwError = GetLastError();
            DEBUGMSG (1, (L"  UsrExceptDmp!FValidateAndCreatePath: GetFileAttributesW failed, dwError=0x%08X\r\n", dwError));
            hRes = E_FAIL;
            goto Exit_Final;
        }

        // Add the attributes for Watson folders
        FileAttribute |= (WATSON_FOLDER_ATTRIBUTES);
        SetFileAttributesW (pwzPathName, FileAttribute);
        
        // Read the attributes again to make sure they are set correctly
        FileAttribute = GetFileAttributesW(pwzPathName);
        if(0xFFFFFFFF == FileAttribute)
        {
            dwError = GetLastError();
            DEBUGMSG (1, (L"  UsrExceptDmp!FValidateAndCreatePath: GetFileAttributesW failed after setting them, dwError=0x%08X\r\n", dwError));
            hRes = E_FAIL;
            goto Exit_Final;
        }

        // Check the attributes
        if ((FileAttribute & WATSON_FOLDER_ATTRIBUTES) != (WATSON_FOLDER_ATTRIBUTES))
        {
            DEBUGMSG (1, (L"  UsrExceptDmp!FValidateAndCreatePath: Attributes not set correctly, FileAttribute=0x%08X\r\n", FileAttribute));
            hRes = E_FAIL;
            goto Exit_Final;
        }

        // Directory created and attributes correct
        hRes = S_OK;
    }

Exit_Final:
    
    return (hRes);
}


// Create new dump file 
HANDLE CreateDumpFile (TCHAR *szDumpFilePath, DWORD cchDumpFilePathBuffer) 
{
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HKEY hKeyDumpSettings = 0;
    DWORD dwDisp;
    DWORD dwRegType;
    DWORD dwRegSize;
    TCHAR szDumpDirectory [_MAX_PATH];
    TCHAR szDumpFile [_MAX_PATH];
    DWORD dwMaxDiskUsage = 0;
    DWORD dwDumpFilesToday;
    DWORD dwDirLength;
    LONG nRet;
    HRESULT hRes;
    DWORD dwError;
    
    szDumpDirectory[0] = 0;
    szDumpFile[0] = 0;

    ASSERT(NULL != szDumpFilePath);
    if (NULL == szDumpFilePath)
    {
        DEBUGMSG (1, (L"  UsrExceptDmp!CreateDumpFile: Bad Parameter, buffer is NULL.\r\n"));
        goto Exit;
    }

    ASSERT(cchDumpFilePathBuffer >= _MAX_PATH);
    if (cchDumpFilePathBuffer < _MAX_PATH)
    {
        DEBUGMSG (1, (L"  UsrExceptDmp!CreateDumpFile: Bad Parameter, buffer to small.\r\n"));
        goto Exit;
    }

    // Create/Open the RegKey for the Watson Dump Settings
    nRet = ::RegCreateKeyEx (HKEY_LOCAL_MACHINE, WATSON_REGKEY_DUMP_SETTINGS, 0, TEXT(""), 0, 0, NULL, &hKeyDumpSettings, &dwDisp);
    if (ERROR_SUCCESS != nRet)
    {
        DEBUGMSG (1, (L"  UsrExceptDmp!CreateDumpFile: RegCreateKeyEx failed creating Dump Settings key, error=0x%08X\r\n", nRet));
        hKeyDumpSettings = 0;
        goto Exit;
    }

    // Get the directory where dump files should be stored from the registry
    dwRegType = REG_SZ;
    dwRegSize = sizeof (szDumpDirectory);
    nRet = ::RegQueryValueEx (hKeyDumpSettings, WATSON_REGVALUE_DUMP_DIRECTORY, NULL, &dwRegType, (BYTE *) szDumpDirectory, &dwRegSize);
    if (ERROR_SUCCESS != nRet)
    {
        // If we are unable to read the registry value then create it

        // First get the default location for storing dump files
        hRes = ::StringCbCopy(szDumpDirectory, sizeof(szDumpDirectory), WATSON_REGVALUE_DUMP_DIRECTORY_DEFAULT_DATA);
        if (FAILED(hRes))
        {
            DEBUGMSG (1, (L"  UsrExceptDmp!CreateDumpFile: StringCbCopy failed for default dump directory, error=0x%08X\r\n", hRes));
            goto Exit;
        }

        // Then set the registry value so that the Dw Client (dw.exe) knows where to find the dumps
        dwRegType = REG_SZ;
        dwRegSize = (lstrlen(szDumpDirectory) + 1) * sizeof(TCHAR);
        nRet = ::RegSetValueEx(hKeyDumpSettings, WATSON_REGVALUE_DUMP_DIRECTORY, 0, dwRegType, (BYTE *) szDumpDirectory, dwRegSize);
        if (ERROR_SUCCESS != nRet)
        {
            DEBUGMSG (1, (L"  UsrExceptDmp!CreateDumpFile: RegSetValueEx failed setting dump directory, error=0x%08X\r\n", nRet));
            goto Exit;
        }
    }

    // Strip off any tailend backslash or spaces, otherwise CreateDirectory will fail
    dwDirLength = lstrlen(szDumpDirectory);
    while ((dwDirLength > 0) && ((szDumpDirectory[dwDirLength-1] == TEXT('\\')) || (szDumpDirectory[dwDirLength-1] == TEXT(' '))))
    {
        szDumpDirectory[dwDirLength-1] = 0;
        -- dwDirLength;
    }

    // Dump files must be stored in a sub directory
    if (lstrlen(szDumpDirectory) <= 0)
    {
        DEBUGMSG (1, (L"  UsrExceptDmp!CreateDumpFile: szDumpDirectory is zero length, it must be greater than zero.\r\n"));
        goto Exit;
    }

    // We now have the directory required, create it if it does not exist
    hRes = FValidateAndCreatePath(szDumpDirectory);
    if (FAILED(hRes))
    {
        DEBUGMSG (1, (L"  UsrExceptDmp!CreateDumpFile: FValidateAndCreatePath failed creating dump directory, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

    ////
    // Next step - Check if we have exceeded Max Allowed Disk usage

    // Get the Max Disk Usage for dump files from the registry
    dwRegType = REG_DWORD;
    dwRegSize = sizeof (DWORD);
    nRet = ::RegQueryValueEx (hKeyDumpSettings, WATSON_REGVALUE_MAX_DISK_USAGE, NULL, &dwRegType, (BYTE *) &dwMaxDiskUsage, &dwRegSize);
    if (ERROR_SUCCESS != nRet)
    {
        // If we are unable to read the registry value then create it

        // First get the default size for max disk usage
        dwMaxDiskUsage = WATSON_REGVALUE_MAX_DISK_USAGE_DEFAULT_DATA;
        
        // Then set the registry value for next time
        dwRegType = REG_DWORD;
        dwRegSize = sizeof (DWORD);
        nRet = ::RegSetValueEx(hKeyDumpSettings, WATSON_REGVALUE_MAX_DISK_USAGE, 0, dwRegType, (BYTE *) &dwMaxDiskUsage, dwRegSize);
        if (ERROR_SUCCESS != nRet)
        {
            DEBUGMSG (1, (L"  UsrExceptDmp!CreateDumpFile: RegSetValueEx failed setting max disk usage, error=0x%08X\r\n", nRet));
            goto Exit;
        }
    }

    // Now get the current size of the dump directory
    DWORD64 dw64CurrentDiskUsage; 
    hRes = DirectorySize (szDumpDirectory, &dw64CurrentDiskUsage);
    if (FAILED(hRes))
    {
        DEBUGMSG (1, (L"  UsrExceptDmp!CreateDumpFile: DirectorySize failed, error=0x%08X\r\n", hRes));
        goto Exit;
    }

    // Make sure we have not exceeded the max disk usage
    if (dw64CurrentDiskUsage > dwMaxDiskUsage)
    {
        DEBUGMSG (1, (L"  UsrExceptDmp!CreateDumpFile: Max disk usage exceeded, Current=%I64u, Max=%u\r\n", dw64CurrentDiskUsage, dwMaxDiskUsage));
        goto Exit;
    }

    ////
    // Final Step - Create the dump sub folder & file

    // Dump sub folder & file name = CeMMDDYY-NN\CeMMDDYY-NN.udmp, NN = count of dumps on same day
    dwDumpFilesToday = 0;
    SYSTEMTIME stLocalTime;

    GetLocalTime(&stLocalTime);

    while (INVALID_HANDLE_VALUE == hFile)
    {
        ++ dwDumpFilesToday;
        if (dwDumpFilesToday > WATSON_MAX_DUMP_FILES_PER_DAY)
        {
            DEBUGMSG (1, (L"  UsrExceptDmp!CreateDumpFile: Exceeded max dump files allowed per day (%u).\r\n", WATSON_MAX_DUMP_FILES_PER_DAY));
            goto Exit;
        }

        // First create a unique dump folder
        hRes = StringCbPrintf(szDumpFile, sizeof(szDumpFile), WATSON_DUMP_FOLDER_NAME_FORMAT, 
                              stLocalTime.wMonth, stLocalTime.wDay, stLocalTime.wYear%100, dwDumpFilesToday);
        if (FAILED(hRes))
        {
           DEBUGMSG (1, (L"  UsrExceptDmp!CreateDumpFile: StringCbPrintf failed for Dump Folder name, error=0x%08X\r\n", hRes));
           goto Exit;
        }
        
        hRes = ::StringCchPrintf (szDumpFilePath, cchDumpFilePathBuffer, TEXT("%s\\%s"), szDumpDirectory, szDumpFile);
        if (FAILED(hRes))
        {
            DEBUGMSG (1, (L"  UsrExceptDmp!CreateDumpFile: StringCbPrintf failed for Dump Folder Path, error=0x%08X\r\n", hRes));
            goto Exit;
        }

        if (!CreateDirectory(szDumpFilePath,NULL))
        {
            dwError = GetLastError();

            // If it already exists we will loop round and try again
            if (dwError != ERROR_ALREADY_EXISTS)
            {
                // For any other error we quit
                DEBUGMSG (1, (L"  UsrExceptDmp!CreateDumpFile: CreateDirectory failed creating dump folder, error=0x%08X\r\n", dwError));
                goto Exit;
            }
        }
        else
        {
            // Now create the dump file
            hRes = StringCbPrintf(szDumpFile, sizeof(szDumpFile), WATSON_DUMP_FILE_NAME_FORMAT, 
                                  stLocalTime.wMonth, stLocalTime.wDay, stLocalTime.wYear%100, dwDumpFilesToday);
            if (FAILED(hRes))
            {
               DEBUGMSG (1, (L"  UsrExceptDmp!CreateDumpFile: StringCbPrintf failed for Dump File name, error=0x%08X\r\n", hRes));
               goto Exit;
            }
            
            hRes = ::StringCchCat(szDumpFilePath, cchDumpFilePathBuffer, TEXT("\\"));
            if (FAILED(hRes))
            {
                DEBUGMSG (1, (L"  UsrExceptDmp!CreateDumpFile: StringCchCat failed adding '\' to Dump File Path, error=0x%08X\r\n", hRes));
                goto Exit;
            }

            hRes = ::StringCchCat(szDumpFilePath, cchDumpFilePathBuffer, szDumpFile);
            if (FAILED(hRes))
            {
                DEBUGMSG (1, (L"  UsrExceptDmp!CreateDumpFile: StringCchCat failed adding file name to Dump File Path, error=0x%08X\r\n", hRes));
                goto Exit;
            }

            hFile = ::CreateFile (szDumpFilePath, 
                                            GENERIC_WRITE,
                                            FILE_SHARE_READ,
                                            NULL,
                                            CREATE_NEW,
                                            FILE_ATTRIBUTE_NORMAL,
                                            0);
            if (INVALID_HANDLE_VALUE == hFile)
            {
                dwError = GetLastError();
                DEBUGMSG (1, (L"  UsrExceptDmp!CreateDumpFile: CreateFile failed, error=0x%08X\r\n", dwError));
                goto Exit;
            }
        }
    }

    DEBUGMSG (1, (L"  UsrExceptDmp!CreateDumpFile: Creating Dump File=%s\r\n", szDumpFilePath));
    
Exit:    
    if (hKeyDumpSettings != 0)
    {
        ::RegCloseKey (hKeyDumpSettings);
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

BOOL WatsonDumpEnabled()
{
    BOOL fDumpEnabled = FALSE;
    HKEY hKeyDumpSettings = 0;
    DWORD dwDisp, dwRegType, dwRegSize, DwDumpEnabled = 0;
    LONG nRet;
    
    // Create/Open the RegKey for the Watson Dump Settings
    nRet = ::RegCreateKeyEx (HKEY_LOCAL_MACHINE, WATSON_REGKEY_DUMP_SETTINGS, 0, TEXT(""), 0, 0, NULL, &hKeyDumpSettings, &dwDisp);
    if (ERROR_SUCCESS != nRet)
    {
        DEBUGMSG (1, (L"  UsrExceptDmp!WatsonDumpEnabled: RegCreateKeyEx failed creating Dump Settings key, error=0x%08X\r\n", nRet));
        goto Exit;
    }

    // Check if the dumping of files is enabled
    dwRegType = REG_DWORD;
    dwRegSize = sizeof (DWORD);
    nRet = ::RegQueryValueEx (hKeyDumpSettings, WATSON_REGVALUE_DUMP_ENABLED, NULL, &dwRegType, (BYTE *) &DwDumpEnabled, &dwRegSize);
    if (ERROR_SUCCESS != nRet)
    {
        // If we are unable to read the registry value then create it

        // First get the default setting for dump enabled
        DwDumpEnabled = WATSON_REGVALUE_DUMP_ENABLED_DEFAULT_DATA;
        
        // Then set the registry value for next time
        dwRegType = REG_DWORD;
        dwRegSize = sizeof (DWORD);
        nRet = ::RegSetValueEx(hKeyDumpSettings, WATSON_REGVALUE_DUMP_ENABLED, 0, dwRegType, (BYTE *) &DwDumpEnabled, dwRegSize);
        if (ERROR_SUCCESS != nRet)
        {
            DEBUGMSG (1, (L"  UsrExceptDmp!CreateDumpFile: RegSetValueEx failed setting dump enabled, error=0x%08X\r\n", nRet));
            goto Exit;
        }
    }

    fDumpEnabled = DwDumpEnabled ? TRUE : FALSE;

Exit:
    if (hKeyDumpSettings != 0)
    {
        ::RegCloseKey (hKeyDumpSettings);
    }

    return fDumpEnabled;
}

void Usage (void)
{
    DEBUGMSG (1, (L"UsrExceptDmp usage: \"UsrExceptDmp -RegDebugger\" will register UsrExceptDmp\r\n")) ;
    DEBUGMSG (1, (L"    as Just-In-Time user mode debugger. UsrExceptDmp will then be called\r\n")) ;
    DEBUGMSG (1, (L"    automatically from a 2nd chance exception and a micro-dump will be\r\n")) ;
    DEBUGMSG (1, (L"    generated with the name 'CeDDMMYY-NN.udmp'. The location of the dump\r\n")) ;
    DEBUGMSG (1, (L"    file is determined by the registry key value:\r\n")) ;
    DEBUGMSG (1, (L"    HKLM\\%s\\%s.\r\n",WATSON_REGKEY_DUMP_SETTINGS,WATSON_REGVALUE_DUMP_DIRECTORY)) ;
    DEBUGMSG (1, (L"\r\n")) ;
}


int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPreviousInstance, LPTSTR pszCmdLine, int nCommandShow)
{
    int iRetVal = 1;
    DWORD dwPID = 0;
    HANDLE hMutexWatsonDumpFiles = NULL;

    BOOL bOldKMode = ::SetKMode (TRUE);
    DWORD dwOldProcPermissions = ::SetProcPermissions (0xFFFFFFFF);

    // Create mutex to prevent multiple access to dump files and dump file registry settings
    hMutexWatsonDumpFiles = CreateMutex(NULL, FALSE, WATSON_MUTEX_DUMP_FILES);
    if (NULL == hMutexWatsonDumpFiles)
    {
        iRetVal = 1;
        goto Exit;
    }

    __try
    {
        if (::wcsicmp (pszCmdLine, L""))
        { // Not empty       
            if (!::wcsicmp (pszCmdLine, L"-RegDebugger"))
            { // empty command line
                RegisterAsDebugger ();
                iRetVal = 0;
            }
            else if (WatsonDumpEnabled())
            {
                dwPID = StringToDWord (pszCmdLine);
                if ((0xFFFFFFFF == dwPID) || (0 == dwPID))
                {
                    DEBUGMSG (1, (L"  UsrExceptDmp!WinMain: Invalid PID on command line - %s\r\n", pszCmdLine));
                }
                else
                {
                    DEBUG_EVENT DebugEvent;
                    if (!AttachAsAppDebugger (dwPID, &DebugEvent))
                    {
                        DEBUGMSG (1, (L"  UsrExceptDmp!WinMain: Failed to attach as debugger to PID=0x%08X.\r\n", dwPID));
                    }
                    else
                    {
                        DWORD dwWait = WaitForSingleObject(hMutexWatsonDumpFiles, WATSON_TIMEOUT_MUTEX_DUMP_FILES);
                        if (WAIT_OBJECT_0 == dwWait)
                        {
                            TCHAR szDumpFilePath [_MAX_PATH];
                            HANDLE hFile = CreateDumpFile (szDumpFilePath, ARRAYSIZE(szDumpFilePath));
                            if (INVALID_HANDLE_VALUE != hFile)
                            {
                                BOOL fSuccess = GenerateDumpFileContent (hFile, &DebugEvent);
                                if (fSuccess)
                                {
                                    DWORD dwDisp;
                                    HKEY hKey;

                                    // Create a registry entry to indicate a new dump file created
                                    LONG nRet = ::RegCreateKeyEx (HKEY_LOCAL_MACHINE, WATSON_REGKEY_DUMP_FILES, 0, L"", 0, 0, NULL, &hKey, &dwDisp);
                                    if (ERROR_SUCCESS == nRet)
                                    {
                                        DWORD dwRegType = REG_DWORD;
                                        DWORD dwRegSize = sizeof(DWORD);
                                        DWORD dwRegValue = 0;
                                        nRet = ::RegSetValueEx (hKey, szDumpFilePath, NULL, dwRegType, (BYTE *) &dwRegValue, dwRegSize);
                                        if (ERROR_SUCCESS != nRet)
                                        {
                                            DEBUGMSG (1, (L"  UsrExceptDmp!WinMain: RegSetValueEx failed setting dump file attributes, error=0x%08X\r\n", nRet));
                                            fSuccess = FALSE;
                                        }
                                        ::RegCloseKey (hKey);
                                    }

                                    if (fSuccess)
                                    {
                                        // Launch the Watson upload client
                                        TCHAR szUploadClient [_MAX_PATH];
                                        szUploadClient[0] = 0;
                                        
                                        // First check if there is an upload client registered in the registry
                                        nRet = ::RegCreateKeyEx (HKEY_LOCAL_MACHINE, WATSON_REGKEY_DUMP_SETTINGS, 0, L"", 0, 0, NULL, &hKey, &dwDisp);
                                        if (ERROR_SUCCESS == nRet)
                                        {
                                            DWORD dwRegType = REG_SZ;
                                            DWORD dwRegSize = sizeof (szUploadClient);
                                            nRet = ::RegQueryValueEx (hKey, WATSON_REGVALUE_UPLOAD_CLIENT, NULL, &dwRegType, (BYTE *) szUploadClient, &dwRegSize);
                                            ::RegCloseKey (hKey);
                                        }

                                        if (ERROR_SUCCESS != nRet)
                                        {
                                            // If there is no upload client registered, use the default
                                            HRESULT hRes = ::StringCbCopy(szUploadClient, sizeof(szUploadClient), WATSON_REGVALUE_UPLOAD_CLIENT_DEFAULT_DATA);
                                            if (FAILED(hRes))
                                            {
                                                DEBUGMSG (1, (L"  UsrExceptDmp!WinMain: StringCbCopy failed for upload client, error=0x%08X\r\n", hRes));
                                            }
                                            else
                                            {
                                                nRet = ERROR_SUCCESS;
                                            }
                                        }

                                        if (ERROR_SUCCESS == nRet)
                                        {
                                            // Launch the upload client process
                                            PROCESS_INFORMATION pi;
                                            if (CreateProcess(szUploadClient, NULL, NULL, NULL, NULL, CREATE_NEW_CONSOLE, NULL, NULL, NULL, &pi))
                                            {
                                                // We don't need these handles, so close immediately
                                                CloseHandle(pi.hProcess);
                                                CloseHandle(pi.hThread);
                                            }
                                            
                                        }
                                    }
                                }
                                ::CloseHandle (hFile);

                                if (!fSuccess)
                                {
                                    DEBUGMSG (1, (L"  UsrExceptDmp!WinMain: failed generating dump, deleting dump file '%s'.\r\n", szDumpFilePath));
                                    if(!DeleteFile(szDumpFilePath))
                                    {
                                        DEBUGMSG (1, (L"  UsrExceptDmp!WinMain: DeleteFile failed, error=0x%08X\r\n", GetLastError()));
                                    }
                                }
                                else
                                {
                                    iRetVal = 0;
                                }
                                
                            }
                            else
                            {
                                DEBUGMSG (1, (L"  UsrExceptDmp!WinMain: No dump file created.\r\n"));
                            }
                            ReleaseMutex(hMutexWatsonDumpFiles);
                        }
                        else
                        {
                            if (WAIT_TIMEOUT == dwWait)
                            {
                                DEBUGMSG (1, (L"  UsrExceptDmp!WinMain: Timeout waiting for Dump file mutex.\r\n"));
                            }
                            else
                            {
                                DEBUGMSG (1, (L"  UsrExceptDmp!WinMain: Error waiting for Dump file mutex, error=0x%08X\r\n", GetLastError()));
                            }
                        }
                        
                        if (!::ContinueDebugEvent (DebugEvent.dwProcessId, DebugEvent.dwThreadId, DBG_EXCEPTION_NOT_HANDLED))
                        {
                            DEBUGMSG (1, (L"  UsrExceptDmp!WinMain: Failed to continue unhandled.\r\n"));
                        }
                    }
                }
            }
            else
            {
                DEBUGMSG (1, (L"  UsrExceptDmp!WinMain: Watson dump disabled in registry.\r\n"));
            }
        }
        if (iRetVal)
        {
            Usage ();
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DEBUGMSG (1, (L"  UsrExceptDmp!WinMain: Unexpected exception - bailing out to prevent reentrance.\r\n"));
    }
    
    CloseHandle(hMutexWatsonDumpFiles);

Exit:
    ::SetProcPermissions (dwOldProcPermissions) ;
    ::SetKMode (bOldKMode) ;

    return iRetVal;
}

