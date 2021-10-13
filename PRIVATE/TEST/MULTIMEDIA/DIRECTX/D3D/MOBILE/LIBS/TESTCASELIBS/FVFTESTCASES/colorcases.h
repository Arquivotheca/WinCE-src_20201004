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
//   | D3DMFVFTEST_COLR01  | Floating point primitive with diffuse
//   | D3DMFVFTEST_COLR02  | Floating point primitive with specular
//   | D3DMFVFTEST_COLR03  | Floating point primitive with diffuse + specular
//

//
// Color vertex - Case 01
//
#define D3DMFVFTEST_COLR01_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_DIFFUSE)

typedef struct _D3DMFVFTEST_COLR01 {
	float x, y, z, rhw;
	D3DMCOLOR Diffuse;
} D3DMFVFTEST_COLR01;

static D3DMFVFTEST_COLR01 FVFColr01[D3DQA_NUMVERTS] = {
//  |   X   |   Y   |   Z   |  RHW  |  Diffuse   |
//  +-------+-------+-------+-------+------------+
    {  POSX1,  POSY1,  POSZ1,   1.0f, 0xFFF00000},
    {  POSX2,  POSY2,  POSZ2,   1.0f, 0xFF00F000},
    {  POSX3,  POSY3,  POSZ3,   1.0f, 0xFF0000F0},
    {  POSX4,  POSY4,  POSZ4,   1.0f, 0xFFF0F0F0}
};


//
// Color vertex - Case 02
//
#define D3DMFVFTEST_COLR02_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_SPECULAR)

typedef struct _D3DMFVFTEST_COLR02 {
	float x, y, z, rhw;
	D3DMCOLOR Specular;
} D3DMFVFTEST_COLR02;

static D3DMFVFTEST_COLR02 FVFColr02[D3DQA_NUMVERTS] = {
//  |   X   |   Y   |   Z   |  RHW  |  Specular  |
//  +-------+-------+-------+-------+------------+
    {  POSX1,  POSY1,  POSZ1,   1.0f, 0xFFF00000},
    {  POSX2,  POSY2,  POSZ2,   1.0f, 0xFF00F000},
    {  POSX3,  POSY3,  POSZ3,   1.0f, 0xFF0000F0},
    {  POSX4,  POSY4,  POSZ4,   1.0f, 0xFFF0F0F0}
};

//
// Color vertex - Case 03
//
#define D3DMFVFTEST_COLR03_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_DIFFUSE | D3DMFVF_SPECULAR)

typedef struct _D3DMFVFTEST_COLR03 {
	float x, y, z, rhw;
	D3DMCOLOR Diffuse;
	D3DMCOLOR Specular;
} D3DMFVFTEST_COLR03;

static D3DMFVFTEST_COLR03 FVFColr03[D3DQA_NUMVERTS] = {
//  |   X   |   Y   |   Z   |  RHW  |  Diffuse  |  Specular  |
//  +-------+-------+-------+-------+-----------+------------+
    {  POSX1,  POSY1,  POSZ1,   1.0f, 0xFFF00000, 0xFFC0C0C0},
    {  POSX2,  POSY2,  POSZ2,   1.0f, 0xFF00F000, 0xFFC0C0C0},
    {  POSX3,  POSY3,  POSZ3,   1.0f, 0xFF0000F0, 0xFFC0C0C0},
    {  POSX4,  POSY4,  POSZ4,   1.0f, 0xFFF0F0F0, 0xFFC0C0C0}
};
