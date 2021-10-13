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
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------

#include "..\globals.h"
#include "..\winmain.h"

CKato *g_pKato = NULL;
SPS_SHELL_INFO *g_pShellInfo;

TESTPROCAPI TouchDriverDetectionTest( UINT uMsg, 
                                                            TPPARAM tpParam, 
                                                                           LPFUNCTION_TABLE_ENTRY lpFTE );

// --------------------------------------------------------------------
// TUX Function table
extern "C" {
    FUNCTION_TABLE_ENTRY g_lpFTE[] = {
        TEXT("Touch Driver Detection Test" ), 1,  0,  DETECTOR_BASE+ 1, TouchDriverDetectionTest,   
        {NULL, 0, 0, 0, NULL}
    };
}

//-----------------------------------------------------------------------------
//  TouchDriverDetectionTest 
//  Searching registy key to find touch driver
//  Test passes if touch driver registry key is found
// ---------------------------------------------------------------------------- 
TESTPROCAPI TouchDriverDetectionTest( 
                      UINT uMsg, 
                      TPPARAM tpParam, 
                      LPFUNCTION_TABLE_ENTRY lpFTE )

{    

    if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }

    int driverVersion = NO_TOUCH;

    driverVersion =  TouchDriverVersion();

    if ( ( driverVersion == CE7) || ( driverVersion == CE6))
    {
        // found touch driver
        return TPR_PASS;
    }
    else
    {
        return TPR_FAIL;
    }
}
        
    
// --------------------------------------------------------------------
SHELLPROCAPI 
ShellProc(
    UINT uMsg, 
    SPPARAM spParam) 
{
    switch (uMsg) 
    {
        case SPM_LOAD_DLL: 
        {
            // If we are UNICODE, then tell Tux this by setting the following flag.
            #ifdef UNICODE
                ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
            #endif

            // Get/Create our global logging object.
            g_pKato = (CKato*)KatoGetDefaultObject();            
            
            return SPR_HANDLED;
        }

        case SPM_UNLOAD_DLL: 
        {               
            return SPR_HANDLED;
        }

        case SPM_SHELL_INFO: 
        {        
         
            // Store a pointer to our shell info for later use.
            g_pShellInfo = (LPSPS_SHELL_INFO)spParam;

            if( g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine )
            {
                // Display our Dlls command line if we have one.
                g_pKato->Log( LOG_DETAIL, 
                    _T("Command Line: \"%s\"."), g_pShellInfo->szDllCmdLine);
            } 

            // get the global pointer to our HINSTANCE
            g_hInstance = g_pShellInfo->hLib;                        

            return SPR_HANDLED;
        }

        case SPM_REGISTER: 
        {
            ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
            #ifdef UNICODE
                return SPR_HANDLED | SPF_UNICODE;
            #else
                return SPR_HANDLED;
            #endif
        }

        case SPM_START_SCRIPT: 
        {           
            return SPR_HANDLED;
        }

        case SPM_STOP_SCRIPT: 
        {
            return SPR_HANDLED;
        }

        case SPM_BEGIN_GROUP: 
        {
        
            return SPR_HANDLED;
        }

        case SPM_END_GROUP: 
        {
              
            return SPR_HANDLED;
        }

        case SPM_BEGIN_TEST: 
        {
            return SPR_HANDLED;
        }

        case SPM_END_TEST: 
        {
                        
            return SPR_HANDLED;
        }

        case SPM_EXCEPTION: 
        {
            g_pKato->Log(LOG_EXCEPTION, TEXT("Exception occurred!"));
            return SPR_HANDLED;
        }
    }

    return SPR_NOT_HANDLED;

}

