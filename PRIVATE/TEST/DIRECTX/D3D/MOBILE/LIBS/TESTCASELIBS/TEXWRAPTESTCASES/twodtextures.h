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

