//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
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
