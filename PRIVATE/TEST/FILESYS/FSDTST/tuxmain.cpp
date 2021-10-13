//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "TuxMain.h"
#include "TestProc.h"
#include "storeutil.h"

// debugging
#define Debug NKDbgPrintfW

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
CKato *g_pKato = NULL;

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO *g_pShellInfo;

TEST_OPTIONS g_testOptions = {0};

#define BASE 4000

// extern "C"
#ifdef __cplusplus
extern "C" {
#endif

// --------------------------------------------------------------------
// Tux testproc function table
//
FUNCTION_TABLE_ENTRY g_lpFTE[] = {

    _T("FSD Basic BVT Test Cases"                                                       ), 0, 0,            0, NULL,
    _T(  "Create, write to, read from, verify, and delete a file"                       ), 1, 0,        1001, Tst_BVT1,
    _T(  "Create, copy, and delete a file"                                              ), 1, 0,        1002, Tst_BVT2,
    _T(  "Create, move, and delete a file"                                              ), 1, 0,        1003, Tst_BVT3,
    _T(  "Create, enumerate, and delete two files"                                      ), 1, 0,        1004, Tst_BVT4,
    _T(  "Create mulitple files, find the first, and delete all files"                  ), 1, 0,        1005, Tst_BVT5,
    _T(  "Create, get attributes for, and delete a file"                                ), 1, 0,        1006, Tst_BVT6,
    _T(  "Create, get information by handle for, and delete a file"                     ), 1, 0,        1007, Tst_BVT7,
    _T(  "Create 32 empty files in a directory"                                         ), 1, 32,       1008, Tst_BVT8,
    _T(  "Create 128 empty files in a directory"                                        ), 1, 128,      1009, Tst_BVT8,
    _T(  "Test memory mapping"                                                          ), 1, 0,        1010, Tst_BVT9,
    
    _T("FSD Depth Test Cases"                                                           ), 0, 0,           0, NULL,
    _T(  "Create, modify, and delete cluster size files w/all attrib combos"            ), 1, 0,        5001, Tst_Depth1,
    _T(  "Create, modify, and delete cluster size - 1 files w/all attrib combos"        ), 1, 0,        5002, Tst_Depth2,
    _T(  "Create, modify, and delete cluster size + 1 files w/all attrib combos"        ), 1, 0,        5003, Tst_Depth3,
    _T(  "Create, modify, and delete 1 byte files w/all attrib combos"                  ), 1, 0,        5004, Tst_Depth4,
    _T(  "Create, modify, and delete 0 byte files w/all attrib combos"                  ), 1, 0,        5005, Tst_Depth5,
    _T(  "Create deep single-level directories of MAX_PATH +/- 10 in size"              ), 1, 0,        5006, Tst_Depth6,
    _T(  "Create deep multiple-level directories of MAX_PATH +/- x  in size"            ), 1, 0,        5007, Tst_Depth7,
    _T(  "Create a file in a directory that does not exist"                             ), 1, 0,        5008, Tst_Depth8,
    _T(  "Create subdirs in dirs and create files in those subdirs"                     ), 1, 0,        5009, Tst_Depth9,
    _T(  "Create files in dirs, copy them to another dir, and moving a dir"             ), 1, 0,        5010, Tst_Depth10,
    _T(  "FileTimeToLocalTime, FileTimeToSystemTime, and LocalTimeToFileTime"           ), 1, 0,        5011, Tst_Depth11,
    _T(  "Fragment test - fill drive with 4 byte files, delete every other one."        ), 1, 0,        5012, Tst_Depth12,
    _T(  "Fragment test - fill drive with cluster-sized files, delete every other one." ), 1, 0,        5013, Tst_Depth13,
    _T(  "Create files in dirs, copy them to another dir and back, and move dir"        ), 1, 0,        5014, Tst_Depth14,
    _T(  "Create files on card, copy to ObjStore, move dir back to card"                ), 1, 0,        5015, Tst_Depth15,
    _T(  "Create files on ObjStore, copy to card, move dir back to ObjStore"            ), 1, 0,        5016, Tst_Depth16,
    _T(  "Creating/removing the \"Storage Card\" mount-point from the name space"       ), 1, 0,        5017, Tst_Depth17,
    _T(  "Truncating a file"                                                            ), 1, 0,        5018, Tst_Depth18,
    _T(  "Create maximum root directory entries"                                        ), 1, 0,        5019, Tst_Depth19,
    _T(  "Create a file with the same name as an existing dir"                          ), 1, 0,        5020, Tst_Depth20,
    _T(  "Create a dir with the same name as an existing file"                          ), 1, 0,        5021, Tst_Depth21,
    _T(  "Create more than 999 files using long file names (name mangling)"             ), 1, 0,        5022, Tst_Depth22,  
    _T(  "Create a 100 byte file with 2 clusters free"                                  ), 1, 0,        5023, Tst_Depth23,
    _T(  "Create a dir with only 1 cluster free"                                        ), 1, 0,        5024, Tst_Depth24,
    _T(  "Create a 100 byte file with 1 cluster free"                                   ), 1, 0,        5025, Tst_Depth25,
    _T(  "Create dirs with 1 clusters free until disk is full"                          ), 1, 0,        5026, Tst_Depth26,
    _T(  "Create a dir with only 0 clusters free"                                       ), 1, 0,        5027, Tst_Depth27,    
    _T(  "Create a 0 byte file with 0 clusters free"                                    ), 1, 0,        5028, Tst_Depth28,    
    _T(  "Create a 100 byte file with 0 clusters free"                                  ), 1, 0,        5029, Tst_Depth29,
    _T(  "WriteFileWithSeek at 10 B offsets with 1 cluster free until the disk is full" ), 1, 10,       5030, Tst_Depth30,    
    _T(  "Write and read data at 1 KB offsets using Write/ReadFileWithSeek"             ), 1, 1024,     5031, Tst_Depth31,

    _T("Store Partition tools"                                                          ), 0, 0,           0, NULL,
    _T(  "Create 1 partition on stores"                                                 ), 1, 1,        6001, Tst_Partition,
    _T(  "Create 2 partitions on stores"                                                ), 1, 2,        6002, Tst_Partition,
    _T(  "Create 3 partitions on stores"                                                ), 1, 3,        6003, Tst_Partition,

    _T("FSD Multi-Threaded Depth Test Cases"                                            ), 0, 0,           0, NULL,
    _T(  "Create and verify files using a different sub-directory for each thread"      ), 1, 0,        8001, Tst_MultiThrd1,

    _T("FSD Format"                                                                     ), 0, 0,           0, NULL,
    _T(  "Format mounted volumes"                                                       ), 1, 0,        9001, Tst_Format,
    
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
        case DLL_PROCESS_DETACH:
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            return TRUE;
    }
    return FALSE;
}

#ifdef __cplusplus
} // end extern "C"
#endif

static BOOL ProcessCmdLine(LPCTSTR szCmdLine, PTEST_OPTIONS pTestOptions);
static VOID SetupDefaults(PTEST_OPTIONS pTestOptions);

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

            // If we are UNICODE, then tell Tux this by setting the following flag.
            #ifdef UNICODE
                ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
            #endif

            // turn on kato debugging
            KatoDebug(1, KATO_MAX_VERBOSITY,KATO_MAX_VERBOSITY,KATO_MAX_LEVEL);

            // Get/Create our global logging object.
            g_pKato = (CKato*)KatoGetDefaultObject();            

            return SPR_HANDLED;        

        // --------------------------------------------------------------------
        // Message: SPM_UNLOAD_DLL
        //
        case SPM_UNLOAD_DLL: 
            Debug(_T("ShellProc(SPM_UNLOAD_DLL, ...) called"));
            DestroyFSList();
            return SPR_HANDLED;      

        // --------------------------------------------------------------------
        // Message: SPM_SHELL_INFO
        //
        case SPM_SHELL_INFO:
            Debug(_T("ShellProc(SPM_SHELL_INFO, ...) called\r\n"));
      
            // Store a pointer to our shell info for later use.
            g_pShellInfo = (LPSPS_SHELL_INFO)spParam;

            SetupDefaults(&g_testOptions);
        
            if( g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine )
            {
                // Display our Dlls command line if we have one.
                g_pKato->Log( LOG_DETAIL, 
                    _T("Command Line: \"%s\"."), g_pShellInfo->szDllCmdLine);

                ProcessCmdLine(g_pShellInfo->szDllCmdLine, &g_testOptions);
            } 
            BuildFSList();

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
            
            return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_STOP_SCRIPT
        //
        case SPM_STOP_SCRIPT:
            
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

                if( _T('\"') == *pCurPos || _T('\'') == *pCurPos)
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

static BOOL ProcessCmdLine(LPCTSTR szCmdLine, PTEST_OPTIONS pTestOptions)
{
    // parse command line
    int option = 0;
    LPCTSTR szOpt = _T("p:s:r:c:d:fq");

    Debug(_T("%s: Usage: tux -o -d fsdtst -c\"[-p PROFILE -s FILESYSTEM.DLL] [-r PATH]")
          _T("-c clusterbytes -d maxrootdirs -q -f\""), MODULE_NAME);
    
    while(EOF != (option = WinMainGetOpt(szCmdLine, szOpt)))
    {
        switch(option)
        {
        case _T('p'):
        case _T('P'):
            
            // If none is specified, only devices using the default profile
            // will be tested
            //
            _tcsncpy(pTestOptions->szProfile, optArg, 
                sizeof(pTestOptions->szProfile)/sizeof(pTestOptions->szProfile[0]));
            Debug(_T("TEST OPTION: Storage Profile is [%s]\r\n"), pTestOptions->szProfile);
            break;

        case _T('s'):
        case _T('S'):
            _tcsncpy(pTestOptions->szFilesystem, optArg, 
                sizeof(pTestOptions->szFilesystem)/sizeof(pTestOptions->szFilesystem[0]));
            Debug(_T("TEST OPTION: File System is [%s]\r\n"), pTestOptions->szFilesystem);
            break;

        case _T('q'):
        case _T('Q'):
            pTestOptions->fQuickFill = !pTestOptions->fQuickFill;
            if(pTestOptions->fQuickFill)
                Debug(_T("TEST OPTION: Quick-Fill option set\r\n"));
            else
                Debug(_T("TEST OPTION: Quick-Fill option un-set\r\n"));
            break;

        case _T('c'):
        case _T('C'):
            pTestOptions->nClusterSize = _ttoi(optArg);
            if(pTestOptions->nClusterSize > 0)
            {
                Debug(_T("TEST OPTION: Cluster Size (for non-FAT volumes) is [%u bytes]\r\n"),
                    pTestOptions->nClusterSize);
            }
            else
            {
                // user specified a bad option... resorting back to default
                //
                pTestOptions->nClusterSize = DEF_CLUSTER_SIZE;
            }
            break;

        case _T('d'):
        case _T('D'):
            pTestOptions->nMaxRootDirs = _ttoi(optArg);
            if(pTestOptions->nMaxRootDirs > 0)
            {
                Debug(_T("TEST OPTION: Max root directory entries is [%u]\r\n"),
                    pTestOptions->nMaxRootDirs);
            }
            else
            {
                // user specified a bad option... resorting back to default
                //
                pTestOptions->nMaxRootDirs = DEF_ROOT_DIR_ENTRIES;
            }
            break;

        case _T('f'):
        case _T('F'):
            pTestOptions->fFormat = !pTestOptions->fFormat;
            if(pTestOptions->fFormat)
                Debug(_T("TEST OPTION: Format-Before-Each-Test option set\r\n"));
            else
                Debug(_T("TEST OPTION: Format-Before-Each-Test option un-set\r\n"));
            break;

        case _T('r'):
        case _T('R'):
            _tcsncpy(pTestOptions->szPath, optArg,
                sizeof(pTestOptions->szPath)/sizeof(pTestOptions->szPath[0]));
            break;
            
        default:
            // bad parameters
            Debug(_T("%s: Unknown option\r\n"), MODULE_NAME, option);
            return FALSE;
        }
    }

    return TRUE;
}

/* setup test options with default values defined in tuxmain.h */
static VOID SetupDefaults(PTEST_OPTIONS pTestOptions)
{
    _tcsncpy(pTestOptions->szFilesystem, DEF_FILESYS_DLL, 
        sizeof(pTestOptions->szFilesystem)/sizeof(pTestOptions->szFilesystem[0]));

    _tcsncpy(pTestOptions->szProfile, DEF_STORAGE_PROFILE, 
        sizeof(pTestOptions->szProfile)/sizeof(pTestOptions->szProfile[0]));

    _tcsncpy(pTestOptions->szPath, DEF_STORAGE_PATH,
        sizeof(pTestOptions->szPath)/sizeof(pTestOptions->szPath[0]));

    pTestOptions->fQuickFill = DEF_QUICK_FILL_FLAG;
    pTestOptions->fFormat = DEF_FORMAT_FLAG;
    pTestOptions->nFileCount = DEF_FILE_COUNT;
    pTestOptions->nThreadsPerVol = DEF_THREADS_PER_VOL;
    pTestOptions->nMaxRootDirs = DEF_ROOT_DIR_ENTRIES;
    pTestOptions->nClusterSize = DEF_CLUSTER_SIZE;
}
