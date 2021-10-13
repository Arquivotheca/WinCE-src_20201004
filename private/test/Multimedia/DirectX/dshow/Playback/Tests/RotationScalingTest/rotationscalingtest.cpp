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
//  RotationScalingTest function table
//
//
////////////////////////////////////////////////////////////////////////////////

#include "RotationScalingTest.h"
#include "TestDescParser.h"
#include "RotationScalingTestDescParser.h"
//we need to include this file so that the functions under it can be built individually for each test
//if this is not included there will be linker errors
#include "Verifier.h"

GraphFunctionTableEntry g_lpLocalFunctionTable[] = 
{
    {L"Rotation Scaling VMR Test", L"Rotation_Scaling_Test_VMR", Rotation_Scaling_Test_VMR, ParseRotationScalingTestDesc},
    {L"Rotation Scaling Frame Step Test", L"Rotation_Scaling_FrameStep_Test", Rotation_Scaling_FrameStep_Test, ParseRotationScalingTestDesc},
    {L"Rotation Scaling Stress Test", L"Rotation_Scaling_Stress_Test", Rotation_Scaling_Stress_Test, ParseRotationScalingTestDesc},

    // Manual test the rotation scaling in the secure chamber and visually inspect the video image.
    {L"Rotation Scaling VMR Test: test rotation and scaling in secure chamber", L"RotationScalingManualTest", Rotation_Scaling_Manual_Test, ParseRotationScalingTestDesc},
};
int g_nLocalTestFunctions = countof(g_lpLocalFunctionTable);
