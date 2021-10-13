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
//  Module: THAI.h
//          
//
//  Revision History:changed #define values:
//
////////////////////////////////////////////////////////////////////////////////

#include "..\kbddef.h"

// ****************************************************
// Thai keyboard - Kedmanee
//
//  Num  VK1   VK2    Num   Unicode
testSequence KBDTests_Thai[] = 
{
    // With CAPSLOCK OFF
    // Without VK_SHIFT
    {1, {0x20, 0x00}, 1,    0x0020}, // Space key
    //  {1, {0x1B, 0x00}, 1,    0x001B}, // Esc key
    {1, {0x08, 0x00}, 1,    0x0008}, // BackSpace key
    {1, {0x0D, 0x00}, 1,    0x000D}, // Enter key
    {1, {0x09, 0x00}, 1,    0x0009}, // Tab key
    /* Keys `1234567890-= */
    {1, {0xC0, 0x00}, 1,    0x005F},      
    {1, {0x31, 0x00}, 1,    0x0E45},      
    {1, {0x32, 0x00}, 1,    0x002F},      
    {1, {0x33, 0x00}, 1,    0x002D},      
    {1, {0x34, 0x00}, 1,    0x0E20},      
    {1, {0x35, 0x00}, 1,    0x0E16},      
    {1, {0x36, 0x00}, 1,    0x0E38},      
    {1, {0x37, 0x00}, 1,    0x0E36},      
    {1, {0x38, 0x00}, 1,    0x0E04},      
    {1, {0x39, 0x00}, 1,    0X0E15},      
    {1, {0x30, 0x00}, 1,    0x0E08},      
    {1, {0xBD, 0x00}, 1,    0x0E02},      
    {1, {0xBB, 0x00}, 1,    0x0E0A},      
    /* Keys qwertyuiop[]\ */
    {1, {0x51, 0x00}, 1,    0x0E46},            
    {1, {0x57, 0x00}, 1,    0x0E44},            
    {1, {0x45, 0x00}, 1,    0x0E33},            
    {1, {0x52, 0x00}, 1,    0x0E1E},            
    {1, {0x54, 0x00}, 1,    0x0E30},            
    {1, {0x59, 0x00}, 1,    0x0E31},            
    {1, {0x55, 0x00}, 1,    0x0E35},            
    {1, {0x49, 0x00}, 1,    0x0E23},            
    {1, {0x4F, 0x00}, 1,    0x0E19},            
    {1, {0x50, 0x00}, 1,    0x0E22},            
    {1, {0xDB, 0x00}, 1,    0x0E1A},            
    {1, {0xDD, 0x00}, 1,    0x0E25},            
    {1, {0xDC, 0x00}, 1,    0x0E03},            
    /* Keys asdfghjkl;' */
    {1, {0x41, 0x00}, 1,    0x0E1F},            
    {1, {0x53, 0x00}, 1,    0x0E2B},            
    {1, {0x44, 0x00}, 1,    0x0E01},            
    {1, {0x46, 0x00}, 1,    0x0E14},            
    {1, {0x47, 0x00}, 1,    0x0E40},            
    {1, {0x48, 0x00}, 1,    0x0E49},            
    {1, {0x4A, 0x00}, 1,    0x0E48},            
    {1, {0x4B, 0x00}, 1,    0x0E32},            
    {1, {0x4C, 0x00}, 1,    0x0E2A},            
    {1, {0xBA, 0x00}, 1,    0x0E27},      
    {1, {0xDE, 0x00}, 1,    0x0E07},      
    /* Keys zxcvbnm,./ */
    {1, {0x5A, 0x00}, 1,    0x0E1C},          
    {1, {0x58, 0x00}, 1,    0x0E1B},            
    {1, {0x43, 0x00}, 1,    0x0E41},            
    {1, {0x56, 0x00}, 1,    0x0E2D},
    {1, {0x42, 0x00}, 1,    0x0E34},            
    {1, {0x4E, 0x00}, 1,    0x0E37},            
    {1, {0x4D, 0x00}, 1,    0x0E17},      
    {1, {0xBC, 0x00}, 1,    0x0E21},
    {1, {0xBE, 0x00}, 1,    0x0E43},      
    {1, {0xBF, 0x00}, 1,    0x0E1D},
    // With VK_SHIFT
    /* Keys `1234567890-= */
    {2, {0x10, 0xC0}, 1,    0x0025},     
    {2, {0x10, 0x31}, 1,    0x002B},      
    {2, {0x10, 0x32}, 1,    0x0E51},     
    {2, {0x10, 0x33}, 1,    0x0E52},      
    {2, {0x10, 0x34}, 1,    0x0E53},      
    {2, {0x10, 0x35}, 1,    0x0E54},      
    {2, {0x10, 0x36}, 1,    0x0E39},     
    {2, {0x10, 0x37}, 1,    0x0E3F},     
    {2, {0x10, 0x38}, 1,    0x0E55},     
    {2, {0x10, 0x39}, 1,    0x0E56},     
    {2, {0x10, 0x30}, 1,    0x0E57},      
    {2, {0x10, 0xBD}, 1,    0x0E58},     
    {2, {0x10, 0xBB}, 1,    0x0E59},      
    /* Keys qwertyuiop[]\ */
    {2, {0x10, 0x51}, 1,    0x0E50},            
    {2, {0x10, 0x57}, 1,    0x0022},            
    {2, {0x10, 0x45}, 1,    0x0E0E},        
    {2, {0x10, 0x52}, 1,    0x0E11},            
    {2, {0x10, 0x54}, 1,    0x0E18},
    {2, {0x10, 0x59}, 1,    0x0E4D},            
    {2, {0x10, 0x55}, 1,    0x0E4A},            
    {2, {0x10, 0x49}, 1,    0x0E13},            
    {2, {0x10, 0x4F}, 1,    0x0E2F},            
    {2, {0x10, 0x50}, 1,    0x0E0D},            
    {2, {0x10, 0xDB}, 1,    0x0E10},            
    {2, {0x10, 0xDD}, 1,    0x002C},        
    {2, {0x10, 0xDC}, 1,    0x0E05},            
    /* Keys asdfhjkl;' */
    {2, {0x10, 0x41}, 1,    0x0E24},            
    {2, {0x10, 0x53}, 1,    0x0E06},            
    {2, {0x10, 0x44}, 1,    0x0E0F},            
    {2, {0x10, 0x46}, 1,    0x0E42},
    {2, {0x10, 0x47}, 1,    0x0E0C},            
    {2, {0x10, 0x48}, 1,    0x0E47},            
    {2, {0x10, 0x4A}, 1,    0x0E4B},            
    {2, {0x10, 0x4B}, 1,    0x0E29},            
    {2, {0x10, 0x4C}, 1,    0x0E28},            
    {2, {0x10, 0xBA}, 1,    0x0E0B},    
    {2, {0x10, 0xDE}, 1,    0x002E},     
    /* Keys zxcvbnm,./ */
    {2, {0x10, 0x5A}, 1,    0x0028},          
    {2, {0x10, 0x58}, 1,    0x0029},            
    {2, {0x10, 0x43}, 1,    0x0E09},            
    {2, {0x10, 0x56}, 1,    0x0E2E},            
    {2, {0x10, 0x42}, 1,    0x0E3A},
    {2, {0x10, 0x4E}, 1,    0x0E4C},            
    {2, {0x10, 0x4D}, 1,    0x003F},            
    {2, {0x10, 0xBC}, 1,    0x0E12},     
    {2, {0x10, 0xBE}, 1,    0x0E2C},      
    {2, {0x10, 0xBF}, 1,    0x0E26},
    // Control chars (with VK_CONTROL)
    // Row ZXCV
    {2, {0x11, 90}, 1, 0x1A},           // Ctrl Z
    {2, {0x11, 88}, 1, 0x18},           // Ctrl X
    {2, {0x11, 67}, 1, 0x03},           // Ctrl C
    {2, {0x11, 86}, 1, 0x16},           // Ctrl V
    {2, {0x11, 66}, 1, 0x02},           // Ctrl B
    {2, {0x11, 78}, 1, 0x0E},           // Ctrl N
    {2, {0x11, 77}, 1, 0x0D},           // Ctrl M
    //Row ASDF
    {2, {0x11, 65}, 1,0x01},            // Ctrl A
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

    // Turn CAPSLOCK ON
    {STATE_CAPSLOCK, {0x10, 0x14}, 0, 0},

    // With CAPSLOCK ON
    // Without VK_SHIFT
    {1, {0x20, 0x00}, 1,    0x0020}, // Space key
    //  {1, {0x1B, 0x00}, 1,    0x001B}, // Esc key
    {1, {0x08, 0x00}, 1,    0x0008}, // BackSpace key
    {1, {0x0D, 0x00}, 1,    0x000D}, // Enter key
    {1, {0x09, 0x00}, 1,    0x0009}, // Tab key
    /* Keys `1234567890-= */
    {1, {0xC0, 0x00}, 1,    0x0025},      
    {1, {0x31, 0x00}, 1,    0x002B},      
    {1, {0x32, 0x00}, 1,    0x0E51},      
    {1, {0x33, 0x00}, 1,    0x0E52},      
    {1, {0x34, 0x00}, 1,    0x0E53},      
    {1, {0x35, 0x00}, 1,    0x0E54},      
    {1, {0x36, 0x00}, 1,    0x0E39},      
    {1, {0x37, 0x00}, 1,    0x0E3f},      
    {1, {0x38, 0x00}, 1,    0x0E55},      
    {1, {0x39, 0x00}, 1,    0x0E56},      
    {1, {0x30, 0x00}, 1,    0x0E57},      
    {1, {0xBD, 0x00}, 1,    0x0E58},      
    {1, {0xBB, 0x00}, 1,    0x0E59},      
    /* Keys qwertyuiop[]\ */
    {1, {0x51, 0x00}, 1,    0x0E50},            
    {1, {0x57, 0x00}, 1,    0x0022},            
    {1, {0x45, 0x00}, 1,    0x0E0E},            
    {1, {0x52, 0x00}, 1,    0x0E11},            
    {1, {0x54, 0x00}, 1,    0x0E18},            
    {1, {0x59, 0x00}, 1,    0x0E4D},            
    {1, {0x55, 0x00}, 1,    0x0E4A},            
    {1, {0x49, 0x00}, 1,    0x0E13},            
    {1, {0x4F, 0x00}, 1,    0x0E2F},            
    {1, {0x50, 0x00}, 1,    0x0E0D},            
    {1, {0xDB, 0x00}, 1,    0x0E10},            
    {1, {0xDD, 0x00}, 1,    0x002C},            
    {1, {0xDC, 0x00}, 1,    0x0E05},            
    /* Keys asdfghjkl;' */
    {1, {0x41, 0x00}, 1,    0x0E24},            
    {1, {0x53, 0x00}, 1,    0x0E06},            
    {1, {0x44, 0x00}, 1,    0x0E0F},            
    {1, {0x46, 0x00}, 1,    0x0E42},            
    {1, {0x47, 0x00}, 1,    0x0E0C},            
    {1, {0x48, 0x00}, 1,    0x0E47},            
    {1, {0x4A, 0x00}, 1,    0x0E4B},            
    {1, {0x4B, 0x00}, 1,    0x0E29},            
    {1, {0x4C, 0x00}, 1,    0x0E28},            
    {1, {0xBA, 0x00}, 1,    0x0E0B},      
    {1, {0xDE, 0x00}, 1,    0x002E},      
    /* Keys zxcvbnm,./ */
    {1, {0x5A, 0x00}, 1,    0x0028},          
    {1, {0x58, 0x00}, 1,    0x0029},            
    {1, {0x43, 0x00}, 1,    0x0E09},            
    {1, {0x56, 0x00}, 1,    0x0E2E},
    {1, {0x42, 0x00}, 1,    0x0E3A},            
    {1, {0x4E, 0x00}, 1,    0x0E4C},
    {1, {0x4D, 0x00}, 1,    0x003F},      
    {1, {0xBC, 0x00}, 1,    0x0E12},
    {1, {0xBE, 0x00}, 1,    0x0E2C},      
    {1, {0xBF, 0x00}, 1,    0x0E26},
    // With VK_SHIFT
    /* Keys `1234567890-= */
    {2, {0x10, 0xC0}, 1,    0x005F},     
    {2, {0x10, 0x31}, 1,    0x0E45},      
    {2, {0x10, 0x32}, 1,    0x002F},     
    {2, {0x10, 0x33}, 1,    0x002D},      
    {2, {0x10, 0x34}, 1,    0x0E20},      
    {2, {0x10, 0x35}, 1,    0x0E16},      
    {2, {0x10, 0x36}, 1,    0x0E38},     
    {2, {0x10, 0x37}, 1,    0x0E36},     
    {2, {0x10, 0x38}, 1,    0x0E04},     
    {2, {0x10, 0x39}, 1,    0x0E15},     
    {2, {0x10, 0x30}, 1,    0x0E08},      
    {2, {0x10, 0xBD}, 1,    0x0E02},     
    {2, {0x10, 0xBB}, 1,    0x0E0A},      
    /* Keys qwertyuiop[]\ */
    {2, {0x10, 0x51}, 1,    0x0E46},            
    {2, {0x10, 0x57}, 1,    0x0E44},            
    {2, {0x10, 0x45}, 1,    0x0E33},        
    {2, {0x10, 0x52}, 1,    0x0E1E},            
    {2, {0x10, 0x54}, 1,    0x0E30},
    {2, {0x10, 0x59}, 1,    0x0E31},            
    {2, {0x10, 0x55}, 1,    0x0E35},            
    {2, {0x10, 0x49}, 1,    0x0E23},            
    {2, {0x10, 0x4F}, 1,    0x0E19},            
    {2, {0x10, 0x50}, 1,    0x0E22},            
    {2, {0x10, 0xDB}, 1,    0x0E1A},            
    {2, {0x10, 0xDD}, 1,    0x0E25},        
    {2, {0x10, 0xDC}, 1,    0x0E03},            
    /* Keys asdfhjkl;' */
    {2, {0x10, 0x41}, 1,    0x0E1F},            
    {2, {0x10, 0x53}, 1,    0x0E2B},            
    {2, {0x10, 0x44}, 1,    0x0E01},            
    {2, {0x10, 0x46}, 1,    0x0E14},
    {2, {0x10, 0x47}, 1,    0x0E40},            
    {2, {0x10, 0x48}, 1,    0x0E49},            
    {2, {0x10, 0x4A}, 1,    0x0E48},            
    {2, {0x10, 0x4B}, 1,    0x0E32},            
    {2, {0x10, 0x4C}, 1,    0x0E2A},            
    {2, {0x10, 0xBA}, 1,    0x0E27},    
    {2, {0x10, 0xDE}, 1,    0x0E07},     
    /* Keys zxcvbnm,./ */
    {2, {0x10, 0x5A}, 1,    0x0E1C},          
    {2, {0x10, 0x58}, 1,    0x0E1B},            
    {2, {0x10, 0x43}, 1,    0x0E41},            
    {2, {0x10, 0x56}, 1,    0x0E2D},            
    {2, {0x10, 0x42}, 1,    0x0E34},
    {2, {0x10, 0x4E}, 1,    0x0E37},            
    {2, {0x10, 0x4D}, 1,    0x0E17},            
    {2, {0x10, 0xBC}, 1,    0x0E21},     
    {2, {0x10, 0xBE}, 1,    0x0E43},      
    {2, {0x10, 0xBF}, 1,    0x0E1D},
    // Control chars (with VK_CONTROL)
    // Row ZXCV
    {2, {0x11, 90}, 1, 0x1A},          // Ctrl Z
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

    //Turn CAPSLOCK OFF
    {STATE_CAPSLOCK, {0x10, 0x14}, 0,  0},
}; // end Thai keyboard
