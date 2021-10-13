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
//  Module: US_DOVRAK.h
//          
//
//  Revision History:changed #define values:
//
////////////////////////////////////////////////////////////////////////////////

#include "..\kbddef.h"

// ****************************************************
// US English DVORAK keyboard 
//
//  Num  VK1   VK2    Num   Unicode
testSequence KBDTests_USDVORAK[] =
{
    {1, {0x20, 0x00}, 1,    0x20},      // ' '
    {2, {0x10, 0x51}, 1,    0x44},      // 'D'
    {2, {0x10, 0x57}, 1,    0x56},      // 'V'
    {2, {0x10, 0x45}, 1,    0x4F},      // 'O'
    {2, {0x10, 0x54}, 1,    0x52},      // 'R'
    {1, {0x50, 0x00}, 1,    0x64},      // 'd'
};
