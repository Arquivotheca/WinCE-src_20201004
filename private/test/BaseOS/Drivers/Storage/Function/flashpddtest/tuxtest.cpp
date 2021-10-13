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
#include "volume.h"

// ----------------------------------------------------------------------------
// LOG
//  Printf-like wrapping around g_pKato->Log(LOG_DETAIL, ...)
//
// Parameters:
//  szFormat        Formatting string (as in printf).
//  ...             Additional arguments.
//
// Return value:
//  None.
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
// DllMain
//  Main entry point of the DLL. Called when the DLL is loaded or unloaded.
//
// Parameters:
//  hInstance       Module instance of the DLL.
//  dwReason        Reason for the function call.
//  lpReserved      Reserved for future use.
//
// Return value:
// ----------------------------------------------------------------------------
BOOL WINAPI DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(dwReason);
    UNREFERENCED_PARAMETER(lpReserved);
    // Any initialization code goes here.

    return TRUE;
}

// --------------------------------------------------------------------
// Prints out usage
// --------------------------------------------------------------------
void Usage()
{
    g_pKato->Log(LOG_DETAIL, _T("*********************************************************************************************"));
    g_pKato->Log(LOG_DETAIL, _T("Usage: [-%s <profile>] [-%s <path>] [-%s <disk>] [-%s] [-%s <regkey> -%s <dll> ] -%s/%s"),
        FLAG_STORAGE_PROFILE,
        FLAG_STORAGE_PATH,
        FLAG_DEVICE_NAME,
        FLAG_USE_OPEN_STORE,
        FLAG_PDD_REG_KEY,
        FLAG_PDD_DLL_NAME,
        FLAG_TEST_CONSENT,
        FLAG_TEST_READONLY);
    g_pKato->Log(LOG_DETAIL, _T("\nTo specify a flash drive that is already activated by the MDD driver:"));
    g_pKato->Log(LOG_DETAIL, _T("\t-%s <profile> : specifies the profile of the device to test, e.g. MSFlash"), FLAG_STORAGE_PROFILE);
    g_pKato->Log(LOG_DETAIL, _T("\t-%s <path>    : specifies the path of a mounted flash device, e.g. \\Flash Disk"), FLAG_STORAGE_PATH);
    g_pKato->Log(LOG_DETAIL, _T("\t-%s <disk>    : specifies the name of the flash device, e.g. DSK1:"), FLAG_DEVICE_NAME);
    g_pKato->Log(LOG_DETAIL, _T("\t-%s       : use OpenStore() instead of CreateFile() to get the device handle"), FLAG_USE_OPEN_STORE);
    g_pKato->Log(LOG_DETAIL, _T("\nTo exercise a flash PDD driver directly, without activating the MDD driver:"));
    g_pKato->Log(LOG_DETAIL, _T("\t-%s <regkey>  : specifies the registry path that contains the PDD settings, e.g.:"), FLAG_PDD_REG_KEY);
    g_pKato->Log(LOG_DETAIL, _T("\t\tDrivers\\BlockDevice\\MyFlash"));
    g_pKato->Log(LOG_DETAIL, _T("\t-%s <dll>     : specifies the name of the flash PDD dll, e.g.:"), FLAG_PDD_DLL_NAME);
    g_pKato->Log(LOG_DETAIL, _T("\t\tmyflashpdd.dll"));
    g_pKato->Log(LOG_DETAIL, _T("\nRequired flag(s):"));
    g_pKato->Log(LOG_DETAIL, _T("\t-%s       : this flag allows the test suite to modify and destroy data on the flash device"), FLAG_TEST_CONSENT);
    g_pKato->Log(LOG_DETAIL, _T("\t-%s       : this flag prevents the test suite from modifing  or destroying data on the flash device"), FLAG_TEST_READONLY);
    g_pKato->Log(LOG_DETAIL, _T("\t          : Either -%s or -%s must be supplied on the command line or all tests will be skipped"), FLAG_TEST_CONSENT, FLAG_TEST_READONLY);
    g_pKato->Log(LOG_DETAIL, _T("*********************************************************************************************"));
}

// --------------------------------------------------------------------
// Prints info about -zorch flag
// --------------------------------------------------------------------
BOOL TestConsent(DWORD dwTestId)
{
    if(g_fZorch)
    {
        return TRUE;
    }
    else if(g_fReadOnly)
    {
        for(DWORD i = 0; i < READONLY_TEST_COUNT; i++)
        {
            if(gc_dwReadOnlyTestIds[i] == dwTestId)
            {
                return TRUE;
            }
        }
        g_pKato->Log(LOG_SKIP, _T("DETAIL: This test is not readonly, %s flag was given.  The test case will skip"), FLAG_TEST_READONLY);
        return FALSE;
    }
    else
    {
        g_pKato->Log(LOG_SKIP, _T("WARNING: Neither -%s or -%s were supplied on the command line.  Test will skip"), FLAG_TEST_CONSENT, FLAG_TEST_READONLY);
        return FALSE;
    }
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
            ERR("Error : ParseCommandLine : out of memory");
            goto done;
        }
    }
    else
    {
        ERR("Error : ParseCommandLine : commandline is NULL");
        goto done;
    }

    // //Look for the consent flag or the read only flag// //
    // The consent flag lets the user acknowledge that
    // running this test suite will overwrite the data on their flash device
    //
    // Giving the ReadOnly flag lets the user indicate that
    // the flash drive should not be modified during testing.
    //
    // If neither flag is specified all test cases will skip.
    // This behavior is intended to prevent users running a subset
    // when they intended to run all tests.
    g_fZorch = pCmdLine->GetOpt(FLAG_TEST_CONSENT);
    g_fReadOnly = pCmdLine->GetOpt(FLAG_TEST_READONLY);
    if (!g_fZorch || !g_fReadOnly)
    {
        WARN("Warning : no -zorch or -readonly flag specified. All test cases will skip!");
    }
    if (g_fZorch && g_fReadOnly)
    {
        WARN("Warning : -zorch AND -readonly were passed in the command line.  Ignoring zorch flag.");
        g_fZorch = FALSE;
    }

    // Look for the profile flag
    if (pCmdLine->GetOpt(FLAG_STORAGE_PROFILE))
    {
        g_fProfileSpecified = pCmdLine->GetOptString(
            FLAG_STORAGE_PROFILE,
            g_szDeviceProfile,
            PROFILENAMESIZE);
    }
    // Look for path flag
    if (pCmdLine->GetOpt(FLAG_STORAGE_PATH))
    {
        if (g_fProfileSpecified)
        {
            ERR("Error : ParseCommandLine : please use a single storage specifier, such as profile, path, device name, or pdd dll name");
            goto done;
        }
        g_fPathSpecified = pCmdLine->GetOptString(
            FLAG_STORAGE_PATH,
            g_szPath,
            MAX_PATH);
    }
    // Look for device name flag
    if (pCmdLine->GetOpt(FLAG_DEVICE_NAME))
    {
        if (g_fProfileSpecified || g_fPathSpecified)
        {
            ERR("Error : ParseCommandLine : please use a single storage specifier, such as profile, path, device name, or pdd dll name");
            goto done;
        }
        g_fDeviceNameSpecified = pCmdLine->GetOptString(
            FLAG_DEVICE_NAME,
            g_szDeviceName,
            MAX_PATH);
    }
    // Look for the pdd dll name fllag
    if (pCmdLine->GetOpt(FLAG_PDD_DLL_NAME))
    {
        if (g_fProfileSpecified || g_fPathSpecified || g_fDeviceNameSpecified)
        {
            ERR("Error : ParseCommandLine : please use a single storage specifier, such as profile, path, device name, or pdd dll name");
            goto done;
        }
        g_fPDDLibrarySpecified = pCmdLine->GetOptString(
            FLAG_PDD_DLL_NAME,
            g_szPDDLibraryName,
            MAX_PATH);
        // Ensure that the registry key was also passed
        if (!pCmdLine->GetOpt(FLAG_PDD_REG_KEY))
        {
            ERR("Error : ParseCommandLine : the pdd dll parameter requires the registry key path");
            goto done;
        }
        // Read the registry key value
        pCmdLine->GetOptString(
            FLAG_PDD_REG_KEY,
            g_szPDDRegistryKey,
            MAX_PATH);
    }
    // Look for the option to enable OpenStore()
    g_fUseOpenStore = pCmdLine->GetOpt(FLAG_USE_OPEN_STORE);
    if (g_fUseOpenStore && !g_fPDDLibrarySpecified)
    {
        DETAIL("Info : ParseCommandLine : will use OpenStore() instead of CreateFile() to get device handle");
    }
    // Ensure that at least one storage specifier flag was passed
    if (!(g_fProfileSpecified || g_fPathSpecified || g_fDeviceNameSpecified || g_fPDDLibrarySpecified))
    {
        ERR("Error : ParseCommandLine : no storage profile, path, device name, or pdd dll name specified");
        goto done;
    }

    fRet = TRUE;
done:
    // Clean Up
    CHECK_DELETE(pCmdLine);
    return fRet;
}

// --------------------------------------------------------------------
// Initializes global test parameters
// --------------------------------------------------------------------
BOOL Initialize()
{
    BOOL fRet = FALSE;

    // Parse the command line
    if (!ParseCommandLine())
    {
        WARN("ParseCommandLine() did not succeed, test will skip");
        fRet = TRUE;
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
    CHECK_CLOSE_HANDLE(g_hDevice);
    CHECK_DEACTIVATE_DEVICE(g_hActivateHandle);
    CHECK_FREE(g_pFlashRegionInfoTable);
}

// ----------------------------------------------------------------------------
// ShellProc
//  Processes messages from the TUX shell.
//
// Parameters:
//  uMsg            Message code.
//  spParam         Additional message-dependent data.
//
// Return value:
//  Depends on the message.
// ----------------------------------------------------------------------------

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

