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



#include "BadHandles.h"


extern BOOL FullDuplex();


TESTPROCAPI BadHandles( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
     /*
     * This procedure verifies that audio APIs trap and appropriately respond
     * when invalid handles are passed to them as parameters.
     */

     BEGINTESTPROC

     if( g_bSkipOut ) 
          return TPR_SKIP;

     //---- check for waveIn device
     if( !waveInGetNumDevs() )
     {
          LOG( TEXT("ERROR: waveInGetNumDevs reported zero devices, we need at least one.") );
          return TPR_SKIP;
     }

     //---- check for waveout device
     if( !waveOutGetNumDevs() )
     {
          LOG( TEXT("ERROR: waveOutGetNumDevs reported zero devices, we need at least one.") );
          return TPR_SKIP;
     }

     BOOL         bResult            = TRUE;
     DWORD        dwBufferLength;
     DWORD        dwExpectedPlayTime = 100; // in milliseconds
     DWORD        dwLoopIndex        = 0;
     DWORD        dwReturn           = TPR_PASS;
     char*        data               = NULL;
     HMODULE      hmod               = NULL;
     HWAVEIN      hwi                = NULL;
     HWAVEIN      hwi2               = NULL;
     HWAVEOUT     hwo                = NULL;
     HWAVEOUT     hwo2               = NULL;
     MMRESULT     MMResult           = MMSYSERR_NOERROR;

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

     for( dwLoopIndex = 0;   dwLoopIndex < 4;   dwLoopIndex++ )
     {
          switch( dwLoopIndex )
          {
               case 0:
                    //---- Pass NULL handles to APIs that should succeed otherwise, and
                    //---- verify that they respond appropriately.
                    hwi = NULL;
                    hwo = NULL;
                    break;

               case 1:
                    //---- If either the command line or the driver reports half-duplex operation,
                    //---- we should skip this case because it requires input and output handles open at the same time.
                    LOG(TEXT("Checking Driver's Ability to Handle Full-Duplex Operation"));

                    if( !FullDuplex() )
                    {
                         //---- driver reports half-duplex
                         if( !g_useSound )
                         {
                              //---- command line agrees, this is a half duplex device, skip this switch() case
                              LOG(TEXT("Command line reports half-duplex, driver agrees, moving to next test case."));
                              continue;
                         }
                         else
                         {
                              //---- command line disagrees 
                              //---- turn off full duplex operation, log this and continue with next switch() case                              
                              LOG(TEXT("Warning:  Unable to open waveIn and waveOut at the same time"));
                              LOG(TEXT("          Your driver claims Half-Duplex Operation"));
                              LOG(TEXT("          Your commandline claims Full-Duplex Operation"));
                              LOG(TEXT("          Turning off Full-Duplex Operation and moving to next test case."));
                              
                              dwReturn = TPR_ABORT;
                              g_useSound = FALSE;
                              continue;
                         }
                    }
                    else if( !g_useSound )
                    {
                         //---- driver reports full duplex, command line disagrees (has -e switch)
                         LOG(TEXT("Abort:    Able to open waveIn and waveOut at the same time"));
                         LOG(TEXT("          Your driver claims Full-Duplex Operation"));
                         LOG(TEXT("          Your commandline claims Half-Duplex Operation (-c \"-e\")"));
                         LOG(TEXT("          Fix your driver to work in Half-Duplex by making sure that" ));
                         LOG(TEXT("          waveIn and waveOut cannot be opened at the same time"));
                         LOG(TEXT("          or test your driver as a Full-Duplex driver"));
                         LOG(TEXT("          without commandline -c \"-e\" options."));
                         
                         dwReturn = TPR_ABORT;
                         
                         continue;
                    }

                    //---- Obtain valid device handles, but assign them to the wrong identifiers.
                    //---- Repeat all the tests using these corrupted handles instead of NULL.
                    MMResult = waveInOpen(&hwi2, g_dwInDeviceID, &wfx, NULL, 0, NULL );
                    if( MMSYSERR_NOERROR == MMResult )
                         LOG( TEXT( "waveInOpen\tcreated handle %u" ), hwi2 );
                    else 
                    {
                         LOG( TEXT( "ERROR:\tin [%s:%u]" ),TEXT( __FILE__ ), __LINE__ );
                         LOG( TEXT( "\t\twaveInOpen\tfailed. MMResult = %u" ), MMResult );
                         dwReturn = TPR_FAIL;
                    } 
                    MMResult = waveOutOpen( &hwo2, g_dwOutDeviceID, &wfx, NULL, 0, NULL );
                    if( MMSYSERR_NOERROR == MMResult )
                         LOG( TEXT( "waveOutOpen\tcreated handle %u" ), hwo2 );
                    else 
                    {
                         LOG( TEXT( "ERROR:\tin [%s:%u]" ), TEXT( __FILE__ ), __LINE__ );
                         LOG( TEXT( "\t\twaveOutOpen\tfailed. MMResult = %u" ),MMResult );
                         dwReturn = TPR_FAIL;
                    } 
                    hwi = (HWAVEIN)hwo2;
                    hwo = (HWAVEOUT)hwi2;
                    break;

               case 2:
                    //---- Reassign hwi and hwo to their correct values.  Then Close the
                    //---- devices go back and pass the closed handles to APIs to verify
                    //---- that they are trapped as invalid.

                    if( !g_useSound ) 
                    {
                         //---- We skipped the last case because of half-duplex operation.
                         //---- Open each handle one at a time and then close it.
                         MMResult = waveInOpen(&hwi, g_dwInDeviceID, &wfx, NULL, 0, NULL );
                         if( MMSYSERR_NOERROR == MMResult )
                              LOG( TEXT( "waveInOpen\tcreated handle %u" ), hwi );
                         else 
                         {
                              LOG( TEXT( "ERROR:\tin [%s:%u]" ),TEXT( __FILE__ ), __LINE__ );
                              LOG( TEXT( "\t\twaveInOpen\tfailed. MMResult = %u" ), MMResult );
                              dwReturn = TPR_FAIL;
                         }
                    }
                    else
                    {
                         //---- handles hwi2 and hwo2 are already open from above case.
                         hwi = hwi2;
                         hwo = hwo2;
                    }

                    MMResult = waveInClose( hwi );
                    if( MMSYSERR_NOERROR == MMResult )
                    LOG( TEXT( "waveInClose\tclosed handle %u" ), hwi );
                    else
                    {
                         LOG( TEXT( "ERROR:\tin [%s:%u]" ),TEXT( __FILE__ ), __LINE__ );
                         LOG( TEXT( "\t\twaveInClose\tfailed while trying to close handle %u. MMResult = %u"), hwi, MMResult );
                         dwReturn = TPR_FAIL;
                    }

                    if( !g_useSound ) 
                    {
                         //---- We skipped the last case because of half-duplex operation.
                         //---- Open each handle one at a time and then close it.
                         MMResult = waveOutOpen( &hwo, g_dwOutDeviceID, &wfx, NULL, 0, NULL );
                         if( MMSYSERR_NOERROR == MMResult )
                              LOG( TEXT( "waveOutOpen\tcreated handle %u" ), hwo );
                         else 
                         {
                              LOG( TEXT( "ERROR:\tin [%s:%u]" ), TEXT( __FILE__ ), __LINE__ );
                              LOG( TEXT( "\t\twaveOutOpen\tfailed. MMResult = %u" ),MMResult );
                              dwReturn = TPR_FAIL;
                         }
                    }                         
                    MMResult = waveOutClose( hwo );
                    if( MMSYSERR_NOERROR == MMResult )
                         LOG( TEXT( "waveOutClose\tclosed handle %u" ), hwo );
                    else
                    {
                         LOG( TEXT( "ERROR:\tin [%s:%u]" ),TEXT( __FILE__ ), __LINE__ );
                         LOG( TEXT("\t\twaveOutClose\tfailed while trying to close handle %u. MMResult = %u"), hwo, MMResult );
                         dwReturn = TPR_FAIL;
                    }

                    break;

               case 3:
                    //---- Assign large invalid values to hwi and hwo.
                    hwi = (HWAVEIN) 34567890;
                    hwo = (HWAVEOUT)45678901;
                    LOG( TEXT( "hwi = %u" ), hwi );
                    LOG( TEXT( "hwo = %u" ), hwo );
                    break;

               default:
                    LOG( TEXT( "ERROR:\tin [%s:%u]" ),TEXT( __FILE__ ), __LINE__ );
                    LOG( TEXT( "\t\tTest over. dwLoopIndex = %u" ), dwLoopIndex );
                    break;

          } //---- switch()

          dwReturn = GetReturnCode( dwReturn, TestInHandle(  hwi, wh ) );
          dwReturn = GetReturnCode( dwReturn, TestOutHandle( hwo, wh ) );
     } //---- for()

     delete[] data;
     data = NULL;
     return dwReturn;

} //---- BadHandles
