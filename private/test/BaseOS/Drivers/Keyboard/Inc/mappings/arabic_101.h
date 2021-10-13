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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
////////////////////////////////////////////////////////////////////////////////
//
//  TUXTEST TUX DLL
//  Copyright (c) Microsoft Corporation
//
//  Module: ARABIC_101.h
//          
//
//  Revision History:changed #define values:
//
////////////////////////////////////////////////////////////////////////////////

#include "..\kbddef.h"

// ****************************************************
// Saudi Arabia keyboard (101)
//
//  Num  VK1   VK2    Num   Unicode
testSequence KBDTests_Arabic[] = 
{
    // Without VK_SHIFT
    {1, {0x20, 0x00}, 1,    0x0020}, // Space key
    //  {1, {0x1B, 0x00}, 1,    0x001B}, // Esc key
    {1, {0x08, 0x00}, 1,    0x0008}, // BackSpace key
    {1, {0x0D, 0x00}, 1,    0x000D}, // Enter key
    {1, {0x09, 0x00}, 1,    0x0009}, // Tab key
    /* Keys `1234567890-= */
    {1, {0xC0, 0x00}, 1,    0x0630},      
    {1, {0x31, 0x00}, 1,    0x0031},      
    {1, {0x32, 0x00}, 1,    0x0032},      
    {1, {0x33, 0x00}, 1,    0x0033},      
    {1, {0x34, 0x00}, 1,    0x0034},      
    {1, {0x35, 0x00}, 1,    0x0035},      
    {1, {0x36, 0x00}, 1,    0x0036},      
    {1, {0x37, 0x00}, 1,    0x0037},      
    {1, {0x38, 0x00}, 1,    0x0038},      
    {1, {0x39, 0x00}, 1,    0X0039},      
    {1, {0x30, 0x00}, 1,    0x0030},      
    {1, {0xBD, 0x00}, 1,    0x002D},      
    {1, {0xBB, 0x00}, 1,    0x003D},      
    /* Keys qwertyuiop[]\ */
    {1, {0x51, 0x00}, 1,    0x0636},            
    {1, {0x57, 0x00}, 1,    0x0635},            
    {1, {0x45, 0x00}, 1,    0x062B},            
    {1, {0x52, 0x00}, 1,    0x0642},            
    {1, {0x54, 0x00}, 1,    0x0641},            
    {1, {0x59, 0x00}, 1,    0x063A},            
    {1, {0x55, 0x00}, 1,    0x0639},            
    {1, {0x49, 0x00}, 1,    0x0647},            
    {1, {0x4F, 0x00}, 1,    0x062E},            
    {1, {0x50, 0x00}, 1,    0x062D},            
    {1, {0xDB, 0x00}, 1,    0x062C},            
    {1, {0xDD, 0x00}, 1,    0x062F},            
    {1, {0xDC, 0x00}, 1,    0x005C},            
    /* Keys asdfghjkl;' */
    {1, {0x41, 0x00}, 1,    0x0634},            
    {1, {0x53, 0x00}, 1,    0x0633},            
    {1, {0x44, 0x00}, 1,    0x064A},            
    {1, {0x46, 0x00}, 1,    0x0628},            
    {1, {0x47, 0x00}, 1,    0x0644},            
    {1, {0x48, 0x00}, 1,    0x0627},            
    {1, {0x4A, 0x00}, 1,    0x062A},            
    {1, {0x4B, 0x00}, 1,    0x0646},            
    {1, {0x4C, 0x00}, 1,    0x0645},            
    {1, {0xBA, 0x00}, 1,    0x0643},      
    {1, {0xDE, 0x00}, 1,    0x0637},      
    /* Keys zxcvnm,./ */
    {1, {0x5A, 0x00}, 1,    0x0626},          
    {1, {0x58, 0x00}, 1,    0x0621},            
    {1, {0x43, 0x00}, 1,    0x0624},            
    {1, {0x56, 0x00}, 1,    0x0631},            
    {1, {0x4E, 0x00}, 1,    0x0649},            
    {1, {0x4D, 0x00}, 1,    0x0629},            
    {1, {0xBC, 0x00}, 1,    0x0648},      
    {1, {0xBE, 0x00}, 1,    0x0632},      
    {1, {0xBF, 0x00}, 1,    0x0638},      
    // With VK_SHIFT
    /* Keys `1234567890-= */
    {2, {0x10, 0xC0}, 1,    0x0651},     
    {2, {0x10, 0x31}, 1,    0x0021},      
    {2, {0x10, 0x32}, 1,    0x0040},     
    {2, {0x10, 0x33}, 1,    0x0023},      
    {2, {0x10, 0x34}, 1,    0x0024},      
    {2, {0x10, 0x35}, 1,    0x0025},      
    {2, {0x10, 0x36}, 1,    0x005E},     
    {2, {0x10, 0x37}, 1,    0x0026},     
    {2, {0x10, 0x38}, 1,    0x002A},     
    {2, {0x10, 0x39}, 1,    0x0029},     
    {2, {0x10, 0x30}, 1,    0x0028},      
    {2, {0x10, 0xBD}, 1,    0x005F},     
    {2, {0x10, 0xBB}, 1,    0x002B},      
    /* Keys qweryuiop[]\ */
    {2, {0x10, 0x51}, 1,    0x064E},            
    {2, {0x10, 0x57}, 1,    0x064B},            
    {2, {0x10, 0x45}, 1,    0x064F},        
    {2, {0x10, 0x52}, 1,    0x064C},            
    {2, {0x10, 0x59}, 1,    0x0625},            
    {2, {0x10, 0x55}, 1,    0x2018},            
    {2, {0x10, 0x49}, 1,    0x00F7},            
    {2, {0x10, 0x4F}, 1,    0x00D7},            
    {2, {0x10, 0x50}, 1,    0x061B},            
    {2, {0x10, 0xDB}, 1,    0x003C},            
    {2, {0x10, 0xDD}, 1,    0x003E},        
    {2, {0x10, 0xDC}, 1,    0x007C},            
    /* Keys asdfhjkl;' */
    {2, {0x10, 0x41}, 1,    0x0650},            
    {2, {0x10, 0x53}, 1,    0x064D},            
    {2, {0x10, 0x44}, 1,    0x005D},            
    {2, {0x10, 0x46}, 1,    0x005B},            
    {2, {0x10, 0x48}, 1,    0x0623},            
    {2, {0x10, 0x4A}, 1,    0x0640},            
    {2, {0x10, 0x4B}, 1,    0x060C},            
    {2, {0x10, 0x4C}, 1,    0x002F},            
    {2, {0x10, 0xBA}, 1,    0x003A},    
    {2, {0x10, 0xDE}, 1,    0x0022},     
    /* Keys zxcvnm,./ */
    {2, {0x10, 0x5A}, 1,    0x007E},          
    {2, {0x10, 0x58}, 1,    0x0652},            
    {2, {0x10, 0x43}, 1,    0x007D},            
    {2, {0x10, 0x56}, 1,    0x007B},            
    {2, {0x10, 0x4E}, 1,    0x0622},            
    {2, {0x10, 0x4D}, 1,    0x2019},            
    {2, {0x10, 0xBC}, 1,    0x002C},     
    {2, {0x10, 0xBE}, 1,    0x002E},      
    {2, {0x10, 0xBF}, 1,    0x061F},
    // Control chars
    // Row ZXCV
    {2, {0x11, 90}, 1, 0x1A},           // Ctrl Z
    {2, {0x11, 88}, 1, 0x18},           // Ctrl X
    {2, {0x11, 67}, 1, 0x03},           // Ctrl C
    {2, {0x11, 86}, 1, 0x16},           // Ctrl V
    {2, {0x11, 66}, 1, 0x02},           // Ctrl B
    {2, {0x11, 78}, 1, 0x0E},           // Ctrl N
    {2, {0x11, 77}, 1, 0x0D},           // Ctrl M
    //Row ASDF
    {2, {0x11, 65}, 1, 0x01},           // Ctrl A
    {2, {0x11, 83}, 1, 0x13},           // Ctrl S
    {2, {0x11, 68}, 1, 0x04},           // Ctrl D
    {2, {0x11, 70}, 1, 0x06},           // Ctrl F
    {2, {0x11, 71}, 1, 0x07},           // Ctrl G
    {2, {0x11, 72}, 1, 0x08},           // Ctrl H
    {2, {0x11, 74}, 1, 0x0A},           // Ctrl J
    {2, {0x11, 75}, 1, 0x0B},           // Ctrl K
    {2, {0x11, 76}, 1, 0x0C},           // Ctrl L
    // Row QWER
    {2, {0x11, 81}, 1, 0x11},           // Ctrl Q
    {2, {0x11, 87}, 1, 0x17},           // Ctrl W
    {2, {0x11, 69}, 1, 0x05},           // Ctrl E
    {2, {0x11, 82}, 1, 0x12},           // Ctrl R
    {2, {0x11, 84}, 1, 0x14},           // Ctrl T
    {2, {0x11, 89}, 1, 0x19},           // Ctrl Y
    {2, {0x11, 85}, 1, 0x15},           // Ctrl U
    {2, {0x11, 73}, 1, 0x09},           // Ctrl I
    {2, {0x11, 79}, 1, 0x0F},           // Ctrl O
    {2, {0x11, 80}, 1, 0x10},           // Ctrl P
    // Keystrokes that generate multiple chars
    {1, {0x42, 0x00}, 2,    {0x0644, 0x0627}}, // Key B
    {2, {0x10, 0x54}, 2,    {0x0644, 0x0625}}, // Keys Shift + T
    {2, {0x10, 0x47}, 2,    {0x0644, 0x0623}}, // Keys Shift + G
    {2, {0x10, 0x42}, 2,    {0x0644, 0x0622}}, // Keys Shift + B
}; // end Arabic (101) keyboard
