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

#include "BTSPerfLog.h"

#undef __FILE_NAME__
#define __FILE_NAME__   _T("TUXMAIN.CPP")

#ifndef Debug
#define Debug NKDbgPrintfW
#endif



// callback for DebugOutput
void PerfDbgPrint(LPCTSTR str)
{
    // should use '%s' instead using 'str' directly, otherwise strings 
    // that contain '%' such as "%95 Filled ..." can't be printed correctly
    LOG(_T("%s"), str);
}

void LOG(LPCTSTR szFormat, ...)
{
    va_list va;

    if(g_pKato)
    {
        va_start(va, szFormat);
        g_pKato->LogV(LOG_DETAIL, szFormat, va);
        va_end(va);
    }
};

DWORD GetModulePath(HMODULE hMod, LPWSTR tszPath, DWORD dwCchSize)
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

// extern "C"
#ifdef __cplusplus
extern "C" {
#endif

// --------------------------------------------------------------------
BOOL WINAPI 
DllMain(
    HANDLE hInstance, 
    ULONG dwReason, 
    LPVOID lpReserved ) 
// --------------------------------------------------------------------    
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(lpReserved);

    switch( dwReason )
    {
    case DLL_PROCESS_ATTACH:         
        Debug(TEXT("%s: DLL_PROCESS_ATTACH\r\n"), __FILE_NAME__);           
        return TRUE;

    case DLL_PROCESS_DETACH:
        Debug(TEXT("%s: DLL_PROCESS_DETACH\r\n"), __FILE_NAME__);
        return TRUE;
    }
    return FALSE;
}

#ifdef __cplusplus
}
#endif
// end extern "C"

static void Initialize();

static void Usage()
{    
    Debug(TEXT(""));
    Debug(TEXT("DISKTEST_PERF: Usage: tux -o -d disktest_perf -c\"/disk <disk> /profile <profile> /maxsectors <count> /repeat <count> /sectors <n1>,<n2>,... /log <type> /logfile <file name> /BTSPerf /zorch \""));
    UsageCommon(NULL, g_pKato);
    Debug(TEXT("       /repeat <count>        : number of times to repeat each timed operation; default = %u"), DEF_NUM_ITER);
    Debug(TEXT("       /sectors <n1>,<n2>,... : number of sectors for each timed operation; default = %u"), DEF_NUM_SECTORS);
    Debug(TEXT("       /maxsectors <count>    : maximum number of sectors per operation; default = %u"), DEF_MAX_SECTORS);
    Debug(TEXT("       /BTSPerf               : Generate ceperf/perfscenario formatted output"));
    Debug(TEXT("       /log <type>            : 'dbg' - log to debug output"));
    Debug(TEXT("                                'csv' - log to .csv file"));
    Debug(TEXT("                                'perflog' - logging through perflog.dll"));
    Debug(TEXT("                                default = 'perflog'"));
    Debug(TEXT("       /logfile <file name>   : location of .csv file to log performance"));
    Debug(TEXT("       /zorch                 : Required to enable this test to run. With this parameter, the test will run and destroy all data on the specified storage device(s)."));
    Debug(TEXT(""));
}


// --------------------------------------------------------------------
SHELLPROCAPI 
ShellProc(
    UINT uMsg, 
    SPPARAM spParam ) 
// --------------------------------------------------------------------    
{
    LPSPS_BEGIN_TEST pBT = {0};
    LPSPS_END_TEST pET = {0};

    switch (uMsg)
    {    
        // --------------------------------------------------------------------
        // Message: SPM_LOAD_DLL
        //
        case SPM_LOAD_DLL: 
        Debug(_T("ShellProc(SPM_LOAD_DLL, ...) called"));

        // If we are UNICODE, then tell Tux this by setting the following flag.
        #ifdef UNICODE
            ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
        #endif

        // turn on kato debugging
        KatoDebug(1, KATO_MAX_VERBOSITY,KATO_MAX_VERBOSITY,KATO_MAX_LEVEL);

        // Get/Create our global logging object.
        g_pKato = (CKato*)KatoGetDefaultObject();

        // Initialize our global critical section.
        InitializeCriticalSection(&g_csProcess);

        Usage();

        return SPR_HANDLED;        

        // --------------------------------------------------------------------
        // Message: SPM_UNLOAD_DLL
        //
        case SPM_UNLOAD_DLL: 
        Debug(_T("ShellProc(SPM_UNLOAD_DLL, ...) called"));

        // This is a good place to delete our global critical section.
        DeleteCriticalSection(&g_csProcess);

        return SPR_HANDLED;      

        // --------------------------------------------------------------------
        // Message: SPM_SHELL_INFO
        //
        case SPM_SHELL_INFO:
        Debug(_T("ShellProc(SPM_SHELL_INFO, ...) called"));

        // Store a pointer to our shell info for later use.
        g_pShellInfo = (LPSPS_SHELL_INFO)spParam;

            Initialize();
        
        return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_REGISTER
        //
        case SPM_REGISTER:
        Debug(_T("ShellProc(SPM_REGISTER, ...) called"));            

        ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
        #ifdef UNICODE
            return SPR_HANDLED | SPF_UNICODE;
        #else
            return SPR_HANDLED;
        #endif

        // --------------------------------------------------------------------
        // Message: SPM_START_SCRIPT
        //
        case SPM_START_SCRIPT:
        Debug(_T("ShellProc(SPM_START_SCRIPT, ...) called")); 

        if(INVALID_HANDLE(g_hDisk))
        {
            g_pKato->Log(LOG_FAIL, _T("FAIL: unable to open Hard Disk media"));
        }

        return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_STOP_SCRIPT
        //
        case SPM_STOP_SCRIPT:

        if (g_pPerfLog)
        {
            delete g_pPerfLog;
        }
         
        if(VALID_HANDLE(g_hDisk))
        {
            g_pKato->Log(LOG_DETAIL, _T("Closing disk handle..."));
            CloseHandle(g_hDisk);
        }
        return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_BEGIN_GROUP
        //
        case SPM_BEGIN_GROUP:
        Debug(_T("ShellProc(SPM_BEGIN_GROUP, ...) called"));
        g_pKato->BeginLevel(0, _T("BEGIN GROUP: DISKTEST_PERF.DLL"));

        return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_END_GROUP
        //
        case SPM_END_GROUP:
        Debug(_T("ShellProc(SPM_END_GROUP, ...) called"));
        g_pKato->EndLevel(_T("END GROUP: DISKTEST_PERF.DLL"));

        return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_BEGIN_TEST
        //
        case SPM_BEGIN_TEST:
        Debug(_T("ShellProc(SPM_BEGIN_TEST, ...) called"));

              Calibrate();

               // Start our logging level.
               pBT = (LPSPS_BEGIN_TEST)spParam;
               g_pKato->BeginLevel(pBT->lpFTE->dwUniqueID, 
                                   _T("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
                                   pBT->lpFTE->lpDescription, pBT->dwThreadCount,
                                   pBT->dwRandomSeed);

        return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_END_TEST
        //
        case SPM_END_TEST:
        Debug(_T("ShellProc(SPM_END_TEST, ...) called"));

        // End our logging level.
        pET = (LPSPS_END_TEST)spParam;
        g_pKato->EndLevel(_T("END TEST: \"%s\", %s, Time=%u.%03u"),
                              pET->lpFTE->lpDescription,
                              pET->dwResult == TPR_SKIP ? _T("SKIPPED") :
                              pET->dwResult == TPR_PASS ? _T("PASSED") :
                              pET->dwResult == TPR_FAIL ? _T("FAILED") : _T("ABORTED"),
                              pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000);

        return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_EXCEPTION
        //
        case SPM_EXCEPTION:
        Debug(_T("ShellProc(SPM_EXCEPTION, ...) called"));
        g_pKato->Log(LOG_EXCEPTION, _T("Exception occurred!"));
        return SPR_HANDLED;

        default:
        Debug(_T("ShellProc received bad message: 0x%X"), uMsg);
        return SPR_NOT_HANDLED;
    }
}

static void
Initialize(void)
{
    WCHAR szBuffer[MAX_PATH];
    CClParse cmdLine(g_pShellInfo->szDllCmdLine);
    HRESULT hr = E_FAIL;
    ProcessCmdLineCommon(g_pShellInfo->szDllCmdLine, g_pKato);
    // Required for the PerfScenario library

    if (!cmdLine.GetOptVal(_T("maxsectors"), &g_dwMaxSectorsPerOp))
    {
        g_dwMaxSectorsPerOp = DEF_MAX_SECTORS;
    }

    if (!cmdLine.GetOptVal(_T("repeat"), &g_dwPerfIterations))
    {
        g_dwPerfIterations = DEF_NUM_ITER;
    }
    
    if (cmdLine.GetOptString(_T("sectors"), szBuffer, countof(szBuffer)))
    {
        LPCTSTR sept = _T(" ,");
        TCHAR * nextToken = NULL;
        LPCTSTR token = _tcstok_s(szBuffer, sept, &nextToken);
        DWORD num = 0, i = 0;
        while (NULL != token)
        {
            num = _ttoi(token);
            if (num != 0) 
            {
                g_sectorList[i++] = num;
            }
            token = _tcstok_s(NULL, sept, &nextToken);
        }
        g_sectorListCounts = i;
    }
    else
    {
        // default test with 128 sectors
        g_sectorList[0] = DEF_NUM_SECTORS;
        g_sectorListCounts = 1;
    }
    
    if (OpenDevice())
    {
        // select logging method
        if (cmdLine.GetOptString(_T("log"), szBuffer, countof(szBuffer)))
        {
            if (_tcsnicmp(szBuffer, _T("dbg"), 3) == 0)
            {
                g_pPerfLog = new DebugOutput(PerfDbgPrint);
                LOG(_T("Logging to Debug Output"));

                if (g_pPerfLog == NULL) 
                {
                    LOG(_T("*** OOM creating DebugOutput object ***"));
                }
                g_logMethod = LOG_DBG;
            }
            else if (_tcsnicmp(szBuffer, _T("csv"), 3) == 0)
            {
                // get wirte .csv log file name             
                if (cmdLine.GetOptString(_T("logfile"), szBuffer, countof(szBuffer))) 
                {
                    hr = StringCchCopy(g_csvFilename, countof(g_csvFilename), szBuffer);
                    if (FAILED(hr))
                    {
                        g_csvFilename[0] = _T('\0');
                    }
                }
                if (g_csvFilename[0] == _T('\0'))
                {
                    if (GetModulePath(GetModuleHandle(NULL), g_csvFilename, countof(g_csvFilename)) > 0)
                    {
                        HRESULT hr = StringCchCat(g_csvFilename, countof(g_csvFilename), _T("Disktest_Perf.csv"));
                        if (FAILED(hr))
                        {
                            g_csvFilename[0] = _T('\0');
                        }
                    }                  
                }
                if (g_csvFilename[0] != _T('\0'))
                {                
                    g_pPerfLog = new CsvFile(g_csvFilename, 0);
                    LOG(_T("(Write) Logging to [CSV file %s]"), g_csvFilename); 
                }

                if (g_pPerfLog == NULL) 
                {
                    LOG(_T("*** OOM creating CsvFile object ***"));
                }
                g_logMethod = LOG_CSV;
            }       
        }

        if (cmdLine.GetOpt(_T("BTSPerf")))
        {
            g_logMethod = LOG_BTS;            // Set the flag to output BTS Perf metrics
        }

        // set default logging
        if (g_pPerfLog == NULL && g_logMethod != LOG_BTS)
        {
            LOG(_T("Logging to CE Perflog"));
            g_pPerfLog = new CEPerfLog();
            g_logMethod = LOG_PERFLOG;
        }

        if (g_pPerfLog != NULL) 
        {
            g_pPerfLog->PerfSetTestName(_T("Disktest_perf"));
        }
       }

    g_pKato->Log(LOG_DETAIL, _T("DISKTEST_PERF: Max Sectors per operation = %u"), g_dwMaxSectorsPerOp);
}
