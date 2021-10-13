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



