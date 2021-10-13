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
#ifndef __ATTRIBUTETEST_H__
#define __ATTRIBUTETEST_H__

#include "globals.h"
#include "util.h"
#include <pathtable.h>
#include <findfile.h>

//
// AttributesTest constants
//
const DWORD GET_FILE_ATTRIBUTE_ERROR    = 0xFFFFFFFF;

//
// AttributesTest test variations
//
enum ATTRIBUTES_TEST 
{
    ATTRIBUTES = 0,
    ATTRIBUTES_EX,
    DISK_FREE_SPACE,
    INFO_BY_HANDLE,
    FILE_SIZE,
    FILE_TIME
};

// 
// Tux functions exported for the Attributes Test
//
TESTPROCAPI FSAttributesTest( UINT, TPPARAM,    LPFUNCTION_TABLE_ENTRY );


#endif
