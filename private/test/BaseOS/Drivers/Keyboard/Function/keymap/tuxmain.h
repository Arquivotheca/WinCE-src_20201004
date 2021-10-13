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
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1997  Microsoft Corporation.  All Rights Reserved.

Module Name:

tuxmain.h  

Abstract:
Functions:
Notes:
--*/
/*++


Module Name:

tuxmain.h

Abstract:

This file contians the global information needed
by tests.

Author:

Uknown (unknown)

Notes:

--*/
#pragma once

#include <Tux.h>
#include <KatoEx.h>

extern CKato *g_pKato;
extern SPS_SHELL_INFO  g_spsShellInfo;



const static int PROJECT_CE = 0x01;
const static int PROJECT_SMARTFON = 0x02;
const static int PROJECT_UNSUPPORTED = 0xD903;
const static int PLATFORM_CEPC = 0x10;
const static int PLATFORM_TORNADO = 0x12;
const static int PLATFORM_EXCALIBER = 0x13;
const static int PLATFORM_UNSUPPORTED = 0xD903;
const static int TYPE_RETAIL = 0x20;
const static int TYPE_DEBUG = 0x21;

const static int MAX_STRING_NAME = 256;

int GetPlatformCode(_In_count_(uiLen) wchar_t *pPlatformName, UINT uiLen);
int GetProjectCode(_In_count_(uiLen) wchar_t *pProjectName, UINT uiLen);

// Maximum # of slots available in preload reg key. This #define is taken from \public\common\oak\inc\pwinuser.h
#define MAX_HKL_PRELOAD_COUNT   20
#define PRELOAD_PATH TEXT("Keyboard Layout\\Preload\\")
#define LAYOUT_PATH TEXT ("System\\CurrentControlSet\\Control\\Layouts\\")

//Global array to store the input locale lists from the registry at the start of the test
HKL g_rghInputLocale [MAX_HKL_PRELOAD_COUNT] = {NULL};
//Global array to store the Keyboard Layouts associated with each Input Locale at the start of the test
HKL g_rghKeyboardLayout [MAX_HKL_PRELOAD_COUNT] = {NULL};

//total number of locale's 
UINT g_cInputLocale = 0;

BOOL GetPreloadHKLs ();



