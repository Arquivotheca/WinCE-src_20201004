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

#include <windows.h>

// Number of characters needed to output a byte in binary (including NULL terminator)
#define QA_BYTE_BIT_LENGTH 9

//
// Information needed to "unpack" a pixel format into
// seperate components, for convenience in color conversion.
//
typedef struct _PIXEL_UNPACK {
    DWORD dwRedMask;
    DWORD dwGreenMask;
    DWORD dwBlueMask;
    DWORD dwAlphaMask;
    UCHAR uchRedSHR;
    UCHAR uchGreenSHR;
    UCHAR uchBlueSHR;
    UCHAR uchAlphaSHR;
    UCHAR uchRedBits;
    UCHAR uchGreenBits;
    UCHAR uchBlueBits;
    UCHAR uchAlphaBits;
    UINT  uiTotalBytes;
    WCHAR wszDescription[10];
} PIXEL_UNPACK;

//
// Descriptions of pixel formats, useful for seperating out each color component.
// Indices of this table are aligned with the D3DMFORMAT enumeration.
//
#define QA_NUM_UNPACK_FORMATS 12
__declspec(selectany) PIXEL_UNPACK UnpackFormat[QA_NUM_UNPACK_FORMATS] = 
// |                COMPONENT MASKS                    |        COMPONENT "SHIFTS"         |                                   |            |                     |
// |   (FOR PIXEL FORMAT RIGHT-ALIGNED IN DWORD)       | (RSHIFT TO RIGHT-ALIGN COMPONENT) |        BITS PER COMPONENT         | BYTES PER  |      COMPONENT      |
// |                                                   |                                   |                                   |   PIXEL    |     DESCRIPTION     |
// |     Red    |    Green   |    Blue    |   Alpha    |   Red  |  Green |  Blue  |  Alpha |   Red  |  Green |  Blue  |  Alpha |            |                     |
// +------------+------------+------------+------------+--------+--------+--------+--------+--------+--------+--------+--------+------------+---------------------+
{{   0x00000000 , 0x00000000 , 0x00000000 , 0x00000000 ,     0  ,     0  ,     0  ,     0  ,     0  ,     0  ,     0  ,     0  ,        0   ,      L"UNKNOWN"     }, // D3DMFMT_UNKNOWN
 {   0x00FF0000 , 0x0000FF00 , 0x000000FF , 0x00000000 ,    16  ,     8  ,     0  ,     0  ,     8  ,     8  ,     8  ,     0  ,        3   ,      L"R8G8B8"      }, // D3DMFMT_R8G8B8  
 {   0x00FF0000 , 0x0000FF00 , 0x000000FF , 0xFF000000 ,    16  ,     8  ,     0  ,    24  ,     8  ,     8  ,     8  ,     8  ,        4   ,      L"A8R8G8B8"    }, // D3DMFMT_A8R8G8B8
 {   0x00FF0000 , 0x0000FF00 , 0x000000FF , 0x00000000 ,    16  ,     8  ,     0  ,     0  ,     8  ,     8  ,     8  ,     0  ,        4   ,      L"X8R8G8B8"    }, // D3DMFMT_X8R8G8B8
 {   0x0000F800 , 0x000007E0 , 0x0000001F , 0x00000000 ,    11  ,     5  ,     0  ,     0  ,     5  ,     6  ,     5  ,     0  ,        2   ,      L"R5G6B5"      }, // D3DMFMT_R5G6B5  
 {   0x00007C00 , 0x000003E0 , 0x0000001F , 0x00000000 ,    10  ,     5  ,     0  ,     0  ,     5  ,     5  ,     5  ,     0  ,        2   ,      L"X1R5G5B5"    }, // D3DMFMT_X1R5G5B5
 {   0x00007C00 , 0x000003E0 , 0x0000001F , 0x00008000 ,    10  ,     5  ,     0  ,    15  ,     5  ,     5  ,     5  ,     1  ,        2   ,      L"A1R5G5B5"    }, // D3DMFMT_A1R5G5B5
 {   0x00000F00 , 0x000000F0 , 0x0000000F , 0x0000F000 ,     8  ,     4  ,     0  ,    12  ,     4  ,     4  ,     4  ,     4  ,        2   ,      L"A4R4G4B4"    }, // D3DMFMT_A4R4G4B4
 {   0x000000E0 , 0x0000001C , 0x00000003 , 0x00000000 ,     5  ,     2  ,     0  ,     0  ,     3  ,     3  ,     2  ,     0  ,        1   ,      L"R3G3B2"      }, // D3DMFMT_R3G3B2  
 {   0x000000E0 , 0x0000001C , 0x00000003 , 0x0000FF00 ,     5  ,     2  ,     0  ,     8  ,     3  ,     3  ,     2  ,     8  ,        2   ,      L"A8R3G3B2"    }, // D3DMFMT_A8R3G3B2
 {   0x00000F00 , 0x000000F0 , 0x0000000F , 0x00000000 ,     8  ,     4  ,     0  ,     0  ,     4  ,     4  ,     4  ,     0  ,        2   ,      L"X4R4G4B4"    }};// D3DMFMT_X4R4G4B4

//
// Structures for designating various pixel format sizes
//
typedef struct _BPP32 {
    UCHAR Data[4];
} BPP32;
typedef struct _BPP24 {
    UCHAR Data[3];
} BPP24;
typedef struct _BPP16 {
    UCHAR Data[2];
} BPP16;
typedef struct _BPP8 {
    UCHAR Data[1];
} BPP8;

VOID ConvertPixel(CONST PIXEL_UNPACK *pSource, CONST PIXEL_UNPACK *pDest, CONST BYTE* pSourcePixel, BYTE* pDestPixel);
