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

#define BASE 1000

// extern "C"
#ifdef __cplusplus
extern "C" {
#endif

// --------------------------------------------------------------------
// Tux testproc function table
//
FUNCTION_TABLE_ENTRY g_lpFTE[] = {
_T("Block Driver API tests"),			0,0,	0,NULL,
_T( "DISK_IOCTL_GETINFO"),		1,0,  BASE + 1,   tst_DISK_IOCTL_GETINFO,    
_T( "IOCTL_DISK_GETINFO"),		1,0,  BASE + 2   ,tst_IOCTL_DISK_GETINFO,
_T( "DISK_IOCTL_SETINFO"),		1,0,  BASE + 3   ,tst_DISK_IOCTL_SETINFO,
_T( "IOCTL_DISK_SETINFO"),		1,0,  BASE + 4   ,tst_IOCTL_DISK_SETINFO,
_T( "DISK_IOCTL_READ"),			1,0,  BASE + 5   ,tst_DISK_IOCTL_READ,   
_T( "IOCTL_DISK_READ"),			1,0,  BASE + 6   ,tst_IOCTL_DISK_READ,   
_T( "DISK_IOCTL_FORMAT_MEDIA"),	1,0,  BASE + 7   ,tst_DISK_IOCTL_FORMAT_MEDIA,   
_T( "IOCTL_DISK_FORMAT_MEDIA"),	1,0,  BASE + 8,   tst_IOCTL_DISK_FORMAT_MEDIA,  
_T( "IOCTL_DISK_GETNAME"),		1,0,  BASE + 9,   tst_IOCTL_DISK_GETNAME,  
_T( "DISK_IOCTL_GETNAME"),		1,0,  BASE + 10, tst_DISK_IOCTL_GETNAME,  
_T( "IOCTL_DISK_GET_STORAGEID"),1,0,  BASE + 11, tst_IOCTL_DISK_GET_STORAGEID,  
_T( "IOCTL_DISK_DEVICE_INFO"),	1,0,  BASE + 12, tst_IOCTL_DISK_DEVICE_INFO,  
_T( "DISK_IOCTL_WRITE"),			1,0,  BASE + 13, tst_DISK_IOCTL_WRITE,   
_T( "IOCTL_DISK_WRITE"),			1,0,  BASE + 14, tst_IOCTL_DISK_WRITE,   
_T( "IOCTL_DISK_FLUSH_CACHE"), 	1,0,  BASE + 15, tst_IOCTL_DISK_FLUSH_CACHE,

    NULL,                                           0,  0,                  0,          NULL
    
/* T( "DISK_IOCTL_INITIALIZED"),1,0,    BASE +  5,tst_DISK_IOCTL_INITIALIZED,
   _T( "IOCTL_DISK_INITIALIZED"),1,0,    BASE +  13,tst_IOCTL_DISK_INITIALIZED,
   _T( "IOCTL_DISK_READ_CDROM"),1,0,    BASE +  17,tst_IOCTL_DISK_READ_CDROM,
   _T( "IOCTL_DISK_WRITE_CDROM"),1,0,    BASE +  18,tst_IOCTL_DISK_WRITE_CDROM,
   _T( "IOCTL_DISK_DELETE_SECTORS"),1,0,    BASE +  19,tst_IOCTL_DISK_DELETE_SECTORS,
   _T( "IOCTL_DISK_GET_SECTOR_ADDR"),1,0,    BASE +  20,tst_IOCTL_DISK_GET_SECTOR_ADDR,
   _T( "IOCTL_DISK_COPY_EXTERNAL_START"),1,0,    BASE +  21,tst_IOCTL_DISK_COPY_EXTERNAL_START,
   _T( "IOCTL_DISK_COPY_EXTERNAL_COMPLETE"),1,0,    BASE +  22,tst_IOCTL_DISK_COPY_EXTERNAL_COMPLETE,
   _T( "IOCTL_DISK_GETPMTIMINGS"),1,0,    BASE +  23,tst_IOCTL_DISK_GETPMTIMINGS,
   _T( "IOCTL_DISK_SECURE_WIPE"),1,0,    BASE +  24,tst_IOCTL_DISK_SECURE_WIPE,
   _T( "IOCTL_DISK_SET_SECURE_WIPE_FLAG"),1,0,    BASE +  25,tst_IOCTL_DISK_SET_SECURE_WIPE_FLAG,
   _T( "IOCTL_FILE_WRITE_GATHER"),1,0,    BASE +  26,tst_IOCTL_FILE_WRITE_GATHER,
   _T( "IOCTL_FILE_READ_SCATTER"),1,0,    BASE +  27,tst_IOCTL_FILE_READ_SCATTER,
   _T( "IOCTL_FILE_GET_VOLUME_INFO"),1,0,    BASE +  28,tst_IOCTL_FILE_GET_VOLUME_INFO,
   _T( "IOCTL_DISK_FORMAT_VOLUME"),1,0,    BASE +  29,tst_IOCTL_DISK_FORMAT_VOLUME,
   _T( "IOCTL_DISK_SCAN_VOLUME"),1,0,    BASE +  30,tst_IOCTL_DISK_SCAN_VOLUME,

#define DISK_IOCTL_INITIALIZED  4
#define IOCTL_DISK_BASE FILE_DEVICE_DISK
#define IOCTL_DISK_INITIALIZED     \
#define IOCTL_DISK_SET_STANDBY_TIMER     \
#define IOCTL_DISK_STANDBY_NOW     \
#define IOCTL_DISK_DELETE_CLUSTER	\

IOCTL_CDROM_DISC_INFO 			This IOCTL retrieves disc information to fill the CDROM_DISCINFO structure. 
IOCTL_CDROM_EJECT_MEDIA		The IOCTL ejects the CD-ROM. 
IOCTL_CDROM_GET_SENSE_DATA 	This IOCTL retrieves CD-ROM sense information and fills the CD_SENSE_DATA structure. 
IOCTL_CDROM_ISSUE_INQUIRY 		This IOCTL retrieves information used in the INQUIRY_DATA structure. 
IOCTL_CDROM_PAUSE_AUDIO 		This IOCTL suspends audio play. 
IOCTL_CDROM_PLAY_AUDIO_MSF 	This IOCTL plays audio from the specified range of the medium. 
IOCTL_CDROM_READ_SG 			This IOCTL reads scatter buffers from the CD-ROM and the information is stored in the CDROM_READ structure. 
IOCTL_CDROM_READ_TOC 			This IOCTL returns the table of contents of the medium. 
IOCTL_CDROM_RESUME_AUDIO 		This IOCTL resumes a suspended audio operation. 
IOCTL_CDROM_SEEK_AUDIO_MSF 	This IOCTL moves the heads to the specified minutes, seconds, and frames on the medium. 
IOCTL_CDROM_STOP_AUDIO 		This IOCTL stops audio play. 
IOCTL_CDROM_TEST_UNIT_READY 	This IOCTL retrieves disc ready information and fills the CDROM_TESTUNITREADY structure. 
IOCTL_DISK_GET_SECTOR_ADDR 	This IOCTL receives an array of logical sectors and returns an array of addresses. Each array of addresses corresponds to the statically-mapped virtual address for a particular logical sector. 
IOCTL_DVD_GET_REGION 			This IOCTL returns DVD disk and drive regions. 
*/
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
    Debug(TEXT("DISKTEST: Usage: tux -o -d disktest -c\"/disk <disk> /profile <profile> /maxsectors <count>\""));
    Debug(TEXT("       /disk <disk>        : name of the disk to test (e.g. DSK1:); default = first detected disk"));
    Debug(TEXT("       /profile <profile>  : limit to devices of the specified storage profile; default = all profiles"));
    Debug(TEXT("       /maxsectors <count> : maximum number of sectors per operation; default = %u"), DEF_MAX_SECTORS);
    Debug(TEXT("       /store              : open the disk using the OpenStore() API"));
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

            g_hDisk = OpenDiskHandle();
	  
            if(INVALID_HANDLE(g_hDisk))
            {
                g_pKato->Log(LOG_FAIL, _T("FAIL: unable to open Hard Disk media"));
				return SPR_FAIL;
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
    if(!cmdLine.GetOptString(_T("profile"), g_szProfile, PROFILENAMESIZE))
    {
        StringCchCopyEx(g_szProfile, PROFILENAMESIZE, _T(""), NULL, NULL, STRSAFE_NULL_ON_FAILURE);
    }
    if(!cmdLine.GetOptVal(_T("maxsectors"), &g_dwMaxSectorsPerOp))
    {
        g_dwMaxSectorsPerOp = DEF_MAX_SECTORS;
    }
    if(!cmdLine.GetOptVal(_T("breakat"),&g_dwBreakAtTestCase))
    {
		  g_dwBreakAtTestCase = 0;
    }
  
	g_dwBreakOnFailure = cmdLine.GetOpt(_T("breakonfailure"));
    g_fOpenAsStore = cmdLine.GetOpt(_T("store"));

    // a user specified disk name will override a user specified profile
    if(g_szDiskName[0])
    {
        g_pKato->Log(LOG_DETAIL, _T("DISKTEST: Disk Device Name = %s"), g_szDiskName);
    }
    else if(g_szProfile[0])
    {
        g_pKato->Log(LOG_DETAIL, _T("DISKTEST: Storage Profile = %s"), g_szProfile);
    }

    if(g_fOpenAsStore)
    {
        g_pKato->Log(LOG_DETAIL, _T("DISKTEST: Will open disk as a store (using OpenStore())"));
    }
    
    g_pKato->Log(LOG_DETAIL, _T("DISKTEST: Max Sectors per operation = %u"), g_dwMaxSectorsPerOp);
}
