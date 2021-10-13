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
// 2D Tex Coords - Case 01
//
#define D3DMFVFTEST_TWOD01_FVF (D3DMFVF_XYZ_FLOAT | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE2(0))

typedef struct _D3DMTEXWRAPTEST_TWOD {
	float x, y, z;
	float u,v;
} D3DMTEXWRAPTEST_TWOD;

static D3DMTEXWRAPTEST_TWOD TexWrapTwoD01[D3DMQA_NUMVERTS] = {
//  |       |       |       |       TEX SET #1      |
//  |   X   |   Y   |   Z   |     u     |     v     |
//  +-------+-------+-------+-----------+-----------+
    {  POSX1,  POSY1,  POSZ1, TEXTUREMIN, TEXTUREMIN},
    {  POSX2,  POSY2,  POSZ2, TEXTUREMAX, TEXTUREMIN},
    {  POSX3,  POSY3,  POSZ3, TEXTUREMIN, TEXTUREMAX},
    {  POSX4,  POSY4,  POSZ4, TEXTUREMAX, TEXTUREMAX}
};

static D3DMTEXWRAPTEST_TWOD TexWrapTwoD02[D3DMQA_NUMVERTS] = {
//  |                      |       |       |       TEX SET #1      |
//  |       X              |   Y   |   Z   |     u     |     v     |
//  +----------------------+-------+-------+-----------+-----------+
    {  POSX1 - POSHALFWIDTH,  POSY1,  POSZ1, TEXTUREMIN, TEXTUREMIN},
    {  POSX2 - POSHALFWIDTH,  POSY2,  POSZ2, TEXTUREMAX, TEXTUREMIN},
    {  POSX3 - POSHALFWIDTH,  POSY3,  POSZ3, TEXTUREMIN, TEXTUREMAX},
    {  POSX4 - POSHALFWIDTH,  POSY4,  POSZ4, TEXTUREMAX, TEXTUREMAX}
};
static D3DMTEXWRAPTEST_TWOD TexWrapTwoD03[D3DMQA_NUMVERTS] = {
//  |       |                       |       |       TEX SET #1      |
//  |   X   |       Y               |   Z   |     u     |     v     |
//  +-------+-----------------------+-------+-----------+-----------+
    {  POSX1,  POSY1 - POSHALFHEIGHT,  POSZ1, TEXTUREMIN, TEXTUREMIN},
    {  POSX2,  POSY2 - POSHALFHEIGHT,  POSZ2, TEXTUREMAX, TEXTUREMIN},
    {  POSX3,  POSY3 - POSHALFHEIGHT,  POSZ3, TEXTUREMIN, TEXTUREMAX},
    {  POSX4,  POSY4 - POSHALFHEIGHT,  POSZ4, TEXTUREMAX, TEXTUREMAX}
};
static D3DMTEXWRAPTEST_TWOD TexWrapTwoD04[D3DMQA_NUMVERTS] = {
//  |                      |                       |       |       TEX SET #1      |
//  |       X              |       Y               |   Z   |     u     |     v     |
//  +----------------------+-----------------------+-------+-----------+-----------+
    {  POSX1 - POSHALFWIDTH,  POSY1 - POSHALFHEIGHT,  POSZ1, TEXTUREMIN, TEXTUREMIN},
    {  POSX2 - POSHALFWIDTH,  POSY2 - POSHALFHEIGHT,  POSZ2, TEXTUREMAX, TEXTUREMIN},
    {  POSX3 - POSHALFWIDTH,  POSY3 - POSHALFHEIGHT,  POSZ3, TEXTUREMIN, TEXTUREMAX},
    {  POSX4 - POSHALFWIDTH,  POSY4 - POSHALFHEIGHT,  POSZ4, TEXTUREMAX, TEXTUREMAX}
};

