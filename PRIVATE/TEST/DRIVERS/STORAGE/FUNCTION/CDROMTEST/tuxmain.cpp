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


// extern "C"
#ifdef __cplusplus
extern "C" {
#endif

// --------------------------------------------------------------------
// Tux testproc function table
//
FUNCTION_TABLE_ENTRY g_lpFTE[] = {

    _T("CDROM Storage Driver Tests"                             ),  0,      0,    0,     NULL,
    _T(   "Device ready check"                                  ),  1,      0,    6001,  TestUnitReady,
    _T(   "Query CD-ROM Disk information"                       ),  1,      0,    6002,  TestCDRomInfo,
    _T(   "Perform 1 Sector Reads"                              ),  1,      1,    6003,  TestRead,
    _T(   "Perform 32 Sector Reads"                             ),  1,     32,    6004,  TestRead,
    _T(   "Perform 128 Sector Reads"                            ),  1,    128,    6005,  TestRead, 
    _T(   "Test increasing read request sizes"                  ),  1,      0,    6006,  TestReadSize,
    _T(   "Perform Multi-SG Reads"                              ),  1,      0,    6007,  TestMultiSgRead,

    _T("CDROM Additional Functionality Tests"                   ),  0,      0,    0,     NULL,
    _T(   "Issue an ATAPI device info inquiry"                  ),  1,      0,    6011,  TestIssueInquiry,
    _T(   "Verify media eject and load functionality"           ),  1,      0,    6012,  TestEjectMedia,
    
    _T("MSF and Raw read tests"                                 ),  0,      0,    0,          NULL,
    _T(   "Read MSF Audio data from audio CD"                   ),  1,      0,    6021,  TestMSFAudio,    
    _T(   "Read MSF Audio track to file in release directory"   ),  1,      1,    6022,  TestMSFAudioData,    
    
    _T("Utilities"                                              ),  0,      0,            0,  NULL,
    _T(   "Make image of CD media"                              ),  1,      0,    6101,  UtilMakeCDImage,

    NULL,                                                   0,                  0,            0,  NULL
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

            g_hDisk = OpenCDRomDiskHandle();

            if(INVALID_HANDLE(g_hDisk))
            {
                g_pKato->Log(LOG_FAIL, _T("FAIL: unable to open CDROM device"));
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
    LPCTSTR szOpt = _T("d:i:s:vt:l:");

    Debug(_T("CDROMTEST: Usage: tux -o -d cdbvt -c\"-d DSK1 -i image.file -s max_sectors_per_read -v -t trackno -l sector_limit\""));
    
    while(EOF != (option = WinMainGetOpt(szCmdLine, szOpt)))
    {
        switch(option)
        {
 
        case _T('d'):                       
            if(DEVICE_NAME_LEN < _tcslen(optArg) )
            {
                _tcsncpy(g_szDiskName, optArg, DEVICE_NAME_LEN + 1);
                _tcsupr(g_szDiskName);
                g_szDiskName[DEVICE_NAME_LEN] = _T(':');
                g_szDiskName[DEVICE_NAME_LEN + 1] = _T('\0');
            }
            else
            {
                g_pKato->Log(LOG_DETAIL, _T("Invalid disk device name: %s"), optArg);
            }
            break;

        case _T('i'):
            // make sure the image file name is not blank
            if(_tcscmp(optArg, _T("")))
            {
                _tcsncpy(g_szImgFile, optArg, MAX_PATH);
            }            
            break;

        case _T('v'): 
            // disk is a DVD 
            g_fIsDVD = TRUE;
            break;

        case _T('l'):
            g_endSector = _ttoi(optArg);

            if(!g_endSector)
                g_endSector = CD_MAX_SECTORS;
            
            break;

        case _T('s'):
            // set an upper bound on the maximum number of sectors per request
            g_cMaxSectors = _ttoi(optArg);
            
            if(!g_cMaxSectors)
                g_cMaxSectors = MAX_READ_SECTORS;

            break;

        case _T('t'):
            // track number to use for audio tests
            g_cTrackNo = _ttoi(optArg);

            if(!g_cTrackNo)
                g_cTrackNo = 1;

            break;

        default:
            // bad parameters
            Debug(_T("CDROMTEST: Unknown option: -%c"), option);
            return;
        }
    }

    // display options
    g_pKato->Log(LOG_DETAIL, _T("CDROMTEST: Disk Device Name = %s"), g_szDiskName);
    g_pKato->Log(LOG_DETAIL, _T("CDROMTEST: Image File Name = %s"), g_szImgFile);
    g_pKato->Log(LOG_DETAIL, _T("CDROMTEST: Max Sectors per Read = %u"), g_cMaxSectors);
    g_pKato->Log(LOG_DETAIL, _T("CDROMTEST: Track = %u"), g_cTrackNo);
    if(g_fIsDVD)
    {
        g_pKato->Log(LOG_DETAIL, _T("CDROMTEST: Disk is a DVD"));
        if(CD_MAX_SECTORS == g_endSector) g_endSector = DVD_MAX_SECTORS;
    }
    g_pKato->Log(LOG_DETAIL, _T("CDROMTEST: End Sector = %u"), g_endSector);
}
