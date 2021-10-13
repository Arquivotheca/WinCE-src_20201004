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
#include "BadGUID.h"
#include "initguid.h"
#include "TEST_WAVETEST.H"

// {846A781A-9A84-4c24-8D5C-A08CAFEE47FA}
DEFINE_GUID(GUID_Bad,
0x846a781a, 0x9a84, 0x4c24, 0x8d, 0x5c, 0xa0, 0x8c, 0xaf, 0xee, 0x47, 0xfa);

 /* 
  * Function Name: BadGUID
  *
  * Purpose: Test Function to verify that waveOutGetProperty() and
  *          waveOutSetProperty() gracefully respond when an invalid GUID is
  *          passed to them.
  */

TESTPROCAPI BadGUID(
    UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    BEGINTESTPROC

    if( g_bSkipOut ) return TPR_SKIP;

    // check for capture device
	if( !waveInGetNumDevs() )
    {
		LOG( TEXT(
            "ERROR: waveInGetNumDevs() reported zero devices, we need at least one."
            ) );
		return TPR_SKIP;
	}

AUDIOGAINCLASS agcData      = { WAGC_PRIORITY_NORMAL, 1000 };
AUDIOGAINCLASS agcParms     = { WAGC_PRIORITY_HIGH,   2000 };
ULONG          cbReturn     = 0;
DWORD          dwGUIDIndex  = 0;
DWORD          dwReturn     = TPR_PASS;
const GUID     GUID_Array[] = { MM_PROPSET_GAINCLASS_CLASS, GUID_Bad };
MMRESULT       MMResult     = MMSYSERR_NOERROR;

    // Test waveOut get and set property functions uising both valid & invalid
    // GUIDs.
    for( dwGUIDIndex = VALID_GUID; dwGUIDIndex <= INVALID_GUID; dwGUIDIndex++ )
    {
        // waveOutGetProperty()
        dwReturn = GetReturnCode( dwReturn, ProcessWaveOutPropFunctionResults(
            waveOutGetProperty( 0, &GUID_Array[ dwGUIDIndex ],
            MM_PROP_GAINCLASS_CLASS, &agcParms, sizeof( agcParms ),
            (LPVOID)&agcData, sizeof( agcData ), &cbReturn ),
            dwGUIDIndex, TEXT( "waveOutGetProperty()" ) ) ); 

        // waveOutSetProperty()
        dwReturn = GetReturnCode( dwReturn, ProcessWaveOutPropFunctionResults(
            waveOutSetProperty( 0, &GUID_Array[ dwGUIDIndex ],
            MM_PROP_GAINCLASS_CLASS, &agcParms, sizeof( agcParms ),
            (LPVOID)&agcData, sizeof( agcData ) ),
            dwGUIDIndex, TEXT( "waveOutSetProperty()" ) ) ); 

    } //  for dwGUIDIndex
    return dwReturn;
} // BadGUID

 /* 
  * Function Name: ProcessWaveOutPropFunctionResults
  *
  * Purpose: Helper Function to evaluate waveOut get & set property function
  *          results, and log appropriate comments.
  */

DWORD ProcessWaveOutPropFunctionResults(
    MMRESULT MMResult, DWORD dwGUIDIndex, const TCHAR *szID )
{
    DWORD dwReturn = TPR_PASS;

    if( dwGUIDIndex == VALID_GUID )
    {
        if( MMSYSERR_NOERROR == MMResult )
            LOG( TEXT( "%s succeeded for valid GUID." ), szID );
        else // !MMSYSERR_NOERROR
        {
		    LOG( TEXT( "FAIL in %s @ line %u:" ), TEXT( __FILE__ ), __LINE__ );
		    LOG( TEXT( "\t%s failed with valid GUID. MMResult = %u" ),
                szID, MMResult );
            dwReturn = TPR_FAIL;
        } // else !MMSYSERR_NOERROR
    } // if dwGUIDIndex VALID_GUID
    else // dwGUIDIndex !VALID_GUID
    {
        if( MMSYSERR_NOTSUPPORTED == MMResult )
            LOG( TEXT( "%s successfully trapped an invalid GUID. MMResult = %u"
                ), szID, MMResult );
        else // !MMSYSERR_NOTSUPPORTED
        {
		    LOG( TEXT(
                "FAIL:\t%s failed to trap an invalid GUID. MMResult = %u (expected %u)"
                ), szID, MMResult, MMSYSERR_NOTSUPPORTED );
            dwReturn = TPR_FAIL;
        } // else !MMSYSERR_NOTSUPPORTED
    } // else dwGUIDIndex !VALID_GUID
    return dwReturn;
} // ProcessWaveOutPropFunctionResults()
