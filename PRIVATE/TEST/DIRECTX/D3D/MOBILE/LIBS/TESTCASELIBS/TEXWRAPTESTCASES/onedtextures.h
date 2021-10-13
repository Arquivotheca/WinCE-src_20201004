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
// 1D Tex Coords - Case 01
//

#define D3DMFVFTEST_ONED01_FVF (D3DMFVF_XYZ_FLOAT | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE1(0))

typedef struct _D3DMTEXWRAPTEST_ONED {
	float x, y, z;
	float u;
} D3DMTEXWRAPTEST_ONED;

static D3DMTEXWRAPTEST_ONED TexWrapOneD01[D3DMQA_NUMVERTS] = {
//  |       |       |       | TEX SET #1 |
//  |   X   |   Y   |   Z   |     u      |
//  +-------+-------+-------+------------+
    {  POSX1,  POSY1,  POSZ1,  TEXTUREMIN},
    {  POSX2,  POSY2,  POSZ2,  TEXTUREMAX},
    {  POSX3,  POSY3,  POSZ3,  TEXTUREMIN},
    {  POSX4,  POSY4,  POSZ4,  TEXTUREMAX}
};

static D3DMTEXWRAPTEST_ONED TexWrapOneD02[D3DMQA_NUMVERTS] = {
//  |                      |       |       | TEX SET #1 |
//  |       X              |   Y   |   Z   |     u      |
//  +----------------------+-------+-------+------------+
    {  POSX1 - POSHALFWIDTH,  POSY1,  POSZ1,  TEXTUREMIN},
    {  POSX2 - POSHALFWIDTH,  POSY2,  POSZ2,  TEXTUREMAX},
    {  POSX3 - POSHALFWIDTH,  POSY3,  POSZ3,  TEXTUREMIN},
    {  POSX4 - POSHALFWIDTH,  POSY4,  POSZ4,  TEXTUREMAX}
};


