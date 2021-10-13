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
////////////////////////////////////////////////////////////////////////////////
//
//  Test Application for large file (64-bit pointer inside filesystem stack)
//
//  Module: fsLargeFiles.cpp
//          Contains some code implementations
//
//  Description:
//      We need to add support for the 64-bit file size,
//      to support large files over 4 GB.
//
//  Note:
//      You will need a big-enough hard disk to run this test suite.
//      The following API will be covered:
//          -GetFileSize
//          -SetFilePointer
//          -FindFirstFile
//          -FindNextFile
//          -GetFileInformationByHandle
//          -ReadFile
//          -WriteFile
//          -ReadFileWithSeek
//          -WriteFileWithSeek
//          -CopyFile
//
//  Instruction:
//      -Mount a Hard Disk onto the system (> 4GB)
//      -Format it
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"

// Some function prototypes
BOOL Test_CreateLargeFile();
BOOL Test_SetFilePointer();
BOOL Test_CopyLargeFile();
BOOL Test_SetFilePointer2GB();
BOOL Test_GetFileInformationByHandle();
BOOL Test_FindFiles();
BOOL Test_ReadWriteFileWithSeek();
BOOL Test_CreateVeryLargeFile();

////////////////////////////////////////////////////////////////////////////////
// Tst_RunTest
//  Executes one test.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Tst_RunTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    switch(lpFTE->dwUserData)
    {
        case TEST_CREATE_LARGE_FILE:
            if(!Test_CreateLargeFile())
                FAIL("Test_CreateLargeFile()");

            break;

        case TEST_SET_FILE_POINTER:
            if(!Test_SetFilePointer())
                FAIL("Test_SetFilePointer()");

            break;

        case TEST_COPY_LARGE_FILE:
            if(!Test_CopyLargeFile())
                FAIL("Test_CopyLargeFile()");

            break;

        case TEST_FILE_INFO:
            if(!Test_GetFileInformationByHandle())
                FAIL("Test_GetFileInformationByHandle()");

            break;

        case TEST_FIND_FILES:
            if(!Test_FindFiles())
                FAIL("Test_FindFiles()");

            break;

        case TEST_RWFILE_WITH_SEEK:
            if(!Test_ReadWriteFileWithSeek())
                FAIL("Test_ReadWriteFileWithSeek()");

            break;

        case TEST_CREATE_VERYLARGE_FILE:
            if(!Test_CreateVeryLargeFile())
                FAIL("Test_CreateVeryLargeFile()");

            break;


        case TEST_FILE_POINTER_AT_2GB:
            if(!Test_SetFilePointer2GB())
                FAIL("Test_Bug139189()");

            break;


        default:
            FAIL("Unexpected test ID");
            break;
    }

    return GetTestResult();
}
