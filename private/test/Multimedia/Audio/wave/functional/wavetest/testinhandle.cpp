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

DWORD TestInHandle( HWAVEIN hwi, WAVEHDR wh )
{
    DWORD  dwReturn       = TPR_PASS;
    int    iFunctionIndex = 0;
    DWORD  MMResult       = MMSYSERR_NOERROR;
    MMTIME mmt;
    UINT   uDeviceID      = 0;

    for( iFunctionIndex =0; iFunctionIndex < NUM_OF_WAVEIN_FUNCTIONS;
        iFunctionIndex++ )
    {
        switch ( iFunctionIndex )
        {
        case 0:
            LOG( TEXT( "Calling waveInAddBuffer." ) );
            MMResult = waveInAddBuffer( hwi, &wh, sizeof( wh ) );
            break;
        case 1:
            LOG( TEXT( "Calling waveInClose." ) );
            MMResult = waveInClose( hwi );
            break;
        case 2:
            LOG( TEXT( "Calling waveInGetID." ) );
            MMResult = waveInGetID( hwi, &uDeviceID );
            break;
        case 3:
            LOG( TEXT( "Calling waveInGetPosition." ) );
            MMResult = waveInGetPosition( hwi, &mmt, sizeof( mmt ) );
            break;
        case 4:
            LOG( TEXT( "Calling waveInPrepareHeader." ) );
            MMResult = waveInPrepareHeader( hwi, &wh, sizeof( wh ) );
            break;
        case 5:
            LOG( TEXT( "Calling waveInReset." ) );
            MMResult = waveInReset( hwi );
            break;
        case 6:
            LOG( TEXT( "Calling waveInStart." ) );
            MMResult = waveInStart( hwi );
            break;
        case 7:
            LOG( TEXT( "Calling waveInStop." ) );
            MMResult = waveInStop( hwi );
            break;
        case 8:
            LOG( TEXT( "Calling waveInUnprepareHeader." ) );
            MMResult = waveInUnprepareHeader( hwi, &wh, sizeof( wh ) );
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