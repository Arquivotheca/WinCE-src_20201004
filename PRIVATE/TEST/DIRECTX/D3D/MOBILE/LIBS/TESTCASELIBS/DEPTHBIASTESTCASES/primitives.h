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
// Colored primitives.
//
#define D3DMDEPTHBIASTEST_COLOR_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_DIFFUSE)

typedef struct _D3DMDEPTHBIASTEST_COLOR {
	float x, y, z, rhw;
	DWORD diffuse;
} D3DMDEPTHBIASTEST_COLOR;

//
// The base primitive will be fullscreen and at 0.5 z
//
static D3DMDEPTHBIASTEST_COLOR DepthBiasBaseVertices[6] = {
//  |       |       |       |       |                                       |
//  |   X   |   Y   |   Z   |  RHW  |             Diffuse Color             |
//  +-------+-------+-------+-------+---------------------------------------+
    {  POSX1,  POSY1,  POSZ1,   1.0f, D3DMCOLOR_ARGB(0xff, 0xff, 0xff, 0xff)},
    {  POSX2,  POSY2,  POSZ2,   1.0f, D3DMCOLOR_ARGB(0xff, 0xff, 0xff, 0xff)},
    {  POSX3,  POSY3,  POSZ3,   1.0f, D3DMCOLOR_ARGB(0xff, 0xff, 0xff, 0xff)},
    {  POSX2,  POSY2,  POSZ2,   1.0f, D3DMCOLOR_ARGB(0xff, 0xff, 0xff, 0xff)},
    {  POSX4,  POSY4,  POSZ4,   1.0f, D3DMCOLOR_ARGB(0xff, 0xff, 0xff, 0xff)},
    {  POSX3,  POSY3,  POSZ3,   1.0f, D3DMCOLOR_ARGB(0xff, 0xff, 0xff, 0xff)}
};


static D3DMDEPTHBIASTEST_COLOR DepthBiasTestVertices[24] = {
//  |                     |                     |                |       |                                       |
//  |          X          |          Y          |       Z        |  RHW  |             Diffuse Color             |
//  +---------------------+---------------------+----------------+-------+---------------------------------------+
    {  POSX1 / 2.0f,        POSY1 / 2.0f,         POSZ1 + 0.0005f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX2 / 2.0f,        POSY2 / 2.0f,         POSZ2 + 0.0005f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX3 / 2.0f,        POSY3 / 2.0f,         POSZ3 + 0.0005f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX2 / 2.0f,        POSY2 / 2.0f,         POSZ2 + 0.0005f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX4 / 2.0f,        POSY4 / 2.0f,         POSZ4 + 0.0005f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX3 / 2.0f,        POSY3 / 2.0f,         POSZ3 + 0.0005f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX1 / 2.0f, POSY1 / 2.0f,         POSZ1 - 0.0005f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX2 / 2.0f, POSY2 / 2.0f,         POSZ2 - 0.0005f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX3 / 2.0f, POSY3 / 2.0f,         POSZ3 - 0.0005f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX2 / 2.0f, POSY2 / 2.0f,         POSZ2 - 0.0005f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX4 / 2.0f, POSY4 / 2.0f,         POSZ4 - 0.0005f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX3 / 2.0f, POSY3 / 2.0f,         POSZ3 - 0.0005f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX1 / 2.0f,        0.5f + POSY1 / 2.0f,  POSZ1 + 0.0015f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX2 / 2.0f,        0.5f + POSY2 / 2.0f,  POSZ2 + 0.0015f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX3 / 2.0f,        0.5f + POSY3 / 2.0f,  POSZ3 + 0.0015f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX2 / 2.0f,        0.5f + POSY2 / 2.0f,  POSZ2 + 0.0015f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX4 / 2.0f,        0.5f + POSY4 / 2.0f,  POSZ4 + 0.0015f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX3 / 2.0f,        0.5f + POSY3 / 2.0f,  POSZ3 + 0.0015f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX1 / 2.0f, 0.5f + POSY1 / 2.0f,  POSZ1 - 0.0015f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX2 / 2.0f, 0.5f + POSY2 / 2.0f,  POSZ2 - 0.0015f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX3 / 2.0f, 0.5f + POSY3 / 2.0f,  POSZ3 - 0.0015f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX2 / 2.0f, 0.5f + POSY2 / 2.0f,  POSZ2 - 0.0015f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX4 / 2.0f, 0.5f + POSY4 / 2.0f,  POSZ4 - 0.0015f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX3 / 2.0f, 0.5f + POSY3 / 2.0f,  POSZ3 - 0.0015f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)}
};

static D3DMDEPTHBIASTEST_COLOR SSDepthBiasYTestVertices[24] = {
//  |                     |                     |      |      |                                       |
//  |          X          |          Y          |   Z  | RHW  |             Diffuse Color             |
//  +---------------------+---------------------+------+------+---------------------------------------+
    {  POSX1 / 2.0f,        POSY1 / 2.0f,         0.28f,  1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX2 / 2.0f,        POSY2 / 2.0f,         0.28f,  1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX3 / 2.0f,        POSY3 / 2.0f,         0.5f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX2 / 2.0f,        POSY2 / 2.0f,         0.28f,  1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX4 / 2.0f,        POSY4 / 2.0f,         0.5f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX3 / 2.0f,        POSY3 / 2.0f,         0.5f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX1 / 2.0f, POSY1 / 2.0f,         0.5f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX2 / 2.0f, POSY2 / 2.0f,         0.5f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX3 / 2.0f, POSY3 / 2.0f,         0.28f,  1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX2 / 2.0f, POSY2 / 2.0f,         0.5f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX4 / 2.0f, POSY4 / 2.0f,         0.28f,  1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX3 / 2.0f, POSY3 / 2.0f,         0.28f,  1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX1 / 2.0f,        0.5f + POSY1 / 2.0f,  0.72f,  1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX2 / 2.0f,        0.5f + POSY2 / 2.0f,  0.72f,  1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX3 / 2.0f,        0.5f + POSY3 / 2.0f,  0.5f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX2 / 2.0f,        0.5f + POSY2 / 2.0f,  0.72f,  1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX4 / 2.0f,        0.5f + POSY4 / 2.0f,  0.5f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX3 / 2.0f,        0.5f + POSY3 / 2.0f,  0.5f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX1 / 2.0f, 0.5f + POSY1 / 2.0f,  0.5f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX2 / 2.0f, 0.5f + POSY2 / 2.0f,  0.5f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX3 / 2.0f, 0.5f + POSY3 / 2.0f,  0.72f,  1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX2 / 2.0f, 0.5f + POSY2 / 2.0f,  0.5f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX4 / 2.0f, 0.5f + POSY4 / 2.0f,  0.72f,  1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX3 / 2.0f, 0.5f + POSY3 / 2.0f,  0.72f,  1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)}
};

static D3DMDEPTHBIASTEST_COLOR SSDepthBiasXTestVertices[24] = {
//  |                     |                     |      |      |                                       |
//  |          X          |          Y          |   Z  |  RHW |             Diffuse Color             |
//  +---------------------+---------------------+------+------+---------------------------------------+
    {  POSX1 / 2.0f,        POSY1 / 2.0f,         0.28f,  1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX2 / 2.0f,        POSY2 / 2.0f,         0.5f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX3 / 2.0f,        POSY3 / 2.0f,         0.28f,  1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX2 / 2.0f,        POSY2 / 2.0f,         0.5f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX4 / 2.0f,        POSY4 / 2.0f,         0.5f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX3 / 2.0f,        POSY3 / 2.0f,         0.28f,  1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX1 / 2.0f, POSY1 / 2.0f,         0.5f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX2 / 2.0f, POSY2 / 2.0f,         0.28f,  1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX3 / 2.0f, POSY3 / 2.0f,         0.5f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX2 / 2.0f, POSY2 / 2.0f,         0.28f,  1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX4 / 2.0f, POSY4 / 2.0f,         0.28f,  1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX3 / 2.0f, POSY3 / 2.0f,         0.5f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX1 / 2.0f,        0.5f + POSY1 / 2.0f,  0.72f,  1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX2 / 2.0f,        0.5f + POSY2 / 2.0f,  0.5f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX3 / 2.0f,        0.5f + POSY3 / 2.0f,  0.72f,  1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX2 / 2.0f,        0.5f + POSY2 / 2.0f,  0.5f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX4 / 2.0f,        0.5f + POSY4 / 2.0f,  0.5f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  POSX3 / 2.0f,        0.5f + POSY3 / 2.0f,  0.72f,  1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX1 / 2.0f, 0.5f + POSY1 / 2.0f,  0.5f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX2 / 2.0f, 0.5f + POSY2 / 2.0f,  0.72f,  1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX3 / 2.0f, 0.5f + POSY3 / 2.0f,  0.5f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX2 / 2.0f, 0.5f + POSY2 / 2.0f,  0.72f,  1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX4 / 2.0f, 0.5f + POSY4 / 2.0f,  0.72f,  1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)},
    {  0.5f + POSX3 / 2.0f, 0.5f + POSY3 / 2.0f,  0.5f,   1.0f, D3DMCOLOR_ARGB(0xff, 0x00, 0x00, 0xff)}
};

