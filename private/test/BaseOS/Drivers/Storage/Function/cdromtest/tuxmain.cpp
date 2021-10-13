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
    _T(   "Test various buffer allocation types"                ),  1,      0,    6008,  TestReadBufferTypes,
    _T(   "Read CD-ROM table of contents"                       ),  1,      0,    6009,  TestReadTOC,

    _T("CDROM Additional Functionality Tests"                   ),  0,      0,    0,     NULL,
    _T(   "Issue an ATAPI device info inquiry"                  ),  1,      0,    6011,  TestIssueInquiry,
    _T(   "Verify media eject and load functionality"           ),  1,      0,    6012,  TestEjectMedia,
    
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

static void ProcessCmdLine( LPCTSTR );

static void Usage()
{    
    Debug(TEXT(""));
    Debug(TEXT("CDROMTEST: Usage: tux -o -d cdromtest -c\"/disk <disk> /profile <profile> /image <file> /maxsectors <count> /endsector <sector> /retries <count> /dvd /store /vol\""));
    Debug(TEXT("       /disk <disk>        : name of the disk to test (e.g. DSK1:); default = first detected data cd"));
    Debug(TEXT("       /profile <profile>  : limit to devices of the specified storage profile; default = all profiles"));
    Debug(TEXT("       /image <file>       : image file for data comparison; default = %s"), DEF_IMG_FILE_NAME);
    Debug(TEXT("       /maxsectors <count> : maximum number of sectors per read; default = %u"), MAX_READ_SECTORS);
    Debug(TEXT("       /endsector <sector> : test will not read past the sector specified; default = %u"), CD_MAX_SECTORS);
    Debug(TEXT("       /retries <count>    : number of read retry attempts before stopping; default = %u"), MAX_READ_RETRIES);
    Debug(TEXT("       /dvd                : indicates that the disk is a DVD"));
    Debug(TEXT("       /store              : open the disk using the OpenStore() API"));
    Debug(TEXT("       /vol                : open the disk as a volume (\\CDROM Drive\\VOL:)"));
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
            g_pKato->BeginLevel(0, _T("BEGIN GROUP: CDROMTEST.DLL"));
            
            return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_END_GROUP
        //
        case SPM_END_GROUP:
            Debug(_T("ShellProc(SPM_END_GROUP, ...) called"));
            g_pKato->EndLevel(_T("END GROUP: CDROMTEST.DLL"));
            
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

static
void
ProcessCmdLine( 
    LPCTSTR szCmdLine )
{
    CClParse cmdLine(szCmdLine);

    if(!cmdLine.GetOptString(_T("disk"), g_szDiskName, MAX_PATH))
    {
        StringCchCopyEx(g_szDiskName, MAX_PATH, _T(""), NULL, NULL, STRSAFE_NULL_ON_FAILURE);
    }

    if(!cmdLine.GetOptString(_T("image"), g_szImgFile, MAX_PATH))
    {
        StringCchCopyEx(g_szImgFile, MAX_PATH, DEF_IMG_FILE_NAME, NULL, NULL, STRSAFE_NULL_ON_FAILURE);
    }

    if(!cmdLine.GetOptString(_T("profile"), g_szProfile, PROFILENAMESIZE))
    {
        StringCchCopyEx(g_szProfile, PROFILENAMESIZE, _T(""), NULL, NULL, STRSAFE_NULL_ON_FAILURE);
    }

    if(!cmdLine.GetOptVal(_T("maxsect"), &g_cMaxSectors))
    {
        g_cMaxSectors = MAX_READ_SECTORS;
    }

    if(!cmdLine.GetOptVal(_T("retries"), &g_cMaxReadRetries))
    {
        g_cMaxReadRetries = MAX_READ_RETRIES;
    }

    if(!cmdLine.GetOptVal(_T("endsector"), &g_endSector))
    {
        g_endSector = CD_MAX_SECTORS;
    }

    g_fIsDVD = cmdLine.GetOpt(_T("dvd"));    

    g_fOpenAsStore = cmdLine.GetOpt(_T("store"));

    g_fOpenAsVolume = cmdLine.GetOpt(_T("vol"));
    
    if(g_fOpenAsStore && g_fOpenAsVolume)
    {
        // open-as-volume overrides open-as-store
        g_fOpenAsStore = FALSE;
    }

    // display the options
    if(g_szDiskName[0]) 
    {
        g_pKato->Log(LOG_DETAIL, _T("CDROMTEST: Disk Device Name = %s"), g_szDiskName);
    }
    else 
    {
        g_pKato->Log(LOG_DETAIL, _T("CDROMTEST: Disk Device will be auto-detected"));
    }
    
    if(g_szProfile[0]) 
    {
        g_pKato->Log(LOG_DETAIL, _T("CDROMTEST: Storage Profile = %s"), g_szProfile);
    }

    if(g_szImgFile[0]) 
    {
        g_pKato->Log(LOG_DETAIL, _T("CDROMTEST: Image File Name = %s"), g_szImgFile);
    }

    g_pKato->Log(LOG_DETAIL, _T("CDROMTEST: Max Sectors per Read = %u"), g_cMaxSectors);    

    if(g_fIsDVD)
    {
        g_pKato->Log(LOG_DETAIL, _T("CDROMTEST: Disk is a DVD"));
        if(CD_MAX_SECTORS == g_endSector) g_endSector = DVD_MAX_SECTORS;
    }
    
    if(g_fOpenAsStore)
    {
        g_pKato->Log(LOG_DETAIL, _T("CDROMTEST: Will open disk as a store (using OpenStore())"));
    }
    
    if(g_fOpenAsVolume)
    {
        g_pKato->Log(LOG_DETAIL, _T("CDROMTEST: Will open disk as a volume (\\CDROM Drive\\VOL:)"));
    }
    g_pKato->Log(LOG_DETAIL, _T("CDROMTEST: End Sector = %u"), g_endSector);
}
