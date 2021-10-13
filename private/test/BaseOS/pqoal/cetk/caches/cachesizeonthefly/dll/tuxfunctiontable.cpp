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
/*
 * tuxFunctionTable.cpp
 */

/*
 * pull in the function defs for the cache tests
 */
#include "..\code\tuxCacheSizeCalculation.h"
#include "..\code\cacheDefs.h"

#define UNIQUE_ID 1000

FUNCTION_TABLE_ENTRY g_lpFTE[] = 
  { 
    _T("Print Test Usage"), 2, 0, UNIQUE_ID + 1, entryCacheSizePrintUsage,
    _T("CacheSizeCalculation"), 2, 0, UNIQUE_ID + 8, entryCacheSizeCalculation,
    NULL, 0, 0, 0, NULL, // marks end of list
};