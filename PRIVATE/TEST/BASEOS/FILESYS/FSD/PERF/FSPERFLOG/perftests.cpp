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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include "main.h"
#include "globals.h"

// --------------------------------------------------------------------
// Runs all the throughput perf tests in sequence
// --------------------------------------------------------------------
TESTPROCAPI Tst_RunSuite(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD dwRet = TPR_FAIL;

    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    g_dwDataSize = lpFTE->dwUserData;

    if(TPR_FAIL == Tst_WriteFile(uMsg, tpParam, lpFTE))
    {
        goto Error;
    }

    if(TPR_FAIL == Tst_ReadFile(uMsg, tpParam, lpFTE))
    {
        goto Error;
    }

    if(TPR_PASS != Tst_WriteFileRandom(uMsg, tpParam, lpFTE))
    {
        goto Error;
    }

    if(TPR_PASS != Tst_ReadFileRandom(uMsg, tpParam, lpFTE))
    {
        goto Error;
    }

    if(TPR_PASS != Tst_ReverseReadFile(uMsg, tpParam, lpFTE))
    {
        goto Error;
    }

    // success
    dwRet = TPR_PASS;

Error:
    return dwRet;
}

// --------------------------------------------------------------------
// Write out a test file in blocks of specified size
// --------------------------------------------------------------------
TESTPROCAPI Tst_WriteFile(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD dwRet = TPR_FAIL;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    LPBYTE pBuffer = NULL;
    DWORD dwBytesTransfered = 0;
    DWORD dwNumTransfers = 0;
    LPTSTR pszPerfMarkerName = NULL;
    LPTSTR pszPerfMarkerParams = NULL;
    CPerflog * pCPerflog = NULL;

    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    // Skip if there are no testable paths
    if ((!VALID_POINTER(g_pCFSUtils)) || (!g_pCFSUtils->IsValid()))
    {
        return TPR_SKIP;
    }
    
    // Calculate number of writes required at the current data size
    if (0 == g_dwDataSize)
    {
        ERRFAIL("Invalid data size specified, skipping test");
        return TPR_SKIP;
    }
    else if (g_dwDataSize > g_dwFileSize)
    {
        dwNumTransfers = 1;
    }
    else
    {
        // Make sure we write at least once
        dwNumTransfers = max(g_dwFileSize / g_dwDataSize, 1);
    }

    // Allocate string buffers for perf parameters
    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CPerflog::MAX_NAME_LENGTH);
    pszPerfMarkerParams = (TCHAR*) malloc(sizeof(TCHAR) * CPerflog::MAX_PARAM_LENGTH);
    if ((NULL == pszPerfMarkerName) || (NULL == pszPerfMarkerName))
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Fill the perf strings with test-specific data
    CreateMarkerName(
        pszPerfMarkerName,
        CPerflog::MAX_NAME_LENGTH,
        _T("WriteFile"));

    CreateMarkerParams(
        pszPerfMarkerParams,
        CPerflog::MAX_PARAM_LENGTH,
        g_dwDataSize,
        g_dwFileSize,
        _T("Bytes/sec"));

    // Create perf logger object
    pCPerflog = new CPerflog(pszPerfMarkerName, pszPerfMarkerParams);
    if (NULL == pCPerflog)
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Allocate room for write buffer
    pBuffer = (LPBYTE)LocalAlloc(LMEM_FIXED, g_dwDataSize);
    if(NULL == pBuffer)
    {
        ERRFAIL("LocalAlloc()");
        goto Error;
    }

    // Start test loop
    for(DWORD dwTestIteration = 1; dwTestIteration <= g_dwNumTestIterations; dwTestIteration++)
    {
        g_pCFSUtils->DeleteFile(g_szTestFileName);

        // Create a new file for writing
        hFile = g_pCFSUtils->CreateTestFile(
            g_szTestFileName,
            GENERIC_WRITE,
            0,
            NULL,
            CREATE_ALWAYS,
            g_dwFileCreateFlags,
            NULL);
        if (INVALID_HANDLE(hFile))
        {
            ERRFAIL("CreateFile()");
            goto Error;
        }

        // Start the perf logging
        START_PERF();

        // Write out file in blocks of specified size
        for(DWORD dwDataTransfer = 1; dwDataTransfer <= dwNumTransfers; dwDataTransfer++)
        {
            if (!WriteFile(hFile, pBuffer, g_dwDataSize, &dwBytesTransfered, NULL))
            {
                ERRFAIL("WriteFile()");
                goto Error;
            }
            else if (dwBytesTransfered != g_dwDataSize)
            {
                ERRFAIL("WriteFile()");
                goto Error;
            }
        }

        CLOSE_FILE_END_PERF(hFile);
    }

    // success
    dwRet = TPR_PASS;

Error:
    // Close handle, free memory, clean up
    CHECK_END_PERF(pCPerflog);
    CHECK_CLOSE_HANDLE(hFile);
    CHECK_LOCAL_FREE(pBuffer);
    CHECK_FREE(pszPerfMarkerName);
    CHECK_FREE(pszPerfMarkerParams);
    CHECK_DELETE_FILE(g_szTestFileName);
    return dwRet;
}

// --------------------------------------------------------------------
// Write out a test file in blocks of specified size at random offsets
// --------------------------------------------------------------------
TESTPROCAPI Tst_WriteFileRandom(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD dwRet = TPR_FAIL;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    LPBYTE pBuffer = NULL;
    DWORD dwBytesTransfered = 0;
    DWORD dwNumTransfers = 0;
    DWORD dwFileOffset = 0;
    LPTSTR pszPerfMarkerName = NULL;
    LPTSTR pszPerfMarkerParams = NULL;
    CPerflog * pCPerflog = NULL;

    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    // Skip if there are no testable paths
    if ((!VALID_POINTER(g_pCFSUtils)) || (!g_pCFSUtils->IsValid()))
    {
        return TPR_SKIP;
    }
    
    // Calculate number of writes required at the current data size
    if (0 == g_dwDataSize)
    {
        ERRFAIL("Invalid data size specified, skipping test");
        return TPR_SKIP;
    }
    else if (g_dwDataSize > g_dwFileSize)
    {
        dwNumTransfers = 1;
    }
    else
    {
        // Make sure we write at least once
        dwNumTransfers = max(g_dwFileSize / g_dwDataSize, 1);
    }

    // Allocate string buffers for perf parameters
    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CPerflog::MAX_NAME_LENGTH);
    pszPerfMarkerParams = (TCHAR*) malloc(sizeof(TCHAR) * CPerflog::MAX_PARAM_LENGTH);
    if ((NULL == pszPerfMarkerName) || (NULL == pszPerfMarkerName))
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Fill the perf strings with test-specific data
    CreateMarkerName(
        pszPerfMarkerName,
        CPerflog::MAX_NAME_LENGTH,
        _T("WriteFileRandom"));

    CreateMarkerParams(
        pszPerfMarkerParams,
        CPerflog::MAX_PARAM_LENGTH,
        g_dwDataSize,
        g_dwFileSize,
        _T("Bytes/sec"));

    // Create perf logger object
    pCPerflog = new CPerflog(pszPerfMarkerName, pszPerfMarkerParams);
    if (NULL == pCPerflog)
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Allocate room for write buffer
    pBuffer = (LPBYTE)LocalAlloc(LMEM_FIXED, g_dwDataSize);
    if(NULL == pBuffer)
    {
        ERRFAIL("LocalAlloc()");
        goto Error;
    }

    // Start test loop
    for(DWORD dwTestIteration = 1; dwTestIteration <= g_dwNumTestIterations; dwTestIteration++)
    {
        g_pCFSUtils->DeleteFile(g_szTestFileName);

        // Create a new file for writing
        hFile = g_pCFSUtils->CreateTestFile(
            g_szTestFileName,
            GENERIC_WRITE,
            0,
            NULL,
            CREATE_ALWAYS,
            g_dwFileCreateFlags,
            NULL);
        if (INVALID_HANDLE(hFile))
        {
            ERRFAIL("CreateFile()");
            goto Error;
        }

        // Start the perf logging
        START_PERF();

        // Write out file in blocks of specified size
        for(DWORD dwDataTransfer = 1; dwDataTransfer <= dwNumTransfers; dwDataTransfer++)
        {
            // Pick a random place within the file to write to
            dwFileOffset = Random() % (g_dwFileSize - g_dwDataSize);

            // Set file pointer at the random location
            if (!SetFilePointer(hFile, dwFileOffset, NULL, FILE_BEGIN))
            {
                ERRFAIL("SetFilePointer()");
                goto Error;
            }

            if (!WriteFile(hFile, pBuffer, g_dwDataSize, &dwBytesTransfered, NULL))
            {
                ERRFAIL("WriteFile()");
                goto Error;
            }
            else if (dwBytesTransfered != g_dwDataSize)
            {
                ERRFAIL("WriteFile()");
                goto Error;
            }
        }

        CLOSE_FILE_END_PERF(hFile);
    }

    // success
    dwRet = TPR_PASS;

Error:
    // Close handle, free memory, clean up
    CHECK_END_PERF(pCPerflog);
    CHECK_CLOSE_HANDLE(hFile);
    CHECK_LOCAL_FREE(pBuffer);
    CHECK_FREE(pszPerfMarkerName);
    CHECK_FREE(pszPerfMarkerParams);
    CHECK_DELETE_FILE(g_szTestFileName);
    return dwRet;
}

// --------------------------------------------------------------------
// Read in a test file in blocks of specified size
// --------------------------------------------------------------------
TESTPROCAPI Tst_ReadFile(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD dwRet = TPR_FAIL;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    LPBYTE pBuffer = NULL;
    DWORD dwBytesTransfered = 0;
    DWORD dwNumTransfers = 0;
    LPTSTR pszPerfMarkerName = NULL;
    LPTSTR pszPerfMarkerParams = NULL;
    CPerflog * pCPerflog = NULL;

    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    // Skip if there are no testable paths
    if ((!VALID_POINTER(g_pCFSUtils)) || (!g_pCFSUtils->IsValid()))
    {
        return TPR_SKIP;
    }
    
    // Calculate number of reads required at the current data size
    if (0 == g_dwDataSize)
    {
        ERRFAIL("Invalid data size specified, skipping test");
        return TPR_SKIP;
    }
    else if (g_dwDataSize > g_dwFileSize)
    {
        dwNumTransfers = 1;
    }
    else
    {
        // Make sure we read at least once
        dwNumTransfers = max(g_dwFileSize / g_dwDataSize, 1);
    }

    // Allocate string buffers for perf parameters
    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CPerflog::MAX_NAME_LENGTH);
    pszPerfMarkerParams = (TCHAR*) malloc(sizeof(TCHAR) * CPerflog::MAX_PARAM_LENGTH);
    if ((NULL == pszPerfMarkerName) || (NULL == pszPerfMarkerName))
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Fill the perf strings with test-specific data
    CreateMarkerName(
        pszPerfMarkerName,
        CPerflog::MAX_NAME_LENGTH,
        _T("ReadFile"));

    CreateMarkerParams(
        pszPerfMarkerParams,
        CPerflog::MAX_PARAM_LENGTH,
        g_dwDataSize,
        g_dwFileSize,
        _T("Bytes/sec"));

    // Create perf logger object
    pCPerflog = new CPerflog(pszPerfMarkerName, pszPerfMarkerParams);
    if (NULL == pCPerflog)
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Allocate room for read buffer
    pBuffer = (LPBYTE)LocalAlloc(LMEM_FIXED, g_dwDataSize);
    if(NULL == pBuffer)
    {
        ERRFAIL("LocalAlloc()");
        goto Error;
    }

    // Create a file for reading
    if (!g_pCFSUtils->CreateTestFile(g_szTestFileName, g_dwFileSize, FALSE))
    {
        ERRFAIL("CreateTestFile()");
        goto Error;
    }

    // Start test loop
    for(DWORD dwTestIteration = 1; dwTestIteration <= g_dwNumTestIterations; dwTestIteration++)
    {
        // Open the test file for read
        hFile = g_pCFSUtils->CreateTestFile(
            g_szTestFileName,
            GENERIC_READ,
            0,
            NULL,
            OPEN_EXISTING,
            g_dwFileCreateFlags,
            NULL);
        if (INVALID_HANDLE(hFile))
        {
            ERRFAIL("CreateFile()");
            goto Error;
        }

        // Start the perf logging
        START_PERF();

        // Read in file in blocks of specified size
        for(DWORD dwDataTransfer = 1; dwDataTransfer <= dwNumTransfers; dwDataTransfer++)
        {
            if (!ReadFile(hFile, pBuffer, g_dwDataSize, &dwBytesTransfered, NULL))
            {
                ERRFAIL("ReadFile()");
                goto Error;
            }
            else if (dwBytesTransfered != g_dwDataSize)
            {
                ERRFAIL("ReadFile()");
                goto Error;
            }
        }

        CLOSE_FILE_END_PERF(hFile);
    }

    // success
    dwRet = TPR_PASS;

Error:
    // Close handle, free memory, clean up
    CHECK_END_PERF(pCPerflog);
    CHECK_CLOSE_HANDLE(hFile);
    CHECK_LOCAL_FREE(pBuffer);
    CHECK_FREE(pszPerfMarkerName);
    CHECK_FREE(pszPerfMarkerParams);
    CHECK_DELETE_FILE(g_szTestFileName);
    return dwRet;
}

// --------------------------------------------------------------------
// Read in a test file in blocks of specified size at random offsets
// --------------------------------------------------------------------
TESTPROCAPI Tst_ReadFileRandom(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD dwRet = TPR_FAIL;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    LPBYTE pBuffer = NULL;
    DWORD dwBytesTransfered = 0;
    DWORD dwNumTransfers = 0;
    DWORD dwFileOffset = 0;
    LPTSTR pszPerfMarkerName = NULL;
    LPTSTR pszPerfMarkerParams = NULL;
    CPerflog * pCPerflog = NULL;

    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    // Skip if there are no testable paths
    if ((!VALID_POINTER(g_pCFSUtils)) || (!g_pCFSUtils->IsValid()))
    {
        return TPR_SKIP;
    }
    
    // Calculate number of reads required at the current data size
    if (0 == g_dwDataSize)
    {
        ERRFAIL("Invalid data size specified, skipping test");
        return TPR_SKIP;
    }
    else if (g_dwDataSize > g_dwFileSize)
    {
        dwNumTransfers = 1;
    }
    else
    {
        // Make sure we read at least once
        dwNumTransfers = max(g_dwFileSize / g_dwDataSize, 1);
    }

    // Allocate string buffers for perf parameters
    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CPerflog::MAX_NAME_LENGTH);
    pszPerfMarkerParams = (TCHAR*) malloc(sizeof(TCHAR) * CPerflog::MAX_PARAM_LENGTH);
    if ((NULL == pszPerfMarkerName) || (NULL == pszPerfMarkerName))
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Fill the perf strings with test-specific data
    CreateMarkerName(
        pszPerfMarkerName,
        CPerflog::MAX_NAME_LENGTH,
        _T("ReadFileRandom"));

    CreateMarkerParams(
        pszPerfMarkerParams,
        CPerflog::MAX_PARAM_LENGTH,
        g_dwDataSize,
        g_dwFileSize,
        _T("Bytes/sec"));

    // Create perf logger object
    pCPerflog = new CPerflog(pszPerfMarkerName, pszPerfMarkerParams);
    if (NULL == pCPerflog)
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Allocate room for read buffer
    pBuffer = (LPBYTE)LocalAlloc(LMEM_FIXED, g_dwDataSize);
    if(NULL == pBuffer)
    {
        ERRFAIL("LocalAlloc()");
        goto Error;
    }

    // Create a file for reading
    if (!g_pCFSUtils->CreateTestFile(g_szTestFileName, g_dwFileSize, FALSE))
    {
        ERRFAIL("CreateTestFile()");
        goto Error;
    }

    // Start test loop
    for(DWORD dwTestIteration = 1; dwTestIteration <= g_dwNumTestIterations; dwTestIteration++)
    {
        // Open the test file for read
        hFile = g_pCFSUtils->CreateTestFile(
            g_szTestFileName,
            GENERIC_READ,
            0,
            NULL,
            OPEN_EXISTING,
            g_dwFileCreateFlags,
            NULL);
        if (INVALID_HANDLE(hFile))
        {
            ERRFAIL("CreateFile()");
            goto Error;
        }

        // Start the perf logging
        START_PERF();

        // Read in file in blocks of specified size
        for(DWORD dwDataTransfer = 1; dwDataTransfer <= dwNumTransfers; dwDataTransfer++)
        {
            // Pick a random place within the file to read from
            dwFileOffset = Random() % (g_dwFileSize - g_dwDataSize);

            // Set file pointer at the random location
            if (!SetFilePointer(hFile, dwFileOffset, NULL, FILE_BEGIN))
            {
                ERRFAIL("SetFilePointer()");
                goto Error;
            }

            if (!ReadFile(hFile, pBuffer, g_dwDataSize, &dwBytesTransfered, NULL))
            {
                ERRFAIL("ReadFile()");
                goto Error;
            }
            else if (dwBytesTransfered != g_dwDataSize)
            {
                ERRFAIL("ReadFile()");
                goto Error;
            }
        }

        CLOSE_FILE_END_PERF(hFile);
    }

    // success
    dwRet = TPR_PASS;

Error:
    // Close handle, free memory, clean up
    CHECK_END_PERF(pCPerflog);
    CHECK_CLOSE_HANDLE(hFile);
    CHECK_LOCAL_FREE(pBuffer);
    CHECK_FREE(pszPerfMarkerName);
    CHECK_FREE(pszPerfMarkerParams);
    CHECK_DELETE_FILE(g_szTestFileName);
    return dwRet;
}

// --------------------------------------------------------------------
// Read in a test file in blocks of specified size while moving
// backwards through the file
// --------------------------------------------------------------------
TESTPROCAPI Tst_ReverseReadFile(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD dwRet = TPR_FAIL;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    LPBYTE pBuffer = NULL;
    DWORD dwBytesTransfered = 0;
    DWORD dwNumTransfers = 0;
    LPTSTR pszPerfMarkerName = NULL;
    LPTSTR pszPerfMarkerParams = NULL;
    CPerflog * pCPerflog = NULL;

    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    // Skip if there are no testable paths
    if ((!VALID_POINTER(g_pCFSUtils)) || (!g_pCFSUtils->IsValid()))
    {
        return TPR_SKIP;
    }
    
    // Calculate number of reads required at the current data size
    if (0 == g_dwDataSize)
    {
        ERRFAIL("Invalid data size specified, skipping test");
        return TPR_SKIP;
    }
    else if (g_dwDataSize > g_dwFileSize)
    {
        dwNumTransfers = 1;
    }
    else
    {
        // Make sure we read at least once
        dwNumTransfers = max(g_dwFileSize / g_dwDataSize, 1);
    }

    // Allocate string buffers for perf parameters
    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CPerflog::MAX_NAME_LENGTH);
    pszPerfMarkerParams = (TCHAR*) malloc(sizeof(TCHAR) * CPerflog::MAX_PARAM_LENGTH);
    if ((NULL == pszPerfMarkerName) || (NULL == pszPerfMarkerName))
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Fill the perf strings with test-specific data
    CreateMarkerName(
        pszPerfMarkerName,
        CPerflog::MAX_NAME_LENGTH,
        _T("ReverseReadFile"));

    CreateMarkerParams(
        pszPerfMarkerParams,
        CPerflog::MAX_PARAM_LENGTH,
        g_dwDataSize,
        g_dwFileSize,
        _T("Bytes/sec"));

    // Create perf logger object
    pCPerflog = new CPerflog(pszPerfMarkerName, pszPerfMarkerParams);
    if (NULL == pCPerflog)
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Allocate room for read buffer
    pBuffer = (LPBYTE)LocalAlloc(LMEM_FIXED, g_dwDataSize);
    if(NULL == pBuffer)
    {
        ERRFAIL("LocalAlloc()");
        goto Error;
    }

    // Create a file for reading
    if (!g_pCFSUtils->CreateTestFile(g_szTestFileName, g_dwFileSize, FALSE))
    {
        ERRFAIL("CreateTestFile()");
        goto Error;
    }

    // Start test loop
    for(DWORD dwTestIteration = 1; dwTestIteration <= g_dwNumTestIterations; dwTestIteration++)
    {
        // Open the test file for read
        hFile = g_pCFSUtils->CreateTestFile(
            g_szTestFileName,
            GENERIC_READ,
            0,
            NULL,
            OPEN_EXISTING,
            g_dwFileCreateFlags,
            NULL);
        if (INVALID_HANDLE(hFile))
        {
            ERRFAIL("CreateFile()");
            goto Error;
        }

        // Set the file pointer one block away from the end of file
        if (0xFFFFFFFF == SetFilePointer(hFile, -(long)g_dwDataSize, NULL, FILE_END))
        {
            ERRFAIL("SetFilePointer()");
            goto Error;
        }

        // Start the perf logging
        START_PERF();

        // Read in file in blocks of specified size
        for(DWORD dwDataTransfer = 1; dwDataTransfer <= dwNumTransfers; dwDataTransfer++)
        {
            if (!ReadFile(hFile, pBuffer, g_dwDataSize, &dwBytesTransfered, NULL))
            {
                ERRFAIL("ReadFile()");
                goto Error;
            }
            else if (dwBytesTransfered != g_dwDataSize)
            {
                ERRFAIL("ReadFile()");
                goto Error;
            }
            // Move the pointer backwards
            if ((0xFFFFFFFF == SetFilePointer(hFile, -(2*(long)g_dwDataSize), NULL, FILE_CURRENT))
                && (ERROR_SUCCESS != GetLastError()))
            {
                break;
            }
        }

        CLOSE_FILE_END_PERF(hFile);
    }

    // success
    dwRet = TPR_PASS;

Error:
    // Close handle, free memory, clean up
    CHECK_END_PERF(pCPerflog);
    CHECK_CLOSE_HANDLE(hFile);
    CHECK_LOCAL_FREE(pBuffer);
    CHECK_FREE(pszPerfMarkerName);
    CHECK_FREE(pszPerfMarkerParams);
    CHECK_DELETE_FILE(g_szTestFileName);
    return dwRet;
}

// --------------------------------------------------------------------
// Time how long it takes to perform a specific number of seeks
// followed by reads or writes
// --------------------------------------------------------------------
TESTPROCAPI Tst_SeekSpeed(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD dwRet = TPR_FAIL;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    LPBYTE pBuffer = NULL;
    BOOL fReadFile = TRUE;
    DWORD dwFileAccessFlags = 0;
    DWORD dwAccessSize = 1;
    DWORD dwNumSeeks = NUM_TEST_SEEKS;
    DWORD dwBytesTransfered = 0;
    DWORD dwFileOffset = 0;
    LPTSTR pszPerfMarkerName = NULL;
    LPTSTR pszPerfMarkerParams = NULL;
    CPerflog * pCPerflog = NULL;

    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    // Skip if there are no testable paths
    if ((!VALID_POINTER(g_pCFSUtils)) || (!g_pCFSUtils->IsValid()))
    {
        return TPR_SKIP;
    }

    // Read the parameter 1 = Read, 0 = Write
    fReadFile = (BOOL) lpFTE->dwUserData;
    if (fReadFile)
    {
        dwFileAccessFlags = GENERIC_READ;
    }
    else
    {
        dwFileAccessFlags = GENERIC_WRITE;
    }

    // Allocate string buffers for perf parameters
    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CPerflog::MAX_NAME_LENGTH);
    pszPerfMarkerParams = (TCHAR*) malloc(sizeof(TCHAR) * CPerflog::MAX_PARAM_LENGTH);
    if ((NULL == pszPerfMarkerName) || (NULL == pszPerfMarkerName))
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Fill the perf strings with test-specific data
    CreateMarkerName(
        pszPerfMarkerName,
        CPerflog::MAX_NAME_LENGTH,
        fReadFile ? _T("SeekSpeed (Read)") : _T("SeekSpeed (Write"));

    CreateMarkerParams(
        pszPerfMarkerParams,
        CPerflog::MAX_PARAM_LENGTH,
        0,
        dwNumSeeks,
        _T("Seeks/sec"));

    // Create perf logger object
    pCPerflog = new CPerflog(pszPerfMarkerName, pszPerfMarkerParams);
    if (NULL == pCPerflog)
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Allocate room for read/write buffer
    pBuffer = (LPBYTE)LocalAlloc(LMEM_FIXED, dwAccessSize);
    if(NULL == pBuffer)
    {
        ERRFAIL("LocalAlloc()");
        goto Error;
    }

    // Create a test file for reading/writing
    if (!g_pCFSUtils->CreateTestFile(g_szTestFileName, g_dwFileSize, FALSE))
    {
        ERRFAIL("CreateTestFile()");
        goto Error;
    }

    // Start test loop
    for(DWORD dwTestIteration = 1; dwTestIteration <= g_dwNumTestIterations; dwTestIteration++)
    {
        // Create a new file for writing
        hFile = g_pCFSUtils->CreateTestFile(
            g_szTestFileName,
            dwFileAccessFlags,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        if (INVALID_HANDLE(hFile))
        {
            ERRFAIL("CreateFile()");
            goto Error;
        }

        // Start the perf logging
        START_PERF();

        // Write out file in blocks of specified size
        for(DWORD dwDataTransfer = 1; dwDataTransfer <= dwNumSeeks; dwDataTransfer++)
        {
            // Pick a random offset to seek to
            dwFileOffset = Random() % (g_dwFileSize - dwAccessSize);

            // Set file pointer at the random location
            if (!SetFilePointer(hFile, dwFileOffset, NULL, FILE_BEGIN))
            {
                ERRFAIL("SetFilePointer()");
                goto Error;
            }

            if (fReadFile && !ReadFile(hFile, pBuffer, dwAccessSize, &dwBytesTransfered, NULL))
            {
                ERRFAIL("ReadFile()");
                goto Error;
            }

            if (!fReadFile && !WriteFile(hFile, pBuffer, dwAccessSize, &dwBytesTransfered, NULL))
            {
                ERRFAIL("WriteFile()");
                goto Error;
            }
            
            if (dwBytesTransfered != dwAccessSize)
            {
                ERRFAIL("I/O Failure");
                goto Error;
            }
        }

        // End the logging
        CLOSE_FILE_END_PERF(hFile);
    }

    // success
    dwRet = TPR_PASS;

Error:
    // Close handle, free memory, clean up
    CHECK_END_PERF(pCPerflog);
    CHECK_CLOSE_HANDLE(hFile);
    CHECK_LOCAL_FREE(pBuffer);
    CHECK_FREE(pszPerfMarkerName);
    CHECK_FREE(pszPerfMarkerParams);
    CHECK_DELETE_FILE(g_szTestFileName);
    return dwRet;
}

// --------------------------------------------------------------------
// Create a directory tree n levels deep
// --------------------------------------------------------------------
TESTPROCAPI Tst_CreateDeepTree(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD dwRet = TPR_FAIL;
    DWORD dwTreeDepth = 0;
    TCHAR szPath[MAX_PATH] = _T("");
    LPTSTR pszPerfMarkerName = NULL;
    LPTSTR pszPerfMarkerParams = NULL;
    CPerflog * pCPerflog = NULL;

    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    // Skip if there are no testable paths
    if ((!VALID_POINTER(g_pCFSUtils)) || (!g_pCFSUtils->IsValid()))
    {
        return TPR_SKIP;
    }

    // Read the tree depth parameter
    dwTreeDepth = (DWORD) lpFTE->dwUserData;

    // Allocate string buffers for perf parameters
    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CPerflog::MAX_NAME_LENGTH);
    pszPerfMarkerParams = (TCHAR*) malloc(sizeof(TCHAR) * CPerflog::MAX_PARAM_LENGTH);
    if ((NULL == pszPerfMarkerName) || (NULL == pszPerfMarkerName))
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Fill the perf strings with test-specific data
    CreateMarkerName(
        pszPerfMarkerName,
        CPerflog::MAX_NAME_LENGTH,
        _T("DeepDirectoryTree"));

    CreateMarkerParams(
        pszPerfMarkerParams,
        CPerflog::MAX_PARAM_LENGTH,
        0,
        dwTreeDepth,
        _T("Dirs/sec"));

    // Create perf logger object
    pCPerflog = new CPerflog(pszPerfMarkerName, pszPerfMarkerParams);
    if (NULL == pCPerflog)
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Remove test directory if it exists prior to the test
    if (!g_pCFSUtils->CleanDirectory(DEFAULT_TEST_DIRECTORY, TRUE))
    {
        ERRFAIL("CleanTestDirectory()");
        goto Error;
    }

    // Create test root
    if (!g_pCFSUtils->CreateDirectory(DEFAULT_TEST_DIRECTORY))
    {
        ERRFAIL("CreateDirectory()");
        goto Error;
    }

    for(DWORD dwTestIteration = 1; dwTestIteration <= g_dwNumTestIterations; dwTestIteration++)
    {
        // Copy test root into path variable
        VERIFY(SUCCEEDED(StringCchPrintf(
            szPath,
            MAX_PATH,
            _T("%s"),
            DEFAULT_TEST_DIRECTORY)));

        // Start the perf logging
        START_PERF();

        // Create a vertical tree g_dwFileSize deep
        for (DWORD i = 0; i < dwTreeDepth; i++)
        {
            // Append another directory to path
            VERIFY(SUCCEEDED(StringCchCat(
                szPath,
                MAX_PATH,
                DEFAULT_DIRECTORY_NAME)));
            // Create a directory
            if (!g_pCFSUtils->CreateDirectory(szPath))
            {
                ERRFAIL("CreateDirectory()");
                goto Error;
            }
        }

        // End the perf logging
        END_PERF();

        if (!g_pCFSUtils->CleanDirectory(DEFAULT_TEST_DIRECTORY, FALSE))
        {
            ERRFAIL("CleanDirectory()");
            goto Error;
        }
    }

    // success
    dwRet = TPR_PASS;

Error:
    // Cleanup
    CHECK_END_PERF(pCPerflog);
    CHECK_FREE(pszPerfMarkerName);
    CHECK_FREE(pszPerfMarkerParams);
    CHECK_CLEAN_DIRECTORY(g_pCFSUtils, DEFAULT_TEST_DIRECTORY);
    return dwRet;
}

// --------------------------------------------------------------------
// Test the time it takes to create a large file with SetEndOfFile
// --------------------------------------------------------------------
TESTPROCAPI Tst_SetEndOfFile(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD dwRet = TPR_FAIL;
    DWORD dwFileSize = 0;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    LPTSTR pszPerfMarkerName = NULL;
    LPTSTR pszPerfMarkerParams = NULL;
    CPerflog * pCPerflog = NULL;

    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    // Skip if there are no testable paths
    if ((!VALID_POINTER(g_pCFSUtils)) || (!g_pCFSUtils->IsValid()))
    {
        return TPR_SKIP;
    }

    // Read the file size parameter
    dwFileSize = (DWORD) lpFTE->dwUserData;

    // Skip if there is not enough space to perform this operation
    if (g_pCFSUtils->GetDiskFreeSpace() < dwFileSize)
    {
        LOG(_T("The filesystem does not have enough diskspace to run this test. Skipping."));
        return TPR_SKIP;
    }

    // Allocate string buffers for perf parameters
    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CPerflog::MAX_NAME_LENGTH);
    pszPerfMarkerParams = (TCHAR*) malloc(sizeof(TCHAR) * CPerflog::MAX_PARAM_LENGTH);
    if ((NULL == pszPerfMarkerName) || (NULL == pszPerfMarkerName))
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Fill the perf strings with test-specific data
    CreateMarkerName(
        pszPerfMarkerName,
        CPerflog::MAX_NAME_LENGTH,
        _T("SetEndOfFile"));

    CreateMarkerParams(
        pszPerfMarkerParams,
        CPerflog::MAX_PARAM_LENGTH,
        0,
        dwFileSize,
        _T("Bytes/sec"));

    // Create perf logger object
    pCPerflog = new CPerflog(pszPerfMarkerName, pszPerfMarkerParams);
    if (NULL == pCPerflog)
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Start test loop
    for(DWORD dwTestIteration = 1; dwTestIteration <= g_dwNumTestIterations; dwTestIteration++)
    {
        g_pCFSUtils->DeleteFile(g_szTestFileName);

        // Create a new file for writing
        hFile = g_pCFSUtils->CreateTestFile(
            g_szTestFileName,
            GENERIC_WRITE,
            0,
            NULL,
            CREATE_ALWAYS,
            g_dwFileCreateFlags,
            NULL);
        if (INVALID_HANDLE(hFile))
        {
            ERRFAIL("CreateFile()");
            goto Error;
        }

        // Start the perf logging
        START_PERF();

        if (!g_pCFSUtils->CreateTestFileFast(hFile, dwFileSize))
        {
            ERRFAIL("CreateTestFileFast()");
            goto Error;
        }

        CLOSE_FILE_END_PERF(hFile);
    }

    // success
    dwRet = TPR_PASS;

Error:
    // Close handle, free memory, clean up
    CHECK_END_PERF(pCPerflog);
    CHECK_CLOSE_HANDLE(hFile);
    CHECK_FREE(pszPerfMarkerName);
    CHECK_FREE(pszPerfMarkerParams);
    CHECK_DELETE_FILE(g_szTestFileName);
    return dwRet;
}

// --------------------------------------------------------------------
// Copies files between root directory and test path
// --------------------------------------------------------------------
TESTPROCAPI Tst_CopyFiles(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD dwRet = TPR_FAIL;
    LPTSTR pszPerfMarkerName = NULL;
    LPTSTR pszPerfMarkerParams = NULL;
    BOOL fStorageToRoot = FALSE;
    CPerflog * pCPerflog = NULL;
    CFSUtils * pCFSUtils = NULL;
    CFSUtils * pSrcFS = NULL;
    CFSUtils * pDstFS = NULL;
    TCHAR szSrcDir[MAX_PATH] = _T("");
    TCHAR szDstDir[MAX_PATH] = _T("");

    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    // Skip if there are no testable paths
    if ((!VALID_POINTER(g_pCFSUtils)) || (!g_pCFSUtils->IsValid()))
    {
        return TPR_SKIP;
    }

    // Determine which way to copy the files
    fStorageToRoot = (BOOL) lpFTE->dwUserData;

    // Create another CFSUtils object for the root filesystem
    pCFSUtils = CFSUtils::CreateWithPath(_T("\\"));
    if (!VALID_POINTER(pCFSUtils))
    {
        ERRFAIL("CFSUtils::CreateWithPath() failed");
        goto Error;
    }

    // Allocate string buffers for perf parameters
    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CPerflog::MAX_NAME_LENGTH);
    pszPerfMarkerParams = (TCHAR*) malloc(sizeof(TCHAR) * CPerflog::MAX_PARAM_LENGTH);
    if ((NULL == pszPerfMarkerName) || (NULL == pszPerfMarkerName))
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Fill the perf strings with test-specific data
    CreateMarkerName(
        pszPerfMarkerName,
        CPerflog::MAX_NAME_LENGTH,
        fStorageToRoot ? _T("CopyFilesToRoot") : _T("CopyFilesToStorage"));

    CreateMarkerParams(
        pszPerfMarkerParams,
        CPerflog::MAX_PARAM_LENGTH,
        0,
        DEFAULT_FILE_SET_SIZE,
        _T("Bytes/sec"));

    // Create perf logger object
    pCPerflog = new CPerflog(pszPerfMarkerName, pszPerfMarkerParams);
    if (NULL == pCPerflog)
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Assign the source and destination objects
    if (fStorageToRoot)
    {
        pSrcFS = g_pCFSUtils;
        pDstFS = pCFSUtils;
    }
    else
    {
        pSrcFS = pCFSUtils;
        pDstFS = g_pCFSUtils;
    }

    // Delete the test directories if they exist
    if (!pSrcFS->CleanDirectory(DEFAULT_SRC_DIRECTORY, TRUE) ||
        !pDstFS->CleanDirectory(DEFAULT_TEST_DIRECTORY, TRUE))
    {
        ERRFAIL("CleanTestDirectory()");
        goto Error;
    }

    // Create the test file set in the source/dst file systems
    if (!pSrcFS->CreateDirectory(DEFAULT_SRC_DIRECTORY) ||
        !pDstFS->CreateDirectory(DEFAULT_DST_DIRECTORY))
    {
        ERRFAIL("CreateDirectory()");
        goto Error;
    }

    // Get fully qualified src and target paths
    pSrcFS->GetFullPath(szSrcDir, MAX_PATH, DEFAULT_SRC_DIRECTORY);
    pDstFS->GetFullPath(szDstDir, MAX_PATH, DEFAULT_DST_DIRECTORY);

    // Populate src directory with test files
    if (!pSrcFS->CreateTestFileSet(DEFAULT_SRC_DIRECTORY, DEFAULT_FILE_SET_COUNT, DEFAULT_FILE_SET_SIZE))
    {
        ERRFAIL("CreateTestFileSet()");
        goto Error;
    }

    // Start test loop
    for(DWORD dwTestIteration = 1; dwTestIteration <= g_dwNumTestIterations; dwTestIteration++)
    {
        // Clear destination directory
        if (!pDstFS->CleanDirectory(DEFAULT_DST_DIRECTORY, FALSE))
        {
            ERRFAIL("CleanTree()");
            goto Error;
        }

        // Start the perf logging
        START_PERF();

        // Copy directory tree
        if (!CFSUtils::CopyDirectoryTree(szSrcDir, szDstDir))
        {
            ERRFAIL("CopyDirectoryTree()");
            goto Error;
        }

        END_PERF();
    }

    // success
    dwRet = TPR_PASS;

Error:
    // Free memory, clean up
    CHECK_CLEAN_DIRECTORY(pSrcFS, DEFAULT_SRC_DIRECTORY);
    CHECK_CLEAN_DIRECTORY(pDstFS, DEFAULT_DST_DIRECTORY);
    CHECK_END_PERF(pCPerflog);
    CHECK_DELETE(pCFSUtils);
    CHECK_FREE(pszPerfMarkerName);
    CHECK_FREE(pszPerfMarkerParams);
    return dwRet;
}

// --------------------------------------------------------------------
// Reads and appends data to a large fragmented file
// --------------------------------------------------------------------
TESTPROCAPI Tst_ReadAppendFragmentedFile(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD dwRet = TPR_FAIL;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    LPBYTE pBuffer = NULL;
    DWORD dwBytesTransfered = 0;
    DWORD dwNumTransfers = 0;
    DWORD dwInitialFileSize = 0;
    DWORD dwTotalDataSize = 0;
    LPTSTR pszPerfMarkerName = NULL;
    LPTSTR pszPerfMarkerParams = NULL;
    CPerflog * pCPerflog = NULL;

    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    // Skip if there are no testable paths
    if ((!VALID_POINTER(g_pCFSUtils)) || (!g_pCFSUtils->IsValid()))
    {
        return TPR_SKIP;
    }
    
    // Get file size parameter
    dwInitialFileSize = (DWORD) lpFTE->dwUserData;
    // We will read half the file and write half the file
    dwNumTransfers = (dwInitialFileSize >> 1) / DEFAULT_WRITE_SIZE;
    // Calculate the amount of data that will be transfered
    // during the test(approx the original size of the file)
    // We are counting both the bytes that will be read and written
    dwTotalDataSize = dwNumTransfers * DEFAULT_WRITE_SIZE * 2;

    // Skip if there is not enough space to perform this operation
    if (g_pCFSUtils->GetDiskFreeSpace() < dwInitialFileSize*3)
    {
        LOG(_T("The filesystem does not have enough diskspace to run this test. Skipping."));
        return TPR_SKIP;
    }

    // Allocate string buffers for perf parameters
    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CPerflog::MAX_NAME_LENGTH);
    pszPerfMarkerParams = (TCHAR*) malloc(sizeof(TCHAR) * CPerflog::MAX_PARAM_LENGTH);
    if ((NULL == pszPerfMarkerName) || (NULL == pszPerfMarkerName))
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Fill the perf strings with test-specific data
    CreateMarkerName(
        pszPerfMarkerName,
        CPerflog::MAX_NAME_LENGTH,
        _T("ReadAppendFragmentedFile"));

    CreateMarkerParams(
        pszPerfMarkerParams,
        CPerflog::MAX_PARAM_LENGTH,
        g_dwDataSize,
        dwTotalDataSize,
        _T("Bytes/sec"));

    // Create perf logger object
    pCPerflog = new CPerflog(pszPerfMarkerName, pszPerfMarkerParams);
    if (NULL == pCPerflog)
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Allocate room for read buffer
    pBuffer = (LPBYTE)LocalAlloc(LMEM_FIXED, DEFAULT_WRITE_SIZE);
    if(NULL == pBuffer)
    {
        ERRFAIL("LocalAlloc()");
        goto Error;
    }

    // Create a file for reading
    if (!g_pCFSUtils->CreateFragmentedTestFile(g_szTestFileName, dwInitialFileSize))
    {
        ERRFAIL("CreateTestFile()");
        goto Error;
    }

    // Start test loop
    for(DWORD dwTestIteration = 1; dwTestIteration <= g_dwNumTestIterations; dwTestIteration++)
    {
        // Open the test file for read
        hFile = g_pCFSUtils->CreateTestFile(
            g_szTestFileName,
            GENERIC_READ|GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            g_dwFileCreateFlags,
            NULL);
        if (INVALID_HANDLE(hFile))
        {
            ERRFAIL("CreateFile()");
            goto Error;
        }

        // Start the perf logging
        START_PERF();

        // Read in file in blocks of specified size
        for(DWORD dwDataTransfer = 1; dwDataTransfer <= dwNumTransfers; dwDataTransfer++)
        {
            // Append a block of data to the end
            if (0xFFFFFFFF == SetFilePointer(hFile, 0, NULL, FILE_END))
            {
                ERRFAIL("SetFilePointer()");
                goto Error;
            }
            if (!WriteFile(hFile, pBuffer, DEFAULT_WRITE_SIZE, &dwBytesTransfered, NULL))
            {          
                ERRFAIL("WriteFile()");
                goto Error;
            }
            if (dwBytesTransfered != DEFAULT_WRITE_SIZE)
            {
                ERRFAIL("WriteFile()");
                goto Error;
            }
            // Read a block of data from the beginning
            if (0xFFFFFFFF == SetFilePointer(hFile, DEFAULT_WRITE_SIZE * (dwDataTransfer-1), NULL, FILE_BEGIN))
            {
                ERRFAIL("SetFilePointer()");
                goto Error;
            }
            if (!ReadFile(hFile, pBuffer, DEFAULT_WRITE_SIZE, &dwBytesTransfered, NULL))
            {
                ERRFAIL("ReadFile()");
                goto Error;
            }
            if (dwBytesTransfered != DEFAULT_WRITE_SIZE)
            {
                ERRFAIL("ReadFile()");
                goto Error;
            }
        }

        CLOSE_FILE_END_PERF(hFile);
    }

    // success
    dwRet = TPR_PASS;

Error:
    // Close handle, free memory, clean up
    CHECK_END_PERF(pCPerflog);
    CHECK_CLOSE_HANDLE(hFile);
    CHECK_LOCAL_FREE(pBuffer);
    CHECK_FREE(pszPerfMarkerName);
    CHECK_FREE(pszPerfMarkerParams);
    CHECK_DELETE_FILE(g_szTestFileName);
    return dwRet;
}