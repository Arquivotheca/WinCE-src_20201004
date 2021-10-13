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
#include <nled.h>
//
// Debug label properties
//
#define DEBUG_LABEL_TEXT   _T("NLEDDETECTOR: ")
#define DEBUG_LABEL_INDENT 11
//
// Find driver by IClass GUID.
//
#define NLED_GUID_FORMAT   L"{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}"
//?#define GUIDSTRING_NLED   _T("CBB4F234-F35F-485B-A490-ADC7804A4EF3")
// #define NLED_DRIVER_CLASS_T("{CBB4F234-F35F-485b-A490-ADC7804A4EF3}")
// Test ID base number.
//
#define NLED_DETECTOR_BASE 1000
//
// Function prototypes.
//
TESTPROCAPI DetectNLEDDriver( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
//
// TUX Function table
//
extern "C" {
    FUNCTION_TABLE_ENTRY g_lpFTE[] = {
        { _T("NLED Driver Detection Test"),      1, 0, NLED_DETECTOR_BASE+1, DetectNLEDDriver },
        {NULL, 0, 0, 0, NULL}
    };
}
