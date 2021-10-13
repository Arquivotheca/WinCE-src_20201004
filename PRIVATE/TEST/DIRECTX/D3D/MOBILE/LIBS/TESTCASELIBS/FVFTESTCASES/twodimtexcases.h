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
//   | D3DMFVFTEST_TWOD01  | Floating point primitive with 1 set 2D tex coords
//   | D3DMFVFTEST_TWOD02  | Floating point primitive with 2 sets 2D tex coords
//   | D3DMFVFTEST_TWOD03  | Floating point primitive with 3 sets 2D tex coords
//   | D3DMFVFTEST_TWOD04  | Floating point primitive with 4 sets 2D tex coords
//   | D3DMFVFTEST_TWOD05  | Floating point primitive with 1 set 2D tex coords; blended with diffuse
//   | D3DMFVFTEST_TWOD06  | Floating point primitive with 1 set 2D tex coords; blended with specular
//


//
// 2D Tex Coords - Case 01
//
#define D3DMFVFTEST_TWOD01_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE2(0))

typedef struct _D3DMFVFTEST_TWOD01 {
	float x, y, z, rhw;
	float u,v;
} D3DMFVFTEST_TWOD01;

static D3DMFVFTEST_TWOD01 FVFTwoD01[D3DQA_NUMVERTS] = {
//  |       |       |       |       |       TEX SET #1      |
//  |   X   |   Y   |   Z   |  RHW  |     u     |     v     |
//  +-------+-------+-------+-------+-----------+-----------+
    {  POSX1,  POSY1,  POSZ1,   1.0f,       0.0f,       0.0f},
    {  POSX2,  POSY2,  POSZ2,   1.0f,       1.0f,       0.0f},
    {  POSX3,  POSY3,  POSZ3,   1.0f,       0.0f,       1.0f},
    {  POSX4,  POSY4,  POSZ4,   1.0f,       1.0f,       1.0f}
};


//
// 2D Tex Coords - Case 02
//
#define D3DMFVFTEST_TWOD02_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_TEX2 | D3DMFVF_TEXCOORDSIZE2(0) | D3DMFVF_TEXCOORDSIZE2(1))

typedef struct _D3DMFVFTEST_TWOD02 {
	float x, y, z, rhw;
	float u1,v1;
	float u2,v2;
} D3DMFVFTEST_TWOD02;

static D3DMFVFTEST_TWOD02 FVFTwoD02[D3DQA_NUMVERTS] = {
//  |       |       |       |       |       TEX SET #1      |       TEX SET #2      |
//  |   X   |   Y   |   Z   |  RHW  |     u     |     v     |     u     |     v     |
//  +-------+-------+-------+-------+-----------+-----------+-----------+-----------+
    {  POSX1,  POSY1,  POSZ1,   1.0f,       0.0f,       0.0f,       0.0f,       0.0f},
    {  POSX2,  POSY2,  POSZ2,   1.0f,       1.0f,       0.0f,       1.0f,       0.0f},
    {  POSX3,  POSY3,  POSZ3,   1.0f,       0.0f,       1.0f,       0.0f,       1.0f},
    {  POSX4,  POSY4,  POSZ4,   1.0f,       1.0f,       1.0f,       1.0f,       1.0f}
};



//
// 2D Tex Coords - Case 03
//
#define D3DMFVFTEST_TWOD03_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_TEX3 | D3DMFVF_TEXCOORDSIZE2(0) | D3DMFVF_TEXCOORDSIZE2(1) | D3DMFVF_TEXCOORDSIZE2(2))

typedef struct _D3DMFVFTEST_TWOD03 {
	float x, y, z, rhw;
	float u1,v1;
	float u2,v2;
	float u3,v3;
} D3DMFVFTEST_TWOD03;

static D3DMFVFTEST_TWOD03 FVFTwoD03[D3DQA_NUMVERTS] = {
//  |       |       |       |       |       TEX SET #1      |       TEX SET #2      |       TEX SET #3      |
//  |   X   |   Y   |   Z   |  RHW  |     u     |     v     |     u     |     v     |     u     |     v     |
//  +-------+-------+-------+-------+-----------+-----------+-----------+-----------+-----------+-----------+
    {  POSX1,  POSY1,  POSZ1,   1.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f},
    {  POSX2,  POSY2,  POSZ2,   1.0f,       1.0f,       0.0f,       1.0f,       0.0f,       1.0f,       0.0f},
    {  POSX3,  POSY3,  POSZ3,   1.0f,       0.0f,       1.0f,       0.0f,       1.0f,       0.0f,       1.0f},
    {  POSX4,  POSY4,  POSZ4,   1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f}
};


//
// 2D Tex Coords - Case 04
//
#define D3DMFVFTEST_TWOD04_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_TEX4 | D3DMFVF_TEXCOORDSIZE2(0) | D3DMFVF_TEXCOORDSIZE2(1) | D3DMFVF_TEXCOORDSIZE2(2) | D3DMFVF_TEXCOORDSIZE2(3))

typedef struct _D3DMFVFTEST_TWOD04 {
	float x, y, z, rhw;
	float u1,v1;
	float u2,v2;
	float u3,v3;
	float u4,v4;
} D3DMFVFTEST_TWOD04;

static D3DMFVFTEST_TWOD04 FVFTwoD04[D3DQA_NUMVERTS] = {
//  |       |       |       |       |       TEX SET #1      |       TEX SET #2      |       TEX SET #3      |       TEX SET #4      |
//  |   X   |   Y   |   Z   |  RHW  |     u     |     v     |     u     |     v     |     u     |     v     |     u     |     v     |
//  +-------+-------+-------+-------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+
    {  POSX1,  POSY1,  POSZ1,   1.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f},
    {  POSX2,  POSY2,  POSZ2,   1.0f,       1.0f,       0.0f,       1.0f,       0.0f,       1.0f,       0.0f,       1.0f,       0.0f},
    {  POSX3,  POSY3,  POSZ3,   1.0f,       0.0f,       1.0f,       0.0f,       1.0f,       0.0f,       1.0f,       0.0f,       1.0f},
    {  POSX4,  POSY4,  POSZ4,   1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f}
};



//
// 2D Tex Coords - Case 05
//
#define D3DMFVFTEST_TWOD05_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_DIFFUSE | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE2(0))

typedef struct _D3DMFVFTEST_TWOD05 {
	float x, y, z, rhw;
	DWORD Diffuse;
	float u1,v1;
} D3DMFVFTEST_TWOD05;

static D3DMFVFTEST_TWOD05 FVFTwoD05[D3DQA_NUMVERTS] = {
//  |       |       |       |       |           |       TEX SET #1      |
//  |   X   |   Y   |   Z   |  RHW  |  Diffuse  |     u     |     v     |
//  +-------+-------+-------+-------+-----------+-----------+-----------+
    {  POSX1,  POSY1,  POSZ1,   1.0f, 0xFFFF8080,       0.0f,       0.0f},
    {  POSX2,  POSY2,  POSZ2,   1.0f, 0xFF40FF40,       1.0f,       0.0f},
    {  POSX3,  POSY3,  POSZ3,   1.0f, 0xFF6060FF,       0.0f,       1.0f},
    {  POSX4,  POSY4,  POSZ4,   1.0f, 0xFFFF2020,       1.0f,       1.0f}
};


//
// 2D Tex Coords - Case 06
//
#define D3DMFVFTEST_TWOD06_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_SPECULAR | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE2(0))

typedef struct _D3DMFVFTEST_TWOD06 {
	float x, y, z, rhw;
	DWORD Specular;
	float u1,v1;
} D3DMFVFTEST_TWOD06;

static D3DMFVFTEST_TWOD06 FVFTwoD06[D3DQA_NUMVERTS] = {
//  |       |       |       |       |           |       TEX SET #1      |
//  |   X   |   Y   |   Z   |  RHW  |  Specular |     u     |     v     |
//  +-------+-------+-------+-------+-----------+-----------+-----------+
    {  POSX1,  POSY1,  POSZ1,   1.0f, 0xFFFF8080,       0.0f,       0.0f},
    {  POSX2,  POSY2,  POSZ2,   1.0f, 0xFF40FF40,       1.0f,       0.0f},
    {  POSX3,  POSY3,  POSZ3,   1.0f, 0xFF6060FF,       0.0f,       1.0f},
    {  POSX4,  POSY4,  POSZ4,   1.0f, 0xFFFF2020,       1.0f,       1.0f}
};
