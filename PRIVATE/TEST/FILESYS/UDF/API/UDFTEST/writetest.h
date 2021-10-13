//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

