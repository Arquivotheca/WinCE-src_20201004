//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//=--------------------------------------------------------------------------=
// Globals.C
//=--------------------------------------------------------------------------=
//
// contains global variables and strings and the like that just don't fit
// anywhere else.
//
#include "IPServer.H"

//=--------------------------------------------------------------------------=
// support for licensing
//
BOOL g_fMachineHasLicense;
BOOL g_fCheckedForLicense;

//=--------------------------------------------------------------------------=
// does our server have a type library?
//
BOOL g_fServerHasTypeLibrary = TRUE;

//=--------------------------------------------------------------------------=
// our instance handles
//
HINSTANCE    g_hInstance;
HINSTANCE    g_hInstResources;
VARIANT_BOOL g_fHaveLocale;

//=--------------------------------------------------------------------------=
// our global memory allocator and global memory pool
//
HANDLE   g_hHeap;

//=--------------------------------------------------------------------------=
// apartment threading support.
//
CRITICAL_SECTION    g_CriticalSection;

//=--------------------------------------------------------------------------=
// global parking window for parenting various things.
//
HWND     g_hwndParking;



