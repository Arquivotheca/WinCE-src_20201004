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
#include "main.h"
#include "globals.h"
#include "fsutils.h"
#include "devinfo.h"

// debugging
#define Debug NKDbgPrintfW

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

BOOL WINAPI DllMain(HANDLE /*hInstance*/, ULONG /*dwReason*/, LPVOID /*lpReserved*/)
{
    // Any initialization code goes here.

    return TRUE;
}


// --------------------------------------------------------------------
// Prints out usage
// --------------------------------------------------------------------
void Usage()
{
    Debug(_T("Usage : %s -c\"[-p <profile>|-r <path>|-root] [-nb -wt -flush] [-cpu] [-desc <description>]\""), MODULE_NAME);
    Debug(_T("      -p <profile> : profile of the storage device, e.g. HDProfile, SDMemory, PCMCIA"));
    Debug(_T("      -r <path> : file system path, e.g. \\, \\Storage Card, \\Hard Disk"));
    Debug(_T("      -root : test the root filesystem (same as -r \\)"));
    Debug(_T("      -nb : create files with FILE_FLAG_NO_BUFFERING (can be combined with -wt)"));
    Debug(_T("      -wt : create files with FILE_FLAG_WRITE_THROUGH (can be combined with -nb)"));
    Debug(_T("      -flush : include the CloseHandle()/FlushFileBuffers() time in performance calculations"));
    Debug(_T("      -cpu : include the CPU usage statistics in the performance log"));
    Debug(_T("      -desc <description> : optional string that will be included in the log output"));
    Debug(_T("      -iosize # : I/O buffer size for test cases 1001-1005 (default = 512)"));
    Debug(_T("      -filesize # : file size for test cases 1001-1005,2001-2014 (default = 2MB)"));
    Debug(_T("      -rand : for tests that perform random I/O, uses tux-provided random seed"));
    Debug(_T(""));
    Debug(_T("Note : If the file VirtualReleaseOverride.txt exists at the root of the device "));
    Debug(_T("       and contains a valid path on the first line, fsperflog.xml will be stored "));
    Debug(_T("       along that path.  If VirtualReleaseOverride.txt does not exist, \\release will be used.  "));
    Debug(_T("       If \\release does not exist, the module's starting directory will be used. "));
    
}

// --------------------------------------------------------------------
// Creates a marker name for perf logging
// --------------------------------------------------------------------
LPTSTR CreateMarkerName(__out_ecount(cbLength) LPTSTR pszPerfMarkerName,
                        DWORD /*cbLength*/,
                        LPCTSTR pszTestName)
{
    VERIFY(SUCCEEDED(StringCchPrintf(pszPerfMarkerName, CPerfScenario::MAX_NAME_LENGTH, _T("%s"), pszTestName)));
    return pszPerfMarkerName;

}


// --------------------------------------------------------------------
// Creates a new logging object
// --------------------------------------------------------------------
CPerfHelper * MakeNewPerflogger(
    LPCTSTR pszPerfMarkerName,
    LPCTSTR pszPerfMarkerParams,
    PerfDataType dataType,
    DWORD dwDataSize,
    DWORD dwFullSize,
    float /*flScalingFactor*/)
{
    CPerfHelper * pPerfObject = NULL;

    pPerfObject = new CPerfScenario(pszPerfMarkerName, pszPerfMarkerParams, dataType, dwDataSize, dwFullSize, (float)g_dwNumTestIterations);

    return pPerfObject;
}

// --------------------------------------------------------------------
// Returns true if dwArg <= dwUpperLimit and dwArg is a power of 2
// --------------------------------------------------------------------
BOOL IsValidDataSize(DWORD dwArg, DWORD dwUpperLimit)
{
    BOOL fRet = FALSE;
    DWORD dwPowOfTwo = 1;

    if(dwArg > dwUpperLimit || dwArg < 1)
        goto done;

    while(dwPowOfTwo <= dwArg)
    {
        if(dwArg%dwPowOfTwo != 0)
            goto done;
        dwPowOfTwo = dwPowOfTwo << 1;
    }
    
    fRet = TRUE;
done:
    return fRet;
}

// --------------------------------------------------------------------
// Parses commandline passed to the tux framework
// --------------------------------------------------------------------
BOOL ParseCommandLine()
{
    CClParse* pCmdLine = NULL;
    BOOL fRet = FALSE;

    // Read in the commandline
    if (g_pShellInfo->szDllCmdLine)
    {
        pCmdLine = new CClParse(g_pShellInfo->szDllCmdLine);
        if (!pCmdLine)
        {
            ERR("ParseCommandLine : Out of memory!");
            goto done;
        }
    }
    else
    {
        ERR("ParseCommandLine : error, commandline is NULL");
        goto done;
    }

    // Look for the profile flag
    if (pCmdLine->GetOpt(FLAG_STORAGE_PROFILE))
    {
        g_fProfileSpecified = pCmdLine->GetOptString(
            FLAG_STORAGE_PROFILE,
            g_szProfile,
            PROFILENAMESIZE);
    }

    // Look for the path flag
    if (pCmdLine->GetOpt(FLAG_PATH))
    {
        g_fPathSpecified = pCmdLine->GetOptString(
            FLAG_PATH,
            g_szTestPath,
            MAX_PATH);

        if (g_fPathSpecified)
        {
            // User can not specify both parameters at once
            g_fProfileSpecified = FALSE;
        }
    }

    // Look for the storage options flag
    if (pCmdLine->GetOpt(FLAG_STORAGE_DESCRIPTOR))
    {
        pCmdLine->GetOptString(
            FLAG_STORAGE_DESCRIPTOR,
            g_szStorageDescriptor,
            MAX_PATH);
    }

    // Look for the CPU flag
    if (pCmdLine->GetOpt(FLAG_LOG_CPU))
    {
        g_fLogCPUData = TRUE;
        g_dwSysMonitorInterval = DEFAULT_SYS_MON_INTERVAL;
    }

    // Look for write through flag
    if (pCmdLine->GetOpt(FLAG_WRITETHROUGH))
    {
        g_dwFileCreateFlags |= FILE_FLAG_WRITE_THROUGH;
    }

    // Look up the no buffering flag
    if (pCmdLine->GetOpt(FLAG_NO_BUFFERING))
    {
        g_dwFileCreateFlags |= FILE_FLAG_NO_BUFFERING;
    }

    // Look up the flush flag
    if (pCmdLine->GetOpt(FLAG_TIME_FILE_FLUSH))
    {
        g_fTimeFlush = TRUE;
    }

    // Look for the "Root File System" flag
    if (pCmdLine->GetOpt(FLAG_USE_ROOT_FILE_SYSTEM))
    {
        g_fPathSpecified = FALSE;
        g_fProfileSpecified = FALSE;
        g_fUseRootFileSystem = TRUE;
        g_dwFileCreateFlags = FILE_ATTRIBUTE_NORMAL;
    }

    // Look for the io size flag
    if(pCmdLine->GetOpt(FLAG_IO_SIZE))
    {
        pCmdLine->GetOptVal(FLAG_IO_SIZE, &g_dwDataSize);
        if(!IsValidDataSize(g_dwDataSize, MAX_IO_DATA_SIZE))
        {
            Debug(_T("User-specified data size exceeds max size %u, or is not a power of two. Using default size = %u."),
                MAX_IO_DATA_SIZE,
                DEFAULT_DATA_SIZE);
            g_dwDataSize = DEFAULT_DATA_SIZE;
        }
    }

    // Look for the filesize flag
    if(pCmdLine->GetOpt(FLAG_FILE_SIZE))
    {
        pCmdLine->GetOptVal(FLAG_FILE_SIZE, &g_dwFileSize);
        if(!IsValidDataSize(g_dwFileSize, MAX_FILE_SIZE))
        {
            Debug(_T("User-specified file size exceeds max size %u, or is not a power of two. Using default size = %u."),
                MAX_FILE_SIZE,
                DEFAULT_FILE_SIZE);
            g_dwFileSize = DEFAULT_FILE_SIZE;
        }
    }

    // Look for the burst write flag
    if(pCmdLine->GetOpt(FLAG_BURST_WRITE_PAUSE))
    {
        pCmdLine->GetOptVal(
            FLAG_BURST_WRITE_PAUSE,
            &g_dwBurstWritePauseMs);
        g_fDoBurstWrite = TRUE;
    }

    // Look for the random seed flag
    if (pCmdLine->GetOpt(FLAG_USE_TUX_RANDOM_SEED))
    {
        g_fUseTuxRandomSeed = TRUE;
    }


    // If no storage options were specified, use the default profile
    if (!(g_fProfileSpecified || g_fPathSpecified || g_fUseRootFileSystem))
    {
        g_fProfileSpecified = TRUE;
        Debug(_T("The user did not specify a storage profile [-p], path [-r], or root filesystem [-root] parameter"));
        Debug(_T("Using the default storage profile %s"), g_szProfile);
    }

    fRet = TRUE;
done:
    // Clean Up
    CHECK_DELETE(pCmdLine);

    return fRet;
}

// --------------------------------------------------------------------
// Initialize perf logging functionality
// --------------------------------------------------------------------
BOOL InitPerfLog()
{
    BOOL fRet = FALSE;
    //send false for "fUseReleaseDirectory", this causes release directory to 
    //be selected using the criteria described in this test's usage info
    if (g_fProfileSpecified)
    {
        if (!CPerfScenario::Initialize(MODULE_NAME, FALSE, g_szProfile))
        {
            goto done;
        }
    }
    else if (g_fPathSpecified)
    {
        if (!CPerfScenario::Initialize(MODULE_NAME, FALSE, g_szTestPath))
        {
            goto done;
        }
    } 
    else if (g_fUseRootFileSystem)
    {
        if (!CPerfScenario::Initialize(MODULE_NAME, FALSE, L"\\"))
        {
            goto done;
        }
    }
    else
    {
        if (!CPerfScenario::Initialize(MODULE_NAME, FALSE, L""))
        {
            goto done;
        }
    }

    fRet = TRUE;

done:
    return fRet;
}

// --------------------------------------------------------------------
// Initializes the filesystem object
// --------------------------------------------------------------------
BOOL InitFileSys()
{
    BOOL fRet = FALSE;
    CFSUtils * pCFSUtils = NULL;

    // Each test case will call this procedure
    // Check if the file system interface has already been initialized
    if (g_fFileSysInitPerformed)
    {
        return (g_pCFSUtils != NULL);
    }
    g_fFileSysInitPerformed = TRUE;

    if (g_fProfileSpecified)
    {
        pCFSUtils = CFSUtils::CreateWithProfile(g_szProfile);
    }
    else if (g_fPathSpecified)
    {
        pCFSUtils = CFSUtils::CreateWithPath(g_szTestPath);
    }
    else if (g_fUseRootFileSystem)
    {
        pCFSUtils = CFSUtils::CreateWithPath(_T("\\"));
    }

    if ((NULL == pCFSUtils) || (!pCFSUtils->IsValid()))
    {
        ERR("InitFileSys() : invalid filesystem parameters");
        goto done;
    }

    // Initialize gloabl utils object
    g_pCFSUtils = pCFSUtils;

    fRet = TRUE;
done:
    return fRet;
}

// --------------------------------------------------------------------
// Initializes global values
// --------------------------------------------------------------------
BOOL InitGlobals()
{
    // Get the build number
    g_osVerInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    if (!GetVersionEx(&g_osVerInfo))
    {
        g_osVerInfo.dwBuildNumber = 0;
        g_osVerInfo.dwMajorVersion = 0;
        g_osVerInfo.dwMinorVersion = 0;
    }

    return TRUE;
}

// --------------------------------------------------------------------
// Initializes global test parameters
// --------------------------------------------------------------------
BOOL Initialize()
{
    BOOL fRet = FALSE;

    // Parse the command line
    if (!(ParseCommandLine() && InitGlobals()))
    {
        goto done;
    }

    fRet = TRUE;
done:
    return fRet;
}

// --------------------------------------------------------------------
// Cleanup and housekeeping
// --------------------------------------------------------------------
void Shutdown()
{
    // Clean up memory
    CHECK_DELETE(g_pCFSUtils);
    return;
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
        break;

    case SPM_UNLOAD_DLL:
        // Sent once to the DLL immediately before it is unloaded.
        Debug(TEXT("ShellProc(SPM_UNLOAD_DLL, ...) called"));
        Shutdown();
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

        // Print out the usage
        Usage();

        // Break if the initialization fails
        if (!Initialize())
        {
            return SPR_FAIL;
        }
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
        g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: FSPERFLOG.DLL"));
        break;

    case SPM_END_GROUP:
        // Sent to the DLL after a group of tests from that DLL has completed
        // running. This gives the DLL a time to cleanup after it has been
        // run. This message does not mean that the DLL will not be called
        // again; it just means that the next test to run belongs to a
        // different DLL. SPM_BEGIN_GROUP and SPM_END_GROUP allow the DLL
        // to track when it is active and when it is not active.
        Debug(TEXT("ShellProc(SPM_END_GROUP, ...) called"));
        g_pKato->EndLevel(TEXT("END GROUP: FSPERFLOG.DLL"));
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
