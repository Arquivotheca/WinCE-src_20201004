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
// Video detector for TestKit
//
//

#include "video_detector.h"

#define __BIN_NAME__    TEXT( "video_detector.dll")

CKato *g_pKato = NULL;
SPS_SHELL_INFO *g_pShellInfo;

TESTPROCAPI DetectorTest( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY ); 


// --------------------------------------------------------------------
// TUX Function table
//

extern "C" {
    FUNCTION_TABLE_ENTRY g_lpFTE[] = {
    TEXT( "Video Detector" ), 0, 0, BVT_BASE+0, DetectorTest,
    {NULL, 0, 0, 0, NULL}
    };
}


//
// IsTechPresent
//
// This helper function should be updated with whatever code is needed to 
// actually detect if your technology is present.
//
// Called By:
//        DetectorTest
//
// Return Values:
//        TRUE    tech was detected
//        FALSE    tech is not detected
//

BOOL IsTechPresent(void)
{
    BOOL bTechPresent = FALSE;
    //void *pCoInitializeEx, *pCoCreateInstance, *pCoUninitialize;

    HINSTANCE hOle32 = LoadLibrary( L"ole32.dll" );
    if ( hOle32 )
    {
        PFNCOINITIALIZEEX pfnCoInitializeEx = (PFNCOINITIALIZEEX)GetProcAddress( hOle32, L"CoInitializeEx" );
        PFNCOCREATEINSTANCE pfnCoCreateInstance = (PFNCOCREATEINSTANCE)GetProcAddress( hOle32, L"CoCreateInstance" );
        PFNCOUNINITIALIZE pfnCoUninitialize = (PFNCOUNINITIALIZE)GetProcAddress( hOle32, L"CoUninitialize" );
        if ( ( pfnCoInitializeEx ) && ( pfnCoCreateInstance ) && ( pfnCoUninitialize ) )
        {
            pfnCoInitializeEx(NULL, COINIT_MULTITHREADED);
            IGraphBuilder *pGraph = NULL;
            if ( pfnCoCreateInstance( CLSID_FilterGraph, NULL, CLSCTX_INPROC, IID_IGraphBuilder, (void **)&pGraph ) == S_OK )
            {
                bTechPresent = TRUE;
                pGraph->Release();
            }
            else
            {
                g_pKato->Log( LOG_DETAIL, _T("Error %d in CoCreateInstance for CLSID_FilterGraph"), GetLastError() );
            }
            pfnCoUninitialize();
        }
        else
        {
            g_pKato->Log( LOG_DETAIL, _T("Failed to GetProcAddress in ole32.dll") );
        }
        FreeLibrary( hOle32 );
    }
    else
    {
        g_pKato->Log( LOG_DETAIL, _T("Error %d in LoadLibrary for ole32.dll"), GetLastError() );
    }

    // inform the logs if we found the technology
    if( bTechPresent )
        DETAIL("Successfully detected Video");
    else
        WARN("Did not detect Video");

    return bTechPresent;
}


//
// DetectorTest
//
// This test is called by TUX directly and so must handle the standard TUX messages.  
//
// To make things as simple as possible we suggest putting any acutal detection code in 
// the support function IsTechPresent.
//
// No changes are required to this function.
//
// Returns:
//        TPR_PASS    if tech was detected
//        TPR_FAIL    if tech was not detected
//

TESTPROCAPI DetectorTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT TP_Status = TPR_FAIL;

    // skip any call to us that is not a request to execute
    NOT_EXECUTE(uMsg);
  
    DETAIL("Detecting Video...");
    if(IsTechPresent())
        TP_Status = TPR_PASS;

    return TP_Status;
}


//
// ShellProc
//
// Standard simplified shellproc for supporting calls from TUX
//
// This function should need no modification.
//

SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam) 
{
    switch (uMsg) 
    {
        case SPM_LOAD_DLL: 
        {
            ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
            // Get/Create our global logging object.
            g_pKato = (CKato*)KatoGetDefaultObject();
            return SPR_HANDLED;
        }

        case SPM_SHELL_INFO: 
        {        
            // Store a pointer to our shell info for later use.
            g_pShellInfo = (LPSPS_SHELL_INFO)spParam;
            if( g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine )
            {
                // Display our Dlls command line if we have one.
                g_pKato->Log( LOG_DETAIL, _T("Command Line: \"%s\"."), g_pShellInfo->szDllCmdLine);
            } 
            // get the global pointer to our HINSTANCE
            //g_hInstance = g_pShellInfo->hLib;
            return SPR_HANDLED;
        }

        case SPM_REGISTER: 
        {
            ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
            return SPR_HANDLED | SPF_UNICODE;
        }

        case SPM_UNLOAD_DLL: 
        case SPM_START_SCRIPT: 
        case SPM_STOP_SCRIPT: 
            return SPR_HANDLED;


        case SPM_BEGIN_GROUP: 
        {
            g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: %s"), __BIN_NAME__);
            return SPR_HANDLED;
        }

        case SPM_END_GROUP: 
        {
           g_pKato->EndLevel(TEXT("END GROUP: %s"), __BIN_NAME__);
           return SPR_HANDLED;
        }

        case SPM_BEGIN_TEST: 
        {
           // Start our logging level.
            LPSPS_BEGIN_TEST pBT = (LPSPS_BEGIN_TEST)spParam;
            g_pKato->BeginLevel(pBT->lpFTE->dwUniqueID, 
                                TEXT("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
                                pBT->lpFTE->lpDescription, pBT->dwThreadCount,
                                pBT->dwRandomSeed);
           return SPR_HANDLED;
        }

        case SPM_END_TEST: 
        {
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

        case SPM_EXCEPTION: 
        {
            g_pKato->Log(LOG_EXCEPTION, TEXT("Exception occurred in %s!"),__WFILE__);
            return SPR_HANDLED;
        }
    }
    return SPR_NOT_HANDLED;
}
