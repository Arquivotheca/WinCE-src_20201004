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
//

/*
 * tuxFunctionTable.cpp
 */

/*
 * pull in the function defs for the cache tests
 */
#include "tuxCacheChurn.h"

FUNCTION_TABLE_ENTRY g_lpFTE[] = {

  _T("Usage"), 0, 0, 1, PrintUsage,

  _T(""), 0, 0, 0, NULL,

  _T("CacheSize"), 0, 1, 1000, testCacheChurn,
  _T("CacheSize * 2"), 0, 2, 2000, testCacheChurn,
  _T("CacheSize * 4"), 0, 4, 3000, testCacheChurn,
  _T("CacheSize * 16 (or -maxWorkingSet)"), 0, 16, 4000, testCacheChurn, 
   NULL, 0, 0, 0, NULL  // marks end of list
};


