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
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------


// ***************************************************************************************************************
//
// These functions expose MemoryPerf in a Tux interface for automated testing
//
// ***************************************************************************************************************


// see MemoryPerfTux.cpp for instructions

#include "tuxmain.h"
#include "globals.h"
#include "Latency.h"
#include <tlhelp32.h>
#include <psapi.h>

#pragma warning( disable : 25003 )      // prefast is overly aggressive about wanting pointers to be const which already are so marked

#define __FILE_NAME__   TEXT("TUXMAIN.CPP")

#define MEMORY_PERF TEXT("SYSTEM\\MemoryPerf")

LPVOID pCePerfInternal; // Instantiate required CePerf global
static GUID MemoryPerfGUID = { 0x76ec4798, 0x3f50, 0x47ef, { 0x98, 0xc0, 0xef, 0xb3, 0xcb, 0xe0, 0xc2, 0x51 } };
HANDLE  g_hPerfSession=INVALID_HANDLE_VALUE; //CEPerf session

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
CKato *g_pKato = NULL;

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO *g_pShellInfo;

// Perf log
WCHAR g_szPerfFilename[MAX_PATH];

//global data struct
tGlobal gtGlobal = {0};

static void ProcessCmdLine( LPCTSTR );


// --------------------------------------------------------------------
// Prints out usage
// --------------------------------------------------------------------

void Usage()
{
    LogPrintf("s tux -o -d MemoryPerf.dll -c\"[-c][-f][-l ddd][-o][-p][-r][-s ddd]\" -f\"release\\memoryperf.log\"");
    LogPrintf("-c outputs details to memoryperf.csv");
    LogPrintf("-f fills all allocated buffers with random data before the test");
    LogPrintf("-h Warm Up CPU");
    LogPrintf("-l ddd sets the priority level for the tests to ddd (default 152, consider 0 if power management is disabled)");
    LogPrintf("-o prints Overview latency info to the csv file");
    LogPrintf("-p prints additional explanation and diagnostic info to the csv file");
    LogPrintf("-s ddd sets sleep time at priority changes to ddd ms (default is 1, consider 1 if power management is disabled)");
    LogPrintf("-w toggles warnings which are enabled by default.");
    LogPrintf("-2 disables testing for L2 (normally enabled).\r\n");
    LogPrintf("Reminder:  before running MemoryPerf.dll, in your FRD, copy ceperf_module.dll ceperf.dll");
}


// --------------------------------------------------------------------
// Initialize perf logging functionality
// --------------------------------------------------------------------

BOOL InitPerfLog()
{
    BOOL fRet = FALSE;
    HRESULT hr = S_OK;

    // Activate PerfScenario recording
    hr = PerfScenarioOpenSession(MEMORY_PERF, TRUE);
    if (S_OK != hr)
    {
        g_pKato->Log(LOG_FAIL, _T("Error : InitPerfLog() : PerfScenarioOpenSession() failed with error %x"), hr);
        goto done;
    }

    // Initialize PerfScenario session data
    g_hPerfSession = INVALID_HANDLE_VALUE;
    hr = MDPGPerfOpenSession(&g_hPerfSession, MEMORY_PERF);
    if (S_OK != hr)
    {
        g_pKato->Log(LOG_FAIL, _T("Error : InitPerfLog() : MDPGPerfOpenSession() failed with error %x"), hr);
        goto done;  
    }
    fRet = TRUE;
done:
    return fRet;
 }


// --------------------------------------------------------------------
// Initializes perf handles
// --------------------------------------------------------------------

BOOL PerfReport(LPCTSTR pszPerfMarkerName, DWORD value)
{
    BOOL fRet = FALSE;
    HRESULT hr = S_OK;
    HANDLE perfTracker = {0};
    
    assert( _tcslen( pszPerfMarkerName ) < 32 );

    hr = CePerfRegisterTrackedItem(g_hPerfSession,
        &perfTracker,
        CEPERF_TYPE_STATISTIC,
        pszPerfMarkerName,
        CEPERF_STATISTIC_RECORD_SHORT,
        NULL);
    if (S_OK != hr)
    {
        g_pKato->Log(LOG_FAIL, _T("Error : InitPerfTracker() : CePerfRegisterTrackedItem() failed with error %x"), hr);
        goto done;
    }
    
    hr = CePerfAddStatistic(perfTracker, value, NULL);
    if (S_OK != hr)
    {
        g_pKato->Log(LOG_FAIL, _T("Error : LogStatistic : CeAddStatistics() failed with error %x"), hr);
        goto done;
    }
    CePerfDeregisterTrackedItem(perfTracker);

    fRet = TRUE;
done:
    if (!fRet)
    {
        g_pKato->Log(LOG_FAIL,_T("Error creating the perf report for %s"), pszPerfMarkerName);
    }
    // now keep a record of our perf reports in the log output as well.
    if ( _tcsclen(pszPerfMarkerName)>3 && 0==_tcsncicmp( pszPerfMarkerName+_tcsclen(pszPerfMarkerName)-3, _T(" ps"), 3 ) )
    {   // reports in pico seconds should be logged as nano seconds.
        LogPrintf( "%6.2f ns Perf Report %S\r\n", 0.001f*value, pszPerfMarkerName );
    }
    else
    {   // otherwise just report the value
        LogPrintf( "%6d Perf Report %S\r\n", value, pszPerfMarkerName );
    }

    return fRet;
}


// --------------------------------------------------------------------
// Create Perf log
// --------------------------------------------------------------------

void CreatePerfLog()
{
    MODULEINFO mi = {0};
    

    memset(&mi, 0, sizeof(MODULEINFO));
    if(!CeGetModuleInfo(
                (HANDLE)GetCurrentProcessId(), 
                NULL, 
                MINFO_MODULE_INFO, 
                &mi, 
                sizeof(MODULEINFO)
                ) ||
        !CeGetModuleInfo(
                (HANDLE)GetCurrentProcessId(), 
                mi.lpBaseOfDll, 
                MINFO_FULL_PATH, 
                g_szPerfFilename, 
                sizeof(g_szPerfFilename)
                ))
    {
         g_pKato->Log(LOG_FAIL,_T("Error retrieving location to output performance results. Results will not be written to XML"));
    }
    else
    {
            // Construct full path to the XML file that stores PerfScenario results
            WCHAR szFile[] = L"\\MemoryPerf_test.xml";
            WCHAR chDelimiter = L'\\';

            // Replace '\\tux.exe' with '\\MemoryPerf_text.xml': reverse find '\\' and change to NULL
            LPWSTR lpDelimiter = (LPWSTR) wcsrchr(g_szPerfFilename, chDelimiter);
            *lpDelimiter = L'\0';

            // Concatenate '\\MemoryPerf_test.xml'
            if((MAX_PATH - (wcslen((const wchar_t *)g_szPerfFilename) + 1)) < wcslen((const wchar_t *)szFile))
            {
                g_pKato->Log(LOG_FAIL,_T("Error constructing path to store performance results, filename exceeds MAX_PATH. Results will not be written to XML"));
            }
            else
            {
                wcsncat_s(g_szPerfFilename, MAX_PATH, szFile, wcslen((const wchar_t *)szFile));
            }
    }
}


// --------------------------------------------------------------------
// Initializes global test parameters
// --------------------------------------------------------------------

BOOL Initialize()
{
    BOOL fRet = FALSE;

    memset( &gtGlobal, 0, sizeof(gtGlobal) );

    gtGlobal.bSectionMapTest = false;
    gtGlobal.bFillFirst = false;
    gtGlobal.bUncachedTest = true;
    gtGlobal.bVideoTest = true;
    gtGlobal.bChartDetails = false;
    gtGlobal.bChartOverview = false;
    gtGlobal.bChartOverviewDetails = false;
    gtGlobal.bWarmUpCPU = true;

#ifdef VERIFY_OPTIMIZED
    gtGlobal.bFillFirst = true;        // the buffers need to be initialized to do the verification
#endif

    MemPerfMiscInit();

    // Parse the command line
    ProcessCmdLine(g_pShellInfo->szDllCmdLine);

    if ( gtGlobal.bChartDetails || gtGlobal.bChartOverview )
    {
        if ( !bCSVOpen() )
        {
            gtGlobal.bChartDetails = false;
            gtGlobal.bChartOverview = false;
        }
    }


    if (!InitPerfLog())
    {
        goto done;
    }

    CreatePerfLog();

    fRet = TRUE;
done:
    return fRet;
}


// --------------------------------------------------------------------
// Cleanup and housekeeping
// --------------------------------------------------------------------

BOOL Shutdown()
{
    BOOL fRet = FALSE;

    g_pKato->Log(LOG_DETAIL, _T("Info : Shutdown() : Attempting to flush CEPerf data to log file %s"),
        g_szPerfFilename);

    CePerfFlushSession(g_hPerfSession,
        NULL,
        CEPERF_FLUSH_DATA,
        0);
    
    CePerfCloseSession(g_hPerfSession);

    if (S_OK != PerfScenarioFlushMetrics(
        FALSE,
        &MemoryPerfGUID,
        TEXT("OAL NAMESPACE"),
        TEXT("Memory Performance"),
        g_szPerfFilename,
        NULL,
        NULL))
    {
        g_pKato->Log(LOG_FAIL, _T("Error : Shutdown() : PerfScenarioFlushMetrics() failed"));
        goto done;
    }

    if (S_OK != PerfScenarioCloseSession(MEMORY_PERF))
    {
        g_pKato->Log(LOG_FAIL, _T("Error : Shutdown() : PerfScenarioCloseSession() failed"));
        goto done;
    }

    MemPerfClose();

    fRet = TRUE;
done:
    return fRet;
}


// --------------------------------------------------------------------
// TUX message handler
// --------------------------------------------------------------------

SHELLPROCAPI ShellProc( UINT uMsg, SPPARAM spParam) 
{

    switch (uMsg) {

        case SPM_LOAD_DLL: 
        {
            // If we are UNICODE, then tell Tux this by setting the following flag.
            #ifdef UNICODE
                ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
            #endif

            // Get/Create our global logging object.
            g_pKato = (CKato*)KatoGetDefaultObject(); 
            return SPR_HANDLED;
        }

        case SPM_UNLOAD_DLL: 
        {               
            Shutdown();
            return SPR_HANDLED;
        }

        case SPM_SHELL_INFO: 
        {        
         
            // Store a pointer to our shell info for later use.
            g_pShellInfo = (LPSPS_SHELL_INFO)spParam;

            if( g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine )
            {
                // Display our Dlls command line if we have one.
                g_pKato->Log( LOG_DETAIL,_T("Command Line: \"%s\"."), g_pShellInfo->szDllCmdLine);
            } 
            Usage();

            // Break if the initialization fails
            if (!Initialize())
            {
                return SPR_FAIL;
            }
            break;
        }

        case SPM_REGISTER: 
        {
             ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
            
            #ifdef UNICODE
                return SPR_HANDLED | SPF_UNICODE;
            #else
                return SPR_HANDLED;
            #endif
        }

        case SPM_START_SCRIPT: 
        {           
            return SPR_HANDLED;
        }

        case SPM_STOP_SCRIPT: 
        {
            return SPR_HANDLED;
        }

        case SPM_BEGIN_GROUP: 
        {
            g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: MemoryPerf.DLL"));                      
     
            return SPR_HANDLED;
        }

        case SPM_END_GROUP: 
        {
            

            g_pKato->EndLevel(TEXT("END GROUP: MemoryPerf.DLL"));            

            return SPR_HANDLED;
        }

        case SPM_BEGIN_TEST: 
        {
            // Start our logging level.
            const LPSPS_BEGIN_TEST pBT = (const LPSPS_BEGIN_TEST)spParam;
            g_pKato->BeginLevel(pBT->lpFTE->dwUniqueID, 
                                TEXT("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
                                pBT->lpFTE->lpDescription, pBT->dwThreadCount,
                                pBT->dwRandomSeed);
            return SPR_HANDLED;
        }
        case SPM_END_TEST: 
        {
            // End our logging level.
            const LPSPS_END_TEST pET = (const LPSPS_END_TEST)spParam;
            g_pKato->EndLevel(TEXT("END TEST: \"%s\", %s, Time=%u.%03u"),
                            pET->lpFTE->lpDescription,
                            pET->dwResult == TPR_SKIP ? TEXT("SKIPPED") :
                            pET->dwResult == TPR_PASS ? TEXT("PASSED") :
                            pET->dwResult == TPR_FAIL ? TEXT("FAILED") : TEXT("ABORTED"),
                            pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000);    

             
            return SPR_HANDLED;
        }

        case SPM_EXCEPTION: 
        {
            g_pKato->Log(LOG_EXCEPTION, TEXT("Exception occurred!"));
            return SPR_HANDLED;
        }
    }

    return SPR_NOT_HANDLED;

}


//-----------------------------------------------------------------------------
// parse the command line intended for this test -c"..."
//-----------------------------------------------------------------------------

static void ProcessCmdLine( LPCTSTR szCmdLine )
{
    CClParse cmdLine(szCmdLine);
    const int len = 10;
    TCHAR szArg[ len + 1 ]; //Maximum of DWORD has 10 digits

    if(cmdLine.GetOpt(_T("2")))
    {
        gtGlobal.bNoL2 = true;
    }

    if(cmdLine.GetOpt(_T("c")))
    {
        gtGlobal.bChartDetails = true;
    }
        
    if(cmdLine.GetOpt(_T("f")))
    {
        gtGlobal.bFillFirst = !gtGlobal.bFillFirst;
    }
    if(cmdLine.GetOpt(_T("h")))
    {
        gtGlobal.bWarmUpCPU = !gtGlobal.bWarmUpCPU;
    }
    if(cmdLine.GetOpt(_T("o")))
    {
        gtGlobal.bChartOverview = true;
    }
    if(cmdLine.GetOpt(_T("p")))
    {
        gtGlobal.bChartDetails = true;
        gtGlobal.bExtraPrints = true;
    }
    if (cmdLine.GetOptString(_T("l"),szArg, len))
    {
        szArg[ len ] = 0;   // null terminate just in case.
        int iArg = _wtol(szArg);
        if ( 0 <= iArg && iArg <= 255 )
        {
            gtGlobal.giCEPriorityLevel = iArg;
        }
    }

    if (cmdLine.GetOptString(_T("s"),szArg, len))
    {
        szArg[ len ] = 0;   // null terminate just in case.
        int iArg = _wtol(szArg);
        if ( 0 <= iArg && iArg <= 2000 )
        {
            gtGlobal.giRescheduleSleep = iArg;
        }
    }
    if(cmdLine.GetOpt(_T("w")))
    {
        gtGlobal.bWarnings = !gtGlobal.bWarnings;
    }
    LogPrintf( "Options Csv=%d, Debug=%d, Fill=%d, priorityLevel=%d, Print=%d, Sleep=%d, Warnings=%d\r\n",
                gtGlobal.bChartDetails, 
                gtGlobal.bConsole2, 
                gtGlobal.bFillFirst, 
                gtGlobal.giCEPriorityLevel, 
                gtGlobal.bExtraPrints, 
                gtGlobal.giRescheduleSleep,
                gtGlobal.bWarnings 
              );
}





