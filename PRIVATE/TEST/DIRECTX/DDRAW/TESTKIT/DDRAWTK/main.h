//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//

#ifndef __BVT_MAIN_H__
#define __BVT_MAIN_H__

#include <windows.h>

#include <stdlib.h>
#include <platform_config.h>
#include <tchar.h>

// This is for NT
#ifndef ASSERT
    #include "assert.h" 
    #define ASSERT assert
#endif

#include <ddraw.h>
#include <hqalog.h>
#include <QATestUty/DebugUty.h>
#include <DDrawUty.h>

using DDrawUty::g_ErrorMap;

#endif
