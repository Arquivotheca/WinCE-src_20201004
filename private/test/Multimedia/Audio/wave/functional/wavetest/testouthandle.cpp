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

DWORD TestOutHandle( HWAVEOUT hwo, WAVEHDR wh )
{
    DWORD  dwPitch            = 0;
    DWORD  dwRate             = 0;
    DWORD  dwReturn           = TPR_PASS;
    DWORD  dwVolume           = 0;
    int    iFunctionIndex     = 0;
    DWORD  MMResult           = MMSYSERR_NOERROR;
    MMTIME mmt;
    UINT   uDeviceID          = 0;
    UINT   uNumWaveOutNumDevs = waveOutGetNumDevs();

    for( iFunctionIndex =0; iFunctionIndex < NUM_OF_WAVEOUT_FUNCTIONS;
        iFunctionIndex++ )
    {
        switch ( iFunctionIndex )
        {
        case 0:
            LOG( TEXT( "Calling waveOutBreakLoop." ) );
            MMResult = waveOutBreakLoop( hwo );
            break;
        case 1:
            LOG( TEXT( "Calling waveOutClose." ) );
            MMResult = waveOutClose( hwo );
            break;
        case 2:
            LOG( TEXT( "Calling waveOutGetID." ) );
            MMResult = waveOutGetID( hwo, &uDeviceID );
            break;
        case 3:
            LOG( TEXT( "Calling waveOutGetPitch." ) );
            MMResult = waveOutGetPitch( hwo, &dwPitch );
            break;
        case 4:
            LOG( TEXT( "Calling waveOutGetPlaybackRate." ) );
            MMResult = waveOutGetPlaybackRate( hwo, &dwRate );
            break;
        case 5:
            LOG( TEXT( "Calling waveOutGetPosition." ) );
            MMResult = waveOutGetPosition( hwo, &mmt, sizeof( mmt ) );
            break;
        case 6:
            if( (UINT)hwo >= uNumWaveOutNumDevs )
            {
                // Suppress this test if hwo is a valid device  ID.
                LOG( TEXT( "Calling waveOutGetVolume." ) );
                MMResult = waveOutGetVolume( hwo, &dwVolume );
                break;
            }
            iFunctionIndex++;
            __fallthrough;
        case 7:
            LOG( TEXT( "Calling waveOutPause." ) );
            MMResult = waveOutPause( hwo );
            break;
        case 8:
            LOG( TEXT( "Calling waveOutReset." ) );
            MMResult = waveOutReset( hwo );
            break;
        case 9:
            LOG( TEXT( "Calling waveOutSetPitch." ) );
            MMResult = waveOutSetPitch( hwo, dwPitch );
            break;
        case 10:
            LOG( TEXT( "Calling waveOutSetPlaybackRate." ) );
            MMResult = waveOutSetPlaybackRate( hwo, dwPitch );
            break;
        case 11:
            if( (UINT)hwo >= uNumWaveOutNumDevs )
            {
                // Suppress this test if hwo is a valid device  ID.
                LOG( TEXT( "Calling waveOutSetVolume." ) );
                MMResult = waveOutSetVolume( hwo, dwVolume );
                break;
            }
            iFunctionIndex++;
            __fallthrough;
        case 12:
            LOG( TEXT( "Calling waveOutUnprepareHeader." ) );
            MMResult = waveOutUnprepareHeader( hwo, &wh, sizeof( wh ) );
            break;
        case 13:
            LOG( TEXT( "Calling waveOutUnprepareHeader." ) );
            MMResult = waveOutUnprepareHeader( hwo, &wh, sizeof( wh ) );
            break;
        case 14:
            LOG( TEXT( "Calling waveOutWrite." ) );
            MMResult = waveOutWrite( hwo, &wh, sizeof( wh ) );
            break;
        default:
		    LOG( TEXT( "ERROR:\tin %s @ line %u"),
                TEXT( __FILE__ ), __LINE__ );
            LOG( TEXT( "iFunctionIndex out of range." ) );            
            MMResult = MMSYSERR_ERROR;
            break;
        } // switch

        if( MMSYSERR_INVALHANDLE == MMResult ) LOG( TEXT( "TEST OK" ) );
        else
        {
            LOG( TEXT( "ERROR:  Test failed" ) );
            dwReturn = TPR_FAIL;
        }
    } // for
    return dwReturn;
}