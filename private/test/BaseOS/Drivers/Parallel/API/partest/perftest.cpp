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
/* -------------------------------------------------------------------------------
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1997 - 2000  Microsoft Corporation.  All Rights Reserved.
  
    Module Name:

        perftest.cpp
     
    Abstract:

    Functions:


    
    Notes:


------------------------------------------------------------------------------- */

#include "main.h"
#include "globals.h"
#include "parport.h"
#include "util.h"

#ifdef UNDER_CE 
#include <pegdpar.h>
#endif

#define __FILE_NAME__ TEXT("PERFTEST.CPP")

// ----------------------------------------------------------------------------
// TestProc()'s
// ----------------------------------------------------------------------------

/* ----------------------------------------------------------------------------
   Function: TestOpenClosePort()

   Description: tests ability to read and sCE_PTOet timeout values on parallel port

   Arguments: standard TUX args

   Return Value:

      TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED
---------------------------------------------------------------------------- */
TESTPROCAPI 
TestWriteThroughput(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
{

    DWORD dwBufferSize = lpFTE->dwUserData;

    //
    // If the user supplied buffer size is too small, change it
    //
    if( 0 == dwBufferSize )
    {
        dwBufferSize = 10;
    }

    


    return TPR_PASS;
}
