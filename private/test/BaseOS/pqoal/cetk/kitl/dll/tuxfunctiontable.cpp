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
 * pull in function defs for the Kitl tests
 */

#include "..\code\tuxKitl.h"


/***** Base to start the tests at *******/
#define KITL_BASE 1000


FUNCTION_TABLE_ENTRY g_lpFTE[] = {

  _T("Kitl Tests Usage Message"), 0, 0, 1, KitlTestsUsageMessage,

  _T("Tests for Kitl"), 0, 0, 0, NULL,

  _T("Stress Kitl - Load\Unload dll"), 0, KITL_LOAD_UNLOAD_DLL_TEST, KITL_BASE, testKitlStress,
  _T("Stress Kitl - Dump debug spew"), 0, KITL_DUMP_DEBUG_OUTPUT_TEST, KITL_BASE + 1000, testKitlStress,

   NULL, 0, 0, 0, NULL  // marks end of list
};


