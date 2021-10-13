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
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2002 BSQUARE Corporation. All rights reserved.
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
// --------------------------------------------------------------------
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// --------------------------------------------------------------------
//
//
// Global.h
//
//
#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#include <tchar.h>
#include <tux.h>
#include <kato.h>
#include "sd_tst_cmn.h"

////////////////////////////////////////////////////////////////////////////////
// Local macros

#ifdef __GLOBALS_CPP__
#define GLOBAL
#define INIT(x) = x
#else // __GLOBALS_CPP__
#define GLOBAL  extern
#define INIT(x)
#endif // __GLOBALS_CPP__

////////////////////////////////////////////////////////////////////////////////
// Global function prototypes

void Debug(LPCTSTR, ...);
SHELLPROCAPI ShellProc(UINT, SPPARAM);
BOOL IsWrongType(int iStat);

////////////////////////////////////////////////////////////////////////////////
// TUX Function table

#include "ft.h"

////////////////////////////////////////////////////////////////////////////////
// Globals

// Global CKato logging object. Set while processing SPM_LOAD_DLL message.
GLOBAL CKato *g_pKato;// INIT(NULL);

// Global shell info structure. Set while processing SPM_SHELL_INFO message.
GLOBAL SPS_SHELL_INFO *g_pShellInfo;

// Add more globals of your own here. There are two macros available for this:
//  GLOBAL  Precede each declaration/definition with this macro.
//  INIT    Use this macro to initialize globals, instead of typing "= ..."
//
// For example, to declare two DWORDs, one uninitialized and the other
// initialized to 0x80000000, you could enter the following code:
//
//  GLOBAL DWORD        g_dwUninit,
//                      g_dwInit INIT(0x80000000);
////////////////////////////////////////////////////////////////////////////////
GLOBAL HINSTANCE g_hInst;
GLOBAL BOOL g_bMsgBox;
GLOBAL BOOL g_bMemDrvExists;
GLOBAL BOOL g_bClientDrvExists;
GLOBAL BOOL g_bClassExists;
GLOBAL TCHAR g_szMemDrvName[256];
GLOBAL TCHAR g_szMemPrefix[256];
GLOBAL DWORD g_dwMemIndex;
GLOBAL HANDLE g_hTstDriver;
GLOBAL BOOL g_UsingCombo;
GLOBAL BOOL g_ManualInsertEject;
GLOBAL BOOL g_UseSoftBlock;
GLOBAL TCHAR g_szDiskName[MAX_PATH];
GLOBAL SDTST_DEVICE_TYPE g_deviceType;
GLOBAL DWORD g_sdHostIndex;
GLOBAL DWORD g_sdSlotIndex;
GLOBAL HANDLE g_DriverLoadedEvent;
#endif // __GLOBALS_H__

