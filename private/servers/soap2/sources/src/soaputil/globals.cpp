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
//+---------------------------------------------------------------------------------
//
//
// File:
//      Globals.cpp
//
// Contents:
//
//      Common global variable definitions
//
//----------------------------------------------------------------------------------

#include "Headers.h"

HINSTANCE               g_hInstance          = 0;
#if !defined(UNDER_CE) || defined(DESKTOP_BUILD)
HINSTANCE               g_hInstance_language = 0;
#endif 
IGlobalInterfaceTable  *g_pGIT               = 0;

LONG                    g_cLock              = 0;
LONG                    g_cObjects           = 0;
IMalloc                *g_pIMalloc           = 0;

CErrorCollection        g_cErrorCollection;
