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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#pragma once
#ifndef __BadDeviceID_H__
#define __BadDeviceID_H__

#include "TUXMAIN.H"

#define BITSPERSAMPLE  8
#define CHANNELS       2
#define SAMPLESPERSEC  44100
#define BLOCKALIGN     CHANNELS * BITSPERSAMPLE / 8
#define AVGBYTESPERSEC SAMPLESPERSEC * BLOCKALIGN
#define SIZE           0

TESTPROCAPI BadDeviceID(
    UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );

// Helper Functions for use by the tests
DWORD ProcessWaveformFunctionResults( MMRESULT MMResult, UINT uWaveOutDevNum,
    UINT uNumWaveOutNumDevs, const TCHAR *szID );

#endif