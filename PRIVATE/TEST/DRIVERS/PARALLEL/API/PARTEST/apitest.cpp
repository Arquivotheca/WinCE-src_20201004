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

    TESTPROCAPI TestReadAndSetTimeouts( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
    TESTPROCAPI TestGetDeviceStatus( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
    TESTPROCAPI TestGetDeviceID( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
    TESTPROCAPI TestGetECPChannel32( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
    TESTPROCAPI ReadBytes( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
    TESTPROCAPI WriteBytes( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )

    
    Notes:

        -   Most of these functions will only work under Win CE.
        -   ReadBytes() will always fail because LPT_Read() under CE is not
            implemented to actually read anything.

------------------------------------------------------------------------------- */

#include "main.h"
#include "globals.h"
#include "parport.h"
#include "util.h"

#ifdef UNDER_CE 
#include <pegdpar.h>
#endif

#define __FILE_NAME__ TEXT("BVTTEST.CPP")

#define MAX_READ_WRITE_RETRIES  50
#define FILL_BYTE               0xA3

// ----------------------------------------------------------------------------
// TestProc()'s
// ----------------------------------------------------------------------------

/* ----------------------------------------------------------------------------
   Function: TestOpenClosePort()

   Description: tests ability to read and set timeout values on parallel port

   Arguments: standard TUX args

   Return Value:

      TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED
---------------------------------------------------------------------------- */
TESTPROCAPI 
TestOpenClosePort(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
{
    if ( uMsg != TPM_EXECUTE ) {
        return TPR_NOT_HANDLED;
    }

    PARPORT pp;
    BOOL fRet = TRUE;

    //
    // Attempt to open the parallel port
    //
    if( !pp.Open() )
    {
        fRet = FALSE;
        goto Exit;
    }
    g_pKato->Log( LOG_DETAIL, TEXT("parallel port opened successfully") );

    //
    // Attempt to close the parallel port
    //
    if( !pp.Close() )
    {
        fRet = FALSE;
        goto Exit;
    }
    g_pKato->Log( LOG_DETAIL, TEXT("parallel port closed successfully") );

Exit:
    return fRet ? TPR_PASS : TPR_FAIL;;
}    

/* ----------------------------------------------------------------------------
   Function: TestReadAndSetTimeouts()

   Description: tests ability to read and set timeout values on parallel port
                using the IOCTL codes rather than the Win32 API

   Arguments: standard TUX args

   Return Value:

      TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED
---------------------------------------------------------------------------- */
TESTPROCAPI 
TestReadAndSetTimeouts(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
{
    if ( uMsg != TPM_EXECUTE ) {
        return TPR_NOT_HANDLED;
    }

    //
    // else begin the test
    //
    PARPORT pp;
    COMMTIMEOUTS ct1, ct2;
    BOOL fRet = TRUE;
    BOOL fUseWin32API = lpFTE->dwUserData;

    //
    // open the parallel port
    //
    if( !pp.Open() )
    {
        fRet = FALSE;
        goto Exit;
    }

    // CE does not support the use of the SetCommTimeouts/GetCommTimeouts Win32 API
    if( fUseWin32API )
    {
        g_pKato->Log(LOG_DETAIL, 
            _T("SetCommTimeouts()/GetCommTimeouts() API is unsupported by the parallel port driver"));
        fRet = TRUE;
        goto Exit;
    }

    //
    // get current/default timeouts
    //
    if( !pp.GetTimeouts( &ct1, fUseWin32API ) )
    {
        fRet = FALSE;
        goto Exit;
    }

    //
    // display current timeouts 
    //
    PrintCommTimeouts( &ct1 );

    //
    // modify timeouts (the value is arbitrary)
    //
    ct1.ReadIntervalTimeout += 10;
    ct1.ReadTotalTimeoutConstant += 10;
    ct1.ReadTotalTimeoutMultiplier += 10;
    ct1.WriteTotalTimeoutConstant += 10;
    ct1.WriteTotalTimeoutMultiplier += 10;
    
    g_pKato->Log( LOG_DETAIL, TEXT("modified timeout values") );

    //
    // set the timeout values to the new modified ones
    //
    g_pKato->Log( LOG_DETAIL, TEXT("setting new comm timeout values") );
    if( !pp.SetTimeouts( &ct1, fUseWin32API ) )
    {
        fRet = FALSE;
        goto Exit;
    }

    //
    // read back the timeout values
    //
    g_pKato->Log( LOG_DETAIL, TEXT("reading back port timeout values") );
    if( !pp.GetTimeouts( &ct2, fUseWin32API ) )
    {      
        fRet = FALSE;
        goto Exit;
    }

    //
    // display new timeout values
    //
    PrintCommTimeouts( &ct2 );

    //
    // compare the timeout values with the ones attempted
    //
    g_pKato->Log( LOG_DETAIL, TEXT("comparing port timeout values") );
    if(     ct1.ReadIntervalTimeout == ct2.ReadIntervalTimeout &&
            ct1.ReadTotalTimeoutConstant == ct2.ReadTotalTimeoutConstant &&
            ct1.ReadTotalTimeoutMultiplier == ct2.ReadTotalTimeoutMultiplier &&
            ct1.WriteTotalTimeoutConstant == ct2.WriteTotalTimeoutConstant &&
            ct1.WriteTotalTimeoutMultiplier == ct2.WriteTotalTimeoutMultiplier )
    {
        g_pKato->Log( LOG_PASS, TEXT("Timeouts match") );
        goto Exit;
    }

    else
    {
        g_pKato->Log( LOG_FAIL, TEXT("Timeouts do not match") );
        fRet = FALSE;
        goto Exit;
    }

Exit:   
    pp.Close();
    return fRet ? TPR_PASS : TPR_FAIL;
}

/* ----------------------------------------------------------------------------
   Function: TestGetDeviceID()

   Description: retrieves device ID string from device connected to parallel
                port

   Arguments: standard TUX args

   Return Value: 

      TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED
---------------------------------------------------------------------------- */
TESTPROCAPI 
TestGetDeviceID(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
{
    if( TPM_EXECUTE != uMsg )
    {
        return TPR_NOT_HANDLED;
    }

    // else begin the test
    PARPORT pp;
    BOOL fRet = TRUE;
    DWORD dwID = 0;
    BYTE bBuffer[1024];
    WCHAR wcBuffer[1024];
    DWORD dwBytes = 0;
    
    if( !pp.Open() )
    {
        fRet = FALSE;
        goto Exit;
    }

    if( !pp.GetDeviceID( bBuffer, 1024, &dwBytes ) )
    {
        fRet = FALSE;
        goto Exit;
    }

    // dwID should be a null terminated char string (not Unicode)
    MultiByteToWideChar( 
        CP_ACP, 
        0,
        (PCHAR)bBuffer,
        -1, // null terminated, don't specify a length
        wcBuffer,
        1024 );
        
    g_pKato->Log( LOG_DETAIL, TEXT("Device ID: %s"), wcBuffer);

Exit:
    pp.Close();
    return fRet ? TPR_PASS : TPR_FAIL;
}

/* ----------------------------------------------------------------------------
   Function: TestGetDeviceStatus()

   Description: retrieves device status from parallel port

   Arguments: standard TUX args

   Return Value:

      TPR_PASS, TPR_FAIL, or TPR_NOT_HANDLED
---------------------------------------------------------------------------- */
TESTPROCAPI 
TestGetDeviceStatus(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
{
    if( TPM_EXECUTE != uMsg )
    {
        return TPR_NOT_HANDLED;
    }
    
    //
    // else begin the test
    //
    PARPORT pp;
    BOOL fRet = TRUE;
    DWORD dwStatus = 0;
    
    if( !pp.Open() )
    {
        fRet = FALSE;
        goto Exit;
    }
            
    if( !pp.GetStatus( &dwStatus ) )
    {
        fRet = FALSE;
        goto Exit;
    }

    g_pKato->Log( LOG_DETAIL, TEXT("Device status: 0x%x"), dwStatus );
    PrintCommStatus( dwStatus );

Exit:        
    pp.Close();
    return fRet ? TPR_PASS : TPR_FAIL;
}

/* ----------------------------------------------------------------------------
   Function: TestGetECPChannel32()

   Description: retrieves data from device attached to parallel port

   Arguments: standard TUX args

   Return Value:

      TPR_PASS, TPR_FAIL, or TPR_NOT_HANDLED
---------------------------------------------------------------------------- */
TESTPROCAPI 
TestGetECPChannel32(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
{
    if( TPM_EXECUTE != uMsg )
    {
        return TPR_NOT_HANDLED;
    }

    // else begin the test
    //
    PARPORT pp;
    BOOL fRet = TRUE;
    DWORD dwChan32 = 0;
    
    if( !pp.Open() )
    {
        fRet = FALSE;
        goto Exit;
    }
            
    if( !pp.GetECPChannel32( &dwChan32) )
    {
        fRet = FALSE;
        goto Exit;
    }

    g_pKato->Log( LOG_DETAIL, TEXT("ECP Channel 32 retrieved") );

Exit:
    pp.Close();
    return fRet ? TPR_PASS : TPR_FAIL;
}


/* ----------------------------------------------------------------------------
   Function: ReadBytes()

   Description: Continually read bytes from the parallel port

   Arguments: standard TUX args

   Return Value:

      TPR_PASS, TPR_FAIL, or TPR_NOT_HANDLED
---------------------------------------------------------------------------- */
TESTPROCAPI 
ReadBytes(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE ) 
{
    // 
    // we only handle TPM_EXECUTE messages
    //
    if( TPM_EXECUTE != uMsg )
    {
        return TPR_NOT_HANDLED;
    }
    
    PARPORT pp;
    BOOL fRet           = TRUE;
    LPBYTE lpb          = NULL;
    DWORD dwRead        = 0;
    DWORD dwStatus      = 0;
    DWORD dwCount       = MAX_READ_WRITE_RETRIES;
    DWORD dwBufferSize  = lpFTE->dwUserData;

    //
    // allocate a buffer of user-specified size for reading data
    //
    lpb = new BYTE[dwBufferSize];
    
    if( NULL == lpb )
    {
        FAIL("out of memory");
        ErrorMessage( GetLastError() );
        fRet = FALSE;
        goto Exit;
    }
    
    //
    // open the parallel port
    //
    if( !pp.Open() )
    {
        fRet = FALSE;
        goto Exit;
    }
    g_pKato->Log( LOG_DETAIL, TEXT("Parallel Port opened successfully") );

    // 
    // read bytes off of port
    //
    while((dwCount--) > 0)
    {
        if( !pp.Read(lpb, lpFTE->dwUserData, &dwRead ) )
        {
            fRet = FALSE;
            goto Exit;
        }
        g_pKato->Log( LOG_DETAIL,  TEXT("Read %d bytes from parallel port"), dwRead );

        // 
        // make sure the requested number of bytes were read
        //
        if( dwRead != dwBufferSize )
        {
            g_pKato->Log( LOG_DETAIL, TEXT("Failed to read %d bytes"), dwBufferSize );
            fRet = FALSE;
            
            //
            // check and print device status
            //
            if( !pp.GetStatus( &dwStatus ) )
            {
                g_pKato->Log( LOG_FAIL, TEXT("failed to retreive device status") );
                goto Exit;
            }
            g_pKato->Log( LOG_DETAIL, TEXT("Device status: 0x%x"), dwStatus );
            PrintCommStatus( dwStatus );
            goto Exit;
        }
    }

Exit:
    if( NULL != lpb )
    {
        delete[] lpb;
    }
    pp.Close();   
    return fRet ? TPR_PASS : TPR_FAIL;
}

/* ----------------------------------------------------------------------------
   Function: WriteBytes()

   Description: Continually send bytes to the parallel port

   Arguments: standard TUX args

   Return Value: 

      TPR_PASS, TPR_FAIL, or TPR_NOT_HANDLED
---------------------------------------------------------------------------- */
TESTPROCAPI 
WriteBytes(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // 
    // we only handle TPM_EXECUTE messages
    //
    if( TPM_EXECUTE != uMsg )
    {
        return TPR_NOT_HANDLED;
    }
    
    PARPORT pp;
    BOOL fRet           = TRUE;
    LPBYTE lpb          = NULL;
    DWORD dwWrote       = 0;
    DWORD dwStatus      = 0;
    DWORD dwCount       = MAX_READ_WRITE_RETRIES;
    DWORD dwBufferSize  = 10;
    BOOL fUseWin32API   = lpFTE->dwUserData;

    //
    // allocate a buffer of user-specified size and fill with data
    //
    lpb = new BYTE[dwBufferSize];
    if( NULL == lpb )
    {
        FAIL("out of memory");
        ErrorMessage( GetLastError() );
        fRet = FALSE;
        goto Exit;
    }
    memset( lpb, (int)FILL_BYTE, dwBufferSize );

    //
    // open the parallel port
    //
    if( !pp.Open() )
    {
        fRet = FALSE;
        goto Exit;
    }
    
    g_pKato->Log( LOG_DETAIL, TEXT("Parallel Port opened successfully") );

    //
    // write bytes to port
    //
    while((dwCount--) > 0)
    {
        if( !pp.Write(lpb, dwBufferSize, &dwWrote, fUseWin32API ) )
        {
            fRet = FALSE;
            goto Exit;
        }       
    
        // 
        // make sure the requested number of bytes were written
        //
        if( dwWrote != dwBufferSize )
        {
            //
            // In case there was a failure, log it
            //
            g_pKato->Log( LOG_DETAIL, TEXT("Failed to write %d bytes"), dwBufferSize );
            fRet = FALSE;
            
            //
            // check and log device status
            //
            if( !pp.GetStatus( &dwStatus ) )
            {
                goto Exit;
            }

            g_pKato->Log( LOG_DETAIL, TEXT("Device status: 0x%x"), dwStatus );
            PrintCommStatus( dwStatus );

            goto Exit;
        }
    }
    
Exit:
    if( NULL != lpb )
    {
        delete[] lpb;
    }
    
    pp.Close();
    return fRet ? TPR_PASS : TPR_FAIL;
}
