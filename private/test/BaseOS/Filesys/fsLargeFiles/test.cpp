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
//  Test cases for large files (64-bit size)
//  Copyright (c) Microsoft Corporation
//
//  Module: test.cpp
//          Contains the test case implementations
//
//    Note: Refer to the comments on the begining of fsLargeFiles.cpp for
//          instructions.
//
////////////////////////////////////////////////////////////////////////////////

#include <string.h>
#include "main.h"
#include "globals.h"


#define BUFFER_SIZE 512*1024 // Let's use a 512k buffer
LPBYTE g_pBuf[BUFFER_SIZE];  // scrap buffer for writes

#define NUM_WRITES_4G 8192 // 4GB
#define NUM_WRITES_2G 4096 // 2GB

#define WRITE_SIZE 0x80000

WCHAR szLargeFile1[] = TEXT("\\LargeF1.foo");
WCHAR szLargeFile2[] = TEXT("\\LargeF2.foo");
WCHAR szLargeFile3[] = TEXT("\\LargeF3.foo");

////////////////////////////////////////////////////////////////////////////////
// Test_CreateLargeFile
//  Test creating large files
//        Requires a big-enough harddrive. Mount it as "Hard Disk" on a CePC and
//        format it as FAT.
//
//        The following APIs are covered in this test case:
//            -GetFileSize
//            -SetFilePointer
//            -WriteFile
//            -SetEndOfFile
//
//        Test scenarios:
//            1. Create a file slightly less than 4 GB
//            2. Verify the size
//            3. Add bytes to that file to be exactly 4 GB
//            4. Verify the size
//            5. Try to shrink the size by 1 bit to make the size 4GB-1
//            6. Check the size, should be 0xFFFFFFFF, but GetLastError should
//               return NO_ERROR
//            7. SetFilePointer ahead from 4GB-1 address by 256k and verify
//               the size
//            8. Move the pointer back by 256k-1 to make it back to 4GB (exact)
//               and verify the size
//            9. Add bytes to make it a 6 GB file
//            10. Verify the size
//
// Parameters:
//
// Return value:
//  TRUE if successful, FALSE to indicate an error condition.

BOOL Test_CreateLargeFile()
{
    WIN32_FIND_DATA fd;
    HANDLE           hFind;
    HANDLE           hFile;
    WCHAR            szBuf[MAX_PATH];
    DWORD            i, j, dwBytes;
    DWORD            dwPos;      // Current position in the file
    DWORD            dwOffset;   // Offset to add to current position in the file
    DWORD            dwFileSize;
    DWORD            dwFileSizeHigh;

    LOG(TEXT("Test_CreateLargeFile: Start testing"));

    // First, check if we have Hard Disk mounted on the system
    SetLastError(0);
    hFind = FindFirstFile(g_szHardDiskRoot, &fd);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        ERRFAIL("FindFirstFile");
        LOG(TEXT("!! %s does not exist!"), g_szHardDiskRoot);
        return FALSE;
    }
    if (!FindClose(hFind))
    {
        ERRFAIL("FindClose");
        return FALSE;
    }

    // Clean up existing file, if any
    StringCchCopy(szBuf, _countof(szBuf), g_szHardDiskRoot);
    StringCchCat(szBuf, _countof(szBuf), szLargeFile1);
    DeleteFile(szBuf);

    // Scenario 1: Create a file slightly < 4GB
    LOG(TEXT("Create file %s. Please wait, this might take a couple of minutes..."), szBuf);
    hFile = CreateFile(szBuf, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        ERRFAIL("CreateFile");
        return FALSE;
    }

    // Fill the buffer with the current address to make the data recognizable
    for (j = 0; j < WRITE_SIZE/32; j++)
    {
        ((DWORD*)(g_pBuf))[j] = j;
    }

    dwOffset = BUFFER_SIZE;
    dwPos = 0;

    // This will write file with size 4GB - 8*BUFFER_SIZE = 0xFFC00000 (~3.996 GB)
    for (i = 0; i < (NUM_WRITES_4G/8)-1; i++)
    {
        // We're going to advance 4MB at a time: 512k data + 7*512k holes.

        // Write 512k data from the buffer
        if (!WriteFile(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL)
            || (dwBytes != WRITE_SIZE))
        {
            ERRFAIL("WriteFile");
            VERIFY(CloseHandle(hFile));
            return FALSE;
        }
        // Put 7*512k holes here to save time on file creation
        if (SetFilePointer(hFile, 7*dwOffset, NULL, FILE_CURRENT) == 0xFFFFFFFF)
        {
            if (GetLastError() == NO_ERROR)
                LOG(TEXT("SetFilePointer returns 0xFFFFFFFF, but NO_ERROR"));
            else
                ERRFAIL("SetFilePointer");

            return FALSE;
        }
    }

    SetEndOfFile(hFile);

    // Scenario 2: Verify the size, should be < 4GB, thus lpFileSizeHigh can be NULL
    dwFileSize = GetFileSize(hFile, NULL);
    LOG(TEXT("%s size=0x%08x"), szBuf, dwFileSize);

    // Size should be 4GB - 8*BUFFER_SIZE
    if (dwFileSize == 0xFFC00000)
    {
        LOG(TEXT("Size=0x%08x matched."), dwFileSize);
    }
    else
    {
        DWORD dwErr = GetLastError();

        if (dwErr == NO_ERROR)
            LOG(TEXT("GetFileSize: NO_ERROR - we shouldn't be here (size didn't match)!!"));
        else
            LOG(TEXT("GetFileSize: Size didn't match!! Error=%u"), dwErr);

        FAIL("GetFileSize");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    VERIFY(CloseHandle(hFile));

    // Scenario 3: Add bytes to that file to be exactly 4 GB
    // First, open the existing file
    hFile = CreateFile(szBuf, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        ERRFAIL("CreateFile");
        return FALSE;
    }

    // Go to the end of the file, add 8*BUFFER_SIZE of data+holes
    if (SetFilePointer(hFile, 0, NULL, FILE_END) == 0xFFFFFFFF)
    {
        if (GetLastError() == NO_ERROR)
            LOG(TEXT("SetFilePointer returns 0xFFFFFFFF, but NO_ERROR"));
        else
            ERRFAIL("SetFilePointer");

        return FALSE;
    }
    // Write 512k data from the buffer
    if (!WriteFile(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL)
        || (dwBytes != WRITE_SIZE))
    {
        ERRFAIL("WriteFile");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }
    // Put 7*512k holes here
    if (SetFilePointer(hFile, 7*dwOffset, NULL, FILE_CURRENT) == 0xFFFFFFFF)
    {
        if (GetLastError() == NO_ERROR)
            LOG(TEXT("SetFilePointer returns 0xFFFFFFFF, but NO_ERROR"));
        else
            ERRFAIL("SetFilePointer");

        return FALSE;
    }

    SetEndOfFile(hFile);

    // Scenario 4: Check the file size. This should be exactly 4GB
    dwFileSize = GetFileSize(hFile, &dwFileSizeHigh);
    LOG(TEXT("%s FileSize=0x%08x FileSizeHigh=0x%08x"), szBuf, dwFileSize, dwFileSizeHigh);

    // Size should be exactly 4GB
    if (dwFileSize == 0x00000000 && dwFileSizeHigh == 0x00000001)
    {
        LOG(TEXT("Size 4GB matched."));
    }
    else
    {
        DWORD dwErr = GetLastError();

        if (dwErr == NO_ERROR)
            LOG(TEXT("GetFileSize: NO_ERROR - we shouldn't be here (size didn't match)!!"));
        else
            LOG(TEXT("GetFileSize: Size didn't match!! Error=%u"), dwErr);

        FAIL("GetFileSize");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    // Scenario 5: Try to shrink the size by 1 bit to make the size 4GB-1
    if (SetFilePointer(hFile, -1, NULL, FILE_CURRENT) == 0xFFFFFFFF)
    {
        if (GetLastError() == NO_ERROR)
        {
            LOG(TEXT("SetFilePointer returns 0xFFFFFFFF, but NO_ERROR (expected)"));
        }
        else
        {
            ERRFAIL("SetFilePointer");
            VERIFY(CloseHandle(hFile));
            return FALSE;
        }
    }

    SetEndOfFile(hFile);

    // Scenario 6: Check the size, should be 0xFFFFFFFF, but GetLastError should return NO_ERROR
    dwFileSize = GetFileSize(hFile, &dwFileSizeHigh);
    LOG(TEXT("%s FileSize=0x%08x FileSizeHigh=0x%08x"), szBuf, dwFileSize, dwFileSizeHigh);

    if (dwFileSize == 0xFFFFFFFF)
    {
        DWORD dwErr =GetLastError();
        if (dwErr== NO_ERROR)
            LOG(TEXT("GetFileSize: NO_ERROR (expected)"));
        else
        {
            LOG(TEXT("GetFileSize: Size didn't match!! Error=%u"), dwErr);
            FAIL("GetFileSize");
            VERIFY(CloseHandle(hFile));
            return FALSE;
        }
    }
    else
    {
        ERRFAIL("GetFileSize: Size didn't match");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    VERIFY(CloseHandle(hFile));

    // Scenario 7: SetFilePointer ahead from 4GB-1 address to some bytes ahead and verify the size
    // First, open the existing file
    hFile = CreateFile(szBuf, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        ERRFAIL("CreateFile");
        return FALSE;
    }

    // Set offset to be 256k
    dwOffset = 256*1024;

    if (SetFilePointer(hFile, dwOffset, NULL, FILE_END) == 0xFFFFFFFF)
    {
        if (GetLastError() == NO_ERROR)
        {
            LOG(TEXT("SetFilePointer returns 0xFFFFFFFF, but NO_ERROR (expected)"));
        }
        else
        {
            ERRFAIL("SetFilePointer");
            VERIFY(CloseHandle(hFile));
            return FALSE;
        }
    }

    SetEndOfFile(hFile);

    // Size should be 4GB-1 + 256k = 0x10003FFFF
    dwFileSize = GetFileSize(hFile, &dwFileSizeHigh);
    LOG(TEXT("%s FileSize=0x%08x FileSizeHigh=0x%08x"), szBuf, dwFileSize, dwFileSizeHigh);

    if (dwFileSize == 0x0003FFFF && dwFileSizeHigh == 0x00000001)
    {
        LOG(TEXT("Scenario 7: Size matched"));
    }
    else
    {
        ERRFAIL("GetFileSize: Size didn't match");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    // Scenario 8: Move the pointer back by 256k-1 to make it back to 4GB (exact)
    // First, open the existing file

    // Set offset to be 256k-1
    dwOffset = 256*1024-1;

    if (SetFilePointer(hFile, -1*dwOffset, NULL, FILE_END) == 0xFFFFFFFF)
    {
        if (GetLastError() == NO_ERROR)
            LOG(TEXT("SetFilePointer returns 0xFFFFFFFF, but NO_ERROR (unexpected)"));
        else
            ERRFAIL("SetFilePointer");

        VERIFY(CloseHandle(hFile));
        return FALSE;

    }

    SetEndOfFile(hFile);

    // Size should be 4GB exactly
    dwFileSize = GetFileSize(hFile, &dwFileSizeHigh);
    LOG(TEXT("%s FileSize=0x%08x FileSizeHigh=0x%08x"), szBuf, dwFileSize, dwFileSizeHigh);

    if (dwFileSize == 0x00000000 && dwFileSizeHigh == 0x00000001)
    {
        LOG(TEXT("Scenario 8: Size matched"));
    }
    else
    {
        ERRFAIL("GetFileSize: Size didn't match");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    VERIFY(CloseHandle(hFile));

    // Fill the buffer with a different pattern (index + WRITE_SIZE/32)
    for (j = 0; j < WRITE_SIZE/32; j++)
    {
        ((DWORD*)(g_pBuf))[j] = j + WRITE_SIZE/32;
    }

    // Scenario 9: Write more bytes to the file with this different pattern.
    // Fill it up to 6GB.
    hFile = CreateFile(szBuf, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        ERRFAIL("CreateFile");
        return FALSE;
    }

    dwOffset = BUFFER_SIZE;

    // Go to the end of the file
    if (SetFilePointer(hFile, 0, NULL, FILE_END) == 0xFFFFFFFF)
    {
        if (GetLastError() == NO_ERROR)
            LOG(TEXT("File Hole Test: Reached file size limit on SetFilePointer"));
        else
            ERRFAIL("SetFilePointer");

        return FALSE;
    }

    // This will write 2GB data/holes to the existing file
    LOG(TEXT("Writing another 2GB of data. Please wait, this might take a couple of minutes..."));
    for (i = 0; i < NUM_WRITES_2G/8; i++)
    {
        // Write 512k data from the buffer
        if (!WriteFile(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL)
            || (dwBytes != WRITE_SIZE))
        {
            ERRFAIL("WriteFile");
            VERIFY(CloseHandle(hFile));
            return FALSE;
        }
        // Put 7*512k holes here to save time on file creation
        if (SetFilePointer(hFile, 7*dwOffset, NULL, FILE_CURRENT) == 0xFFFFFFFF)
        {
            if (GetLastError() == NO_ERROR)
                LOG(TEXT("File Hole Test: Reached file size limit on SetFilePointer"));
            else
                ERRFAIL("SetFilePointer");

            return FALSE;
        }
    }

    SetEndOfFile(hFile);

    // Size should be 6GB exactly
    dwFileSize = GetFileSize(hFile, &dwFileSizeHigh);
    LOG(TEXT("%s FileSize=0x%08x FileSizeHigh=0x%08x"), szBuf, dwFileSize, dwFileSizeHigh);

    if (dwFileSize == 0x80000000 && dwFileSizeHigh == 0x00000001)
    {
        LOG(TEXT("Scenario 9: Size matched"));
    }
    else
    {
        ERRFAIL("GetFileSize: Size didn't match");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    VERIFY(CloseHandle(hFile));

    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////
// Test_SetFilePointer
//  Test setting the file pointer on a large file
//        Note: The file is created by the previous test case.
//
//        The following APIs are covered in this test case:
//            -SetFilePointer
//            -FindFirstFile
//            -FindClose
//            -ReadFile
//
//        Test scenarios:
//            1. Open the large file, set file pointer to 1GB and verify
//            2. Test 64-bit file size mechanism, set file pointer to 5GB,
//               3GB, 4GB and verify
//            3. Set file pointer from the end (backward) by 3GB
//
// Parameters:
//
// Return value:
//  TRUE if successful, FALSE to indicate an error condition.

BOOL Test_SetFilePointer()
{
    WIN32_FIND_DATA fd;
    HANDLE          hFind;
    HANDLE          hFile;
    WCHAR           szBuf[MAX_PATH];
    DWORD           i, dwBytes;
    DWORD           dwPos;          // Current position in the file
    DWORD           dwOffset;       // Offset to add to current position in the file
    LONG            lOffsetHigh;    // Offset high
    LONG            lRet=0;

    LOG(TEXT("Test_SetFilePointer: Start testing"));
    // First, check to make sure the large file (created on the previous test
    // case) exists.

    StringCchCopy(szBuf, _countof(szBuf), g_szHardDiskRoot);
    StringCchCat(szBuf, _countof(szBuf), szLargeFile1);

    SetLastError(0);
    hFind = FindFirstFile(szBuf, &fd);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        ERRFAIL("FindFirstFile");
        LOG(TEXT("!! %s does not exist! Please be sure to run test case 1001 first"), szBuf);
        return FALSE;
    }
    if (!FindClose(hFind))
    {
        ERRFAIL("FindClose");
        return FALSE;
    }

    // Scenario 1: Move the file pointer by 1GB and verify the data.
    hFile = CreateFile(szBuf, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        ERRFAIL("CreateFile");
        return FALSE;
    }

    LOG(TEXT("Try to move the pointer from the begining by 1GB"));
    dwOffset = 0x40000000;
    dwPos=SetFilePointer(hFile, dwOffset, NULL, FILE_BEGIN);
    if (dwPos== 0xFFFFFFFF)
    {
        if (GetLastError() == NO_ERROR)
            LOG(TEXT("SetFilePointer returns 0xFFFFFFFF, but NO_ERROR Pos=0x%08x"), dwPos);
        else
            ERRFAIL("SetFilePointer");

        return FALSE;
    }

    // Read some data to verify
    if (!ReadFile(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL)
        || (dwBytes != WRITE_SIZE))
    {
        ERRFAIL("ReadFile");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    // Verify the buffer
    for ( i = 0; i < WRITE_SIZE/32; i++)
    {
        if ( ((DWORD*)g_pBuf)[i] != i )
        {
            LOG(TEXT("g_pBuf[%u]=%u doesn't match!!"), i, ((DWORD*)g_pBuf)[i]);
            FAIL("Buffer's data was corrupted");
            VERIFY(CloseHandle(hFile));
            return FALSE;
        }
    }

    // If we made it this far, the buffer matches
    LOG(TEXT("Buffer at address 1GB matched"));

    VERIFY(CloseHandle(hFile));


    // Scenario 2: 64-bit values
    // Try to move the pointer by 5GB
    hFile = CreateFile(szBuf, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        ERRFAIL("CreateFile");
        return FALSE;
    }

    LOG(TEXT("Try to move the pointer from the begining by 5GB"));
    dwOffset = 0x40000000;
    lOffsetHigh = 0x00000001;
    dwPos=SetFilePointer(hFile, dwOffset, &lOffsetHigh, FILE_BEGIN);
    if ( dwPos == 0xFFFFFFFF)
    {
        if (GetLastError() == NO_ERROR)
            LOG(TEXT("SetFilePointer returns 0xFFFFFFFF, but NO_ERROR Pos=0x%08x"), dwPos);
        else
            ERRFAIL("SetFilePointer");

        return FALSE;
    }

    // Read some data to verify
    if (!ReadFile(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL)
        || (dwBytes != WRITE_SIZE))
    {
        ERRFAIL("ReadFile");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    // Verify the buffer
    for ( i = 0; i < WRITE_SIZE/32; i++)
    {
        if ( ((DWORD*)g_pBuf)[i] != i + WRITE_SIZE/32 )
        {
            LOG(TEXT("g_pBuf[%u]=%u doesn't match!!"), i, ((DWORD*)g_pBuf)[i]);
            FAIL("Buffer's data was corrupted");
            VERIFY(CloseHandle(hFile));
            return FALSE;
        }
    }

    // If we made it this far, the buffer matches
    LOG(TEXT("Buffer at address 5GB matched"));
    VERIFY(CloseHandle(hFile));

    // Try to move the pointer by 3GB
    hFile = CreateFile(szBuf, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        ERRFAIL("CreateFile");
        return FALSE;
    }

    LOG(TEXT("Try to move the pointer from the begining by 3GB"));
    dwOffset = 0xc0000000;
    lOffsetHigh = 0;
    dwPos=SetFilePointer(hFile, dwOffset, &lOffsetHigh, FILE_BEGIN);
    if (dwPos == 0xFFFFFFFF)
    {
        if (GetLastError() == NO_ERROR)
            LOG(TEXT("SetFilePointer returns 0xFFFFFFFF, but NO_ERROR Pos=0x%08x"), dwPos);
        else
            ERRFAIL("SetFilePointer");

        return FALSE;
    }

    // Read some data to verify
    if (!ReadFile(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL)
        || (dwBytes != WRITE_SIZE))
    {
        ERRFAIL("ReadFile");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    // Verify the buffer
    for ( i = 0; i < WRITE_SIZE/32; i++)
    {
        DWORD dwTemp = ((DWORD*)g_pBuf)[i];
        if ( ((DWORD*)g_pBuf)[i] != i )
        {
            LOG(TEXT("g_pBuf[%u]=%u doesn't match!!"), i, ((DWORD*)g_pBuf)[i]);
            FAIL("Buffer's data was corrupted");
            VERIFY(CloseHandle(hFile));
            return FALSE;
        }
    }

    // If we made it this far, the buffer matches
    LOG(TEXT("Buffer at address 3GB matched"));
    VERIFY(CloseHandle(hFile));


    // Try to move the pointer by 4GB
    hFile = CreateFile(szBuf, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        ERRFAIL("CreateFile");
        return FALSE;
    }

    LOG(TEXT("Try to move the pointer from the begining by 4GB"));
    dwOffset = 0;
    lOffsetHigh = 0x00000001;
    dwPos=SetFilePointer(hFile, dwOffset, &lOffsetHigh, FILE_BEGIN) ;
    if (dwPos== 0xFFFFFFFF)
    {
        if (GetLastError() == NO_ERROR)
            LOG(TEXT("SetFilePointer returns 0xFFFFFFFF, but NO_ERROR Pos=0x%08x"), dwPos);
        else
            ERRFAIL("SetFilePointer");

        return FALSE;
    }

    // Read some data to verify
    if (!ReadFile(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL)
        || (dwBytes != WRITE_SIZE))
    {
        ERRFAIL("ReadFile");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    // Verify the buffer
    for ( i = 0; i < WRITE_SIZE/32; i++)
    {
        if ( ((DWORD*)g_pBuf)[i] != i + WRITE_SIZE/32 )
        {
            LOG(TEXT("g_pBuf[%u]=%u doesn't match!!"), i, ((DWORD*)g_pBuf)[i]);
            FAIL("Buffer's data was corrupted");
            VERIFY(CloseHandle(hFile));
            return FALSE;
        }
    }

    // If we made it this far, the buffer matches
    LOG(TEXT("Buffer at address 4GB matched"));
    VERIFY(CloseHandle(hFile));

    // Scenario 3: Now let's try to move the pointer backward from the end
    // Move the pointer by 3GB backward
    hFile = CreateFile(szBuf, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        ERRFAIL("CreateFile");
        return FALSE;
    }

    LOG(TEXT("Try to move the pointer from the end by 3GB (backward)"));
    dwOffset = 0x40000000;
    lOffsetHigh = 0xFFFFFFFF;
    dwPos=SetFilePointer(hFile, dwOffset, &lOffsetHigh, FILE_END);
    if (dwPos == 0xFFFFFFFF)
    {
        if (GetLastError() == NO_ERROR)
            LOG(TEXT("SetFilePointer returns 0xFFFFFFFF, but NO_ERROR Pos=0x%08x"), dwPos);
        else
            ERRFAIL("SetFilePointer");

        return FALSE;
    }

    // Read some data to verify
    if (!ReadFile(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL)
        || (dwBytes != WRITE_SIZE))
    {
        ERRFAIL("ReadFile");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    // Verify the buffer
    for ( i = 0; i < WRITE_SIZE/32; i++)
    {
        if ( ((DWORD*)g_pBuf)[i] != i )
        {
            LOG(TEXT("g_pBuf[%u]=%u doesn't match!!"), i, ((DWORD*)g_pBuf)[i]);
            FAIL("Buffer's data was corrupted");
            VERIFY(CloseHandle(hFile));
            return FALSE;
        }
    }

    // If we made it this far, the buffer matches
    LOG(TEXT("Buffer at address 3GB (6GB-3GB) matched"));
    VERIFY(CloseHandle(hFile));

    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////
// Test_CopyLargeFile
//  Test copying a large file
//        Note: The file is created by the previous test case. This test case
//        might take a couple of minutes to execute
//
//        The following APIs are covered / somewhat-covered in this test case:
//            -CopyFile
//            -DeleteFile
//            -ReadFile
//            -FindFirstFile
//            -FindClose
//
//        CopyFile calls CopyFileEx
//
//        Test scenarios:
//            1. Copy the large file to another file
//            2. Verify the size and some contents of the new file
//
// Parameters:
//
// Return value:
//  TRUE if successful, FALSE to indicate an error condition.

BOOL Test_CopyLargeFile()
{
    WIN32_FIND_DATA fd;
    HANDLE          hFind;
    HANDLE          hFile;
    WCHAR           szBuf1[MAX_PATH];
    WCHAR           szBuf2[MAX_PATH];
    DWORD           i, dwBytes;
    DWORD           dwPos;         // Current position in the file
    DWORD           dwOffset;      // Offset to add to current position in the file
    LONG            lOffsetHigh;   // Offset high
    DWORD           dwFileSize;
    DWORD           dwFileSizeHigh;

    LOG(TEXT("Test_CopyLargeFile: Start testing"));
    // First, check to make sure the large file (created on the previous test
    // case) exists.

    StringCchCopy(szBuf1, _countof(szBuf1), g_szHardDiskRoot);
    StringCchCat(szBuf1, _countof(szBuf1), szLargeFile1);

    SetLastError(0);
    hFind = FindFirstFile(szBuf1, &fd);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        ERRFAIL("FindFirstFile");
        LOG(TEXT("!! %s does not exist! Please be sure to run test case 1001 first"), szBuf1);
        return FALSE;
    }
    if (!FindClose(hFind))
    {
        ERRFAIL("FindClose");
        return FALSE;
    }

    // Scenario 1: Copy the large file to another file
    StringCchCopy(szBuf2,  _countof(szBuf2), g_szHardDiskRoot);
    StringCchCat(szBuf2,  _countof(szBuf2), szLargeFile2);
    DeleteFile(szBuf2); // Clean up the destination file first

    LOG(TEXT("Copying large file %s to %s. Please wait..."), szBuf1, szBuf2);
    if (!CopyFile(szBuf1, szBuf2, TRUE))
    {
        ERRFAIL("CopyFile");
        return FALSE;
    }

    // Scenario 2: Verify the new file
    // Verify the size
    // Size should be 6GB exactly
    hFile = CreateFile(szBuf2, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        ERRFAIL("CreateFile");
        return FALSE;
    }

    dwFileSize = GetFileSize(hFile, &dwFileSizeHigh);
    LOG(TEXT("%s FileSize=0x%08x FileSizeHigh=0x%08x"), szBuf2, dwFileSize, dwFileSizeHigh);

    if (dwFileSize == 0x80000000 && dwFileSizeHigh == 0x00000001)
    {
        LOG(TEXT("Size matched"));
    }
    else
    {
        ERRFAIL("GetFileSize: Size didn't match");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    VERIFY(CloseHandle(hFile));

    // Verify some bytes on the new copied file.
    hFile = CreateFile(szBuf2, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        ERRFAIL("CreateFile");
        return FALSE;
    }

    // Verify data at the begining of the file
    LOG(TEXT("Check the data at the begining of the file"));
    dwPos=SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    if ( dwPos == 0xFFFFFFFF)
    {
        if (GetLastError() == NO_ERROR)
            LOG(TEXT("SetFilePointer returns 0xFFFFFFFF, but NO_ERROR Pos=0x%08x"), dwPos);
        else
            ERRFAIL("SetFilePointer");

        return FALSE;
    }

    // Read some data to verify
    if (!ReadFile(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL)
        || (dwBytes != WRITE_SIZE))
    {
        ERRFAIL("ReadFile");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    // Verify the buffer
    for ( i = 0; i < WRITE_SIZE/32; i++)
    {
        if ( ((DWORD*)g_pBuf)[i] != i )
        {
            LOG(TEXT("g_pBuf[%u]=%u doesn't match!!"), i, ((DWORD*)g_pBuf)[i]);
            FAIL("Buffer's data was corrupted");
            VERIFY(CloseHandle(hFile));
            return FALSE;
        }
    }
    LOG(TEXT("Data at the begining of the file is ok."));

    // Try to move the pointer by 5GB
    LOG(TEXT("Try to move the pointer from the begining by 5GB"));
    dwOffset = 0x40000000;
    lOffsetHigh = 0x00000001;
    dwPos=SetFilePointer(hFile, dwOffset, &lOffsetHigh, FILE_BEGIN);
    if (dwPos== 0xFFFFFFFF)
    {
        if (GetLastError() == NO_ERROR)
            LOG(TEXT("SetFilePointer returns 0xFFFFFFFF, but NO_ERROR Pos=0x%08x"), dwPos);
        else
            ERRFAIL("SetFilePointer");

        return FALSE;
    }

    // Read some data to verify
    if (!ReadFile(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL)
        || (dwBytes != WRITE_SIZE))
    {
        ERRFAIL("ReadFile");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    // Verify the buffer
    for ( i = 0; i < WRITE_SIZE/32; i++)
    {
        if ( ((DWORD*)g_pBuf)[i] != i + WRITE_SIZE/32 )
        {
            LOG(TEXT("g_pBuf[%u]=%u doesn't match!!"), i, ((DWORD*)g_pBuf)[i]);
            FAIL("Buffer's data was corrupted");
            VERIFY(CloseHandle(hFile));
            return FALSE;
        }
    }

    // If we made it this far, the buffer matches
    LOG(TEXT("Buffer at address 5GB matched"));

    LOG(TEXT("Try to move the pointer from the begining by 3GB"));
    dwOffset = 0xc0000000;
    lOffsetHigh = 0;
    dwPos=SetFilePointer(hFile, dwOffset, &lOffsetHigh, FILE_BEGIN);
    if (dwPos == 0xFFFFFFFF)
    {
        if (GetLastError() == NO_ERROR)
            LOG(TEXT("SetFilePointer returns 0xFFFFFFFF, but NO_ERROR Pos=0x%08x"), dwPos);
        else
            ERRFAIL("SetFilePointer");

        return FALSE;
    }

    // Read some data to verify
    if (!ReadFile(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL)
        || (dwBytes != WRITE_SIZE))
    {
        ERRFAIL("ReadFile");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    // Verify the buffer
    for ( i = 0; i < WRITE_SIZE/32; i++)
    {
        DWORD dwTemp = ((DWORD*)g_pBuf)[i];
        if ( ((DWORD*)g_pBuf)[i] != i )
        {
            LOG(TEXT("g_pBuf[%u]=%u doesn't match!!"), i, ((DWORD*)g_pBuf)[i]);
            FAIL("Buffer's data was corrupted");
            VERIFY(CloseHandle(hFile));
            return FALSE;
        }
    }

    // If we made it this far, the buffer matches
    LOG(TEXT("Buffer at address 3GB matched"));

    // Try to move the pointer by 4GB
    LOG(TEXT("Try to move the pointer from the begining by 4GB"));
    dwOffset = 0;
    lOffsetHigh = 0x00000001;
    dwPos=SetFilePointer(hFile, dwOffset, &lOffsetHigh, FILE_BEGIN);
    if (dwPos == 0xFFFFFFFF)
    {
        if (GetLastError() == NO_ERROR)
            LOG(TEXT("SetFilePointer returns 0xFFFFFFFF, but NO_ERROR Pos=0x%08x"), dwPos);
        else
            ERRFAIL("SetFilePointer");

        return FALSE;
    }

    // Read some data to verify
    if (!ReadFile(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL)
        || (dwBytes != WRITE_SIZE))
    {
        ERRFAIL("ReadFile");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    // Verify the buffer
    for ( i = 0; i < WRITE_SIZE/32; i++)
    {
        if ( ((DWORD*)g_pBuf)[i] != i + WRITE_SIZE/32 )
        {
            LOG(TEXT("g_pBuf[%u]=%u doesn't match!!"), i, ((DWORD*)g_pBuf)[i]);
            FAIL("Buffer's data was corrupted");
            VERIFY(CloseHandle(hFile));
            return FALSE;
        }
    }

    // If we made it this far, the buffer matches
    LOG(TEXT("Buffer at address 4GB matched"));

    VERIFY(CloseHandle(hFile));

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// Test_SetFilePointer2GB
//    SetFilePointer/ReadFile on address beginning + 2GB
//        Note: The file is created by the previous test case 1001.
//
//        Test scenarios:
//            1. SetFilePointer to 0x7FFFFFFF then SetFilePointer to 1
//               (lpDistanceToMoveHigh is NULL)
//            2. Move the pointer 1GB at a time.
//
// Parameters:
//
// Return value:
//  TRUE if successful, FALSE to indicate an error condition.

BOOL Test_SetFilePointer2GB()
{
    WIN32_FIND_DATA fd;
    HANDLE          hFind;
    HANDLE          hFile;
    WCHAR           szBuf[MAX_PATH];
    DWORD           i, dwBytes;
    DWORD           dwPos;          // Current position in the file
    LONG            lRet=0;

    LOG(TEXT("Test_SetFilePointer2GB: Start testing"));
    // First, check to make sure the large file (created on the previous test
    // case) exists.

    StringCchCopy(szBuf,  _countof(szBuf), g_szHardDiskRoot);
    StringCchCat(szBuf, _countof(szBuf), szLargeFile1);

    SetLastError(0);
    hFind = FindFirstFile(szBuf, &fd);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        ERRFAIL("FindFirstFile");
        LOG(TEXT("!! %s does not exist! Please be sure to run test case 1001 first"), szBuf);
        return FALSE;
    }
    if (!FindClose(hFind))
    {
        ERRFAIL("FindClose");
        return FALSE;
    }

    // Scenario 1: Move the pointer to 2GB-1 then move it again by 1.
    // SetFilePointer with lpDistanceToMoveHigh NULL
    hFile = CreateFile(szBuf, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        ERRFAIL("CreateFile");
        return FALSE;
    }

    LOG(TEXT("Move by 0x7FFFFFFF"));
    dwPos=SetFilePointer(hFile, 0x7FFFFFFF, NULL, FILE_BEGIN);
    if (dwPos== 0xFFFFFFFF)
    {
        if (GetLastError() == NO_ERROR)
            LOG(TEXT("SetFilePointer returns 0xFFFFFFFF, but NO_ERROR Pos=0x%08x"), dwPos);
        else
            ERRFAIL("SetFilePointer");

        return FALSE;
    }

    LOG(TEXT("Move by 1"));
    dwPos=SetFilePointer(hFile, 1, NULL, FILE_CURRENT);
    if (dwPos == 0xFFFFFFFF)
    {
        if (GetLastError() == NO_ERROR)
            LOG(TEXT("SetFilePointer returns 0xFFFFFFFF, but NO_ERROR Pos=0x%08x"), dwPos);
        else
            ERRFAIL("SetFilePointer");

        return FALSE;
    }

    // Read some data to verify
    if (!ReadFile(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL)
        || (dwBytes != WRITE_SIZE))
    {
        LOG(TEXT("ReadFile - dwBytes=%u"), dwBytes);
        ERRFAIL("ReadFile");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    // Verify the buffer
    for ( i = 0; i < WRITE_SIZE/32; i++)
    {
        if ( ((DWORD*)g_pBuf)[i] != i )
        {
            LOG(TEXT("g_pBuf[%u]=%u doesn't match!!"), i, ((DWORD*)g_pBuf)[i]);
            FAIL("Buffer's data was corrupted");
            VERIFY(CloseHandle(hFile));
            return FALSE;
        }
    }

    // If we made it this far, the buffer matches
    LOG(TEXT("Buffer at address 2GB matched"));
    VERIFY(CloseHandle(hFile));

    // Scenario 2: Move the pointer to 1GB at a time.
    // SetFilePointer with lpDistanceToMoveHigh NULL
    hFile = CreateFile(szBuf, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        ERRFAIL("CreateFile");
        return FALSE;
    }

    LOG(TEXT("Move by 0x40000000"));
    dwPos=SetFilePointer(hFile, 0x40000000, NULL, FILE_BEGIN);
    if (dwPos == 0xFFFFFFFF)
    {
        if (GetLastError() == NO_ERROR)
            LOG(TEXT("SetFilePointer returns 0xFFFFFFFF, but NO_ERROR Pos=0x%08x"), dwPos);
        else
            ERRFAIL("SetFilePointer");

        return FALSE;
    }

    LOG(TEXT("Move by 0x40000000"));
    dwPos=SetFilePointer(hFile, 0x40000000, NULL, FILE_CURRENT);
    if (dwPos == 0xFFFFFFFF)
    {
        if (GetLastError() == NO_ERROR)
            LOG(TEXT("SetFilePointer returns 0xFFFFFFFF, but NO_ERROR Pos=0x%08x"), dwPos);
        else
            ERRFAIL("SetFilePointer");

        return FALSE;
    }

    // Read some data to verify
    if (!ReadFile(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL)
        || (dwBytes != WRITE_SIZE))
    {
        ERRFAIL("ReadFile");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }
    LOG(TEXT("ReadFile - dwBytes=%u"), dwBytes);

    // Verify the buffer
    for ( i = 0; i < WRITE_SIZE/32; i++)
    {
        if ( ((DWORD*)g_pBuf)[i] != i )
        {
            LOG(TEXT("g_pBuf[%u]=%u doesn't match!!"), i, ((DWORD*)g_pBuf)[i]);
            FAIL("Buffer's data was corrupted");
            VERIFY(CloseHandle(hFile));
            return FALSE;
        }
    }

    // If we made it this far, the buffer matches
    LOG(TEXT("Buffer at address 2GB matched"));
    VERIFY(CloseHandle(hFile));

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// Test_GetFileInformationByHandle
//  Test GetFileInformationByHandle on a large file
//        Note: The file is created by the previous test case: 1001
//
//        Test scenarios:
//            1. Call GetFileInformationByHandle on a large file and verify info
//
// Parameters:
//
// Return value:
//  TRUE if successful, FALSE to indicate an error condition.

BOOL Test_GetFileInformationByHandle()
{
    WIN32_FIND_DATA            fd;
    HANDLE                     hFind;
    HANDLE                     hFile;
    WCHAR                      szBuf[MAX_PATH];
    BY_HANDLE_FILE_INFORMATION bhFileInfo;

    LOG(TEXT("Test_GetFileInformationByHandle: Start testing"));
    // First, check to make sure the large file (created on the previous test
    // case) exists.

    StringCchCopy(szBuf, _countof(szBuf), g_szHardDiskRoot);
    StringCchCat(szBuf, _countof(szBuf), szLargeFile1);

    SetLastError(0);
    hFind = FindFirstFile(szBuf, &fd);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        ERRFAIL("FindFirstFile");
        LOG(TEXT("!! %s does not exist! Please be sure to run test case 1001 first"), szBuf);
        return FALSE;
    }
    if (!FindClose(hFind))
    {
        ERRFAIL("FindClose");
        return FALSE;
    }

    // Scenario 1: Call the API on a large file created on the previous test case
    hFile = CreateFile(szBuf, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        ERRFAIL("CreateFile");
        return FALSE;
    }

    if (!GetFileInformationByHandle(hFile, &bhFileInfo))
    {
        ERRFAIL("GetFileInformationByHandle");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    // Print the information
    LOG(TEXT("File info: serial number=%u, fSizeHi=%.08x fSizeLo=%.08x fIndexHi=%.08x fIndexLo=%.08x nNumberOfLinks=%u"),
            bhFileInfo.dwVolumeSerialNumber, bhFileInfo.nFileSizeHigh, bhFileInfo.nFileSizeLow,
            bhFileInfo.nFileIndexHigh, bhFileInfo.nFileIndexLow, bhFileInfo.nNumberOfLinks);

    // Verify the size
    if (! (bhFileInfo.nFileSizeHigh == 0x00000001 && bhFileInfo.nFileSizeLow == 0x80000000) )
    {
        FAIL("GetFileInformationByHandle: Size is wrong!!");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    // Verify the number of links (always 1 for FAT)
    if (bhFileInfo.nNumberOfLinks != 1)
    {
        FAIL("GetFileInformationByHandle: nNumberOfLinks is wrong!!");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    VERIFY(CloseHandle(hFile));

    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////
// Test_FindFiles
//  Test FindFirst/NextFile when there are some large files arounds
//        Note: The files are created by the previous test case: 1001 & 1003
//        This needs some manual checkings
//
//        Test scenarios:
//            1. Populate \Hard Disk with some files
//            2. Call FindFirstFile and FindNextFile
//
// Parameters:
//
// Return value:
//  TRUE if successful, FALSE to indicate an error condition.

BOOL Test_FindFiles()
{
    WIN32_FIND_DATA fd;
    HANDLE          hFind;
    HANDLE          hFile;
    DWORD           i, j, k, dwBytes;
    WCHAR           szBuf[MAX_PATH];
    WCHAR           szTmp[MAX_PATH];
    int             nNumFiles;

    LOG(TEXT("Test_FindFiles: Start testing"));
    // First, check to make sure the large file (created on the previous test
    // case) exists.

    StringCchCopy(szBuf, _countof(szBuf), g_szHardDiskRoot);
    StringCchCat(szBuf, _countof(szBuf), szLargeFile1);

    SetLastError(0);
    hFind = FindFirstFile(szBuf, &fd);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        ERRFAIL("FindFirstFile");
        LOG(TEXT("!! %s does not exist! Please be sure to run test case 1001 first"), szBuf);
        return FALSE;
    }

    // Check the size, should be exactly 6GB
    if (!(fd.nFileSizeHigh == 0x00000001 && fd.nFileSizeLow == 0x80000000))
    {
        FAIL("FindFirstFile size didn't match!!");
        FindClose(hFind);
        return FALSE;
    }

    if (!FindClose(hFind))
    {
        ERRFAIL("FindClose");
        return FALSE;
    }

    // Populate Hard Disk with some files
    StringCchCopy(szBuf, _countof(szBuf), g_szHardDiskRoot);
    StringCchCat(szBuf, _countof(szBuf), TEXT("\\TestFile"));

    for (k = 0; k < 10; k++)
    {
        StringCchPrintf(szTmp, _countof(szTmp), TEXT("%s%d"), szBuf, k);

        LOG(TEXT("Create file %s. Please wait..."), szTmp);
        hFile = CreateFile(szTmp, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

        if (hFile == INVALID_HANDLE_VALUE)
        {
            ERRFAIL("CreateFile");
            return FALSE;
        }

        // Fill the buffer with the current address
        for (j = 0; j < WRITE_SIZE/32; j++)
        {
            ((DWORD*)(g_pBuf))[j] = j;
        }

        // Write the file
        for (i = 0; i < 10; i++)
        {
            // Write 512k data from the buffer
            if (!WriteFile(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL)
                || (dwBytes != WRITE_SIZE))
            {
                ERRFAIL("WriteFile");
                VERIFY(CloseHandle(hFile));
                return FALSE;
            }
        }

        SetEndOfFile(hFile);
        VERIFY(CloseHandle(hFile));
    }

    // Do FindFirstFile / FindNextFile
    LOG(TEXT("Test FindFirstFile and FindNextFile. Do some checking manually here"));
    hFind = FindFirstFile(TEXT("Hard Disk\\*.*"), &fd);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        ERRFAIL("FindFirstFile");
        return FALSE;
    }
    else
        LOG(TEXT("Found: %s sizeHi=%.08x sizeLo=%.08x"), fd.cFileName, fd.nFileSizeHigh, fd.nFileSizeLow);

    for (nNumFiles = 1;
         FindNextFile(hFind, &fd);
         nNumFiles++)
        LOG(TEXT("Found: %s sizeHi=%.08x sizeLo=%.08x"), fd.cFileName, fd.nFileSizeHigh, fd.nFileSizeLow);

    if (!FindClose(hFind))
    {
        ERRFAIL("FindClose");
        return FALSE;
    }

    LOG(TEXT("Found %d files"), nNumFiles);

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// Test_ReadWriteFileWithSeek
//  Test ReadFileWithSeek and WriteFileWithSeek on a large file.
//        Note: The files are created by the previous test case: 1001
//        You can only run this test case once after you run 1001, if you want
//        to run it again, rerun 1001 first.
//
//        Test scenarios:
//            1. Call ReadFileWithSeek on an existing file (6GB)
//
//            2. Call WriteFileWithSeek on an existing file, write on the "holes"
//               and some boundary conditions at the following addresses
//                    -1GB+1024k
//                    -5GB+1024k
//                    -4GB
//
//            3. Verify the data written on #2 with another ReadFileWithSeek
//
// Parameters:
//
// Return value:
//  TRUE if successful, FALSE to indicate an error condition.

BOOL Test_ReadWriteFileWithSeek()
{
    WIN32_FIND_DATA fd;
    HANDLE          hFind;
    HANDLE          hFile;
    DWORD           i, j, dwBytes;
    WCHAR           szBuf[MAX_PATH];

    LOG(TEXT("Test_ReadWriteFileWithSeek: Start testing - note please run 1001 first"));
    // First, check to make sure the large file (created on the previous test
    // case) exists.

    StringCchCopy(szBuf, _countof(szBuf), g_szHardDiskRoot);
    StringCchCat(szBuf, _countof(szBuf), szLargeFile1);

    SetLastError(0);
    hFind = FindFirstFile(szBuf, &fd);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        ERRFAIL("FindFirstFile");
        LOG(TEXT("!! %s does not exist! Please be sure to run test case 1001 first"), szBuf);
        return FALSE;
    }

    // Scenario 1: Call ReadFileWithSeek on the exisiting file
    // Try on various address of the file, first try on 1GB
    hFile = CreateFile(szBuf, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (!ReadFileWithSeek(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL, 0x40000000, 0)
        || (dwBytes != WRITE_SIZE))
    {
        LOG(TEXT("ReadFileWithSeek - dwBytes=%u"), dwBytes);
        ERRFAIL("ReadFileWithSeek");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    // Verify the buffer
    for ( i = 0; i < WRITE_SIZE/32; i++)
    {
        if ( ((DWORD*)g_pBuf)[i] != i )
        {
            LOG(TEXT("g_pBuf[%u]=%u doesn't match!!"), i, ((DWORD*)g_pBuf)[i]);
            FAIL("Buffer's data was corrupted");
            VERIFY(CloseHandle(hFile));
            return FALSE;
        }
    }

    // If we made it this far, the buffer matches
    LOG(TEXT("Buffer at address 1GB matched"));
    VERIFY(CloseHandle(hFile));

    // Try on various address of the file, try on 2GB
    hFile = CreateFile(szBuf, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (!ReadFileWithSeek(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL, 0x80000000, 0)
        || (dwBytes != WRITE_SIZE))
    {
        LOG(TEXT("ReadFileWithSeek - dwBytes=%u"), dwBytes);
        ERRFAIL("ReadFileWithSeek");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    // Verify the buffer
    for ( i = 0; i < WRITE_SIZE/32; i++)
    {
        if ( ((DWORD*)g_pBuf)[i] != i )
        {
            LOG(TEXT("g_pBuf[%u]=%u doesn't match!!"), i, ((DWORD*)g_pBuf)[i]);
            FAIL("Buffer's data was corrupted");
            VERIFY(CloseHandle(hFile));
            return FALSE;
        }
    }

    // If we made it this far, the buffer matches
    LOG(TEXT("Buffer at address 2GB matched"));
    VERIFY(CloseHandle(hFile));

    // Try on various address of the file, try on 3GB
    hFile = CreateFile(szBuf, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (!ReadFileWithSeek(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL, 0xc0000000, 0)
        || (dwBytes != WRITE_SIZE))
    {
        LOG(TEXT("ReadFileWithSeek - dwBytes=%u"), dwBytes);
        ERRFAIL("ReadFileWithSeek");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    // Verify the buffer
    for ( i = 0; i < WRITE_SIZE/32; i++)
    {
        if ( ((DWORD*)g_pBuf)[i] != i )
        {
            LOG(TEXT("g_pBuf[%u]=%u doesn't match!!"), i, ((DWORD*)g_pBuf)[i]);
            FAIL("Buffer's data was corrupted");
            VERIFY(CloseHandle(hFile));
            return FALSE;
        }
    }

    // If we made it this far, the buffer matches
    LOG(TEXT("Buffer at address 3GB matched"));
    VERIFY(CloseHandle(hFile));

    // Try on various address of the file, try on 4GB
    hFile = CreateFile(szBuf, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (!ReadFileWithSeek(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL, 0, 0x00000001)
        || (dwBytes != WRITE_SIZE))
    {
        LOG(TEXT("ReadFileWithSeek - dwBytes=%u"), dwBytes);
        ERRFAIL("ReadFileWithSeek");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    // Verify the buffer
    for ( i = 0; i < WRITE_SIZE/32; i++)
    {
        if ( ((DWORD*)g_pBuf)[i] != (i + WRITE_SIZE/32))
        {
            LOG(TEXT("g_pBuf[%u]=%u doesn't match!!"), i, ((DWORD*)g_pBuf)[i]);
            FAIL("Buffer's data was corrupted");
            VERIFY(CloseHandle(hFile));
            return FALSE;
        }
    }

    // If we made it this far, the buffer matches
    LOG(TEXT("Buffer at address 4GB matched"));
    VERIFY(CloseHandle(hFile));

    // Try on various address of the file, try on 5GB
    hFile = CreateFile(szBuf, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (!ReadFileWithSeek(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL, 0x40000000, 0x00000001)
        || (dwBytes != WRITE_SIZE))
    {
        LOG(TEXT("ReadFileWithSeek - dwBytes=%u"), dwBytes);
        ERRFAIL("ReadFileWithSeek");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    // Verify the buffer
    for ( i = 0; i < WRITE_SIZE/32; i++)
    {
        if ( ((DWORD*)g_pBuf)[i] != (i + WRITE_SIZE/32))
        {
            LOG(TEXT("g_pBuf[%u]=%u doesn't match!!"), i, ((DWORD*)g_pBuf)[i]);
            FAIL("Buffer's data was corrupted");
            VERIFY(CloseHandle(hFile));
            return FALSE;
        }
    }

    // If we made it this far, the buffer matches
    LOG(TEXT("Buffer at address 5GB matched"));
    VERIFY(CloseHandle(hFile));

    // Scenario 2: Call WriteFileWithSeek to write data in the "holes"
    hFile = CreateFile(szBuf, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    // Fill the buffer with a different pattern (index + 999)
    for (j = 0; j < WRITE_SIZE/32; j++)
    {
        ((DWORD*)(g_pBuf))[j] = j + 999;
    }

    // Write some data (512k) here at address 1GB + 1024k
    if (!WriteFileWithSeek(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL, 0x40100000, 0)
        || (dwBytes != WRITE_SIZE))
    {
        LOG(TEXT("WriteFileWithSeek - dwBytes=%u"), dwBytes);
        ERRFAIL("WriteFileWithSeek");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    VERIFY(CloseHandle(hFile));

    // Scenario 3: Verify the data written by WriteDataWithSeek
    hFile = CreateFile(szBuf, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (!ReadFileWithSeek(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL, 0x40100000, 0)
        || (dwBytes != WRITE_SIZE))
    {
        LOG(TEXT("ReadFileWithSeek - dwBytes=%u"), dwBytes);
        ERRFAIL("ReadFileWithSeek");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    // Verify the buffer
    for ( i = 0; i < WRITE_SIZE/32; i++)
    {
        if ( ((DWORD*)g_pBuf)[i] != (i + 999))
        {
            LOG(TEXT("g_pBuf[%u]=%u doesn't match!!"), i, ((DWORD*)g_pBuf)[i]);
            FAIL("Buffer's data was corrupted");
            VERIFY(CloseHandle(hFile));
            return FALSE;
        }
    }

    // If we made it this far, the buffer matches
    LOG(TEXT("Buffer at address 1GB+1024k matched"));
    VERIFY(CloseHandle(hFile));

    // Scenario 2b: Call WriteFileWithSeek to write data in the "holes"
    hFile = CreateFile(szBuf, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    // Fill the buffer with a different pattern (index + 777)
    for (j = 0; j < WRITE_SIZE/32; j++)
    {
        ((DWORD*)(g_pBuf))[j] = j + 777;
    }

    // Write some data (512k) here at address 5GB + 1024k
    if (!WriteFileWithSeek(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL, 0x40100000, 0x00000001)
        || (dwBytes != WRITE_SIZE))
    {
        LOG(TEXT("WriteFileWithSeek - dwBytes=%u"), dwBytes);
        ERRFAIL("WriteFileWithSeek");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    VERIFY(CloseHandle(hFile));

    // Scenario 3b: Verify the data written by WriteDataWithSeek
    hFile = CreateFile(szBuf, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (!ReadFileWithSeek(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL, 0x40100000, 0x00000001)
        || (dwBytes != WRITE_SIZE))
    {
        LOG(TEXT("ReadFileWithSeek - dwBytes=%u"), dwBytes);
        ERRFAIL("ReadFileWithSeek");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    // Verify the buffer
    for ( i = 0; i < WRITE_SIZE/32; i++)
    {
        if ( ((DWORD*)g_pBuf)[i] != (i + 777))
        {
            LOG(TEXT("g_pBuf[%u]=%u doesn't match!!"), i, ((DWORD*)g_pBuf)[i]);
            FAIL("Buffer's data was corrupted");
            VERIFY(CloseHandle(hFile));
            return FALSE;
        }
    }

    // If we made it this far, the buffer matches
    LOG(TEXT("Buffer at address 5GB+1024k matched"));
    VERIFY(CloseHandle(hFile));

    // Scenario 2c: Call WriteFileWithSeek to write data in the boundary condition
    hFile = CreateFile(szBuf, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    // Fill the buffer with a different pattern (index + 555)
    for (j = 0; j < WRITE_SIZE/32; j++)
    {
        ((DWORD*)(g_pBuf))[j] = j + 555;
    }

    // Write some data (512k) here at address 4GB (overwrite the previous data)
    if (!WriteFileWithSeek(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL, 0, 0x00000001)
        || (dwBytes != WRITE_SIZE))
    {
        LOG(TEXT("WriteFileWithSeek - dwBytes=%u"), dwBytes);
        ERRFAIL("WriteFileWithSeek");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    VERIFY(CloseHandle(hFile));

    // Scenario 3c: Verify the data written by WriteDataWithSeek
    hFile = CreateFile(szBuf, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (!ReadFileWithSeek(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL, 0, 0x00000001)
        || (dwBytes != WRITE_SIZE))
    {
        LOG(TEXT("ReadFileWithSeek - dwBytes=%u"), dwBytes);
        ERRFAIL("ReadFileWithSeek");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    // Verify the buffer
    for ( i = 0; i < WRITE_SIZE/32; i++)
    {
        if ( ((DWORD*)g_pBuf)[i] != (i + 555))
        {
            LOG(TEXT("g_pBuf[%u]=%u doesn't match!!"), i, ((DWORD*)g_pBuf)[i]);
            FAIL("Buffer's data was corrupted");
            VERIFY(CloseHandle(hFile));
            return FALSE;
        }
    }

    // If we made it this far, the buffer matches
    LOG(TEXT("Buffer at address 4GB matched"));
    VERIFY(CloseHandle(hFile));

    // Let's write the data back (as original)
    hFile = CreateFile(szBuf, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    // Fill the buffer with the original pattern (index + WRITE_SIZE/32)
    for (j = 0; j < WRITE_SIZE/32; j++)
    {
        ((DWORD*)(g_pBuf))[j] = j + WRITE_SIZE/32;
    }

    // Write some data (512k) here at address 4GB (overwrite the previous data)
    if (!WriteFileWithSeek(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL, 0, 0x00000001)
        || (dwBytes != WRITE_SIZE))
    {
        LOG(TEXT("WriteFileWithSeek - dwBytes=%u"), dwBytes);
        ERRFAIL("WriteFileWithSeek");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    VERIFY(CloseHandle(hFile));


    // Scenario 2d: Call WriteFileWithSeek to write data in the boundary condition
    hFile = CreateFile(szBuf, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    // Fill the buffer with a different pattern (index + 333)
    for (j = 0; j < WRITE_SIZE/32; j++)
    {
        ((DWORD*)(g_pBuf))[j] = j + 333;
    }

    // Write some data (512k) here at address 4GB (overwrite the previous data)
    if (!WriteFileWithSeek(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL, 0x80000000, 0)
        || (dwBytes != WRITE_SIZE))
    {
        LOG(TEXT("WriteFileWithSeek - dwBytes=%u"), dwBytes);
        ERRFAIL("WriteFileWithSeek");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    VERIFY(CloseHandle(hFile));

    // Scenario 3d: Verify the data written by WriteDataWithSeek
    hFile = CreateFile(szBuf, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (!ReadFileWithSeek(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL, 0x80000000, 0)
        || (dwBytes != WRITE_SIZE))
    {
        LOG(TEXT("ReadFileWithSeek - dwBytes=%u"), dwBytes);
        ERRFAIL("ReadFileWithSeek");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    // Verify the buffer
    for ( i = 0; i < WRITE_SIZE/32; i++)
    {
        if ( ((DWORD*)g_pBuf)[i] != (i + 333))
        {
            LOG(TEXT("g_pBuf[%u]=%u doesn't match!!"), i, ((DWORD*)g_pBuf)[i]);
            FAIL("Buffer's data was corrupted");
            VERIFY(CloseHandle(hFile));
            return FALSE;
        }
    }

    // If we made it this far, the buffer matches
    LOG(TEXT("Buffer at address 2GB matched"));
    VERIFY(CloseHandle(hFile));

    // Write it back with the original data
    hFile = CreateFile(szBuf, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    // Fill the buffer with the original pattern (index)
    for (j = 0; j < WRITE_SIZE/32; j++)
    {
        ((DWORD*)(g_pBuf))[j] = j;
    }

    // Write some data (512k) here at address 4GB (overwrite the previous data)
    if (!WriteFileWithSeek(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL, 0x80000000, 0)
        || (dwBytes != WRITE_SIZE))
    {
        LOG(TEXT("WriteFileWithSeek - dwBytes=%u"), dwBytes);
        ERRFAIL("WriteFileWithSeek");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    VERIFY(CloseHandle(hFile));


    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// Test_CreateVeryLargeFile
//  Test creating a very large file (up to the disk free space)
//        Warning: This test case might take a long time to execute.
//
//        Test scenarios:
//            1. Create a file whose size is up to the free disk space
//            2. Verify the size
//            3. Try to advance the pointer towards the end of the file and verify
//               the data
//
// Parameters:
//
// Return value:
//  TRUE if successful, FALSE to indicate an error condition.

BOOL Test_CreateVeryLargeFile()
{
    WIN32_FIND_DATA fd;
    HANDLE          hFind;
    HANDLE          hFile;
    WCHAR           szBuf[MAX_PATH];
    DWORD           i, j, dwBytes;
    DWORD           dwOffset;      // Offset to add to current position in the file
    LONG            lOffsetHigh;   // Offset high
    DWORD           dwFileSize;
    DWORD           dwFileSizeHigh;
    ULARGE_INTEGER  ulFreeBytes;
    ULARGE_INTEGER  ulTotNumBytes;
    ULARGE_INTEGER  ulTotNumFreeBytes;
    ULARGE_INTEGER  ulFileSizeTmp;
    ULARGE_INTEGER  ulFilePointerTmp;
    DWORD           dwNumOfWrite;
    DWORD           dwPos;

    LOG(TEXT("Test_CreateVeryLargeFile: Start testing"));

    // First, check if we have Hard Disk mounted on the system
    SetLastError(0);
    hFind = FindFirstFile(g_szHardDiskRoot, &fd);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        ERRFAIL("FindFirstFile");
        LOG(TEXT("!! %s does not exist!"), g_szHardDiskRoot);
        return FALSE;
    }
    if (!FindClose(hFind))
    {
        ERRFAIL("FindClose");
        return FALSE;
    }

    // Clean up existing file, if any
    StringCchCopy(szBuf, _countof(szBuf), g_szHardDiskRoot);
    StringCchCat(szBuf, _countof(szBuf), szLargeFile3);
    DeleteFile(szBuf);

    // Get the disk free space to determine how big should the file be
    if ( !(GetDiskFreeSpaceEx(g_szHardDiskRoot, &ulFreeBytes, &ulTotNumBytes, &ulTotNumFreeBytes)) )
    {
        ERRFAIL("GetDiskFreeSpaceEx");
        return FALSE;
    }

    LOG(TEXT("Avail %.08x %.08x, Total %.08x %.08x Free %.08x %.08x"),ulFreeBytes.HighPart, ulFreeBytes.LowPart,
                                                    ulTotNumBytes.HighPart, ulTotNumBytes.LowPart,
                                                    ulTotNumFreeBytes.HighPart, ulTotNumFreeBytes.LowPart);

    // Scenario 1: Create a very large file (up to the free disk space)
    LOG(TEXT("Create file %s. Please wait, this might take a long time..."), szBuf);
    hFile = CreateFile(szBuf, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        ERRFAIL("CreateFile");
        return FALSE;
    }

    // Fill the buffer with the current address to make the data recognizable
    for (j = 0; j < WRITE_SIZE/32; j++)
    {
        ((DWORD*)(g_pBuf))[j] = j;
    }

    dwOffset = BUFFER_SIZE;

    // I'm assuming the result of this calculation fits in a DWORD (32-bit)
    // We will write 32 * 512k at a time. 512k is the size of the buffer
    // Write 1*512k of data + 31*512k of holes
    dwNumOfWrite = (DWORD) (ulFreeBytes.QuadPart / (32*512*1024));
    LOG(TEXT("# of write = %d"), dwNumOfWrite);

    // This will write file with size specified above
    for (i = 0; i < dwNumOfWrite; i++)
    {
        // We're going to advance 16MB at a time: 512k data + 31*512k holes.

        // Write 512k data from the buffer
        if (!WriteFile(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL)
            || (dwBytes != WRITE_SIZE))
        {
            ERRFAIL("WriteFile");
            VERIFY(CloseHandle(hFile));
            return FALSE;
        }
        // Put 7*512k holes here to save time on file creation
        if (SetFilePointer(hFile, 31*dwOffset, NULL, FILE_CURRENT) == 0xFFFFFFFF)
        {
            if (GetLastError() == NO_ERROR)
                LOG(TEXT("SetFilePointer returns 0xFFFFFFFF, but NO_ERROR"));
            else
                ERRFAIL("SetFilePointer");

            return FALSE;
        }
    }

    SetEndOfFile(hFile);

    // Scenarion 2: Verify the size
    dwFileSize = GetFileSize(hFile, &dwFileSizeHigh);
    LOG(TEXT("%s FileSize=0x%08x FileSizeHigh=0x%08x"), szBuf, dwFileSize, dwFileSizeHigh);

    ulFileSizeTmp.QuadPart = dwNumOfWrite;
    ulFileSizeTmp.QuadPart = ulFileSizeTmp.QuadPart*32*512*1024;

    // Size should be the same as we calculated before
    if (dwFileSize == ulFileSizeTmp.LowPart && dwFileSizeHigh == ulFileSizeTmp.HighPart)
    {
        LOG(TEXT("Size matched."));
    }
    else
    {
        DWORD dwErr=GetLastError();

        if (dwErr == NO_ERROR)
            LOG(TEXT("GetFileSize: NO_ERROR - we shouldn't be here (size didn't match)!!"));
        else
            LOG(TEXT("GetFileSize: Size didn't match!! Error=%u"), dwErr);

        FAIL("GetFileSize");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    VERIFY(CloseHandle(hFile));

    // Scenario 3: Try to set the pointer towards the end and verify the data
    ulFilePointerTmp.QuadPart = ulFileSizeTmp.QuadPart - (32*512*1024);

    // Try to move the pointer by 3GB
    hFile = CreateFile(szBuf, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        ERRFAIL("CreateFile");
        return FALSE;
    }

    LOG(TEXT("Try to move the pointer from the begining by %.08x %.08x"),
                    ulFilePointerTmp.HighPart, ulFilePointerTmp.LowPart);

    dwOffset = ulFilePointerTmp.LowPart;
    lOffsetHigh = ulFilePointerTmp.HighPart;
    dwPos=SetFilePointer(hFile, dwOffset, &lOffsetHigh, FILE_BEGIN);
    if (dwPos == 0xFFFFFFFF)
    {
        if (GetLastError() == NO_ERROR)
            LOG(TEXT("SetFilePointer returns 0xFFFFFFFF, but NO_ERROR Pos=0x%08x"), dwPos);
        else
            ERRFAIL("SetFilePointer");

        return FALSE;
    }

    // Read some data to verify
    if (!ReadFile(hFile, g_pBuf, WRITE_SIZE, &dwBytes, NULL)
        || (dwBytes != WRITE_SIZE))
    {
        ERRFAIL("ReadFile");
        VERIFY(CloseHandle(hFile));
        return FALSE;
    }

    // Verify the buffer
    for ( i = 0; i < WRITE_SIZE/32; i++)
    {
        DWORD dwTemp = ((DWORD*)g_pBuf)[i];
        if ( ((DWORD*)g_pBuf)[i] != i )
        {
            LOG(TEXT("g_pBuf[%u]=%u doesn't match!!"), i, ((DWORD*)g_pBuf)[i]);
            FAIL("Buffer's data was corrupted");
            VERIFY(CloseHandle(hFile));
            return FALSE;
        }
    }

    // If we made it this far, the buffer matches
    LOG(TEXT("Buffer at the specified address matched!!"));
    VERIFY(CloseHandle(hFile));

    return TRUE;
}
