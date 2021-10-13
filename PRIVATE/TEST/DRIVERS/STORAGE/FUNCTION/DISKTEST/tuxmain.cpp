//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "main.h"

#undef __FILE_NAME__
#define __FILE_NAME__   _T("TUXMAIN.CPP")

// debugging
#define Debug NKDbgPrintfW

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
CKato *g_pKato = NULL;

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO *g_pShellInfo;

// Global critical section to be used by threaded tests if necessary.
CRITICAL_SECTION g_csProcess;

extern TCHAR optArg[];

#define BASE 4000

// extern "C"
#ifdef __cplusplus
extern "C" {
#endif

// --------------------------------------------------------------------
// Tux testproc function table
//
FUNCTION_TABLE_ENTRY g_lpFTE[] = {
    _T("Hard Disk Storage Driver Tests"               ),  0,                0,            0,  NULL,
    _T( "Format mass storage disk"                    ),  1,                0,    BASE +  2,  TestFormatMedia,    
    _T( "Write, Read, Verify 1 sector at at time"     ),  1,                1,    BASE +  3,  TestReadWriteSeq,
    _T( "Write, Read, Verify 8 sectors at a time"     ),  1,                8,    BASE +  4,  TestReadWriteSeq,
    _T( "Write, Read, Verify 32 sectors at a time"    ),  1,               32,    BASE +  5,  TestReadWriteSeq,   
    _T( "Write, Read, Verify varying sized buffers"   ),  1,                0,    BASE +  6,  TestReadWriteSize,

// this test will only work if driver supports multiple sg buffers
    _T( "Write, Read with 3 SG buffers, Verify"       ),  1,                3,    BASE +  7,  TestReadWriteMulti,
    _T( "Write, Read with 2 SG buffers, Verify"       ),  1,                2,    BASE +  8,  TestReadWriteMulti,
    _T( "Write, Read with 8 SG buffers, Verify"       ),  1,                2,    BASE +  9,  TestReadWriteMulti,
    _T( "Write, Read with 32 SG buffers, Verify"      ),  1,                5,    BASE + 10,  TestReadWriteMulti,
    _T( "Vaious SG buffer sizezs"                     ),  1,                0,    BASE + 11,  TestSGBoundsCheck,

// these tests don't need to be run because they are not necessary for the functioning of FATFS
// and don't always pass even on valid block driver
    _T( "Read buffer extends past last sector"        ),  1,                0,    BASE + 12,  TestReadPastEnd,
    _T( "Read starts past final sector"               ),  1,                0,    BASE + 13,  TestReadOffDisk,    
    _T( "Write buffer extends past last sector"       ),  1,                0,    BASE + 14,  TestWritePastEnd,
    _T( "Write starts past final sector"              ),  1,                0,    BASE + 15,  TestWriteOffDisk,  

    _T( "Delete sector test cases"                    ),  1,                0,    BASE + 16,  TestInvalidDeleteSectors,  


    NULL,                                           0,  0,                  0,          NULL
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

static void ProcessCmdLine( LPCTSTR );

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
            Debug(_T("ShellProc(SPM_REGISTER, ...) called"));

            Debug(_T("DISKTEST: Usage: tux -o -d disktest -c\"-d DSK1 -p DSK -o\""));
            Debug(_T("DISKTEST:        -d <DISKNAME>  : specifies disk to test"));
            Debug(_T("DISKTEST:        -p <PREFIX>    : enumerate devices with a different prefix"));
            Debug(_T("DISKTEST:        -o             : force old DISK_IOCTL commands (for ATADisk, etc.)")); 
            
            
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

            g_hDisk = OpenDiskHandle();

            if(INVALID_HANDLE(g_hDisk))
            {
                g_pKato->Log(LOG_FAIL, _T("FAIL: unable to open Hard Disk media"));
            }
            
            return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_STOP_SCRIPT
        //
        case SPM_STOP_SCRIPT:

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
            g_pKato->BeginLevel(0, _T("BEGIN GROUP: FSDTEST.DLL"));
            
            return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_END_GROUP
        //
        case SPM_END_GROUP:
            Debug(_T("ShellProc(SPM_END_GROUP, ...) called"));
            g_pKato->EndLevel(_T("END GROUP: TUXDEMO.DLL"));
            
            return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_BEGIN_TEST
        //
        case SPM_BEGIN_TEST:
            Debug(_T("ShellProc(SPM_BEGIN_TEST, ...) called"));

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
    LPCTSTR szOpt = _T("d:p:o");
    
    while(EOF != (option = WinMainGetOpt(szCmdLine, szOpt)))
    {
        switch(option)
        {
 
        case _T('d'):                       
            // supplied a specific disk name
            _tcsncpy(g_szDiskName, optArg, MAX_PATH);
            if(!wcschr(g_szDiskName, _T(':')))
                wcscat(g_szDiskName, _T(":"));
            break;   

        case _T('p'):                       
            // supplied a new disk prefix to be used-- default is DSK
            _tcsncpy(g_szDiskPrefix, optArg, MAX_PATH);
            break;   

        case _T('o'):
            g_fOldIoctls = TRUE;
            break;

        default:
            // bad parameters
            Debug(_T("DISKTEST: Unknown option"), option);
            return;
        }
    }

    // output parameters
    if(g_szDiskName[0])
    {
        g_pKato->Log(LOG_DETAIL, _T("Disk Device Name = %s"), g_szDiskName);
    }
    else
    {
        g_pKato->Log(LOG_DETAIL, _T("Disk search prefix = %s"), g_szDiskPrefix);
    }
    
    if(g_fOldIoctls)
    {
        g_pKato->Log(LOG_DETAIL, _T("Using old DISK_IOCTL* style control codes"));
    }
}
