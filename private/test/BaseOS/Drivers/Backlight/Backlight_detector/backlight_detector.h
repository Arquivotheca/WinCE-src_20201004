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
//******************************************************************************

#pragma once

#include <windows.h>
#include <tchar.h>
#include <katoex.h>
#include <tux.h>
#include <clparse.h>
#include <svsutil.hxx>
#include "debug.h"
//
// Debug label properties
//
#define DEBUG_LABEL_TEXT   _T("BKLDETECTOR: ")
#define DEBUG_LABEL_INDENT 11
//
// Find driver by device name.
//.
#define DEVICENAME_BKL L"BKL*"
//
// Find driver by IClass GUID.
//
#define GUIDSTRINGS 2
#define GUIDSTRING_BKL0 _T("F922DDE3-6A6D-4775-B23C-6842DB4BF094")
#define GUIDSTRING_BKL1 _T("0007AE3D-413C-4e7e-8786-E2A696E73A6E")
//
// Test ID base number.
//
#define BKL_DETECTOR_BASE 1000
//
// Function prototypes.
//
TESTPROCAPI DetectBKLDriver( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY, INT );
TESTPROCAPI SingleBKLDetectorTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
TESTPROCAPI MultiBKLDetectorTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
//
// TUX Function table
//
extern "C" {
    FUNCTION_TABLE_ENTRY g_lpFTE[] = {
		{ TEXT("Backlight Driver Detection Test"),           1, 0, BKL_DETECTOR_BASE+1, SingleBKLDetectorTest },
		{ TEXT("Multiple Backlight Driver Detection Test"),  1, 0, BKL_DETECTOR_BASE+2, MultiBKLDetectorTest },
		{NULL, 0, 0, 0, NULL}
    };
}
