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
#include "main.h"
#include "globals.h"
#include "perfscenconsts.h"

static const DWORD AUXDATA_BUFF_SIZE = 128;
const LPCTSTR FINDFIRST_SEARCH_STRINGS[] = {L"perf.file1.jpg", L"perf.file?.*", L"perf.file*.*"};

// Throughput tests:
TESTPROCAPI Tst_WriteFile(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/);
TESTPROCAPI Tst_ReadFile(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/);
TESTPROCAPI Tst_WriteFileRandom(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/);
TESTPROCAPI Tst_ReadFileRandom(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/);
TESTPROCAPI Tst_ReverseReadFile(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/);
TESTPROCAPI Tst_OverWriteFile(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/);

static enum ENUM_THROUGHPUT_TESTS
{
    TP_WriteFile = 0,
    TP_ReadFile,
    TP_WriteFileRandom,
    TP_ReadFileRandom,
    TP_ReverseReadFile,
    TP_OverWriteFile,
    NUM_THROUGHPUT_TESTS,
};   

static const TESTPROC PFN_THROUGHPUT_TESTS[]=
{
    Tst_WriteFile,
    Tst_ReadFile,
    Tst_WriteFileRandom,
    Tst_ReadFileRandom,
    Tst_ReverseReadFile,
    Tst_OverWriteFile
};

// --------------------------------------------------------------------
// Provides a lookup table index into the g_PERFGUID array.
// --------------------------------------------------------------------
DWORD ThroughputTestGUIDIndex(DWORD dwTestID)
{
    DWORD dwRet = (dwTestID - THROUGHPUT_BASE) * NUM_THROUGHPUT_TESTS;
    return dwRet;
}

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

    if (!InitFileSys())
    {
        SKIP("The test volume has not been successfully initialized, skipping test case");
        dwRet = TPR_SKIP;
        goto Error;
    }

    g_dwDataSize = lpFTE->dwUserData;

    // NOTE: If you add another test to this suite, you will need to add to
    //       the GUIDs used by the test during reporting as well as the arrays
    //       throughput test arrays/enums.
    for (DWORD i=0; i<NUM_THROUGHPUT_TESTS; i++)
    {
        if (TPR_FAIL == PFN_THROUGHPUT_TESTS[i](uMsg, tpParam, lpFTE))
        {
            goto Error;
        }
    }

    // success
    dwRet = TPR_PASS;


Error:
    return dwRet;
}

// --------------------------------------------------------------------
// Write out a test file in blocks of specified size
// --------------------------------------------------------------------
TESTPROCAPI Tst_WriteFile(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD dwRet = TPR_FAIL;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    LPBYTE pBuffer = NULL;
    DWORD dwBytesTransfered = 0;
    DWORD dwNumTransfers = 0;
    LPTSTR pszPerfMarkerName = NULL;
    CPerfHelper * pCPerflog = NULL;

    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    // Skip if there are no testable paths
    if (!InitFileSys())
    {
        SKIP("The test volume has not been successfully initialized, skipping test case");
        dwRet = TPR_SKIP;
        goto Error;
    }
    else if (!InitPerfLog())
    {
        SKIP("Problem initializing CePerf and PerfScenario. Ensure that both CePerf.dll and PerfScenario.dll are available before running the test");
        dwRet = TPR_SKIP;
        goto Error;
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
    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CPerfScenario::MAX_NAME_LENGTH);
    if (NULL == pszPerfMarkerName)
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Fill the perf strings with test-specific data
    CreateMarkerName(
        pszPerfMarkerName,
        CPerfScenario::MAX_NAME_LENGTH,
        STR_THROUGHPUT);

    // Create perf logger object
    pCPerflog = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_THROUGHPUT, g_dwDataSize, g_dwFileSize);
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

        if(g_fDoBurstWrite)
        {
            for(DWORD dwDataTransfer = 1; dwDataTransfer <= dwNumTransfers; dwDataTransfer++)
            {
                // Start the perf logging
                START_PERF();

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

                END_PERF();
                Sleep(g_dwBurstWritePauseMs);
            }

            START_PERF();
            CLOSE_FILE_END_PERF(hFile);
        }
        else
        {
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
    }

    // success
    dwRet = TPR_PASS;

Error:
    // Close handle, free memory, clean up
    CHECK_END_PERF(pCPerflog);
    CHECK_CLOSE_HANDLE(hFile);
    CHECK_LOCAL_FREE(pBuffer);
    CHECK_FREE(pszPerfMarkerName);
    CHECK_DELETE_FILE(g_szTestFileName);
    if (CPerfScenario::CheckInit())
    {
        TCHAR szTmp[AUXDATA_BUFF_SIZE] = {0};
        StringCchPrintf(szTmp, _countof(szTmp), L"%d", g_dwDataSize);
        CPerfScenario::AddAuxData(L"BufferSize(bytes)", szTmp);

        StringCchPrintf(szTmp, _countof(szTmp), PERFSCEN_NAME_THROUGHPUT_WR, g_dwDataSize);
        CPerfScenario::DumpLogEx(&g_PERFGUID[ThroughputTestGUIDIndex(lpFTE->dwUniqueID) + TP_WriteFile],
                                 PERFSCEN_NAMESPACE_THROUGHPUT_WR, szTmp);
    }

    return dwRet;
}



// Write out a test file in blocks of specified size
// --------------------------------------------------------------------
TESTPROCAPI Tst_OverWriteFile(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD dwRet = TPR_FAIL;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    LPBYTE pBuffer = NULL;
    DWORD dwBytesTransfered = 0;
    DWORD dwNumTransfers = 0;
    LPTSTR pszPerfMarkerName = NULL;
    CPerfHelper * pCPerflog = NULL;

    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    // Skip if there are no testable paths
    if (!InitFileSys())
    {
        SKIP("The test volume has not been successfully initialized, skipping test case");
        dwRet = TPR_SKIP;
        goto Error;
    }
    else if (!InitPerfLog())
    {
        SKIP("Problem initializing CePerf and PerfScenario. Ensure that both CePerf.dll and PerfScenario.dll are available before running the test");
        dwRet = TPR_SKIP;
        goto Error;
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
    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CPerfScenario::MAX_NAME_LENGTH);
    if (NULL == pszPerfMarkerName)
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Fill the perf strings with test-specific data
    CreateMarkerName(
        pszPerfMarkerName,
        CPerfScenario::MAX_NAME_LENGTH,
        STR_THROUGHPUT);

    // Create perf logger object
    pCPerflog = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_THROUGHPUT, g_dwDataSize, g_dwFileSize);
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

    // Pre-write the test file
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
            GENERIC_WRITE,
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

        if(g_fDoBurstWrite)
        {
            for(DWORD dwDataTransfer = 1; dwDataTransfer <= dwNumTransfers; dwDataTransfer++)
            {
                // Start the perf logging
                START_PERF();

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

                END_PERF();
                Sleep(g_dwBurstWritePauseMs);
            }

            START_PERF();
            CLOSE_FILE_END_PERF(hFile);
        }
        else
        {
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
    }

    // success
    dwRet = TPR_PASS;

Error:
    g_pCFSUtils->DeleteFile(g_szTestFileName);
    // Close handle, free memory, clean up
    CHECK_END_PERF(pCPerflog);
    CHECK_CLOSE_HANDLE(hFile);
    CHECK_LOCAL_FREE(pBuffer);
    CHECK_FREE(pszPerfMarkerName);
    CHECK_DELETE_FILE(g_szTestFileName);
    if (CPerfScenario::CheckInit())
    {
        TCHAR szTmp[AUXDATA_BUFF_SIZE] = {0};
        StringCchPrintf(szTmp, _countof(szTmp), L"%d", g_dwDataSize);
        CPerfScenario::AddAuxData(L"BufferSize(bytes)", szTmp);

        StringCchPrintf(szTmp, _countof(szTmp), PERFSCEN_NAME_THROUGHPUT_OVRWR, g_dwDataSize);
        CPerfScenario::DumpLogEx(&g_PERFGUID[ThroughputTestGUIDIndex(lpFTE->dwUniqueID) + TP_OverWriteFile],
                                 PERFSCEN_NAMESPACE_THROUGHPUT_OVRWR, szTmp);
    }

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
    CPerfHelper * pCPerflog = NULL;
    UINT uNumber = 0;
    UINT uNumber1 = 0;
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    // Skip if there are no testable paths
    if (!InitFileSys())
    {
        SKIP("The test volume has not been successfully initialized, skipping test case");
        dwRet = TPR_SKIP;
        goto Error;
    }
    else if (!InitPerfLog())
    {
        SKIP("Problem initializing CePerf and PerfScenario. Ensure that both CePerf.dll and PerfScenario.dll are available before running the test");
        dwRet = TPR_SKIP;
        goto Error;
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

    // Initialize the random number generator
    if (g_fUseTuxRandomSeed)
    {
        srand((UINT)(((TPS_EXECUTE *)tpParam)->dwRandomSeed));
    }
    else
    {
        srand(0);
    }

    // Allocate string buffers for perf parameters
    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CPerfScenario::MAX_NAME_LENGTH);
    if (NULL == pszPerfMarkerName)
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Fill the perf strings with test-specific data
    CreateMarkerName(
        pszPerfMarkerName,
        CPerfScenario::MAX_NAME_LENGTH,
        STR_THROUGHPUT);

    // Create perf logger object
    pCPerflog = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_THROUGHPUT, g_dwDataSize, g_dwFileSize);
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
        if(g_fDoBurstWrite)
        {
            // Write out file in blocks of specified size
            for(DWORD dwDataTransfer = 1; dwDataTransfer <= dwNumTransfers; dwDataTransfer++)
            {

                // Start the perf logging
                START_PERF();
                rand_s(&uNumber);
                rand_s(&uNumber1);

                // Pick a random place within the file to write to
                dwFileOffset = ((uNumber<<16) + uNumber1) % (g_dwFileSize - g_dwDataSize);

                // Set file pointer at the random location
                if (0xFFFFFFFF == SetFilePointer(hFile, dwFileOffset, NULL, FILE_BEGIN))
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

                END_PERF();
                Sleep(g_dwBurstWritePauseMs);
            }

            START_PERF();
            CLOSE_FILE_END_PERF(hFile);
        }

        else
        {
            // Start the perf logging
            START_PERF();

            // Write out file in blocks of specified size
            for(DWORD dwDataTransfer = 1; dwDataTransfer <= dwNumTransfers; dwDataTransfer++)
            {
                rand_s(&uNumber);
                rand_s(&uNumber1);
                // Pick a random place within the file to write to
                dwFileOffset = ((uNumber<<16) + uNumber1) % (g_dwFileSize - g_dwDataSize);

                // Set file pointer at the random location
                if (0xFFFFFFFF == SetFilePointer(hFile, dwFileOffset, NULL, FILE_BEGIN))
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
    }

    // success
    dwRet = TPR_PASS;

Error:
    // Close handle, free memory, clean up
    CHECK_END_PERF(pCPerflog);
    CHECK_CLOSE_HANDLE(hFile);
    CHECK_LOCAL_FREE(pBuffer);
    CHECK_FREE(pszPerfMarkerName);
    CHECK_DELETE_FILE(g_szTestFileName);

    if (CPerfScenario::CheckInit())
    {
        TCHAR szTmp[AUXDATA_BUFF_SIZE] = {0};
        StringCchPrintf(szTmp, _countof(szTmp), L"%d", g_dwDataSize);
        CPerfScenario::AddAuxData(L"BufferSize(bytes)", szTmp);

        StringCchPrintf(szTmp, _countof(szTmp), PERFSCEN_NAME_THROUGHPUT_WR_RAND, g_dwDataSize);
        CPerfScenario::DumpLogEx(&g_PERFGUID[ThroughputTestGUIDIndex(lpFTE->dwUniqueID) + TP_WriteFileRandom],
                                 PERFSCEN_NAMESPACE_THROUGHPUT_WR_RAND, szTmp);
    }

    return dwRet;
}

// --------------------------------------------------------------------
// Read in a test file in blocks of specified size
// --------------------------------------------------------------------
TESTPROCAPI Tst_ReadFile(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD dwRet = TPR_FAIL;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    LPBYTE pBuffer = NULL;
    DWORD dwBytesTransfered = 0;
    DWORD dwNumTransfers = 0;
    LPTSTR pszPerfMarkerName = NULL;
    CPerfHelper * pCPerflog = NULL;

    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    // Skip if there are no testable paths
    if (!InitFileSys())
    {
        SKIP("The test volume has not been successfully initialized, skipping test case");
        dwRet = TPR_SKIP;
        goto Error;
    }
    else if (!InitPerfLog())
    {
        SKIP("Problem initializing CePerf and PerfScenario. Ensure that both CePerf.dll and PerfScenario.dll are available before running the test");
        dwRet = TPR_SKIP;
        goto Error;
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
    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CPerfScenario::MAX_NAME_LENGTH);
    if ((NULL == pszPerfMarkerName))
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Fill the perf strings with test-specific data
    CreateMarkerName(
        pszPerfMarkerName,
        CPerfScenario::MAX_NAME_LENGTH,
        STR_THROUGHPUT);

    // Create perf logger object
    pCPerflog = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_THROUGHPUT, g_dwDataSize, g_dwFileSize);
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
    CHECK_DELETE_FILE(g_szTestFileName);
    if (CPerfScenario::CheckInit())
    {
        TCHAR szTmp[AUXDATA_BUFF_SIZE] = {0};
        StringCchPrintf(szTmp, _countof(szTmp), L"%d", g_dwDataSize);
        CPerfScenario::AddAuxData(L"BufferSize(bytes)", szTmp);

        StringCchPrintf(szTmp, _countof(szTmp), PERFSCEN_NAME_THROUGHPUT_RD, g_dwDataSize);
        CPerfScenario::DumpLogEx(&g_PERFGUID[ThroughputTestGUIDIndex(lpFTE->dwUniqueID) + TP_ReadFile],
                                 PERFSCEN_NAMESPACE_THROUGHPUT_RD, szTmp);
    }

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
    CPerfHelper * pCPerflog = NULL;
    UINT uNumber = 0;
    UINT uNumber1 = 0;

    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    // Skip if there are no testable paths
    if (!InitFileSys())
    {
        SKIP("The test volume has not been successfully initialized, skipping test case");
        dwRet = TPR_SKIP;
        goto Error;
    }
    else if (!InitPerfLog())
    {
        SKIP("Problem initializing CePerf and PerfScenario. Ensure that both CePerf.dll and PerfScenario.dll are available before running the test");
        dwRet = TPR_SKIP;
        goto Error;
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

    // Initialize the random number generator
    if (g_fUseTuxRandomSeed)
    {
        srand((UINT)(((TPS_EXECUTE *)tpParam)->dwRandomSeed));
    }
    else
    {
        srand(0);
    }

    // Allocate string buffers for perf parameters
    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CPerfScenario::MAX_NAME_LENGTH);
    if ((NULL == pszPerfMarkerName))
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Fill the perf strings with test-specific data
    CreateMarkerName(
        pszPerfMarkerName,
        CPerfScenario::MAX_NAME_LENGTH,
        STR_THROUGHPUT);

    // Create perf logger object
    pCPerflog = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_THROUGHPUT, g_dwDataSize, g_dwFileSize);
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
            rand_s(&uNumber);
            rand_s(&uNumber1);
            // Pick a random place within the file to read from
            dwFileOffset = ((uNumber<<16) + uNumber1) % (g_dwFileSize - g_dwDataSize);

            // Set file pointer at the random location
            if (0xFFFFFFFF == SetFilePointer(hFile, dwFileOffset, NULL, FILE_BEGIN))
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
    CHECK_DELETE_FILE(g_szTestFileName);
    if (CPerfScenario::CheckInit())
    {
        TCHAR szTmp[AUXDATA_BUFF_SIZE] = {0};
        StringCchPrintf(szTmp, _countof(szTmp), L"%d", g_dwDataSize);
        CPerfScenario::AddAuxData(L"BufferSize(bytes)", szTmp);

        StringCchPrintf(szTmp, _countof(szTmp), PERFSCEN_NAME_THROUGHPUT_RD_RAND, g_dwDataSize);
        CPerfScenario::DumpLogEx(&g_PERFGUID[ThroughputTestGUIDIndex(lpFTE->dwUniqueID) + TP_ReadFileRandom],
                                 PERFSCEN_NAMESPACE_THROUGHPUT_RD_RAND, szTmp);
    }

    return dwRet;
}

// --------------------------------------------------------------------
// Read in a test file in blocks of specified size while moving
// backwards through the file
// --------------------------------------------------------------------
TESTPROCAPI Tst_ReverseReadFile(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD dwRet = TPR_FAIL;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    LPBYTE pBuffer = NULL;
    DWORD dwBytesTransfered = 0;
    DWORD dwNumTransfers = 0;
    LPTSTR pszPerfMarkerName = NULL;
    CPerfHelper * pCPerflog = NULL;

    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    // Skip if there are no testable paths
    if (!InitFileSys())
    {
        SKIP("The test volume has not been successfully initialized, skipping test case");
        dwRet = TPR_SKIP;
        goto Error;
    }
    else if (!InitPerfLog())
    {
        SKIP("Problem initializing CePerf and PerfScenario. Ensure that both CePerf.dll and PerfScenario.dll are available before running the test");
        dwRet = TPR_SKIP;
        goto Error;
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
    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CPerfScenario::MAX_NAME_LENGTH);
    if ((NULL == pszPerfMarkerName))
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Fill the perf strings with test-specific data
    CreateMarkerName(
        pszPerfMarkerName,
        CPerfScenario::MAX_NAME_LENGTH,
        STR_THROUGHPUT);

    // Create perf logger object
    pCPerflog = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_THROUGHPUT, g_dwDataSize, g_dwFileSize);
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
    CHECK_DELETE_FILE(g_szTestFileName);

    if (CPerfScenario::CheckInit())
    {
        TCHAR szTmp[AUXDATA_BUFF_SIZE] = {0};
        StringCchPrintf(szTmp, _countof(szTmp), L"%d", g_dwDataSize);
        CPerfScenario::AddAuxData(L"BufferSize(bytes)", szTmp);

        StringCchPrintf(szTmp, _countof(szTmp), PERFSCEN_NAME_THROUGHPUT_RD_REVERSE, g_dwDataSize);
        CPerfScenario::DumpLogEx(&g_PERFGUID[ThroughputTestGUIDIndex(lpFTE->dwUniqueID) + TP_ReverseReadFile],
                                 PERFSCEN_NAMESPACE_THROUGHPUT_RD_REVERSE, szTmp);
    }

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
    CPerfHelper * pCPerflog = NULL;
    UINT uNumber = 0;
    UINT uNumber1 = 0;

    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    // Skip if there are no testable paths
    if (!InitFileSys())
    {
        SKIP("The test volume has not been successfully initialized, skipping test case");
        dwRet = TPR_SKIP;
        goto Error;
    }
    else if (!InitPerfLog())
    {
        SKIP("Problem initializing CePerf and PerfScenario. Ensure that both CePerf.dll and PerfScenario.dll are available before running the test");
        dwRet = TPR_SKIP;
        goto Error;
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

    // Initialize the random number generator
    if (g_fUseTuxRandomSeed)
    {
        srand((UINT)(((TPS_EXECUTE *)tpParam)->dwRandomSeed));
    }
    else
    {
        srand(0);
    }

    // Allocate string buffers for perf parameters
    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CPerfScenario::MAX_NAME_LENGTH);
    if ((NULL == pszPerfMarkerName))
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Fill the perf strings with test-specific data
    CreateMarkerName(
        pszPerfMarkerName,
        CPerfScenario::MAX_NAME_LENGTH,
        _T("Seek Rate (Seeks/Sec)"));

    // Create perf logger object
    pCPerflog = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_THROUGHPUT, 0, dwNumSeeks);
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
            rand_s(&uNumber);
            rand_s(&uNumber1);
            // Pick a random offset to seek to
            dwFileOffset = ((uNumber<<16) + uNumber1) % (g_dwFileSize - dwAccessSize);

            // Set file pointer at the random location
            if (0xFFFFFFFF == SetFilePointer(hFile, dwFileOffset, NULL, FILE_BEGIN))
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
    CHECK_DELETE_FILE(g_szTestFileName);

    if (CPerfScenario::CheckInit())
    {
        TCHAR szTmp[AUXDATA_BUFF_SIZE] = {0};
        StringCchPrintf(szTmp, _countof(szTmp), L"%s", 
                        (lpFTE->dwUniqueID-SEEKSPEED_BASE) ? L"write" : L"read");
        CPerfScenario::AddAuxData(L"Operation", szTmp);

        StringCchPrintf(szTmp, _countof(szTmp), PERFSCEN_NAME_SEEKSPEED, 
                        (lpFTE->dwUniqueID-SEEKSPEED_BASE) ? L"write" : L"read");
        CPerfScenario::DumpLogEx(&g_PERFGUID[(lpFTE->dwUniqueID-SEEKSPEED_BASE) + GUID_OFFSET_SEEKSPEED],
                                 PERFSCEN_NAMESPACE_SEEKSPEED, szTmp);
    }

    return dwRet;
}


// --------------------------------------------------------------------
// Create a directory tree n levels deep
// --------------------------------------------------------------------
TESTPROCAPI Tst_CreateDeepTree(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD dwRet = TPR_FAIL;
    DWORD dwTreeDepth = 0;
    TCHAR szPath[MAX_PATH] = _T("");
    LPTSTR pszPerfMarkerName = NULL;
    CPerfHelper * pCPerflog = NULL;

    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    // Skip if there are no testable paths
    if (!InitFileSys())
    {
        SKIP("The test volume has not been successfully initialized, skipping test case");
        dwRet = TPR_SKIP;
        goto Error;
    }
    else if (!InitPerfLog())
    {
        SKIP("Problem initializing CePerf and PerfScenario. Ensure that both CePerf.dll and PerfScenario.dll are available before running the test");
        dwRet = TPR_SKIP;
        goto Error;
    }

    // Read the tree depth parameter
    dwTreeDepth = (DWORD) lpFTE->dwUserData;

    // Allocate string buffers for perf parameters
    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CPerfScenario::MAX_NAME_LENGTH);
    if ((NULL == pszPerfMarkerName))
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Fill the perf strings with test-specific data
    CreateMarkerName(
        pszPerfMarkerName,
        CPerfScenario::MAX_NAME_LENGTH,
        _T("Speed (dirs/s)"));

    // Create perf logger object
    pCPerflog = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_THROUGHPUT, 0, dwTreeDepth);
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
    CHECK_CLEAN_DIRECTORY(g_pCFSUtils, DEFAULT_TEST_DIRECTORY);

    if (CPerfScenario::CheckInit())
    {
        TCHAR szTmp[AUXDATA_BUFF_SIZE] = {0};
        StringCchPrintf(szTmp, _countof(szTmp), L"%d", dwTreeDepth);
        CPerfScenario::AddAuxData(L"Depth(levels)", szTmp);
        
        StringCchPrintf(szTmp, _countof(szTmp), PERFSCEN_NAME_DEEPTREE, dwTreeDepth);
        CPerfScenario::DumpLogEx(&g_PERFGUID[(lpFTE->dwUniqueID-CREATEDEEPTREE_BASE) + GUID_OFFSET_DEEPTREE],
                                 PERFSCEN_NAMESPACE_DEEPTREE, szTmp);
    }

    return dwRet;
}

// --------------------------------------------------------------------
// Test the time it takes to create a large file with SetEndOfFile
// --------------------------------------------------------------------
TESTPROCAPI Tst_SetEndOfFile(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD dwRet = TPR_FAIL;
    DWORD dwFileSize = 0;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    LPTSTR pszPerfMarkerName = NULL;
    CPerfHelper * pCPerflog = NULL;

    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    // Skip if there are no testable paths
    if (!InitFileSys())
    {
        SKIP("The test volume has not been successfully initialized, skipping test case");
        dwRet = TPR_SKIP;
        goto Error;
    }
    else if (!InitPerfLog())
    {
        SKIP("Problem initializing CePerf and PerfScenario. Ensure that both CePerf.dll and PerfScenario.dll are available before running the test");
        dwRet = TPR_SKIP;
        goto Error;
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
    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CPerfScenario::MAX_NAME_LENGTH);
    if ((NULL == pszPerfMarkerName))
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Fill the perf strings with test-specific data
    CreateMarkerName(
        pszPerfMarkerName,
        CPerfScenario::MAX_NAME_LENGTH,
        STR_THROUGHPUT);

    // Create perf logger object
    pCPerflog = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_THROUGHPUT, 0, dwFileSize);
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
    CHECK_DELETE_FILE(g_szTestFileName);

    if (CPerfScenario::CheckInit())
    {
        TCHAR szTmp[AUXDATA_BUFF_SIZE] = {0};
        StringCchPrintf(szTmp, _countof(szTmp), L"%d", dwFileSize);
        CPerfScenario::AddAuxData(L"FileSize(bytes)", szTmp);
        
        StringCchPrintf(szTmp, _countof(szTmp), PERFSCEN_NAME_SETENDOFFILE, dwFileSize);
        CPerfScenario::DumpLogEx(&g_PERFGUID[(lpFTE->dwUniqueID-SETENDOFFILE_BASE) + GUID_OFFSET_SETENDOFFILE],
                                 PERFSCEN_NAMESPACE_SETENDOFFILE, szTmp);
    }

    return dwRet;
}

// --------------------------------------------------------------------
// Copies files between root directory and test path
// --------------------------------------------------------------------
TESTPROCAPI Tst_CopyFiles(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD dwRet = TPR_FAIL;
    LPTSTR pszPerfMarkerName = NULL;
    BOOL fStorageToRoot = FALSE;
    CPerfHelper * pCPerflog = NULL;
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
    if (!InitFileSys())
    {
        SKIP("The test volume has not been successfully initialized, skipping test case");
        dwRet = TPR_SKIP;
        goto Error;
    }
    else if (!InitPerfLog())
    {
        SKIP("Problem initializing CePerf and PerfScenario. Ensure that both CePerf.dll and PerfScenario.dll are available before running the test");
        dwRet = TPR_SKIP;
        goto Error;
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
    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CPerfScenario::MAX_NAME_LENGTH);
    if ((NULL == pszPerfMarkerName))
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Fill the perf strings with test-specific data
    CreateMarkerName(
        pszPerfMarkerName,
        CPerfScenario::MAX_NAME_LENGTH,
        STR_THROUGHPUT);

    // Create perf logger object
    pCPerflog = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_THROUGHPUT, 0, DEFAULT_FILE_SET_SIZE);
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

    if (CPerfScenario::CheckInit())
    {
       TCHAR szTmp[AUXDATA_BUFF_SIZE] = {0};
       StringCchPrintf(szTmp, _countof(szTmp), L"%s", 
                       fStorageToRoot ? L"from storage to device root" : L"from device root to storage");
       CPerfScenario::AddAuxData(L"Direction", szTmp);
      

       StringCchPrintf(szTmp, _countof(szTmp), PERFSCEN_NAME_COPYFILES, 
                       fStorageToRoot ? L"from storage to device root" : L"from device root to storage" );
       CPerfScenario::DumpLogEx(&g_PERFGUID[(lpFTE->dwUniqueID-COPYFILES_BASE) + GUID_OFFSET_COPYFILES],
                                PERFSCEN_NAMESPACE_COPYFILES, szTmp);
    }

    return dwRet;
}

// --------------------------------------------------------------------
// Reads and appends data to a large fragmented file
// --------------------------------------------------------------------
TESTPROCAPI Tst_ReadAppendFragmentedFile(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD dwRet = TPR_FAIL;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    LPBYTE pBuffer = NULL;
    DWORD dwBytesTransfered = 0;
    DWORD dwNumTransfers = 0;
    DWORD dwInitialFileSize = 0;
    DWORD dwTotalDataSize = 0;
    LPTSTR pszPerfMarkerName = NULL;
    CPerfHelper * pCPerflog = NULL;

    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    // Skip if there are no testable paths
    if (!InitFileSys())
    {
        SKIP("The test volume has not been successfully initialized, skipping test case");
        dwRet = TPR_SKIP;
        goto Error;
    }
    else if (!InitPerfLog())
    {
        SKIP("Problem initializing CePerf and PerfScenario. Ensure that both CePerf.dll and PerfScenario.dll are available before running the test");
        dwRet = TPR_SKIP;
        goto Error;
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
    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CPerfScenario::MAX_NAME_LENGTH);
    if ((NULL == pszPerfMarkerName))
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Fill the perf strings with test-specific data
    CreateMarkerName(
        pszPerfMarkerName,
        CPerfScenario::MAX_NAME_LENGTH,
        STR_THROUGHPUT);

    // Create perf logger object
    pCPerflog = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_THROUGHPUT, g_dwDataSize, dwTotalDataSize);
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
    CHECK_DELETE_FILE(g_szTestFileName);

    if (CPerfScenario::CheckInit())
    {
        TCHAR szTmp[AUXDATA_BUFF_SIZE] = {0};
        StringCchPrintf(szTmp, _countof(szTmp), L"%d", dwTotalDataSize);
        CPerfScenario::AddAuxData(L"TotalDataTransferSize(bytes)", szTmp);
        
        StringCchPrintf(szTmp, _countof(szTmp), PERFSCEN_NAME_READAPPENDFRAG, dwTotalDataSize);
        CPerfScenario::DumpLogEx(&g_PERFGUID[(lpFTE->dwUniqueID-READAPPENDFRAG_BASE) + GUID_OFFSET_READAPPENDFRAG],
                                 PERFSCEN_NAMESPACE_READAPPENDFRAG, szTmp);
    }

    return dwRet;
}

//-------------------------------------
// Tests the Performance of findfirst/Next for same type of files in a directory
//one file match.
//all matching files.
//-------------------------------------

TESTPROCAPI Tst_FindFirstNext(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD   dwRet = TPR_FAIL;
    DWORD   dwTotFiles=0;
    DWORD   i=0;
    DWORD   dwFileSize=0;
    DWORD   dwTotalFileSize=0;
    TCHAR   pszPath[MAX_PATH]={0};
    TCHAR   pszRootPath[MAX_PATH]= {0};
    TCHAR   szBuf[MAX_PATH] ={0};
    TCHAR   newFileName[MAX_PATH] = _T("");
    CPerfHelper * pCPerflog = NULL;
    LPTSTR   pszPerfMarkerName = NULL;
    size_t  ccLen = 0;

    // The shell doesn't necessarily want us to execute the test. Make sure first
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    // Skip if there are no testable paths
    if (!InitFileSys())
    {
        SKIP("The test volume has not been successfully initialized, skipping test case");
        dwRet = TPR_SKIP;
        goto Error;
    }

    //Read the Total Files Parameter to perform FindFileFirst/Next.
    dwTotFiles = (DWORD) lpFTE->dwUserData;

    //Delete directory if already exists.
    if (!g_pCFSUtils->CleanDirectory(DEFAULT_TEST_DIRECTORY, TRUE))
    {
        ERRFAIL("CleanTestDirectory()");
        goto Error;
    }

    //Create Directory.
    if (!g_pCFSUtils->CreateDirectory(DEFAULT_TEST_DIRECTORY))
    {
        ERRFAIL("CreateDirectory()");
        goto Error;
    }
    COMMENT("Directory Created.");
    dwTotalFileSize=DEFAULT_FILE_SET_SIZE;
    dwFileSize = dwTotalFileSize / dwTotFiles;

    for (DWORD dwI = 1; dwI <= dwTotFiles; dwI++)
    {
        VERIFY(SUCCEEDED(StringCchPrintf(newFileName,
            MAX_PATH,
            _T("%s\\%s%u.%s"),
            DEFAULT_TEST_DIRECTORY,
            DEFAULT_TEST_FILE_NAME,
            dwI,DEFAULT_TEST_FIND_FILE)));

        if (!g_pCFSUtils -> CreateTestFile(newFileName, dwFileSize, FALSE))
        {
            if(GetLastError() == ERROR_DISK_FULL)
                break;
            FAIL("CFSUtils::CreateTestFileSet: CreateTestFile() failed");
            goto Error;
        }

    }
    dwTotFiles = dwI-1; //some time because of disk full we might not be creating number of files we wished
    COMMENT("Created files.");

    
    // Allocate string buffers for perf parameters
    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CPerfScenario::MAX_NAME_LENGTH);
    if (NULL == pszPerfMarkerName)
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }
    
    if(g_pCFSUtils->GetPath(pszRootPath,MAX_PATH)==NULL || pszRootPath == NULL)
    {
        FAIL("g_pCFSUtils->GetPath() return invalid path");
        goto Error;
    }


    //Begin to search
    for (i=0;i<=2;i++)
    {

        //Construct the path we want for directory
        StringCchLength(pszRootPath, MAX_PATH, &ccLen);
        if(ccLen>=2)
            StringCchPrintf(pszPath, MAX_PATH-2, TEXT("%s\\%s"),pszRootPath, DEFAULT_TEST_DIRECTORY);
        else
            StringCchPrintf(pszPath, MAX_PATH-2, TEXT("\\%s"),DEFAULT_TEST_DIRECTORY);
        
        // Copy the path to a temp buffer.
        StringCchCopy(szBuf, MAX_PATH, pszPath);
        szBuf[MAX_PATH-1]=_T('\0');
        
        // Get the length of the path.
        StringCchLength(szBuf, MAX_PATH, &ccLen);
        if(ccLen<=0)
        {
            FAIL("Length of the directory path is invalid to search for files");
            goto Error;
        }

        // Ensure the path ends with a wack.
        if (szBuf[ccLen - 1] != TEXT('\\')) 
        {
            szBuf[ccLen++] = TEXT('\\');
            szBuf[ccLen] = TEXT('\0');
        }

        // Locate the end of the path.
        LPTSTR szAppend = szBuf + ccLen;
        StringCchCopy(szAppend, MAX_PATH, FINDFIRST_SEARCH_STRINGS[i]);

        // Set up performance markers
        if (!InitPerfLog())
        {
            ERRFAIL("Problem initializing CePerf and PerfScenario. Ensure that both CePerf.dll and PerfScenario.dll are available before running the test");
            goto Error;
        }

        CreateMarkerName(
            pszPerfMarkerName,
            CPerfScenario::MAX_NAME_LENGTH,
            _T("Speed (files/s)"));
                
        // Create perf logger object
        pCPerflog = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_THROUGHPUT, 0,dwTotFiles );
        if (NULL == pCPerflog)
        {
            ERRFAIL("Out of memory!");
            goto Error;
        }

        START_PERF();

        // Start the file/subdirectory find.
        WIN32_FIND_DATA w32fd;
        HANDLE hFind = FindFirstFile(szBuf, &w32fd);

        // Loop on each file/subdirectory in this directory.
        if (hFind != INVALID_HANDLE_VALUE) 
        {
            do 
            {
                // Check to see if the find is a directory.
                if (!(w32fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
                {
                    // Append the file to our path.
                    StringCchCopy(szAppend, MAX_PATH, w32fd.cFileName);
                }

                // Get next file/directory
            } while (FindNextFile(hFind, &w32fd));

            // Close our find.
            if(!FindClose(hFind))
            {
                ERRFAIL("FindClose()");
                goto Error;
            }
        }
        else
        {
            ERRFAIL("FindFirstFile()");
            goto Error;
            
        }
        CHECK_END_PERF(pCPerflog);

        // Store the result
        if (CPerfScenario::CheckInit())
        {
            TCHAR szTmp[AUXDATA_BUFF_SIZE] = {0};
            CPerfScenario::AddAuxData(L"SearchString", FINDFIRST_SEARCH_STRINGS[i]);
            StringCchPrintf(szTmp, _countof(szTmp), L"%d", dwTotFiles);
            CPerfScenario::AddAuxData(L"TotalNumFiles", szTmp);
                               
            StringCchPrintf(szTmp, _countof(szTmp), PERFSCEN_NAME_FINDFIRST, FINDFIRST_SEARCH_STRINGS[i], dwTotFiles);
            CPerfScenario::DumpLogEx(&g_PERFGUID[(lpFTE->dwUniqueID-FINDFIRSTNEXT_BASE)*_countof(FINDFIRST_SEARCH_STRINGS) + GUID_OFFSET_FINDFIRST + i],
                                     PERFSCEN_NAMESPACE_FINDFIRST, szTmp);
        }
    }

    //Clean up the directory
    if (!g_pCFSUtils->CleanDirectory(DEFAULT_TEST_DIRECTORY, FALSE))
    {
        ERRFAIL("CleanTree()");
        goto Error;
    }

    dwRet =  TPR_PASS;
    
Error:
    if(dwRet != TPR_SKIP) //We dont need to all this in case test is being skipped
    {
        g_pCFSUtils->CleanDirectory(DEFAULT_TEST_DIRECTORY, FALSE);
        CHECK_END_PERF(pCPerflog);
        CHECK_FREE(pszPerfMarkerName);
    }

    return dwRet;
}
//----------------------------------------------------------------
//Find file First/Next implemented for different types of files in a directory.
//----------------------------------------------------------------
TESTPROCAPI Tst_FindFirstNextRandom(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD   dwRet = TPR_FAIL;
    DWORD   dwTotFiles=0;
    TCHAR   pszPath[MAX_PATH]={0};
    TCHAR   pszRootPath[MAX_PATH]= {0};
    TCHAR   szBuf[MAX_PATH] ={0};
    TCHAR   newFileName[MAX_PATH] = _T("");
    LPBYTE   pBuffer = NULL;
    CPerfHelper * pCPerflog = NULL;
    LPTSTR   pszPerfMarkerName = NULL;
    size_t   ccLen = 0;

    // The shell doesn't necessarily want us to execute the test. Make sure first
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    // Skip if there are no testable paths
    if (!InitFileSys())
    {
        SKIP("The test volume has not been successfully initialized, skipping test case");
        dwRet = TPR_SKIP;
        goto Error;
    }

    //Read the Total Files Parameter to perform FindFileFirst/Next.
    dwTotFiles = (DWORD) lpFTE->dwUserData;

    //Delete directory if already exists.
    if (!g_pCFSUtils->CleanDirectory(DEFAULT_TEST_DIRECTORY, TRUE))
    {
        ERRFAIL("CleanTestDirectory()");
        goto Error;
    }

    //Create Directory.
    if (!g_pCFSUtils->CreateDirectory(DEFAULT_TEST_DIRECTORY))
    {
        ERRFAIL("CreateDirectory()");
        goto Error;
    }

    COMMENT("Directory Created.");

    //Create Files such that every 10th file is of our search Pattern.
    for (DWORD dwFileNo=1;dwFileNo<=dwTotFiles;dwFileNo++)
    {
        if(dwFileNo%10 == 0)
        {
            VERIFY(SUCCEEDED(StringCchPrintf(newFileName,MAX_PATH,
                _T("%s\\%s%u.%s"),
                DEFAULT_TEST_DIRECTORY,
                DEFAULT_TEST_FILE_NAME,
                dwFileNo,DEFAULT_TEST_FIND_FILE)));

            if (!g_pCFSUtils->CreateTestFile(newFileName, DEFAULT_SEARCH_FILE_SIZE, FALSE))
            {
                if(GetLastError() == ERROR_DISK_FULL)
                        break;
                FAIL("CFSUtils::CreateTestFileSet: CreateTestFile() failed");
                goto Error;
            }
            continue;
        }
        VERIFY(SUCCEEDED(StringCchPrintf(newFileName,MAX_PATH,
                _T("%s\\%s.%u"),
                DEFAULT_TEST_DIRECTORY,
                DEFAULT_TEST_FILE_NAME,
                dwFileNo)));

        if (!g_pCFSUtils->CreateTestFile(newFileName, DEFAULT_SEARCH_FILE_SIZE, FALSE))
        {
            if(GetLastError() == ERROR_DISK_FULL)
                break;
            FAIL("CFSUtils::CreateTestFileSet: CreateTestFile() failed");
            goto Error;
        }

    }
    COMMENT("Created files.");
    dwTotFiles = dwFileNo-1; //some time because of disk full we might not be creating number of files we wished

    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CPerfScenario::MAX_NAME_LENGTH);
    if (NULL == pszPerfMarkerName)
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }
    if(g_pCFSUtils->GetPath(pszRootPath,MAX_PATH)==NULL)
    {
        FAIL("g_pCFSUtils->GetPath() return invalid path");
        goto Error;
    }
    //Begin to search
    for (int i=0;i<=2;i++)
    {
        //Construct the path we want for directory
        StringCchLength(pszRootPath, MAX_PATH, &ccLen);   
        if(ccLen>=2)
            StringCchPrintf(pszPath, MAX_PATH-2, TEXT("%s\\%s"),pszRootPath, DEFAULT_TEST_DIRECTORY);
        else
            StringCchPrintf(pszPath, MAX_PATH-2, TEXT("\\%s"),DEFAULT_TEST_DIRECTORY);

        // Copy the path to a temp buffer.
        StringCchCopy(szBuf, MAX_PATH, pszPath);
        szBuf[MAX_PATH-1]=_T('\0');

        // Get the length of the path.
        StringCchLength(szBuf, MAX_PATH, &ccLen);      
        if(ccLen<=0)
        {
            FAIL("Length of the directory path is invalid to search for files");
            goto Error;
        }

        // Ensure the path ends with a wack.
        if (szBuf[ccLen - 1] != TEXT('\\')) 
        {
            szBuf[ccLen++] = TEXT('\\');
            szBuf[ccLen] = TEXT('\0');
        }

        // Locate the end of the path.
        LPTSTR szAppend = szBuf + ccLen;
        StringCchCopy(szAppend, MAX_PATH, FINDFIRST_SEARCH_STRINGS[i]);

        // Set up performance markers
        if (!InitPerfLog())
        {
            ERRFAIL("Problem initializing CePerf and PerfScenario. Ensure that both CePerf.dll and PerfScenario.dll are available before running the test");
            dwRet = TPR_FAIL;
            goto Error;
        }

        CreateMarkerName(
            pszPerfMarkerName,
            CPerfScenario::MAX_NAME_LENGTH,
            _T("Speed (files/s)"));

        // Create perf logger object
        pCPerflog = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_THROUGHPUT, 0,dwTotFiles );
        if (NULL == pCPerflog)
        {
            ERRFAIL("Out of memory!");
            goto Error;
        }

        START_PERF();

        // Start the file/subdirectory find.
        WIN32_FIND_DATA w32fd;
        HANDLE hFind = FindFirstFile(szBuf, &w32fd);

        // Loop on each file/subdirectory in this directory.
        if (hFind != INVALID_HANDLE_VALUE) 
        {
            do 
            {
                // Check to see if the find is a directory.
                if (!(w32fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
                {
                    // Append the file to our path.
                    StringCchCopy(szAppend, MAX_PATH, w32fd.cFileName);
                }
                // Get next file/directory
            } while (FindNextFile(hFind, &w32fd));

            // Close our find.
            if(!FindClose(hFind))
            {
                ERRFAIL("FindClose()");
                goto Error;
            }
        }
        CHECK_END_PERF(pCPerflog);

        // Store the result
        if (CPerfScenario::CheckInit())
        {
            TCHAR szTmp[AUXDATA_BUFF_SIZE] = {0};
            CPerfScenario::AddAuxData(L"SearchString", FINDFIRST_SEARCH_STRINGS[i]);
            StringCchPrintf(szTmp, _countof(szTmp), L"%d", dwTotFiles);
            CPerfScenario::AddAuxData(L"TotalNumFiles", szTmp);

            StringCchPrintf(szTmp, _countof(szTmp), PERFSCEN_NAME_FINDFIRSTRAND, FINDFIRST_SEARCH_STRINGS[i], dwTotFiles);
            CPerfScenario::DumpLogEx(&g_PERFGUID[(lpFTE->dwUniqueID-FINDFIRSTNEXTRAND_BASE)*_countof(FINDFIRST_SEARCH_STRINGS) + GUID_OFFSET_FINDFIRSTRAND + i],
                                     PERFSCEN_NAMESPACE_FINDFIRSTRAND, szTmp);
        }
    }

    //Clean up the directory
    if (!g_pCFSUtils->CleanDirectory(DEFAULT_TEST_DIRECTORY, FALSE))
    {
        ERRFAIL("CleanTree()");
        goto Error;
    }
    
    dwRet =  TPR_PASS;

Error:
    // Close handle, free memory, clean up
    if(dwRet != TPR_SKIP)
    {
        g_pCFSUtils->CleanDirectory(DEFAULT_TEST_DIRECTORY, FALSE);
        CHECK_END_PERF(pCPerflog);
        CHECK_LOCAL_FREE(pBuffer);
        CHECK_FREE(pszPerfMarkerName);
    }

    return dwRet;
}

// --------------------------------------------------------------------
// Create a files with different depth across different directories
// --------------------------------------------------------------------
TESTPROCAPI Tst_CreateFileNew(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD dwRet = TPR_FAIL;
    DWORD dwNoFiles = 0;
    TCHAR szPath[MAX_PATH] = _T("");
    TCHAR szDirName[MAX_PATH] = _T("");
    TCHAR szDirPath[MAX_PATH] = _T("");
    LPTSTR pszPerfMarkerName = NULL;
    CPerfHelper * pCPerflog = NULL;
    DWORD   dwTreeDepth = g_dwNumTestIterations; //create no of iterations of nested dirs
    
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    // Skip if there are no testable paths
    if (!InitFileSys())
    {
        SKIP("The test volume has not been successfully initialized, skipping test case");
        dwRet = TPR_SKIP;
        goto Error;
    }
    else if (!InitPerfLog())
    {
        SKIP("Problem initializing CePerf and PerfScenario. Ensure that both CePerf.dll and PerfScenario.dll are available before running the test");
        dwRet = TPR_SKIP;
        goto Error;
    }

    // Read the No of Files
    dwNoFiles = (DWORD) lpFTE->dwUserData;

    // Allocate string buffers for perf parameters
    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CPerfScenario::MAX_NAME_LENGTH);
    if (NULL == pszPerfMarkerName )
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Fill the perf strings with test-specific data
    CreateMarkerName(
        pszPerfMarkerName,
        CPerfScenario::MAX_NAME_LENGTH,
        _T("Speed (files/s)"));


    // Create perf logger object
    pCPerflog = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_THROUGHPUT, 0, dwNoFiles);
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
    
    // Copy test root into path variable
    VERIFY(SUCCEEDED(StringCchPrintf(szPath,MAX_PATH,_T("%s"),DEFAULT_TEST_DIRECTORY)));
    // Create a vertical tree g_dwFileSize deep
    for (DWORD dwTree = 1; dwTree <=dwTreeDepth; dwTree++)
    {
        // Append another directory to path
        VERIFY(SUCCEEDED(StringCchCat(szPath,MAX_PATH,DEFAULT_DIRECTORY_NAME)));
        // Create a directory
        if (!g_pCFSUtils->CreateDirectory(szPath))
        {
            ERRFAIL("CreateDirectory()");
            goto Error;
        }

        // Start the perf logging
        START_PERF();
        for(DWORD dwDir=1;dwDir<=dwTree;dwDir++) //created different directories as per no of nested dirs
        {

            VERIFY(SUCCEEDED(StringCchPrintf(szDirName,MAX_PATH,_T("%s%u"),DEFAULT_DIRECTORY_NAME,dwDir)));
            VERIFY(SUCCEEDED(StringCchPrintf(szDirPath,MAX_PATH,_T("%s\\%s"),szPath,szDirName)));
            // Create a directory
            if (!g_pCFSUtils->CreateDirectory(szDirPath))
            {
                ERRFAIL("CreateDirectory()");
                goto Error;
            }

            // Populate src directory with test files
            if (!g_pCFSUtils->CreateTestFileSetNoWrite(szDirPath, dwNoFiles, CREATE_NEW))
            {
                FAIL("CreateTestFileSet()");
                goto Error;
            }
        }
        END_PERF();

    }
    CHECK_END_PERF(pCPerflog);
    if (!g_pCFSUtils->CleanDirectory(DEFAULT_TEST_DIRECTORY, FALSE))
    {
        ERRFAIL("CleanDirectory()");
        goto Error;
    }

    // success
    dwRet = TPR_PASS;

Error:
    // Cleanup
    CHECK_END_PERF(pCPerflog);
    CHECK_FREE(pszPerfMarkerName);
    CHECK_CLEAN_DIRECTORY(g_pCFSUtils, DEFAULT_TEST_DIRECTORY);

    if (CPerfScenario::CheckInit())
    {
        TCHAR szTmp[AUXDATA_BUFF_SIZE] = {0};
        StringCchPrintf(szTmp, _countof(szTmp), L"%d", dwNoFiles);
        CPerfScenario::AddAuxData(L"TotalNumFiles", szTmp);
     
        StringCchPrintf(szTmp, _countof(szTmp), PERFSCEN_NAME_CREATEFILENEW, dwNoFiles);
        CPerfScenario::DumpLogEx(&g_PERFGUID[(lpFTE->dwUniqueID-CREATEFILENEW_BASE) + GUID_OFFSET_CREATEFILENEW],
                                 PERFSCEN_NAMESPACE_CREATEFILENEW, szTmp);
    }

    return dwRet;
}
// --------------------------------------------------------------------
// First Create a files with different depth across different directories and them measure CreateFile with OPEN_EXISTING
// --------------------------------------------------------------------
TESTPROCAPI Tst_CreateFileExisting(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD dwRet = TPR_FAIL;
    DWORD dwNoFiles = 0;
    TCHAR szPath[MAX_PATH] = _T("");
    TCHAR szDirName[MAX_PATH] = _T("");
    TCHAR szDirPath[MAX_PATH] = _T("");
    LPTSTR pszPerfMarkerName = NULL;
    CPerfHelper * pCPerflog = NULL;
    DWORD   dwTreeDepth = g_dwNumTestIterations; //create no of iterations of nested dirs
    
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    // Skip if there are no testable paths
    if (!InitFileSys())
    {
        SKIP("The test volume has not been successfully initialized, skipping test case");
        dwRet = TPR_SKIP;
        goto Error;
    }
    else if (!InitPerfLog())
    {
        SKIP("Problem initializing CePerf and PerfScenario. Ensure that both CePerf.dll and PerfScenario.dll are available before running the test");
        dwRet = TPR_SKIP;
        goto Error;
    }

    // Read the No of Files
    dwNoFiles = (DWORD) lpFTE->dwUserData;

    // Allocate string buffers for perf parameters
    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CPerfScenario::MAX_NAME_LENGTH);
    if (NULL == pszPerfMarkerName)
    {
        ERRFAIL("Out of memory!");
        goto Error;
    }

    // Fill the perf strings with test-specific data
    CreateMarkerName(
        pszPerfMarkerName,
        CPerfScenario::MAX_NAME_LENGTH,
        _T("Speed (files/s)"));

    // Create perf logger object
    pCPerflog = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_THROUGHPUT, 0, dwNoFiles);
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
    
    // Copy test root into path variable
    VERIFY(SUCCEEDED(StringCchPrintf(szPath,MAX_PATH,_T("%s"),DEFAULT_TEST_DIRECTORY)));
    // Create a vertical tree g_dwFileSize deep
    for (DWORD dwTree = 1; dwTree <=dwTreeDepth; dwTree++)
    {
        // Append another directory to path
        VERIFY(SUCCEEDED(StringCchCat(szPath,MAX_PATH,DEFAULT_DIRECTORY_NAME)));
        // Create a directory
        if (!g_pCFSUtils->CreateDirectory(szPath))
        {
            ERRFAIL("CreateDirectory()");
            goto Error;
        }

        for(DWORD dwDir=1;dwDir<=dwTree;dwDir++) //created different directories as per no of nested dirs
        {
            VERIFY(SUCCEEDED(StringCchPrintf(szDirName,MAX_PATH,_T("%s%u"),DEFAULT_DIRECTORY_NAME,dwDir)));
            VERIFY(SUCCEEDED(StringCchPrintf(szDirPath,MAX_PATH,_T("%s\\%s"),szPath,szDirName)));
            // Create a directory
            if (!g_pCFSUtils->CreateDirectory(szDirPath))
            {
                ERRFAIL("CreateDirectory()");
                goto Error;
            }

            // Populate src directory with test files
            if (!g_pCFSUtils->CreateTestFileSetNoWrite(szDirPath, dwNoFiles, CREATE_NEW))
            {
                FAIL("CreateTestFileSet()");
                goto Error;
            }
        }

    }

    // Copy test root into path variable
    VERIFY(SUCCEEDED(StringCchPrintf(szPath,MAX_PATH,_T("%s"),DEFAULT_TEST_DIRECTORY)));
    // Create a vertical tree g_dwFileSize deep
    for (DWORD dwTree = 1; dwTree <=dwTreeDepth; dwTree++)
    {
        // Append another directory to path
        VERIFY(SUCCEEDED(StringCchCat(szPath,MAX_PATH,DEFAULT_DIRECTORY_NAME)));
        // Start the perf logging
        START_PERF();
        for(DWORD dwDir=1;dwDir<=dwTree;dwDir++) //created different directories as per no of nested dirs
        {
            VERIFY(SUCCEEDED(StringCchPrintf(szDirName,MAX_PATH,_T("%s%u"),DEFAULT_DIRECTORY_NAME,dwDir)));
            VERIFY(SUCCEEDED(StringCchPrintf(szDirPath,MAX_PATH,_T("%s\\%s"),szPath,szDirName)));
            
            
            // Populate src directory with test files
            if (!g_pCFSUtils->CreateTestFileSetNoWrite(szDirPath, dwNoFiles, OPEN_EXISTING))
            {
                FAIL("CreateTestFileSet()");
                goto Error;
            }
            
        }
        END_PERF();

    }
    CHECK_END_PERF(pCPerflog); //end the perf logging
    
    if (!g_pCFSUtils->CleanDirectory(DEFAULT_TEST_DIRECTORY, FALSE))
    {
        ERRFAIL("CleanDirectory()");
        goto Error;
    }

    // success
    dwRet = TPR_PASS;

Error:
    // Cleanup
    CHECK_END_PERF(pCPerflog);
    CHECK_FREE(pszPerfMarkerName);
    CHECK_CLEAN_DIRECTORY(g_pCFSUtils, DEFAULT_TEST_DIRECTORY);

    if (CPerfScenario::CheckInit())
    {
        TCHAR szTmp[AUXDATA_BUFF_SIZE] = {0};
        StringCchPrintf(szTmp, _countof(szTmp), L"%d", dwNoFiles);
        CPerfScenario::AddAuxData(L"TotalNumFiles", szTmp);
        
        StringCchPrintf(szTmp, _countof(szTmp), PERFSCEN_NAME_CREATEFILEEXISTING, dwNoFiles);
        CPerfScenario::DumpLogEx(&g_PERFGUID[(lpFTE->dwUniqueID-CREATEFILEEXISTING_BASE) + GUID_OFFSET_CREATEFILEEXISTING],
                                 PERFSCEN_NAMESPACE_CREATEFILEEXISTING, szTmp);
    }

    return dwRet;
}