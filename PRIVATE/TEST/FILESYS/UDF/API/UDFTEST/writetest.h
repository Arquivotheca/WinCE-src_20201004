//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
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

