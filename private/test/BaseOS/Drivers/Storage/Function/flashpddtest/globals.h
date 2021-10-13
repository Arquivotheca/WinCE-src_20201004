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
#ifndef __GLOBALS_H__
#define __GLOBALS_H__

// --------------------------------------------------------------------
// INIT Macro
// --------------------------------------------------------------------
#ifdef __GLOBALS_CPP__
#define GLOBAL
#define INIT(x) = x
#else // __GLOBALS_CPP__
#define GLOBAL  extern
#define INIT(x)
#endif // __GLOBALS_CPP__

// --------------------------------------------------------------------
// Module Name
// --------------------------------------------------------------------
#define MODULE_NAME _T("flashpddtest")

// --------------------------------------------------------------------
// Global macros and definitions
// --------------------------------------------------------------------
#define Debug NKDbgPrintfW
#define VALID_POINTER(X)        (X != NULL && 0xcccccccc != (int) X)
#define INVALID_POINTER(X)       (X == NULL || 0xcccccccc == (int) X)
#define CHECK_DELETE(X) if (VALID_POINTER(X)) { delete (X); X = NULL; }
#define CHECK_CLOSE_HANDLE(X) if (VALID_HANDLE(X)) { CloseHandle(X); X = INVALID_HANDLE_VALUE; }
#define CHECK_FREE(X) if (VALID_POINTER(X)) { free(X); X = NULL; }
#define CHECK_FREE_LIBRARY(X) if (VALID_POINTER(X)) { FreeLibrary(X); X = NULL; }
#define CHECK_CLOSE_KEY(X) if (VALID_POINTER(X)) { RegCloseKey(X); X = NULL; }
#define CHECK_LOCAL_FREE(X) if (VALID_POINTER(X)) { LocalFree(X); X = NULL; }
#define CHECK_STOP_DEVICE_NOTIFICATIONS(X) if (VALID_HANDLE(X)) { StopDeviceNotifications(X); X = INVALID_HANDLE_VALUE; }
#define CHECK_DEACTIVATE_DEVICE(X) if ((0 != X) && (INVALID_HANDLE_VALUE != X)) { DeactivateDevice(X); }
#define MAX_BLOCK_RUN_SIZE 16
#define MAX_TRANSFER_REQUESTS 4

// --------------------------------------------------------------------
// Command line flags
// --------------------------------------------------------------------
#define FLAG_STORAGE_PROFILE       _T("p")
#define FLAG_STORAGE_PATH          _T("r")
#define FLAG_DEVICE_NAME           _T("d")
#define FLAG_PDD_DLL_NAME          _T("dll")
#define FLAG_PDD_REG_KEY           _T("regkey")
#define FLAG_USE_OPEN_STORE        _T("store")
#define FLAG_TEST_CONSENT          _T("zorch")
#define FLAG_TEST_READONLY		   _T("readonly")

// --------------------------------------------------------------------
// Tux function prototypes
// --------------------------------------------------------------------
void            Debug(LPCTSTR, ...);
void Shutdown();
BOOL Initialize();
SHELLPROCAPI    ShellProc(UINT, SPPARAM);
BOOL TestConsent(DWORD dwTestId);

// --------------------------------------------------------------------
// Pull in the function table
// --------------------------------------------------------------------
#include "ft.h"

// --------------------------------------------------------------------
// Global Constants
// --------------------------------------------------------------------
#define READONLY_TEST_COUNT 7
extern const DWORD gc_dwReadOnlyTestIds[READONLY_TEST_COUNT];



// --------------------------------------------------------------------
// Global Variables
// --------------------------------------------------------------------
GLOBAL CKato            *g_pKato INIT(NULL);
GLOBAL SPS_SHELL_INFO   *g_pShellInfo;
GLOBAL TCHAR            g_szDeviceProfile[PROFILENAMESIZE] INIT(_T(""));
GLOBAL TCHAR            g_szPath[MAX_PATH] INIT(_T(""));
GLOBAL TCHAR            g_szDeviceName[MAX_PATH] INIT(_T(""));
GLOBAL TCHAR            g_szPDDLibraryName[MAX_PATH] INIT(_T(""));
GLOBAL TCHAR            g_szPDDRegistryKey[MAX_PATH] INIT(_T(""));
GLOBAL BOOL             g_fProfileSpecified INIT(FALSE);
GLOBAL BOOL             g_fPathSpecified INIT(FALSE);
GLOBAL BOOL             g_fDeviceNameSpecified INIT(FALSE);
GLOBAL BOOL             g_fUseOpenStore INIT(FALSE);
GLOBAL BOOL             g_fPDDLibrarySpecified INIT(FALSE);
GLOBAL BOOL             g_fVolumeInitPerformed INIT(FALSE);
GLOBAL BOOL             g_fZorch INIT(FALSE);
GLOBAL BOOL				g_fReadOnly INIT(FALSE);
GLOBAL HANDLE           g_hDevice INIT(INVALID_HANDLE_VALUE);
GLOBAL HANDLE           g_hActivateHandle INIT(INVALID_HANDLE_VALUE);

// --------------------------------------------------------------------
// Globals for keeping track of flash geometry
// --------------------------------------------------------------------
GLOBAL  DWORD                   g_dwFlashRegionCount INIT(0);
GLOBAL  FLASH_REGION_INFO       *g_pFlashRegionInfoTable INIT(NULL);

#endif // __GLOBALS_H__