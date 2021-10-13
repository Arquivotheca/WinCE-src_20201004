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
// This structure provides storage for bit ranges for each possible channel in
// a D3DMFORMAT surface format.
// 
//   Channel Abbreviations:
// 
//    A = Alpha
//    R = Red
//    G = Green
//    B = Blue 
//    X = Unused Bits
//    P = Palette
//    S = Stencil
//    D = Depth (e.g. Z or W buffer) 
// 
typedef struct _D3DMFORMATSPECS 
{
	// Red Bits
	INT iFirstR;
	INT iLastR;

	// Green Bits
	INT iFirstG;
	INT iLastG;

	// Blue Bits
	INT iFirstB;
	INT iLastB;

	// Alpha Bits
	INT iFirstA;
	INT iLastA;

	// Unused Bits
	INT iFirstX;
	INT iLastX;

	// Palette Bits
	INT iFirstP;
	INT iLastP;

	// Stencil Bits
	INT iFirstS;
	INT iLastS;

	// Depth Bits
	INT iFirstD;
	INT iLastD;

	//
	// Bytes Per Pixel
	//
	UINT uiBPP;

} D3DMFORMATSPECS;

__declspec(selectany) D3DMFORMATSPECS FormatSpecs[] = {
//          BIT RANGE SPECIFICATION TABLE FOR D3DMFORMAT ENUMERATION
//
//     |  Red  | Green | Blue  | Alpha |Unused |Palette|Stencil| Depth |  BPP  |
//     +-------+-------+-------+-------+-------+-------+-------+-------+-------+
//     |       |       |       |       |       |       |       |       |       |
	{    -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,    0   }, // D3DMFMT_UNKNOWN  =  0,
	{     0, 7,   8,15,  16,23,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,    3   }, // D3DMFMT_R8G8B8   =  1,
	{     8,15,  16,23,  24,31,   0, 7,  -1,-1,  -1,-1,  -1,-1,  -1,-1,    4   }, // D3DMFMT_A8R8G8B8 =  2,
	{     8,15,  16,23,  24,31,  -1,-1,   0, 7,  -1,-1,  -1,-1,  -1,-1,    4   }, // D3DMFMT_X8R8G8B8 =  3,
	{     0, 4,   5,10,  11,15,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,    2   }, // D3DMFMT_R5G6B5   =  4,
	{     1, 5,   6,10,  11,15,  -1,-1,   0, 1,  -1,-1,  -1,-1,  -1,-1,    2   }, // D3DMFMT_X1R5G5B5 =  5,
	{     1, 5,   6,10,  11,15,   0, 1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,    2   }, // D3DMFMT_A1R5G5B5 =  6,
	{     4, 7,   8,11,  12,15,   0, 3,  -1,-1,  -1,-1,  -1,-1,  -1,-1,    2   }, // D3DMFMT_A4R4G4B4 =  7,
	{     0, 2,   3, 5,   6, 7,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,    1   }, // D3DMFMT_R3G3B2   =  8,
	{     8,10,  11,13,  14,15,   0, 7,  -1,-1,  -1,-1,  -1,-1,  -1,-1,    2   }, // D3DMFMT_A8R3G3B2 =  9,
	{     4, 7,   8,11,  12,15,  -1,-1,   0, 3,  -1,-1,  -1,-1,  -1,-1,    2   }, // D3DMFMT_X4R4G4B4 = 10,
	{    -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,    0   }, // D3DMFMT_A8P8     = 11,
	{    -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,    0   }, // D3DMFMT_P8       = 12,
	{    -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,    0   }, // D3DMFMT_P1       = 13,
	{    -1,-1,  -1,-1,  -1,-1,   0, 7,  -1,-1,  -1,-1,  -1,-1,  -1,-1,    1   }, // D3DMFMT_A8       = 14,
	{    -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,    0   }, // D3DMFMT_UYVY     = 15,
	{    -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,    0   }, // D3DMFMT_YUY2     = 16,
	{    -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,    0   }, // D3DMFMT_DXT1     = 17,
	{    -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,    0   }, // D3DMFMT_DXT2     = 18,
	{    -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,    0   }, // D3DMFMT_DXT3     = 19,
	{    -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,    0   }, // D3DMFMT_DXT4     = 20,
	{    -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,    0   }, // D3DMFMT_DXT5     = 21,
	{    -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,   0,31,    4   }, // D3DMFMT_D32      = 22,
	{    -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  15,15,   0,14,    2   }, // D3DMFMT_D15S1    = 23,
	{    -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  24,31,   0,23,    4   }, // D3DMFMT_D24S8    = 24,
	{    -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,  -1,-1,   0,15,    2   }, // D3DMFMT_D16      = 25,
	{    -1,-1,  -1,-1,  -1,-1,  -1,-1,  24,31,  -1,-1,  -1,-1,   0,23,    4   }, // D3DMFMT_D24X8    = 26,
	{    -1,-1,  -1,-1,  -1,-1,  -1,-1,  24,27,  -1,-1,  28,31,   0,23,    4   }  // D3DMFMT_D24X4S4  = 27,
};
//
// Notes:
//  * Any range denoted as -1, -1 indicates that the particular channel is not present in this format.
//  * Formats not handled by this table:  paletted, DXTn, YUV.  These are denoted by a zero in the BPP field.
//

#define D3DQA_BYTES_PER_PIXEL(_f) (FormatSpecs[_f].uiBPP)

