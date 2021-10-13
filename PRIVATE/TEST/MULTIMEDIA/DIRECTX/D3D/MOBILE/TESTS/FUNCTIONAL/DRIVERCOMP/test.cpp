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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include <tchar.h>
#include <katoutils.h>
#include <clparse.h>   // Command-line parsing utility
#include "ft.h"
#include "ImageManagement.h"
#include "TestIDs.h"
#include "DebugOutput.h"

#include "ParseArgs.h"
#include "TestWindow.h"
#include "Initializer.h"
#include "DriverComp.h"

//Avoid C4100
#define UNUSED_PARAM(_a) _a

//
// Any cmd-line argument value string that exceeds 100 characters
// will be truncated.
//
#define D3DQA_MAX_OPT_LEN 100

CKato *g_pKato;

HANDLE g_hInstance = NULL;

static CALLER_INSTRUCTIONS g_CallerInstr;
static WINDOW_ARGS g_WindowArgs;

bool ParseCommonCmdLineArgs(LPCALLER_INSTRUCTIONS pCallerInstr,
                            LPWINDOW_ARGS pWindowArgs);

//
// DllMain
//
//   Main entry point of the DLL. Called when the DLL is loaded or unloaded.
//
// Return Value:
// 
//   BOOL
//
BOOL WINAPI DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved)
{
    UNUSED_PARAM(dwReason);
    UNUSED_PARAM(lpReserved);

    g_hInstance = hInstance;
    return TRUE;
}

//
// ShellProc
//
//   Processes messages from the TUX shell.
//
// Arguments:
//
//   uMsg            Message code.
//   spParam         Additional message-dependent data.
//
// Return value:
//
//   Depends on the message.
//
SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam)
{
    LPSPS_BEGIN_TEST    pBT;
    LPSPS_END_TEST      pET;

    switch (uMsg)
    {
    case SPM_LOAD_DLL:
        // This message is sent once to the DLL immediately after it is loaded.
        // The ShellProc must set the fUnicode member of this structure to TRUE
        // if the DLL was built with the UNICODE flag set.
        DebugOut(TEXT("ShellProc(SPM_LOAD_DLL, ...) called"));

#ifdef UNICODE
        ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
#endif // UNICODE
        g_pKato = (CKato*)KatoGetDefaultObject();
        SetKato(g_pKato);
        SetImageManagementLogger(DebugOut);
        break;

    case SPM_UNLOAD_DLL:
        // Sent once to the DLL immediately before it is unloaded.
        DebugOut(TEXT("ShellProc(SPM_UNLOAD_DLL, ...) called"));
        SetKato(NULL);
        SetImageManagementLogger(NULL);
        break;

    case SPM_SHELL_INFO:
        // This message is sent once to the DLL immediately following the
        // SPM_LOAD_DLL message to give the DLL information about its parent shell,
        // via SPS_SHELL_INFO.
        DebugOut(TEXT("ShellProc(SPM_SHELL_INFO, ...) called"));

        // Store a pointer to our shell info, in case it is useful later on
        g_pShellInfo = (LPSPS_SHELL_INFO)spParam;
        
        if (false == ParseCommonCmdLineArgs(&g_CallerInstr, &g_WindowArgs))
        {
            DebugOut(DO_ERROR, L"Command line parsing failed. Skipping.");
            return SPR_FAIL;
        }

        break;

    case SPM_REGISTER:
        // This message is sent to query the DLL for its function table. 
        DebugOut(TEXT("ShellProc(SPM_REGISTER, ...) called"));
        ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;

#ifdef UNICODE
        return SPR_HANDLED | SPF_UNICODE;
#else // UNICODE
        return SPR_HANDLED;
#endif // UNICODE

    case SPM_START_SCRIPT:
        // This message is sent to the DLL immediately before a script is started
        DebugOut(TEXT("ShellProc(SPM_START_SCRIPT, ...) called"));
        break;

    case SPM_STOP_SCRIPT:
        // This message is sent to the DLL after the script has stopped.
        DebugOut(TEXT("ShellProc(SPM_STOP_SCRIPT, ...) called"));
        break;

    case SPM_BEGIN_GROUP:
        // Sent to the DLL before a group of tests from that DLL is about to
        // be executed. This gives the DLL a time to initialize or allocate
        // data for the tests to follow.
        DebugOut(TEXT("ShellProc(SPM_BEGIN_GROUP, ...) called"));
        g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: TUXTEST.DLL"));
        InitPowerRequirements();
        break;

    case SPM_END_GROUP:
        // Sent to the DLL after a group of tests from that DLL has completed
        // running. This gives the DLL a time to cleanup after it has been
        // run. This message does not mean that the DLL will not be called
        // again; it just means that the next test to run belongs to a
        // different DLL. SPM_BEGIN_GROUP and SPM_END_GROUP allow the DLL
        // to track when it is active and when it is not active.
        DebugOut(TEXT("ShellProc(SPM_END_GROUP, ...) called"));
        g_pKato->EndLevel(TEXT("END GROUP: TUXTEST.DLL"));
        FreePowerRequirements();
        break;

    case SPM_BEGIN_TEST:
        // This message is sent to the DLL before a single test or group of
        // tests from that DLL is executed. 
       
        DebugOut(TEXT("ShellProc(SPM_BEGIN_TEST, ...) called"));
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
        // This message is sent to the DLL after a single test case from
        // the DLL executes. 
        
        DebugOut(TEXT("ShellProc(SPM_END_TEST, ...) called"));
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
        DebugOut(TEXT("ShellProc(SPM_EXCEPTION, ...) called"));
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

int GetModulePath(HMODULE hMod, __out_ecount(dwCchSize) TCHAR * tszPath, DWORD dwCchSize)
{
    int iLen = GetModuleFileName(hMod, tszPath, dwCchSize);

    while (iLen > 0 && tszPath[iLen - 1] != _T('\\'))
        --iLen;

    // Changing the item at iLen to '\0' will leave a trailing \ on the path.
    if (iLen >= 0)
    {
        tszPath[iLen] = 0;
    }
    return iLen;
}

//
// ParseCommonCmdLineArgs
//
//   Parse cmd-line args that are consistent for all tests.  Args that are specific
//   to one particular test case are handled in the specific TESTPROCAPI that defines
//   that case.
//
// Return Value:
//
//  bool:  false indicates that execution should be aborted (for example, if usage
//         was requested), true indicates that execution should continue
//
bool ParseCommonCmdLineArgs(LPCALLER_INSTRUCTIONS pCallerInstr,
                            LPWINDOW_ARGS pWindowArgs)
{
    //
    // Temporary storage for use with cmd-line parser
    //
    WCHAR wszArg[D3DQA_MAX_OPT_LEN];

    //
    // Arg validation
    //
    if ((NULL == pCallerInstr) || (NULL == pWindowArgs))
    {
        DebugOut(L"ParseCommonCmdLineArgs aborting due to invalid arguments.");
        return false;
    }

    //
    // Command-line parsing
    //
    if ((NULL == g_pShellInfo) || (NULL == g_pShellInfo->szDllCmdLine))
    {
        DebugOut(L"No command-line arguments specified.  Using defaults.");
        return false;
    }
    else
    {
        CClParse CmdLineParse(g_pShellInfo->szDllCmdLine);

        GetModulePath(
            (HMODULE)g_hInstance, 
            g_pwchImageComparitorDir0, 
            sizeof(g_pwchImageComparitorDir0)/sizeof(*g_pwchImageComparitorDir0));
        GetModulePath(
            (HMODULE)g_hInstance, 
            g_pwchImageComparitorDir1, 
            sizeof(g_pwchImageComparitorDir1)/sizeof(*g_pwchImageComparitorDir1));
        GetModulePath(
            (HMODULE)g_hInstance, 
            g_pwchTestDependenciesDir0, 
            sizeof(g_pwchTestDependenciesDir0)/sizeof(*g_pwchTestDependenciesDir0));
        GetModulePath(
            (HMODULE)g_hInstance, 
            g_pwchTestDependenciesDir1, 
            sizeof(g_pwchTestDependenciesDir1)/sizeof(*g_pwchTestDependenciesDir1));
        GetModulePath(
            (HMODULE)g_hInstance, 
            g_pwchSoftwareDeviceFilename0, 
            sizeof(g_pwchSoftwareDeviceFilename0)/sizeof(*g_pwchSoftwareDeviceFilename0));
        GetModulePath(
            (HMODULE)g_hInstance, 
            g_pwchSoftwareDeviceFilename1, 
            sizeof(g_pwchSoftwareDeviceFilename1)/sizeof(*g_pwchSoftwareDeviceFilename1));

        //
        // Initialize string pointers to valid storage
        //
        pCallerInstr->TestCaseArgs[0].pwchImageComparitorDir = g_pwchImageComparitorDir0;
        pCallerInstr->TestCaseArgs[1].pwchImageComparitorDir = g_pwchImageComparitorDir1;

        pCallerInstr->TestCaseArgs[0].pwchTestDependenciesDir = g_pwchTestDependenciesDir0;
        pCallerInstr->TestCaseArgs[1].pwchTestDependenciesDir = g_pwchTestDependenciesDir1;
        
        pCallerInstr->TestCaseArgs[0].pwchSoftwareDeviceFilename = g_pwchSoftwareDeviceFilename0;
        pCallerInstr->TestCaseArgs[1].pwchSoftwareDeviceFilename = g_pwchSoftwareDeviceFilename1;

        //
        // Assign class / wnd names
        //
        pWindowArgs->lpClassName  = g_lpClassName;
        pWindowArgs->lpWindowName = g_lpWindowName;



        //
        // If the "-?" command-line argument exists, show the usage information,
        // and skip the test.
        //
        if (CmdLineParse.GetOpt(_T("?")))
        {
            g_pKato->Log(LOG_COMMENT,_T(""));
            g_pKato->Log(LOG_COMMENT,_T("Description:"));
            g_pKato->Log(LOG_COMMENT,_T("\t"));
            g_pKato->Log(LOG_COMMENT,_T("\tDriver comparison tests.  Verifies output of a Direct3D Mobile driver by comparing to \"known good\" image captures."));
            g_pKato->Log(LOG_COMMENT,_T("\tThe source of the \"known good\" captures is typically the Direct3D Mobile Reference Driver                  "));
            g_pKato->Log(LOG_COMMENT,_T("\t"));
            g_pKato->Log(LOG_COMMENT,_T("Syntax:"));
            g_pKato->Log(LOG_COMMENT,_T("\t"));
            g_pKato->Log(LOG_COMMENT,_T("\ttux -o -d D3DM_DriverComp.dll -c \"[arguments]\""));
            g_pKato->Log(LOG_COMMENT,_T("\t"));
            g_pKato->Log(LOG_COMMENT,_T("\tTest Mode:"));
            g_pKato->Log(LOG_COMMENT,_T("\t"));
            g_pKato->Log(LOG_COMMENT,_T("\t\t/t1: Test mode #1;  instantiate both the Direct3D Mobile Reference Driver and a production driver for each test case and compare output (default)"));
            g_pKato->Log(LOG_COMMENT,_T("\t"));
            g_pKato->Log(LOG_COMMENT,_T("\t\t/t2: Test mode #2;  instantiate only a production driver for each test case and compare to stored Direct3D Mobile Reference Driver output"));
            g_pKato->Log(LOG_COMMENT,_T("\t"));
            g_pKato->Log(LOG_COMMENT,_T("\t\t/t3: Test mode #3;  instantiate only the Direct3D Mobile Reference Driver for each test case and store output for future use (no comparison)"));
            g_pKato->Log(LOG_COMMENT,_T("\t\t                    (for debugging purposes, to generate frames from a production driver, specify \"/p\" on command-line)"));
            g_pKato->Log(LOG_COMMENT,_T("\t\t"));
            g_pKato->Log(LOG_COMMENT,_T("\t"));
            g_pKato->Log(LOG_COMMENT,_T("\t"));
            g_pKato->Log(LOG_COMMENT,_T("\tDirectories:"));
            g_pKato->Log(LOG_COMMENT,_T("\t"));
            g_pKato->Log(LOG_COMMENT,_T("\t\t/c: Path for comparitor's images (both loading and saving; include trailing '\\')."));
            g_pKato->Log(LOG_COMMENT,_T("\t\t"));
            g_pKato->Log(LOG_COMMENT,_T("\t"));
            g_pKato->Log(LOG_COMMENT,_T("\tWindow extents:"));
            g_pKato->Log(LOG_COMMENT,_T("\t"));
            g_pKato->Log(LOG_COMMENT,_T("\t\t/e1: Window extents of 64x64 (default)."));
            g_pKato->Log(LOG_COMMENT,_T("\t\t/e2: Window extents of 220x176."));
            g_pKato->Log(LOG_COMMENT,_T("\t\t/e3: Window extents of 176x220."));
            g_pKato->Log(LOG_COMMENT,_T("\t\t/e4: Window extents of 320x240."));
            g_pKato->Log(LOG_COMMENT,_T("\t\t/e5: Window extents of 240x320."));
            g_pKato->Log(LOG_COMMENT,_T("\t\t/e6: Window extents of 640x480."));
            g_pKato->Log(LOG_COMMENT,_T("\t\t/e7: Full screen in current mode."));
            g_pKato->Log(LOG_COMMENT,_T("\t"));
            g_pKato->Log(LOG_COMMENT,_T("\tWindow styles:"));
            g_pKato->Log(LOG_COMMENT,_T("\t"));
            g_pKato->Log(LOG_COMMENT,_T("\t\t/w1: WS_OVERLAPPED (default)."));
            g_pKato->Log(LOG_COMMENT,_T("\t\t/w2: WS_POPUP."));
            g_pKato->Log(LOG_COMMENT,_T("\t"));
            g_pKato->Log(LOG_COMMENT,_T("\tDevice:"));
            g_pKato->Log(LOG_COMMENT,_T("\t"));
            g_pKato->Log(LOG_COMMENT,_T("\t\t/s: DLL filename of software device to register (if present test loads the DLL, calls RegisterSoftwareDevice,"));
            g_pKato->Log(LOG_COMMENT,_T("\t\t     and creates the device with D3DMADAPTER_REGISTERED_DEVICE; otherwise, the test uses the system's default"));
            g_pKato->Log(LOG_COMMENT,_T("\t\t     adapter by creating the device with D3DMADAPTER_DEFAULT)."));
            g_pKato->Log(LOG_COMMENT,_T("\t"));
            g_pKato->Log(LOG_COMMENT,_T("\tGeneral:"));
            g_pKato->Log(LOG_COMMENT,_T("\t"));
            g_pKato->Log(LOG_COMMENT,_T("\t\t/?: Display command-line argument syntax."));
            g_pKato->Log(LOG_COMMENT,_T("\t\t/i: Keep comparitor input frames generated during test execution."));
            g_pKato->Log(LOG_COMMENT,_T("\t\t/o: Keep comparitor output frames."));
            g_pKato->Log(LOG_COMMENT,_T("\t"));

            //
            // Abort execution; user merely wanted to see the usage information
            //
            return false;
        }


        if (CmdLineParse.GetOpt(_T("t2")))
        {

            //
            // Test mode #2:
            //
            // Instantiate only a production driver for each test case and compare to stored
            // Direct3D Mobile Reference Driver output.
            //

            pCallerInstr->bCompare = TRUE;    
            pCallerInstr->bTestValid[0] = TRUE;
            pCallerInstr->bTestValid[1] = FALSE;
            pCallerInstr->bKeepInputFrames[1] = TRUE; // Don't want to delete ref frames that existed prior to test

            //
            // Keep input frames?
            //
            if (CmdLineParse.GetOpt(_T("i")))
                pCallerInstr->bKeepInputFrames[0] = TRUE;
            else
                pCallerInstr->bKeepInputFrames[0] = FALSE;

            //
            // Set up device instance 0
            //
            if (CmdLineParse.GetOptString(_T("s"), wszArg, D3DQA_MAX_OPT_LEN))
            {
                //
                // Register a software device
                //
                StringCchCopy(pCallerInstr->TestCaseArgs[0].pwchSoftwareDeviceFilename, D3DQA_FILELENGTH,  wszArg);
                pCallerInstr->TestCaseArgs[0].uiAdapterOrdinal = D3DMADAPTER_REGISTERED_DEVICE;
            }
            else
            {
                //
                // Use the default adapter
                //
                pCallerInstr->TestCaseArgs[0].pwchSoftwareDeviceFilename = NULL;
                pCallerInstr->TestCaseArgs[0].uiAdapterOrdinal = D3DMADAPTER_DEFAULT;
            }

            //
            // Set up device instance 1
            //
            pCallerInstr->TestCaseArgs[1].pwchSoftwareDeviceFilename = NULL;
            pCallerInstr->TestCaseArgs[1].uiAdapterOrdinal = 0;
            


        }
        else if (CmdLineParse.GetOpt(_T("t3")))
        {
            //
            // Test mode #3:
            //
            // Instantiate only the Direct3D Mobile Reference Driver for each test case and store
            // output for future use (no comparison).  
            //
            // (for debugging purposes the device arguments can be used to run in this mode for a
            //  production driver)
            //

            pCallerInstr->bCompare = FALSE;    


            //
            // Set up device instance 0  --  unused in this mode
            //
            pCallerInstr->bTestValid[0] = FALSE;
            pCallerInstr->TestCaseArgs[0].uiAdapterOrdinal = 0;           
            pCallerInstr->bKeepInputFrames[0] = TRUE;  // test shouldn't delete frames that it did not generate
            pCallerInstr->TestCaseArgs[0].pwchSoftwareDeviceFilename = NULL;

            //
            // Set up device instance 1 -- definitely used in this mode
            //
            pCallerInstr->bTestValid[1] = TRUE;
            pCallerInstr->bKeepInputFrames[1] = TRUE;  // Assumed; see help

            //
            // This mode is typically used for "known good" frame capture using the reference driver;
            // however, the user can optionally capture frames with a production driver of their choice.
            //
            // Initialize three items:
            //
            //    (1) Adapter ordinal for CreateDevice
            //    (2) Filename for RegisterSoftwareDevice (only if using D3DMADAPTER_REGISTERED_DEVICE)
            //    (3) BOOL to indicate whether Create
            //
            if (CmdLineParse.GetOpt(_T("p")))
            {
                //
                // Register a software device, or use default adapter?
                //
                if (CmdLineParse.GetOptString(_T("s"), wszArg, D3DQA_MAX_OPT_LEN))
                {
                    StringCchCopy(pCallerInstr->TestCaseArgs[1].pwchSoftwareDeviceFilename, D3DQA_FILELENGTH,  wszArg);
                    pCallerInstr->TestCaseArgs[1].uiAdapterOrdinal = D3DMADAPTER_REGISTERED_DEVICE;
                }
                else // default adapter
                {
                    pCallerInstr->TestCaseArgs[1].pwchSoftwareDeviceFilename = NULL;
                    pCallerInstr->TestCaseArgs[1].uiAdapterOrdinal = D3DMADAPTER_DEFAULT;
                }
            }
            else
            {
                //
                // Reference driver
                //
                StringCchCopy(pCallerInstr->TestCaseArgs[1].pwchSoftwareDeviceFilename, D3DQA_FILELENGTH,  D3DQA_D3DMREF_FILENAME);
                pCallerInstr->TestCaseArgs[1].uiAdapterOrdinal = D3DMADAPTER_REGISTERED_DEVICE;
            }

        }
        else // default, or, if (CmdLineParse.GetOpt(_T("t1")))
        {

            //
            // Test mode #1 (default):
            //
            // Instantiate both the Direct3D Mobile Reference Driver and a production driver
            // for each test case and compare output
            //

            pCallerInstr->bCompare = TRUE;    

            //
            // Keep input frames?
            //
            if (CmdLineParse.GetOpt(_T("i")))
            {
                pCallerInstr->bKeepInputFrames[0] = TRUE;
                pCallerInstr->bKeepInputFrames[1] = TRUE;
            }
            else
            {
                pCallerInstr->bKeepInputFrames[0] = FALSE;
                pCallerInstr->bKeepInputFrames[1] = FALSE;
            }

            //
            // Set up device instance 0
            //
            pCallerInstr->bTestValid[0] = TRUE;
            if (CmdLineParse.GetOptString(_T("s"), wszArg, D3DQA_MAX_OPT_LEN))
            {
                //
                // Register a software device
                //
                StringCchCopy(pCallerInstr->TestCaseArgs[0].pwchSoftwareDeviceFilename, D3DQA_FILELENGTH,  wszArg);
                pCallerInstr->TestCaseArgs[0].uiAdapterOrdinal = D3DMADAPTER_REGISTERED_DEVICE;
            }
            else
            {
                //
                // Use the default adapter
                //
                pCallerInstr->TestCaseArgs[0].pwchSoftwareDeviceFilename = NULL;
                pCallerInstr->TestCaseArgs[0].uiAdapterOrdinal = D3DMADAPTER_DEFAULT;
            }

            //
            // Set up device instance 1
            //
            pCallerInstr->bTestValid[1] = TRUE;
            StringCchCopy(pCallerInstr->TestCaseArgs[1].pwchSoftwareDeviceFilename, D3DQA_FILELENGTH,  D3DQA_D3DMREF_FILENAME);
            pCallerInstr->TestCaseArgs[1].uiAdapterOrdinal = D3DMADAPTER_REGISTERED_DEVICE;
            
        }

        //
        // If output frames are going to be generated, should they be kept or deleted?
        //
        if (pCallerInstr->bCompare)
        {
            if (CmdLineParse.GetOpt(_T("o")))
            {
                pCallerInstr->bKeepOutputFrames = TRUE;
            }
            else
            {
                pCallerInstr->bKeepOutputFrames = FALSE;
            }
        }

        //
        // Save/load directories; default has already been populated, and will remain if option is not found
        //
        CmdLineParse.GetOptString(_T("r"), pCallerInstr->TestCaseArgs[0].pwchTestDependenciesDir, MAX_PATH);
        CmdLineParse.GetOptString(_T("r"), pCallerInstr->TestCaseArgs[1].pwchTestDependenciesDir, MAX_PATH);
        CmdLineParse.GetOptString(_T("c"), pCallerInstr->TestCaseArgs[0].pwchImageComparitorDir, MAX_PATH);
        CmdLineParse.GetOptString(_T("c"), pCallerInstr->TestCaseArgs[1].pwchImageComparitorDir, MAX_PATH);


        //
        // Window extents
        //
        if (CmdLineParse.GetOpt(_T("e1")))
        {
            pWindowArgs->uiWindowWidth  = 64; 
            pWindowArgs->uiWindowHeight = 64; 
            pWindowArgs->bPParmsWindowed = TRUE;
        }
        else if (CmdLineParse.GetOpt(_T("e2")))
        {
            pWindowArgs->uiWindowWidth  = 220; 
            pWindowArgs->uiWindowHeight = 176; 
            pWindowArgs->bPParmsWindowed = TRUE;
        }
        else if (CmdLineParse.GetOpt(_T("e3")))
        {
            pWindowArgs->uiWindowWidth  = 176; 
            pWindowArgs->uiWindowHeight = 220; 
            pWindowArgs->bPParmsWindowed = TRUE;
        }
        else if (CmdLineParse.GetOpt(_T("e4")))
        {
            pWindowArgs->uiWindowWidth  = 320; 
            pWindowArgs->uiWindowHeight = 240; 
            pWindowArgs->bPParmsWindowed = TRUE;
        }
        else if (CmdLineParse.GetOpt(_T("e5")))
        {
            pWindowArgs->uiWindowWidth  = 240; 
            pWindowArgs->uiWindowHeight = 320; 
            pWindowArgs->bPParmsWindowed = TRUE;
        }
        else if (CmdLineParse.GetOpt(_T("e6")))
        {
            pWindowArgs->uiWindowWidth  = 640; 
            pWindowArgs->uiWindowHeight = 480; 
            pWindowArgs->bPParmsWindowed = TRUE;
        }
        else if (CmdLineParse.GetOpt(_T("e7")))
        {
            pWindowArgs->uiWindowWidth  = GetSystemMetrics(SM_CXSCREEN); 
            pWindowArgs->uiWindowHeight = GetSystemMetrics(SM_CYSCREEN); 
            pWindowArgs->bPParmsWindowed = FALSE;
        }
        else // and default
        {
            pWindowArgs->uiWindowWidth  = 64; 
            pWindowArgs->uiWindowHeight = 64; 
            pWindowArgs->bPParmsWindowed = TRUE;
        }


        //
        // Window styles
        //
        if (CmdLineParse.GetOpt(_T("w1")))
        {
            pWindowArgs->uiWindowStyle = WS_OVERLAPPED;
        }
        else if (CmdLineParse.GetOpt(_T("w2")))
        {
            pWindowArgs->uiWindowStyle = WS_POPUP;
        }
        else // default
        {
            pWindowArgs->uiWindowStyle = WS_OVERLAPPED;
        }

    }

    return true;
}

//
// TuxTest
// 
//   This TESTPROCAPI is the sole one for all test case ordinals in this binary.  Very little
//   test-specific code is contained herein.  This test merely branches to the correct
//   member function for the specific test ordinal.
//
// Arguments:
//
//   (As defined by tux interface)
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
TESTPROCAPI TuxTest(UINT uTuxMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    HRESULT hr;

    //
    // Result of call to single test case
    //
    INT TestResult;

    //
    // Result to return from this TESTPROCAPI
    //
    INT TuxResult = TPR_PASS;

    //
    // Instance handle
    //
    UINT uiInstanceHandle;

    //
    // Number of frames that the test case expects to generate (assuming a fully featured driver)
    //
    UINT uiNumFrames = 0;

    //
    // Identifies specific test case to be executed
    //
    ULONG ulTestOrdinal;

    //
    // Identifies information about each frame of a test (if there are more than one)
    //
    LPFRAMEDESCRIPTION pFrameDescriptors = NULL;

    if(uTuxMsg == TPM_QUERY_THREAD_COUNT)
    {
        //
        // Set default number of threads to 1
        //
        ((TPS_QUERY_THREAD_COUNT*)(tpParam))->dwThreadCount = 1;
        return TPR_HANDLED;
    }
    else if(uTuxMsg == TPM_EXECUTE)
    {
        if (NULL == lpFTE)
        {
            DebugOut(DO_ERROR, L"Invalid function table entry. Aborting.");
            return TPR_ABORT;
        }

        ulTestOrdinal = lpFTE->dwUniqueID;

        for (uiInstanceHandle = 0; uiInstanceHandle < 2; uiInstanceHandle++)
        {
            //
            // Within this block, don't return directly.  Set result and jump to cleanup,
            // for automatic deletion of stored bitmap files.
            //
            // Cleanup for this block specifically depends on having completed successful 
            // command-line parsing.  Thus, don't use cleanup from outside of this block.
            //

            //
            // Skip this iteration?
            //
            if (FALSE == g_CallerInstr.bTestValid[uiInstanceHandle])
                continue;

            DriverCompTest CompTest;

            if (FAILED(hr = CompTest.Init(&g_CallerInstr.TestCaseArgs[uiInstanceHandle], // LPCALLER_INSTRUCTIONS pCallerInstr
                                     &g_WindowArgs,                                 // LPWINDOW_ARGS pWindowArgs
                                     uiInstanceHandle,                            // UINT uiInstanceHandle
                                     ulTestOrdinal)))                             // UINT uiTestOrdinal
            {
                if (0 == uiInstanceHandle)
                {
                    DebugOut(DO_ERROR, L"Test object initialization failed. (hr = 0x%08x) Skipping.", hr);
                    TuxResult = TPR_SKIP;
                }
                else if (1 ==  uiInstanceHandle)
                {
                    DebugOut(L"Test object initialization failed for Reference image instance. (hr = 0x%08x) Aborting.", hr);
                    DebugOut(L"Without this instance, test case is invalid. Driver name: %s",
                        g_CallerInstr.TestCaseArgs[uiInstanceHandle].pwchSoftwareDeviceFilename);
                    DebugOut(L"In order to run these tests, either the reference driver (d3dmref.dll) must be available,");
                    DebugOut(L"or the /t2 command line parameter must be used with pregenerated reference images.");
                    DebugOut(L"Please verify that the reference driver is either in the flat release directory, the");
                    DebugOut(L"device's \\window directory, or in another location that is automatically checked by the");
                    DebugOut(DO_ERROR, L"DLL loader on your device.");
                    TuxResult = TPR_ABORT;
                }
                goto cleanup;
            }

            PostQuitMessage(0); // MessageLoop is not needed

            //
            // Pick a test class to instantiate; call a member therein
            //
            if ((ulTestOrdinal >= D3DMQA_CULLTEST_BASE) &&
                (ulTestOrdinal <= D3DMQA_CULLTEST_MAX))
            {

                //
                // Test simply requires a zero-based index to identify test case
                //
                TestResult = CompTest.ExecuteCullTest(ulTestOrdinal, &uiNumFrames, &pFrameDescriptors);
            }
            else if ((ulTestOrdinal >= D3DMQA_MIPMAPTEST_BASE) &&
                     (ulTestOrdinal <= D3DMQA_MIPMAPTEST_MAX))
            {
                //
                // Test simply requires a zero-based index to identify test case
                //
                TestResult = CompTest.ExecuteMipMapTest(ulTestOrdinal, &uiNumFrames, &pFrameDescriptors);
            }
            else if ((ulTestOrdinal >= D3DMQA_LIGHTTEST_BASE) &&
                     (ulTestOrdinal <= D3DMQA_LIGHTTEST_MAX))
            {
                //
                // Test simply requires a zero-based index to identify test case
                //
                TestResult = CompTest.ExecuteLightTest(ulTestOrdinal, &uiNumFrames, &pFrameDescriptors);
            }
            else if ((ulTestOrdinal >= D3DMQA_DEPTHBUFTEST_BASE) &&
                     (ulTestOrdinal <= D3DMQA_DEPTHBUFTEST_MAX))
            {
                //
                // Test simply requires a zero-based index to identify test case
                //
                TestResult = CompTest.ExecuteDepthBufferTest(ulTestOrdinal, &uiNumFrames, &pFrameDescriptors);
            }
            else if ((ulTestOrdinal >= D3DMQA_TRANSFORMTEST_BASE) &&
                     (ulTestOrdinal <= D3DMQA_TRANSFORMTEST_MAX))
            {
                //
                // Test simply requires a zero-based index to identify test case
                //
                TestResult = CompTest.ExecuteTransformTest(ulTestOrdinal, &uiNumFrames, &pFrameDescriptors);
            }
            else if ((ulTestOrdinal >= D3DMQA_PRIMRASTTEST_BASE) &&
                     (ulTestOrdinal <= D3DMQA_PRIMRASTTEST_MAX))
            {

                //
                // Test simply requires a zero-based index to identify test case
                //
                TestResult = CompTest.ExecutePrimRastTest(ulTestOrdinal, &uiNumFrames, &pFrameDescriptors);
            }
            else if ((ulTestOrdinal >= D3DMQA_CLIPPINGTEST_BASE) &&
                     (ulTestOrdinal <= D3DMQA_CLIPPINGTEST_MAX))
            {

                //
                // Test simply requires a zero-based index to identify test case
                //
                TestResult = CompTest.ExecuteClippingTest(ulTestOrdinal, &uiNumFrames, &pFrameDescriptors);
            }
            else if ((ulTestOrdinal >= D3DMQA_FVFTEST_BASE) &&
                     (ulTestOrdinal <= D3DMQA_FVFTEST_MAX))
            {
                //
                // Test simply requires a zero-based index to identify test case
                //
                TestResult = CompTest.ExecuteFVFTest(ulTestOrdinal, &uiNumFrames, &pFrameDescriptors);

            }
            else if ((ulTestOrdinal >= D3DMQA_FOGTEST_BASE) &&
                     (ulTestOrdinal <= D3DMQA_FOGTEST_MAX))
            {
                //
                // Test simply requires a zero-based index to identify test case
                //
                TestResult = CompTest.ExecuteFogTest(ulTestOrdinal, &uiNumFrames, &pFrameDescriptors);
            }
            else if ((ulTestOrdinal >= D3DMQA_STRETCHRECTTEST_BASE) &&
                     (ulTestOrdinal <= D3DMQA_STRETCHRECTTEST_MAX))
            {
                //
                // Test simply requires a zero-based index to identify test case
                //
                TestResult = CompTest.ExecuteStretchRectTest(ulTestOrdinal, &uiNumFrames, &pFrameDescriptors);
            }
            else if ((ulTestOrdinal >= D3DMQA_COPYRECTSTEST_BASE) &&
                     (ulTestOrdinal <= D3DMQA_COPYRECTSTEST_MAX))
            {
                //
                // Test simply requires a zero-based index to identify test case
                //
                TestResult = CompTest.ExecuteCopyRectsTest(ulTestOrdinal, &uiNumFrames, &pFrameDescriptors);
            }
            else if ((ulTestOrdinal >= D3DMQA_COLORFILLTEST_BASE) &&
                     (ulTestOrdinal <= D3DMQA_COLORFILLTEST_MAX))
            {
                //
                // Test simply requires a zero-based index to identify test case
                //
                TestResult = CompTest.ExecuteColorFillTest(ulTestOrdinal, &uiNumFrames, &pFrameDescriptors);
            }
            else if ((ulTestOrdinal >= D3DMQA_LASTPIXELTEST_BASE) &&
                     (ulTestOrdinal <= D3DMQA_LASTPIXELTEST_MAX))
            {
                //
                // Test simply requires a zero-based index to identify test case
                //
                TestResult = CompTest.ExecuteLastPixelTest(ulTestOrdinal, &uiNumFrames, &pFrameDescriptors);
            }
            else if ((ulTestOrdinal >= D3DMQA_TEXWRAPTEST_BASE) &&
                     (ulTestOrdinal <= D3DMQA_TEXWRAPTEST_MAX))
            {
                //
                // Test simply requires a zero-based index to identify test case
                //
                TestResult = CompTest.ExecuteTexWrapTest(ulTestOrdinal, &uiNumFrames, &pFrameDescriptors);
            }
            else if ((ulTestOrdinal >= D3DMQA_ALPHATESTTEST_BASE) &&
                     (ulTestOrdinal <= D3DMQA_ALPHATESTTEST_MAX))
            {
                //
                // Test simply requires a zero-based index to identify test case
                //
                TestResult = CompTest.ExecuteAlphaTestTest(ulTestOrdinal, &uiNumFrames, &pFrameDescriptors);
            }
            else if ((ulTestOrdinal >= D3DMQA_DEPTHBIASTEST_BASE) &&
                     (ulTestOrdinal <= D3DMQA_DEPTHBIASTEST_MAX))
            {
                //
                // Test simply requires a zero-based index to identify test case
                //
                TestResult = CompTest.ExecuteDepthBiasTest(ulTestOrdinal, &uiNumFrames, &pFrameDescriptors);
            }
            else if ((ulTestOrdinal >= D3DMQA_SWAPCHAINTEST_BASE) &&
                     (ulTestOrdinal <= D3DMQA_SWAPCHAINTEST_MAX))
            {
                //
                // Test simply requires a zero-based index to identify test case
                //
                TestResult = CompTest.ExecuteSwapChainTest(ulTestOrdinal, &uiNumFrames, &pFrameDescriptors);
            }
            else if ((ulTestOrdinal >= D3DMQA_OVERDRAWTEST_BASE) &&
                     (ulTestOrdinal <= D3DMQA_OVERDRAWTEST_MAX))
            {
                //
                // Test simply requires a zero-based index to identify test case
                //
                TestResult = CompTest.ExecuteOverDrawTest(ulTestOrdinal, &uiNumFrames, &pFrameDescriptors);
            }
            else if ((ulTestOrdinal >= D3DMQA_TEXSTAGETEST_BASE) &&
                     (ulTestOrdinal <= D3DMQA_TEXSTAGETEST_MAX))
            {
                //
                // Test simply requires a zero-based index to identify test case
                //
                TestResult = CompTest.ExecuteTexStageTest(ulTestOrdinal, &uiNumFrames, &pFrameDescriptors);
            }
            else if ((ulTestOrdinal >= D3DMQA_DXTNTEST_BASE) &&
                     (ulTestOrdinal <= D3DMQA_DXTNTEST_MAX))
            {
                //
                // Test simply requires a zero-based index to identify test case
                //
                TestResult = CompTest.ExecuteDXTnTest(ulTestOrdinal, &uiNumFrames, &pFrameDescriptors);
            }
            else if ((ulTestOrdinal >= D3DMQA_DXTNMIPMAPTEST_BASE) &&
                     (ulTestOrdinal <= D3DMQA_DXTNMIPMAPTEST_MAX))
            {
                //
                // Test simply requires a zero-based index to identify test case
                //
                TestResult = CompTest.ExecuteDXTnMipMapTest(ulTestOrdinal, &uiNumFrames, &pFrameDescriptors);
            }
            else if ((ulTestOrdinal >= D3DMQA_ADDTLSWAPCHAINSTEST_BASE) &&
                     (ulTestOrdinal <= D3DMQA_ADDTLSWAPCHAINSTEST_MAX))
            {
                //
                // Test simply requires a zero-based index to identify test case
                //
                TestResult = CompTest.ExecuteAddtlSwapChainsTest(ulTestOrdinal, &uiNumFrames, &pFrameDescriptors);
            }
            else if ((ulTestOrdinal >= D3DMQA_MULTIINSTTEST_BASE) &&
                     (ulTestOrdinal <= D3DMQA_MULTIINSTTEST_MAX))
            {
                //
                // Test simply requires a zero-based index to identify test case
                //
                TestResult = CompTest.ExecuteMultiInstTest(ulTestOrdinal, &uiNumFrames, &pFrameDescriptors, &g_CallerInstr.TestCaseArgs[uiInstanceHandle]);
            }
            else
            {
                DebugOut(DO_ERROR, L"Invalid test case ordinal. Failing");
                TuxResult = TPR_FAIL;
                goto cleanup;
            }

            //
            // Anything other than pass?  Return now, even if a comparison was intended based
            // on the next iteration's output.
            //
            if (TPR_PASS != TestResult)
            {

                if (1 == uiInstanceHandle)
                {
                    DebugOut(DO_ERROR, L"Resulting image set cannot be used as a reference set; it is incomplete.");
                }

                TuxResult = TestResult;
                goto cleanup;
            }

        }


        //
        // This code will only execute if all iterations above have resulted in TPR_PASS
        //
        if (g_CallerInstr.bCompare)
        {
            if (FAILED(CreateDeltas((WCHAR*)g_CallerInstr.TestCaseArgs[0].pwchImageComparitorDir, ulTestOrdinal, 0, 1, 2, 0, uiNumFrames-1, pFrameDescriptors)))
            {
                DebugOut(DO_ERROR, L"Unable to create difference bitmaps. Failing.");
                TuxResult = TPR_FAIL;
                goto cleanup;
            }

            if (FAILED(CheckImageDeltas((WCHAR*)g_CallerInstr.TestCaseArgs[0].pwchImageComparitorDir, ulTestOrdinal, 2, 0, uiNumFrames-1, pFrameDescriptors)))
            {
                DebugOut(DO_ERROR, L"Difference frame exceeds tolerance. Failing.");
                TuxResult = TPR_FAIL;
                goto cleanup;
            }
        }

        cleanup:

        //
        // Delete frames if no longer needed.  If something goes wrong during deletion, don't
        // change test result because of this.  Deletion is just "nice to have" cleanup.
        //


        //
        // If comparitor input frames were generated by instance zero, conditionally delete them.
        //
        if (TRUE == g_CallerInstr.bTestValid[0])
        {
            if (FALSE == g_CallerInstr.bKeepInputFrames[0])
                DeleteFiles(g_CallerInstr.TestCaseArgs[0].pwchImageComparitorDir, // WCHAR *wszPath,
                            0,            // UINT uiInstance,
                            ulTestOrdinal,// UINT uiTestID,
                            0,            // UINT uiBeginFrame,
                            uiNumFrames,  // UINT uiFrameCount
                            pFrameDescriptors);
        }

        //
        // If comparitor input frames were generated by instance one, conditionally delete them.
        //
        if (TRUE == g_CallerInstr.bTestValid[1])
        {
            if (FALSE == g_CallerInstr.bKeepInputFrames[1])
                DeleteFiles(g_CallerInstr.TestCaseArgs[1].pwchImageComparitorDir, // WCHAR *wszPath,
                            1,            // UINT uiInstance,
                            ulTestOrdinal,// UINT uiTestID,
                            0,            // UINT uiBeginFrame,
                            uiNumFrames,  // UINT uiFrameCount
                            pFrameDescriptors);
        }

        //
        // If comparitor output frames were generated; conditionally delete them.
        //
        if (g_CallerInstr.bCompare)
        {
            if (FALSE == g_CallerInstr.bKeepOutputFrames)
                DeleteFiles(g_CallerInstr.TestCaseArgs[0].pwchImageComparitorDir, // WCHAR *wszPath,
                            2,            // UINT uiInstance,
                            ulTestOrdinal,// UINT uiTestID,
                            0,            // UINT uiBeginFrame,
                            uiNumFrames,  // UINT uiFrameCount
                            pFrameDescriptors);
        }

        
        //
        // Free allocated memory (if any)
        //
        if (pFrameDescriptors)
        {
            delete pFrameDescriptors;
            pFrameDescriptors = NULL;
        }


        return TuxResult;

    }
    else
    {
        return TPR_NOT_HANDLED;
    }

}

