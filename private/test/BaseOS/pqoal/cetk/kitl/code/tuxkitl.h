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
 * tuxKitl.h
 */

#ifndef __TUX_KITL_H__
#define __TUX_KITL_H__

#include <windows.h>
#include <tux.h>


/*
 * This file includes TESTPROCs for testing the Kitl. Anything 
 * needed by the tux function table must be included here. This file 
 * is included by the file with the tux function table.
 *
 * Please keep non-tux related stuff (stuff local to Kitl) out of 
 * this file to keep from polluting the name space.
 */

// defines for the KITL tests
#define KITL_LOAD_UNLOAD_DLL_TEST 1
#define KITL_DUMP_DEBUG_OUTPUT_TEST 2


/**************************************************************************
 * 
 *                           Kitl Test Functions
 *
 **************************************************************************/
 
/****************************** Usage Message *****************************/
TESTPROCAPI KitlTestsUsageMessage(
            UINT uMsg,
            TPPARAM tpParam,
            LPFUNCTION_TABLE_ENTRY lpFTE);


/*************************** Kitl Stress Test ****************************/
TESTPROCAPI testKitlStress(
              UINT uMsg, 
              TPPARAM tpParam, 
              LPFUNCTION_TABLE_ENTRY lpFTE);


#endif // __TUX_KITL_H__
