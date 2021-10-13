//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
