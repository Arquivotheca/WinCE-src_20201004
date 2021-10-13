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

//
//
//   |      Test Data      | Description
//   +---------------------+----------------------------------------------------------------
//   | D3DMFVFTEST_ONLY01  | Floating point primitive - position only



//
// Position Only - Case 01
//
#define D3DMFVFTEST_ONLY01_FVF (D3DMFVF_XYZRHW_FLOAT)

typedef struct _D3DMFVFTEST_ONLY01 {
	float x, y, z, rhw;
} D3DMFVFTEST_ONLY01;

static D3DMFVFTEST_ONLY01 FVFOnly01[D3DQA_NUMVERTS] = {
//  |   X   |   Y   |   Z   |  RHW  |
//  +-------+-------+-------+-------+
    {  POSX1,  POSY1,  POSZ1,   1.0f},
    {  POSX2,  POSY2,  POSZ2,   1.0f},
    {  POSX3,  POSY3,  POSZ3,   1.0f},
    {  POSX4,  POSY4,  POSZ4,   1.0f}
};
