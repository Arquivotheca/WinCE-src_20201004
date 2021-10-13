//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <tchar.h>
#include <katoutils.h>
#include <clparse.h>   // Command-line parsing utility
#include "ft.h"

#include "TestWindow.h"
#include "Initializer.h"
#include "DriverVerif.h"
#include "DebugOutput.h"

//Avoid C4100
#define UNUSED_PARAM(_a) _a

//
// Any cmd-line argument value string that exceeds 100 characters
// will be truncated.
//
#define D3DQA_MAX_OPT_LEN 100

CKato *g_pKato;


//
// Storage for test case argument string field:
//
//      * TEST_CASE_ARGS.pwchSoftwareDeviceFilename
//
static WCHAR g_pwchSoftwareDeviceFilename[MAX_PATH]; // Unused in case of D3DMADAPTER_DEFAULT ( pwchSoftwareDeviceFilename

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
	UNUSED_PARAM(hInstance);
	UNUSED_PARAM(dwReason);
	UNUSED_PARAM(lpReserved);

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
        DebugOut(TEXT("ShellProc(SPM_LOAD_DLL, ...) called\n"));

#ifdef UNICODE
        ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
#endif // UNICODE
		g_pKato = (CKato*)KatoGetDefaultObject();
        break;

    case SPM_UNLOAD_DLL:
        // Sent once to the DLL immediately before it is unloaded.
        DebugOut(TEXT("ShellProc(SPM_UNLOAD_DLL, ...) called\n"));
        break;

    case SPM_SHELL_INFO:
        // This message is sent once to the DLL immediately following the
		// SPM_LOAD_DLL message to give the DLL information about its parent shell,
		// via SPS_SHELL_INFO.
        DebugOut(TEXT("ShellProc(SPM_SHELL_INFO, ...) called\n"));

        // Store a pointer to our shell info, in case it is useful later on
        g_pShellInfo = (LPSPS_SHELL_INFO)spParam;
        break;

    case SPM_REGISTER:
        // This message is sent to query the DLL for its function table. 
        DebugOut(TEXT("ShellProc(SPM_REGISTER, ...) called\n"));
        ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;

#ifdef UNICODE
        return SPR_HANDLED | SPF_UNICODE;
#else // UNICODE
        return SPR_HANDLED;
#endif // UNICODE

    case SPM_START_SCRIPT:
        // This message is sent to the DLL immediately before a script is started
        DebugOut(TEXT("ShellProc(SPM_START_SCRIPT, ...) called\n"));
        break;

    case SPM_STOP_SCRIPT:
        // This message is sent to the DLL after the script has stopped.
        DebugOut(TEXT("ShellProc(SPM_STOP_SCRIPT, ...) called\n"));
        break;

    case SPM_BEGIN_GROUP:
        // Sent to the DLL before a group of tests from that DLL is about to
        // be executed. This gives the DLL a time to initialize or allocate
        // data for the tests to follow.
        DebugOut(TEXT("ShellProc(SPM_BEGIN_GROUP, ...) called\n"));
        g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: TUXTEST.DLL"));
		break;

    case SPM_END_GROUP:
        // Sent to the DLL after a group of tests from that DLL has completed
        // running. This gives the DLL a time to cleanup after it has been
        // run. This message does not mean that the DLL will not be called
        // again; it just means that the next test to run belongs to a
        // different DLL. SPM_BEGIN_GROUP and SPM_END_GROUP allow the DLL
        // to track when it is active and when it is not active.
        DebugOut(TEXT("ShellProc(SPM_END_GROUP, ...) called\n"));
        g_pKato->EndLevel(TEXT("END GROUP: TUXTEST.DLL"));
		break;

    case SPM_BEGIN_TEST:
        // This message is sent to the DLL before a single test or group of
		// tests from that DLL is executed. 
       
		DebugOut(TEXT("ShellProc(SPM_BEGIN_TEST, ...) called\n"));
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
		
        DebugOut(TEXT("ShellProc(SPM_END_TEST, ...) called\n"));
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
        DebugOut(TEXT("ShellProc(SPM_EXCEPTION, ...) called\n"));
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

//
// ParseCommonCmdLineArgs
//
//   Parse cmd-line args that are consistent for all tests.  Args that are specific
//   to one particular test case are handled in the specific TESTPROCAPI that defines
//   that case.
//
// Arguments:
//  
//
// Return Value:
//
//  bool:  false indicates that execution should be aborted (for example, if usage
//         was requested), true indicates that execution should continue
//
bool ParseCommonCmdLineArgs(LPTEST_CASE_ARGS pTestCaseArgs)
{
	//
	// Temporary storage for use with cmd-line parser
	//
	WCHAR wszArg[D3DQA_MAX_OPT_LEN];

	//
	// Arg validation
	//
	if (NULL == pTestCaseArgs)
	{
		DebugOut(_T("ParseCommonCmdLineArgs aborting due to invalid arguments.\n"));
		return false;
	}

	//
	// Command-line parsing
	//
	if ((NULL == g_pShellInfo) || (NULL == g_pShellInfo->szDllCmdLine))
	{
		DebugOut(_T("No command-line arguments specified.  Using defaults.\n"));
	}
	else
	{
		CClParse CmdLineParse(g_pShellInfo->szDllCmdLine);

		//
		// If the "-?" command-line argument exists, show the usage information,
		// and skip the test.
		//
		if (CmdLineParse.GetOpt(_T("-?")))
		{
			g_pKato->Log(LOG_COMMENT,_T("\n"));
			g_pKato->Log(LOG_COMMENT,_T("Description:\n"));
			g_pKato->Log(LOG_COMMENT,_T("\tDriver verification tests that do not depend on reference driver comparison.\n"));
			g_pKato->Log(LOG_COMMENT,_T("\t\n"));
			g_pKato->Log(LOG_COMMENT,_T("Sample command line:\n"));
			g_pKato->Log(LOG_COMMENT,_T("\ttux -d D3DM_DriverVerif.dll\n"));
			g_pKato->Log(LOG_COMMENT,_T("\t\n"));
			g_pKato->Log(LOG_COMMENT,_T("\t\t/s: DLL filename of software device to register (if present test loads the DLL, calls RegisterSoftwareDevice,\n"));
			g_pKato->Log(LOG_COMMENT,_T("\t\t     and creates the device with D3DMADAPTER_REGISTERED_DEVICE; otherwise, the test uses the system's default\n"));
			g_pKato->Log(LOG_COMMENT,_T("\t\t     adapter by creating the device with D3DMADAPTER_DEFAULT).\n"));
			g_pKato->Log(LOG_COMMENT,_T("\t\n"));

			//
			// Abort execution; user merely wanted to see the usage information
			//
			return false;
		}

		//
		// Register a software device, or use default adapter?
		//
		if (CmdLineParse.GetOptString(_T("s"), wszArg, D3DQA_MAX_OPT_LEN))
		{
			wcscpy(pTestCaseArgs->pwchSoftwareDeviceFilename, wszArg);
			pTestCaseArgs->uiAdapterOrdinal = D3DMADAPTER_REGISTERED_DEVICE;
		}
		else // default adapter
		{
			pTestCaseArgs->pwchSoftwareDeviceFilename = NULL;
			pTestCaseArgs->uiAdapterOrdinal = D3DMADAPTER_DEFAULT;
		}

		if (CmdLineParse.GetOptString(_T("d"), wszArg, D3DQA_MAX_OPT_LEN))
		{
		    if (1 != swscanf(wszArg, L"%f", &pTestCaseArgs->fTestTolerance))
		    {
		        g_pKato->Log(LOG_COMMENT,_T("Error parsing /d option."));
		        pTestCaseArgs->fTestTolerance = D3DQA_DEFAULT_MAX_DELTA;
		    }
		    else
		    {
		        g_pKato->Log(LOG_COMMENT,_T("Using non-default tolerance %f"),pTestCaseArgs->fTestTolerance);
		    }
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
	ULONG ulTestOrdinal;             // Identifies specific test case to be executed

	//
	// To be updated based on command-line arguments
	//
	TEST_CASE_ARGS TestCaseArgs =
	{
		0,                            // UINT uiAdapterOrdinal;
		g_pwchSoftwareDeviceFilename, // PWCHAR pwchSoftwareDeviceFilename;		
		NULL,                         // PWCHAR pwchImageComparitorDir;
		NULL,                         // PWCHAR pwchTestDependenciesDir;	
		D3DQA_DEFAULT_MAX_DELTA,      // FLOAT fTestTolerance;
	};

	//
	// This test does not have any particular requirements
	// regarding window configuration/extents
	//
	WINDOW_ARGS WindowArgs = 
	{
		50,             // UINT uiWindowWidth;
		50,             // UINT uiWindowHeight;		
		WS_OVERLAPPED,  // DWORD uiWindowStyle;
		g_lpClassName,  // LPCTSTR lpClassName;
		g_lpWindowName, // LPCTSTR lpWindowName;
		TRUE            // BOOL bPParmsWindowed;
	};


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
			DebugOut(_T("Invalid function table entry; aborting.\n"));
			return TPR_ABORT;
		}

		ulTestOrdinal = lpFTE->dwUniqueID;

		if (false == ParseCommonCmdLineArgs(&TestCaseArgs))
		{
			return TPR_SKIP;  // Cmd-line-arg parser requested a halt of test execution
                }

		if (D3DQA_DEFAULT_MAX_DELTA != TestCaseArgs.fTestTolerance)
		{
		    DebugOut(_T("***** Warning: Do not set the Tolerances flag without first contacting a Microsoft representative *****"));
		}

		DriverVerifTest VerifTest;
		
		if (FAILED(VerifTest.Init(&TestCaseArgs,    // LPTEST_CASE_ARGS pTestCaseArgs
		                          &WindowArgs,      // LPWINDOW_ARGS pWindowArgs
		                           ulTestOrdinal))) // UINT uiTestOrdinal
		{
			DebugOut(_T("Test object initialization failed.\n"));
			return TPR_SKIP;
		}


		// MessageLoop is not needed
		PostQuitMessage(0);

		//
		// Individual cases do not need to print out PASS/FAIL/SKIP
		// debug spew here; more granular debug spew has already been
		// provided within the specific test case's code.
		//

		//
		// Pick a test set
		//
		if ((ulTestOrdinal >= D3DMQA_BLENDTEST_BASE) &&
		    (ulTestOrdinal <= D3DMQA_BLENDTEST_MAX))
		{

			//
			// Test simply requires a zero-based index to identify test case
			//
			return VerifTest.ExecuteBlendTest(ulTestOrdinal);
		}

		if ((ulTestOrdinal >= D3DMQA_STENCILTEST_BASE) &&
		    (ulTestOrdinal <= D3DMQA_STENCILTEST_MAX))
		{

			//
			// Test simply requires a zero-based index to identify test case
			//
			return VerifTest.ExecuteStencilTest(ulTestOrdinal);
		}

		if ((ulTestOrdinal >= D3DMQA_RESMANTEST_BASE) &&
		    (ulTestOrdinal <= D3DMQA_RESMANTEST_MAX))
		{

			//
			// Test simply requires a zero-based index to identify test case
			//
			return VerifTest.ExecuteResManTest(ulTestOrdinal);
		}

		if ((ulTestOrdinal >= D3DMQA_PROCVERTSTEST_BASE) &&
		    (ulTestOrdinal <= D3DMQA_PROCVERTSTEST_MAX))
		{
			switch(ulTestOrdinal)
			{
			case D3DQAID_PV_NOSTREAMSOURCE:
				return VerifTest.ExecuteNoStreamSourceTest();
				break;
			case D3DQAID_PV_STREAMSOURCE:
				return VerifTest.ExecuteStreamSourceTest();
				break;
			case D3DQAID_PV_DONOTCOPYDATA:
				return VerifTest.ExecuteDoNotCopyDataTest();
				break;
			case D3DQAID_PV_NOTENOUGHVERTS:
				return VerifTest.ExecuteNotEnoughVertsTest();
				break;
			case D3DQAID_PV_MULTTEST:                
				return VerifTest.ExecuteMultTest();
				break;
			case D3DQAID_PV_ADDTEST:                 
				return VerifTest.ExecuteAddTest();
				break;
			case D3DQAID_PV_TEXTRANS:                
				return VerifTest.ExecuteTexTransRotate();
				break;
			case D3DQAID_PV_PERSPROJ:                
				return VerifTest.ExecutePerspectiveProjTest();
				break;
			case D3DQAID_PV_ORTHPROJ:                
				return VerifTest.ExecuteOrthoProjTest();
				break;
			case D3DQAID_PV_VIEWPORT:                
				return VerifTest.ExecuteViewportTest();
				break;
			case D3DQAID_PV_DIFFUSE_MATERIAL_SOURCE: 
				return VerifTest.ExecuteDiffuseMaterialSourceTest();
				break;
			case D3DQAID_PV_SPECULAR_MATERIAL_SOURCE:
				return VerifTest.ExecuteSpecularMaterialSourceTest();
				break;
			case D3DQAID_PV_SPECULAR_POWER:          
				return VerifTest.ExecuteSpecularPowerTest();
				break;
			case D3DQAID_PV_HALFWAYVEC:              
				return VerifTest.ExecuteHalfwayVectorTest();
				break;
			case D3DQAID_PV_NONLOCALVIEWER:          
				return VerifTest.ExecuteNonLocalViewerTest();
				break;
			case D3DQAID_PV_GLOBALAMBIENT:           
				return VerifTest.ExecuteGlobalAmbientTest();
				break;
			case D3DQAID_PV_LOCALAMBIENT:            
				return VerifTest.ExecuteLocalAmbientTest();
				break;

			default:
				DebugOut(_T("Invalid test case ordinal.\n"));
				return TPR_FAIL;
				break;
			}
		}
		else 
		{
			DebugOut(_T("Invalid test case ordinal.\n"));
			return TPR_FAIL;
		}
	}
	else
	{
	    return TPR_NOT_HANDLED;
	}
}

