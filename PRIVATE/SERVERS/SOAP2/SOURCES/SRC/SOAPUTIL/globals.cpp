//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
