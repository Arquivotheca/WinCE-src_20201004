//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "tuxmain.h"
#include "fathlp.h"
#include "mfs.h"
#include "storeutil.h"
#include "legacy.h"
#include "testproc.h"

#define BVT_FILE_SIZE       256
#define ITERATIONS          128

/****************************************************************************

    FUNCTION:   BVT1 (FFBVT Case 1)

    PURPOSE:    To test creating, writing to, reading from, and deleting
                a 256 byte file.
    
    NOTE:       This tests a minimum of the following conditions:

                1.  Creating an 8.3 directory off the root.
                2.  Creating a file in this directory that is 256 bytes.  
                    This also gets read back and the data is compared with 
                    the original data.
                3.  Setting file pointer to the beginning of the file.
                4.  Deleting normal files.
                5.  Deleting directory off the root.
                
****************************************************************************/
TESTPROCAPI Tst_BVT1(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;

    DWORD i, dwWritten, dwRead;
    TCHAR szPathBuf[MAX_PATH], szFileName[MAX_PATH];
    BYTE buffer1[BVT_FILE_SIZE], buffer2[BVT_FILE_SIZE];
    HANDLE hFile = INVALID_HANDLE_VALUE;    
    
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = GetPartitionCount();
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }

    if(0 == GetPartitionCount())
    {
        retVal = TPR_SKIP;
        LOG(_T("There are no [%s] stores with [%s] partitions to test, skipping"),
            g_testOptions.szProfile, g_testOptions.szFilesystem);
        goto Error;
    }

    if(((TPS_EXECUTE *)tpParam)->dwThreadNumber > GetPartitionCount())
    {
       retVal = TPR_PASS;
       LOG(_T("No partition for thread %u to test"), ((TPS_EXECUTE *)tpParam)->dwThreadNumber);
       goto Error;
    }

    pMtdPart = GetPartition(((TPS_EXECUTE *)tpParam)->dwThreadNumber-1);
    if(NULL == pMtdPart)
    {
        FAIL("GetPartition()");
        goto Error;
    }

    if(g_testOptions.fFormat && !pMtdPart->FormatVolume())
    {
        FAIL("FormatVolume()");
        goto Error;
    }

    if(!CompleteCreateDirectory(pMtdPart->GetTestFilePath(szPathBuf)))
    {
        FAIL("CompleteCreateDirectory()");
        goto Error;
    }

    _stprintf(szFileName, _T("%s\\%s.TST"), szPathBuf, TEST_FILE_NAME);
    LOG(_T("Create test file \"%s\""), szFileName);
    hFile = CreateFile(szFileName, GENERIC_WRITE | GENERIC_READ, 0, NULL, 
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(INVALID_HANDLE(hFile))
    {
        LOG(_T("FAILED CreateFile(%s)"), szFileName);
        ERRFAIL("CreateFile()");
        goto Error;
    }

    // initialize data patter in buffer
    for(i = 0; i < BVT_FILE_SIZE; i++)
        buffer1[i] = (BYTE)i;
    
    // zero out the read buffer
    memset(buffer2, 0, sizeof(buffer2));

    LOG(_T("Write data to file \"%s\""), szFileName);
    if(!WriteFile(hFile, buffer1, sizeof(buffer1), &dwWritten, NULL))
    {
        LOG(_T("FAILED WriteFile(%s)"), szFileName);
        ERRFAIL("WriteFile()");
        goto Error;
    }

    LOG(_T("Set file pointer to beginning of file \"%s\""), szFileName);
    if(0xFFFFFFFF == SetFilePointer(hFile, 0, NULL, FILE_BEGIN))
    {
        LOG(_T("FAILED SetFilePointer(%s)"), szFileName);
        ERRFAIL("SetFilePointer()");
        goto Error;
    }

    LOG(_T("Read data from file \"%s\""), szFileName);
    if(!ReadFile(hFile, buffer2, sizeof(buffer2), &dwRead, NULL))
    {
        LOG(_T("FAILED ReadFile(%s)"), szFileName);
        ERRFAIL("ReadFile()");
        goto Error;
    }

    LOG(_T("Verify data read from file \"%s\""), szFileName);
    if(0 != memcmp(buffer1, buffer2, sizeof(buffer1)))
    {
        LOG(_T("Data comparison failed; read/write mismatch in file \"%s\""),
            szFileName);
        FAIL("Data Mismatch");
        goto Error;
    }

    if(!CloseHandle(hFile))
    {
        ERRFAIL("CloseHandle()");
        retVal = TPR_FAIL;
    }
    hFile = INVALID_HANDLE_VALUE;

    if(!DeleteFile(szFileName))
    {
        LOG(_T("FAILED DeleteFile(%s)"), szFileName);
        ERRFAIL("DeleteFile()");
        goto Error;
    }
        
    // success
    retVal = TPR_PASS;
    
Error:

    if(VALID_HANDLE(hFile) && !CloseHandle(hFile))
    {
        ERRFAIL("CloseHandle()");
        retVal = TPR_FAIL;
    }

    if(pMtdPart && !DeleteTree(pMtdPart->GetTestFilePath(szPathBuf), FALSE))
    {
        LOG(_T("Unable to remove \"%s\""), szPathBuf);
        FAIL("DeleteTree()");
        retVal = TPR_FAIL;
    }
    
    return retVal;
}

/****************************************************************************

    FUNCTION:   BVT2 (FFBVT Case 2)

    PURPOSE:    To test creating, copying, and deleting a file
    
    NOTE:       This tests a minimum of the following conditions:

                1.  Creating an 8.3 directory off the root.
                2.  Creating a file in this directory.
                3.  Copying the file to a new name in the same directory.
                4.  Deleting both the original and the copied files.
                5.  Deleting directory off the root.
                
****************************************************************************/
TESTPROCAPI Tst_BVT2(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;

    TCHAR szPathBuf[MAX_PATH], szFile1[MAX_PATH], szFile2[MAX_PATH];
    HANDLE hFile = INVALID_HANDLE_VALUE;
    
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = GetPartitionCount();
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }

    if(0 == GetPartitionCount())
    {
        retVal = TPR_SKIP;
        LOG(_T("There are no [%s] stores with [%s] partitions to test, skipping"),
            g_testOptions.szProfile, g_testOptions.szFilesystem);
        goto Error;
    }

    if(((TPS_EXECUTE *)tpParam)->dwThreadNumber > GetPartitionCount())
    {
       retVal = TPR_PASS;
       LOG(_T("No partition for thread %u to test"), ((TPS_EXECUTE *)tpParam)->dwThreadNumber);
       goto Error;
    }

    pMtdPart = GetPartition(((TPS_EXECUTE *)tpParam)->dwThreadNumber-1);
    if(NULL == pMtdPart)
    {
        FAIL("GetPartition()");
        goto Error;
    }

    if(g_testOptions.fFormat && !pMtdPart->FormatVolume())
    {
        FAIL("FormatVolume()");
        goto Error;
    }

    if(!CompleteCreateDirectory(pMtdPart->GetTestFilePath(szPathBuf)))
    {
        FAIL("CompleteCreateDirectory()");
        goto Error;
    }

    _stprintf(szFile1, _T("%s\\%s1.TST"), szPathBuf, TEST_FILE_NAME);
    _stprintf(szFile2, _T("%s\\%s2.TST"), szPathBuf, TEST_FILE_NAME);

    LOG(_T("Create test file \"%s\""), szFile1);
    hFile = CreateFile(szFile1, GENERIC_WRITE | GENERIC_READ, 0, NULL, 
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(INVALID_HANDLE(hFile))
    {
        LOG(_T("FAILED CreateFile(%s)"), szFile1);
        ERRFAIL("CreateFile()");
        goto Error;
    }
    if(!CloseHandle(hFile))
    {
        ERRFAIL("CloseHandle()");
        goto Error;
    }

    LOG(_T("Copy file \"%s\" to \"%s\""), szFile1, szFile2);
    if(!CopyFile(szFile1, szFile2, FALSE))
    {
        LOG(_T("FAILED CopyFile(%s, %s)"), szFile1, szFile2);
        ERRFAIL("CopyFile()");
        goto Error;
    }

    LOG(_T("Delete file \"%s\""), szFile1);
    if(!DeleteFile(szFile1))
    {
        LOG(_T("FAILED DeleteFile(%s)"), szFile1);
        ERRFAIL("DeleteFile()");
        goto Error;
    }
    
    LOG(_T("Delete file \"%s\""), szFile2);
    if(!DeleteFile(szFile2))
    {
        LOG(_T("FAILED DeleteFile(%s)"), szFile2);
        ERRFAIL("DeleteFile()");
        goto Error;
    }
        
    // success
    retVal = TPR_PASS;
    
Error:

    if(pMtdPart && !DeleteTree(pMtdPart->GetTestFilePath(szPathBuf), FALSE))
    {
        LOG(_T("Unable to remove \"%s\""), szPathBuf);
        FAIL("DeleteTree()");
        retVal = TPR_FAIL;
    }
    
    return retVal;
}

/****************************************************************************

    FUNCTION:   BVT3 (FFBVT Case 3)

    PURPOSE:    To test creating, moving, and deleting a file
    
    NOTE:       This tests a minimum of the following conditions:

                1.  Creating an 8.3 directory off the root.
                2.  Creating a file in this directory.
                3.  Move the file to a new name in the same directory.
                4.  Deleting moved file.
                5.  Verify deleting the original file fails.
                6.  Deleting directory off the root.
                
****************************************************************************/
TESTPROCAPI Tst_BVT3(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;

    TCHAR szPathBuf[MAX_PATH], szFile1[MAX_PATH], szFile2[MAX_PATH];
    HANDLE hFile = INVALID_HANDLE_VALUE;
    
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = GetPartitionCount();
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }

    if(0 == GetPartitionCount())
    {
        retVal = TPR_SKIP;
        LOG(_T("There are no [%s] stores with [%s] partitions to test, skipping"),
            g_testOptions.szProfile, g_testOptions.szFilesystem);
        goto Error;
    }

    if(((TPS_EXECUTE *)tpParam)->dwThreadNumber > GetPartitionCount())
    {
       retVal = TPR_PASS;
       LOG(_T("No partition for thread %u to test"), ((TPS_EXECUTE *)tpParam)->dwThreadNumber);
       goto Error;
    }

    pMtdPart = GetPartition(((TPS_EXECUTE *)tpParam)->dwThreadNumber-1);
    if(NULL == pMtdPart)
    {
        FAIL("GetPartition()");
        goto Error;
    }

    if(g_testOptions.fFormat && !pMtdPart->FormatVolume())
    {
        FAIL("FormatVolume()");
        goto Error;
    }

    if(!CompleteCreateDirectory(pMtdPart->GetTestFilePath(szPathBuf)))
    {
        FAIL("CompleteCreateDirectory()");
        goto Error;
    }

    _stprintf(szFile1, _T("%s\\%s1.TST"), szPathBuf, TEST_FILE_NAME);
    _stprintf(szFile2, _T("%s\\%s2.TST"), szPathBuf, TEST_FILE_NAME);

    LOG(_T("Create test file \"%s\""), szFile1);
    hFile = CreateFile(szFile1, GENERIC_WRITE | GENERIC_READ, 0, NULL, 
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(INVALID_HANDLE(hFile))
    {
        LOG(_T("FAILED CreateFile(%s)"), szFile1);
        ERRFAIL("CreateFile()");
        goto Error;
    }
    if(!CloseHandle(hFile))
    {
        ERRFAIL("CloseHandle()");
        goto Error;
    }

    LOG(_T("Move file \"%s\" to \"%s\""), szFile1, szFile2);
    if(!MoveFile(szFile1, szFile2))
    {
        LOG(_T("FAILED CopyFile(%s, %s)"), szFile1, szFile2);
        ERRFAIL("CopyFile()");
        goto Error;
    }
    
    LOG(_T("Delete file \"%s\""), szFile2);
    if(!DeleteFile(szFile2))
    {
        LOG(_T("FAILED DeleteFile(%s)"), szFile2);
        ERRFAIL("DeleteFile()");
        goto Error;
    }

    LOG(_T("Attempt to Delete file \"%s\" (expecting failure)"), szFile1);
    if(DeleteFile(szFile1))
    {
        LOG(_T("Succeeded DeleteFile(%s) when the file should not exist!"), szFile1);
        FAIL("DeleteFile()");
        goto Error;
    }
        
    // success
    retVal = TPR_PASS;
    
Error:

    if(pMtdPart && !DeleteTree(pMtdPart->GetTestFilePath(szPathBuf), FALSE))
    {
        LOG(_T("Unable to remove \"%s\""), szPathBuf);
        FAIL("DeleteTree()");
        retVal = TPR_FAIL;
    }
    
    return retVal;
}

/****************************************************************************

    FUNCTION:   BVT4 (FFBVT Case 4)

    PURPOSE:    To test creating, enumerating, and deleting two files
    
    NOTE:       This tests a minimum of the following conditions:

                1.  Creating an 8.3 directory off the root.
                2.  Creating files in this directory.
                3.  Finding a file with FindFirstFile()
                4.  Finding a file with FindNextFile()
                5.  Deleting the files.
                6.  Deleting directory off the root.
                
****************************************************************************/
TESTPROCAPI Tst_BVT4(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;

    TCHAR szPathBuf[MAX_PATH], szFile1[MAX_PATH], szFile2[MAX_PATH], szPattern[MAX_PATH];
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATA wfd;    
    
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = GetPartitionCount();
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }

    if(0 == GetPartitionCount())
    {
        retVal = TPR_SKIP;
        LOG(_T("There are no [%s] stores with [%s] partitions to test, skipping"),
            g_testOptions.szProfile, g_testOptions.szFilesystem);
        goto Error;
    }

    if(((TPS_EXECUTE *)tpParam)->dwThreadNumber > GetPartitionCount())
    {
       retVal = TPR_PASS;
       LOG(_T("No partition for thread %u to test"), ((TPS_EXECUTE *)tpParam)->dwThreadNumber);
       goto Error;
    }

    pMtdPart = GetPartition(((TPS_EXECUTE *)tpParam)->dwThreadNumber-1);
    if(NULL == pMtdPart)
    {
        FAIL("GetPartition()");
        goto Error;
    }

    if(g_testOptions.fFormat && !pMtdPart->FormatVolume())
    {
        FAIL("FormatVolume()");
        goto Error;
    }

    if(!CompleteCreateDirectory(pMtdPart->GetTestFilePath(szPathBuf)))
    {
        FAIL("CompleteCreateDirectory()");
        goto Error;
    }

    _stprintf(szFile1, _T("%s\\%s1.TST"), szPathBuf, TEST_FILE_NAME);
    _stprintf(szFile2, _T("%s\\%s2.TST"), szPathBuf, TEST_FILE_NAME);

    LOG(_T("Create test file \"%s\""), szFile1);
    hFile = CreateFile(szFile1, GENERIC_WRITE | GENERIC_READ, 0, NULL, 
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(INVALID_HANDLE(hFile))
    {
        LOG(_T("FAILED CreateFile(%s)"), szFile1);
        ERRFAIL("CreateFile()");
        goto Error;
    }
    if(!CloseHandle(hFile))
    {
        ERRFAIL("CloseHandle()");
        goto Error;
    }

    LOG(_T("Create test file \"%s\""), szFile2);
    hFile = CreateFile(szFile2, GENERIC_WRITE | GENERIC_READ, 0, NULL, 
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(INVALID_HANDLE(hFile))
    {
        LOG(_T("FAILED CreateFile(%s)"), szFile2);
        ERRFAIL("CreateFile()");
        goto Error;
    }
    if(!CloseHandle(hFile))
    {
        ERRFAIL("CloseHandle()");
        goto Error;
    }

    LOG(_T("Finding files in \"%s\""), szPathBuf);
    _stprintf(szPattern, _T("%s\\*.TST"), szPathBuf);
    hFind = FindFirstFile(szPattern, &wfd);
    if(INVALID_HANDLE(hFind))
    {
        ERRFAIL("FindFirstFile()");
        goto Error;
    }
    LOG(_T("Found file \"%s\""), wfd.cFileName);
    if(!FindNextFile(hFind, &wfd))
    {
        ERRFAIL("FindNextFile()");
        goto Error;
    }
    LOG(_T("Found file \"%s\""), wfd.cFileName);

    if(!FindClose(hFind))
    {
        ERRFAIL("FindClose()");
        goto Error;
    }

    LOG(_T("Delete file \"%s\""), szFile1);
    if(!DeleteFile(szFile1))
    {
        LOG(_T("FAILED DeleteFile(%s)"), szFile1);
        ERRFAIL("DeleteFile()");
        goto Error;
    }
    
    LOG(_T("Delete file \"%s\""), szFile2);
    if(!DeleteFile(szFile2))
    {
        LOG(_T("FAILED DeleteFile(%s)"), szFile2);
        ERRFAIL("DeleteFile()");
        goto Error;
    }
        
    // success
    retVal = TPR_PASS;
    
Error:

    if(pMtdPart && !DeleteTree(pMtdPart->GetTestFilePath(szPathBuf), TRUE))
    {
        LOG(_T("Unable to remove \"%s\""), szPathBuf);
        FAIL("DeleteTree()");
        retVal = TPR_FAIL;
    }
    
    return retVal;
}

/****************************************************************************

    FUNCTION:   BVT5 (FFBVT Case 5)

    PURPOSE:    To test creating, enumerating, and deleting two files
    
    NOTE:       This tests a minimum of the following conditions:

                1.  Creating an 8.3 directory off the root.
                2.  Creating files in this directory.
                3.  Finding a file with FindFirstFile()
                4.  Finding files with FindNextFile()
                5.  Deleting the files.
                6.  Deleting directory off the root.
                
****************************************************************************/
TESTPROCAPI Tst_BVT5(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;

    DWORD i;
    TCHAR szPathBuf[MAX_PATH], szFile[MAX_PATH], szPattern[MAX_PATH];
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATA wfd;    
    
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = GetPartitionCount();
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }

    if(0 == GetPartitionCount())
    {
        retVal = TPR_SKIP;
        LOG(_T("There are no [%s] stores with [%s] partitions to test, skipping"),
            g_testOptions.szProfile, g_testOptions.szFilesystem);
        goto Error;
    }

    if(((TPS_EXECUTE *)tpParam)->dwThreadNumber > GetPartitionCount())
    {
       retVal = TPR_PASS;
       LOG(_T("No partition for thread %u to test"), ((TPS_EXECUTE *)tpParam)->dwThreadNumber);
       goto Error;
    }

    pMtdPart = GetPartition(((TPS_EXECUTE *)tpParam)->dwThreadNumber-1);
    if(NULL == pMtdPart)
    {
        FAIL("GetPartition()");
        goto Error;
    }

    if(g_testOptions.fFormat && !pMtdPart->FormatVolume())
    {
        FAIL("FormatVolume()");
        goto Error;
    }

    if(!CompleteCreateDirectory(pMtdPart->GetTestFilePath(szPathBuf)))
    {
        FAIL("CompleteCreateDirectory()");
        goto Error;
    }

    // create 10 test files
    for(i = 0; i < 10; i++)
    {
        _stprintf(szFile, _T("%s\\%s%u.TST"), szPathBuf, TEST_FILE_NAME, i);

        LOG(_T("Create test file \"%s\""), szFile);
        hFile = CreateFile(szFile, GENERIC_WRITE | GENERIC_READ, 0, NULL, 
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if(INVALID_HANDLE(hFile))
        {
            LOG(_T("FAILED CreateFile(%s)"), szFile);
            ERRFAIL("CreateFile()");
            goto Error;
        }
        if(!CloseHandle(hFile))
        {
            ERRFAIL("CloseHandle()");
            goto Error;
        }
    }

    LOG(_T("Finding first file in \"%s\""), szPathBuf);
    _stprintf(szPattern, _T("%s\\*.TST"), szPathBuf);
    hFind = FindFirstFile(szPattern, &wfd);
    if(INVALID_HANDLE(hFind))
    {
        ERRFAIL("FindFirstFile()");
        goto Error;
    }
    LOG(_T("Found file \"%s\""), wfd.cFileName);

    LOG(_T("Deleting files in \"%s\""), szPathBuf);
    for(i = 0; i < 10; i++)
    {
        _stprintf(szFile, _T("%s\\%s%u.TST"), szPathBuf, TEST_FILE_NAME, i);
        if(!DeleteFile(szFile))
        {
            LOG(_T("FAILURE DeleteFile(%s)"), szFile);
            ERRFAIL("DeleteFile()");            
            goto Error;
        }
    }

    LOG(_T("Attempting to find more files in \"%s\" (expecting failure)"),
        szPathBuf);
    if(FindNextFile(hFind, &wfd))
    {
        LOG(_T("FindNextFile() in \"%s\" succeeded when it should have failed!"));
        FAIL("FindNextFile()");
        goto Error;
    }
        
    // success
    retVal = TPR_PASS;
    
Error:

    if(VALID_HANDLE(hFind) && !FindClose(hFind))
    {
        ERRFAIL("FindClose()");
        goto Error;
    }

    if(pMtdPart && !DeleteTree(pMtdPart->GetTestFilePath(szPathBuf), FALSE))
    {
        LOG(_T("Unable to remove \"%s\""), szPathBuf);
        FAIL("DeleteTree()");
        retVal = TPR_FAIL;
    }
    
    return retVal;
}

/****************************************************************************

    FUNCTION:   BVT6 (FFBVT Case 1)

    PURPOSE:    To test creating, checking attributes, and deleting a file
    
    NOTE:       This tests a minimum of the following conditions:

                1.  Creating an 8.3 directory off the root.
                2.  Creating a file in this directory.
                3.  Check the attributes of the file
                4.  Delete the file.
                5.  Delete the directory.
                
****************************************************************************/
TESTPROCAPI Tst_BVT6(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;

    DWORD dwAttributes;
    TCHAR szPathBuf[MAX_PATH], szFileName[MAX_PATH];
    HANDLE hFile = INVALID_HANDLE_VALUE;    
    
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = GetPartitionCount();
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }

    if(0 == GetPartitionCount())
    {
        retVal = TPR_SKIP;
        LOG(_T("There are no [%s] stores with [%s] partitions to test, skipping"),
            g_testOptions.szProfile, g_testOptions.szFilesystem);
        goto Error;
    }

    if(((TPS_EXECUTE *)tpParam)->dwThreadNumber > GetPartitionCount())
    {
       retVal = TPR_PASS;
       LOG(_T("No partition for thread %u to test"), ((TPS_EXECUTE *)tpParam)->dwThreadNumber);
       goto Error;
    }

    pMtdPart = GetPartition(((TPS_EXECUTE *)tpParam)->dwThreadNumber-1);
    if(NULL == pMtdPart)
    {
        FAIL("GetPartition()");
        goto Error;
    }

    if(g_testOptions.fFormat && !pMtdPart->FormatVolume())
    {
        FAIL("FormatVolume()");
        goto Error;
    }

    if(!CompleteCreateDirectory(pMtdPart->GetTestFilePath(szPathBuf)))
    {
        FAIL("CompleteCreateDirectory()");
        goto Error;
    }

    _stprintf(szFileName, _T("%s\\%s.TST"), szPathBuf, TEST_FILE_NAME);
    LOG(_T("Create test file \"%s\""), szFileName);
    hFile = CreateFile(szFileName, GENERIC_WRITE | GENERIC_READ, 0, NULL, 
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(INVALID_HANDLE(hFile))
    {
        LOG(_T("FAILED CreateFile(%s)"), szFileName);
        ERRFAIL("CreateFile()");
        goto Error;
    }
    if(!CloseHandle(hFile))
    {
        ERRFAIL("CloseHandle()");
        goto Error;
    }

    LOG(_T("Check attributes of file \"%s\""), szFileName);
    dwAttributes = GetFileAttributes(szFileName);
    if(0xFFFFFFFF == dwAttributes)
    {
        LOG(_T("FAILED GetFileAtributes(%s)"), szFileName);
        ERRFAIL("GetFileAttributes()");
        goto Error;
    }

    LOG(_T("Attributes of file \"%s\" are 0x%08.08X"), szFileName, dwAttributes);

    if(!DeleteFile(szFileName))
    {
        LOG(_T("FAILED DeleteFile(%s)"), szFileName);
        ERRFAIL("DeleteFile()");
        goto Error;
    }
        
    // success
    retVal = TPR_PASS;
    
Error:

    if(pMtdPart && !DeleteTree(pMtdPart->GetTestFilePath(szPathBuf), FALSE))
    {
        LOG(_T("Unable to remove \"%s\""), szPathBuf);
        FAIL("DeleteTree()");
        retVal = TPR_FAIL;
    }
    
    return retVal;
}

/****************************************************************************

    FUNCTION:   BVT7

    PURPOSE:    To test creating, getting file information by handle, 
                and deleting a file
    
    NOTE:       This tests a minimum of the following conditions:

                1.  Creating an 8.3 directory off the root.
                2.  Creating a file in this directory.
                3.  Get file information by handle for the file
                4.  Delete the file.
                5.  Delete the directory.
                
****************************************************************************/
TESTPROCAPI Tst_BVT7(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;

    BY_HANDLE_FILE_INFORMATION bhfi;
    TCHAR szPathBuf[MAX_PATH], szFileName[MAX_PATH];
    HANDLE hFile = INVALID_HANDLE_VALUE;    
    
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = GetPartitionCount();
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }

    if(0 == GetPartitionCount())
    {
        retVal = TPR_SKIP;
        LOG(_T("There are no [%s] stores with [%s] partitions to test, skipping"),
            g_testOptions.szProfile, g_testOptions.szFilesystem);
        goto Error;
    }

    if(((TPS_EXECUTE *)tpParam)->dwThreadNumber > GetPartitionCount())
    {
       retVal = TPR_PASS;
       LOG(_T("No partition for thread %u to test"), ((TPS_EXECUTE *)tpParam)->dwThreadNumber);
       goto Error;
    }

    pMtdPart = GetPartition(((TPS_EXECUTE *)tpParam)->dwThreadNumber-1);
    if(NULL == pMtdPart)
    {
        FAIL("GetPartition()");
        goto Error;
    }

    if(g_testOptions.fFormat && !pMtdPart->FormatVolume())
    {
        FAIL("FormatVolume()");
        goto Error;
    }

    if(!CompleteCreateDirectory(pMtdPart->GetTestFilePath(szPathBuf)))
    {
        FAIL("CompleteCreateDirectory()");
        goto Error;
    }

    _stprintf(szFileName, _T("%s\\%s.TST"), szPathBuf, TEST_FILE_NAME);
    LOG(_T("Create test file \"%s\""), szFileName);
    hFile = CreateFile(szFileName, GENERIC_WRITE | GENERIC_READ, 0, NULL, 
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(INVALID_HANDLE(hFile))
    {
        LOG(_T("FAILED CreateFile(%s)"), szFileName);
        ERRFAIL("CreateFile()");
        goto Error;
    }

    LOG(_T("Get information by handle of file \"%s\""), szFileName);
    if(!GetFileInformationByHandle(hFile, &bhfi))
    {
        LOG(_T("FAILED GetFileInformationByHandle(%s)"), szFileName);
        ERRFAIL("GetFileInformationByHandle()");
        goto Error;
    }
    
    if(!CloseHandle(hFile))
    {
        ERRFAIL("CloseHandle()");
        goto Error;
    }
    
    hFile = INVALID_HANDLE_VALUE;

    if(!DeleteFile(szFileName))
    {
        LOG(_T("FAILED DeleteFile(%s)"), szFileName);
        ERRFAIL("DeleteFile()");
        goto Error;
    }
        
    // success
    retVal = TPR_PASS;
    
Error:

    if(VALID_HANDLE(hFile) && !CloseHandle(hFile))
    {
        ERRFAIL("CloseHandle()");
        goto Error;
    }
    
    if(pMtdPart && !DeleteTree(pMtdPart->GetTestFilePath(szPathBuf), FALSE))
    {
        LOG(_T("Unable to remove \"%s\""), szPathBuf);
        FAIL("DeleteTree()");
        retVal = TPR_FAIL;
    }
    
    return retVal;
}

/****************************************************************************

    FUNCTION:   BVT8 (FFSTRESS Case 1)

    PURPOSE:    To test creating, enumerating, and deleting two files
    
    NOTE:       This tests a minimum of the following conditions:

                1.  Creating an 8.3 directory off the root.
                2.  Creating lots of files in this directory.
                3.  Deleting directory off the root.
                
****************************************************************************/
TESTPROCAPI Tst_BVT8(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;

    DWORD i;
    TCHAR szPathBuf[MAX_PATH], szFile[MAX_PATH];
    HANDLE hFile = INVALID_HANDLE_VALUE;
    
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = GetPartitionCount();
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }

    if(0 == GetPartitionCount())
    {
        retVal = TPR_SKIP;
        LOG(_T("There are no [%s] stores with [%s] partitions to test, skipping"),
            g_testOptions.szProfile, g_testOptions.szFilesystem);
        goto Error;
    }

    if(((TPS_EXECUTE *)tpParam)->dwThreadNumber > GetPartitionCount())
    {
       retVal = TPR_PASS;
       LOG(_T("No partition for thread %u to test"), ((TPS_EXECUTE *)tpParam)->dwThreadNumber);
       goto Error;
    }

    pMtdPart = GetPartition(((TPS_EXECUTE *)tpParam)->dwThreadNumber-1);
    if(NULL == pMtdPart)
    {
        FAIL("GetPartition()");
        goto Error;
    }

    if(g_testOptions.fFormat && !pMtdPart->FormatVolume())
    {
        FAIL("FormatVolume()");
        goto Error;
    }

    if(!CompleteCreateDirectory(pMtdPart->GetTestFilePath(szPathBuf)))
    {
        FAIL("CompleteCreateDirectory()");
        goto Error;
    }

    // create test files
    for(i = 0; i < lpFTE->dwUserData; i++)
    {
        _stprintf(szFile, _T("%s\\%s%u.TST"), szPathBuf, TEST_FILE_NAME, i);

        LOG(_T("Create test file \"%s\""), szFile);
        hFile = CreateFile(szFile, GENERIC_WRITE | GENERIC_READ, 0, NULL, 
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if(INVALID_HANDLE(hFile))
        {
            LOG(_T("FAILED CreateFile(%s)"), szFile);
            ERRFAIL("CreateFile()");
            goto Error;
        }
        if(!CloseHandle(hFile))
        {
            ERRFAIL("CloseHandle()");
            goto Error;
        }
    }

    retVal = TPR_PASS;
    
Error:

    if(pMtdPart && !DeleteTree(pMtdPart->GetTestFilePath(szPathBuf), FALSE))
    {
        LOG(_T("Unable to remove \"%s\""), szPathBuf);
        FAIL("DeleteTree()");
        retVal = TPR_FAIL;
    }
    
    return retVal;
}

TESTPROCAPI Tst_BVT9(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    UINT i = 0;
    DWORD retVal = TPR_FAIL;
    DWORD dwWritten = 0;
    DWORD dwFileSize = 0;
    LPBYTE lpAddr1 = NULL;
    LPBYTE lpAddr2 = NULL;
    LPBYTE lpBuffer = NULL;
    HANDLE hFile1 = INVALID_HANDLE_VALUE;
    HANDLE hFile2 = INVALID_HANDLE_VALUE;
    HANDLE hMapping = NULL;
    SYSTEM_INFO si = {0};
    TCHAR szPathBuf[MAX_PATH], szFile[MAX_PATH];

    CMtdPart *pMtdPart = NULL;
    
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = GetPartitionCount();
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }

    if(0 == GetPartitionCount())
    {
        retVal = TPR_SKIP;
        LOG(_T("There are no [%s] stores with [%s] partitions to test, skipping"),
            g_testOptions.szProfile, g_testOptions.szFilesystem);
        goto Error;
    }

    if(((TPS_EXECUTE *)tpParam)->dwThreadNumber > GetPartitionCount())
    {
       retVal = TPR_PASS;
       LOG(_T("No partition for thread %u to test"), ((TPS_EXECUTE *)tpParam)->dwThreadNumber);
       goto Error;
    }

    pMtdPart = GetPartition(((TPS_EXECUTE *)tpParam)->dwThreadNumber-1);
    if(NULL == pMtdPart)
    {
        FAIL("GetPartition()");
        goto Error;
    }

    if(g_testOptions.fFormat && !pMtdPart->FormatVolume())
    {
        FAIL("FormatVolume()");
        goto Error;
    }

    GetSystemInfo(&si);
    lpBuffer = (LPBYTE)HeapAlloc(GetProcessHeap(), 0, si.dwPageSize);

    if(!CompleteCreateDirectory(pMtdPart->GetTestFilePath(szPathBuf)))
    {
        FAIL("CompleteCreateDirectory()");
        goto Error;
    }

    // create test file
    _stprintf(szFile, _T("%s\\%s.MAP"), szPathBuf, TEST_FILE_NAME);
    LOG(_T("Creating file %s with data"), szFile);    
    hFile1 = CreateFile(szFile, GENERIC_READ | GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
    if(INVALID_HANDLE_VALUE == hFile1)
    {
        ERRFAIL("CreateFile()");
        goto Error;
    }

    // write the file
    dwFileSize = 0;
    for(i = 0; i < ITERATIONS; i++)
    {
        memset(lpBuffer, (UCHAR)i, si.dwPageSize);
        if(!WriteFile(hFile1, lpBuffer, si.dwPageSize, &dwWritten, NULL))
        {
            break;
        }
        dwFileSize +=dwWritten;
    }
    if(0 == dwFileSize)
    {
        ERRFAIL("WriteFile()");
        goto Error;
    }
    
    if(!CloseHandle(hFile1))
    {
        ERRFAIL("CloseHandle()");
        goto Error;
    }

    LOG(_T("Wrote %u bytes to the file"), dwFileSize);

    // reopen the file for mapping
    LOG(_T("Create mapping for file %s"), szFile);
    hFile2 = CreateFileForMapping(szFile, GENERIC_READ, FILE_SHARE_READ, NULL, 
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(INVALID_HANDLE_VALUE == hFile2)
    {
        ERRFAIL("CreateFileForMapping()");
        goto Error;
    }
    hMapping = CreateFileMapping(hFile2, NULL, PAGE_READONLY, 0, 
        dwFileSize, (LPTSTR)tpParam);
    if(NULL == hMapping)
    {
        ERRFAIL("CreateFileMapping()");
        goto Error;
    }
    lpAddr1 = (LPBYTE)MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    if(NULL == lpAddr1)
    {
        ERRFAIL("MapViewOfFile()");
        goto Error;
    }

    // read from map and compare to written file
    LOG(_T("Reading from maped file %s, comparing to actual file data..."), szFile);

    lpAddr2 = lpAddr1;
    for(i = 0; i < dwFileSize/si.dwPageSize; i++)
    {
        memset(lpBuffer, (UCHAR)i, si.dwPageSize);
        if(IsBadReadPtr(lpAddr2, si.dwPageSize))
        {
            ERRFAIL("IsBadReadPtr()");
            goto Error;
        }
        if(memcmp(lpBuffer, lpAddr2, si.dwPageSize))
        {
            LOG(_T("Data in map file %s differs from data written at offset %u"),
                szFile, i*si.dwPageSize);
            ERRFAIL("data mismatch");
            goto Error;
        }
        LOG(_T("File %s Iteration = %ld\tAddress = %08X"), szFile, i, lpAddr2);
        lpAddr2 += si.dwPageSize;
    }
        
    // success
    retVal = TPR_PASS;
    
Error:

    if(lpAddr1)
    {
        if(!UnmapViewOfFile(lpAddr1))
        {
            ERRFAIL("UnmapViewOfFile()");
            retVal = TPR_FAIL;
        }
    }

    if(hMapping)
    {
        if(!CloseHandle(hMapping))
        {
            ERRFAIL("CloseHandle()");
            retVal = TPR_FAIL;
        }
    }

    if(lpBuffer)
    {
        if(!HeapFree(GetProcessHeap(), 0, lpBuffer))
        {
            ERRFAIL("HeapFree()");
            retVal = TPR_FAIL;
        }
    }

    if(pMtdPart && !DeleteTree(pMtdPart->GetTestFilePath(szPathBuf), TRUE))
    {
        LOG(_T("Unable to remove \"%s\""), szPathBuf);
        FAIL("DeleteTree()");
        retVal = TPR_FAIL;
    }
    
    return retVal;
}
