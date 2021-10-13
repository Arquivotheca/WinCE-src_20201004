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
// 2D Tex Coords - Colorless
//
#define D3DMFVFTEST_TWOD_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE2(0))

typedef struct _D3DMALPHATESTTEST_TWOD {
	float x, y, z, rhw;
	float u,v;
} D3DMALPHATESTTEST_TWOD;

//
// The primitive will have an alpha gradient going from transparent at top to opaque at bottom.
//
static D3DMALPHATESTTEST_TWOD AlphaTestTwoD[4] = {
//  |       |       |       |       |       TEX SET #1      |
//  |   X   |   Y   |   Z   |  RHW  |     u     |     v     |
//  +-------+-------+-------+-------+-----------+-----------+
    {  POSX1,  POSY1,  POSZ1,   1.0f,       0.0f,       0.0f},
    {  POSX2,  POSY2,  POSZ2,   1.0f,       1.0f,       0.0f},
    {  POSX3,  POSY3,  POSZ3,   1.0f,       0.0f,       1.0f},
    {  POSX4,  POSY4,  POSZ4,   1.0f,       1.0f,       1.0f}
};

//
// 2D Tex Coords - Alpha Gradient
//
#define D3DMFVFTEST_TWOD_COLOR_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE2(0) | D3DMFVF_DIFFUSE)

typedef struct _D3DMALPHATESTTEST_TWOD_COLOR {
	float x, y, z, rhw;
	DWORD diffuse;
	float u,v;
} D3DMALPHATESTTEST_TWOD_COLOR;

//
// The primitive will have an alpha gradient going from transparent at top to opaque at bottom.
//
static D3DMALPHATESTTEST_TWOD_COLOR AlphaTestTwoDAlphaGradient[4] = {
//  |       |       |       |       |                                       |       TEX SET #1      |
//  |   X   |   Y   |   Z   |  RHW  |             Diffuse Color             |     u     |     v     |
//  +-------+-------+-------+-------+---------------------------------------+-----------+-----------+
    {  POSX1,  POSY1,  POSZ1,   1.0f, D3DMCOLOR_ARGB(0x00, 0xff, 0xff, 0xff),       0.0f,       0.0f},
    {  POSX2,  POSY2,  POSZ2,   1.0f, D3DMCOLOR_ARGB(0x00, 0xff, 0xff, 0xff),       1.0f,       0.0f},
    {  POSX3,  POSY3,  POSZ3,   1.0f, D3DMCOLOR_ARGB(0xff, 0xff, 0xff, 0xff),       0.0f,       1.0f},
    {  POSX4,  POSY4,  POSZ4,   1.0f, D3DMCOLOR_ARGB(0xff, 0xff, 0xff, 0xff),       1.0f,       1.0f}
};

//
// The primitives will be side-by-side with different alphas
//
static D3DMALPHATESTTEST_TWOD_COLOR AlphaTestTwoDAlphaDifBlocks[18] = {
//  |       |       |       |       |                                       |       TEX SET #1      |
//  |   X   |   Y   |   Z   |  RHW  |             Diffuse Color             |     u     |     v     |
//  +-------+-------+-------+-------+---------------------------------------+-----------+-----------+
    {   0.0f,   1.0f,  POSZ1,   1.0f, D3DMCOLOR_ARGB(0x00, 0xff, 0xff, 0xff),       0.0f,       0.0f},
    {   0.0f,   0.0f,  POSZ2,   1.0f, D3DMCOLOR_ARGB(0x00, 0xff, 0xff, 0xff),       0.0f,       1.0f},
    { 0.125f,   1.0f,  POSZ3,   1.0f, D3DMCOLOR_ARGB(0x01, 0xff, 0xff, 0xff),     0.125f,       0.0f},
    { 0.125f,   0.0f,  POSZ4,   1.0f, D3DMCOLOR_ARGB(0x01, 0xff, 0xff, 0xff),     0.125f,       1.0f},
    {  0.25f,   1.0f,  POSZ1,   1.0f, D3DMCOLOR_ARGB(0x7e, 0xff, 0xff, 0xff),      0.25f,       0.0f},
    {  0.25f,   0.0f,  POSZ1,   1.0f, D3DMCOLOR_ARGB(0x7e, 0xff, 0xff, 0xff),      0.25f,       1.0f},
    { 0.375f,   1.0f,  POSZ1,   1.0f, D3DMCOLOR_ARGB(0x7f, 0xff, 0xff, 0xff),     0.375f,       0.0f},
    { 0.375f,   0.0f,  POSZ1,   1.0f, D3DMCOLOR_ARGB(0x7f, 0xff, 0xff, 0xff),     0.375f,       1.0f},
    {   0.5f,   1.0f,  POSZ1,   1.0f, D3DMCOLOR_ARGB(0x80, 0xff, 0xff, 0xff),       0.5f,       0.0f},
    {   0.5f,   0.0f,  POSZ1,   1.0f, D3DMCOLOR_ARGB(0x80, 0xff, 0xff, 0xff),       0.5f,       1.0f},
    { 0.625f,   1.0f,  POSZ1,   1.0f, D3DMCOLOR_ARGB(0xfd, 0xff, 0xff, 0xff),     0.625f,       0.0f},
    { 0.625f,   0.0f,  POSZ1,   1.0f, D3DMCOLOR_ARGB(0xfd, 0xff, 0xff, 0xff),     0.625f,       1.0f},
    {  0.75f,   1.0f,  POSZ1,   1.0f, D3DMCOLOR_ARGB(0xfe, 0xff, 0xff, 0xff),      0.75f,       0.0f},
    {  0.75f,   0.0f,  POSZ1,   1.0f, D3DMCOLOR_ARGB(0xfe, 0xff, 0xff, 0xff),      0.75f,       1.0f},
    { 0.875f,   1.0f,  POSZ1,   1.0f, D3DMCOLOR_ARGB(0xff, 0xff, 0xff, 0xff),     0.875f,       0.0f},
    { 0.875f,   0.0f,  POSZ1,   1.0f, D3DMCOLOR_ARGB(0xff, 0xff, 0xff, 0xff),     0.875f,       1.0f},
    {   1.0f,   1.0f,  POSZ1,   1.0f, D3DMCOLOR_ARGB(0xff, 0xff, 0xff, 0xff),       1.0f,       0.0f},
    {   1.0f,   0.0f,  POSZ1,   1.0f, D3DMCOLOR_ARGB(0xff, 0xff, 0xff, 0xff),       1.0f,       1.0f},
};

