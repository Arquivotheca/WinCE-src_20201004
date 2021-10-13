//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once

#define D3DQA_NUMPRIM  2
#define D3DQA_PRIMTYPE D3DMPT_TRIANGLESTRIP
#define D3DQA_NUMVERTS 4

//    (1)        (2) 
//     +--------+  +
//     |       /  /|
//     |      /  / |
//     |     /  /  |
//     |    /  /   |
//     |   /  /    |
//     |  /  /     |
//     | /  /      | 
//     |/  /       |
//     +  +--------+
//    (3)         (4)
//

#define POSX1    0.0f
#define POSY1    0.0f
#define POSZ1    0.0f

#define POSX2    1.0f
#define POSY2    0.0f
#define POSZ2    0.0f

#define POSX3    0.0f
#define POSY3    1.0f
#define POSZ3    0.0f

#define POSX4    1.0f
#define POSY4    1.0f
#define POSZ4    0.0f

//
// FVF, structure, data
//
#define D3DMTEXSTAGETEST_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_DIFFUSE | D3DMFVF_SPECULAR | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE2(0))

typedef struct _D3DMTEXSTAGETEST_VERTS {
	float x, y, z, rhw;
	D3DMCOLOR Diffuse;
	D3DMCOLOR Specular;
	float u,v;
} D3DMTEXSTAGETEST_VERTS;

static D3DMTEXSTAGETEST_VERTS TexStageTestVerts[D3DQA_NUMVERTS] = {
//  |       |       |       |       |           |           |       TEX SET #1      |
//  |   X   |   Y   |   Z   |  RHW  |  Diffuse  |  Specular |     u     |     v     |
//  +-------+-------+-------+-------+-----------+-----------+-----------+-----------+
    {  POSX1,  POSY1,  POSZ1,   1.0f, 0xFFFFFFFF, 0xFFFFFFFF,       0.0f,       0.0f},
    {  POSX2,  POSY2,  POSZ2,   1.0f, 0xFFFFFFFF, 0xFFFFFFFF,       1.0f,       0.0f},
    {  POSX3,  POSY3,  POSZ3,   1.0f, 0xFFFFFFFF, 0xFFFFFFFF,       0.0f,       1.0f},
    {  POSX4,  POSY4,  POSZ4,   1.0f, 0xFFFFFFFF, 0xFFFFFFFF,       1.0f,       1.0f}
};

