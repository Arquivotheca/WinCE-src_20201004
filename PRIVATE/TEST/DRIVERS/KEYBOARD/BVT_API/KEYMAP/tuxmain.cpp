/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1998  Microsoft Corporation.  All Rights Reserved.

Module Name:

     tuxmain.cpp  
 
Abstract:
 
	This is file contains the the function needed to set up the
	TUX test enviroment.

 
--*/
#include <windows.h>
#include <tchar.h>
#include <tux.h>
#include <katoex.h>
#include "errmacro.h"
//********************************************************************************************************

extern TESTPROCAPI testUSKeyMapping( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );

extern TESTPROCAPI testJapKeyMapping( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE ); //a-rajtha
//extern TESTPROCAPI testJapCapsKeyMapping( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );

extern TESTPROCAPI Instructions_T( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE ); //a-rajtha


FUNCTION_TABLE_ENTRY g_lpFTE[] = {
#define BVT_BASE 10

    { TEXT("Keyboard Mapping Test"          ), 0,   0,  0,              NULL },
    { TEXT("English Keymap"                 ), 1,   0,  BVT_BASE+40,	testUSKeyMapping },
//	{ TEXT("Japanese Keymap"                ), 1,   0,  BVT_BASE+50,    testJapKeyMapping},
    { NULL,                                    0,   0,  0,              NULL }  // marks end of list
};


//********************************************************************************************************


#ifdef      UNDER_CE
#define     DebugLog   NKDbgPrintfW
#else
#include    <stdio.h>
#define     DebugLog   printf
#endif

static const LPTSTR cszThisFile = TEXT("TUXMAIN.CPP");
static const LPTSTR cszThisTest = TEXT("Keyboard Mapping Test");


#define     gcszThisFile    cszThisFile




// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
CKato *g_pKato = NULL;

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO  g_spsShellInfo;
BOOL            g_fAbortTesting = FALSE;


//#define DEBUG_DLLMAIN
#ifdef  DEBUG_DLLMAIN
 
BOOL WINAPI DllMain(HANDLE hModule, DWORD dwReason, LPVOID lpRes)
{
   switch ( dwReason )
   {
      case DLL_PROCESS_ATTACH:
         DebugLog( TEXT("%s: DLL_PROCESS_ATTACH, hModule = 0x%lx\r\n"), gcszThisFile, hModule);
         break;

      case DLL_THREAD_ATTACH:
         DebugLog( TEXT("%s: DLL_THREAD_ATTACH, hModule = 0x%lx\r\n"), gcszThisFile, hModule);
         break;

      case DLL_PROCESS_DETACH:
         DebugLog( TEXT("%s: DLL_PROCESS_DETACH, hModule = 0x%lx\r\n"), gcszThisFile, hModule);
         break;

      case DLL_THREAD_DETACH:
         DebugLog( TEXT("%s: DLL_THREAD_DETACH, hModule = 0x%lx\r\n"), gcszThisFile, hModule);
         break;

      default:
         DebugLog( TEXT("%s: DllMain default case -- should never happen\r\n"), gcszThisFile, hModule);
         return FALSE;
   }
   return TRUE;
}

#else

BOOL WINAPI DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved) {
   return TRUE;
} // end BOOL WINAPI DllMain

#endif

/*++
 
ShellProc:
 
	This function provides the interface with the tux shell
 
Arguments:
 
	Standard Tux Shell Arguments
 
Return Value:
 
	Standard Tux Shell return codes
 


    Unknown (unknown) 
 
Notes:

    Addapted 1/23/97 by Uknown (unknown)
 
--*/

//#define DEBUG_SHELLPROC
#ifdef  DEBUG_SHELLPROC

LPTSTR          ShellMsg2String ( LPTSTR lpMsg, UINT uMsg );
LPTSTR          ShellRet2String ( LPTSTR lpRet, UINT iRet );
SHELLPROCAPI    DebugShellProc  ( UINT uMsg, SPPARAM spParam );

SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam) 
{
    INT iRet;

    DebugLog( TEXT("%s: ENTER ShellProc(), uMsg=%u (%s), spParam=0x%08X.\n\r"),
        gcszThisFile,
        uMsg,
        ShellMsg2String(NULL,uMsg),
        spParam
        );

    iRet = DebugShellProc( uMsg, spParam );

    DebugLog( TEXT("%s: LEAVE ShellProc(), uMsg=%u (%s), spParam=0x%08X, iRet=%u (%s).\n\r"),
        gcszThisFile,
        uMsg,
        ShellMsg2String(NULL,uMsg),
        spParam,
        iRet,
        ShellRet2String(NULL,iRet)
        );

    return iRet;
}

LPTSTR  ShellMsg2String( LPTSTR lpMsg, UINT uMsg )
{
    LPTSTR lpStr;

    switch (uMsg) 
    {
        case SPM_LOAD_DLL:      lpStr = TEXT("SPM_LOAD_DLL    ");    break;
        case SPM_UNLOAD_DLL:    lpStr = TEXT("SPM_UNLOAD_DLL  ");    break;
        case SPM_SHELL_INFO:    lpStr = TEXT("SPM_SHELL_INFO  ");    break;
        case SPM_REGISTER:      lpStr = TEXT("SPM_REGISTER    ");    break;
        case SPM_START_SCRIPT:  lpStr = TEXT("SPM_START_SCRIPT");    break;
        case SPM_STOP_SCRIPT:   lpStr = TEXT("SPM_STOP_SCRIPT ");    break;
        case SPM_BEGIN_GROUP:   lpStr = TEXT("SPM_BEGIN_GROUP ");    break;
        case SPM_END_GROUP:     lpStr = TEXT("SPM_END_GROUP   ");    break;
        case SPM_BEGIN_TEST:    lpStr = TEXT("SPM_BEGIN_TEST  ");    break;
        case SPM_END_TEST:      lpStr = TEXT("SPM_END_TEST    ");    break;
        case SPM_EXCEPTION:     lpStr = TEXT("SPM_EXCEPTION   ");    break;
        default:                lpStr = TEXT("UNKNOWN         ");    break;
   }

   if ( lpMsg )
   {
        _tcscpy( lpMsg, lpStr );
        lpStr = lpMsg;
   }

   return lpStr;
}

LPTSTR  ShellRet2String( LPTSTR lpRet, UINT iRet )
{
    LPTSTR lpStr;

    switch (iRet) 
    {
        case SPR_NOT_HANDLED:  lpStr = TEXT("SPR_NOT_HANDLED ");    break;
        case SPR_HANDLED    :  lpStr = TEXT("SPR_HANDLED     ");    break;
        case SPR_SKIP       :  lpStr = TEXT("SPR_SKIP        ");    break;
        case SPR_FAIL       :  lpStr = TEXT("SPR_FAIL        ");    break;
        default:               lpStr = TEXT("UNKNOWN         ");    break;
   }

   if ( lpRet )
   {
        _tcscpy( lpRet, lpStr );
        lpStr = lpRet;
   }

   return lpStr;
}

SHELLPROCAPI DebugShellProc(UINT uMsg, SPPARAM spParam) 
{
#else

SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam) 
{
#endif

    BOOL    fRtn    = FALSE;
    
    switch (uMsg) 
    {

    case SPM_LOAD_DLL:

        #ifdef UNICODE
        ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
        #endif
       
        /* ----------------------------------------------------------------
            Get a kato object to do system logging	
        ---------------------------------------------------------------- */
        /*
        *   NOTE: Kato excepts when using new (handled)
        */
//        g_pKato = new CKato( cszThisTest );
        g_pKato = (CKato*)KatoGetDefaultObject();

        if( NULL == g_pKato )
        {
            OutputDebugString( TEXT("FATIAL ERROR: Couldn't get Kato Logging Object") );
            return SPR_FAIL;

        } // end if( NULL == g_pKato )

        //KatoDebug(1, KATO_MAX_VERBOSITY,KATO_MAX_VERBOSITY,KATO_MAX_LEVEL);

        g_pKato->Log( LOG_DETAIL, 
                      TEXT("In %s @ line %d:  %s built on %hs @ %hs ****"), 
                      cszThisFile, __LINE__, 
                      cszThisTest, __DATE__, __TIME__ );

        return SPR_HANDLED;

    case SPM_UNLOAD_DLL:
        return SPR_HANDLED;

    case SPM_SHELL_INFO:
        g_spsShellInfo = *(LPSPS_SHELL_INFO)spParam;
       
        return SPR_HANDLED;

    case SPM_REGISTER:

        ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
       
        return SPR_HANDLED;

      case SPM_START_SCRIPT:
         return SPR_HANDLED;

      case SPM_STOP_SCRIPT:
         return SPR_HANDLED;

      case SPM_BEGIN_GROUP:
         g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: %s"), cszThisTest );

         return SPR_HANDLED;

      case SPM_END_GROUP:
         g_pKato->EndLevel(TEXT("END GROUP: %s"), cszThisTest );
         
         return SPR_HANDLED;

      case SPM_BEGIN_TEST:

         /* ---------------------------------------------------------------
         	if testing aborted skip all tests.
         --------------------------------------------------------------- */
         g_pKato->BeginLevel(((LPSPS_BEGIN_TEST)spParam)->lpFTE->dwUniqueID, 
                                TEXT("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
                                ((LPSPS_BEGIN_TEST)spParam)->lpFTE->lpDescription,
                                ((LPSPS_BEGIN_TEST)spParam)->dwThreadCount,
                                ((LPSPS_BEGIN_TEST)spParam)->dwRandomSeed);

         if( g_fAbortTesting ) return SPR_SKIP;
                                
                                         
         return SPR_HANDLED;

        case SPM_END_TEST:

            g_pKato->EndLevel( TEXT("END TEST: \"%s\" result == %d"),
                                   ((LPSPS_END_TEST)spParam)->lpFTE->lpDescription,
                                   ((LPSPS_END_TEST)spParam)->dwResult );

            if( TPR_ABORT == ((LPSPS_END_TEST)spParam)->dwResult )
            {
                g_pKato->Log( LOG_DETAIL, TEXT("Aborting on %d == %d"),
                              TPR_ABORT, ((LPSPS_END_TEST)spParam)->dwResult );
                g_fAbortTesting = TRUE;

            }

         return SPR_HANDLED;

      case SPM_EXCEPTION:
         g_pKato->Log(LOG_EXCEPTION, TEXT("Exception occurred!"));
         return SPR_HANDLED;
   }

   return SPR_NOT_HANDLED;

} // end ShellProc( ... )
