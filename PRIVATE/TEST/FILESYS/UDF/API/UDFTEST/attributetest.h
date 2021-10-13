//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
