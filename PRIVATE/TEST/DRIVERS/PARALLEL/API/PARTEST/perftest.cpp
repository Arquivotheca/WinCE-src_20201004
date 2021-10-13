/* -------------------------------------------------------------------------------
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1997 - 2000  Microsoft Corporation.  All Rights Reserved.
  
    Module Name:

        testproc.cpp
     
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
