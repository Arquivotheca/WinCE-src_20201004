//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#pragma once

#include "shcpriv.h"

// Path prototypes
#define _SHLWAPI_
#include <shlwapi.h>
#include <shcore.h>

STDAPI_(BOOL) PathMakeUniqueNameEx(
    LPCTSTR lpszPath,
    LPCTSTR lpszT1,
    LPCTSTR lpszT2,
    BOOL bLink,
    BOOL bSourceIsDirectory,
    LPTSTR lpszUniqueName
    );