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
//  Module: JPN,h
//          
//
//  Revision History:changed #define values:
//
////////////////////////////////////////////////////////////////////////////////

#include "..\kbddef.h"

// ****************************************************
// French keyboard
//
// Simple demo keyboard with deadkeys
//
//  Num  VK1   VK2    Num   Unicode
testSequence KBDTests_French[] = 
{
    {1, {0x20, 0x00}, 1,    0x20},      // spacekey results in ' '
    {1, {0x51 }, 1, 0x71},             // 'a' key on US keyboards is the 'q' key on French keyboards.

    {1, {0x31}, 1, 0x26},              // '1' key displays '&' in french keyboard :)
    {2, {VK_SHIFT, 0x31}, 1, 0x31},    // shift + '1' results in '1' on french keyboard

    {1, {0x32}, 1, 0xE9},              // '2' key displays 'e' w/ accent on top
    {2, {VK_SHIFT, 0x32}, 1, 0x32},    // shift + '2' results in '2'

    // try out a dead key...
    //{3, {VK_CONTROL, VK_MENU, 0x32}, 1, 0x73}, // ALT-Gr key + '2' results in tilde dead key
    {3, {VK_CONTROL, VK_MENU, 0x32}, 0, 0}, // dead key generate WM_DEADCHAR but not WM_CHAR
    {1, {0x41, }, 1, 0xE3},            // press 'a' key and result is 'a'+tilde

    //{3, {VK_CONTROL, VK_MENU, 0x32}, 1, 0x73}, // ALT-Gr key + '2' results in tilde dead key
    {3, {VK_CONTROL, VK_MENU, 0x32}, 0, 0}, // dead key generate WM_DEADCHAR but not WM_CHAR
    {1, {0x51}, 2, {0x7E, 0x71}},           // dead key doesn't effect 'q'... this keystroke generates two WM_CHARs
}; // end French keyboard