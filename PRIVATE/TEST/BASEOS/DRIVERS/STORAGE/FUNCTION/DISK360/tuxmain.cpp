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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
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
    _T("basic mass storage device functionality tests"),  0,                0,            0,  NULL,
    _T( "Write, Read, Verify across entire disk"),       1,                 1,    BASE +  2,  TestReadWriteSeq,    
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

static void Usage()
{    
    Debug(TEXT(""));
    Debug(TEXT("DISKTEST: Usage: tux -o -d disktest %s /maxsectors <count> /oldioctls -zorch\""),COMMON_USAGE_STRING);
    //always call this first
    UsageCommon(_T("DISKTEST"),g_pKato);
    Debug(TEXT("       /sectors <count> : Maximum number of sectors per operation; default = %u"), DEF_MAX_SECTORS);
    Debug(TEXT("       /sectorSpacing   : Number of divisions in the total number of sectors that define the testing boundaries"));
    Debug(TEXT("       /oldioctls       : Use legacy DISK_IOCTL_* codes"));
    Debug(TEXT("       /zorch          	: Tells the test that the user is aware that data will be destroyed on their device"));
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
        
            if( g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine )
            {
                // Display our Dlls command line if we have one.
                g_pKato->Log( LOG_DETAIL, _T("Command Line: \"%s\"."), g_pShellInfo->szDllCmdLine);
                ProcessCmdLineCommon(g_pShellInfo->szDllCmdLine,g_pKato);
				        //now custom command line params
                ProcessCmdLine(g_pShellInfo->szDllCmdLine);
                if(!g_szDiskName[0] && !g_szProfile[0])
				        {
					         Usage();
					         return SPR_NOT_HANDLED;
                } 
                return SPR_HANDLED;
            }
            else
				      return SPR_NOT_HANDLED;

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

		case SPM_SETUP_GROUP:
            Debug(_T("ShellProc(SPM_SETUP_GROUP, ...) called"));
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
    ProcessCmdLineCommon(g_pShellInfo->szDllCmdLine, g_pKato);
    
	CClParse cmdLine(szCmdLine);

     if(!cmdLine.GetOptVal(_T("sectorSpacing"), &g_dwSectorSpacing))
    {
        g_dwSectorSpacing = 0;
    }
    
    if(!cmdLine.GetOptVal(_T("sectors"), &g_dwNumSectorsPerOp))
    {
        g_dwNumSectorsPerOp = DEF_NUM_SECTORS;
    }
    g_fOldIoctls = cmdLine.GetOpt(_T("oldioctls"));

    // a user specified profile will override a user specified disk
	if(g_szProfile[0])
    {
        if(g_szDiskName[0])
        {
            g_pKato->Log(LOG_DETAIL, _T("WARNING:  Overriding /disk option with /profile option\n"));
        }
        g_pKato->Log(LOG_DETAIL, _T("DISK360: Storage Profile = %s"), g_szProfile);
    }    
    else 
		if(g_szDiskName[0])
		{
			g_pKato->Log(LOG_DETAIL, _T("DISK360: Disk Device Name = %s"), g_szDiskName);
		}
    
    if(g_fOldIoctls)
    {
        g_pKato->Log(LOG_DETAIL, _T("DISK360: Forcing use of legacy DISK_IOCTL* style control codes"));
    }
    
    
    g_pKato->Log(LOG_DETAIL, _T("DISK360: Number of sectors per operation = %u"), g_dwNumSectorsPerOp);
}
