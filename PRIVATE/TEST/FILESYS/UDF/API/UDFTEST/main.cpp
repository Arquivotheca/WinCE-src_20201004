//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
#include "main.h"
#include "globals.h"
#include <pathtable.h>

#undef __FILE_NAME__
#define __FILE_NAME__   TEXT("MAIN.CPP")

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
CKato *g_pKato = NULL;

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO *g_pShellInfo;

// Global critical section to be used by threaded tests if necessary.
CRITICAL_SECTION g_csProcess;

// Global command line option flags
BOOL g_fCheckData = FALSE;

// Global PathTable structure
PathTable *g_pPathTable = NULL;

// Global path to disc
TCHAR g_szDiscPath[MAX_PATH];

// internal function declarations
static BOOL SetTestOptions( LPCTSTR );

//******************************************************************************
//***** Windows CE specific definitions 
//******************************************************************************
#ifdef UNDER_CE

#ifdef __cplusplus
extern "C" {
#endif

BOOL WINAPI DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved) {
   UNREFERENCED_PARAMETER(hInstance);
   UNREFERENCED_PARAMETER(dwReason);
   UNREFERENCED_PARAMETER(lpReserved);
   return TRUE;
}

#ifdef __cplusplus
}
#endif

#endif // UNDER_CE

//******************************************************************************
//***** ShellProc()
//******************************************************************************

SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam) {

   switch (uMsg) {

      //------------------------------------------------------------------------
      // Message: SPM_LOAD_DLL
      //
      // Sent once to the DLL immediately after it is loaded.  The spParam 
      // parameter will contain a pointer to a SPS_LOAD_DLL structure.  The DLL
      // should set the fUnicode member of this structre to TRUE if the DLL is
      // built with the UNICODE flag set.  By setting this flag, Tux will ensure
      // that all strings passed to your DLL will be in UNICODE format, and all
      // strings within your function table will be processed by Tux as UNICODE.
      // The DLL may return SPR_FAIL to prevent the DLL from continuing to load.
      //------------------------------------------------------------------------

      case SPM_LOAD_DLL: {
         Debug(TEXT("ShellProc(SPM_LOAD_DLL, ...) called"));

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
      }

      //------------------------------------------------------------------------
      // Message: SPM_UNLOAD_DLL
      //
      // Sent once to the DLL immediately before it is unloaded.
      //------------------------------------------------------------------------

      case SPM_UNLOAD_DLL: {
         Debug(TEXT("ShellProc(SPM_UNLOAD_DLL, ...) called"));

         // This is a good place to delete our global critical section.
         DeleteCriticalSection(&g_csProcess);

         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_SHELL_INFO
      //
      // Sent once to the DLL immediately after SPM_LOAD_DLL to give the DLL
      // some useful information about its parent shell and environment.  The
      // spParam parameter will contain a pointer to a SPS_SHELL_INFO structure.
      // The pointer to the structure may be stored for later use as it will
      // remain valid for the life of this Tux Dll.  The DLL may return SPR_FAIL
      // to prevent the DLL from continuing to load.
      //------------------------------------------------------------------------

      case SPM_SHELL_INFO: {
        Debug(TEXT("ShellProc(SPM_SHELL_INFO, ...) called"));

        // set the gloabl path to the CD drive; this may be overwritten by
        // SetTestOptions when the command line is processed
        _tcscpy( g_szDiscPath, CD_DRIVE_STRING );
        
        // Store a pointer to our shell info for later use.
        g_pShellInfo = (LPSPS_SHELL_INFO)spParam;
        
        if( g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine )
        {
            // Display our Dlls command line if we have one.
            g_pKato->Log( LOG_DETAIL, 
                TEXT("Command Line: \"%s\"."), g_pShellInfo->szDllCmdLine);

            //
            // set the test options based on the command line
            //
            if( !SetTestOptions( g_pShellInfo->szDllCmdLine ) )
            {
                g_pKato->Log( LOG_FAIL, 
                              TEXT("FAIL in %s @ line %d: Couldn't set test options"),
                              __FILE_NAME__, __LINE__ );
                return SPR_FAIL;
            
            } // end if( NULL == g_pKato );
        }

        return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_REGISTER
      //
      // This is the only ShellProc() message that a DLL is required to handle
      // (except for SPM_LOAD_DLL if you are UNICODE).  This message is sent
      // once to the DLL immediately after the SPM_SHELL_INFO message to query
      // the DLL for it’s function table.  The spParam will contain a pointer to
      // a SPS_REGISTER structure.  The DLL should store its function table in
      // the lpFunctionTable member of the SPS_REGISTER structure.  The DLL may
      // return SPR_FAIL to prevent the DLL from continuing to load.
      //------------------------------------------------------------------------

      case SPM_REGISTER: {
         Debug(TEXT("ShellProc(SPM_REGISTER, ...) called"));
         ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
         #ifdef UNICODE
            return SPR_HANDLED | SPF_UNICODE;
         #else
            return SPR_HANDLED;
         #endif
      }

      //------------------------------------------------------------------------
      // Message: SPM_START_SCRIPT
      //
      // Sent to the DLL immediately before a script is started.  It is sent to
      // all Tux DLLs, including loaded Tux DLLs that are not in the script.
      // All DLLs will receive this message before the first TestProc() in the
      // script is called.
      //------------------------------------------------------------------------

      case SPM_START_SCRIPT: {
         Debug(TEXT("ShellProc(SPM_START_SCRIPT, ...) called"));
         
         // create and build the global pathtable pointer
         g_pPathTable = new PathTable();
         if( NULL == g_pPathTable )
         {
            SystemErrorMessage( GetLastError() );
            FAIL("Unable to create path table; out of memory");
            return TPR_FAIL;
         }

         if( 0 == g_pPathTable->BuildTable(g_szDiscPath) )
         {
            FAIL("Unable to find media");
            delete g_pPathTable;
            g_pPathTable = NULL;
            return TPR_FAIL;
         }
         
         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_STOP_SCRIPT
      //
      // Sent to the DLL when the script has stopped.  This message is sent when
      // the script reaches its end, or because the user pressed stopped prior
      // to the end of the script.  This message is sent to all Tux DLLs,
      // including loaded Tux DLLs that are not in the script.
      //------------------------------------------------------------------------

      case SPM_STOP_SCRIPT: {
         Debug(TEXT("ShellProc(SPM_STOP_SCRIPT, ...) called"));
         
         if( g_pPathTable)
            delete g_pPathTable;

         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_BEGIN_GROUP
      //
      // Sent to the DLL before a group of tests from that DLL is about to be
      // executed.  This gives the DLL a time to initialize or allocate data for
      // the tests to follow.  Only the DLL that is next to run receives this
      // message.  The prior DLL, if any, will first receive a SPM_END_GROUP
      // message.  For global initialization and de-initialization, the DLL
      // should probably use SPM_START_SCRIPT and SPM_STOP_SCRIPT, or even
      // SPM_LOAD_DLL and SPM_UNLOAD_DLL.
      //------------------------------------------------------------------------

      case SPM_BEGIN_GROUP: {
         Debug(TEXT("ShellProc(SPM_BEGIN_GROUP, ...) called"));
         g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: TUXDEMO.DLL"));
         
         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_END_GROUP
      //
      // Sent to the DLL after a group of tests from that DLL has completed
      // running.  This gives the DLL a time to cleanup after it has been run.
      // This message does not mean that the DLL will not be called again to run
      // tests; it just means that the next test to run belongs to a different
      // DLL.  SPM_BEGIN_GROUP and SPM_END_GROUP allow the DLL to track when it
      // is active and when it is not active.
      //------------------------------------------------------------------------

      case SPM_END_GROUP: {
         Debug(TEXT("ShellProc(SPM_END_GROUP, ...) called"));
         g_pKato->EndLevel(TEXT("END GROUP: TUXDEMO.DLL"));
            
         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_BEGIN_TEST
      //
      // Sent to the DLL immediately before a test executes.  This gives the DLL
      // a chance to perform any common action that occurs at the beginning of
      // each test, such as entering a new logging level.  The spParam parameter
      // will contain a pointer to a SPS_BEGIN_TEST structure, which contains
      // the function table entry and some other useful information for the next
      // test to execute.  If the ShellProc function returns SPR_SKIP, then the
      // test case will not execute.
      //------------------------------------------------------------------------

      case SPM_BEGIN_TEST: {
         Debug(TEXT("ShellProc(SPM_BEGIN_TEST, ...) called"));

         // Start our logging level.
         LPSPS_BEGIN_TEST pBT = (LPSPS_BEGIN_TEST)spParam;
         g_pKato->BeginLevel(pBT->lpFTE->dwUniqueID, 
                             TEXT("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
                             pBT->lpFTE->lpDescription, pBT->dwThreadCount,
                             pBT->dwRandomSeed);

         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_END_TEST
      //
      // Sent to the DLL after a single test executes from the DLL.  This gives
      // the DLL a time perform any common action that occurs at the completion
      // of each test, such as exiting the current logging level.  The spParam
      // parameter will contain a pointer to a SPS_END_TEST structure, which
      // contains the function table entry and some other useful information for
      // the test that just completed.
      //------------------------------------------------------------------------

      case SPM_END_TEST: {
         Debug(TEXT("ShellProc(SPM_END_TEST, ...) called"));

         // End our logging level.
         LPSPS_END_TEST pET = (LPSPS_END_TEST)spParam;
         g_pKato->EndLevel(TEXT("END TEST: \"%s\", %s, Time=%u.%03u"),
                           pET->lpFTE->lpDescription,
                           pET->dwResult == TPR_SKIP ? TEXT("SKIPPED") :
                           pET->dwResult == TPR_PASS ? TEXT("PASSED") :
                           pET->dwResult == TPR_FAIL ? TEXT("FAILED") : TEXT("ABORTED"),
                           pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000);

         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_EXCEPTION
      //
      // Sent to the DLL whenever code execution in the DLL causes and exception
      // fault.  By default, Tux traps all exceptions that occur while executing
      // code inside a Tux DLL.
      //------------------------------------------------------------------------

      case SPM_EXCEPTION: {
         Debug(TEXT("ShellProc(SPM_EXCEPTION, ...) called"));
         g_pKato->Log(LOG_EXCEPTION, TEXT("Exception occurred!"));
         return SPR_HANDLED;
      }

      default: {
         ASSERT(!"Default case reached in ShellProc!");
         return SPR_NOT_HANDLED;
      }
   }
}

//******************************************************************************
TESTPROCAPI 
TestDefault(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
//******************************************************************************    
{
    UNREFERENCED_PARAMETER(lpFTE);
    
    //
    // Check the incoming message
    //
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        return SPR_HANDLED;
    }
    //
    // The only other message handled is TPM_EXECUTE
    //
    else if( TPM_EXECUTE != uMsg )
    {
        return TPR_NOT_HANDLED;
    }

    //
    //  Forward declarations
    //
    BOOL fSuccess               = TRUE;
    WIN32_FIND_DATA fileData    = { 0 };
    LPTSTR lpFileName          = TEXT("\\*");    // try to search the root directory
    HANDLE hSearch              = NULL;
    
    //
    // Try to search for the first file in the root directory
    // and get a search handle to continue the search
    //
    hSearch = FindFirstFile( lpFileName, &fileData );
    if( INVALID_HANDLE( hSearch ) )
    {
        Debug(TEXT("FindFirstFile() FAILED"));
        fSuccess = FALSE;
    }
    else
    {
        Debug(TEXT("File is named: %s"), fileData.cFileName );
        while( FindNextFile( hSearch, &fileData ) )  
            Debug(TEXT("File is named: %s"), fileData.cFileName );
    }
    //
    // Return the proper Tux code
    // 
    return fSuccess ? TPR_PASS : TPR_FAIL;
}

//*****************************************************************************
BOOL
SetTestOptions( 
    LPCTSTR szCmdLine )
//
//  Sets the test options based on the command line passed in to Tux
//
//*****************************************************************************
{
    UINT unCmdIdx = 0;
    UINT unPath = 0;    

    TCHAR szPath[MAX_PATH + 1];

    while( TEXT('\0') != szCmdLine[unCmdIdx] )
    {
        //
        // ignore anything that is not a command flag
        //
        while( TEXT('-') != szCmdLine[unCmdIdx] &&
               TEXT('\0') != szCmdLine[unCmdIdx] ) 
        {
            unCmdIdx++;
        }
        
        //
        // if the loop ended on a NULL char, then quit
        //
        if( TEXT('\0') == szCmdLine[unCmdIdx] ) break;

        switch( szCmdLine[++unCmdIdx] )
        {

        case TEXT('c'):
        case TEXT('C'):
            g_fCheckData = TRUE;
            g_pKato->Log( LOG_DETAIL, TEXT("File Checksum option set.") );
            break;

        case TEXT('p'):
        case TEXT('P'):
            while( szCmdLine[++unCmdIdx] == TEXT(' '))
                ; // eat up white space
            
            for( unPath = 0; unPath < MAX_PATH; unPath++ )
            {
                szPath[unPath] = szCmdLine[unCmdIdx++];
                    
                if( TEXT('\0') == szCmdLine[unCmdIdx] )
                {
                    break;
                }
                // require a space between command line options
                // because there might be a dash embedded in a string
                if( TEXT(' ') == szCmdLine[unCmdIdx] &&
                    TEXT('-') == szCmdLine[unCmdIdx + 1] )
                {
                   unCmdIdx++;
                   break;
                }
            }

            szPath[++unPath] = TEXT('\0');

            _tcscpy( g_szDiscPath, szPath );

            break;
            
        
        default:
            g_pKato->Log( LOG_DETAIL,
                TEXT("Invalid command line argument: -%c"), szCmdLine[unCmdIdx] );
            break;
        }
    }

    g_pKato->Log( LOG_DETAIL, TEXT("Path: %s"), g_szDiscPath );
    
    return TRUE;
}
