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
////////////////////////////////////////////////////////////////////////////////
//
//  TUXTEST TUX DLL
//  Copyright (c) Microsoft Corporation
//
//  Module: globals.cpp
//          Causes the header globals.h to actually define the global variables.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"

// By defining the symbol __GLOBALS_CPP__, we force the file globals.h to
// define, instead of declare, all global variables.
#define __GLOBALS_CPP__
#include "globals.h"

static BOOL             bPassedAll = TRUE;

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

         
           

