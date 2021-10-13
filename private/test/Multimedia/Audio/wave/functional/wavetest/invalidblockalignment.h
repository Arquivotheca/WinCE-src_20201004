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

//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------
#pragma once
#ifndef __InvalidBlockAlignment_H__
#define __InvalidBlockAlignment_H__

#include "TEST_WAVETEST.H"
#include "TUXMAIN.H"

#define BITSPERSAMPLE  8
#define CHANNELS       2
#define SAMPLESPERSEC  44100
#define BLOCKALIGN     CHANNELS * BITSPERSAMPLE / 8
#define AVGBYTESPERSEC SAMPLESPERSEC * BLOCKALIGN
#define SIZE           0


TESTPROCAPI InvalidBlockAlignment( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );


DWORD TestValidWaveInAlignment( HWAVEIN * phwi, WAVEFORMATEX * pwfx, WAVEHDR * pwh );
DWORD TestValidWaveOutAlignment( HWAVEOUT * phwo, WAVEFORMATEX * pwfx, WAVEHDR * pwh );

DWORD TestInvalidWaveInAlignment( HWAVEIN * phwi, WAVEFORMATEX * pwfx, WAVEHDR * pwh );
DWORD TestInvalidWaveOutAlignment( HWAVEOUT * phwo, WAVEFORMATEX * pwfx, WAVEHDR * pwh );

#endif