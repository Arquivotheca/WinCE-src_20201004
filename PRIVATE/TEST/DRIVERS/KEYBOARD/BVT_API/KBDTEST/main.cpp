/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1998-2000  Microsoft Corporation.  All Rights Reserved.

Module Name:

     main.cpp  

Abstract:
 
    This is were the ShellProc function is defined and were
    all testing starts.
 --*/

#include <windows.h>
#include <katoex.h>
#include "global.h"
#include "main.h"

// shell and log vars
SPS_SHELL_INFO          g_spsShellInfo;
CKato                   *g_pKato = NULL;
LPFUNCTION_TABLE_ENTRY  g_lpNewFTE = NULL;
static BOOL             bPassedAll = TRUE;

// dummy main
BOOL  WINAPI DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved) {return 1;}

//******************************************************************************
//***** ShellProc
//******************************************************************************
/*++
 
ShellProc:
 
	This function provides the interface with the tux shell
 
Arguments:
 
	Standard Tux Shell Arguments
 
Return Value:
 
	Standard Tux Shell return codes
--*/

SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam) 
{
    static BOOL bExit = FALSE;
    int         iRtn;
    
   switch (uMsg) {
   case SPM_REGISTER:

        
        ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
         #ifdef UNICODE
            return SPR_HANDLED | SPF_UNICODE;
         #else
            return SPR_HANDLED;
         #endif
        break;
      
   case SPM_LOAD_DLL:
      g_pKato = (CKato *)KatoGetDefaultObject();
      break;
      
   case SPM_UNLOAD_DLL:
      if( g_lpNewFTE ) delete g_lpNewFTE;
      break;
      
   case SPM_START_SCRIPT:
   case SPM_STOP_SCRIPT:
      break;
      
   case SPM_SHELL_INFO:
      g_spsShellInfo = *(LPSPS_SHELL_INFO)spParam;
      globalInst     = g_spsShellInfo.hLib;
      KatoDebug(1, KATO_MAX_VERBOSITY,KATO_MAX_VERBOSITY,KATO_MAX_LEVEL); 
      break;
      
   case SPM_BEGIN_GROUP:
      bPassedAll = TRUE;
      g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: ******* Keyboard BVTs *******"));
      initAll();
      break;
      
   case SPM_END_GROUP:
        if( bPassedAll )
        {
            MessageBox( NULL, 
                        TEXT("All Tests were completed without detecting an error."),
                        TEXT("Final Results"),
                        MB_APPLMODAL | MB_OK | MB_ICONINFORMATION );
            g_pKato->EndLevel(TEXT("END GROUP SUCCESS: ******* Keyboard BVTs *******"));
        }            
        else
        {
            MessageBox( NULL, 
                        TEXT("Errors were detected during the tests."),
                        TEXT("Final Results"),
                        MB_APPLMODAL | MB_OK | MB_ICONERROR );
            g_pKato->EndLevel(TEXT("END GROUP WITH FAILURES: ******* Keyboard BVTs *******"));        
        }            
        
        closeAll(); // free up the testing UI
        bExit = FALSE;
        break;
      
    case SPM_BEGIN_TEST:
        if( bExit )
        {
            g_pKato->Log( LOG_SKIP, TEXT("%s"), 
                  ((LPSPS_BEGIN_TEST)spParam)->lpFTE->lpDescription );
            return SPR_SKIP;
        } // end if( bExit )
        
        /* ----------------------------------------------------------------
        	For each none instruction test prompt before continuing.
        ---------------------------------------------------------------- */
        if( ((LPSPS_BEGIN_TEST)spParam)->lpFTE->lpTestProc != Instructions_T )
        {
            iRtn = MessageBox( globalHwnd, 
                               ((LPSPS_BEGIN_TEST)spParam)->lpFTE->lpDescription,
                               TEXT("Continue testing with..."),
                               MB_APPLMODAL | MB_ICONQUESTION | MB_YESNOCANCEL );
                               
            switch( iRtn )
            {
            case IDYES:
                break;
            case IDNO:
                return SPR_SKIP;
            case IDCANCEL:
                bExit = TRUE;
                return SPR_SKIP;
            }
            
        } // end if not Instructions_T test

        g_pKato->BeginLevel(((LPSPS_BEGIN_TEST)spParam)->lpFTE->dwUniqueID, 
                          TEXT("BEGIN TEST: <%s>, Thds=%u"),((LPSPS_BEGIN_TEST)spParam)->lpFTE->lpDescription,
                          ((LPSPS_BEGIN_TEST)spParam)->dwThreadCount);

        break;
      
   case SPM_END_TEST:
      g_pKato->EndLevel(TEXT("END TEST: <%s>"),((LPSPS_END_TEST)spParam)->lpFTE->lpDescription);
      break;
      
   case SPM_EXCEPTION:
	   g_pKato->Log(LOG_DETAIL,TEXT("EXCEPTIONS -RAjendran"));
	   return 0;
  //    g_pKato->Log(LOG_EXCEPTION, TEXT("Exception occurred!"));
      break;
      
   default:
      return SPR_NOT_HANDLED;
   }
   
   return SPR_HANDLED;
}

//******************************************************************************
//   getCode: Determine the highest level log sent to Kato (where failure is highest)
TESTPROCAPI getCode(void) 
{
    int nCount;

    for(int i = 0; i < 15; i++) 
    {
        nCount = g_pKato->GetVerbosityCount((DWORD)i);
        if( nCount > 0)
        {
            switch(i) 
            {
            case LOG_EXCEPTION:      
                return TPR_HANDLED;
            case LOG_FAIL:  
                bPassedAll = FALSE;
                return TPR_FAIL;
            case LOG_WARN:
                return TPR_PASS;
            case LOG_ABORT:
                return TPR_ABORT;
            case LOG_SKIP:           
                return TPR_SKIP;
            case LOG_NOT_IMPLEMENTED:
                return TPR_HANDLED;
            case LOG_PASS:           
                return TPR_PASS;
            case LOG_DETAIL:
                return TPR_PASS;
            default:
                return TPR_PASS;
            }

        }

    }
  
    return TPR_PASS;
    
}

         
           
           
          


