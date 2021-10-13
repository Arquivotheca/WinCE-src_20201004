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
#include <windows.h>
#include "DXTnCases.h"
#include "ImageManagement.h"
#include <BufferTools.h>
#include <tux.h>
#include "DebugOutput.h"
#include "QAD3DMX.h"

#ifndef _PREFAST_
#pragma warning(disable:4068)
#endif

typedef struct {
    float left, top, right, bottom;
} RECTF, *LPRECTF;

static D3DMDXTN_FVF DXTnSimpleVerts[] = {
//  |       |       |       |       TEX SET #1      |
//  |   X   |   Y   |   Z   |     u     |     v     |
//  +-------+-------+-------+-----------+-----------+
    {  POSX1,  POSY1,  POSZ1,       0.0f,       0.0f},
    {  POSX2,  POSY2,  POSZ2,       1.0f,       0.0f},
    {  POSX3,  POSY3,  POSZ3,       0.0f,       1.0f},
    {  POSX4,  POSY4,  POSZ4,       1.0f,       1.0f},
};

D3DMMATRIX FirstQuarter = { 
    _M(2.0f),  _M(0.0f),  _M(0.0f),  _M(0.0f),
    _M(0.0f),  _M(2.0f),  _M(0.0f),  _M(0.0f),
    _M(0.0f),  _M(0.0f),  _M(1.0f),  _M(0.0f),
    _M(1.0f),  _M(-1.0f),  _M(0.0f),  _M(1.0f)
};

D3DMMATRIX SecondQuarter = { 
    _M(2.0f),  _M(0.0f),  _M(0.0f),  _M(0.0f),
    _M(0.0f),  _M(2.0f),  _M(0.0f),  _M(0.0f),
    _M(0.0f),  _M(0.0f),  _M(1.0f),  _M(0.0f),
    _M(-1.0f),  _M(-1.0f),  _M(0.0f),  _M(1.0f)
};
D3DMMATRIX ThirdQuarter = { 
    _M(2.0f),  _M(0.0f),  _M(0.0f),  _M(0.0f),
    _M(0.0f),  _M(2.0f),  _M(0.0f),  _M(0.0f),
    _M(0.0f),  _M(0.0f),  _M(1.0f),  _M(0.0f),
    _M(1.0f),  _M(1.0f),  _M(0.0f),  _M(1.0f)
};
D3DMMATRIX FourthQuarter = { 
    _M(2.0f),  _M(0.0f),  _M(0.0f),  _M(0.0f),
    _M(0.0f),  _M(2.0f),  _M(0.0f),  _M(0.0f),
    _M(0.0f),  _M(0.0f),  _M(1.0f),  _M(0.0f),
    _M(-1.0f),  _M(1.0f),  _M(0.0f),  _M(1.0f)
};


const DXTnTestParameters DXTnParams[] = {
    D3DMFMT_DXT1, &FirstQuarter,
    D3DMFMT_DXT1, &SecondQuarter,
    D3DMFMT_DXT1, &ThirdQuarter,
    D3DMFMT_DXT1, &FourthQuarter,
    D3DMFMT_DXT2, &FirstQuarter,
    D3DMFMT_DXT2, &SecondQuarter,
    D3DMFMT_DXT2, &ThirdQuarter,
    D3DMFMT_DXT2, &FourthQuarter,
    D3DMFMT_DXT3, &FirstQuarter,
    D3DMFMT_DXT3, &SecondQuarter,
    D3DMFMT_DXT3, &ThirdQuarter,
    D3DMFMT_DXT3, &FourthQuarter,
    D3DMFMT_DXT4, &FirstQuarter,
    D3DMFMT_DXT4, &SecondQuarter,
    D3DMFMT_DXT4, &ThirdQuarter,
    D3DMFMT_DXT4, &FourthQuarter,
    D3DMFMT_DXT5, &FirstQuarter,
    D3DMFMT_DXT5, &SecondQuarter,
    D3DMFMT_DXT5, &ThirdQuarter,
    D3DMFMT_DXT5, &FourthQuarter,
};

const DXTnMipMapTestParameters DXTnMipMapParams[] = {
//0
    D3DMFMT_DXT1,  4,  4, 3, 0,
    D3DMFMT_DXT1,  4,  4, 3, 1,
    D3DMFMT_DXT1,  4,  4, 3, 2,
    D3DMFMT_DXT1,  4,  8, 3, 0,
    D3DMFMT_DXT1,  4,  8, 3, 1,
    D3DMFMT_DXT1,  4,  8, 3, 2,
    D3DMFMT_DXT1,  4, 12, 3, 0,
    D3DMFMT_DXT1,  4, 12, 3, 1,
    D3DMFMT_DXT1,  4, 12, 3, 2,
    D3DMFMT_DXT1,  4, 16, 3, 0,
    D3DMFMT_DXT1,  4, 16, 3, 1,
    D3DMFMT_DXT1,  4, 16, 3, 2,
    D3DMFMT_DXT1,  4, 20, 3, 0,
    D3DMFMT_DXT1,  4, 20, 3, 1,
    D3DMFMT_DXT1,  4, 20, 3, 2,
    D3DMFMT_DXT1,  4, 24, 3, 0,
    D3DMFMT_DXT1,  4, 24, 3, 1,
    D3DMFMT_DXT1,  4, 24, 3, 2,
    D3DMFMT_DXT1,  4, 28, 3, 0,
    D3DMFMT_DXT1,  4, 28, 3, 1,
    D3DMFMT_DXT1,  4, 28, 3, 2,
    D3DMFMT_DXT1,  8,  4, 3, 0,
    D3DMFMT_DXT1,  8,  4, 3, 1,
    D3DMFMT_DXT1,  8,  4, 3, 2,
    D3DMFMT_DXT1, 12,  4, 3, 0,
    D3DMFMT_DXT1, 12,  4, 3, 1,
    D3DMFMT_DXT1, 12,  4, 3, 2,
    D3DMFMT_DXT1, 16,  4, 3, 0,
    D3DMFMT_DXT1, 16,  4, 3, 1,
    D3DMFMT_DXT1, 16,  4, 3, 2,
    D3DMFMT_DXT1, 20,  4, 3, 0,
    D3DMFMT_DXT1, 20,  4, 3, 1,
    D3DMFMT_DXT1, 20,  4, 3, 2,
    D3DMFMT_DXT1, 24,  4, 3, 0,
    D3DMFMT_DXT1, 24,  4, 3, 1,
    D3DMFMT_DXT1, 24,  4, 3, 2,
    D3DMFMT_DXT1, 28,  4, 3, 0,
    D3DMFMT_DXT1, 28,  4, 3, 1,
    D3DMFMT_DXT1, 28,  4, 3, 2,
//39
    D3DMFMT_DXT2,  4,  4, 3, 0,
    D3DMFMT_DXT2,  4,  4, 3, 1,
    D3DMFMT_DXT2,  4,  4, 3, 2,
    D3DMFMT_DXT2,  4,  8, 3, 0,
    D3DMFMT_DXT2,  4,  8, 3, 1,
    D3DMFMT_DXT2,  4,  8, 3, 2,
    D3DMFMT_DXT2,  4, 12, 3, 0,
    D3DMFMT_DXT2,  4, 12, 3, 1,
    D3DMFMT_DXT2,  4, 12, 3, 2,
    D3DMFMT_DXT2,  4, 16, 3, 0,
    D3DMFMT_DXT2,  4, 16, 3, 1,
    D3DMFMT_DXT2,  4, 16, 3, 2,
    D3DMFMT_DXT2,  4, 20, 3, 0,
    D3DMFMT_DXT2,  4, 20, 3, 1,
    D3DMFMT_DXT2,  4, 20, 3, 2,
    D3DMFMT_DXT2,  4, 24, 3, 0,
    D3DMFMT_DXT2,  4, 24, 3, 1,
    D3DMFMT_DXT2,  4, 24, 3, 2,
    D3DMFMT_DXT2,  4, 28, 3, 0,
    D3DMFMT_DXT2,  4, 28, 3, 1,
    D3DMFMT_DXT2,  4, 28, 3, 2,
    D3DMFMT_DXT2,  8,  4, 3, 0,
    D3DMFMT_DXT2,  8,  4, 3, 1,
    D3DMFMT_DXT2,  8,  4, 3, 2,
    D3DMFMT_DXT2, 12,  4, 3, 0,
    D3DMFMT_DXT2, 12,  4, 3, 1,
    D3DMFMT_DXT2, 12,  4, 3, 2,
    D3DMFMT_DXT2, 16,  4, 3, 0,
    D3DMFMT_DXT2, 16,  4, 3, 1,
    D3DMFMT_DXT2, 16,  4, 3, 2,
    D3DMFMT_DXT2, 20,  4, 3, 0,
    D3DMFMT_DXT2, 20,  4, 3, 1,
    D3DMFMT_DXT2, 20,  4, 3, 2,
    D3DMFMT_DXT2, 24,  4, 3, 0,
    D3DMFMT_DXT2, 24,  4, 3, 1,
    D3DMFMT_DXT2, 24,  4, 3, 2,
    D3DMFMT_DXT2, 28,  4, 3, 0,
    D3DMFMT_DXT2, 28,  4, 3, 1,
    D3DMFMT_DXT2, 28,  4, 3, 2,
// 78
    D3DMFMT_DXT3,  4,  4, 3, 0,
    D3DMFMT_DXT3,  4,  4, 3, 1,
    D3DMFMT_DXT3,  4,  4, 3, 2,
    D3DMFMT_DXT3,  4,  8, 3, 0,
    D3DMFMT_DXT3,  4,  8, 3, 1,
    D3DMFMT_DXT3,  4,  8, 3, 2,
    D3DMFMT_DXT3,  4, 12, 3, 0,
    D3DMFMT_DXT3,  4, 12, 3, 1,
    D3DMFMT_DXT3,  4, 12, 3, 2,
    D3DMFMT_DXT3,  4, 16, 3, 0,
    D3DMFMT_DXT3,  4, 16, 3, 1,
    D3DMFMT_DXT3,  4, 16, 3, 2,
    D3DMFMT_DXT3,  4, 20, 3, 0,
    D3DMFMT_DXT3,  4, 20, 3, 1,
    D3DMFMT_DXT3,  4, 20, 3, 2,
    D3DMFMT_DXT3,  4, 24, 3, 0,
    D3DMFMT_DXT3,  4, 24, 3, 1,
    D3DMFMT_DXT3,  4, 24, 3, 2,
    D3DMFMT_DXT3,  4, 28, 3, 0,
    D3DMFMT_DXT3,  4, 28, 3, 1,
    D3DMFMT_DXT3,  4, 28, 3, 2,
    D3DMFMT_DXT3,  8,  4, 3, 0,
    D3DMFMT_DXT3,  8,  4, 3, 1,
    D3DMFMT_DXT3,  8,  4, 3, 2,
    D3DMFMT_DXT3, 12,  4, 3, 0,
    D3DMFMT_DXT3, 12,  4, 3, 1,
    D3DMFMT_DXT3, 12,  4, 3, 2,
    D3DMFMT_DXT3, 16,  4, 3, 0,
    D3DMFMT_DXT3, 16,  4, 3, 1,
    D3DMFMT_DXT3, 16,  4, 3, 2,
    D3DMFMT_DXT3, 20,  4, 3, 0,
    D3DMFMT_DXT3, 20,  4, 3, 1,
    D3DMFMT_DXT3, 20,  4, 3, 2,
    D3DMFMT_DXT3, 24,  4, 3, 0,
    D3DMFMT_DXT3, 24,  4, 3, 1,
    D3DMFMT_DXT3, 24,  4, 3, 2,
    D3DMFMT_DXT3, 28,  4, 3, 0,
    D3DMFMT_DXT3, 28,  4, 3, 1,
    D3DMFMT_DXT3, 28,  4, 3, 2,
// 117
    D3DMFMT_DXT4,  4,  4, 3, 0,
    D3DMFMT_DXT4,  4,  4, 3, 1,
    D3DMFMT_DXT4,  4,  4, 3, 2,
    D3DMFMT_DXT4,  4,  8, 3, 0,
    D3DMFMT_DXT4,  4,  8, 3, 1,
    D3DMFMT_DXT4,  4,  8, 3, 2,
    D3DMFMT_DXT4,  4, 12, 3, 0,
    D3DMFMT_DXT4,  4, 12, 3, 1,
    D3DMFMT_DXT4,  4, 12, 3, 2,
    D3DMFMT_DXT4,  4, 16, 3, 0,
    D3DMFMT_DXT4,  4, 16, 3, 1,
    D3DMFMT_DXT4,  4, 16, 3, 2,
    D3DMFMT_DXT4,  4, 20, 3, 0,
    D3DMFMT_DXT4,  4, 20, 3, 1,
    D3DMFMT_DXT4,  4, 20, 3, 2,
    D3DMFMT_DXT4,  4, 24, 3, 0,
    D3DMFMT_DXT4,  4, 24, 3, 1,
    D3DMFMT_DXT4,  4, 24, 3, 2,
    D3DMFMT_DXT4,  4, 28, 3, 0,
    D3DMFMT_DXT4,  4, 28, 3, 1,
    D3DMFMT_DXT4,  4, 28, 3, 2,
    D3DMFMT_DXT4,  8,  4, 3, 0,
    D3DMFMT_DXT4,  8,  4, 3, 1,
    D3DMFMT_DXT4,  8,  4, 3, 2,
    D3DMFMT_DXT4, 12,  4, 3, 0,
    D3DMFMT_DXT4, 12,  4, 3, 1,
    D3DMFMT_DXT4, 12,  4, 3, 2,
    D3DMFMT_DXT4, 16,  4, 3, 0,
    D3DMFMT_DXT4, 16,  4, 3, 1,
    D3DMFMT_DXT4, 16,  4, 3, 2,
    D3DMFMT_DXT4, 20,  4, 3, 0,
    D3DMFMT_DXT4, 20,  4, 3, 1,
    D3DMFMT_DXT4, 20,  4, 3, 2,
    D3DMFMT_DXT4, 24,  4, 3, 0,
    D3DMFMT_DXT4, 24,  4, 3, 1,
    D3DMFMT_DXT4, 24,  4, 3, 2,
    D3DMFMT_DXT4, 28,  4, 3, 0,
    D3DMFMT_DXT4, 28,  4, 3, 1,
    D3DMFMT_DXT4, 28,  4, 3, 2,
// 156
    D3DMFMT_DXT5,  4,  4, 3, 0,
    D3DMFMT_DXT5,  4,  4, 3, 1,
    D3DMFMT_DXT5,  4,  4, 3, 2,
    D3DMFMT_DXT5,  4,  8, 3, 0,
    D3DMFMT_DXT5,  4,  8, 3, 1,
    D3DMFMT_DXT5,  4,  8, 3, 2,
    D3DMFMT_DXT5,  4, 12, 3, 0,
    D3DMFMT_DXT5,  4, 12, 3, 1,
    D3DMFMT_DXT5,  4, 12, 3, 2,
    D3DMFMT_DXT5,  4, 16, 3, 0,
    D3DMFMT_DXT5,  4, 16, 3, 1,
    D3DMFMT_DXT5,  4, 16, 3, 2,
    D3DMFMT_DXT5,  4, 20, 3, 0,
    D3DMFMT_DXT5,  4, 20, 3, 1,
    D3DMFMT_DXT5,  4, 20, 3, 2,
    D3DMFMT_DXT5,  4, 24, 3, 0,
    D3DMFMT_DXT5,  4, 24, 3, 1,
    D3DMFMT_DXT5,  4, 24, 3, 2,
    D3DMFMT_DXT5,  4, 28, 3, 0,
    D3DMFMT_DXT5,  4, 28, 3, 1,
    D3DMFMT_DXT5,  4, 28, 3, 2,
    D3DMFMT_DXT5,  8,  4, 3, 0,
    D3DMFMT_DXT5,  8,  4, 3, 1,
    D3DMFMT_DXT5,  8,  4, 3, 2,
    D3DMFMT_DXT5, 12,  4, 3, 0,
    D3DMFMT_DXT5, 12,  4, 3, 1,
    D3DMFMT_DXT5, 12,  4, 3, 2,
    D3DMFMT_DXT5, 16,  4, 3, 0,
    D3DMFMT_DXT5, 16,  4, 3, 1,
    D3DMFMT_DXT5, 16,  4, 3, 2,
    D3DMFMT_DXT5, 20,  4, 3, 0,
    D3DMFMT_DXT5, 20,  4, 3, 1,
    D3DMFMT_DXT5, 20,  4, 3, 2,
    D3DMFMT_DXT5, 24,  4, 3, 0,
    D3DMFMT_DXT5, 24,  4, 3, 1,
    D3DMFMT_DXT5, 24,  4, 3, 2,
    D3DMFMT_DXT5, 28,  4, 3, 0,
    D3DMFMT_DXT5, 28,  4, 3, 1,
    D3DMFMT_DXT5, 28,  4, 3, 2,
//// 195
//    D3DMFMT_DXT1,  4,  4, 3, 0, false,
//    D3DMFMT_DXT1,  4,  4, 3, 1, false,
//    D3DMFMT_DXT1,  4,  4, 3, 2, false,
//    D3DMFMT_DXT1,  4,  8, 3, 0, false,
//    D3DMFMT_DXT1,  4,  8, 3, 1, false,
//    D3DMFMT_DXT1,  4,  8, 3, 2, false,
//    D3DMFMT_DXT1,  4, 12, 3, 0, false,
//    D3DMFMT_DXT1,  4, 12, 3, 1, false,
//    D3DMFMT_DXT1,  4, 12, 3, 2, false,
//    D3DMFMT_DXT1,  4, 16, 3, 0, false,
//    D3DMFMT_DXT1,  4, 16, 3, 1, false,
//    D3DMFMT_DXT1,  4, 16, 3, 2, false,
//    D3DMFMT_DXT1,  4, 20, 3, 0, false,
//    D3DMFMT_DXT1,  4, 20, 3, 1, false,
//    D3DMFMT_DXT1,  4, 20, 3, 2, false,
//    D3DMFMT_DXT1,  4, 24, 3, 0, false,
//    D3DMFMT_DXT1,  4, 24, 3, 1, false,
//    D3DMFMT_DXT1,  4, 24, 3, 2, false,
//    D3DMFMT_DXT1,  4, 28, 3, 0, false,
//    D3DMFMT_DXT1,  4, 28, 3, 1, false,
//    D3DMFMT_DXT1,  4, 28, 3, 2, false,
//    D3DMFMT_DXT1,  8,  4, 3, 0, false,
//    D3DMFMT_DXT1,  8,  4, 3, 1, false,
//    D3DMFMT_DXT1,  8,  4, 3, 2, false,
//    D3DMFMT_DXT1, 12,  4, 3, 0, false,
//    D3DMFMT_DXT1, 12,  4, 3, 1, false,
//    D3DMFMT_DXT1, 12,  4, 3, 2, false,
//    D3DMFMT_DXT1, 16,  4, 3, 0, false,
//    D3DMFMT_DXT1, 16,  4, 3, 1, false,
//    D3DMFMT_DXT1, 16,  4, 3, 2, false,
//    D3DMFMT_DXT1, 20,  4, 3, 0, false,
//    D3DMFMT_DXT1, 20,  4, 3, 1, false,
//    D3DMFMT_DXT1, 20,  4, 3, 2, false,
//    D3DMFMT_DXT1, 24,  4, 3, 0, false,
//    D3DMFMT_DXT1, 24,  4, 3, 1, false,
//    D3DMFMT_DXT1, 24,  4, 3, 2, false,
//    D3DMFMT_DXT1, 28,  4, 3, 0, false,
//    D3DMFMT_DXT1, 28,  4, 3, 1, false,
//    D3DMFMT_DXT1, 28,  4, 3, 2, false,
////39
//    D3DMFMT_DXT2,  4,  4, 3, 0, false,
//    D3DMFMT_DXT2,  4,  4, 3, 1, false,
//    D3DMFMT_DXT2,  4,  4, 3, 2, false,
//    D3DMFMT_DXT2,  4,  8, 3, 0, false,
//    D3DMFMT_DXT2,  4,  8, 3, 1, false,
//    D3DMFMT_DXT2,  4,  8, 3, 2, false,
//    D3DMFMT_DXT2,  4, 12, 3, 0, false,
//    D3DMFMT_DXT2,  4, 12, 3, 1, false,
//    D3DMFMT_DXT2,  4, 12, 3, 2, false,
//    D3DMFMT_DXT2,  4, 16, 3, 0, false,
//    D3DMFMT_DXT2,  4, 16, 3, 1, false,
//    D3DMFMT_DXT2,  4, 16, 3, 2, false,
//    D3DMFMT_DXT2,  4, 20, 3, 0, false,
//    D3DMFMT_DXT2,  4, 20, 3, 1, false,
//    D3DMFMT_DXT2,  4, 20, 3, 2, false,
//    D3DMFMT_DXT2,  4, 24, 3, 0, false,
//    D3DMFMT_DXT2,  4, 24, 3, 1, false,
//    D3DMFMT_DXT2,  4, 24, 3, 2, false,
//    D3DMFMT_DXT2,  4, 28, 3, 0, false,
//    D3DMFMT_DXT2,  4, 28, 3, 1, false,
//    D3DMFMT_DXT2,  4, 28, 3, 2, false,
//    D3DMFMT_DXT2,  8,  4, 3, 0, false,
//    D3DMFMT_DXT2,  8,  4, 3, 1, false,
//    D3DMFMT_DXT2,  8,  4, 3, 2, false,
//    D3DMFMT_DXT2, 12,  4, 3, 0, false,
//    D3DMFMT_DXT2, 12,  4, 3, 1, false,
//    D3DMFMT_DXT2, 12,  4, 3, 2, false,
//    D3DMFMT_DXT2, 16,  4, 3, 0, false,
//    D3DMFMT_DXT2, 16,  4, 3, 1, false,
//    D3DMFMT_DXT2, 16,  4, 3, 2, false,
//    D3DMFMT_DXT2, 20,  4, 3, 0, false,
//    D3DMFMT_DXT2, 20,  4, 3, 1, false,
//    D3DMFMT_DXT2, 20,  4, 3, 2, false,
//    D3DMFMT_DXT2, 24,  4, 3, 0, false,
//    D3DMFMT_DXT2, 24,  4, 3, 1, false,
//    D3DMFMT_DXT2, 24,  4, 3, 2, false,
//    D3DMFMT_DXT2, 28,  4, 3, 0, false,
//    D3DMFMT_DXT2, 28,  4, 3, 1, false,
//    D3DMFMT_DXT2, 28,  4, 3, 2, false,
//// 234
//    D3DMFMT_DXT3,  4,  4, 3, 0, false,
//    D3DMFMT_DXT3,  4,  4, 3, 1, false,
//    D3DMFMT_DXT3,  4,  4, 3, 2, false,
//    D3DMFMT_DXT3,  4,  8, 3, 0, false,
//    D3DMFMT_DXT3,  4,  8, 3, 1, false,
//    D3DMFMT_DXT3,  4,  8, 3, 2, false,
//    D3DMFMT_DXT3,  4, 12, 3, 0, false,
//    D3DMFMT_DXT3,  4, 12, 3, 1, false,
//    D3DMFMT_DXT3,  4, 12, 3, 2, false,
//    D3DMFMT_DXT3,  4, 16, 3, 0, false,
//    D3DMFMT_DXT3,  4, 16, 3, 1, false,
//    D3DMFMT_DXT3,  4, 16, 3, 2, false,
//    D3DMFMT_DXT3,  4, 20, 3, 0, false,
//    D3DMFMT_DXT3,  4, 20, 3, 1, false,
//    D3DMFMT_DXT3,  4, 20, 3, 2, false,
//    D3DMFMT_DXT3,  4, 24, 3, 0, false,
//    D3DMFMT_DXT3,  4, 24, 3, 1, false,
//    D3DMFMT_DXT3,  4, 24, 3, 2, false,
//    D3DMFMT_DXT3,  4, 28, 3, 0, false,
//    D3DMFMT_DXT3,  4, 28, 3, 1, false,
//    D3DMFMT_DXT3,  4, 28, 3, 2, false,
//    D3DMFMT_DXT3,  8,  4, 3, 0, false,
//    D3DMFMT_DXT3,  8,  4, 3, 1, false,
//    D3DMFMT_DXT3,  8,  4, 3, 2, false,
//    D3DMFMT_DXT3, 12,  4, 3, 0, false,
//    D3DMFMT_DXT3, 12,  4, 3, 1, false,
//    D3DMFMT_DXT3, 12,  4, 3, 2, false,
//    D3DMFMT_DXT3, 16,  4, 3, 0, false,
//    D3DMFMT_DXT3, 16,  4, 3, 1, false,
//    D3DMFMT_DXT3, 16,  4, 3, 2, false,
//    D3DMFMT_DXT3, 20,  4, 3, 0, false,
//    D3DMFMT_DXT3, 20,  4, 3, 1, false,
//    D3DMFMT_DXT3, 20,  4, 3, 2, false,
//    D3DMFMT_DXT3, 24,  4, 3, 0, false,
//    D3DMFMT_DXT3, 24,  4, 3, 1, false,
//    D3DMFMT_DXT3, 24,  4, 3, 2, false,
//    D3DMFMT_DXT3, 28,  4, 3, 0, false,
//    D3DMFMT_DXT3, 28,  4, 3, 1, false,
//    D3DMFMT_DXT3, 28,  4, 3, 2, false,
//// 273
//    D3DMFMT_DXT4,  4,  4, 3, 0, false,
//    D3DMFMT_DXT4,  4,  4, 3, 1, false,
//    D3DMFMT_DXT4,  4,  4, 3, 2, false,
//    D3DMFMT_DXT4,  4,  8, 3, 0, false,
//    D3DMFMT_DXT4,  4,  8, 3, 1, false,
//    D3DMFMT_DXT4,  4,  8, 3, 2, false,
//    D3DMFMT_DXT4,  4, 12, 3, 0, false,
//    D3DMFMT_DXT4,  4, 12, 3, 1, false,
//    D3DMFMT_DXT4,  4, 12, 3, 2, false,
//    D3DMFMT_DXT4,  4, 16, 3, 0, false,
//    D3DMFMT_DXT4,  4, 16, 3, 1, false,
//    D3DMFMT_DXT4,  4, 16, 3, 2, false,
//    D3DMFMT_DXT4,  4, 20, 3, 0, false,
//    D3DMFMT_DXT4,  4, 20, 3, 1, false,
//    D3DMFMT_DXT4,  4, 20, 3, 2, false,
//    D3DMFMT_DXT4,  4, 24, 3, 0, false,
//    D3DMFMT_DXT4,  4, 24, 3, 1, false,
//    D3DMFMT_DXT4,  4, 24, 3, 2, false,
//    D3DMFMT_DXT4,  4, 28, 3, 0, false,
//    D3DMFMT_DXT4,  4, 28, 3, 1, false,
//    D3DMFMT_DXT4,  4, 28, 3, 2, false,
//    D3DMFMT_DXT4,  8,  4, 3, 0, false,
//    D3DMFMT_DXT4,  8,  4, 3, 1, false,
//    D3DMFMT_DXT4,  8,  4, 3, 2, false,
//    D3DMFMT_DXT4, 12,  4, 3, 0, false,
//    D3DMFMT_DXT4, 12,  4, 3, 1, false,
//    D3DMFMT_DXT4, 12,  4, 3, 2, false,
//    D3DMFMT_DXT4, 16,  4, 3, 0, false,
//    D3DMFMT_DXT4, 16,  4, 3, 1, false,
//    D3DMFMT_DXT4, 16,  4, 3, 2, false,
//    D3DMFMT_DXT4, 20,  4, 3, 0, false,
//    D3DMFMT_DXT4, 20,  4, 3, 1, false,
//    D3DMFMT_DXT4, 20,  4, 3, 2, false,
//    D3DMFMT_DXT4, 24,  4, 3, 0, false,
//    D3DMFMT_DXT4, 24,  4, 3, 1, false,
//    D3DMFMT_DXT4, 24,  4, 3, 2, false,
//    D3DMFMT_DXT4, 28,  4, 3, 0, false,
//    D3DMFMT_DXT4, 28,  4, 3, 1, false,
//    D3DMFMT_DXT4, 28,  4, 3, 2, false,
//// 312
//    D3DMFMT_DXT5,  4,  4, 3, 0, false,
//    D3DMFMT_DXT5,  4,  4, 3, 1, false,
//    D3DMFMT_DXT5,  4,  4, 3, 2, false,
//    D3DMFMT_DXT5,  4,  8, 3, 0, false,
//    D3DMFMT_DXT5,  4,  8, 3, 1, false,
//    D3DMFMT_DXT5,  4,  8, 3, 2, false,
//    D3DMFMT_DXT5,  4, 12, 3, 0, false,
//    D3DMFMT_DXT5,  4, 12, 3, 1, false,
//    D3DMFMT_DXT5,  4, 12, 3, 2, false,
//    D3DMFMT_DXT5,  4, 16, 3, 0, false,
//    D3DMFMT_DXT5,  4, 16, 3, 1, false,
//    D3DMFMT_DXT5,  4, 16, 3, 2, false,
//    D3DMFMT_DXT5,  4, 20, 3, 0, false,
//    D3DMFMT_DXT5,  4, 20, 3, 1, false,
//    D3DMFMT_DXT5,  4, 20, 3, 2, false,
//    D3DMFMT_DXT5,  4, 24, 3, 0, false,
//    D3DMFMT_DXT5,  4, 24, 3, 1, false,
//    D3DMFMT_DXT5,  4, 24, 3, 2, false,
//    D3DMFMT_DXT5,  4, 28, 3, 0, false,
//    D3DMFMT_DXT5,  4, 28, 3, 1, false,
//    D3DMFMT_DXT5,  4, 28, 3, 2, false,
//    D3DMFMT_DXT5,  8,  4, 3, 0, false,
//    D3DMFMT_DXT5,  8,  4, 3, 1, false,
//    D3DMFMT_DXT5,  8,  4, 3, 2, false,
//    D3DMFMT_DXT5, 12,  4, 3, 0, false,
//    D3DMFMT_DXT5, 12,  4, 3, 1, false,
//    D3DMFMT_DXT5, 12,  4, 3, 2, false,
//    D3DMFMT_DXT5, 16,  4, 3, 0, false,
//    D3DMFMT_DXT5, 16,  4, 3, 1, false,
//    D3DMFMT_DXT5, 16,  4, 3, 2, false,
//    D3DMFMT_DXT5, 20,  4, 3, 0, false,
//    D3DMFMT_DXT5, 20,  4, 3, 1, false,
//    D3DMFMT_DXT5, 20,  4, 3, 2, false,
//    D3DMFMT_DXT5, 24,  4, 3, 0, false,
//    D3DMFMT_DXT5, 24,  4, 3, 1, false,
//    D3DMFMT_DXT5, 24,  4, 3, 2, false,
//    D3DMFMT_DXT5, 28,  4, 3, 0, false,
//    D3DMFMT_DXT5, 28,  4, 3, 1, false,
//    D3DMFMT_DXT5, 28,  4, 3, 2, false,
};

HRESULT DXTnFillTextureSurface(D3DMLOCKED_RECT * pLocked, const UINT Width, const UINT Height, const D3DMFORMAT Format)
{
    // The stride in words of each 4x4 block of pixels (varies between the DXTn formats)
    UINT BlockStrideW = 0;
    // The offset in words into the color portion of the block (non-zero for DXT2-5)
    UINT ColorOffsetW = 0;

    // How many blocks wide?
    const int WidthInBlocks = (Width + 3) / 4;
    const int HeightInBlocks = (Height + 3) / 4;

    // Pointer to the surface (using WORDs)
    WORD * pSurfaceBuffer = NULL;
    // The stride in words of the surface (how many words to the beginning of the next line of blocks)
    UINT SurfaceStrideW = 0;

    const UINT WidthAssumed = 32;
    const UINT HeightAssumed = 32;

    int InterpIndexLT = 0;
    int InterpIndexEQ = 0;
    int InterpIndexGT = 0;
    int AlphaIndex = 0;
    int AlphaInterpIndex = 0;

    WORD ColorWords[] = {
        0x0000,
        0x0001,
        0x7FFF,
        0x7FFE,
        0x8000,
        0X8001,
        0xFFFE,
        0xFFFF,
    };

    /*
        The Color layout is:
            Word 0: color_0
            Word 1: color_1
            Word 2: bitmap word_0 (0,0) through (1,3)
            Word 3: bitmap word_1 (2,0) through (3,3)
        The bitmap word layout is:
            Bits 1:0   = Texel[0][0]
            Bits 3:2   = Texel[0][1]
            Bits 5:4   = Texel[0][2]
            Bits 7:6   = Texel[0][3]
            Bits 9:8   = Texel[1][0]
            Bits 11:10 = Texel[1][1]
            Bits 13:12 = Texel[1][2]
            Bits 15:14 = Texel[1][3]
        ColorInterps[0] = All 0s (All pixels drawn as color_0
        ColorInterps[1] = All 1s (All pixels as color_1 if not DXT1 or color_0 > color_1, transparent if DXT1 and color_0 <= color_1)
        ColorInterps[2] = Mix of color_0, color_1, and color_2
        ColorInterps[3] = Mix of color_0, color_1, color_2, and either transparent or color_3
    */
    const int InterpCount = 4;
    WORD ColorInterps[InterpCount][2] = {
        /*
            All 00
            00 00 00 00
            00 00 00 00
            00 00 00 00
            00 00 00 00
        */
        0x0000, 0x0000,

        /*
            All 11 (fully transparent with DXT1
            11 11 11 11
            11 11 11 11
            11 11 11 11
            11 11 11 11
        */
        0xFFFF, 0xFFFF,

        /*
            Mix without 11
            00 00 00 01
            00 10 01 00
            01 01 01 10
            10 00 10 10
        */
        0x1840, 0xA295,

        /*
            Mix including 11
            11 10 01 00
            10 11 00 01
            00 01 10 11
            01 00 11 10
        */
        0x4E1B, 0xB1E4
        
    };

    const int Alpha23Count = 4;
    WORD AlphaValues23[Alpha23Count][4] = {
        /*
            Fully Opaque
            F F F F 
            F F F F
            F F F F
            F F F F
        */
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,

        /*
            Fully Transparent
            0 0 0 0
            0 0 0 0
            0 0 0 0
            0 0 0 0
        */
        0x0000, 0x0000, 0x0000, 0x0000,

        /*
            Transparency mix 1: Fully Transparent with Fully Opaque
            0 F 0 F
            F 0 F 0
            0 0 F F
            F 0 0 F
        */
        0xF0F0, 0x0F0F, 0xFF00, 0xF00F,

        /*
            All Transparencies
            0 1 2 3
            4 5 6 7
            8 9 A B
            C D E F
        */
        0x3210, 0x7654, 0xBA98, 0xFEDC,

    };

    BYTE Alpha45Bytes[] = {
        0x00,
        0x01,
        0x7E,
        0x7F,
        0x80,
        0x81,
        0xFE,
        0xFF
    };
    
    const int Alpha45InterpCount = 5;
    WORD AlphaInterpValues45[Alpha45InterpCount][3] = {
        /*
            Fully alpha_0 and alpha_1
            000 001 001 000
            001 000 000 001
            
            000 000 001 001
            000 001 001 001
        */
        0x1048, 0x4020, 0x2482,

        /*
            Ramp 
            001 010 011 100
            101 110 111 000
            
            110 000 011 100
            101 110 001 111
        */
        0x58D1, 0xC61F, 0xE758,

        /*
            Mix 1
            000 111 001 110
            010 101 011 100
            
            100 011 101 010
            110 001 111 000
        */
        0xAC78, 0x5C8E, 0x1CE5,

        /*
            Mix 2
            000 000 001 001
            000 000 001 001
            
            010 010 011 011
            010 010 011 011
        */
        0x0240, 0xD224, 0x6D26,

        /*
            Mix 3
            100 100 101 101
            100 100 101 101

            110 110 111 111
            110 110 111 111
        */
        0x4B64, 0xF6B6, 0xFF6F,
    };

    pSurfaceBuffer = (WORD*)pLocked->pBits;
    SurfaceStrideW = pLocked->Pitch / 2;
    

    if (D3DMFMT_DXT1 == Format)
    {
        BlockStrideW = 1 + 1 + 2; // color_0, color_1, and interpolated colors
        ColorOffsetW = 0;
    }
    else 
    {
        BlockStrideW = 4 + 4; // 64 bit alpha block followed by 64 bit color block
        ColorOffsetW = 4;
    }

    if (Width != WidthAssumed ||
        Height != HeightAssumed)
    {
        DebugOut(DO_ERROR, L"Test Issue: Unexpected Width or Height. Has test code been modified incorrectly?");
        return E_UNEXPECTED;
    }

    if (Width < 4 * (SurfaceStrideW / BlockStrideW))
    {
        DebugOut(DO_ERROR, 
            L"Invalid texture surface pitch returned by driver! Found pitch %d; Expected pitch equal to or greater than %d.",
            pLocked->Pitch,
            (Width / 4) * 2 * BlockStrideW);
        return E_UNEXPECTED;
    }

    WCHAR szOut01[256];
    WCHAR * pOut01End = szOut01;
    WCHAR szOut23[256];
    WCHAR * pOut23End = szOut23;
    WCHAR szOutAlpha01[256];
    WCHAR * pOutAlpha01 = szOutAlpha01;
    WCHAR szOutAlpha23[256];
    WCHAR * pOutAlpha23 = szOutAlpha23;

    // Fill in the blocks
    for (int i = 0; i < HeightInBlocks; i++)
    {
        WORD *pCurrentLine = pSurfaceBuffer + (i * SurfaceStrideW);
        szOut01[0] = 0;
        pOut01End = szOut01;
        szOut23[0] = 0;
        pOut23End = szOut23;
        szOutAlpha01[0] = 0;
        pOutAlpha01 = szOutAlpha01;
        szOutAlpha23[0] = 0;
        pOutAlpha23 = szOutAlpha23;
        
        for (int j = 0; j < WidthInBlocks; j++)
        {
            WORD Color0 = ColorWords[i];
            WORD Color1 = ColorWords[j];
            int LineColorIndex = j * BlockStrideW + ColorOffsetW;
            int LineAlphaIndex = j * BlockStrideW;
            int InterpIndex = 0;

            //
            // Fill in the Alpha block
            //

            // choose alpha colors
            if (D3DMFMT_DXT2 == Format || D3DMFMT_DXT3 == Format)
            {
                for (int a = 0; a < 4; a++)
                {
                    pCurrentLine[LineAlphaIndex + a] = AlphaValues23[AlphaIndex][a];
                    if (a < 2)
                    {
                        pOutAlpha01 += swprintf(pOutAlpha01, L"%04hx ", AlphaValues23[AlphaIndex][a]);
                    }
                    else
                    {
                        pOutAlpha23 += swprintf(pOutAlpha23, L"%04hx ", AlphaValues23[AlphaIndex][a]);
                    }
                }
                pOutAlpha01 += swprintf(pOutAlpha01, L": ");
                pOutAlpha23 += swprintf(pOutAlpha23, L": ");
                AlphaIndex++;
                if (AlphaIndex >= Alpha23Count)
                {
                    AlphaIndex = 0;
                }
            }

            if (D3DMFMT_DXT4 == Format || D3DMFMT_DXT5 == Format)
            {
                pCurrentLine[LineAlphaIndex] = Alpha45Bytes[i] | (Alpha45Bytes[j] << 8);
                for (int a = 1; a < 4; a++)
                {
                    pCurrentLine[LineAlphaIndex + a] = AlphaInterpValues45[AlphaIndex][a-1];
                }
                // For verification purposes, record the alpha values
                for (a = 0; a < 4; a++)
                {
                    if (a < 2)
                    {
                        pOutAlpha01 += swprintf(pOutAlpha01, L"%04hx ",  pCurrentLine[LineAlphaIndex + a]);
                    }
                    else
                    {
                        pOutAlpha23 += swprintf(pOutAlpha23, L"%04hx ",  pCurrentLine[LineAlphaIndex + a]);
                    }
                }
                
                pOutAlpha01 += swprintf(pOutAlpha01, L": ");
                pOutAlpha23 += swprintf(pOutAlpha23, L": ");
                AlphaIndex++;
                if (AlphaIndex >= Alpha45InterpCount)
                {
                    AlphaIndex = 0;
                }
            }

            // perform premultiplication if necessary (only on color0/color1)
            if (D3DMFMT_DXT2 == Format || D3DMFMT_DXT4 == Format)
            {
                // Behavior for premultiplied values that exceed the alpha value is fully defined.
                // Drivers should clamp the output. Don't do anything here so we can verify the
                // premultiplied behavior properly matches the reference driver.
            }
            

            //
            // Determine the Interp block to use (aim for hitting each interp with
            // each type of Color comparison) (with non-dxt1 formats, use the same
            // color blocks anyway)
            //
            if (Color0 > Color1)
            {
                // Fill with appropriate color block
                InterpIndex = InterpIndexGT;
//                DebugOut(L"InterpGT: Index %d", InterpIndexGT);
                ++InterpIndexGT;
                if (InterpCount == InterpIndexGT)
                {
                    InterpIndexGT = 0;
                }
            }
            else if (Color0 == Color1)
            {
                // Fill with appropriate color block
                InterpIndex = InterpIndexEQ;
//                DebugOut(L"InterpEQ: Index %d", InterpIndexEQ);
                ++InterpIndexEQ;
                if (InterpCount == InterpIndexEQ)
                {
                    InterpIndexEQ = 0;
                }
            }
            else if (Color0 < Color1)
            {
                // Fill with appropriate color block
                InterpIndex = InterpIndexLT;
//                DebugOut(L"InterpLT: Index %d", InterpIndexLT);
                ++InterpIndexLT;
                if (InterpCount == InterpIndexLT)
                {
                    InterpIndexLT = 0;
                }
            }

            //
            // Fill in the Color block
            //
            pCurrentLine[LineColorIndex] = Color0;
            pCurrentLine[LineColorIndex + 1] = Color1;
            pCurrentLine[LineColorIndex + 2] = ColorInterps[InterpIndex][0];
            pCurrentLine[LineColorIndex + 3] = ColorInterps[InterpIndex][1];

            pOut01End += swprintf(pOut01End, L"%04hx %04hx : ", Color0, Color1);
            pOut23End += swprintf(pOut23End, L"%04hx %04hx : ", ColorInterps[InterpIndex][0], ColorInterps[InterpIndex][1]);
        }

        if (D3DMFMT_DXT1 != Format)
        {
            DebugOut(szOutAlpha01);
            DebugOut(szOutAlpha23);
        }
        DebugOut(szOut01);
        DebugOut(szOut23);
    }
    
    
    return S_OK;
}

HRESULT DXTnIsTestSupported(IDirect3DMobileDevice * pDevice, const D3DMCAPS * pCaps, DWORD dwTableIndex, bool bMipMap)
{
    HRESULT hr;
    IDirect3DMobile * pD3DM = NULL;
    D3DMDISPLAYMODE Mode;
    bool NeedsLockable = false;
    int MipMapLevels;
    
    D3DMFORMAT Format;

    if (!bMipMap)
    {
        Format = DXTnParams[dwTableIndex].Format;
        MipMapLevels = 0;
    }
    else
    {
        Format = DXTnMipMapParams[dwTableIndex].Format;
        MipMapLevels = DXTnMipMapParams[dwTableIndex].Levels;
        UINT Width = DXTnMipMapParams[dwTableIndex].TopLevelWidth;
        UINT Height = DXTnMipMapParams[dwTableIndex].TopLevelHeight;

        if (!(pCaps->TextureCaps & D3DMPTEXTURECAPS_MIPMAP))
        {
            DebugOut(DO_ERROR, L"Driver does not support MipMap textures");
            hr = E_FAIL;
            goto cleanup;
        }

        if ((pCaps->TextureCaps & D3DMPTEXTURECAPS_SQUAREONLY) && 
            (Width != Height))
        {
            DebugOut(DO_ERROR, L"Driver does not support non-square textures");
            hr = E_FAIL;
            goto cleanup;
        }
        if (DXTnMipMapParams[dwTableIndex].LevelToTest > 0)
        {
            if (!(pCaps->TextureFilterCaps & D3DMPTFILTERCAPS_MIPFPOINT))
            {
                DebugOut(DO_ERROR, L"Driver does not support MipMap point filtering");
                hr = E_FAIL;
                goto cleanup;
            }
            NeedsLockable = true;
        }
    }

    hr = pDevice->GetDirect3D(&pD3DM);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"Could not retrieve Direct3DMoble object from Device. (hr = 0x%08x)", hr);
        goto cleanup;
    }

    memset(&Mode, 0x00, sizeof(Mode));
    hr = pDevice->GetDisplayMode(&Mode);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"Could not retrieve DisplayMode. (hr = 0x%08x)", hr);
        goto cleanup;
    }
    
    // Don't have to worry about TextureCaps (creating a 32x32 texture)

    // Can we create a DXT1 Texture?
    DWORD Usage = 0;
    if (NeedsLockable)
    {
        Usage |= D3DMUSAGE_LOCKABLE;
    }
    hr = pD3DM->CheckDeviceFormat(
        pCaps->AdapterOrdinal, 
        D3DMDEVTYPE_DEFAULT, 
        Mode.Format, 
        Usage, 
        D3DMRTYPE_TEXTURE, 
        D3DMFMT_DXT1);
    if (D3DMERR_NOTAVAILABLE == hr)
    {
        DebugOut(DO_ERROR, L"DXT1 Textures not supported.");
        hr = S_FALSE;
        goto cleanup;
    }
    else if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"Could not Check Device Format. (hr = 0x%08x)", hr);
        goto cleanup;
    }
    hr = S_OK;
    
cleanup:
    SAFERELEASE(pD3DM);
    return hr;
}

HRESULT DXTnCreateAndPrepareTexture(
    IDirect3DMobileDevice * pDevice, 
    DWORD dwTableIndex, 
    IDirect3DMobileTexture ** ppTexture,
    IDirect3DMobileSurface ** ppImageSurface)
{
    HRESULT hr;
    IDirect3DMobileTexture * pTexture = NULL;
    IDirect3DMobileSurface * pSurface = NULL;
    IDirect3DMobileSurface * pSurfaceTemp = NULL;
    D3DMLOCKED_RECT LockedRect = {0};
    const UINT Width = D3DQA_TEXTURE_WIDTH;
    const UINT Height = D3DQA_TEXTURE_HEIGHT;

    D3DMFORMAT Format;

    D3DMPOOL Pools[] = {D3DMPOOL_VIDEOMEM, D3DMPOOL_SYSTEMMEM, D3DMPOOL_MANAGED};


    if ((!ppTexture && !ppImageSurface) ||
        (ppTexture && ppImageSurface))
    {
        DebugOut(DO_ERROR, L"Invalid pointers, must have either texture or surface, not both");
        return E_UNEXPECTED;
    }
    
    if (ppTexture)
    {
        Format = DXTnParams[dwTableIndex].Format;
        //
        // Create the texture.
        //
        hr = E_FAIL;
        for (int i = 0; i < countof(Pools) && FAILED(hr); i++)
        {
            hr = pDevice->CreateTexture(
                Width,
                Height,
                1,      // Levels
                0,      // Usage
                Format,
                Pools[i],
                &pTexture);   
        }
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"Could not create texture in any Pool. (hr = 0x%08x)", hr);
            goto cleanup;
        }

        //
        // Now texture is created. Get surface of texture.
        //
        hr = pTexture->GetSurfaceLevel(0, &pSurface);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"Could not get texture's level 0 surface. (hr = 0x%08x)", hr);
            goto cleanup;
        }
    }
    else
    {
        Format = DXTnMipMapParams[dwTableIndex].Format;
    }

    //
    // Lock texture surface (or image surface to copy to texture)
    // If user passed in a valid ppImageSurface, short circuit lock
    //
    if (ppImageSurface || FAILED(hr = pSurface->LockRect(&LockedRect, NULL, 0)))
    {
        //
        // The surface cannot be locked, so create a temporary image surface instead.
        // We will fill that surface with a gradient, and then copy over to the
        // test source surface.
        //
        if (FAILED(hr = pDevice->CreateImageSurface(Width, Height, Format, &pSurfaceTemp)))
        {
            DebugOut(DO_ERROR, L"Could not create temporary image surface. hr = 0x%08x", hr);
            goto cleanup;
        }

        if (FAILED(hr = pSurfaceTemp->LockRect(&LockedRect, NULL, 0)))
        {
            DebugOut(DO_ERROR, L"Could not lock image surface. hr = 0x%08x", hr);
            goto cleanup;
        }
    }

    //
    // Now we're ready to fill the surface with DXTn data.
    //
    hr = DXTnFillTextureSurface(&LockedRect, Width, Height, Format);

    if (ppTexture)
    {
        //
        // If we needed to create a temporary surface to construct the image,
        // copy the image back to the test surface.
        //
        if (pSurfaceTemp)
        {
            RECT rcSource = {0, 0, Width, Height}, rcDest = {0, 0, Width, Height};
            pSurfaceTemp->UnlockRect();
            pDevice->CopyRects(pSurfaceTemp, NULL, 0, pSurface, NULL);
        }
        else
        {
            pSurface->UnlockRect();
        }

        hr = pDevice->SetTexture(
            0,                      // Stage
            pTexture);
    }
    else if (ppImageSurface)
    {
        if (pSurfaceTemp)
        {
            pSurfaceTemp->UnlockRect();
            *ppImageSurface = pSurfaceTemp;
            (*ppImageSurface)->AddRef();
        }
        else
        {
            hr = E_FAIL;
        }
    }

cleanup:
    SAFERELEASE(pSurface);
    SAFERELEASE(pSurfaceTemp);
    if (FAILED(hr))
    {
        SAFERELEASE(pTexture);
    }
    else if (ppTexture)
    {
        *ppTexture = pTexture;
    }
    return hr;
}

HRESULT DXTnCreateAndPrepareVertexBuffer(
    IDirect3DMobileDevice * pDevice,
    HWND hWnd,
    DWORD dwTableIndex,
    IDirect3DMobileVertexBuffer ** ppVertexBuffer,
    UINT * pVertexBufferStride,
    UINT * pVertexPrimCount)
{
    HRESULT hr = S_OK;
    UINT uiShift;
    FLOAT *pPosX, *pPosY;
    UINT uiSX;
    UINT uiSY;
    //
    // Window client extents
    //
    RECT ClientExtents;
    //
    // Retrieve device capabilities
    //
    if (0 == GetClientRect( hWnd, &ClientExtents))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        DebugOut(DO_ERROR, L"GetClientRect failed. (hr = 0x%08x)", hr);
        goto cleanup;
    }
    
    //
    // Scale factors
    //
    uiSX = ClientExtents.right;
    uiSY = ClientExtents.bottom;

    //
    // Scale to window size
    //
    if (D3DMFVF_XYZRHW_FLOAT & D3DMDXTN_FVFDEF)
    {
        for (uiShift = 0; uiShift < countof(DXTnSimpleVerts); uiShift++)
        {
            pPosX = (PFLOAT)((PBYTE)(DXTnSimpleVerts) + uiShift * sizeof(D3DMDXTN_FVF));
            pPosY = &(pPosX[1]);

            (*pPosX) *= (float)(uiSX - 1);
            (*pPosY) *= (float)(uiSY - 1);

        }
    }

    hr = CreateFilledVertexBuffer(
        pDevice,                              // LPDIRECT3DMOBILEDEVICE pDevice,
        ppVertexBuffer,                       // LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
        (BYTE*)DXTnSimpleVerts,                      // BYTE *pVertices,
        sizeof(D3DMDXTN_FVF),                 // UINT uiVertexSize,
        countof(DXTnSimpleVerts),             // UINT uiNumVertices,
        D3DMDXTN_FVFDEF);                     // DWORD dwFVF
    if (FAILED(hr))    
    {
        DebugOut(DO_ERROR, L"CreateActiveBuffer failed. (hr = 0x%08x)", hr);
        hr = S_FALSE;
    }

#pragma prefast(suppress: 326, "This comparison is here for if the code is changed.")
    if (D3DMQA_PRIMTYPE == D3DMPT_TRIANGLESTRIP)
    {
        *pVertexPrimCount = countof(DXTnSimpleVerts) - 2;
    }
    else
    {
        *pVertexPrimCount = countof(DXTnSimpleVerts) / 3;
    }
    
    //
    // "Unscale" from window size
    //
    if (D3DMFVF_XYZRHW_FLOAT & D3DMDXTN_FVFDEF)
    {
        for (uiShift = 0; uiShift < countof(DXTnSimpleVerts); uiShift++)
        {
            pPosX = (PFLOAT)((PBYTE)(DXTnSimpleVerts) + uiShift * sizeof(D3DMDXTN_FVF));
            pPosY = &(pPosX[1]);

            (*pPosX) /= (float)(uiSX - 1);
            (*pPosY) /= (float)(uiSY - 1);

        }
    }
cleanup:
    return hr;
}

HRESULT DXTnSetTransforms(IDirect3DMobileDevice * pDevice, DWORD dwTableIndex, bool bSetWorld)
{
    HRESULT hr;

	D3DMMATRIX View  = { 
	    _M(1.0f),  _M(0.0f),  _M(0.0f),  _M(0.0f),
	    _M(0.0f),  _M(1.0f),  _M(0.0f),  _M(0.0f),
	    _M(0.0f),  _M(0.0f),  _M(1.0f),  _M(0.0f),
	    _M(0.0f),  _M(0.0f),  _M(0.0f),  _M(1.0f)
	};
	
	D3DMMATRIX Proj  = { 
	    _M(1.0f),  _M(0.0f),  _M(0.0f),  _M(0.0f),
	    _M(0.0f),  _M(1.0f),  _M(0.0f),  _M(0.0f),
	    _M(0.0f),  _M(0.0f),  _M(1.0f),  _M(0.0f),
	    _M(0.0f),  _M(0.0f),  _M(0.0f),  _M(1.0f)  
	};

    if (bSetWorld)
    {
        hr = pDevice->SetTransform(
            D3DMTS_WORLD,              
            DXTnParams[dwTableIndex].pTransform,
            D3DMFMT_D3DMVALUE_FLOAT);
    	if (FAILED(hr))
    	{
    		DebugOut(DO_ERROR, L"SetTransform WORLD failed. (hr = 0x%08x)", hr);
    		return hr;
    	}
    }

    hr = pDevice->SetTransform(
        D3DMTS_VIEW,                          
        &View, 
        D3DMFMT_D3DMVALUE_FLOAT);
	if (FAILED(hr))
	{
		DebugOut(DO_ERROR, L"SetTransform VIEW failed. (hr = 0x%08x)", hr);
		return hr;
	}

    hr = pDevice->SetTransform(
        D3DMTS_PROJECTION,                      
        &Proj, 
        D3DMFMT_D3DMVALUE_FLOAT);
	if (FAILED(hr))
	{
		DebugOut(DO_ERROR, L"SetTransform PROJECTION failed. (hr = 0x%08x)", hr);
		return hr;
	}

	return S_OK;
}


HRESULT DXTnSetDefaultTextureStates(IDirect3DMobileDevice * pDevice)
{
    
    //
    // Give args a default state
    //
    pDevice->SetTextureStageState( 0, D3DMTSS_COLORARG1, D3DMTA_TEXTURE );
    pDevice->SetTextureStageState( 0, D3DMTSS_ALPHAARG1, D3DMTA_TEXTURE );
    pDevice->SetTextureStageState( 1, D3DMTSS_COLORARG1, D3DMTA_TEXTURE );
    pDevice->SetTextureStageState( 1, D3DMTSS_ALPHAARG1, D3DMTA_TEXTURE );

    pDevice->SetTextureStageState( 2, D3DMTSS_COLORARG1, D3DMTA_TEXTURE );
    pDevice->SetTextureStageState( 2, D3DMTSS_ALPHAARG1, D3DMTA_TEXTURE );
    pDevice->SetTextureStageState( 3, D3DMTSS_COLORARG1, D3DMTA_TEXTURE );
    pDevice->SetTextureStageState( 3, D3DMTSS_ALPHAARG1, D3DMTA_TEXTURE );

    pDevice->SetTextureStageState( 0, D3DMTSS_COLORARG2, D3DMTA_CURRENT );
    pDevice->SetTextureStageState( 0, D3DMTSS_ALPHAARG2, D3DMTA_CURRENT );
    pDevice->SetTextureStageState( 1, D3DMTSS_COLORARG2, D3DMTA_CURRENT );
    pDevice->SetTextureStageState( 1, D3DMTSS_ALPHAARG2, D3DMTA_CURRENT );

    pDevice->SetTextureStageState( 2, D3DMTSS_COLORARG2, D3DMTA_CURRENT );
    pDevice->SetTextureStageState( 2, D3DMTSS_ALPHAARG2, D3DMTA_CURRENT );
    pDevice->SetTextureStageState( 3, D3DMTSS_COLORARG2, D3DMTA_CURRENT );
    pDevice->SetTextureStageState( 3, D3DMTSS_ALPHAARG2, D3DMTA_CURRENT );

    //
    // Disable output of texture stages
    //
    pDevice->SetTextureStageState( 0, D3DMTSS_COLOROP, D3DMTOP_SELECTARG1 );
    pDevice->SetTextureStageState( 0, D3DMTSS_ALPHAOP, D3DMTOP_SELECTARG1 );
    pDevice->SetTextureStageState( 1, D3DMTSS_COLOROP, D3DMTOP_DISABLE );
    pDevice->SetTextureStageState( 1, D3DMTSS_ALPHAOP, D3DMTOP_DISABLE );
    pDevice->SetTextureStageState( 2, D3DMTSS_COLOROP, D3DMTOP_DISABLE );
    pDevice->SetTextureStageState( 2, D3DMTSS_ALPHAOP, D3DMTOP_DISABLE );
    pDevice->SetTextureStageState( 3, D3DMTSS_COLOROP, D3DMTOP_DISABLE );
    pDevice->SetTextureStageState( 3, D3DMTSS_ALPHAOP, D3DMTOP_DISABLE );

    return S_OK;
}

HRESULT DXTnSetupRenderState(IDirect3DMobileDevice * pDevice, DWORD dwTableIndex)
{
    HRESULT hr;

    ////////////////////////////////////////
    //
    // Set up general states
    //

    //
    // Enable Alpha Blending
    //
    hr = pDevice->SetRenderState(D3DMRS_ALPHABLENDENABLE, TRUE);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"SetRenderState(D3DMRS_ALPHABLENDENABLE, TRUE) failed. (hr = 0x%08x)", hr);
        return hr;
    }

    hr = pDevice->SetRenderState(D3DMRS_SRCBLEND, D3DMBLEND_SRCALPHA);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"SetRenderState(D3DMRS_SRCBLEND, ) failed. (hr = 0x%08x)", hr);
        return hr;
    }

    hr = pDevice->SetRenderState(D3DMRS_DESTBLEND, D3DMBLEND_INVSRCALPHA);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"SetRenderState(D3DMRS_DESTBLEND, ) failed. (hr = 0x%08x)", hr);
        return hr;
    }

    //
    // Set up the culling mode
    //
    if (FAILED(hr = pDevice->SetRenderState(D3DMRS_CULLMODE, D3DMCULL_NONE)))
    {
        DebugOut(DO_ERROR, L"SetRenderState(D3DMRS_CULLMODE, D3DMCULL_CCW) failed. (hr = 0x%08x)", hr);
        return hr;
    }

    //
    // Set up clipping
    //
    if (FAILED(hr = pDevice->SetRenderState(D3DMRS_CLIPPING, TRUE)))
    {
        DebugOut(DO_ERROR, L"SetRenderState(D3DMRS_CLIPPING, TRUE) failed. (hr = 0x%08x)", hr);
        return hr;
    }

	//
	// Set up the shade mode
	//
	if (FAILED(hr = pDevice->SetRenderState(D3DMRS_SHADEMODE, D3DMSHADE_FLAT)))
	{
		DebugOut(DO_ERROR, L"SetRenderState(D3DMRS_SHADEMODE) failed. (hr = 0x%08x)", hr);
		return hr;
    }
    
    return hr;
}

void * GetDXTnAlphaPointer(
    const D3DMLOCKED_RECT * pLockedRect,
    D3DMFORMAT Format,
    UINT PixelX,
    UINT PixelY)
{
    // Get block offset from pixel offset.
    UINT BlockX = PixelX / 4;
    UINT BlockY = PixelY / 4;
    
    // Determine important widths, etc.
    UINT BlockStride;
    UINT ColorOffset;
    if (D3DMFMT_DXT1 == Format)
    {
        BlockStride = 8;
        ColorOffset = 0;
    }
    else
    {
        BlockStride = 16;
        ColorOffset = 8;
    }
    // Determine address to block offset.
    UINT CurrentBlockOffset = BlockY * pLockedRect->Pitch + BlockX * BlockStride;
    return (void*)((BYTE*)pLockedRect->pBits + CurrentBlockOffset);
}

void * GetDXTnColorPointer(
    const D3DMLOCKED_RECT * pLockedRect,
    D3DMFORMAT Format,
    UINT PixelX,
    UINT PixelY)
{
    // Get block offset from pixel offset.
    UINT BlockX = PixelX / 4;
    UINT BlockY = PixelY / 4;
    
    // Determine important widths, etc.
    UINT BlockStride;
    UINT ColorOffset;
    if (D3DMFMT_DXT1 == Format)
    {
        BlockStride = 8;
        ColorOffset = 0;
    }
    else
    {
        BlockStride = 16;
        ColorOffset = 8;
    }
    // Determine address to block offset.
    UINT CurrentBlockOffset = BlockY * pLockedRect->Pitch + BlockX * BlockStride + ColorOffset;
    return (void*)((BYTE*)pLockedRect->pBits + CurrentBlockOffset);
}

HRESULT GetDXTnBlockHeaders(
    const D3DMLOCKED_RECT * pSourceLocked, 
    D3DMFORMAT Format, 
    UINT iX, 
    UINT iY,
    __out_ecount(2) BYTE AlphaHeaders[2],
    __out_ecount(2) WORD ColorHeaders[2])
{
    const WORD * pColorAddress = (WORD*)GetDXTnColorPointer(pSourceLocked, Format, iX, iY);
    const BYTE * pAlphaAddress = (BYTE*)GetDXTnAlphaPointer(pSourceLocked, Format, iX, iY);
    // Do we need the alpha headers?
    bool IsAlphaNeeded = false;
    if (D3DMFMT_DXT4 == Format || D3DMFMT_DXT5 == Format)
    {
        IsAlphaNeeded = true;
    }
    
    // Get Alpha and Color headers.
    if (IsAlphaNeeded)
    {
        AlphaHeaders[0] = pAlphaAddress[0];
        AlphaHeaders[1] = pAlphaAddress[1];
    }

    ColorHeaders[0] = pColorAddress[0];
    ColorHeaders[1] = pColorAddress[1];

    return S_OK;
}

HRESULT SetDXTnBlockHeaders(
    D3DMLOCKED_RECT * pDestLocked, 
    D3DMFORMAT Format, 
    UINT iX, 
    UINT iY, 
    __out_ecount(2) const BYTE AlphaHeaders[2], 
    __out_ecount(2) const WORD ColorHeaders[2])
{
    WORD * pColorAddress = (WORD*)GetDXTnColorPointer(pDestLocked, Format, iX, iY);
    BYTE * pAlphaAddress = (BYTE*)GetDXTnAlphaPointer(pDestLocked, Format, iX, iY);

    // Do we need the alpha headers?
    bool IsAlphaNeeded = false;
    if (D3DMFMT_DXT4 == Format || D3DMFMT_DXT5 == Format)
    {
        IsAlphaNeeded = true;
    }
    
    // Get Alpha and Color headers.
    if (IsAlphaNeeded)
    {
        pAlphaAddress[0] = AlphaHeaders[0];
        pAlphaAddress[1] = AlphaHeaders[1];
    }

    pColorAddress[0] = ColorHeaders[0];
    pColorAddress[1] = ColorHeaders[1];
    return S_OK;
}

HRESULT GetDXTnPixel(
    D3DMLOCKED_RECT * pSourceLocked, 
    D3DMFORMAT Format, 
    UINT PixelX, 
    UINT PixelY, 
    BYTE * pAlpha, 
    BYTE * pColor)
{
    UINT PixelOffset;
    UINT PixelBitOffset;
    UINT ByteOffset;
    UINT BitOffset;
    BYTE PixelColor;
    BYTE PixelMask;
    const UINT ColorSize = 2;

    const BYTE * pColorAddress = (BYTE*)GetDXTnColorPointer(pSourceLocked, Format, PixelX, PixelY) + 4; // Add two * 2 for the color_0 and color_1
    const BYTE * pAlphaAddress = (BYTE*)GetDXTnAlphaPointer(pSourceLocked, Format, PixelX, PixelY);

    // Determine the block-local pixel addresses
    UINT BlockPixelX = PixelX % 4;
    UINT BlockPixelY = PixelY % 4;

    if (D3DMFMT_DXT2 == Format || 
        D3DMFMT_DXT3 == Format)
    {
        // Get the 4bit alpha value
        const UINT AlphaSize = 4;
        PixelOffset = BlockPixelY * 4 + BlockPixelX;
        PixelBitOffset = PixelOffset * AlphaSize;
        ByteOffset = PixelBitOffset / 8;
        BitOffset = PixelBitOffset % 8;
        PixelColor = pAlphaAddress[ByteOffset];
        PixelColor = PixelColor >> BitOffset;
        PixelMask = (1 << AlphaSize) - 1;
        PixelColor &= PixelMask;
        *pAlpha = PixelColor;
    }
    if (D3DMFMT_DXT4 == Format || 
        D3DMFMT_DXT5 == Format)
    {
        // Get the 3bit alpha value
        const UINT AlphaSize = 3;
        pAlphaAddress += 2; // The two header bytes.
        PixelOffset = BlockPixelY * 4 + BlockPixelX;
        PixelBitOffset = PixelOffset * AlphaSize;
        ByteOffset = PixelBitOffset / 8;
        BitOffset = PixelBitOffset % 8;

        PixelColor = pAlphaAddress[ByteOffset];
        PixelColor =  PixelColor >> BitOffset;
        if (BitOffset > 5)
        {
            BYTE SecondPart = pAlphaAddress[ByteOffset+1];
            PixelColor &= (1 << (8 - BitOffset)) - 1;
            PixelColor |= SecondPart << (8 - BitOffset);
        }
        PixelColor &= (1 << AlphaSize) - 1;
        *pAlpha = PixelColor;
    }

    // Get the 2bit color value
        // Determine pixel offset
    PixelOffset = BlockPixelY * 4 + BlockPixelX;
    PixelBitOffset = PixelOffset * ColorSize;
        // Determine byte offset of current pixel
    ByteOffset = PixelBitOffset / 8;
        // Determine bit offset
    BitOffset = PixelBitOffset % 8;
        // Get byte
    PixelColor = pColorAddress[ByteOffset];
        // Shift byte
    PixelColor = PixelColor >> BitOffset;
        // Mask byte
    PixelColor &= (1 << ColorSize) - 1;

    *pColor = PixelColor;

    return S_OK;
}

HRESULT SetDXTnPixel(
    D3DMLOCKED_RECT * pDestLocked, 
    D3DMFORMAT Format, 
    UINT PixelX, 
    UINT PixelY, 
    BYTE Alpha, 
    BYTE Color)
{
    UINT PixelOffset;
    UINT PixelBitOffset;
    UINT ByteOffset;
    UINT BitOffset;
    BYTE ColorMask;
    const UINT ColorSize = 2;

    BYTE * pColorAddress = (BYTE*)GetDXTnColorPointer(pDestLocked, Format, PixelX, PixelY) + 4; // Add two * 2 for the color_0 and color_1
    BYTE * pAlphaAddress = (BYTE*)GetDXTnAlphaPointer(pDestLocked, Format, PixelX, PixelY);

    // Determine the block-local pixel addresses
    UINT BlockPixelX = PixelX % 4;
    UINT BlockPixelY = PixelY % 4;

    if (D3DMFMT_DXT2 == Format || 
        D3DMFMT_DXT3 == Format)
    {
        const UINT AlphaSize = 4;
        PixelOffset = BlockPixelY * 4 + BlockPixelX;
        PixelBitOffset = AlphaSize * PixelOffset;
        ByteOffset = PixelBitOffset / 8;
        BitOffset = PixelBitOffset % 8;
        Alpha = Alpha << BitOffset;
        
        pAlphaAddress[ByteOffset] &= ~(((1 << AlphaSize) - 1) << BitOffset);
        pAlphaAddress[ByteOffset] |= Alpha;
    }
    if (D3DMFMT_DXT4 == Format || 
        D3DMFMT_DXT5 == Format)
    {
        BYTE AlphaByte1 = 0;
        BYTE AlphaByte2 = 0;
        BYTE AlphaMask1 = 0;
        BYTE AlphaMask2 = 0;
        const UINT AlphaSize = 3;
        pAlphaAddress += 2; // The two header bytes
        PixelOffset = BlockPixelY * 4 + BlockPixelX;
        PixelBitOffset = PixelOffset * AlphaSize;
        ByteOffset = PixelBitOffset / 8;
        BitOffset = PixelBitOffset % 8;

        AlphaByte1 = Alpha << BitOffset;
        AlphaMask1 = ((1 << AlphaSize) - 1) << BitOffset;
        if (BitOffset)
        {
            AlphaByte2 = Alpha >> (8 - BitOffset);
            AlphaMask2 = ((1 << AlphaSize) - 1) >> (8 - BitOffset);
        }
        pAlphaAddress[ByteOffset] &= ~AlphaMask1;
        pAlphaAddress[ByteOffset] |= AlphaByte1;
        if (AlphaMask2)
        {
            pAlphaAddress[ByteOffset + 1] &= ~AlphaMask2;
            pAlphaAddress[ByteOffset + 1] |= AlphaByte2;
        }
    }

    // Get the 2bit color value
    ColorMask = (1 << ColorSize) - 1;
        // Determine pixel offset
    PixelOffset = BlockPixelY * 4 + BlockPixelX;
        // Determine byte offset of current pixel
    PixelBitOffset = PixelOffset * ColorSize;
    ByteOffset = PixelBitOffset / 8;
    BitOffset = PixelBitOffset % 8;

    Color = Color << BitOffset;
    pColorAddress[ByteOffset] &= ~(ColorMask << BitOffset);
    pColorAddress[ByteOffset] |= Color;

    return S_OK;
}

HRESULT DXTnFillSmallMipMapLevel(
    IDirect3DMobileDevice * pDevice,
    IDirect3DMobileSurface * pSource, 
    const RECTF * SourceRect, 
    IDirect3DMobileSurface * pDest)
{
    // Preconditions: source rect equal to or smaller than dest rect (only stretch)
    // Source and Dest rect fall entirely on only one 4x4 block

    HRESULT hr;
    D3DMLOCKED_RECT SourceLocked;
    D3DMLOCKED_RECT DestLocked;
    D3DMSURFACE_DESC DestDesc;
    D3DMFORMAT Format;
    UINT DestWidth = 0;
    UINT DestHeight = 0;
    UINT BlocksWide = 0;
    UINT BlocksHigh = 0;
    UINT BlockCount = 0;
    bool IsDestLocked = false;
    RECT SourceRectCopy;
    POINT DestPointCopy;

    IDirect3DMobileSurface * pSurfaceTemp = NULL;

    hr = pDest->GetDesc(&DestDesc);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"Could not get description of destination surface. (hr = 0x%08x)", hr);
        goto cleanup;
    }

    DestWidth = DestDesc.Width;
    DestHeight = DestDesc.Height;
    Format = DestDesc.Format;
    
    // Lock the surfaces
    hr = pSource->LockRect(&SourceLocked, NULL, 0);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"Could not lock source surface. Surface must be an image surface. (hr = 0x%08x)", hr);
        goto cleanup;
    }

    //
    // The surface cannot be locked, so create a temporary image surface instead.
    // We will fill that surface with a gradient, and then copy over to the
    // test source surface.
    //
    if (FAILED(hr = pDevice->CreateImageSurface((DestDesc.Width + 3) & ~3, (DestDesc.Height + 3) & ~3, DestDesc.Format, &pSurfaceTemp)))
    {
        DebugOut(DO_ERROR, L"Could not create temporary image surface. hr = 0x%08x", hr);
        goto cleanup;
    }

    if (FAILED(hr = pSurfaceTemp->LockRect(&DestLocked, NULL, 0)))
    {
        DebugOut(DO_ERROR, L"Could not lock image surface. hr = 0x%08x", hr);
        goto cleanup;
    }

    // Determine the number of blocks to copy to on the Dest (in case it is 8 or more pixels high or wide)    
    BlocksWide = (DestWidth + 3) / 4;
    BlocksHigh = (DestHeight + 3) / 4;

    BlockCount = BlocksWide * BlocksHigh;

    // For each Dest block:
    for (UINT BlockY = 0; BlockY < BlocksHigh; BlockY++)
    {
        for (UINT BlockX = 0; BlockX < BlocksWide; BlockX++)
        {
            // determine the actual source rect that corresponds to it
                // Take the Source Rect and determine the current block offset into the source rect
            float SourceRectWidth = SourceRect->right - SourceRect->left;
            float SourceRectHeight = SourceRect->bottom - SourceRect->top;
            float BlockWidth = SourceRectWidth / (float)BlocksWide;
            float BlockHeight = SourceRectHeight / (float)BlocksHigh;
            float BlockOffsetX = SourceRect->left + (BlockX * BlockWidth);
            float BlockOffsetY = SourceRect->top + (BlockY * BlockHeight);
            UINT BlockPixelWidth;
            UINT BlockPixelHeight;
            RECTF BlockRect;

            if (BlockX < BlocksWide - 1 || !(DestWidth % 4))
            {
                BlockPixelWidth = 4;
            }
            else
            {
                BlockPixelWidth = DestWidth % 4;
            }
            
            if (BlockY < BlocksHigh - 1 || !(DestHeight % 4))
            {
                BlockPixelHeight = 4;
            }
            else
            {
                BlockPixelHeight = DestHeight % 4;
            }

            if (BlockWidth > 4.0f || BlockHeight > 4.0f)
            {
                DebugOut(DO_ERROR, L"We do not do shrinking!");
                goto cleanup;
            }
            if (BlockWidth <= 0.0f)
            {
                DebugOut(DO_ERROR, L"Block Width less than 0 (%f), adjusting to be 0.001", BlockWidth);
                BlockWidth = 0.001f;
            }
            if (BlockHeight <= 0.0f)
            {
                DebugOut(DO_ERROR, L"Block Height less than 0 (%f), adjusting to be 0.001", BlockHeight);
                BlockHeight = 0.001f;
            }

            BlockRect.left = BlockOffsetX;
            BlockRect.right = BlockOffsetX + BlockWidth;
            BlockRect.top = BlockOffsetY;
            BlockRect.bottom = BlockOffsetY + BlockHeight;
            
            // Crop the source rect to not span any blocks
            if (floor(BlockRect.left / 4.0f) != floor(BlockRect.right / 4.0f))
            {
                float Intersector = 4.0f * (1.0f + (float)floor(BlockRect.left / 4.0f));
                // The X dims need to be cropped to fit.
                // Determine which end to crop
                    // determine distance from center for each endpoint
                if (((BlockRect.left - Intersector) + (BlockRect.right - Intersector)) < 0.0f)
                {
                    // The left point is farthest from the 4x intersection, so crop the right point
                    BlockRect.right -= (float)fmod(BlockRect.right, 4.0f);
                }
                else
                {
                    BlockRect.left += 4.0f - (float)fmod(BlockRect.left, 4.0f);
                }
            }
            if (floor(BlockRect.top / 4.0f) != floor(BlockRect.bottom / 4.0f))
            {
                float Intersector = 4.0f * (1.0f + (float)floor(BlockRect.top / 4.0f));
                // The Y dims need to be cropped to fit.
                // Determine which end to crop
                    // determine distance from center for each endpoint
                if (((BlockRect.top - Intersector) + (BlockRect.bottom - Intersector)) < 0.0f)
                {
                    // The left point is farthest from the 4x intersection, so crop the right point
                    BlockRect.bottom -= (float)fmod(BlockRect.bottom, 4.0f);
                }
                else
                {
                    BlockRect.top += 4.0f - (float)fmod(BlockRect.top, 4.0f);
                }
            }

            // Now that the block rect has possibly been updated, recalculate the width and height
            BlockWidth = BlockRect.right - BlockRect.left;
            BlockHeight = BlockRect.bottom - BlockRect.top;

            // Copy the appropriate header colors and alpha headers

            WORD ColorHeaders[2];
            BYTE AlphaHeaders[2];
            GetDXTnBlockHeaders(
                &SourceLocked, 
                Format, 
                (UINT)(BlockRect.left), 
                (UINT)(BlockRect.top),
                AlphaHeaders,
                ColorHeaders);
            SetDXTnBlockHeaders(&DestLocked, Format, 4 * BlockX, 4 * BlockY, AlphaHeaders, ColorHeaders);
            
            for (UINT iY = 0; iY < BlockPixelHeight; iY++)
            {
                for (UINT iX = 0; iX < BlockPixelWidth; iX++)
                {
                    UINT CurrentSourcePixelX = (UINT)(BlockRect.left + iX*BlockWidth/4.0f);
                    UINT CurrentSourcePixelY = (UINT)(BlockRect.top + iY*BlockHeight/4.0f);
                    UINT DestPixelX = iX + BlockX * 4;
                    UINT DestPixelY = iY + BlockY * 4;
                    BYTE Alpha, Color;
                    GetDXTnPixel(&SourceLocked, Format, CurrentSourcePixelX, CurrentSourcePixelY, &Alpha, &Color);
                    SetDXTnPixel(&DestLocked, Format, DestPixelX, DestPixelY, Alpha, Color);
                }
            }
        }
    }

    
    pSurfaceTemp->UnlockRect();
    SetRect(&SourceRectCopy, 0, 0, DestDesc.Width, DestDesc.Height);
    DestPointCopy.x = 0;
    DestPointCopy.y = 0;
    hr = pDevice->CopyRects(pSurfaceTemp, &SourceRectCopy, 1, pDest, &DestPointCopy);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, 
            L"Could not refresh texture, CopyRects failed (%d,%d %dx%d to %d,%d). (hr = 0x%08x)", 
            SourceRectCopy.left,
            SourceRectCopy.top,
            SourceRectCopy.right - SourceRectCopy.left,
            SourceRectCopy.bottom - SourceRectCopy.top,
            DestPointCopy.x,
            DestPointCopy.y,
            hr);
        goto cleanup;
    }

cleanup:
    // Unlock the surfaces
    if (pSurfaceTemp)
    {
        pSurfaceTemp->Release();
    }
    if (IsDestLocked)
    {
        pDest->UnlockRect();
    }
    pSource->UnlockRect();
    return hr;
}

HRESULT DXTnMipMapFillTextureSurface(
    IDirect3DMobileDevice * pDevice, 
    DWORD dwTableIndex, 
    IDirect3DMobileSurface * pImageSurface,
    IDirect3DMobileTexture * pMipTexture, 
    D3DMSURFACE_DESC * pLevelDesc,
    UINT PrimX,
    UINT PrimY,
    float PrimCountX,
    float PrimCountY)
{
    HRESULT hr;
    D3DMSURFACE_DESC ImageDesc;
    IDirect3DMobileSurface * pMipSurface = NULL;
    const UINT LevelToTest = DXTnMipMapParams[dwTableIndex].LevelToTest;

    // Determine rect to use for destination
    POINT ptDest = {0, 0};
    
    // Use dest rect to determine rect for source
        // Determine width and height of image surface
    hr = pImageSurface->GetDesc(&ImageDesc);
    UINT ImageWidth = ImageDesc.Width;
    UINT ImageHeight = ImageDesc.Height;

        // Calculate where projection of primitive falls on image surface
    float PrimImageWidth = ImageWidth / PrimCountX;
    float PrimImageHeight = ImageHeight / PrimCountY;
    float ImageOffsetX = PrimImageWidth * PrimX;
    float ImageOffsetY = PrimImageHeight * PrimY;

    if (ImageOffsetX >= ImageWidth)
    {
        ImageOffsetX -= 1.0f;
    }
    if (ImageOffsetY >= ImageHeight)
    {
        ImageOffsetY -= 1.0f;
    }
    
        // Setup source rect
    RECTF rcSource = {ImageOffsetX, ImageOffsetY, ImageOffsetX + PrimImageWidth, ImageOffsetY + PrimImageHeight};

    // Get texture's surface
    hr = pMipTexture->GetSurfaceLevel(
        LevelToTest,
        &pMipSurface);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, 
            L"GetSurfaceLevel failed getting level %d. (hr = 0x%08x)",
            LevelToTest,
            hr);
        goto cleanup;
    }
    
    // Use CopyRects to copy from one surface to the other
    hr = DXTnFillSmallMipMapLevel(
        pDevice,
        pImageSurface,
        &rcSource,
        pMipSurface);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"Could not perform CopyRects. (hr = 0x%08x)", hr);
        DebugOut(
            L"Source: (%f,%f) : %f x %f; Dest: (%d ,%d)",
            rcSource.left,
            rcSource.top,
            PrimImageWidth,
            PrimImageHeight,
            ptDest.x,
            ptDest.y);
        goto cleanup;
    }

cleanup:
    if (pMipSurface)
    {
        pMipSurface->Release();
    }
    return hr;
}

HRESULT DXTnMipMapDrawPrimitives(
    IDirect3DMobileDevice * pDevice,
    HWND hwnd,
    DWORD dwTableIndex,
    IDirect3DMobileVertexBuffer * pVertices,
    IDirect3DMobileSurface * pImageSurface)
{
    HRESULT hr;
    IDirect3DMobileTexture * pMipTexture = NULL;
    const UINT TopLevelWidth = DXTnMipMapParams[dwTableIndex].TopLevelWidth;
    const UINT TopLevelHeight = DXTnMipMapParams[dwTableIndex].TopLevelHeight;
    const UINT Levels = DXTnMipMapParams[dwTableIndex].Levels;
    const UINT LevelToTest = DXTnMipMapParams[dwTableIndex].LevelToTest;
    UINT LevelWidth;
    UINT LevelHeight;
    D3DMFORMAT Format = DXTnMipMapParams[dwTableIndex].Format;
    D3DMPOOL Pools[] = {D3DMPOOL_VIDEOMEM, D3DMPOOL_SYSTEMMEM, D3DMPOOL_MANAGED};
    D3DMSURFACE_DESC LevelDesc;
    RECT rcClient;
    UINT ScreenWidth;
    UINT ScreenHeight;
    float PrimCountX;
    float PrimCountY;
    float PrimWidth;
    float PrimHeight;
    float ScaleX;
    float ScaleY;
    D3DMMATRIX MatScale;
    D3DMMATRIX MatTrans;
    D3DMMATRIX MatWorld;
    DWORD Usage = 0;
    D3DMCAPS Caps;

    hr = pDevice->GetDeviceCaps(&Caps);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"GetDeviceCaps failed. (hr = 0x%08x)", hr);
        return hr;
    }

    //
    // Do not use lockable textures, which will ensure that the CopyRects implementation 
    // for the driver works properly with non 4nx4m surface sizes
    //
    Usage = 0;

    int PoolIndex;
    for (PoolIndex = 0, hr = E_FAIL; PoolIndex < countof(Pools) && FAILED(hr); PoolIndex++)
    {
        // Create actual texture to use
        hr = pDevice->CreateTexture(
            TopLevelWidth,
            TopLevelHeight,
            Levels,
            Usage, // Usage
            Format,
            Pools[PoolIndex],
            &pMipTexture);
        if (FAILED(hr))
        {
            DebugOut(L"CreateTexture failed; Pool %d unavailable? (hr = 0x%08x)", Pools[PoolIndex], hr);
        }
    }
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"CreateTexture failed for all memory pools. Should not have failed.");
        goto cleanup;
    }

    // Get the description of the appropriate MipMap level.
    memset(&LevelDesc, 0x00, sizeof(LevelDesc));
    hr = pMipTexture->GetLevelDesc(
        LevelToTest,
        &LevelDesc);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"Could not get level desc for level %d (hr = 0x%08x)", LevelToTest, hr);
        goto cleanup;
    }

    // Set the MipMap texture stages
    hr = pDevice->SetTextureStageState(0, D3DMTSS_MAXMIPLEVEL, LevelToTest);
    if (FAILED(hr))
    {   
        DebugOut(DO_ERROR, L"Could not set texture stage state D3DMTSS_MAXMIPLEVEL to %d. (hr = 0x%08x)", LevelToTest, hr);
        goto cleanup;
    }
    if (LevelToTest > 0)
    {
        // Set the MipMap texture stages
        hr = pDevice->SetTextureStageState(0, D3DMTSS_MIPFILTER, D3DMTEXF_POINT);
        if (FAILED(hr))
        {   
            DebugOut(DO_ERROR, L"Could not set texture stage state D3DMTSS_MIPFILTER to D3DMTEXF_NONE. (hr = 0x%08x)", hr);
            goto cleanup;
        }
    }
    
    // Calculate number of triangles needed
        // Determine height and width
    LevelWidth = LevelDesc.Width;
    LevelHeight = LevelDesc.Height;
    
        // Determine current mode's height and width
    GetClientRect(hwnd, &rcClient);
    ScreenWidth = rcClient.right - rcClient.left;
    ScreenHeight = rcClient.bottom - rcClient.top;
        // Determine how many quads fit
    PrimCountX = ScreenWidth / (float)LevelWidth;
    PrimCountY = ScreenHeight / (float)LevelHeight;

    PrimWidth = 2.0f / PrimCountX;
    PrimHeight = 2.0f / PrimCountY;

    ScaleX = 1.0f / PrimCountX;
    ScaleY = 1.0f / PrimCountY;

    // Set fixed transform values
    memset(&MatScale, 0x00, sizeof(MatScale));
    MatScale.m[0][0] = _M(ScaleX);
    MatScale.m[1][1] = _M(ScaleY);
    MatScale.m[2][2] = _M(1.0f);
    MatScale.m[3][3] = _M(1.0f);

    memset(&MatTrans, 0x00, sizeof(MatTrans));
    MatTrans.m[0][0] = _M(1.0f);
    MatTrans.m[1][1] = _M(1.0f);
    MatTrans.m[2][2] = _M(1.0f);
    MatTrans.m[3][3] = _M(1.0f);

    DebugOut(L"Using %f x %f primitives in current test", PrimCountX, PrimCountY);

    // For each quad needed
    for (int CurrentPrimY = 0; CurrentPrimY < PrimCountY; CurrentPrimY++)
    {
        for (int CurrentPrimX = 0; CurrentPrimX < PrimCountX; CurrentPrimX++)
        {
            // Calculate proper offset and modify transform
            MatTrans.m[3][0] = _M(-1.0f + PrimWidth/2.0f + PrimWidth * CurrentPrimX);
            MatTrans.m[3][1] = _M(1.0f - PrimHeight/2.0f - PrimHeight * CurrentPrimY);
            D3DMatrixMultiply(&MatWorld, &MatScale, &MatTrans);
            hr = pDevice->SetTransform(D3DMTS_WORLD, &MatWorld, D3DMFMT_D3DMVALUE_FLOAT);
            if (FAILED(hr))
            {
                DebugOut(DO_ERROR, L"Could not set world transform. (hr = 0x%08x)", hr);
                goto cleanup;
            }
            // Change texture surface of appropriate level
            hr = DXTnMipMapFillTextureSurface(
                pDevice, 
                dwTableIndex, 
                pImageSurface,
                pMipTexture,
                &LevelDesc,
                CurrentPrimX,
                CurrentPrimY,
                PrimCountX,
                PrimCountY);
            if(FAILED(hr))
            {
                DebugOut(DO_ERROR, L"Could not prepare MipMap texture. (hr = 0x%08x)", hr);
                goto cleanup;
            }

            hr = pDevice->SetTexture(
                0, // Stage
                pMipTexture);
            if (FAILED(hr))
            {
                DebugOut(DO_ERROR, L"SetTexture failed. (hr = 0x%08x)", hr);
                goto cleanup;
            }
            
            // Draw the quad
            hr = pDevice->DrawPrimitive(D3DMQA_PRIMTYPE, 0, 2);
            if (FAILED(hr))
            {
                DebugOut(DO_ERROR, L"DrawPrimitive failed. (hr = 0x%08x)", hr);
                goto cleanup;
            }
        }
    }
cleanup:
    if (pMipTexture)
    {
        pMipTexture->Release();
    }
    return hr;
}


