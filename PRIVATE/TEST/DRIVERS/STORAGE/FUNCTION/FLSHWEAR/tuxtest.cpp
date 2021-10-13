//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
    // Any initialization code goes here.

    return TRUE;
}

static void Usage()
{    
    Debug(TEXT(""));
    Debug(TEXT("Usage: tux -o -d flshwear -c\"/disk <disk> /repeat <count> /sectors <sectors> /readlog <readlog> /writelog <writelog> /delete /store\""));
    Debug(TEXT("       /disk <disk>         : name of the disk to test (e.g. DSK1:); this parameter is required"));
    Debug(TEXT("       /repeat <count>      : number of times to repeat each timed operation; default = %u"), g_cRepeat);
    Debug(TEXT("       /sectors <sectors>   : number of sectors for each timed operation; default = %u"), g_cSectors);
    Debug(TEXT("       /readlog <readlog>   : location of .csv file to log read performance (e.g. \\release\\read.csv); default = none"));
    Debug(TEXT("       /writelog <writelog> : location of .csv file to log write performance (e.g. \\release\\write.csv); default = none"));
    Debug(TEXT("       /delete              : perform an IOCTL_DISK_DELETE_SECTORS operation ofter each timed write operation"));
    Debug(TEXT("       /store               : open the disk using the OpenStore() API"));
    Debug(TEXT(""));

}

////////////////////////////////////////////////////////////////////////////////
// Debug
//  Printf-like wrapping around OutputDebugString.
//
// Parameters:
//  szFormat        Formatting string (as in printf).
//  ...             Additional arguments.
//
// Return value:
//  None.

void Debug(LPCTSTR szFormat, ...)
{
    static  TCHAR   szHeader[] = TEXT("FLSHWEAR: ");
    TCHAR   szBuffer[1024];

    va_list pArgs;
    va_start(pArgs, szFormat);
    lstrcpy(szBuffer, szHeader);
    wvsprintf(
        szBuffer + countof(szHeader) - 1,
        szFormat,
        pArgs);
    va_end(pArgs);

    _tcscat(szBuffer, TEXT("\r\n"));

    OutputDebugString(szBuffer);
}

// --------------------------------------------------------------------
HANDLE 
OpenGivenDevice(
    LPCTSTR pszDiskName)
// --------------------------------------------------------------------
{
    // open the device as either a store or a stream device
    if(g_fOpenAsStore)
    {
        return OpenStore(pszDiskName);
    }
    else
    {
        return CreateFile(pszDiskName, GENERIC_READ, FILE_SHARE_READ, 
                NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }
}


// --------------------------------------------------------------------
BOOL 
FindFlashDevice()
// --------------------------------------------------------------------
{
    TCHAR szDisk[MAX_PATH] = _T("");
    
    HANDLE hDisk = INVALID_HANDLE_VALUE;
    HANDLE hDetect = NULL;

    STORAGEDEVICEINFO sdi = {0};
    DISK_INFO diskInfo = {0};

    DWORD cbReturned = 0;

    if(g_fOpenAsStore)
    {
        // enumerate STORE_MOUNT_GUID devices advertized by storage manager
        hDetect = DEV_DetectFirstDevice(&STORE_MOUNT_GUID, szDisk, MAX_PATH);
    }
    else
    {
        // enumerate BLOCK_DRIVER_GUID devices advertized by device manager
        hDetect = DEV_DetectFirstDevice(&BLOCK_DRIVER_GUID, szDisk, MAX_PATH);
    }

    if(hDetect)
    {
        do
        {
            // open a handle to the enumerated device
            g_pKato->Log(LOG_DETAIL, _T("checking device \"%s\"..."), szDisk);
            hDisk = OpenGivenDevice(szDisk);
            if(INVALID_HANDLE_VALUE == hDisk)
            {
                g_pKato->Log(LOG_DETAIL, _T("unable to open mass storage device \"%s\"; error %u"), szDisk, GetLastError());
                continue;
            }

            //check its profile
            if(!DeviceIoControl(hDisk, IOCTL_DISK_DEVICE_INFO, &sdi, sizeof(STORAGEDEVICEINFO), NULL, 0, &cbReturned, NULL))
            {
                g_pKato->Log(LOG_DETAIL, _T("device \"%s\" does not support IOCTL_DISK_DEVICE_INFO (required for /profile option); error %u"), szDisk, GetLastError());
                VERIFY(CloseHandle(hDisk));
                hDisk = INVALID_HANDLE_VALUE;
                continue;
            }
            else
            {
                // check for a profile match
                if(0 != wcsicmp(_T("FlashDisk"), sdi.szProfile))
                {
                    g_pKato->Log(LOG_DETAIL, _T("device \"%s\" profile \"%s\" does not match specified profile \"FlashDisk\""), szDisk, sdi.szProfile);
                    VERIFY(CloseHandle(hDisk));
                    hDisk = INVALID_HANDLE_VALUE;
                    continue;
                }
            }

            if(!Dsk_GetDiskInfo(hDisk, &diskInfo))
            {
                g_pKato->Log(LOG_DETAIL, _T("ERROR: device \"%s\" does not appear to be a mass storage device"), szDisk);
                VERIFY(CloseHandle(hDisk));
                hDisk = INVALID_HANDLE_VALUE;
                continue;
            }
            
            g_pKato->Log(LOG_DETAIL, _T("found appropriate mass storage device: \"%s\""), szDisk);

            break;

        } while(DEV_DetectNextDevice(hDetect, szDisk, MAX_PATH));
        DEV_DetectClose(hDetect);
    }

    if(INVALID_HANDLE_VALUE == hDisk)
    {
        g_pKato->Log(LOG_DETAIL, _T("ERROR: found no mass storage devices!"));
        return FALSE;
    }
    else
    {        
        g_pKato->Log(LOG_DETAIL, _T("    Total Sectors: %u"), diskInfo.di_total_sectors); 
        g_pKato->Log(LOG_DETAIL, _T("    Bytes Per Sector: %u"), diskInfo.di_bytes_per_sect); 
        g_pKato->Log(LOG_DETAIL, _T("    Cylinders: %u"), diskInfo.di_cylinders); 
        g_pKato->Log(LOG_DETAIL, _T("    Heads: %u"), diskInfo.di_heads); 
        g_pKato->Log(LOG_DETAIL, _T("    Sectors: %u"), diskInfo.di_sectors);             
        g_pKato->Log(LOG_DETAIL, _T("    Flags: 0x%08X"), diskInfo.di_flags); 
        wcscpy(g_szDisk, szDisk);
    }

    return TRUE;
}

void AdjustDeviceName(){

    HANDLE hDisk = OpenDevice();
    if(hDisk != NULL &&  hDisk != INVALID_HANDLE_VALUE){
        STORAGEDEVICEINFO sdi = {0};
        DWORD cbReturned = 0;
        if(!DeviceIoControl(hDisk, IOCTL_DISK_DEVICE_INFO, &sdi, sizeof(STORAGEDEVICEINFO), NULL, 0, &cbReturned, NULL)){
            g_pKato->Log(LOG_DETAIL, _T("device \"%s\" does not support IOCTL_DISK_DEVICE_INFO (required for /profile option); error %u"), g_szDisk, GetLastError());
            CloseHandle(hDisk);
            hDisk = INVALID_HANDLE_VALUE;
        }
        else{
            // check for a profile match
            if(0 != wcsicmp(_T("FlashDisk"), sdi.szProfile)){
                g_pKato->Log(LOG_DETAIL, _T("device \"%s\" profile \"%s\" does not match specified profile \"FlashDisk!\""), g_szDisk, sdi.szProfile);
                VERIFY(CloseHandle(hDisk));
                hDisk = INVALID_HANDLE_VALUE;
            }
            else{
                return; // this device is a MSFlash device, good to go
            }
        }
    }

    g_pKato->Log(LOG_DETAIL, _T("!!!%s is not a MSFlash device, we have to re-enumerate the system to find one!!!"), g_szDisk);
    g_szDisk[0] = (TCHAR)0;

    if(FindFlashDevice() == FALSE){
        g_pKato->Log(LOG_FAIL, _T("!!!ERROR: There's no MSFlash device in the system!!!"));
    }
    else{
        g_pKato->Log(LOG_FAIL, _T("!!!We find MSFlash device in the system: %s!!!"), g_szDisk);
    }
}

void Initialize(void)
{
    WCHAR szFileName[MAX_PATH];
    CClParse cmdLine(g_pShellInfo->szDllCmdLine);

    if(!cmdLine.GetOptString(L"disk", g_szDisk, MAX_PATH)) {
        StringCchCopyEx(g_szDisk, MAX_PATH, L"", NULL, NULL, STRSAFE_NULL_ON_FAILURE);
    }

    if(!cmdLine.GetOptVal(L"repeat", &g_cRepeat)) {
        g_cRepeat = 2;
    }

    if(!cmdLine.GetOptVal(L"sectors", &g_cSectors)) {
        g_cSectors = 1; // sectors
    }

    g_pReadPerfLog = NULL;
    if(cmdLine.GetOptString(L"readlog", szFileName, MAX_PATH)) {
        g_pReadPerfLog = new CPerfCSV(szFileName);
        Debug(L"logging read performance to file \"%s\"\r\n", szFileName); 
    }

    g_pWritePerfLog = NULL;
    if(cmdLine.GetOptString(L"writelog", szFileName, MAX_PATH)) {
        g_pWritePerfLog = new CPerfCSV(szFileName);
        Debug(L"logging write performance to file \"%s\"\r\n", szFileName); 
    }

    g_fDelete = cmdLine.GetOpt(L"delete");

    g_fOpenAsStore = cmdLine.GetOpt(L"store");

    AdjustDeviceName();
        
    Debug(L"using \"%s\" disk device\r\n", g_szDisk); 
    Debug(L"repeat each read/write operation %u times\r\n", g_cRepeat); 
    Debug(L"will write %u sectors during each read/write operation\r\n", g_cSectors); 
    if(g_fOpenAsStore) {
        Debug(L"will open the device using OpenStore() API");
    }
    if(g_fDelete) {
        Debug(L"will send IOCTL_DISK_DELETE_SECTORS between write operations");
    }
}

void Shutdown(void)
{
    if(g_pReadPerfLog) delete g_pReadPerfLog;
    if(g_pWritePerfLog) delete g_pWritePerfLog;
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
        Debug(TEXT("ShellProc(SPM_LOAD_DLL, ...) called"));

        // If we are UNICODE, then tell Tux this by setting the following flag.
#ifdef UNICODE
        ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
#endif // UNICODE
        g_pKato = (CKato*)KatoGetDefaultObject();

        Usage();

        break;

    case SPM_UNLOAD_DLL:
        // Sent once to the DLL immediately before it is unloaded.
        Shutdown();
        Debug(TEXT("ShellProc(SPM_UNLOAD_DLL, ...) called"));
        break;

    case SPM_SHELL_INFO:
        // Sent once to the DLL immediately after SPM_LOAD_DLL to give the DLL
        // some useful information about its parent shell and environment. The
        // spParam parameter will contain a pointer to a SPS_SHELL_INFO
        // structure. The pointer to the structure may be stored for later use
        // as it will remain valid for the life of this Tux Dll. The DLL may
        // return SPR_FAIL to prevent the DLL from continuing to load.
        Debug(TEXT("ShellProc(SPM_SHELL_INFO, ...) called"));

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
        Debug(TEXT("ShellProc(SPM_REGISTER, ...) called"));
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
        Debug(TEXT("ShellProc(SPM_START_SCRIPT, ...) called"));
        break;

    case SPM_STOP_SCRIPT:
        // Sent to the DLL when the script has stopped. This message is sent
        // when the script reaches its end, or because the user pressed
        // stopped prior to the end of the script. This message is sent to
        // all Tux DLLs, including loaded Tux DLLs that are not in the script.
        Debug(TEXT("ShellProc(SPM_STOP_SCRIPT, ...) called"));
        break;

    case SPM_BEGIN_GROUP:
        // Sent to the DLL before a group of tests from that DLL is about to
        // be executed. This gives the DLL a time to initialize or allocate
        // data for the tests to follow. Only the DLL that is next to run
        // receives this message. The prior DLL, if any, will first receive
        // a SPM_END_GROUP message. For global initialization and
        // de-initialization, the DLL should probably use SPM_START_SCRIPT
        // and SPM_STOP_SCRIPT, or even SPM_LOAD_DLL and SPM_UNLOAD_DLL.
        Debug(TEXT("ShellProc(SPM_BEGIN_GROUP, ...) called"));
        g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: TUXTEST.DLL"));
        break;

    case SPM_END_GROUP:
        // Sent to the DLL after a group of tests from that DLL has completed
        // running. This gives the DLL a time to cleanup after it has been
        // run. This message does not mean that the DLL will not be called
        // again; it just means that the next test to run belongs to a
        // different DLL. SPM_BEGIN_GROUP and SPM_END_GROUP allow the DLL
        // to track when it is active and when it is not active.
        Debug(TEXT("ShellProc(SPM_END_GROUP, ...) called"));
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
        Debug(TEXT("ShellProc(SPM_BEGIN_TEST, ...) called"));
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
        Debug(TEXT("ShellProc(SPM_END_TEST, ...) called"));
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
        Debug(TEXT("ShellProc(SPM_EXCEPTION, ...) called"));
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
