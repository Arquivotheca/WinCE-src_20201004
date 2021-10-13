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
    _T("basic filesystem performance tests"),  0,                0,            0,  NULL,
    _T( "Read/Write Performance"                    ),  1,                0,    BASE +  2,  TestReadWritePerf,    
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

static bool ProcessCmdLine( LPCTSTR );
static bool ParseRange(TCHAR* textRange,DWORD* Start,DWORD* End);
static void Usage()
{    
    Debug(TEXT(""));
    Debug(TEXT("FATFSPERF: Usage: tux -o -d fatfsperf -c\"/path Storage Path /FileSize start-end /TransferSize /start-end\""));
    Debug(TEXT("       /path <Storage Path>: name of the path to test (e.g. \\Hard Disk:);"));
    Debug(TEXT("       /FileSize <Range>: Range of filesizes to test in sectors of 512 bytes (e.g. 1-65536 512 bytes to 32MB);"));
    Debug(TEXT("       /TransferSize <Range>: Range of transfer sizes to test in sectors of 512 bytes (e.g. 1-64 512 bytes to 32k);"));
    Debug(TEXT("       /ClusterSize <Range>: Range of cluster sizes to test in sectors of 512 bytes (e.g. 1-64 512 bytes to 32k);"));
    Debug(TEXT("       /Writethrough: Sets writethrough cache on;"));
    Debug(TEXT("       /PreAllocateFile: Preallocates file when writing by using SetFilePointer(EOF);"));
    Debug(TEXT("       /FatCache <size>: Turns FAT caching on with specified number of sectors;"));
    Debug(TEXT("       /DataCache <size>: Turns data caching on with specified number of sectors;"));
    Debug(TEXT("       /DiskUsage <Range): Sets range of disk % full (e.g. 0-100).  Only increments in intervals of 10%;"));
    
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
                g_pKato->Log( LOG_DETAIL,_T("Command Line: \"%s\"."), g_pShellInfo->szDllCmdLine);
                if(!ProcessCmdLine(g_pShellInfo->szDllCmdLine))
                  return SPR_FAIL;
            } 
            else
            {
                return SPR_FAIL;
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
            //calibrate the perf logger
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
            ASSERT(!"Default case reached in ShellProc!");
            return SPR_NOT_HANDLED;
    }
}

static
bool
ProcessCmdLine( 
    LPCTSTR szCmdLine )
{
    CClParse cmdLine(szCmdLine);
    if(!cmdLine.GetOptString(_T("path"), g_szPath, MAX_PATH))
    {
         g_pKato->Log(LOG_DETAIL, _T("FATFSPERF: path not specified."));
         Usage();
         return false;
    }
    TCHAR tempVal[MAX_PATH];
    if(!cmdLine.GetOptString(_T("FileSize"), tempVal,MAX_PATH))
    {
        g_pKato->Log(LOG_DETAIL, _T("FATFSPERF: FileSize range not specified"));
        Usage();
        return false;
    }
    else
    {
      if(!ParseRange(tempVal,&g_dwStartFileSize,&g_dwEndFileSize))
      {
          g_pKato->Log(LOG_DETAIL, _T("FATFSPERF: Couldn't parse string %s."),tempVal);
          Usage();
          return false;
      }    
      g_dwStartFileSize = NearestPowerOfTwo(g_dwStartFileSize);
      g_dwEndFileSize = NearestPowerOfTwo(g_dwEndFileSize);
    }
  
    if(!cmdLine.GetOptString(_T("TransferSize"), tempVal,MAX_PATH))
    {
        g_pKato->Log(LOG_DETAIL, _T("FATFSPERF: TransferSize range not specified"));
        Usage();
        return false;
    }
    else
    {
      if(!ParseRange(tempVal,&g_dwStartTransferSize,&g_dwEndTransferSize))
      {
          g_pKato->Log(LOG_DETAIL, _T("FATFSPERF: Couldn't parse string %s."),tempVal);
          Usage();
          return false;
      }   
      g_dwStartTransferSize = NearestPowerOfTwo(g_dwStartTransferSize);
      g_dwEndTransferSize = NearestPowerOfTwo(g_dwEndTransferSize); 
    }
  
    if(!cmdLine.GetOptString(_T("ClusterSize"), tempVal,MAX_PATH))
    {
        g_pKato->Log(LOG_DETAIL, _T("FATFSPERF: ClusterSize range not specified"));
        Usage();
        return false;
    }
    else
    {
      if(!ParseRange(tempVal,&g_dwStartClusterSize,&g_dwEndClusterSize))
      {
          g_pKato->Log(LOG_DETAIL, _T("FATFSPERF: Couldn't parse string %s."),tempVal);
          Usage();
          return false;
      } 
      g_dwStartClusterSize = NearestPowerOfTwo(g_dwStartClusterSize);
      g_dwEndClusterSize = NearestPowerOfTwo(g_dwEndClusterSize); 
    }
    
    if(!cmdLine.GetOptString(_T("DiskUsage"), tempVal,MAX_PATH))
    {
        g_pKato->Log(LOG_DETAIL, _T("FATFSPERF: DiskUsage range not specified."));
        Usage();
        return false;
    }
    else
    {
      if(!ParseRange(tempVal,&g_dwStartDiskUsage,&g_dwEndDiskUsage))
      {
          g_pKato->Log(LOG_DETAIL, _T("FATFSPERF: Couldn't parse string %s."), tempVal);
          Usage();
          return false;
      }  
      else
      {
        if(  g_dwStartDiskUsage < 0 || g_dwEndDiskUsage > 100)
        {
            g_pKato->Log(LOG_DETAIL, _T("FATFSPERF: DiskUsage is invalid  %d-%d."),g_dwStartDiskUsage,g_dwEndDiskUsage);
            Usage();
            return false;
        }
        g_dwStartDiskUsage = NearestIntervalOf33(g_dwStartDiskUsage);
        g_dwEndDiskUsage = NearestIntervalOf33(g_dwEndDiskUsage);
              
      }  
    }
    //start as false, then set to true if FATCache or DATACache is specified
    g_fUseCache = false;
    if(!cmdLine.GetOptString(_T("FATCache"), tempVal,MAX_PATH))
    {
        g_pKato->Log(LOG_DETAIL, _T("FATFSPERF: FATCache range not specified."));
        
    }
    else
    {
       if(!ParseRange(tempVal,&g_dwStartFATCacheSize,&g_dwEndFATCacheSize))
       {
          g_pKato->Log(LOG_DETAIL, _T("FATFSPERF: Couldn't parse string %s."), tempVal);
          Usage();
          return false;
       }  
       else
       {
        g_fUseCache = true;
        g_dwStartFATCacheSize = NearestPowerOfTwo(g_dwStartFATCacheSize);
        g_dwEndFATCacheSize = NearestPowerOfTwo(g_dwEndFATCacheSize);
       }   
    }
  
    if(!cmdLine.GetOptString(_T("DATACache"), tempVal,MAX_PATH))
    {
        g_pKato->Log(LOG_DETAIL, _T("FATFSPERF: DATACache range not specified."));
        
    }
    else
    {
       if(!ParseRange(tempVal,&g_dwStartDATACacheSize,&g_dwEndDATACacheSize))
       {
          g_pKato->Log(LOG_DETAIL, _T("FATFSPERF: Couldn't parse string %s."), tempVal);
          Usage();
          return false;
       }  
       else
       {
        g_fUseCache = true;
        g_dwStartDATACacheSize = NearestPowerOfTwo(g_dwStartDATACacheSize);
        g_dwEndDATACacheSize = NearestPowerOfTwo(g_dwEndDATACacheSize);
       }   
    }
  
    g_fWriteThrough = cmdLine.GetOpt(_T("WriteThrough"));
    //g_fFATCache = cmdLine.GetOpt(_T("FatCache"));
    //g_fDATACache = cmdLine.GetOpt(_T("DataCache"));
    g_fSetFilePointerEOF = cmdLine.GetOpt(_T("PreAllocateFile"));
    g_fTFAT = cmdLine.GetOpt(_T("TFAT"));
    return true;
}

bool ParseRange(TCHAR* textRange,DWORD* Start,DWORD* End)
{
   // g_pKato->Log(LOG_DETAIL, _T("textRange = %s\n"),textRange);
    int dash = _tcscspn(textRange,TEXT("_"));
    if(dash == 0)
      return false;
    TCHAR start[MAX_PATH],end[MAX_PATH];
    _tcsncpy(start, textRange,   dash);
    start[dash] = '\0';
    TCHAR* stopString;
    _tcscpy(end, textRange + dash + 1);
    //g_pKato->Log(LOG_DETAIL, _T("END = %s, dash = %d"),end,dash);
    *Start = _tcstol(start,&stopString,10);
    *End = _tcstol(end,&stopString,10);
    if((*End == 0 && end[0] != '0') || (*Start == 0 && start[0] != '0'))
      return false;
    //g_pKato->Log(LOG_DETAIL, _T("Start: %d\tEnd:%d\n."),*Start,*End);
            
    return true;
}