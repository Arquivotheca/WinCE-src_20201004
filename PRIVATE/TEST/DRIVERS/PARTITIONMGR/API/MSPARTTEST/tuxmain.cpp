//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "TuxMain.h"
#include "TPartDrv.h"
#include "TestProc.h"

// debugging
#define Debug NKDbgPrintfW

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
CKato *g_pKato = NULL;

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO *g_pShellInfo;

// Global critical section to be used by threaded tests if necessary.
CRITICAL_SECTION g_csProcess;

// name of the partition driver to load
TCHAR g_szDriverName[MAX_PATH] = DEF_DRIVERNAME;

#define BASE 4000

// extern "C"
#ifdef __cplusplus
extern "C" {
#endif

// --------------------------------------------------------------------
// Tux testproc function table
//
FUNCTION_TABLE_ENTRY g_lpFTE[] = {

    _T("Partition Driver BVT/API"                               ),      0,      0,          0,  NULL,
    _T( "Partition Driver Store API"                            ),      1,      0,       5001,  Tst_StoreBVT,
    _T( "Partition Driver Partition API"                        ),      1,      0,       5002,  Tst_PartBVT,

    _T("Partition Driver Depth Tests"                           ),      0,      0,          0,  NULL,
    _T( "Create, enumerate, & delete alternating partitions"    ),      1,      0,       5101,  Tst_MultiPart,
    _T( "Create and rename partitions"                          ),      1,      0,       5102,  Tst_PartRename,
    _T( "R/W and bounds check disk partition data"              ),      1,      0,       5103,  Tst_ReadWrite,
    _T( "Verify disk info IOCTL"                                ),      1,      0,       5104,  Tst_DiskInfo,
    _T( "Verify PD_STOREINFO info gets updated properly"        ),      1,      0,       5105,  Tst_InfoUpdate,
    _T( "Test partition boundaries for corruption"              ),      1,      0,       5106,  Tst_PartitionBounds,

    NULL,   0,  0,  0,  NULL
};
// --------------------------------------------------------------------

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
            Debug(TEXT("%s: DLL_PROCESS_ATTACH\r\n"), MODULE_NAME);    
            return TRUE;

        case DLL_PROCESS_DETACH:
            Debug(TEXT("%s: DLL_PROCESS_DETACH\r\n"), MODULE_NAME);
            return TRUE;
    }
    return FALSE;
}

#ifdef __cplusplus
} // end extern "C"
#endif

static void ProcessCmdLine( LPCTSTR );

// --------------------------------------------------------------------
void LOG(LPWSTR szFmt, ...)
// --------------------------------------------------------------------
{
    va_list va;

    if(g_pKato)
    {
        va_start(va, szFmt);
        g_pKato->LogV( LOG_DETAIL, szFmt, va);
        va_end(va);
    }
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

    switch (uMsg) {
    
        // --------------------------------------------------------------------
        // Message: SPM_LOAD_DLL
        //
        case SPM_LOAD_DLL: 
            Debug(_T("ShellProc(SPM_LOAD_DLL, ...) called\r\n"));

            Debug(_T("!!! Warning                                                   !!!\r\n"));
            Debug(_T("!!! This test will erase data on all mounted storage devices  !!!\r\n"));
            Debug(_T("!!!                                                           !!!\r\n"));
            // If we are UNICODE, then tell Tux this by setting the following flag.
            #ifdef UNICODE
                ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
            #endif

            // turn on kato debugging
            //KatoDebug(1, KATO_MAX_VERBOSITY,KATO_MAX_VERBOSITY,KATO_MAX_LEVEL);

            // Get/Create our global logging object.
            g_pKato = (CKato*)KatoGetDefaultObject();

            // Initialize our global critical section.
            InitializeCriticalSection(&g_csProcess);

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
            Debug(_T("ShellProc(SPM_SHELL_INFO, ...) called\r\n"));
      
            // Store a pointer to our shell info for later use.
            g_pShellInfo = (LPSPS_SHELL_INFO)spParam;
        
            if( g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine )
            {
                // Display our Dlls command line if we have one.
                g_pKato->Log( LOG_DETAIL, 
                    _T("Command Line: \"%s\"."), g_pShellInfo->szDllCmdLine);

                ProcessCmdLine(g_pShellInfo->szDllCmdLine);
            } 

        return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_REGISTER
        //
        case SPM_REGISTER:
            Debug(_T("ShellProc(SPM_REGISTER, ...) called\r\n"));
            
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
            Debug(_T("ShellProc(SPM_START_SCRIPT, ...) called\r\n"));           
            if(!LoadPartitionDriver(g_szDriverName))
            {
                Debug(_T("Unable to load partition driver\r\n"));
            }
            else
            {
                Debug(_T("Loaded partition driver %s\r\n"), g_szDriverName);
            }
            return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_STOP_SCRIPT
        //
        case SPM_STOP_SCRIPT:
            FreePartitionDriver();
            return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_BEGIN_GROUP
        //
        case SPM_BEGIN_GROUP:
            Debug(_T("ShellProc(SPM_BEGIN_GROUP, ...) called\r\n"));
            g_pKato->BeginLevel(0, _T("BEGIN GROUP: FSDTEST.DLL"));
            
            return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_END_GROUP
        //
        case SPM_END_GROUP:
            Debug(_T("ShellProc(SPM_END_GROUP, ...) called\r\n"));
            g_pKato->EndLevel(_T("END GROUP: TUXDEMO.DLL"));
            
            return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_BEGIN_TEST
        //
        case SPM_BEGIN_TEST:
            Debug(_T("ShellProc(SPM_BEGIN_TEST, ...) called\r\n"));

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
            Debug(_T("ShellProc(SPM_END_TEST, ...) called\r\n"));

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
            Debug(_T("ShellProc(SPM_EXCEPTION, ...) called\r\n"));
            g_pKato->Log(LOG_EXCEPTION, _T("Exception occurred!"));
            return SPR_HANDLED;

        default:
            Debug(_T("ShellProc received bad message: 0x%X\r\n"), uMsg);
            ASSERT(!"Default case reached in ShellProc!");
            return SPR_NOT_HANDLED;
    }
}

const DWORD MAX_OPT_ARG = 256;
const TCHAR END_OF_STRING = (TCHAR)NULL;
const TCHAR BAD_ARG = _T('?');
const TCHAR ARG_FLAG = _T('-');
const TCHAR OPT_ARG = _T(':');

TCHAR optArg[MAX_OPT_ARG];

// --------------------------------------------------------------------
INT
WinMainGetOpt(
    LPCTSTR szCmdLine,
    LPCTSTR szOptions )
// --------------------------------------------------------------------    
{
    static LPCTSTR pCurPos = szCmdLine;   
    LPCTSTR pCurOpt = szOptions;
    LPTSTR pOptArg = optArg;
    UINT cOptArg = 0;
    INT option = 0;
    TCHAR quoteChar = TCHAR(' ');

    // end of string reached, or NULL string
    if( NULL == pCurPos || END_OF_STRING == *pCurPos )
    {
        return EOF;
    }

    // eat leading white space
    while( _T(' ') == *pCurPos )
        pCurPos++;

    // trailing white space
    if( END_OF_STRING == *pCurPos )
    {
       return EOF;
    }

    // found an option
    if( ARG_FLAG != *pCurPos )
    {
        return BAD_ARG;
    }
    
    pCurPos++;

    // found - at end of string
    if( END_OF_STRING == *pCurPos )
    {
        return BAD_ARG;
    }

    // search all options requested
    for( pCurOpt = szOptions; *pCurOpt != END_OF_STRING; pCurOpt++ )
    {
        // found matching option
        if( *pCurOpt == *pCurPos )
        {
            option = (int)*pCurPos;
            
            pCurOpt++;
            pCurPos++;
            if( OPT_ARG == *pCurOpt )
            {
                // next argument is the arg value
                // look for something between quotes or whitespace                
                while( _T(' ') == *pCurPos )
                    pCurPos++;

                if( END_OF_STRING == *pCurPos )
                {
                    return BAD_ARG;
                }

                if( _T('\"') == *pCurPos )
                {
                    // arg value is between quotes
                    quoteChar = *pCurPos;
                    pCurPos++;
                }
                
                else if( ARG_FLAG == *pCurPos )
                {
                    return BAD_ARG;
                }

                else                
                {
                    // arg end at next whitespace
                    quoteChar = _T(' ');
                }
                
                pOptArg = optArg;
                cOptArg = 0;
                
                while( quoteChar != *pCurPos && END_OF_STRING != *pCurPos  && cOptArg < MAX_OPT_ARG )
                {
                    *pOptArg++ = *pCurPos++;      
                    cOptArg++;
                }

                // missing final quote
                if( _T(' ') != quoteChar && quoteChar != *pCurPos )
                {
                    return BAD_ARG;
                }
                
                // append NULL char to output string
                *pOptArg = END_OF_STRING;

                // if there was no argument value, fail
                if( 0 == cOptArg )
                {
                    return BAD_ARG;
                }   
            }  

            return option;            
        }
    }
    pCurPos++;
    // did not find a macthing option in list
    return BAD_ARG;
}

static
void
ProcessCmdLine( 
    LPCTSTR szCmdLine )
{
    // parse command line
    int option = 0;
    LPCTSTR szOpt = _T("d:n:s:v:");

    Debug(_T("%s: Usage: tux -o -d msparttest [-v drivername]\r\n"), MODULE_NAME);
    
    while(EOF != (option = WinMainGetOpt(szCmdLine, szOpt)))
    {
        switch(option)
        {
        case L'v':
            wcsncpy(g_szDriverName, optArg, MAX_PATH);
            Debug(_T("Driver Name: %s\r\n"), g_szDriverName);
            break;

#ifdef USE_TOOLS
        case L'd': // disk name (DSKx:)
            wcsncpy(g_szDskName, optArg, MAX_PATH);
            if(!wcschr(g_szDskName, L':'))
                wcscat(g_szDskName, L":");
            Debug(_T("Disk: %s\r\n"), g_szDskName);
            break;
            
        case L'n': // partition name, for create partition util
            wcsncpy(g_szPartName, optArg, MAX_PATH);
            Debug(_T("Partition Name: %s\r\n"), g_szPartName);
            break;

        case L's': // parition size, for create partition util
            g_snPartSize = wcstoul(optArg, L'\0', 10); // 10 = base 10
            Debug(_T("Partition Size: %u\r\n"), g_snPartSize);
            break;
#endif // USE_TOOLS

        default:
            // bad parameters
            Debug(_T("%s: Unknown option\r\n"), MODULE_NAME, option);
            return;
        }
    }
}
