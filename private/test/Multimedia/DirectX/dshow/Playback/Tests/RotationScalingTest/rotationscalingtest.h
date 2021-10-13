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
////////////////////////////////////////////////////////////////////////////////
//
//  GraphTest helper class
//
//
////////////////////////////////////////////////////////////////////////////////

#ifndef ROTATIONSCALING_TEST_H
#define ROTATIONSCALING_TEST_H

#include <tux.h>
#include "TestDesc.h"
#include "globals.h"
#include "RS_Util.h"

// Test functions

// Rotation Scaling Scenarios

// This test CoCreates a filter graph and queries the commonly used playback interfaces. 
// This test fails if the essential interfaces are not retrieved. 
// IMediaControl, IMediaEvent, IMediaSeeking are considered to be essential
// Config File: no dependency
TESTPROCAPI Rotation_Scaling_Test_VMR(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI Rotation_Scaling_FrameStep_Test(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI Rotation_Scaling_Stress_Test(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI Rotation_Scaling_Manual_Test( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );

#endif
