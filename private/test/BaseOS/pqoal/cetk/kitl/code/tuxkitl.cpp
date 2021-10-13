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
/**********************************  Headers  *********************************/

#include <windows.h>
#include <tchar.h>
#include <tux.h>

/* common utils */
#include "..\..\..\common\commonUtils.h"

#include "clparse.h"

#include "tuxKitl.h"


////////////////////////////////////////////////////////////////////////////////
/***************  Constants and Defines Local to This File  *******************/

// Dummy DLL to load.  This can be set to anything.  Currently
// we use an empty dll compiled specifically for this purpose
#define LIBRARY_TO_LOAD (_T("oalTestDllForKitlTest"))


// Number of seconds between printing messages during the
// KITL_LOAD_UNLOAD_DLL_TEST
#define PRINT_EVERY_N_SECONDS (60)


// default run time in seconds
#define DEFAULT_RUN_TIME_S (10 * 60)


// We calibrate how long the test runs by running it until a threshold time is
// reached.  We then extrapolate how long a real run would take.  This is the
// calibration threshold in ms.  If the clocks won't work this won't work.
//
// This is assumed to be a dword.  Make sure that the multiplication
// won't overflow.

#define TEST_CALIBRATION_THRESHOLD_MS (10000)



////////////////////////////////////////////////////////////////////////////////
/************************ Helper Functions  ***********************************/

// This function loads and unloads a given DLL the specified number of times
// and it also prints a message after every N load/unload operations.
BOOL loadUnloadDll (DWORD dwNumOfLoadUnloads, DWORD printMsgEveryNTimes);

// This function dumps debug spew until the specified number of output
// messages is reached
BOOL dumpOutput (DWORD dwNumOutputMsgs);



////////////////////////////////////////////////////////////////////////////////
/********************************* TEST PROCS *********************************/

/*******************************************************************************
 *
 *                                Usage Message
 *
 ******************************************************************************/
/*
 * Prints out the usage message for the Kitl tests. It tells the user
 * what the tests do and also specifies the input that the user needs to
 * provide to run the tests.
 */

TESTPROCAPI
KitlTestsUsageMessage(
       UINT uMsg,
       TPPARAM tpParam,
       LPFUNCTION_TABLE_ENTRY lpFTE)
{
    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return (TPR_NOT_HANDLED);
    }

    Info (_T("These tests aim to stress kitl. Errors/hangs seen here "));
    Info (_T("most likely suggest either errors in the kitl routines or"));
    Info (_T("errors in the interrupt routines. These tests generate a lot of"));
    Info (_T("debug spew."));
    Info (_T(""));
    Info (_T("The tests require no arguments, but acceptable arguments are:"));
    Info (_T("-runTimeS <seconds>   Run time in seconds for each test."));
    Info (_T("-CalibThresh <milliseconds>  Calibration Threshold in milliseconds"));
    Info (_T("    The test must run a quick calibration to determine how many iterations are needed"));
    Info (_T("    to run for the full test time.  The default length of the calibration test is 10 seconds,"));
    Info (_T("    but the length of the calibration test can be changed with the –CalibThresh <time in ms>"));
    Info (_T("    option on the command line.  -CalibThresh should never be set to 0."));
    Info (_T(""));
    Info (_T("If no run time is specified, the default run time is 10 minutes."));
    Info (_T(""));

    return (TPR_PASS);
}


/*******************************************************************************
 *
 *                             KITL STRESS TESTS
 *
 ******************************************************************************/
/*
 * These tests aim to stress Kitl. Depending on the input one of the tests is
 * run. The tests are run for the given time, be it from the command line or
 * the default run time.
 * The KITL_LOAD_UNLOAD_DLL_TEST loads and unloads a given dll.
 * The KITL_DUMP_DEBUG_OUTPUT_TEST dumps debug spew. This test generates a
 * lot of debug spew.
 */

TESTPROCAPI testKitlStress(
              UINT uMsg,
              TPPARAM tpParam,
              LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_FAIL;

    // Local return value
    BOOL rVal = FALSE;
    DWORD dwCalibThresh = TEST_CALIBRATION_THRESHOLD_MS;
    // Get possible command line value
    CClParse cmdLine(g_szDllCmdLine);

    // Only supporting executing the test
    if (uMsg != TPM_EXECUTE)
    {
        returnVal =  TPR_NOT_HANDLED;
        goto cleanAndReturn;
    }

    breakIfUserDesires ();

    // Display a description of what the test does to the user
    Info (_T("")); //Blank line
    switch (lpFTE->dwUserData)
    {
    case KITL_LOAD_UNLOAD_DLL_TEST:
        Info (_T("This test loads and unloads a DLL for the time specified."));
        Info (_T("It prints out the number of load/unload operations remaining"));
        Info (_T("once per minute."));
        break;
    case KITL_DUMP_DEBUG_OUTPUT_TEST:
        Info (_T("This function dumps debug spew for the time specified."));
        Info (_T("The debug spew can be viewed in PB debug window."));
        break;
    default:
        Error (_T("Test function table is incorrect."));
        goto cleanAndReturn;
    }

    Info (_T(""));  //Blank line

    // Get the Run time for the test in seconds from the command line
    DWORD dwRunTimeS = 0;

    if (cmdLine.GetOptVal (_T("runTimeS"), &dwRunTimeS))
    {
        Info (_T("Got run time of %u s from the command line."), dwRunTimeS);
    }
    else
    {
        // If no option specified by the user or couldn't read it, use default
        dwRunTimeS = DEFAULT_RUN_TIME_S;
        Info (_T("No valid Run time specified by the user."));
        Info (_T("Using default run time of %u s."), dwRunTimeS);
    }

    if (cmdLine.GetOptVal (_T("CalibThresh"), &dwCalibThresh))
    {
        Info (_T("Got CalibThresh of %u ms from the command line."), dwCalibThresh);
    }
    else
    {
        // If no option specified by the user or couldn't read it, use default
        dwCalibThresh = TEST_CALIBRATION_THRESHOLD_MS;
        Info (_T("No valid CalibThresh specified by the user."));
        Info (_T("Using default CalibThresh of %u ms."), dwCalibThresh);
    }

    // Check to see if the Run time is greater than the calibration threshold.
    if (dwRunTimeS*1000 < dwCalibThresh)
    {
        Error (_T("Run time must be greater than the calibration time of %u ms."),
           dwCalibThresh);
        goto cleanAndReturn;
    }

    // Print the run time in realistic units so the user can understand
    double doTime;
    TCHAR * szUnits;
    realisticTime ((double) dwRunTimeS, doTime, &szUnits);

    Info (_T("The test will run for %g %s."), doTime, szUnits);


    // Now calculate the number of iterations required for the given run time.
    // For this, run the test with different number of iterations until the time
    // taken to run them exceeds the specified threshold. Now using these
    // numIterations and the time taken to run them, calculate the numIterations
    // for the required run time.
    Info (_T(""));
    Info (_T("Calibrating the number of iterations required for the given "));
    Info (_T("run time."));
    Info (_T(""));

    DWORD dwTestCalibrationRuns = 1;   // start with one iteration
    ULONGLONG ullTestCalibrationRunTimeMS = 0;

    // Get the number of iterations such that the time taken to run them exceeds
    // the threshold time
    for(;;)
    {
        ULONGLONG ullTimeBegin, ullTimeEnd;

        // Get the start time from GTC
        ullTimeBegin = (ULONGLONG)GetTickCount();
        if(!ullTimeBegin)
        {
            Error (_T("Couldn't get the start time from GTC."));
            goto cleanAndReturn;
        }

        // Run the given test with dwTestCalibrationRuns
        rVal = FALSE;

        switch (lpFTE->dwUserData)
        {
        case KITL_LOAD_UNLOAD_DLL_TEST:
            rVal = loadUnloadDll (dwTestCalibrationRuns, 0);
            break;
        case KITL_DUMP_DEBUG_OUTPUT_TEST:
            rVal = dumpOutput (dwTestCalibrationRuns);
            break;
        default:
            Error (_T("Test function table is incorrect"));
            break;
        }

        if (!rVal)
        {
            Error (_T("Test failed during calibration."));
            goto cleanAndReturn;
        }

        // Now get the end time from GTC
        ullTimeEnd = (ULONGLONG)GetTickCount();
        if(!ullTimeEnd)
        {
            Error (_T("Couldn't get end time from GTC"));
            goto cleanAndReturn;
        }

        // Subtract, accounting for overflow
        if (!AminusB (ullTestCalibrationRunTimeMS,
           ullTimeEnd, ullTimeBegin, DWORD_MAX))
        {
            IntErr (_T("AminusB failed in testInterruptKitl."));
            Error (_T("Could not calculate the time taken."));
            goto cleanAndReturn;
        }

        // Check if ullDiff time has crossed the threshold time, if yes we're done.
        // If not we double the number of Iterations and run again.
        if (ullTestCalibrationRunTimeMS < dwCalibThresh)
        {
            // If within threshold double number of runs and try again
            DWORD prev = dwTestCalibrationRuns;

            dwTestCalibrationRuns *= 2;

            if (prev >= dwTestCalibrationRuns)
            {
                Error (_T("Could not calibrate.  We overflowed the variable "));
                Error (_T("storing the number of calibration runs.  Either the "));
                Error (_T("test routines are running very fast or the "));
                Error (_T("calibration threshold is set too high."));
                Error (_T("Reduce the calibration threshold using the"));
                Error (_T("-CalibThresh command line option."));
                goto cleanAndReturn;
            }
        }
        else
        {
            Info (_T(""));
            Info (_T("Done calibrating."));
            Info (_T(""));
            break;
        }
    } // end while


    // If dwTestCalibrationRuns iterations ran in ullTestCalibrationRunTimeMS time,
    // then how many iterations for dwRunTimeS time?

    // Now calculate time taken for one iteration.
    double doRunPerS =
        ((double) dwTestCalibrationRuns / (double) ullTestCalibrationRunTimeMS) * 1000.0;

    // Number of iterations that can be run in the given time is:
    DWORD dwNumIterations = DWORD ((double) dwRunTimeS * doRunPerS);

    // If running the KITL_TEST_DUMP_OUTPUT, calculate the frequency of debug
    // lines to be printed
    DWORD dwPrintEveryN = DWORD (doRunPerS * (double) PRINT_EVERY_N_SECONDS);

    Info (_T("The number of runs for this test are going to be %u."),
                                                          dwNumIterations);

    // Now run the tests for the calibrated number of iterations
    rVal = FALSE;

    Info (_T("Now starting the test..."));
    Info (_T(""));

    switch (lpFTE->dwUserData)
    {
        case KITL_LOAD_UNLOAD_DLL_TEST:
            rVal = loadUnloadDll (dwNumIterations, dwPrintEveryN);
            break;
        case KITL_DUMP_DEBUG_OUTPUT_TEST:
            rVal = dumpOutput (dwNumIterations);
            break;
        default:
            Error (_T("Test function table is incorrect."));
            break;
    }

    Info (_T("")); //Blank line

    if (!rVal)
    {
        Info (_T("Test failed."));
        goto cleanAndReturn;
    }

    Info (_T("Test passed."));

    returnVal = TPR_PASS;

cleanAndReturn:

    return (returnVal);

}

HRESULT IsDebuggerActive(BOOL *pfDebuggerActive)
{
        HRESULT hr = E_FAIL;
        DWORD fDebugger;
        DWORD dwBytesReturned;

        if (!pfDebuggerActive)
        {
        return E_INVALIDARG;
        }

        if (KernelLibIoControl ((HANDLE) KMOD_CORE,
        IOCTL_KLIB_ISKDPRESENT,
        NULL,
        0,
        &fDebugger,
        sizeof (fDebugger),
        &dwBytesReturned))
        {
        *pfDebuggerActive = fDebugger;
        hr = S_OK;
        }

        return hr;
}

/*******************************************************************************
 *                   KITL_LOAD_UNLOAD_DLL_TEST
 ******************************************************************************/

// This function loads and unloads a given DLL the specified number of times
// and it also prints a message after every N load/unload operations
//
// Arguments:
//     Input: Number of load/unload sets
//                Print frquency with respect to number of load/unload operations
//                         (If 0, no messages are printed.)
//         Output: None
//
// Return value:
//      TRUE if successful, FALSE otherwise

BOOL loadUnloadDll (DWORD dwNumOfLoadUnloads, DWORD printMsgEveryNTimes)
{
    BOOL returnVal = FALSE;
    HRESULT hr = E_FAIL;
    BOOL fIsDebuggerActive = FALSE;

    hr = IsDebuggerActive(&fIsDebuggerActive);
    if (FAILED(hr))
    {
        Error (_T("Failed to detect debugger status"));
        goto cleanAndReturn;
    }

    if (!fIsDebuggerActive)
    {
        Error (_T("Debugger is not attached.  Please attach a debugger and re-run this test"));
        goto cleanAndReturn;
    }

    for (; dwNumOfLoadUnloads != 0; dwNumOfLoadUnloads--)
    {
        HINSTANCE hLib;

        hLib = LoadLibrary (LIBRARY_TO_LOAD);
        if(!hLib)
        {
            Error (_T("Couldn't load lib %s.  GetLastError is %u."),
              LIBRARY_TO_LOAD, GetLastError());
            goto cleanAndReturn;
        }

        if (!FreeLibrary(hLib))
        {
            Error (_T("Couldn't free lib %s.  GetLastError is %u."),
                LIBRARY_TO_LOAD, GetLastError());
            goto cleanAndReturn;
        }

        // If printMsgEveryNTimes isn't 0 and dwNumOfLoadUnloads is on a multiple
        // of printMsgEveryNTimes, then print message.
        if (printMsgEveryNTimes && (dwNumOfLoadUnloads % printMsgEveryNTimes == 0))
        {
            Info (_T("There are %u load/unload sets to go."), dwNumOfLoadUnloads);
        }
    }

    returnVal = TRUE;

cleanAndReturn:

    return (returnVal);

}


/*******************************************************************************
 *                         KITL_DUMP_DEBUG_OUTPUT_TEST
 ******************************************************************************/

// This function dumps debug spew until the specified number of output
// messages is reached
//
// Arguments:
//      Input: Number of ouput messages to be printed
//             Output: None
//
// Return value:
//      TRUE if successful, FALSE otherwise

BOOL dumpOutput (DWORD dwNumOutputMsgs)
{
    for (; dwNumOutputMsgs != 0; dwNumOutputMsgs--)
    {
        //Sleep for 1ms to introduce a delay or we will have too much debug output
        Sleep(1);

        // Now print the debug output.
        Debug (_T("TEST LINE: The KITL dump debug output test is running."));
    }

    return (TRUE);
}

