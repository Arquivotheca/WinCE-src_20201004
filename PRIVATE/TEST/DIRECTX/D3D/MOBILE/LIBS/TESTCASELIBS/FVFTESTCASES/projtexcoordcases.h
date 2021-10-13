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
//   | D3DMFVFTEST_PROJ01  | Floating point projected to 1D tex coords (vertical)
//   | D3DMFVFTEST_PROJ02  | Floating point projected to 1D tex coords (horizontal)
//   | D3DMFVFTEST_PROJ03  | Floating point projected to 2D tex coords
//   | D3DMFVFTEST_PROJ04  | Floating point projected to 1D tex coords (vertical); blended with diffuse
//   | D3DMFVFTEST_PROJ05  | Floating point projected to 1D tex coords (horizontal); blended with diffuse
//   | D3DMFVFTEST_PROJ06  | Floating point projected to 2D tex coords; blended with diffuse
//   | D3DMFVFTEST_PROJ07  | Floating point projected to 1D tex coords (vertical); blended with specular
//   | D3DMFVFTEST_PROJ08  | Floating point projected to 1D tex coords (horizontal); blended with specular
//   | D3DMFVFTEST_PROJ09  | Floating point projected to 2D tex coords; blended with specular

//
// Projected Textures - Case 01
//
#define D3DMFVFTEST_PROJ01_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE2(0))

typedef struct _D3DMFVFTEST_PROJ01 {
	float x, y, z, rhw;
	float u,p;
} D3DMFVFTEST_PROJ01;

static D3DMFVFTEST_PROJ01 FVFProj01[D3DQA_NUMVERTS] = {
//  |       |       |       |       | TEX SET #1             |
//  |   X   |   Y   |   Z   |  RHW  |     u      |  project  |
//  +-------+-------+-------+-------+------------+-----------+
    {  POSX1,  POSY1,  POSZ1,   1.0f,        0.0f,       2.0f},
    {  POSX2,  POSY2,  POSZ2,   1.0f,        1.0f,       2.0f},
    {  POSX3,  POSY3,  POSZ3,   1.0f,        0.0f,       2.0f},
    {  POSX4,  POSY4,  POSZ4,   1.0f,        1.0f,       2.0f}
};




//
// Projected Textures - Case 02
//
#define D3DMFVFTEST_PROJ02_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE2(0))

typedef struct _D3DMFVFTEST_PROJ02 {
	float x, y, z, rhw;
	float v,p;
} D3DMFVFTEST_PROJ02;

static D3DMFVFTEST_PROJ01 FVFProj02[D3DQA_NUMVERTS] = {
//  |       |       |       |       | TEX SET #1             |
//  |   X   |   Y   |   Z   |  RHW  |     v      |  project  |
//  +-------+-------+-------+-------+------------+-----------+
    {  POSX1,  POSY1,  POSZ1,   1.0f,        0.0f,       2.0f},
    {  POSX2,  POSY2,  POSZ2,   1.0f,        0.0f,       2.0f},
    {  POSX3,  POSY3,  POSZ3,   1.0f,        1.0f,       2.0f},
    {  POSX4,  POSY4,  POSZ4,   1.0f,        1.0f,       2.0f}
};


//
// Projected Textures - Case 03
//
#define D3DMFVFTEST_PROJ03_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE3(0))

typedef struct _D3DMFVFTEST_PROJ03 {
	float x, y, z, rhw;
	float u,v,p;
} D3DMFVFTEST_PROJ03;

static D3DMFVFTEST_PROJ03 FVFProj03[D3DQA_NUMVERTS] = {
//  |       |       |       |       |            TEX SET #1             |     
//  |   X   |   Y   |   Z   |  RHW  |     u     |     v     |  project  |
//  +-------+-------+-------+-------+-----------+-----------+-----------+
    {  POSX1,  POSY1,  POSZ1,   1.0f,       0.0f,       0.0f,       2.0f},
    {  POSX2,  POSY2,  POSZ2,   1.0f,       1.0f,       0.0f,       2.0f},
    {  POSX3,  POSY3,  POSZ3,   1.0f,       0.0f,       1.0f,       2.0f},
    {  POSX4,  POSY4,  POSZ4,   1.0f,       1.0f,       1.0f,       2.0f}
};



//
// Projected Textures - Case 04
//
#define D3DMFVFTEST_PROJ04_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_DIFFUSE | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE2(0))

typedef struct _D3DMFVFTEST_PROJ04 {
	float x, y, z, rhw;
	DWORD Diffuse;
	float u,p;
} D3DMFVFTEST_PROJ04;

static D3DMFVFTEST_PROJ04 FVFProj04[D3DQA_NUMVERTS] = {
//  |       |       |       |       |           | TEX SET #1             |
//  |   X   |   Y   |   Z   |  RHW  |  Diffuse  |     u      |  project  |
//  +-------+-------+-------+-------+-----------+------------+-----------+
    {  POSX1,  POSY1,  POSZ1,   1.0f, 0xFFFF8080,        0.0f,       2.0f},
    {  POSX2,  POSY2,  POSZ2,   1.0f, 0xFF40FF40,        1.0f,       2.0f},
    {  POSX3,  POSY3,  POSZ3,   1.0f, 0xFF6060FF,        0.0f,       2.0f},
    {  POSX4,  POSY4,  POSZ4,   1.0f, 0xFFFF2020,        1.0f,       2.0f}
};

//
// Projected Textures - Case 05
//
#define D3DMFVFTEST_PROJ05_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_DIFFUSE | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE2(0))

typedef struct _D3DMFVFTEST_PROJ05 {
	float x, y, z, rhw;
	DWORD Diffuse;
	float v,p;
} D3DMFVFTEST_PROJ05;

static D3DMFVFTEST_PROJ05 FVFProj05[D3DQA_NUMVERTS] = {
//  |       |       |       |       |           | TEX SET #1             |
//  |   X   |   Y   |   Z   |  RHW  |  Diffuse  |     v      |  project  |
//  +-------+-------+-------+-------+-----------+------------+-----------+
    {  POSX1,  POSY1,  POSZ1,   1.0f, 0xFFFF8080,        0.0f,       2.0f},
    {  POSX2,  POSY2,  POSZ2,   1.0f, 0xFF40FF40,        0.0f,       2.0f},
    {  POSX3,  POSY3,  POSZ3,   1.0f, 0xFF6060FF,        1.0f,       2.0f},
    {  POSX4,  POSY4,  POSZ4,   1.0f, 0xFFFF2020,        1.0f,       2.0f}
};

//
// Projected Textures - Case 06
//
#define D3DMFVFTEST_PROJ06_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_DIFFUSE | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE3(0))

typedef struct _D3DMFVFTEST_PROJ06 {
	float x, y, z, rhw;
	DWORD Diffuse;
	float u,v,p;
} D3DMFVFTEST_PROJ06;

static D3DMFVFTEST_PROJ06 FVFProj06[D3DQA_NUMVERTS] = {
//  |       |       |       |       |           |            TEX SET #1             |
//  |   X   |   Y   |   Z   |  RHW  |  Diffuse  |     u     |     v     |  project  |
//  +-------+-------+-------+-------+-----------+-----------+-----------+-----------+
    {  POSX1,  POSY1,  POSZ1,   1.0f, 0xFFFF8080,       0.0f,       0.0f,       2.0f},
    {  POSX2,  POSY2,  POSZ2,   1.0f, 0xFF40FF40,       1.0f,       0.0f,       2.0f},
    {  POSX3,  POSY3,  POSZ3,   1.0f, 0xFF6060FF,       0.0f,       1.0f,       2.0f},
    {  POSX4,  POSY4,  POSZ4,   1.0f, 0xFFFF2020,       1.0f,       1.0f,       2.0f}
};














//
// Projected Textures - Case 07
//
#define D3DMFVFTEST_PROJ07_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_DIFFUSE | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE2(0))

typedef struct _D3DMFVFTEST_PROJ07 {
	float x, y, z, rhw;
	DWORD Diffuse;
	float u,p;
} D3DMFVFTEST_PROJ07;

static D3DMFVFTEST_PROJ07 FVFProj07[D3DQA_NUMVERTS] = {
//  |       |       |       |       |           | TEX SET #1             |
//  |   X   |   Y   |   Z   |  RHW  |  Diffuse  |     u      |  project  |
//  +-------+-------+-------+-------+-----------+------------+-----------+
    {  POSX1,  POSY1,  POSZ1,   1.0f, 0xFFFF8080,        0.0f,       2.0f},
    {  POSX2,  POSY2,  POSZ2,   1.0f, 0xFF40FF40,        1.0f,       2.0f},
    {  POSX3,  POSY3,  POSZ3,   1.0f, 0xFF6060FF,        0.0f,       2.0f},
    {  POSX4,  POSY4,  POSZ4,   1.0f, 0xFFFF2020,        1.0f,       2.0f}
};

//
// Projected Textures - Case 08
//
#define D3DMFVFTEST_PROJ08_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_DIFFUSE | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE2(0))

typedef struct _D3DMFVFTEST_PROJ08 {
	float x, y, z, rhw;
	DWORD Diffuse;
	float v,p;
} D3DMFVFTEST_PROJ08;

static D3DMFVFTEST_PROJ08 FVFProj08[D3DQA_NUMVERTS] = {
//  |       |       |       |       |           | TEX SET #1             |
//  |   X   |   Y   |   Z   |  RHW  |  Diffuse  |     v      |  project  |
//  +-------+-------+-------+-------+-----------+------------+-----------+
    {  POSX1,  POSY1,  POSZ1,   1.0f, 0xFFFF8080,        0.0f,       2.0f},
    {  POSX2,  POSY2,  POSZ2,   1.0f, 0xFF40FF40,        0.0f,       2.0f},
    {  POSX3,  POSY3,  POSZ3,   1.0f, 0xFF6060FF,        1.0f,       2.0f},
    {  POSX4,  POSY4,  POSZ4,   1.0f, 0xFFFF2020,        1.0f,       2.0f}
};

//
// Projected Textures - Case 09
//
#define D3DMFVFTEST_PROJ09_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_DIFFUSE | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE3(0))

typedef struct _D3DMFVFTEST_PROJ09 {
	float x, y, z, rhw;
	DWORD Diffuse;
	float u,v,p;
} D3DMFVFTEST_PROJ09;

static D3DMFVFTEST_PROJ09 FVFProj09[D3DQA_NUMVERTS] = {
//  |       |       |       |       |           |            TEX SET #1             |
//  |   X   |   Y   |   Z   |  RHW  |  Diffuse  |     u     |     v     |  project  |
//  +-------+-------+-------+-------+-----------+-----------+-----------+-----------+
    {  POSX1,  POSY1,  POSZ1,   1.0f, 0xFFFF8080,       0.0f,       0.0f,       2.0f},
    {  POSX2,  POSY2,  POSZ2,   1.0f, 0xFF40FF40,       1.0f,       0.0f,       2.0f},
    {  POSX3,  POSY3,  POSZ3,   1.0f, 0xFF6060FF,       0.0f,       1.0f,       2.0f},
    {  POSX4,  POSY4,  POSZ4,   1.0f, 0xFFFF2020,       1.0f,       1.0f,       2.0f}
};


