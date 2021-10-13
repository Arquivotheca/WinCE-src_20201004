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
#ifndef __WRITETEST_H__
#define __WRITETEST_H__

#include "globals.h"
#include "util.h"
#include <findfile.h>

//
// WriteTest constants
//
#define WRITE_SIZE 100

//
// WriteTest test variations
//
enum WRITE_TEST 
{
    REMOVE_DIRECTORY = 0,
    CREATE_DIRECTORY,
    CREATE_NEW_FILE,
    CREATE_FILE_WRITE,
    DELETE_FILE,
    DELETE_RENAME_FILE,
    MOVE_FILE,
    SET_FILE_ATTRIBUTES,
    SET_END_OF_FILE,
    WRITE_FILE,
    FLUSH_FILE_BUFFERS,
    SET_FILE_TIME
};

// 
// Tux functions exported for the Write Test
//
TESTPROCAPI FSWriteTest( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );


#endif

