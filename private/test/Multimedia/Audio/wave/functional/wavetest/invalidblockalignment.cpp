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

#include "InvalidBlockAlignment.h"

 /* 
  * Function Name: InvalidBlockAlignment
  *
  * Purpose: Test Function to verify that using invalid block alignment values with wav API methods
  * are handled gracefully.
  */

TESTPROCAPI InvalidBlockAlignment(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
     /*
     * This procedure verifies that audio APIs trap and appropriately respond
     * when invalid block alignment is passed to them within a parameter.
     */

     BEGINTESTPROC

     //---- First check if any previous tests found no wave input and no wave output devices.
     if( g_bSkipIn   &&   g_bSkipOut )
     { 
          LOG( TEXT("ERROR: no wave input or output devices, skipping Invalid Block Alignment tests.") );
          return TPR_SKIP;
     }

     //---- Now do our own check for waveIn device
     if( !waveInGetNumDevs() )
     {
          LOG( TEXT("ERROR: waveInGetNumDevs reported zero devices, we need at least one.") );
          g_bSkipIn = true;
     }

     //---- Now do our own check for waveout device
     if( !waveOutGetNumDevs() )
     {
          LOG( TEXT("ERROR: waveOutGetNumDevs reported zero devices, we need at least one.") );
          g_bSkipOut = true;
     }

     //---- Make sure our check found either a wave In device or a wave Out device.
     if( g_bSkipIn   &&   g_bSkipOut )
     { 
          LOG( TEXT("ERROR: no wave input or output devices, skipping Invalid Block Alignment tests.") );
          return TPR_SKIP;
     }

     BOOL         bResult            = TRUE;
     DWORD        dwBufferLength;
     DWORD        dwExpectedPlayTime = 100; // in milliseconds
     DWORD        dwReturn           = TPR_PASS;
     char*        data               = NULL;
     HMODULE      hmod               = NULL;
     HWAVEIN      hwi                = NULL;
     HWAVEOUT     hwo                = NULL;


     //---- In order to set up our sine wave buffer, we initially give this a valid block alignment value,
     //----  BLOCKALIGN, which is #define'd as CHANNELS * BITSPERSAMPLE / 8
     WAVEFORMATEX wfx                =
     {
          WAVE_FORMAT_PCM,
          CHANNELS,
          SAMPLESPERSEC,
          AVGBYTESPERSEC,
          BLOCKALIGN,
          BITSPERSAMPLE,
          SIZE
     };
     WAVEHDR  wh;

     //---- Set up an audio buffer for test.
     dwBufferLength = dwExpectedPlayTime * wfx.nAvgBytesPerSec / 1000;
     data = new char[ dwBufferLength ];
     if( !data )
     {
          LOG( TEXT( "ERROR:\tNew failed for data [%s:%u]" ), TEXT( __FILE__ ), __LINE__ );
          LOG( TEXT( "\tPossible Cause:  Out of Memory\n" ) );
          return TPR_ABORT;
     }

     ZeroMemory( data, dwBufferLength );
     ZeroMemory( &wh,sizeof( WAVEHDR ) );

     //---- Check for a buffer that is smaller than we expected. 
     if( dwExpectedPlayTime >= ( ULONG_MAX / wfx.nAvgBytesPerSec ) )
     {
          LOG( TEXT( "FAIL in %s @ line %u:" ), TEXT( __FILE__ ), __LINE__ );
          LOG( TEXT( "\tPotential overflow, dwExpectedPlayTime = %d ms."), dwExpectedPlayTime );
          delete[] data;
          data = NULL;
          return TPR_ABORT;
     }

     PREFAST_SUPPRESS(419, "The above line is checking for overflow. This seems to be prefast noise, since the path/trace Prefast lists is not possible. (Prefast team concurs so far.)");

     wh.lpData         = data;
     wh.dwBufferLength = dwBufferLength;
     wh.dwLoops        = 1;
     wh.dwFlags        = 0;

     if( !SineWave( wh.lpData, wh.dwBufferLength, &wfx ) )
     {
          LOG( TEXT( "FAIL in %s @ line %u:\tSineWave returned a buffer of length zero." ), TEXT( __FILE__), __LINE__ );
          delete[] data;
          data = NULL;
          return TPR_ABORT;
     }

     if( false == g_bSkipIn )
          dwReturn = GetReturnCode( dwReturn, TestValidWaveInAlignment( &hwi, &wfx, &wh ) );
     if( false == g_bSkipOut )
          dwReturn = GetReturnCode( dwReturn, TestValidWaveOutAlignment( &hwo, &wfx, &wh ) );

     if( false == g_bSkipIn )
          dwReturn = GetReturnCode( dwReturn, TestInvalidWaveInAlignment( &hwi, &wfx, &wh ) );
     if( false == g_bSkipOut )
          dwReturn = GetReturnCode( dwReturn, TestInvalidWaveOutAlignment( &hwo, &wfx, &wh ) );

     if( g_bSkipIn   ||   g_bSkipOut )
          dwReturn = GetReturnCode( dwReturn, TPR_SKIP );

     delete[] data;
     data = NULL;
     return dwReturn;

} //---- InvalidBlockAlignment()






 /* 
  * Function Name: TestValidWaveInAlignment
  *
  * Purpose: Helper Function to verify that using valid block alignment value with Wave API methods
  *          gets valid wave input device handle, and that subsequently changing the alignment to invalid value
  *          on user side doesn't prevent audio driver from preparing WAVEHEADER for that device.
  */

DWORD TestValidWaveInAlignment( HWAVEIN * phwi, WAVEFORMATEX * pwfx, WAVEHDR * pwh )
{
     if( !phwi   ||   INVALID_HANDLE_VALUE == *phwi )
     {
          LOG( TEXT( "Uninitialized HWAVEIN handle, skipping test of valid waveIn block alignment." ) );
          return TPR_SKIP;
     }
     if( !pwfx )
     {
          LOG( TEXT( "Uninitialized WAVEFORMATEX, skipping test of valid waveIn block alignment." ) );
          return TPR_SKIP;
     }
     if( !pwh   ||   !pwh->lpData   ||   pwh->dwBufferLength == 0   ||   pwh->dwFlags != 0 )
     {
          LOG( TEXT( "Uninitialized WAVEHEADER, skipping test of valid waveIn block alignment." ) );
          return TPR_SKIP;
     }

     MMRESULT MMResult = MMSYSERR_NOERROR;

     //---- First we call waveInOpen() with the valid block alignment.
     pwfx->nBlockAlign = BLOCKALIGN;

     LOG( TEXT( "Calling waveInOpen() with valid block alignment of %d." ), pwfx->nBlockAlign );
     __try
     {
          MMResult = waveInOpen( phwi, g_dwInDeviceID, pwfx, NULL, 0, NULL );
     }
     __except( EXCEPTION_EXECUTE_HANDLER )
     {
          LOG( TEXT( "FAIL:  exception raised calling waveInOpen()." ) );
          return TPR_FAIL;
     }
     if( MMSYSERR_NOERROR != MMResult )
     {
          LOG( TEXT( "FAIL:  waveInOpen() returned error code %u with valid block alignment of %d." ), MMResult, pwfx->nBlockAlign );
          return TPR_FAIL;
     }
     else
     {
          LOG( TEXT( "waveInOpen() succeeds." ) );

          //---- We have a valid waveIn device handle.
          //---- Now set block alignment to 0 and verify that driver still uses the valid handle and successfully prepares header.
          pwfx->nBlockAlign = 0;
          LOG( TEXT( "Calling waveInPrepareHeader() and waveInUnprepareHeader() after setting block alignment to 0. These calls should succeed." ) );
          __try
          {
               MMResult = waveInPrepareHeader( *phwi, pwh, sizeof(*pwh) );
          }
          __except( EXCEPTION_EXECUTE_HANDLER )
          {
               LOG( TEXT( "FAIL:  exception raised calling waveInPrepareHeader()." ) );
               MMResult = waveInClose( *phwi );
               return TPR_FAIL;
          }
          if( MMSYSERR_NOERROR != MMResult )
          {
               LOG( TEXT( "FAIL:  waveInPrepareHeader() returned error code %u after opening valid handle and setting block alignment to invalid value of %d." ), MMResult, pwfx->nBlockAlign );
               MMResult = waveInClose( *phwi );
               return TPR_FAIL;
          }
          else
          {
               __try
               {
                    MMResult = waveInUnprepareHeader( *phwi, pwh, sizeof(*pwh) );
               }
               __except( EXCEPTION_EXECUTE_HANDLER )
               {
                    LOG( TEXT( "FAIL:  exception raised calling waveInUnprepareHeader()." ) );
                    MMResult = waveInClose( *phwi );
                    return TPR_FAIL;
               }
               if( MMSYSERR_NOERROR != MMResult )
               {
                    LOG( TEXT( "FAIL:  waveInUnprepareHeader() returned error code %u after opening valid handle and setting block alignment to invalid value of %d." ), MMResult, pwfx->nBlockAlign );
                    MMResult = waveInClose( *phwi );
                    return TPR_FAIL;
               }
               else
                    LOG( TEXT( "waveInPrepareHeader() and waveInUnprepareHeader() success." ) );
          }
          MMResult = waveInClose( *phwi );
     }
     return TPR_PASS;
}






 /* 
  * Function Name: TestValidWaveOutAlignment
  *
  * Purpose: Helper Function to verify that using valid block alignment value with Wave API methods
  *          gets valid wave output device handle, and that subsequently changing the alignment to invalid value
  *          on user side doesn't prevent audio driver from preparing WAVEHEADER for that device.
  */

DWORD TestValidWaveOutAlignment( HWAVEOUT * phwo, WAVEFORMATEX * pwfx, WAVEHDR * pwh )
{
     if( !phwo   ||   INVALID_HANDLE_VALUE == *phwo )
     {
          LOG( TEXT( "Uninitialized HWAVEOUT handle, skipping test of valid waveOut block alignment." ) );
          return TPR_SKIP;
     }
     if( !pwfx )
     {
          LOG( TEXT( "Uninitialized WAVEFORMATEX, skipping test of valid waveOut block alignment." ) );
          return TPR_SKIP;
     }
     if( !pwh   ||   !pwh->lpData   ||   pwh->dwBufferLength == 0   ||   pwh->dwFlags != 0 )
     {
          LOG( TEXT( "Uninitialized WAVEHEADER, skipping test of valid waveOut block alignment." ) );
          return TPR_SKIP;
     }

     MMRESULT MMResult = MMSYSERR_NOERROR;

     //---- First we call waveOutOpen() with the valid block alignment.
     pwfx->nBlockAlign = BLOCKALIGN;

     LOG( TEXT( "Calling waveOutOpen() with valid block alignment of %d." ), pwfx->nBlockAlign );
     __try
     {
          MMResult = waveOutOpen( phwo, g_dwOutDeviceID, pwfx, NULL, 0, NULL );
     }
     __except( EXCEPTION_EXECUTE_HANDLER )
     {
          LOG( TEXT( "FAIL:  exception raised calling waveOutOpen()." ) );
          return TPR_FAIL;
     }
     if( MMSYSERR_NOERROR != MMResult )
     {
          LOG( TEXT( "FAIL:  waveOutOpen() returned error code %u with valid block alignment of %d." ), MMResult, pwfx->nBlockAlign );
          return TPR_FAIL;
     }
     else
     {
          LOG( TEXT( "waveOutOpen() succeeds." ) );

          //---- We have a valid waveOut device handle.
          //---- Now set block alignment to 0 and verify that driver still uses the valid handle and successfully prepares header.
          pwfx->nBlockAlign = 0;
          LOG( TEXT( "Calling waveOutPrepareHeader() and waveOutUnprepareHeader() after setting block alignment to 0. These calls should succeed." ) );
          __try
          {
               MMResult = waveOutPrepareHeader( *phwo, pwh, sizeof(*pwh) );
          }
          __except( EXCEPTION_EXECUTE_HANDLER )
          {
               LOG( TEXT( "FAIL:  exception raised calling waveOutPrepareHeader()." ) );
               MMResult = waveOutClose( *phwo );
               return TPR_FAIL;
          }
          if( MMSYSERR_NOERROR != MMResult )
          {
               LOG( TEXT( "FAIL:  waveOutPrepareHeader() returned error code %u after opening valid handle and setting block alignment to invalid value of %d." ), MMResult, pwfx->nBlockAlign );
               MMResult = waveOutClose( *phwo );
               return TPR_FAIL;
          }
          else
          {
               __try
               {
                    MMResult = waveOutUnprepareHeader( *phwo, pwh, sizeof(*pwh) );
               }
               __except( EXCEPTION_EXECUTE_HANDLER )
               {
                    LOG( TEXT( "FAIL:  exception raised calling waveOutUnprepareHeader()." ) );
                    MMResult = waveOutClose( *phwo );
                    return TPR_FAIL;
               }
               if( MMSYSERR_NOERROR != MMResult )
               {
                    LOG( TEXT( "FAIL:  waveOutUnprepareHeader() returned error code %u after opening valid handle and setting block alignment to invalid value of %d." ), MMResult, pwfx->nBlockAlign );
                    MMResult = waveOutClose( *phwo );
                    return TPR_FAIL;
               }
               else
                    LOG( TEXT( "waveOutPrepareHeader() and waveOutUnprepareHeader() success." ) );
          }
          waveOutClose( *phwo );
     }
     return TPR_PASS;
}






 /* 
  * Function Name: TestInvalidWaveInAlignment
  *
  * Purpose: Helper Function to verify that using various invalid block alignment values with Wave API methods
  *          is handled gracefully and as expected.
  */

DWORD TestInvalidWaveInAlignment( HWAVEIN * phwi, WAVEFORMATEX * pwfx, WAVEHDR * pwh )
{
     if( !phwi   ||   INVALID_HANDLE_VALUE == *phwi )
     {
          LOG( TEXT( "Uninitialized HWAVEIN handle, skipping test of invalid waveIn block alignment." ) );
          return TPR_SKIP;
     }
     if( !pwfx )
     {
          LOG( TEXT( "Uninitialized WAVEFORMATEX, skipping test of invalid waveIn block alignment." ) );
          return TPR_SKIP;
     }
     if( !pwh   ||   !pwh->lpData   ||   pwh->dwBufferLength == 0   ||   pwh->dwFlags != 0 )
     {
          LOG( TEXT( "Uninitialized WAVEHEADER, skipping test of invalid waveIn block alignment." ) );
          return TPR_SKIP;
     }

     MMRESULT MMResult = MMSYSERR_NOERROR;

     DWORD dwLocalReturn = TPR_PASS;
     int iBlockAlign = -1;
     for( int i = 1;   i <= 6;   i++ )
     {
          switch( i )
          {
               case 1:
                    break;
               case 2:
                    iBlockAlign = 0;
                    break;
               case 3:
                    iBlockAlign = 3;
                    break;
               case 4:
                    iBlockAlign = 5;
                    break;
               case 5:
                    iBlockAlign = 6;
                    break;
               case 6:
                    iBlockAlign = 7;
                    break;
          }

          //----  We now replace block alignment value with the appropriate invalid value.
          pwfx->nBlockAlign = iBlockAlign;

          LOG( TEXT( "Calling waveInOpen() with invalid block alignment of %d." ), pwfx->nBlockAlign );
          __try
          {
               MMResult = waveInOpen (phwi, g_dwInDeviceID, pwfx, NULL, 0, NULL );
          }
          __except( EXCEPTION_EXECUTE_HANDLER )
          {
               LOG( TEXT( "FAIL:  exception raised calling waveInOpen()." ) );
               return TPR_FAIL;
          }
          if( MMSYSERR_NOERROR == MMResult )
          {
               LOG( TEXT( "FAIL:  waveInOpen() succeeds with invalid block alignment of %d. It should return WAVERR_BADFORMAT (32) in this case." ), pwfx->nBlockAlign );
               MMResult = waveInClose( *phwi );
               dwLocalReturn = GetReturnCode( dwLocalReturn, TPR_FAIL );
               continue;
          }
          else if( WAVERR_BADFORMAT != MMResult )
          {
               LOG( TEXT( "FAIL:  waveInOpen() returned %u with invalid block alignment of %d. It should return WAVERR_BADFORMAT (32) in this case." ), MMResult, pwfx->nBlockAlign );
               dwLocalReturn = GetReturnCode( dwLocalReturn, TPR_FAIL );
               continue;
          }

          LOG( TEXT( "waveInOpen() properly returns WAVERR_BADFORMAT with invalid block alignment of %d." ), pwfx->nBlockAlign );
     }
     return dwLocalReturn;
}






 /* 
  * Function Name: TestInvalidWaveOutAlignment
  *
  * Purpose: Helper Function to verify that using various invalid block alignment values with Wave API methods
  *          is handled gracefully and as expected.
  */

DWORD TestInvalidWaveOutAlignment( HWAVEOUT * phwo, WAVEFORMATEX * pwfx, WAVEHDR * pwh )
{
     if( !phwo   ||   INVALID_HANDLE_VALUE == *phwo )
     {
          LOG( TEXT( "Uninitialized HWAVEOUT handle, skipping test of invalid waveOut block alignment." ) );
          return TPR_SKIP;
     }
     if( !pwfx )
     {
          LOG( TEXT( "Uninitialized WAVEFORMATEX, skipping test of invalid waveOut block alignment." ) );
          return TPR_SKIP;
     }
     if( !pwh   ||   !pwh->lpData   ||   pwh->dwBufferLength == 0   ||   pwh->dwFlags != 0 )
     {
          LOG( TEXT( "Uninitialized WAVEHEADER, skipping test of invalid waveOut block alignment." ) );
          return TPR_SKIP;
     }

     MMRESULT MMResult = MMSYSERR_NOERROR;

     DWORD dwLocalReturn = TPR_PASS;
     int iBlockAlign = -1;
     for( int i = 1;   i <= 6;   i++ )
     {
          switch( i )
          {
               case 1:
                    break;
               case 2:
                    iBlockAlign = 0;
                    break;
               case 3:
                    iBlockAlign = 3;
                    break;
               case 4:
                    iBlockAlign = 5;
                    break;
               case 5:
                    iBlockAlign = 6;
                    break;
               case 6:
                    iBlockAlign = 7;
                    break;
          }

          //----  We now replace block alignment value with the appropriate invalid value.
          pwfx->nBlockAlign = iBlockAlign;

          LOG( TEXT( "Calling waveOutOpen() with invalid block alignment of %d." ), pwfx->nBlockAlign );
          __try
          {
               MMResult = waveOutOpen (phwo, g_dwOutDeviceID, pwfx, NULL, 0, NULL );
          }
          __except( EXCEPTION_EXECUTE_HANDLER )
          {
               LOG( TEXT( "FAIL:  exception raised calling waveOutOpen()." ) );
               return TPR_FAIL;
          }
          if( MMSYSERR_NOERROR == MMResult )
          {
               LOG( TEXT( "FAIL:  waveOutOpen() succeeds with invalid block alignment of %d. It should return WAVERR_BADFORMAT (32) in this case." ), pwfx->nBlockAlign );
               MMResult = waveOutClose( *phwo );
               dwLocalReturn = GetReturnCode( dwLocalReturn, TPR_FAIL );
               continue;
          }
          else if( WAVERR_BADFORMAT != MMResult )
          {
               LOG( TEXT( "FAIL:  waveOutOpen() returned %u with invalid block alignment of %d. It should return WAVERR_BADFORMAT (32) in this case." ), MMResult, pwfx->nBlockAlign );
               dwLocalReturn = GetReturnCode( dwLocalReturn, TPR_FAIL );
               continue;
          }

          LOG( TEXT( "waveOutOpen() properly returns WAVERR_BADFORMAT with invalid block alignment of %d." ), pwfx->nBlockAlign );
     }
     return dwLocalReturn;
}
