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
//   | D3DMFVFTEST_ONED01  | Floating point primitive with 1D tex coords (vertical)
//   | D3DMFVFTEST_ONED02  | Floating point primitive with 1D tex coords (horizontal)
//


//
// 1D Tex Coords - Case 01
//
#define D3DMFVFTEST_ONED01_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE1(0))

typedef struct _D3DMFVFTEST_ONED01 {
	float x, y, z, rhw;
	float u;
} D3DMFVFTEST_ONED01;

static D3DMFVFTEST_ONED01 FVFOneD01[D3DQA_NUMVERTS] = {
//  |       |       |       |       | TEX SET #1 |
//  |   X   |   Y   |   Z   |  RHW  |     u      |
//  +-------+-------+-------+-------+------------+
    {  POSX1,  POSY1,  POSZ1,   1.0f,        0.0f},
    {  POSX2,  POSY2,  POSZ2,   1.0f,        1.0f},
    {  POSX3,  POSY3,  POSZ3,   1.0f,        0.0f},
    {  POSX4,  POSY4,  POSZ4,   1.0f,        1.0f}
};


//
// 1D Tex Coords - Case 02
//
#define D3DMFVFTEST_ONED02_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE1(0))

typedef struct _D3DMFVFTEST_ONED02 {
	float x, y, z, rhw;
	float v;
} D3DMFVFTEST_ONED02;

static D3DMFVFTEST_ONED02 FVFOneD02[D3DQA_NUMVERTS] = {
//  |       |       |       |       | TEX SET #1 |
//  |   X   |   Y   |   Z   |  RHW  |     v      |
//  +-------+-------+-------+-------+------------+
    {  POSX1,  POSY1,  POSZ1,   1.0f,        0.0f},
    {  POSX2,  POSY2,  POSZ2,   1.0f,        0.0f},
    {  POSX3,  POSY3,  POSZ3,   1.0f,        1.0f},
    {  POSX4,  POSY4,  POSZ4,   1.0f,        1.0f}
};

