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
////////////////////////////////////////////////////////////////////////////////
//
//  FLSHWEAR TUX DLL
//
//  Module: tuxtest.cpp
//          Contains the shell processing function.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#include "dskioctl.h"
#include "globals.h"

#ifndef Debug
#define Debug NKDbgPrintfW
#endif

enum { T_UNKNOWN, T_PERCENT, T_SECTORS, T_AMOUNT };

// logging types
enum { LOG_DBG, LOG_CSV, LOG_PERFLOG };

// default logger
CEPerfLog g_CEPerfLog;
DWORD g_logMethod = LOG_PERFLOG;

// filename for .CSV files
TCHAR g_csvFilename[MAX_PATH];


////////////////////////////////////////////////////////////////////////////////
// GetTestResult
//  Checks the kato object to see if failures, aborts, or skips were logged
//  and returns the result accordingly
//
// Parameters:
//  None.
//
// Return value:
//  TESTPROCAPI valud of either TPR_FAIL, TPR_ABORT, TPR_SKIP, or TPR_PASS.

TESTPROCAPI GetTestResult(void) 
{   
    // Check to see if we had any LOG_EXCEPTIONs or LOG_FAILs and, if so,
    // return TPR_FAIL
    //
    for(int i = 0; i <= LOG_FAIL; i++) 
    {
        if(g_pKato->GetVerbosityCount(i)) 
        {
            return TPR_FAIL;
        }
    }

    // Check to see if we had any LOG_ABORTs and, if so, 
    // return TPR_ABORT
    //
    for( ; i <= LOG_ABORT; i++) 
    {
        if(g_pKato->GetVerbosityCount(i)) 
        {
            return TPR_ABORT;
        }
    }

    // Check to see if we had LOG_SKIPs or LOG_NOT_IMPLEMENTEDs and, if so,
    // return TPR_SKIP
    //
    for( ; i <= LOG_NOT_IMPLEMENTED; i++) 
    {
        if (g_pKato->GetVerbosityCount(i)) 
        {
            return TPR_SKIP;
        }
    }

    // If we got here, we only had LOG_PASSs, LOG_DETAILs, and LOG_COMMENTs
    // return TPR_PASS
    //
    return TPR_PASS;
}

////////////////////////////////////////////////////////////////////////////////
// LOG
//  Printf-like wrapping around g_pKato->Log(LOG_DETAIL, ...)
//
// Parameters:
//  szFormat        Formatting string (as in printf).
//  ...             Additional arguments.
//
// Return value:
//  None.

void LOG(LPWSTR szFormat, ...)
{
    va_list va;

    if(g_pKato)
    {
        va_start(va, szFormat);
        g_pKato->LogV( LOG_DETAIL, szFormat, va);
        va_end(va);
    }
}

// callback for DebugOutput
void PerfDbgPrint(LPCTSTR str)
{
    // should use '%s' instead using 'str' directly, otherwise strings 
    // that contain '%' such as "%95 Filled ..." can't be printed correctly
    LOG(_T("%s"), str);
}

////////////////////////////////////////////////////////////////////////////////
// DllMain
//  Main entry point of the DLL. Called when the DLL is loaded or unloaded.
//
// Parameters:
//  hInstance       Module instance of the DLL.
//  dwReason        Reason for the function call.
//  lpReserved      Reserved for future use.
//
// Return value:
//  TRUE if successful, FALSE to indicate an error condition.

BOOL WINAPI DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(lpReserved);

    // Any initialization code goes here.    
    switch(dwReason)
    {
    case DLL_PROCESS_ATTACH:         
        Debug(TEXT("DLL_PROCESS_ATTACH\r\n"));           
        return TRUE;

    case DLL_PROCESS_DETACH:
        Debug(TEXT("DLL_PROCESS_DETACH\r\n"));
        return TRUE;
    }
    return TRUE;
}

DWORD GetModulePath(HMODULE hMod, TCHAR * tszPath, DWORD dwCchSize)
{
    DWORD fullNameLen = GetModuleFileName(hMod, tszPath, dwCchSize);
    DWORD len = fullNameLen;
    
    while (len > 0 && tszPath[len - 1] != _T('\\'))
        --len;

    // Changing the item at iLen to '\0' will leave a trailing \ on the path.
    if (len >= 0)
    {
        tszPath[len] = 0;
    }
    return fullNameLen;
}

static void Usage()
{    
    LOG(TEXT(""));
    LOG(TEXT("Usage: tux -o -d flshwear -c\"/disk <disk> /profile <profile> /repeat <count> /sectors <sectors> /rw <???P|S|K|M|G>")); 
    LOG(TEXT("                              /log <method> /readlog <readlog> /writelog <writelog> /fill <list> /delete /store /part\""));
    UsageCommon(NULL, g_pKato);
    LOG(TEXT("       /repeat <count>     : number of times to repeat each timed operation; default = %u"), g_cRepeat);
    LOG(TEXT("       /sectors <sectors>  : number of sectors for each timed operation; default = %u"), g_cSectors);
    LOG(TEXT("       /rw <count>         : '??P' - percentage of the volume of the storage device to be tested (e.g. '20P' = 20%%)"));
    LOG(TEXT("                             '??(K|M|G)' - amount of the storage space (in bytes) to be tested (e.g. '10M' = 10 MegaBytes)"));
    LOG(TEXT("                             '??S' - number of sectors to be tested (e.g. '100S' = 100 sectors)"));
    LOG(TEXT("                             default = the total volume of the storage device specified"));
    LOG(TEXT("       /log <type>         : 'dbg' - log to debug output"));
    LOG(TEXT("                             'csv' - log to .csv file"));
    LOG(TEXT("                             'perflog' - logging through perflog.dll"));
    LOG(TEXT("                             default = 'perflog'"));
    LOG(TEXT("       /readlog <readlog>  : location of .csv file to log read performance when /log csv is used;"));
    LOG(TEXT("       /writelog <writelog>: location of .csv file to log write performance when /log csv is used;"));
    LOG(TEXT("       /fill <list>        : the list of filled percent of flash; numbers in the list should be spearated by ',' or/and white space"));
    LOG(TEXT("                             (e.g. 5,10 - test with the condition of %d%% filled and %d%% filled separately)"), 5, 10);
    LOG(TEXT("       /delete             : perform an IOCTL_DISK_DELETE_SECTORS operation ofter each timed write operation"));
    LOG(TEXT("       /flash              : use IOCTL_FLASH rather then IOCTL_DISK"));
    LOG(TEXT(""));
}


// enumerates all storage devices in the system
void EnumerateStorageDevice()
{
    // enumerate STORE_MOUNT_GUID /  BLOCK_DRIVER_GUID devices advertized by storage manager
    HANDLE hDetect = DEV_DetectFirstDevice((g_fOpenAsStore||g_fOpenAsPartition) ? &STORE_MOUNT_GUID : &BLOCK_DRIVER_GUID, 
        g_szDiskName, MAX_PATH);

    LOG(_T("Enumerating all storage devices in the system"));
    
    if(hDetect)
    {
        DWORD nFound = 0;
        HANDLE hDisk = INVALID_HANDLE_VALUE;
        do {
            // open a handle to the enumerated device
            hDisk = OpenDevice(g_szDiskName, g_fOpenAsStore, g_fOpenAsPartition);
            if(INVALID_HANDLE_VALUE == hDisk)
            {
                LOG(_T("Unable to open storage device \"%s\"; error %u"), g_szDiskName, GetLastError());
            }
            else
            {
                STORAGEDEVICEINFO sdi = {0};
                DWORD cbReturned = 0;

                // get profile name of the device
                if(!DeviceIoControl(hDisk, IOCTL_DISK_DEVICE_INFO, &sdi, 
                    sizeof(STORAGEDEVICEINFO), NULL, 0, &cbReturned, NULL))
                {
                    LOG(_T("Device \"%s\" does not support IOCTL_DISK_DEVICE_INFO"
                        _T("(required for /profile option); error %u")), g_szDiskName, GetLastError());
                }
                else
                {
                    LOG(_T("\tDEVICE \"%s\" : PROFILE \"%s\" "), g_szDiskName, sdi.szProfile);
                    nFound++;
                }
                CloseHandle(hDisk);
            }
        } while (DEV_DetectNextDevice(hDetect, g_szDiskName, MAX_PATH));
        
        DEV_DetectClose(hDetect);
        
        if (nFound > 0)
        {
            LOG(_T("Please select a profile or disk listed above as the test device"));
            LOG(_T("This test can be used for these profiles - SNDPCI, STRARA, and RAMFMD"));
        }
    } 
    else
    {
        LOG(_T("There is no storage device in the system, please check it"));
    }    
}


// open device by Profile or diskname 
BOOL OpenDevice()
{
    BOOL fRet = FALSE;
    FLASH_PARTITION_INFO * pPartTable = NULL;

    if(g_szProfile[0])
    {
        if(g_szDiskName[0])
        {
            LOG(_T("WARNING:  Overriding /disk option with /profile option\n"));
        }
        g_hDisk = OpenDiskHandleByProfile(g_pKato,g_fOpenAsStore,g_szProfile,g_fOpenAsPartition);
    }
    else
    {
        if (g_szDiskName[0])
        {
            g_hDisk = OpenDiskHandleByDiskName(g_pKato,g_fOpenAsStore,g_szDiskName,g_fOpenAsPartition);    
        }
        else
        {
            g_hDisk = NULL;
        }
    }
    if(INVALID_HANDLE(g_hDisk))
    {
        EnumerateStorageDevice();
        LOG(_T("FAIL: unable to open media"));
        goto done;
    }
    //If flash was specified, fill g_FlashPartInfo for use in IOCTL_FLASH*
    if(g_bUseFlashIOCTLs){
        if (!Dsk_GetFlashRegionInfo(g_hDisk, &g_FlashRegionInfo))
        {
            LOG(_T("FAIL: unable to get flash region information"));
            goto done;
        }
        if (!Dsk_GetFlashPartitionInfo(g_hDisk, &g_FlashPartInfo))
        {
            LOG(_T("FAIL: unable to get flash partition information"));
            goto done;
        }
        // Calculate the number of sectors in the flash and adjust
        // the global diskinfo value
        g_diskInfo.di_total_sectors = (DWORD)g_FlashPartInfo.LogicalBlockCount *
            g_FlashRegionInfo.SectorsPerBlock;
    }
    fRet = TRUE;
done:
    if(pPartTable){
        free(pPartTable);
    }
    return fRet;
}


void Initialize(void)
{
    WCHAR szBuffer[MAX_PATH];
    CClParse cmdLine(g_pShellInfo->szDllCmdLine);
    ULONGLONG sectorsToWrite = 0;
    
    ProcessCmdLineCommon(g_pShellInfo->szDllCmdLine, g_pKato);

    if(!cmdLine.GetOptVal(L"repeat", &g_cRepeat)) {
        g_cRepeat = 2;
    }

    if(!cmdLine.GetOptVal(L"sectors", &g_cSectors)) {
        g_cSectors = 1; // sectors
    }

    g_bUseFlashIOCTLs = cmdLine.GetOpt(L"flash");

    if(g_bUseFlashIOCTLs) {
        LOG(L"\"flash\" specified, will use flash IOCTLS");
    }

    g_fDelete = cmdLine.GetOpt(L"delete");
    
    if(g_fDelete && !g_bUseFlashIOCTLs) {
        LOG(L"will send IOCTL_DISK_DELETE_SECTORS between write operations");
    }
    else if(g_fDelete){
        LOG(L"will send IOCTL_FLASH_DELETE_LOGICAL_SECTORS between write operations");
    }
        
    if (OpenDevice())
    {
        // select logging method
        if (cmdLine.GetOptString(_T("log"), szBuffer, countof(szBuffer)))
        {
            if (_tcsnicmp(szBuffer, _T("dbg"), 3) == 0)
            {
                g_pReadPerfLog = new DebugOutput(PerfDbgPrint);
                g_pWritePerfLog = g_pReadPerfLog;
                LOG(_T("Logging to Debug Output"));
                
                if (g_pWritePerfLog == NULL) 
                {
                    LOG(_T("*** OOM creating DebugOutput object ***"));
                }
                g_logMethod = LOG_DBG;
            }
            else if (_tcsnicmp(szBuffer, _T("csv"), 3) == 0)
            {
                // get wirte .csv log file name             
                if (cmdLine.GetOptString(_T("writelog"), szBuffer, countof(szBuffer))) 
                {
                    HRESULT hr = StringCchCopy(g_csvFilename, countof(g_csvFilename), szBuffer);
                    if (FAILED(hr))
                    {
                        g_csvFilename[0] = _T('\0');
                    }
                }
                if (g_csvFilename[0] == _T('\0'))
                {
                    if (GetModulePath(GetModuleHandle(NULL), g_csvFilename, countof(g_csvFilename)) > 0)
                    {
                        HRESULT hr = StringCchCat(g_csvFilename, countof(g_csvFilename), _T("FlshwearWrite.csv"));
                        if (FAILED(hr))
                        {
                            g_csvFilename[0] = _T('\0');
                        }
                    }                  
                }
                if (g_csvFilename[0] != _T('\0'))
                {                
                    g_pWritePerfLog = new CsvFile(g_csvFilename, 0);
                    LOG(_T("(Write) Logging to [CSV file %s]"), g_csvFilename); 
                }

                // get read .csv log file name 
                g_csvFilename[0] = _T('\0');
                if (cmdLine.GetOptString(_T("readlog"), szBuffer, countof(szBuffer))) 
                {
                    HRESULT hr = StringCchCopy(g_csvFilename, countof(g_csvFilename), szBuffer);
                    if (FAILED(hr))
                    {
                        g_csvFilename[0] = _T('\0');
                    }
                }
                if (g_csvFilename[0] == _T('\0'))
                {
                    if (GetModulePath(GetModuleHandle(NULL), g_csvFilename, countof(g_csvFilename)) > 0)
                    {
                        HRESULT hr = StringCchCat(g_csvFilename, countof(g_csvFilename), _T("FlshwearRead.csv"));
                        if (FAILED(hr))
                        {
                            g_csvFilename[0] = _T('\0');
                        }
                    }                  
                }                
                if (g_csvFilename[0] != _T('\0'))
                { 
                    g_pReadPerfLog = new CsvFile(g_csvFilename, 0);
                    LOG(_T("(Read) Logging to [CSV file %s]"), g_csvFilename); 
                }
                
                if (g_pWritePerfLog == NULL || g_pReadPerfLog == NULL) 
                {
                    LOG(_T("*** OOM creating CsvFile object ***"));
                }
                else
                {
                    g_pWritePerfLog->PerfSetTestName(_T("FlshWear"));
                }
                g_logMethod = LOG_CSV;
            }
        }

        // set default logging
        if (g_pReadPerfLog == NULL || g_pWritePerfLog == NULL)
        {
            if (g_pReadPerfLog)
            {
                delete g_pReadPerfLog; 
            }
            if (g_pWritePerfLog)
            {
                delete g_pWritePerfLog; 
            }
            LOG(_T("Logging to CE Perflog"));
            
            g_pReadPerfLog = g_pWritePerfLog = &g_CEPerfLog;
            g_logMethod = LOG_PERFLOG;
        }
        g_pReadPerfLog->PerfSetTestName(_T("FlshWear"));
        
        // set writing sectors
        if (cmdLine.GetOptString(_T("rw"), szBuffer, MAX_PATH))
        {
            DWORD strLen = 0;
            VERIFY(SUCCEEDED(StringCchLength(szBuffer, countof(szBuffer), (size_t*)&strLen)));
            TCHAR postfix = szBuffer[strLen - 1];
            DWORD value = 0;
            DWORD multiple = 0;
            DWORD type = T_UNKNOWN;
            
            if (_totupper(postfix) == _T('P'))
            {
                type = T_PERCENT;
            }
            else if (_totupper(postfix) == _T('S'))
            {
                type = T_SECTORS;
            }
            else if (_totupper(postfix) == _T('K'))
            {
                type = T_AMOUNT;
                multiple = 1024;
            }
            else if (_totupper(postfix) == _T('M'))
            {
                type = T_AMOUNT;
                multiple = 1024 * 1024;            
            }
            else if (_totupper(postfix) == _T('G'))
            {
                type = T_AMOUNT;
                multiple = 1024 * 1024 * 1024;              
            }

            szBuffer[strLen - 1] = _T('\0');
            value = _ttol(szBuffer);

            switch (type) {
            case T_PERCENT:
                sectorsToWrite = (ULONGLONG) g_diskInfo.di_total_sectors * value / 100;
                break;
            case T_SECTORS:
                sectorsToWrite = value;
                break;
            case T_AMOUNT:
                sectorsToWrite = (ULONGLONG) value * multiple / g_diskInfo.di_bytes_per_sect;
                break;
            default:
                sectorsToWrite = g_diskInfo.di_total_sectors;
            }

            if (sectorsToWrite > g_diskInfo.di_total_sectors)
            {
                LOG(_T("WARNING: user specified read/write amount is too big, set to default value"));
                sectorsToWrite = g_diskInfo.di_total_sectors;
            }
        }

        // get how many filled
        for (DWORD i = 0; i < countof(g_filled); i++)
        {
            g_filled[i] = FALSE;
        }
        if (cmdLine.GetOptString(_T("fill"), szBuffer, countof(szBuffer)))
        {
            LPCTSTR sept = _T(" ,");
            TCHAR * nextToken = NULL;
            LPCTSTR token = _tcstok_s(szBuffer, sept, &nextToken);
            while (NULL != token)
            {
                // only '#' or '##' is valid
                if (_istdigit(token[0]) 
                    && ( (token[1] == _T('\0')) 
                        || (_istdigit(token[1]) && token[2] == _T('\0')) ) ) 
                {
                    g_filled[_ttol(token)] = TRUE;
                }
                token = _tcstok_s(NULL, sept, &nextToken);
            }
        }
        else
        {
            // default test with 20%, 40%, 60% and 80% filled
            for (DWORD i = 20; i < countof(g_filled); i += 20)
            {
                g_filled[i] = TRUE;
            }
        }
    }

    // user inputs 0 for sectors
    if (g_cSectors == 0)
    {
        g_cSectors = 1;
    }
    // user doesn't specific how many sectors to be read/write
    if (sectorsToWrite == 0)
    {
        sectorsToWrite = g_diskInfo.di_total_sectors;
    }
    g_totalWrites = (DWORD) sectorsToWrite / g_cSectors;
}

void Shutdown(void)
{
    if (g_logMethod == LOG_DBG)
    {
        delete g_pReadPerfLog;
    }
    else if (g_logMethod == LOG_CSV)
    {
        delete g_pReadPerfLog;
        delete g_pWritePerfLog;
    }

    if(VALID_HANDLE(g_hDisk))
    {
        LOG(_T("Closing disk handle..."));
        CloseHandle(g_hDisk);
    }  
}

////////////////////////////////////////////////////////////////////////////////
// ShellProc
//  Processes messages from the TUX shell.
//
// Parameters:
//  uMsg            Message code.
//  spParam         Additional message-dependent data.
//
// Return value:
//  Depends on the message.

SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam)
{
    LPSPS_BEGIN_TEST    pBT;
    LPSPS_END_TEST      pET;

    switch (uMsg)
    {
    case SPM_LOAD_DLL:
        // Sent once to the DLL immediately after it is loaded. The spParam
        // parameter will contain a pointer to a SPS_LOAD_DLL structure. The
        // DLL should set the fUnicode member of this structure to TRUE if the
        // DLL is built with the UNICODE flag set. If you set this flag, Tux
        // will ensure that all strings passed to your DLL will be in UNICODE
        // format, and all strings within your function table will be processed
        // by Tux as UNICODE. The DLL may return SPR_FAIL to prevent the DLL
        // from continuing to load.

        // If we are UNICODE, then tell Tux this by setting the following flag.
#ifdef UNICODE
        ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
#endif // UNICODE
        g_pKato = (CKato*)KatoGetDefaultObject();
        
        LOG(TEXT("ShellProc(SPM_LOAD_DLL, ...) called"));
        Usage();
        
        break;

    case SPM_UNLOAD_DLL:
        // Sent once to the DLL immediately before it is unloaded.
        Shutdown();
        LOG(TEXT("ShellProc(SPM_UNLOAD_DLL, ...) called"));
        break;

    case SPM_SHELL_INFO:
        // Sent once to the DLL immediately after SPM_LOAD_DLL to give the DLL
        // some useful information about its parent shell and environment. The
        // spParam parameter will contain a pointer to a SPS_SHELL_INFO
        // structure. The pointer to the structure may be stored for later use
        // as it will remain valid for the life of this Tux Dll. The DLL may
        // return SPR_FAIL to prevent the DLL from continuing to load.
        LOG(TEXT("ShellProc(SPM_SHELL_INFO, ...) called"));

        // Store a pointer to our shell info for later use.
        g_pShellInfo = (LPSPS_SHELL_INFO)spParam;

        Initialize();
        
        break;

    case SPM_REGISTER:
        // This is the only ShellProc() message that a DLL is required to
        // handle (except for SPM_LOAD_DLL if you are UNICODE). This message is
        // sent once to the DLL immediately after the SPM_SHELL_INFO message to
        // query the DLL for its function table. The spParam will contain a
        // pointer to a SPS_REGISTER structure. The DLL should store its
        // function table in the lpFunctionTable member of the SPS_REGISTER
        // structure. The DLL may return SPR_FAIL to prevent the DLL from
        // continuing to load.
        LOG(TEXT("ShellProc(SPM_REGISTER, ...) called"));
        ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
#ifdef UNICODE
        return SPR_HANDLED | SPF_UNICODE;
#else // UNICODE
        return SPR_HANDLED;
#endif // UNICODE

    case SPM_START_SCRIPT:
        // Sent to the DLL immediately before a script is started. It is sent
        // to all Tux DLLs, including loaded Tux DLLs that are not in the
        // script. All DLLs will receive this message before the first
        // TestProc() in the script is called.
        LOG(TEXT("ShellProc(SPM_START_SCRIPT, ...) called"));
        break;

    case SPM_STOP_SCRIPT:
        // Sent to the DLL when the script has stopped. This message is sent
        // when the script reaches its end, or because the user pressed
        // stopped prior to the end of the script. This message is sent to
        // all Tux DLLs, including loaded Tux DLLs that are not in the script.
        LOG(TEXT("ShellProc(SPM_STOP_SCRIPT, ...) called"));
        break;

    case SPM_BEGIN_GROUP:
        // Sent to the DLL before a group of tests from that DLL is about to
        // be executed. This gives the DLL a time to initialize or allocate
        // data for the tests to follow. Only the DLL that is next to run
        // receives this message. The prior DLL, if any, will first receive
        // a SPM_END_GROUP message. For global initialization and
        // de-initialization, the DLL should probably use SPM_START_SCRIPT
        // and SPM_STOP_SCRIPT, or even SPM_LOAD_DLL and SPM_UNLOAD_DLL.
        LOG(TEXT("ShellProc(SPM_BEGIN_GROUP, ...) called"));
        g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: TUXTEST.DLL"));
        break;

    case SPM_END_GROUP:
        // Sent to the DLL after a group of tests from that DLL has completed
        // running. This gives the DLL a time to cleanup after it has been
        // run. This message does not mean that the DLL will not be called
        // again; it just means that the next test to run belongs to a
        // different DLL. SPM_BEGIN_GROUP and SPM_END_GROUP allow the DLL
        // to track when it is active and when it is not active.
        LOG(TEXT("ShellProc(SPM_END_GROUP, ...) called"));
        g_pKato->EndLevel(TEXT("END GROUP: TUXTEST.DLL"));
        break;

    case SPM_BEGIN_TEST:
        // Sent to the DLL immediately before a test executes. This gives
        // the DLL a chance to perform any common action that occurs at the
        // beginning of each test, such as entering a new logging level.
        // The spParam parameter will contain a pointer to a SPS_BEGIN_TEST
        // structure, which contains the function table entry and some other
        // useful information for the next test to execute. If the ShellProc
        // function returns SPR_SKIP, then the test case will not execute.
        LOG(TEXT("ShellProc(SPM_BEGIN_TEST, ...) called"));
        // Start our logging level.
        pBT = (LPSPS_BEGIN_TEST)spParam;
        g_pKato->BeginLevel(
            pBT->lpFTE->dwUniqueID,
            TEXT("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
            pBT->lpFTE->lpDescription,
            pBT->dwThreadCount,
            pBT->dwRandomSeed);
        break;

    case SPM_END_TEST:
        // Sent to the DLL after a single test executes from the DLL.
        // This gives the DLL a time perform any common action that occurs at
        // the completion of each test, such as exiting the current logging
        // level. The spParam parameter will contain a pointer to a
        // SPS_END_TEST structure, which contains the function table entry and
        // some other useful information for the test that just completed. If
        // the ShellProc returned SPR_SKIP for a given test case, then the
        // ShellProc() will not receive a SPM_END_TEST for that given test case.
        LOG(TEXT("ShellProc(SPM_END_TEST, ...) called"));
        // End our logging level.
        pET = (LPSPS_END_TEST)spParam;
        g_pKato->EndLevel(
            TEXT("END TEST: \"%s\", %s, Time=%u.%03u"),
            pET->lpFTE->lpDescription,
            pET->dwResult == TPR_SKIP ? TEXT("SKIPPED") :
            pET->dwResult == TPR_PASS ? TEXT("PASSED") :
            pET->dwResult == TPR_FAIL ? TEXT("FAILED") : TEXT("ABORTED"),
            pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000);
        break;

    case SPM_EXCEPTION:
        // Sent to the DLL whenever code execution in the DLL causes and
        // exception fault. TUX traps all exceptions that occur while
        // executing code inside a test DLL.
        LOG(TEXT("ShellProc(SPM_EXCEPTION, ...) called"));
        g_pKato->Log(LOG_EXCEPTION, TEXT("Exception occurred!"));
        break;

    default:
        // Any messages that we haven't processed must, by default, cause us
        // to return SPR_NOT_HANDLED. This preserves compatibility with future
        // versions of the TUX shell protocol, even if new messages are added.
        return SPR_NOT_HANDLED;
    }

    return SPR_HANDLED;
}

////////////////////////////////////////////////////////////////////////////////
