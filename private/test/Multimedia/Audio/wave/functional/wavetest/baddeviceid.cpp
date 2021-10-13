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


#include "BadDeviceID.h"
#include "TEST_WAVETEST.H"

 /* 
  * Function Name: uMax
  *
  * Purpose: Helper Function to select the larger of 2 UINTs.
  */

UINT uMax( UINT a, UINT b )
{
     if( a > b ) return a;
     else        return b;
} // uMax()

 /* 
  * Function Name: BadDeviceID
  *
  * Purpose: Test Function to verify that device ID parameters to audio APIs
  * are validated.
  */

TESTPROCAPI BadDeviceID(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    /*
     * This procedure verifies that audio APIs trap and appropriately respond
     * when invalid BadDevice IDs are passed to them as parameters.
     */

     BEGINTESTPROC

     if( g_bSkipOut ) return TPR_SKIP;

     // check for capture device
     if( !waveInGetNumDevs() )
     {
          LOG( TEXT("ERROR: waveInGetNumDevs() reported zero devices, we need at least one.") );
          return TPR_SKIP;
     }

     // check for waveout device
     if( !waveOutGetNumDevs() )
     {
          LOG( TEXT("ERROR: waveOutGetNumDevs() reported zero devices, we need at least one.") );
          return TPR_SKIP;
     }

     char*         data               = NULL;
     DWORD         dwBufferLength     = 0;
     DWORD         dwExpectedPlayTime = 100; // in milliseconds
     DWORD         dwReturn           = TPR_PASS;
     HWAVEIN       hwi                = NULL;
     HWAVEOUT      hwo                = NULL;
     MMRESULT      MMResult           = MMSYSERR_NOERROR;
     UINT          uLoopIndex         = 0;
     UINT          uNumWaveInNumDevs  = waveInGetNumDevs();
     UINT          uNumWaveOutNumDevs = waveOutGetNumDevs();
     WAVEFORMATEX  wfx                =
     {
          WAVE_FORMAT_PCM,
          CHANNELS,
          SAMPLESPERSEC,
          AVGBYTESPERSEC,
          BLOCKALIGN,
          BITSPERSAMPLE,
          SIZE
     };
     WAVEHDR     wh;
     WAVEINCAPS  wic;
     WAVEOUTCAPS woc;

     // Set up an audio buffer for test.
     dwBufferLength = dwExpectedPlayTime * wfx.nAvgBytesPerSec / 1000;
     data = new char[ dwBufferLength ];
     if( !data )
     {
          LOG( TEXT( "FAIL in %s @ line %u:" ), TEXT( __FILE__ ), __LINE__ );
          LOG( TEXT( "\tPossible Cause:  Out of Memory\n" ) );
          return TPR_ABORT;
     }

     ZeroMemory( data, dwBufferLength );
     ZeroMemory( &wh,sizeof( WAVEHDR ) );

     // Check for a buffer that is smaller than we expected. 
     if( dwExpectedPlayTime >= ( ULONG_MAX / wfx.nAvgBytesPerSec ) )
     {
          LOG( TEXT( "FAIL in %s @ line %u:" ), TEXT( __FILE__ ), __LINE__ );
          LOG( TEXT( "\tPotential overflow, dwExpectedPlayTime = %d ms."), dwExpectedPlayTime );
          delete[] data;
          data = NULL;
          return TPR_ABORT;
     } // if ExpectedPlayTime
     PREFAST_SUPPRESS(419, "The above line is checking for overflow. This seems to be prefast noise, since the path/trace Prefast lists is not possible. (Prefast team concurs so far.)");

     wh.lpData         = data;
     wh.dwBufferLength = dwBufferLength;
     wh.dwLoops        = 1;
     wh.dwFlags        = 0;
     if( !SineWave( wh.lpData, wh.dwBufferLength, &wfx ) )
     {
          LOG( TEXT( "FAIL in %s @ line %u:\tSineWave returned a buffer of length zero."), TEXT( __FILE__), __LINE__ );
          delete[] data;
          data = NULL;
          return TPR_ABORT;
     }

     // Pass valid and invalid device IDs to APIs that should succeed otherwise,
     // and verify that they respond appropriately.
     for( uLoopIndex = 0; uLoopIndex <= uMax( uNumWaveInNumDevs, uNumWaveOutNumDevs ); uLoopIndex++ )
     {
          // Verify waveInGetDevCaps().
          MMResult = waveInGetDevCaps( uLoopIndex, &wic, sizeof( wic ) );
          dwReturn = GetReturnCode( dwReturn, ProcessWaveformFunctionResults( MMResult, uLoopIndex, uNumWaveInNumDevs, TEXT( "waveInGetDevCaps\t" ) ));

          // verify waveInOpen()
          MMResult = waveInOpen( &hwi, uLoopIndex, &wfx, NULL, 0, NULL );
          dwReturn = GetReturnCode( dwReturn, ProcessWaveformFunctionResults( MMResult, uLoopIndex, uNumWaveInNumDevs, TEXT( "waveInOpen\t\t" ) ) );

          // cleanup
          if( MMSYSERR_NOERROR == MMResult )
          {
               MMResult = waveInClose( hwi );
               if( MMSYSERR_NOERROR != MMResult )
               {
                    LOG( TEXT( "ERROR:\tin %s @ line %u:" ), TEXT( __FILE__ ), __LINE__ );
                    LOG( TEXT( "\t\tCouldn't close input device %u. MMResult = %u"), uLoopIndex, MMResult );
                    dwReturn = TPR_FAIL;
               } // if MMSYSERR_NOERROR( waveInClose() )
          } // if MMSYSERR_NOERROR( waveInOpen() )

          // Verify waveOutGetDevCaps().
          MMResult = waveOutGetDevCaps( uLoopIndex, &woc, sizeof( woc) );
          dwReturn = GetReturnCode( dwReturn, ProcessWaveformFunctionResults( MMResult,uLoopIndex, uNumWaveOutNumDevs, TEXT( "waveOutGetDevCaps\t" ) ) );
		  
		  //Skip the device if it is Bluetooth audio--- 
		  //as waveOutOpen will fail without Bluetooth headset paired
		  if(_tcsicmp(woc.szPname, TEXT("Bluetooth Advanced Audio Output")) == 0)
		  {
			  LOG(TEXT("Skipping audio device: %s"), woc.szPname);
			  continue;
		  }
          
          // Verify waveOutGetDevCaps.
          MMResult = waveOutOpen( &hwo, uLoopIndex, &wfx, NULL, 0, NULL );
          dwReturn = GetReturnCode( dwReturn, ProcessWaveformFunctionResults( MMResult, uLoopIndex, uNumWaveOutNumDevs, TEXT( "waveOutOpen\t\t" ) ) );

          // cleanup
          if( MMSYSERR_NOERROR == MMResult )
          {
               MMResult = waveOutClose( hwo );
               if( MMSYSERR_NOERROR != MMResult )
               {
                    LOG( TEXT( "ERROR:\tin %s @ line %u:" ),TEXT( __FILE__ ), __LINE__ );
                    LOG( TEXT( "\t\tCouldn't close output device %u. MMResult = %u"), uLoopIndex, MMResult );
                    dwReturn = TPR_FAIL;
               } // if MMSYSERR_NOERROR( waveInClose() )
          } // if MMSYSERR_NOERROR( waveInOpen() )
  
     } // for
     delete[] data;
     data = NULL;
     return dwReturn;
} // BadDeviceID

 /* 
  * Function Name: ProcessWaveformFunctionResults
  *
  * Purpose: Helper Function to evaluate waveform function results, and log
  *          appropriate comments.
  */

DWORD ProcessWaveformFunctionResults( MMRESULT MMResult, UINT uWaveOutDevNum, UINT uNumWaveOutNumDevs, const TCHAR *szID )
{
     DWORD dwReturn = TPR_PASS;

     if( uWaveOutDevNum < uNumWaveOutNumDevs )
     {
          if( MMSYSERR_NOERROR == MMResult ) 
               LOG(TEXT( "%s succeeded, Device ID: = %u." ), szID, uWaveOutDevNum );
          else // !MMSYSERR_NOERROR
          {
               LOG( TEXT( "FAIL:\t%s failed. Device ID = %u, MMResult = %u" ), szID, uWaveOutDevNum, MMResult );
               dwReturn = TPR_FAIL;
          } // else !MMSYSERR_NOERROR
     } // if uWaveOutDevNum
     else // uWaveOutDevNum >=
     {
          if( MMSYSERR_NOERROR == MMResult )
          {
               LOG( TEXT("FAIL:\t%s failed to trap invalid device %u, MMResult = %u" ),szID, uWaveOutDevNum, MMResult );
               dwReturn = TPR_FAIL;
          } // if MMSYSERR_NOERROR
          else if( MMSYSERR_BADDEVICEID != MMResult )
          {
               //---- some other error returned besides MMSYSERR_BADDEVICEID
               LOG( TEXT("FAIL:\t%s trapped invalid device ID %u but with wrong error code, MMResult = %u. Error should be MMSYSERR_BADDEVICEID (2)." ),szID, uWaveOutDevNum, MMResult );
               dwReturn = TPR_FAIL;
          }
          else
               LOG( TEXT("%s successfully trapped invalid Device ID: %u, MMResult = %u" ),szID, uWaveOutDevNum, MMResult );
     } // else uWaveOutDevNum >=
     return dwReturn;
} // ProcessWaveformFunctionResults()

