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
#define DEBUG_LABEL_TEXT   _T("USBDETECTOR: ")
#define DEBUG_LABEL_INDENT 11
//
// Find driver by device name.
//
#define DEVICENAME_HOST _T("HCD*")
#define DEVICENAME_FN   _T("UFN*")
#define DEVICENAME_OTG  _T("OTG*")
//
// Find driver by IClass GUID.
//
#define GUIDSTRING_FN   _T("E2BDC372-598F-4619-BC50-54B3F7848D35")
#define GUIDSTRING_HOST _T("7D96B50A-6BB5-4f64-ACD2-A0C3A45375FA")
//
// Test ID base number.
//
#define USB_DETECTOR_BASE 1000
//
// USB driver enum type.
//
enum USB_FLAVOR {
	HOST = 0,		// USB host
	FUNCTION = 1,	// USB function
	OTG = 2,		// USB on-the-go
	WTH = 3			// USB what-the-heck
};
//
// Function prototypes.
//
TESTPROCAPI DetectUSBDriver( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY, USB_FLAVOR );
TESTPROCAPI USBHostDetectorTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
TESTPROCAPI USBFnDetectorTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
TESTPROCAPI USBotgDetectorTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
//
// TUX Function table
//
extern "C" {
    FUNCTION_TABLE_ENTRY g_lpFTE[] = {
		{ _T("USB Host Driver Detection Test"),      1, 0, USB_DETECTOR_BASE+1, USBHostDetectorTest },
		{ _T("USB Function Driver Detection Test"),  1, 0, USB_DETECTOR_BASE+2, USBFnDetectorTest },
		{ _T("USB On-the-go Driver Detection Test"), 1, 0, USB_DETECTOR_BASE+3, USBotgDetectorTest },
		{NULL, 0, 0, 0, NULL}
    };
}
