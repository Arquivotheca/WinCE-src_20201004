//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
#ifndef __MAIN_H__
#define __MAIN_H__

#include "util.h"
#include "attributetest.h"
#include "writetest.h"
#include "iotest.h"
#include "memmaptest.h"

//
// Test function prototypes (TestProc's)
//
TESTPROCAPI TestDefault         (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

//
// BASE is a unique value assigned to a given tester or component.  This value,
// when combined with each of the following test's unique IDs, allows every 
// test case within the entire team to be uniquely identified.
//
#define BASE 5000

//
// Our function table that we pass to Tux
//
static FUNCTION_TABLE_ENTRY g_lpFTE[] = {

    //
    // These tests are run to verify that file attributes are working
    //
    TEXT("File Attribute API Tests"         ),  0,  0,                  0,          NULL,
    TEXT(   "Attributes"                    ),  1,  ATTRIBUTES,         BASE +  0,  FSAttributesTest,
    TEXT(   "Attributes Ex"                 ),  1,  ATTRIBUTES_EX,      BASE +  1,  FSAttributesTest,   
    TEXT(   "Free Disk Space"               ),  1,  DISK_FREE_SPACE,    BASE +  2,  FSAttributesTest,
    TEXT(   "Information By Handle"         ),  1,  INFO_BY_HANDLE,     BASE +  3,  FSAttributesTest,
    TEXT(   "File Size"                     ),  1,  FILE_SIZE,          BASE +  4,  FSAttributesTest,
    TEXT(   "File Time"                     ),  1,  FILE_TIME,          BASE +  5,  FSAttributesTest,

    //
    // These tests are run to check the validity of error messages produced when attempts at
    // modifying the file system are made
    //
    TEXT("Write Protection API Tests"       ),  0,  0,                  0,          NULL,
    TEXT(   "Remove Directory"              ),  1,  REMOVE_DIRECTORY,   BASE + 10,  FSWriteTest,
    TEXT(   "Create Directory"              ),  1,  CREATE_DIRECTORY,   BASE + 11,  FSWriteTest,
    TEXT(   "Create New File"               ),  1,  CREATE_NEW_FILE,    BASE + 12,  FSWriteTest,
    TEXT(   "Create File For Write"         ),  1,  CREATE_FILE_WRITE,  BASE + 13,  FSWriteTest,
    TEXT(   "Delete File"                   ),  1,  DELETE_FILE,        BASE + 14,  FSWriteTest,
    TEXT(   "Delete and Rename File"        ),  1,  DELETE_RENAME_FILE, BASE + 15,  FSWriteTest,
    TEXT(   "Move File"                     ),  1,  MOVE_FILE,          BASE + 16,  FSWriteTest,
    TEXT(   "Set File Attributes"           ),  1,  SET_FILE_ATTRIBUTES,BASE + 17,  FSWriteTest,
    TEXT(   "Set End of File"               ),  1,  SET_END_OF_FILE,    BASE + 18,  FSWriteTest,
    TEXT(   "Write File"                    ),  1,  WRITE_FILE,         BASE + 19,  FSWriteTest,
    TEXT(   "Flush File Buffers"            ),  1,  FLUSH_FILE_BUFFERS, BASE + 20,  FSWriteTest,
    TEXT(   "Set File Time"                 ),  1,  SET_FILE_TIME,      BASE + 21,  FSWriteTest,
    
    //
    // These tests check the input ability of the file system through memory mapping and 
    // direct I/O
    //
    TEXT("Basic I/O API Tests"              ),  0,  0,                  0,          NULL,
    TEXT(   "Read 1K Buffer"                ),  1,  1024,               BASE + 30,  FSIoTest,
    TEXT(   "Read 2K Buffer"                ),  1,  2048,               BASE + 31,  FSIoTest,
    TEXT(   "Read 2040B Buffer"             ),  1,  2040,               BASE + 32,  FSIoTest,   // just under cd sector size
    TEXT(   "Read 2050B Buffer"             ),  1,  2050,               BASE + 33,  FSIoTest,   // just over cd sector size
    TEXT(   "Read 64K Buffer"               ),  1,  64*1024,            BASE + 34,  FSIoTest,   // FS_COPYBLOCK_SIZE used by CopyFile
    TEXT(   "Memory Mapped I/O Test"        ),  1,  0,                  BASE + 35,  FSMappedIoTest, 
    
    NULL,                                       0,  0,                  0,          NULL // end of the list
};

#endif // __TUXDEMO_H__
