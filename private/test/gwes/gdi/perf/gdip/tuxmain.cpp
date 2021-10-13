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
#include "tuxmain.h"

// common TUX objects
HINSTANCE       g_hInstDll;
SPS_SHELL_INFO *g_pShellInfo;

// we're using two loggers, one for TUX output and the other for perf results
CKato    *g_pKato        = NULL;
COtak    *g_pCOtakResult = NULL;

// allow the user to request an XML formatted result log file
BOOL      g_fUseXmlLog   = FALSE;
TSTRING   g_tsXmlDest;

// Kato doesn't support verbosity, so implement this internally
DWORD g_dwVerbosity = (DWORD)VERBOSITY_DEFAULT;

// our functions table is dynamically generated on the fly, depending on the
// scriptfile passed in for parsing.
FUNCTION_TABLE_ENTRY *g_lpFTE = NULL;

// global parser used for reading the scriptfile and holds all of the test data
std::list<CSection *> g_SectionList;    

// simple DllMain entry point
BOOL WINAPI 
DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved)
{
    g_hInstDll = (HINSTANCE)hInstance;
    return TRUE;
}

// registers a common window class for use by the test cases which do drawing
// to an HWND on the Primary HDC.
BOOL
InitWindowClass(void)
{
    BOOL bRval = TRUE;

    WNDCLASS wc      = {0};
    wc.lpfnWndProc   = DefWindowProc;
    wc.lpszClassName = TEXT("GDI Performance");
    ATOM Atom        = RegisterClass(&wc);
    if (0x00 == Atom)
    {
        bRval = FALSE;
        info(ABORT, TEXT("RegisterClass failed with Error: %d."), GetLastError());
    }

    return bRval;
}

// shared TUX TestProc entry point
TESTPROCAPI GlobalTestExecutionProc_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // NO_MESSAGES
    if (uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    std::list<CTestSuite *> TestList;

    // search for a test case which matches the requested function table entry
    UINT nCount;
    std::list<CSection *>::iterator itr;
    for(itr=g_SectionList.begin(), nCount=0; itr!=g_SectionList.end(); itr++, nCount++)
    {
        // we're overloading the UserData field to identify which test case we want to execute
        if (nCount == lpFTE->dwUserData)
        {
            TSTRING tsName = (*itr)->GetName();
            CTestSuite * TestCase = NULL;

            // call the tests initialization function
            TestCase = CreateTestSuite(tsName, *itr);

            // add it to the list of tests to run.
            if(NULL != TestCase)
            {
                info(ECHO, TEXT("Execution queue added test case %s."), tsName.c_str());
                TestList.push_back(TestCase);

                // break here since our function table only adds one test case per section
                // header, but if we want to add more than one test case per TestProc, remove
                // this break and update the Tux function table generation to reuse dwUserData
                // when desired.
                break;
            }
            else
                info(ABORT, TEXT("Failed to add test case %s."), tsName.c_str());
        }
    }

    if (TestList.size() == 0)
        info(ABORT, TEXT("Failed to add test case identified %d."), lpFTE->dwUserData);
    else
    {
        // run the test suites.
        if(!RunTestSuite(&TestList))
            info(ABORT, TEXT("Test execution failed."));
    }

    // cleanup any partially allocated test's.
    CleanupTestSuite(&TestList);

    return getCode();
}

// dynamically generate our Tux function table based on our input script
BOOL
GenerateFunctionTable(TSTRING tsFileName)
{
    BOOL bRval = TRUE;
    std::list<CSection *>::iterator itr;

    // parse the script
    if (InitializeSectionList(tsFileName, &g_SectionList))
    {
        // dynamically generate a Tux function table based on the input script
        g_lpFTE = new FUNCTION_TABLE_ENTRY[g_SectionList.size() + 2];
        if (NULL == g_lpFTE)
        {
            bRval = FALSE;
            goto done;
        }

        // the first item is reserved the group header
        g_lpFTE[0].uDepth = 0;
        g_lpFTE[0].dwUserData = 0;
        g_lpFTE[0].dwUniqueID = 100;
        g_lpFTE[0].lpTestProc = NULL;
        g_lpFTE[0].lpDescription = TEXT("GDI Performance");

        UINT nItem = 0;
        for(itr = g_SectionList.begin(); itr != g_SectionList.end(); itr++)
        {
            // create each individual TESTPROC
            g_lpFTE[nItem+1].uDepth = 1;
            g_lpFTE[nItem+1].dwUserData = nItem;
            g_lpFTE[nItem+1].dwUniqueID = 100+nItem;
            g_lpFTE[nItem+1].lpTestProc = GlobalTestExecutionProc_T;
            g_lpFTE[nItem+1].lpDescription = new TCHAR[(*itr)->GetName().size()+1];

            if (NULL == g_lpFTE[nItem+1].lpDescription)
            {
                bRval = FALSE;
                goto done;
            }

            if (FAILED(StringCchCopy((LPTSTR)g_lpFTE[nItem+1].lpDescription, (*itr)->GetName().size()+1, (*itr)->GetName().c_str())))
            {
                bRval = FALSE;
                goto done;
            }

            ++nItem;
        }

        // empty TUX function table signals the end
        g_lpFTE[g_SectionList.size()+1].lpDescription = 0;
        g_lpFTE[g_SectionList.size()+1].uDepth = 0;
        g_lpFTE[g_SectionList.size()+1].dwUserData = 0;
        g_lpFTE[g_SectionList.size()+1].dwUniqueID = 0;
        g_lpFTE[g_SectionList.size()+1].lpTestProc = NULL;
    }

done:
    return bRval;
}

// perform all basic initialization here
BOOL
InitAll(void)
{
    BOOL fOk = TRUE;
    TSTRING tsFileName;
    
    // retrieve the path for our script
    if (!ParseCommandLine(g_pShellInfo->szDllCmdLine, &tsFileName))
    {
        fOk = FALSE;
        goto done;
    }

    // dynamically generate our Tux function table
    if (!GenerateFunctionTable(tsFileName))
    {
        fOk = FALSE;
        goto done;
    }

    // create our shared Window Class
    if (!InitWindowClass())
    {
        fOk = FALSE;
        goto done;
    }

    // initialize our benchmark engine
    CalibrateBenchmarkEngine();

done:
    return fOk;
}

// do the general cleanup 
void 
CleanUpAll(void)
{
    UnregisterClass(TEXT("GDI Performance"), NULL);
    
    // this can be NULL if we failed to generate our function table
    if (g_lpFTE)
    {
        FUNCTION_TABLE_ENTRY *pFTE = g_lpFTE+1;
        while (pFTE->lpDescription)
        {
            // cleanup function descriptions
            delete[] pFTE->lpDescription;
            pFTE->lpDescription = NULL;
            pFTE = pFTE+1;
        }

        delete[] g_lpFTE;
        g_lpFTE = NULL;
    }

    // whether or not Section List initialization succeeded, we need to clean up any allocations.
    FreeSectionList(&g_SectionList);
    
    if(g_pCOtakResult)
    {
        delete g_pCOtakResult;
        g_pCOtakResult = NULL;
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
        break;

    case SPM_UNLOAD_DLL:
        // Sent once to the DLL immediately before it is unloaded.

        // now is a good time to free our dynaimcally generated function table
        // and clean other objects
        CleanUpAll();
        break;

    case SPM_SHELL_INFO:
        // Sent once to the DLL immediately after SPM_LOAD_DLL to give the DLL
        // some useful information about its parent shell and environment. The
        // spParam parameter will contain a pointer to a SPS_SHELL_INFO
        // structure. The pointer to the structure may be stored for later use
        // as it will remain valid for the life of this Tux Dll. The DLL may
        // return SPR_FAIL to prevent the DLL from continuing to load.

        // Store a pointer to our shell info for later use.
        g_pShellInfo = (LPSPS_SHELL_INFO)spParam;
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

        // dynamically generate our function table
        if (!InitAll())
            return SPR_FAIL;

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
        break;

    case SPM_STOP_SCRIPT:
        // Sent to the DLL when the script has stopped. This message is sent
        // when the script reaches its end, or because the user pressed
        // stopped prior to the end of the script. This message is sent to
        // all Tux DLLs, including loaded Tux DLLs that are not in the script.
        break;

    case SPM_BEGIN_GROUP:
        // Sent to the DLL before a group of tests from that DLL is about to
        // be executed. This gives the DLL a time to initialize or allocate
        // data for the tests to follow. Only the DLL that is next to run
        // receives this message. The prior DLL, if any, will first receive
        // a SPM_END_GROUP message. For global initialization and
        // de-initialization, the DLL should probably use SPM_START_SCRIPT
        // and SPM_STOP_SCRIPT, or even SPM_LOAD_DLL and SPM_UNLOAD_DLL.
        g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: TUXTEST.DLL"));
        break;

    case SPM_END_GROUP:
        // Sent to the DLL after a group of tests from that DLL has completed
        // running. This gives the DLL a time to cleanup after it has been
        // run. This message does not mean that the DLL will not be called
        // again; it just means that the next test to run belongs to a
        // different DLL. SPM_BEGIN_GROUP and SPM_END_GROUP allow the DLL
        // to track when it is active and when it is not active.
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

        // Start our logging level.
        g_pKato->BeginLevel(
            ((LPSPS_BEGIN_TEST) spParam)->lpFTE->dwUniqueID,
            TEXT("BEGIN TEST: <%s>, Threads=%u, Seed=%u"),
            ((LPSPS_BEGIN_TEST) spParam)->lpFTE->lpDescription,
            ((LPSPS_BEGIN_TEST) spParam)->dwThreadCount,
            ((LPSPS_BEGIN_TEST) spParam)->dwRandomSeed);
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
        
        // End our logging level.
        g_pKato->EndLevel(
            TEXT("END TEST: <%s>"),
            ((LPSPS_END_TEST) spParam)->lpFTE->lpDescription);
        break;

    case SPM_EXCEPTION:
        // Sent to the DLL whenever code execution in the DLL causes and
        // exception fault. TUX traps all exceptions that occur while
        // executing code inside a test DLL.
        g_pKato->Log(MY_EXCEPTION, TEXT("Exception occurred!"));
        break;

    default:
        // Any messages that we haven't processed must, by default, cause us
        // to return SPR_NOT_HANDLED. This preserves compatibility with future
        // versions of the TUX shell protocol, even if new messages are added.
        return SPR_NOT_HANDLED;
    }

    return SPR_HANDLED;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Log Functions
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
void info(infoType iType, LPCTSTR szFormat, ...) {
    TCHAR szBuffer[1024];

    va_list pArgs;
    va_start(pArgs, szFormat);
    // _vsntprintf deprecated
    StringCbVPrintf(szBuffer, countof(szBuffer) - 1, szFormat, pArgs);
    va_end(pArgs);

    switch(iType) {
    case FAIL:
        if (g_dwVerbosity >= VERBOSITY_RESULTS)
            g_pKato->Log(MY_FAIL,TEXT("FAIL: %s"), szBuffer);
        break;
    case ECHO:
        if (g_dwVerbosity >= VERBOSITY_ECHO)
            g_pKato->Log(MY_PASS,szBuffer);
        break;
    case DETAIL:
        if (g_dwVerbosity >= VERBOSITY_ALL)
            g_pKato->Log(MY_PASS+1,TEXT("    %s"),szBuffer);
        break;
    case ABORT:
        if (g_dwVerbosity >= VERBOSITY_RESULTS)
            g_pKato->Log(MY_ABORT,TEXT("Abort: %s"),szBuffer);
        break;
    case SKIP:
        if (g_dwVerbosity >= VERBOSITY_RESULTS)
            g_pKato->Log(MY_SKIP,TEXT("Skip: %s"),szBuffer);
        break;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI getCode(void) {

    for(int i = 0; i < 15; i++)
        if (g_pKato->GetVerbosityCount((DWORD)i) > 0)
        switch(i) {
        case MY_EXCEPTION:
            return TPR_HANDLED;
        case MY_FAIL:
            return TPR_FAIL;
        case MY_ABORT:
            return TPR_ABORT;
        case MY_SKIP:
            return TPR_SKIP;
        case MY_NOT_IMPLEMENTED:
            return TPR_HANDLED;
        case MY_PASS:
            return TPR_PASS;
        default:
            break;
        }
    return TPR_PASS;
}

