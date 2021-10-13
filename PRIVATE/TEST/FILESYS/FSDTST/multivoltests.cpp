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

#define ITERATIONS          128

#define ROOT_DIR_LIMIT      1024

/****************************************************************************

    FUNCTION:   Test1

    PURPOSE:    To test creating, modifying and deleting a cluster sized 
                file with every possible attribute combination.

    NOTE:       This tests a minimum of the following conditions:

                1.  Creating an 8.3 directory off the root.
                2.  Creating a file in this directory that is exactly one
                    allocation unit in size.  This also gets read back and 
                    the data is compared with the original data.
                3.  Setting all attributes and combinations of attributes 
                    for this file.
                4.  Attempting to open for write files with every possible 
                    attribute combination.
                5.  Attempting to write to files with every possible 
                    attribute combination.
                6.  Attempting to delete files with every possible attribute 
                    combination.
                7.  Clearing every possible attribute combination.
                8.  Deleting normal files.
                
****************************************************************************/
TESTPROCAPI Tst_Depth1(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;
    TCHAR szPathBuf[MAX_PATH]; 
    TCHAR szTempStr[MAX_PATH];
    
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

    LOG(TEXT("File size %u bytes on volume %s"), pMtdPart->GetClusterSize(), 
        pMtdPart->GetVolumeName(szPathBuf));
    if(!CompleteCreateDirectory(pMtdPart->GetTestFilePath(szPathBuf)))
    {
        FAIL("CompleteCreateDirectory()");
        goto Error;
    }

    _stprintf(szTempStr, TEXT("%s\\%s"), szPathBuf, TEST_FILE_NAME);
    if(!TestAllFileAttributes(szTempStr, pMtdPart->GetClusterSize()))
    {
        FAIL("TestAllFileAttributes()");
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

    FUNCTION:   Test2

    PURPOSE:    To test creating, modifying and deleting a (cluster sized - 1) 
                file with every possible attribute combination.

    NOTE:       This tests a minimum of the following conditions:

                1.  Creating an 8.3 directory off the root.
                2.  Creating a file in this directory that is exactly one
                    byte less than an allocation unit in size.  This also 
                    gets read back and the data is compared with the original 
                    data.
                3.  Setting all attributes and combinations of attributes 
                    for this file.
                4.  Attempting to open for write files with every possible 
                    attribute combination.
                5.  Attempting to write to files with every possible 
                    attribute combination.
                6.  Attempting to delete files with every possible attribute 
                    combination.
                7.  Clearing every possible attribute combination.
                8.  Deleting normal files.
                
****************************************************************************/
TESTPROCAPI Tst_Depth2(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;
    TCHAR szPathBuf[MAX_PATH]; 
    TCHAR szTempStr[MAX_PATH];
    
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

    LOG(TEXT("File size %u bytes on volume %s"), pMtdPart->GetClusterSize()-1, 
        pMtdPart->GetVolumeName(szPathBuf));

    if(!CompleteCreateDirectory(pMtdPart->GetTestFilePath(szPathBuf)))
    {
        FAIL("CompleteCreateDirectory()");
        goto Error;
    }

    _stprintf(szTempStr, TEXT("%s\\%s"), szPathBuf, TEST_FILE_NAME);
    if(!TestAllFileAttributes(szTempStr, pMtdPart->GetClusterSize()-1))
    {
        FAIL("TestAllFileAttributes()");
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

    FUNCTION:   Test3

    PURPOSE:    To test creating, modifying and deleting a (cluster sized + 1) 
                file with every possible attribute combination.

    NOTE:       This tests a minimum of the following conditions:

                1.  Creating an 8.3 directory off the root.
                2.  Creating a file in this directory that is exactly one
                    byte more than an allocation unit in size.  This also 
                    gets read back and the data is compared with the original 
                    data.
                3.  Setting all attributes and combinations of attributes 
                    for this file.
                4.  Attempting to open for write files with every possible 
                    attribute combination.
                5.  Attempting to write to files with every possible 
                    attribute combination.
                6.  Attempting to delete files with every possible attribute 
                    combination.
                7.  Clearing every possible attribute combination.
                8.  Deleting normal files.
                
****************************************************************************/
TESTPROCAPI Tst_Depth3(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;
    TCHAR szPathBuf[MAX_PATH]; 
    TCHAR szTempStr[MAX_PATH];
    
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

    LOG(TEXT("File size %u bytes on volume %s"), pMtdPart->GetClusterSize()+1, 
        pMtdPart->GetVolumeName(szPathBuf));

    if(!CompleteCreateDirectory(pMtdPart->GetTestFilePath(szPathBuf)))
    {
        FAIL("CompleteCreateDirectory()");
        goto Error;
    }

    _stprintf(szTempStr, TEXT("%s\\%s"), szPathBuf, TEST_FILE_NAME);
    if(!TestAllFileAttributes(szTempStr, pMtdPart->GetClusterSize()+1))
    {
        FAIL("TestAllFileAttributes()");
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

    FUNCTION:   Test4

    PURPOSE:    To test creating, modifying and deleting a 1 byte
                file with every possible attribute combination.

    NOTE:       This tests a minimum of the following conditions:

                1.  Creating an 8.3 directory off the root.
                2.  Creating a file in this directory that is exactly one
                    byte in size.  This also gets read back and the data is 
                    compared with the original data.
                3.  Setting all attributes and combinations of attributes 
                    for this file.
                4.  Attempting to open for write files with every possible 
                    attribute combination.
                5.  Attempting to write to files with every possible 
                    attribute combination.
                6.  Attempting to delete files with every possible attribute 
                    combination.
                7.  Clearing every possible attribute combination.
                8.  Deleting normal files.
                
****************************************************************************/
TESTPROCAPI Tst_Depth4(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;
    TCHAR szPathBuf[MAX_PATH]; 
    TCHAR szTempStr[MAX_PATH];
    
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

    LOG(TEXT("File size %u bytes on volume %s"), 1, 
        pMtdPart->GetVolumeName(szPathBuf));

    if(!CompleteCreateDirectory(pMtdPart->GetTestFilePath(szPathBuf)))
    {
        FAIL("CompleteCreateDirectory()");
        goto Error;
    }

    _stprintf(szTempStr, TEXT("%s\\%s"), szPathBuf, TEST_FILE_NAME);
    if(!TestAllFileAttributes(szTempStr, 1))
    {
        FAIL("TestAllFileAttributes()");
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

    FUNCTION:   Test5

    PURPOSE:    To test creating, modifying and deleting a 0 byte
                file with every possible attribute combination.

    NOTE:       This tests a minimum of the following conditions:

                1.  Creating an 8.3 directory off the root.
                2.  Creating a file in this directory that is exactly 0
                    bytes in size.
                3.  Setting all attributes and combinations of attributes 
                    for this file.
                4.  Attempting to open for write files with every possible 
                    attribute combination.
                5.  Attempting to write to files with every possible 
                    attribute combination.
                6.  Attempting to delete files with every possible attribute 
                    combination.
                7.  Clearing every possible attribute combination.
                8.  Deleting normal files.
                
****************************************************************************/
TESTPROCAPI Tst_Depth5(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;
    TCHAR szPathBuf[MAX_PATH]; 
    TCHAR szTempStr[MAX_PATH];
    
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

    LOG(TEXT("File size %u bytes on volume %s"), 0, 
        pMtdPart->GetVolumeName(szPathBuf));

    if(!CompleteCreateDirectory(pMtdPart->GetTestFilePath(szPathBuf)))
    {
        FAIL("CompleteCreateDirectory()");
        goto Error;
    }

    _stprintf(szTempStr, TEXT("%s\\%s"), szPathBuf, TEST_FILE_NAME);
    if(!TestAllFileAttributes(szTempStr, 0))
    {
        FAIL("TestAllFileAttributes()");
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

    FUNCTION:   Test6

    PURPOSE:    To test creating deep single-level directories.

    NOTE:       This tests a minimum of the following conditions:

                1.  Creating single level directories of MAX_PATH +/- 10 in size
                    off the root.
                2.  Delete the single level directories off the root.
                
****************************************************************************/
TESTPROCAPI Tst_Depth6(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;

    DWORD dwCurSize;
    TCHAR szLongPath[2*MAX_PATH];
    TCHAR szPathBuf[MAX_PATH];
    BOOL fCallReturn;
    
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

    for(dwCurSize = MAX_PATH-10; dwCurSize <= MAX_PATH+10; dwCurSize++)
    {
        LOG(_T("Creating and removing %u byte directory"), dwCurSize);

        _stprintf(szLongPath, _T("%s%s%s"), pMtdPart->GetVolumeName(szPathBuf),
            A316_BYTE_PATH_OFF_ROOT1, A316_BYTE_PATH_OFF_ROOT2);

        szLongPath[dwCurSize] = _T('\0');

        fCallReturn = CreateAndRemoveRootDirectory(szLongPath, szPathBuf);

        if(dwCurSize < MAX_PATH)
        {
            CHECKTRUE(fCallReturn);
        }
        else
        {
            CHECKFALSE(fCallReturn);
        }
    }

    // success
    retVal = GetTestResult();
    
Error:
    
    return retVal;
}

/****************************************************************************

    FUNCTION:   Test7

    PURPOSE:    To test creating deep multiple-level directories.

    NOTE:       This tests a minimum of the following conditions:

                1.  Creating multiple level directories of MAX_PATH +/- x in size
                    off the root.(x =10 for small cards, 5 for large cards)
                2.  Delete the multiple level directories off the root.
                
****************************************************************************/
TESTPROCAPI Tst_Depth7(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;

    DWORD dwCurSize, dwVolPathSize, i;
    TCHAR szLongPath[2*MAX_PATH];
    TCHAR szPathBuf[MAX_PATH];
    BOOL fCallReturn;
    
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

    for(dwCurSize = MAX_PATH-10; dwCurSize <= MAX_PATH+10; dwCurSize++)
    {
        LOG(_T("Creating and removing %u byte directory"), dwCurSize);

        // get the length of the mount-point part of the string
        dwVolPathSize = _tcslen(pMtdPart->GetVolumeName(szPathBuf));

        _stprintf(szLongPath, TEXT("%s%s"), szPathBuf, A316_BYTE_PATH_OFF_ROOT1);
        szLongPath[dwVolPathSize+2] = _T('\0');
        if(FileExists(szLongPath))
        {
            LOG(TEXT("Warning, directory \"%s\" off root already exists!"), szLongPath);
        }

        _stprintf(szLongPath, _T("%s%s%s"), szPathBuf,
            A316_BYTE_PATH_OFF_ROOT1, A316_BYTE_PATH_OFF_ROOT2);

        // make this a deep multi-level directory by adding '\' every other character
        for(i = dwVolPathSize + 2; i <= dwCurSize; i+=2)
        {
            szLongPath[i] = _T('\\');
        }
        szLongPath[dwCurSize] = _T('\0');

        // make sure the last characer is not a '\'
        if(_T('\\') == szLongPath[dwCurSize-1])
            szLongPath[dwCurSize-1] = _T('z');

        fCallReturn = CreateAndRemoveRootDirectory(szLongPath, szPathBuf);

        if(dwCurSize < MAX_PATH)
        {
            CHECKTRUE(fCallReturn);
        }
        else
        {
            CHECKFALSE(fCallReturn);
        }
    }

    // success
    retVal = GetTestResult();
    
Error:
    
    return retVal;
}

/****************************************************************************

    FUNCTION:   Test8

    PURPOSE:    To test creating a file in a directory that does not exist.

    NOTE:       This tests a minimum of the following conditions:

                1.  The file should not be created and the system should 
                    fail gracefully.
                
****************************************************************************/

TESTPROCAPI Tst_Depth8(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;

    TCHAR szPathBuf[MAX_PATH];
    TCHAR szFileSpec[MAX_PATH];
    HANDLE hFile;
    
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

    if(!CompleteRemoveDirectoryMinusPcCard(pMtdPart->GetTestFilePath(szPathBuf), 
        pMtdPart->GetVolumeName(szFileSpec)))
    {
        if(!DeleteTree(szPathBuf, TRUE))
        {
            LOG(_T("unable to delete path %s"), szPathBuf);
            FAIL("DeleteTree()");
            goto Error;
        }
    }

    _stprintf(szFileSpec, _T("%s\\%s"), szPathBuf, TEST_FILE_NAME);
    hFile = CreateFile(szFileSpec, GENERIC_READ | GENERIC_WRITE, 0,
        NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if(INVALID_HANDLE_VALUE != hFile)
    {
        FAIL("Created a file in invalid directory");
        CloseHandle(hFile);
        goto Error;
    }
    // success
    retVal = TPR_PASS;
    
Error:
    
    return retVal;
}

/****************************************************************************

    FUNCTION:   Test9

    PURPOSE:    To test creating a bunch of directories within a directory,
                with each of the new directories containing a file.

    NOTE:       This tests a minimum of the following conditions:

                1.  Creating multiple directories within a single directory.
                    NOTE: By creating enough directory entries the store will
                    need to use several allocation units to hold the dir
                    entries.
                2.  Creating files within each of these directories.
                3.  Finding files using FindFirstFile() and FindNextFile()
                    across several allocation units.
                4.  Closing handles from the FindFirstFile() and FindNextFile()
                    functions.  If the FindClose() call fails it will not 
                    release access to the directory, causing the 
                    RemoveDirectory() call to fail in DeleteTree().
                5.  Removing directories.
                6.  Deleting files.
                
****************************************************************************/
TESTPROCAPI Tst_Depth9(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;

    UINT  nCurrentDirectory;
    TCHAR szFileSpec[MAX_PATH+20];
    TCHAR szPathBuf[MAX_PATH];
    
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

    if(NULL == pMtdPart->GetVolumeName(szPathBuf))
    {
        FAIL("GetVolumeName()");
        goto Error;
    }
    
    for(nCurrentDirectory = 1; nCurrentDirectory <= g_testOptions.nFileCount; nCurrentDirectory++)
    {
        _stprintf(szFileSpec, _T("%s\\%04.04i"), pMtdPart->GetTestFilePath(szPathBuf), nCurrentDirectory);
        if(!CreateDirectoryAndFile(szFileSpec, TEST_FILE_NAME))
        {
            FAIL("CreateDirectoryAndFile()");
            goto Error;
        }            
    }
    
        
    // success
    retVal = GetTestResult();
    
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

    FUNCTION:   Test10

    PURPOSE:    To test creating a bunch of files within a directory,
                Copying them to another directory, moving this new
                directory to another directory and then deleting them 
                from the original directory and the moved to directory.

    NOTE:       This tests a minimum of the following conditions:

                1.  Creating multiple files within a single directory.
                    NOTE: By creating enough directory entries the store will
                    need to use several allocation units to hold the dir
                    entries.
                2.  Copying files from one directory to another with various
                    attribute combinations set.
                3.  Moving files from one directory to another with various
                    attribute combinations set.
                4.  Removing directories.
                5.  Deleting files.
                
****************************************************************************/
TESTPROCAPI Tst_Depth10(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;

    TCHAR szPathBuf[MAX_PATH];
    TCHAR szTestPathBuf[MAX_PATH];
    TCHAR szMovePathBuf[MAX_PATH];
    TCHAR szCopyPathBuf[MAX_PATH];
    
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

    LOG(TEXT("File size %u bytes on volume %s"), pMtdPart->GetClusterSize()-50, 
        pMtdPart->GetVolumeName(szPathBuf));

    if(!CopyMoveAndDelForAllFileAttributes(pMtdPart->GetTestFilePath(szTestPathBuf),
        pMtdPart->GetCopyFilePath(szCopyPathBuf), pMtdPart->GetMoveFilePath(szMovePathBuf),
        FILE_ATTRIBUTE_NORMAL, g_testOptions.nFileCount, pMtdPart->GetClusterSize()-50,
        FALSE))
    {
        FAIL("CopyMoveAndDelForAllFileAttributes()");
        goto Error;
    }
        
    // success
    retVal = GetTestResult();
    
Error:    
    
    return retVal;
}

/****************************************************************************

    FUNCTION:   Test11

    PURPOSE:    To test the FileTimeToLocalTime(), FileTimeToSystemTime() and
                LocalTimeToFileTime() functions.

                
****************************************************************************/
TESTPROCAPI Tst_Depth11(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;

    TCHAR szPathBuf[MAX_PATH];
    
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

    if(!TestFileTimes(pMtdPart->GetTestFilePath(szPathBuf), TEST_FILE_NAME, 5000))
    {
        FAIL("TestFileTimes()");
        goto Error;
    }
        
    // success
    retVal = GetTestResult();
    
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

    FUNCTION:   Test12

    PURPOSE:    To test the compression logic under fragmented memory
                conditions.  It does this by creating 4 byte files until
                there is no more memory available.  It then deletes every
                other file to free up half the space, with the maximum 
                contiguous space being 4 bytes.

    NOTE:       This tests a minimum of the following conditions:

                1.  Disk full situations.  What happens when you write to
                    a full store.
                2.  Writing a large file to the store when it is too 
                    fragmented to hold the file contiguously.
                
****************************************************************************/
TESTPROCAPI Tst_Depth12(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;

    TCHAR szPathBuf[MAX_PATH], szFileName[MAX_PATH];
    DWORD dwNumFiles, dwCurFile;
    BOOL fDiskFull, fRetVal;
    
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

    if(!pMtdPart->FillDiskMinusXBytes(100000, g_testOptions.fQuickFill))
    {
        FAIL("FillDiskMinusXBytes()");
        goto Error;
    }

    // create 4 byte files to fill the rest of the disk
    dwNumFiles = 0;
    fDiskFull = FALSE;
    do
    {
        if(pMtdPart->GetCallerDiskFreeSpace() < pMtdPart->GetClusterSize())
        {
            fDiskFull = TRUE;
        }

        _stprintf(szFileName, TEXT("%08.08X.fil"), ++dwNumFiles);

        fRetVal = CreateTestFile(pMtdPart->GetTestFilePath(szPathBuf), szFileName, 4, NULL,
            MAX_EXPANSION);

        if(!fRetVal && !fDiskFull)
        {
            LOG(_T("Unable to create test file %s"), szFileName);
            LOG(_T("%u bytes free"), pMtdPart->GetCallerDiskFreeSpace());
        }
        else if(fDiskFull)
        {
            fRetVal = TRUE;
        }
        
    }while(!fDiskFull && fRetVal);

    // delete every other file
    for(dwCurFile = 1; fRetVal && (dwCurFile < dwNumFiles); dwCurFile +=2)
    {
        _stprintf(szFileName, TEXT("%s\\%08.08X.fil"), pMtdPart->GetTestFilePath(szPathBuf), 
            dwCurFile );
        fRetVal = DeleteFile(szFileName);
        if(!fRetVal)
        {
            LOG(_T("Unable to delete test file %s"), szFileName);
            ERRFAIL("DeleteFile()");
            goto Error;
        }
    }

    // deleted every other file, now create one using 1/2 the remaining space
    if(fRetVal)
    {
        if(!CreateTestFile(pMtdPart->GetTestFilePath(szPathBuf), TEST_FILE_NAME,
            pMtdPart->GetCallerDiskFreeSpace()/2, NULL, MAX_EXPANSION))
        {
            LOG(_T("Unable to create %u byte file with %u bytes free"),
                pMtdPart->GetCallerDiskFreeSpace()/2, pMtdPart->GetCallerDiskFreeSpace());
            goto Error;
        }
        
    }
        
    // success
    retVal = GetTestResult();
    
Error:

    if(pMtdPart)
    {
        pMtdPart->UnFillDisk();
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

    FUNCTION:   Test13

    PURPOSE:    To test the compression logic under fragmented memory
                conditions.  It does this by creating 4096 byte files until
                there is no more memory available.  It then deletes every
                other file to free up half the space, with the maximum 
                contiguous space being 4096 bytes.

    NOTE:       This tests a minimum of the following conditions:

                1.  Disk full situations.  What happens when you write to
                    a full store.
                2.  Writing a large file to the store when it is too 
                    fragmented to hold the file contiguously.
                3.  Compressing a 4096 byte block of expandable data into 
                    a 4096 byte block of memory.
                
****************************************************************************/
TESTPROCAPI Tst_Depth13(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;

    TCHAR szPathBuf[MAX_PATH], szFileName[MAX_PATH];
    DWORD dwNumFiles, dwCurFile;
    BOOL fDiskFull, fRetVal;
    
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

    if(pMtdPart->GetCallerDiskFreeSpace() > pMtdPart->GetClusterSize()*512)
    {
        if(!pMtdPart->FillDiskMinusXBytes(pMtdPart->GetClusterSize()*512, 
            g_testOptions.fQuickFill))
        {
            FAIL("FillDiskMinusXBytes()");
            goto Error;
        }
    }
    
    // create 4 byte files to fill the rest of the disk
    dwNumFiles = 0;
    fDiskFull = FALSE;
    do
    {
        if(pMtdPart->GetCallerDiskFreeSpace() < pMtdPart->GetClusterSize())
        {
            fDiskFull = TRUE;
        }

        _stprintf(szFileName, TEXT("%08.08X.fil"), ++dwNumFiles);

        fRetVal = CreateTestFile(pMtdPart->GetTestFilePath(szPathBuf), szFileName, 
            pMtdPart->GetClusterSize(), NULL, MAX_EXPANSION);

        if(!fRetVal && !fDiskFull)
        {
            LOG(_T("Unable to create test file %s"), szFileName);
            LOG(_T("%u bytes free"), pMtdPart->GetCallerDiskFreeSpace());
        }
        else if(fDiskFull)
        {
            fRetVal = TRUE;
        }
        
    }while(!fDiskFull && fRetVal);

    // delete every other file
    for(dwCurFile = 1; fRetVal && (dwCurFile < dwNumFiles); dwCurFile +=2)
    {
        _stprintf(szFileName, TEXT("%s\\%08.08X.fil"), pMtdPart->GetTestFilePath(szPathBuf), 
            dwCurFile );
        fRetVal = DeleteFile(szFileName);
        if(!fRetVal)
        {
            LOG(_T("Unable to delete test file %s"), szFileName);
            ERRFAIL("DeleteFile()");
            goto Error;
        }
    }

    // deleted every other file, now create one using 1/2 the remaining space
    if(fRetVal)
    {
        if(!CreateTestFile(pMtdPart->GetTestFilePath(szPathBuf), TEST_FILE_NAME,
            pMtdPart->GetCallerDiskFreeSpace()/2, NULL, MAX_EXPANSION))
        {
            LOG(_T("Unable to create %u byte file with %u bytes free"),
                pMtdPart->GetCallerDiskFreeSpace()/2, pMtdPart->GetCallerDiskFreeSpace());
            goto Error;
        }
        
    }
        
    // success
    retVal = GetTestResult();
    
Error:

    if(pMtdPart)
    {
        pMtdPart->UnFillDisk();
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

    FUNCTION:   Test14

    PURPOSE:    To test creating a bunch of files within a directory,
                Copying them to another directory, attempting to copy 
                them again over the existing files, moving this new
                directory to another directory and then deleting them 
                from the original directory and the moved to directory.

    NOTE:       This tests a minimum of the following conditions:

                1.  Creating multiple files within a single directory.
                    NOTE: By creating enough directory entries the store will
                    need to use several allocation units to hold the dir
                    entries.
                2.  Copying files from one directory to another with various
                    attribute combinations set.
                3.  Moving files from one directory to another with various
                    attribute combinations set.
                4.  Removing directories.
                5.  Deleting files.
                
****************************************************************************/
TESTPROCAPI Tst_Depth14(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;

    TCHAR szPathBuf[MAX_PATH];
    TCHAR szTestPathBuf[MAX_PATH];
    TCHAR szMovePathBuf[MAX_PATH];
    TCHAR szCopyPathBuf[MAX_PATH];
    
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

    LOG(TEXT("File size %u bytes on volume %s"), pMtdPart->GetClusterSize()-50, 
        pMtdPart->GetVolumeName(szPathBuf));

    if(!CopyMoveAndDelForAllFileAttributes(pMtdPart->GetTestFilePath(szTestPathBuf),
        pMtdPart->GetCopyFilePath(szCopyPathBuf), pMtdPart->GetMoveFilePath(szMovePathBuf),
        FILE_ATTRIBUTE_NORMAL, g_testOptions.nFileCount, pMtdPart->GetClusterSize()-50,
        TRUE))
    {
        FAIL("CopyMoveAndDelForAllFileAttributes()");
        goto Error;
    }
        
    // success
    retVal = GetTestResult();
    
Error:
    
    return retVal;
}

/****************************************************************************

    FUNCTION:   Test15

    PURPOSE:    To test creating a bunch of files on the Storage Card,
                Copying them to the Object Store, moving this new
                directory from the Object Store to the Storage Card and 
                then deleting them.

    NOTE:       This tests a minimum of the following conditions:

                1.  Creating multiple files within a single directory.
                    NOTE: By creating enough directory entries the store will
                    need to use several allocation units to hold the dir
                    entries.
                2.  Copying files from the Storage Card to the Object Store with 
                    various attribute combinations set.
                3.  Moving files from the Object Store to the Storage Card with 
                    various attribute combinations set.
                4.  Removing directories.
                5.  Deleting files.
                
****************************************************************************/
TESTPROCAPI Tst_Depth15(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;

    TCHAR szPathBuf[MAX_PATH];
    TCHAR szTestPathBuf[MAX_PATH];
    TCHAR szMovePathBuf[MAX_PATH];
    TCHAR szCopyPathBuf[MAX_PATH];
    
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

    LOG(TEXT("File size %u bytes on volume %s"), pMtdPart->GetClusterSize()-50, 
        pMtdPart->GetVolumeName(szPathBuf));

    if(!CopyMoveAndDelForAllFileAttributes(pMtdPart->GetTestFilePath(szTestPathBuf),
        pMtdPart->GetOsCopyFilePath(szCopyPathBuf), pMtdPart->GetMoveFilePath(szMovePathBuf),
        FILE_ATTRIBUTE_NORMAL, g_testOptions.nFileCount, pMtdPart->GetClusterSize()-50,
        FALSE))
    {
        FAIL("CopyMoveAndDelForAllFileAttributes()");
        goto Error;
    }
        
    // success
    retVal = GetTestResult();
    
Error:
    
    return retVal;
}

/****************************************************************************

    FUNCTION:   Test16

    PURPOSE:    To test creating a bunch of files in the Object Store,
                Copying them to the Storage Card, moving this new
                directory from the Storage Card to the Object Store and 
                then deleting them.

    NOTE:       This tests a minimum of the following conditions:

                1.  Creating multiple files within a single directory.
                    NOTE: By creating enough directory entries the store will
                    need to use several allocation units to hold the dir
                    entries.
                2.  Copying files from the Object Store to the Storage Card with 
                    various attribute combinations set.
                3.  Moving files from the Storage Card to the Object Store with 
                    various attribute combinations set.
                4.  Removing directories.
                5.  Deleting files.
                
****************************************************************************/
TESTPROCAPI Tst_Depth16(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;

    TCHAR szPathBuf[MAX_PATH];
    TCHAR szTestPathBuf[MAX_PATH];
    TCHAR szMovePathBuf[MAX_PATH];
    TCHAR szCopyPathBuf[MAX_PATH];
    
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

    LOG(TEXT("File size %u bytes on volume %s"), pMtdPart->GetClusterSize()-50, 
        pMtdPart->GetVolumeName(szPathBuf));

    if(!CopyMoveAndDelForAllFileAttributes(pMtdPart->GetOsTestFilePath(szTestPathBuf),
        pMtdPart->GetCopyFilePath(szCopyPathBuf), pMtdPart->GetOsMoveFilePath(szMovePathBuf),
        FILE_ATTRIBUTE_NORMAL, g_testOptions.nFileCount, pMtdPart->GetClusterSize()-50,
        FALSE))
    {
        FAIL("CopyMoveAndDelForAllFileAttributes()");
        goto Error;
    }
        
    // success
    retVal = GetTestResult();
    
Error:
    
    return retVal;
}

/****************************************************************************

    FUNCTION:   Test17

    PURPOSE:    To test creating/removing the "Storage Card" directory in the 
                Object Store.

    NOTE:       This tests a minimum of the following conditions:

                1.  Makes sure that the "Storage Card" directory cannot be created
                    or deleted in the Object Store.
                
****************************************************************************/
TESTPROCAPI Tst_Depth17(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;

    TCHAR szPathBuf[MAX_PATH];
    HANDLE hFile;
    
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

    // create directory
    //
    LOG(_T("Trying CreateDirectory(%s)..."), pMtdPart->GetVolumeName(szPathBuf));
    if(CreateDirectory(szPathBuf, NULL))
    {
        LOG(_T("CreateDirectory(%s) succeeded when it should have failed"),
            szPathBuf);
        FAIL("CreateDirectory()");
        goto Error;
    }
    LOG(_T("CreateDirectory(%s) failed as expected, error=%u"), pMtdPart->GetVolumeName(szPathBuf),
        GetLastError());

    // create file
    //
    LOG(_T("Trying CreateFile(%s)..."), pMtdPart->GetVolumeName(szPathBuf));
    hFile = CreateFile(szPathBuf, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(VALID_HANDLE(hFile))
    {
        LOG(_T("CreateFile(%s) succeeded when it should have failed"),
            szPathBuf);
        FAIL("CreateFile()");
        CloseHandle(hFile);
        goto Error;
    }
    LOG(_T("CreateFile(%s) failed as expected, error=%u"), pMtdPart->GetVolumeName(szPathBuf),
        GetLastError());

    // remove directory
    //
    LOG(_T("Trying RemoveDirectory(%s)..."), pMtdPart->GetVolumeName(szPathBuf));
    if(RemoveDirectory(szPathBuf))
    {
        LOG(_T("RemoveDirectory(%s) succeeded when it should have failed"),
            szPathBuf);
        FAIL("RemoveDirectory()");
        goto Error;
    }
    LOG(_T("RemoveDirectory(%s) failed as expected, error=%u"), pMtdPart->GetVolumeName(szPathBuf),
        GetLastError());
        
    // success
    retVal = TPR_PASS;
    
Error:
    
    return retVal;
}

/****************************************************************************

    FUNCTION:   Test18

    PURPOSE:    To test truncating a file on the Storage Card.

    NOTE:       This tests a minimum of the following conditions:

                1.  Tests creating a 100k file the Storage Card.
                2.  Tests truncating 10k off the end of the file.
                3.  Tests truncating the file to 10k from the beginning.
                
****************************************************************************/
TESTPROCAPI Tst_Depth18(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;

    TCHAR szPathBuf[MAX_PATH], szFileName[MAX_PATH];
    HANDLE hFile;
    DWORD dwFilePtr;
    DWORD dwFileSize = 100000;
    
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

    // create at file to test with
    if(!CreateTestFile(szPathBuf, TEST_FILE_NAME, dwFileSize, NULL, REPETITIVE_BUFFER))
    {
        LOG(_T("Unable to create test file \"%s\\%s\""), szPathBuf, TEST_FILE_NAME);
        ERRFAIL("CreateTestFile()");
        goto Error;
    }

    // re-open the test file
    _stprintf(szFileName, _T("%s\\%s"), szPathBuf, TEST_FILE_NAME);
    hFile = CreateFile(szFileName, GENERIC_READ | GENERIC_WRITE, 0,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(INVALID_HANDLE(hFile))
    {
        LOG(_T("Unable to re-open test file \"%s\""), szFileName);
        ERRFAIL("CreateFile()");
        goto Error;
    }

    // seek to 10k from the end of file
    LOG(_T("Truncating file \"%s\", seeking from end of file..."), szFileName);
    dwFilePtr = SetFilePointer(hFile, -10000, NULL, FILE_END );
    if(dwFilePtr != dwFileSize-10000)
    {
        LOG(_T("Unable to set file pointer from the end in file \"%s\""),
            szFileName);
        LOG(_T("File Ptr == %lu"), dwFilePtr);
        LOG(_T("Expected File Ptr == %lu"), dwFileSize-10000);
        LOG(_T("Seek Value == %lu"), -10000);
        ERRFAIL("SetFilePointer(FILE_END)");
        goto Error;
    }
    
    if(!SetEndOfFile(hFile))
    {
        LOG(_T("Unable to truncate file \"%s\""), szFileName);
        ERRFAIL("SetEndOfFile()");
        goto Error;
    }

    CloseHandle(hFile);

    if(dwFilePtr != FileSize(szFileName))
    {
        LOG(_T("Unable to truncate the file \"%s\", file size mistmatch"),
            szFileName);
        LOG(_T("Expected file size == %lu"), dwFilePtr);
        LOG(_T("Actual file size == %lu"), FileSize(szFileName));
        FAIL("Truncating file");
        goto Error;
    }

    // re-open the test file
    hFile = CreateFile(szFileName, GENERIC_READ | GENERIC_WRITE, 0,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(INVALID_HANDLE(hFile))
    {
        LOG(_T("Unable to re-open test file \"%s\""), szFileName);
        ERRFAIL("CreateFile()");
        goto Error;
    }

    LOG(_T("Truncating file \"%s\", seeking from beginning of file..."), szFileName);
    dwFilePtr = SetFilePointer(hFile, 10000, NULL, FILE_BEGIN );
    if(dwFilePtr != 10000)
    {
        LOG(_T("Unable to set the file pointer from the beginning in file \"%s\""),
            szFileName);
        LOG(_T("File Ptr == %lu"), dwFilePtr);
        LOG(_T("Expected File Ptr == %lu"), 10000);
        LOG(_T("Seek Value == %lu"), 10000);
        ERRFAIL("SetFilePointer(FILE_BEGIN)");
        goto Error;
    }

    if(!SetEndOfFile(hFile))
    {
        LOG(_T("Unable to truncate file \"%s\""), szFileName);
        ERRFAIL("SetEndOfFile()");
        goto Error;
    }

    CloseHandle(hFile);

    if(dwFilePtr != FileSize(szFileName))
    {
        LOG(_T("Unable to truncate the file \"%s\", file size mistmatch"),
            szFileName);
        LOG(_T("Expected file size == %lu"), dwFilePtr);
        LOG(_T("Actual file size == %lu"), FileSize(szFileName));
        FAIL("Truncating file");
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

    FUNCTION:   Test19

    PURPOSE:    To test creating more directory entries than the root can hold.

    NOTE:       This tests a minimum of the following conditions:

                1.  Tests how the Storage Card handles the situation
                    where a user attempts to create too many root 
                    directory entries.
                
****************************************************************************/
TESTPROCAPI Tst_Depth19(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;

    TCHAR szPathBuf[MAX_PATH], szDirName[MAX_PATH];
    DWORD dwMaxRootEntries, dwDirCounter, dwNumRootEntries;
    BOOL fDone;
    
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

    dwMaxRootEntries = pMtdPart->GetMaxRootDirEntries();
    if(ROOT_DIR_LIMIT < dwMaxRootEntries)
    {
        dwMaxRootEntries = ROOT_DIR_LIMIT;
        LOG(_T("Reducing maximum root directory entries for this volume to %u"), dwMaxRootEntries);
    }

    // kill off any dirs that match the one's we'll be creating for this test
    NukeDirectories(pMtdPart->GetVolumeName(szPathBuf), _T("*PATH.DIR"));

    dwDirCounter = pMtdPart->GetNumberOfRootDirEntries();

    if(dwDirCounter >= dwMaxRootEntries)
    {
        LOG(_T("The volume already contains too many (%u) root entries; skipping test"));
        retVal = TPR_SKIP;
        goto Error;
    }

    if(0 < dwDirCounter)
    {
        LOG(_T("Volume \"%s\" already contains %u root entries; test will attempt to create %u more"),
            pMtdPart->GetVolumeName(szPathBuf), dwDirCounter, dwMaxRootEntries - dwDirCounter);
    }
    
    fDone = FALSE;
    while(!fDone)
    {
        _stprintf(szDirName, _T("%s\\%03.03luPATH.DIR"), szPathBuf, dwDirCounter++);
        if(!CreateDirectory(szDirName, NULL))
        {
            ERR("CreateDirectory()");
            dwNumRootEntries = pMtdPart->GetNumberOfRootDirEntries();
            if(dwNumRootEntries >= dwMaxRootEntries)
            {
                LOG(_T("CreateDirectory() failed as expected with %lu root directory entries."), 
                    dwNumRootEntries );
                fDone = TRUE;
            }
            else
            {
                LOG(_T("ERROR: CreateDirectory() failed with only %lu root directory entries."), 
                    dwNumRootEntries );
                FAIL("CreateDirectory()");
                goto Error;
            }
        }
        else // create dir succeeded
        {
            //LOG(_T("Created dir %s ok"), szDirName);
            dwNumRootEntries = pMtdPart->GetNumberOfRootDirEntries();
            if(dwNumRootEntries >= dwMaxRootEntries)
            {
                LOG(_T("Successfully created %lu root directory entries."), 
                    dwNumRootEntries );
                fDone = TRUE;
            }
            if(!(dwNumRootEntries%32))
            {
                LOG(_T("Created %lu root directory entries"), dwNumRootEntries );
            }
        }
    }
        
    // success
    retVal = TPR_PASS;
    
Error:

    if(pMtdPart)
    {
        NukeDirectories(pMtdPart->GetVolumeName(szPathBuf), _T("*PATH.DIR"));
    }
    
    return retVal;
}

/****************************************************************************

    FUNCTION:   Test20

    PURPOSE:    To test creating a file using the same name as an existing
                directory on the Storage Card.

    NOTE:       This tests a minimum of the following conditions:

                1.  Tests how the Storage Card handles creating a file
                    whose name is the same as an existing directory.
                
****************************************************************************/
TESTPROCAPI Tst_Depth20(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

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

    if(!pMtdPart->CreateTestDirectory(TEST_FILE_NAME))
    {
        FAIL("CreateTestDirectory()");
        goto Error;
    }

    if(pMtdPart->CreateTestFile(TEST_FILE_NAME,  0))
    {
        FAIL("CreateFile() succeeded when a directory existed with the same name");
        goto Error;
    }

    LOG(_T("CreateFile() failed as expected when a directory already existed with the same name"));


    if(!pMtdPart->RemoveTestDirectory(TEST_FILE_NAME))
    {
        FAIL("RemoveTestDirectory()");
        goto Error;
    }

    // success
    retVal = TPR_PASS;
    
Error:
    
    return retVal;
}

/****************************************************************************

    FUNCTION:   Test21

    PURPOSE:    To test creating a directory using the same name as an existing
                file on the Storage Card.

    NOTE:       This tests a minimum of the following conditions:

                1.  Tests how the Storage Card handles creating a directory
                    whose name is the same as an existing file.
                
****************************************************************************/
TESTPROCAPI Tst_Depth21(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

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

    if(!pMtdPart->CreateTestFile(TEST_FILE_NAME, 0))
    {
        FAIL("CreateTestFile()");
        goto Error;
    }

    if(pMtdPart->CreateTestDirectory(TEST_FILE_NAME))
    {
        FAIL("CreateTestDirectory() succeeded when a file existed with the same name");
        goto Error;
    }

    LOG(_T("CreateFile() failed as expected when a file already existed with the same name"));


    if(!pMtdPart->DeleteTestFile(TEST_FILE_NAME))
    {
        FAIL("DeleteTestFile()");
        goto Error;
    }
    
    // success
    retVal = TPR_PASS;
    
Error:
    
    return retVal;
}


/****************************************************************************

    FUNCTION:   Test22

    PURPOSE:    To test creating > 999 files with the same extensions and
                first 8 characters in the file name on the Storage Card.

    NOTE:       This tests a minimum of the following conditions:

                1.  Tests how the Storage Card handles creating short file names.
                    It has a limit of 999 and should fail when you attempt 
                    to create the 1,000th.
                
****************************************************************************/
TESTPROCAPI Tst_Depth22(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;

    TCHAR szPathBuf[MAX_PATH], szFileName[MAX_PATH], szTmpName[MAX_PATH];
    DWORD i;
    
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

    LOG(_T("Creating 999 test files in \"%s\""), szPathBuf);
    for(i = 0; i < 999; i++)
    {
        _stprintf(szFileName, TEXT("%s%03.03X.tst"), LONG_FILE_NAME, i );
        if(!CreateTestFile(szPathBuf, szFileName, 0, NULL, REPETITIVE_BUFFER))
        {
            LOG(_T("Unable to create file #%u \"%s\\%s\""), i, szPathBuf, szFileName);
            ERRFAIL("CreateTestFile()");
            goto Error;
        }
    }

    LOG(_T("Attempting to create 1000th test file in \"%s\""), szPathBuf);
    _stprintf(szFileName, TEXT("%s999.tst"), LONG_FILE_NAME);
    if(CreateTestFile(szPathBuf, szFileName, 0, NULL, REPETITIVE_BUFFER))
    {
        LOG(_T("Created file #1000 \"%s\\%s\" when it should have failed"),
            szPathBuf, szFileName);
        FAIL("CreateTestFile()");
        goto Error;
    }
    else
    {
        ERR("CreateTestFile()");
        LOG(_T("Unable to create file #1000 \"%s\\%s\" as expected"), szPathBuf, szFileName);
    }

    if(!CreateTestFile(szPathBuf, TEST_FILE_NAME, 0, NULL, REPETITIVE_BUFFER))
    {
        LOG(_T("Unable to create test file \"%s\\%s\""), szPathBuf, TEST_FILE_NAME);
        ERRFAIL("CreateTestFile()");
        goto Error;
    }

    _stprintf(szTmpName, _T("%s\\%s"), szPathBuf, TEST_FILE_NAME);
    _stprintf(szFileName, _T("%s\\%s999.tst"), szPathBuf, LONG_FILE_NAME);
    if(MoveFile(szTmpName, szFileName))
    {
        LOG(_T("Renamed test file from \"%s\" to \"%s\" when it should have failed"),
            szTmpName, szFileName);
        FAIL("MoveFile()");
        goto Error;
    }
    else
    {
        LOG(_T("Unable to rename test file from \"%s\" to \"%s\" as expected"),
            szTmpName, szFileName);
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

    FUNCTION:   Test23

    PURPOSE:    To test creating a 100 byte file on the Storage Card with 2
                available clusters.

    NOTE:       This tests a minimum of the following conditions:

                1.  Tests how the Storage Card handles disk full situations
                    when creating a file.
                
****************************************************************************/
TESTPROCAPI Tst_Depth23(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

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

    if(!pMtdPart->FillDiskMinusXBytes(2*pMtdPart->GetClusterSize(), g_testOptions.fQuickFill))
    {
        FAIL("FillDiskMinusXBytes()");
        goto Error;
    }

    if(!pMtdPart->CreateTestFile(TEST_FILE_NAME, 100))
    {
        FAIL("CreateTestFile()");
        goto Error;
    }

    if(!pMtdPart->DeleteTestFile(TEST_FILE_NAME))
    {
        FAIL("DeleteTestFile()");
        goto Error;
    }
    
    // success
    retVal = TPR_PASS;
    
Error:

    if(pMtdPart)
    {
        pMtdPart->UnFillDisk();
    }
    
    return retVal;
}

/****************************************************************************

    FUNCTION:   Test24

    PURPOSE:    To test creating a directory on the Storage Card with only one
                available cluster.

    NOTE:       This tests a minimum of the following conditions:

                1.  Tests how the Storage Card handles disk full situations
                    when creating a directory.
                
****************************************************************************/
TESTPROCAPI Tst_Depth24(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

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

    if(!pMtdPart->FillDiskMinusXBytes(1*pMtdPart->GetClusterSize(), g_testOptions.fQuickFill))
    {
        FAIL("FillDiskMinusXBytes()");
        goto Error;
    }

    if(!pMtdPart->CreateTestDirectory(TEST_DIR_NAME))
    {
        FAIL("CreateTestDirectory()");
        goto Error;
    }

    if(!pMtdPart->RemoveTestDirectory(TEST_DIR_NAME))
    {
        FAIL("RemoveTestDirectory()");
        goto Error;
    }
    
    // success
    retVal = TPR_PASS;
    
Error:

    if(pMtdPart)
    {
        pMtdPart->UnFillDisk();
    }
    
    return retVal;
}

/****************************************************************************

    FUNCTION:   Test25

    PURPOSE:    To test creating a 100 byte file on the Storage Card with 1
                available cluster.

    NOTE:       This tests a minimum of the following conditions:

                1.  Tests how the Storage Card handles disk full situations
                    when creating a file.
                
****************************************************************************/
TESTPROCAPI Tst_Depth25(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

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

    if(!pMtdPart->FillDiskMinusXBytes(1*pMtdPart->GetClusterSize(), g_testOptions.fQuickFill))
    {
        FAIL("FillDiskMinusXBytes()");
        goto Error;
    }

    if(!pMtdPart->CreateTestFile(TEST_FILE_NAME, 100))
    {
        FAIL("CreateTestFile()");
        goto Error;
    }

    if(!pMtdPart->DeleteTestFile(TEST_FILE_NAME))
    {
        FAIL("DeleteTestFile()");
        goto Error;
    }
    
    // success
    retVal = TPR_PASS;
    
Error:

    if(pMtdPart)
    {
        pMtdPart->UnFillDisk();
    }
    
    return retVal;
}

/****************************************************************************

    FUNCTION:   Test26

    PURPOSE:    To test creating directories on the Storage Card with only one
                available cluster until the disk is full.

    NOTE:       This tests a minimum of the following conditions:

                1.  Tests how the Storage Card handles disk full situations
                    when creating a directory.
                
****************************************************************************/
TESTPROCAPI Tst_Depth26(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);
    
    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;
    TCHAR szDirName[MAX_PATH];

    BOOL fDone = FALSE;
    INT cLoop = 0;
    DWORD dwLastError = 0;
    
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

    if(!pMtdPart->FillDiskMinusXBytes(1*pMtdPart->GetClusterSize(), g_testOptions.fQuickFill))
    {
        FAIL("FillDiskMinusXBytes()");
        goto Error;
    }

    fDone = FALSE;
    while(!fDone)
    {
        _stprintf(szDirName, _T("%s.%03.03u"), TEST_DIR_NAME, cLoop);
        if(!pMtdPart->CreateTestDirectory(szDirName))
        {
            dwLastError = GetLastError();
            if(ERROR_DISK_FULL != dwLastError)
            {
                ERRFAIL("CreateTestDirectory()");
                goto Error;
            }
            else
            {
                fDone = TRUE;
            }
        }
        else
        {
            cLoop++;
        }
        if(ROOT_DIR_LIMIT == cLoop)
        {
            break;
        }
    }

    // remove all the directories we created
    for(cLoop--; cLoop >= 0; cLoop--)
    {
        _stprintf(szDirName, _T("%s.%03.03u"), TEST_DIR_NAME, cLoop);
        if(!pMtdPart->RemoveTestDirectory(szDirName))
        {
            ERRFAIL("RemoveTestDirectory()");
            goto Error;
        }
    }
    
    // success
    retVal = TPR_PASS;
    
Error:

    if(pMtdPart)
    {
        pMtdPart->UnFillDisk();
    }
    
    return retVal;
}

/****************************************************************************

    FUNCTION:   Test27

    PURPOSE:    To test creating a directory on the Storage Card with no
                available clusters.

    NOTE:       This tests a minimum of the following conditions:

                1.  Tests how the Storage Card handles disk full situations
                    when creating a directory.
                
****************************************************************************/
TESTPROCAPI Tst_Depth27(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

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

    if(!pMtdPart->FillDiskMinusXBytes(0, g_testOptions.fQuickFill))
    {
        FAIL("FillDiskMinusXBytes()");
        goto Error;
    }

    // should be unable to create a directory when the disk is full
    if(pMtdPart->CreateTestDirectory(TEST_DIR_NAME))
    {
        FAIL("CreateTestDirectory() succeeded when the disk was full");
        goto Error;
    }
    
    // success
    retVal = TPR_PASS;
    
Error:

    if(pMtdPart)
    {
        pMtdPart->UnFillDisk();
    }
    
    return retVal;
}

/****************************************************************************

    FUNCTION:   Test27

    PURPOSE:    To test creating a directory on the Storage Card with no
                available clusters.

    NOTE:       This tests a minimum of the following conditions:

                1.  Tests how the Storage Card handles disk full situations
                    when creating a directory.
                
****************************************************************************/
TESTPROCAPI Tst_Depth28(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

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

    if(!pMtdPart->FillDiskMinusXBytes(0, g_testOptions.fQuickFill))
    {
        FAIL("FillDiskMinusXBytes()");
        goto Error;
    }

    // should be able to create a zero-byte file even though disk is full
    if(!pMtdPart->CreateTestFile(TEST_FILE_NAME, 0))
    {
        FAIL("CreateTestFile()");
        goto Error;
    }
    if(!pMtdPart->DeleteTestFile(TEST_FILE_NAME))
    {
        ERRFAIL("DeleteTestFile()");
        goto Error;
    }

    // success
    retVal = TPR_PASS;

Error:

    if(pMtdPart)
    {
        pMtdPart->UnFillDisk();
    }
    
    return retVal;
}

/****************************************************************************
    FUNCTION:   Test29

    PURPOSE:    To test creating a 100 byte file on the Storage Card with 0
                available clusters.

    NOTE:       This tests a minimum of the following conditions:

                1.  Tests how the Storage Card handles disk full situations
                    when creating a file.
                
****************************************************************************/
TESTPROCAPI Tst_Depth29(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

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

    if(!pMtdPart->FillDiskMinusXBytes(0, g_testOptions.fQuickFill))
    {
        FAIL("FillDiskMinusXBytes()");
        goto Error;
    }

    // should be unable to create a 100 byte file because disk is full
    if(pMtdPart->CreateTestFile(TEST_FILE_NAME, 100))
    {
        FAIL("CreateTestFile() succeeded when it should have failed");
        if(!pMtdPart->DeleteTestFile(TEST_FILE_NAME))
        {
            ERRFAIL("DeleteTestFile()");
        }
        goto Error;
    }
    
    // success
    retVal = TPR_PASS;
    
Error:

    if(pMtdPart)
    {
        pMtdPart->UnFillDisk();
    }
    
    return retVal;
}

TESTPROCAPI Tst_Depth30(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;
    UINT i = 0;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    TCHAR szPathBuf[MAX_PATH];
    TCHAR szFile[MAX_PATH];
    DWORD dwData = 0;
    DWORD dwWritten = 0;
    DWORD offset;
    DWORD maxIterations;
    
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

    offset = lpFTE->dwUserData;

    _stprintf(szFile, _T("%s\\%s.DAT"), szPathBuf, TEST_FILE_NAME);
    LOG(_T("Creating file %s"), szFile);
    hFile = CreateFile(szFile, GENERIC_READ | GENERIC_WRITE, 
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, 
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(INVALID_HANDLE(hFile))
    {
        ERRFAIL("CreateFile()");
        goto Error;
    }

    // see if this FSD supports RFWS, WFWS
    if(!ReadFileWithSeek(hFile, NULL, 0, NULL, NULL, 0, 0))
    {
        if(ERROR_NOT_SUPPORTED == GetLastError())
        {
            LOG(_T("ReadFileWithSeek() failed on %s; FSD does not support this API"));
            retVal = TPR_PASS;
            goto Error;
        }
        ERRFAIL("ReadFileWithSeek()");
        goto Error;
    }

    // fill the disk
    if(!pMtdPart->FillDiskMinusXBytes(1*pMtdPart->GetClusterSize(), g_testOptions.fQuickFill))
    {
        FAIL("FillDiskMinusXBytes()");
        goto Error;
    }

    LOG(_T("Writing data to %s at %u offsets using WriteFileWithSeek()..."), szFile, offset);
    // write data to different offsets in the file until we fail from filling the disk
    for(i = 0; ; i++)
    {
        dwData = i;
        LOG(_T("File %s: writing 0x%08X at offset %u"), szFile, dwData, i*offset);
        if(!WriteFileWithSeek(hFile, &dwData, sizeof(dwData), &dwWritten, NULL, 
            i*offset, 0))
        {
            if(ERROR_DISK_FULL == GetLastError() ||
               ERROR_OUTOFMEMORY == GetLastError())
            {
                LOG(_T("Disk filled up while writing to file %s..."), szFile);
                // reset our maximum to the total number that we wrote
                maxIterations = i - 1;
                break;
            }
            ERRFAIL("WriteFileWithSeek()");
            goto Error;
        }
    }    
        
    // success
    retVal = TPR_PASS;
    
Error:

    if(VALID_HANDLE(hFile))
    {
        if(!CloseHandle(hFile))
        {
            ERRFAIL("CloseHandle()");
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
TESTPROCAPI Tst_Depth31(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;
    UINT i = 0;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    TCHAR szPathBuf[MAX_PATH];
    TCHAR szFile[MAX_PATH];
    DWORD dwData = 0;
    DWORD dwWritten = 0;
    DWORD offset;
    DWORD maxIterations = ITERATIONS;
    
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

    offset = lpFTE->dwUserData;

    _stprintf(szFile, _T("%s\\%s.DAT"), szPathBuf, TEST_FILE_NAME);
    LOG(_T("Creating file %s"), szFile);
    hFile = CreateFile(szFile, GENERIC_READ | GENERIC_WRITE, 
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, 
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(INVALID_HANDLE(hFile))
    {
        ERRFAIL("CreateFile()");
        goto Error;
        
    }

    // see if this FSD supports RFWS, WFWS
    if(!ReadFileWithSeek(hFile, NULL, 0, NULL, NULL, 0, 0))
    {
        if(ERROR_NOT_SUPPORTED == GetLastError())
        {
            LOG(_T("ReadFileWithSeek() failed on %s; FSD does not support this API"));
            retVal = TPR_PASS;
            goto Error;
        }
        ERRFAIL("ReadFileWithSeek()");
        goto Error;
    }

    LOG(_T("Writing data to %s at %u offsets using WriteFileWithSeek()..."), szFile, offset);
    // write data to different offsets in the file
    for(i = 0; i < maxIterations ; i++)
    {
        dwData = i;
        LOG(_T("File %s: writing 0x%08X at offset %u"), szFile, dwData, i*offset);
        if(!WriteFileWithSeek(hFile, &dwData, sizeof(dwData), &dwWritten, NULL, 
            i*offset, 0))
        {
            if(ERROR_DISK_FULL == GetLastError() ||
               ERROR_OUTOFMEMORY == GetLastError())
            {
                LOG(_T("Disk filled up while writing to file %s... continuing test"), szFile);
                // reset our maximum to the total number that we wrote
                maxIterations = i - 1;
                break;
            }
            ERRFAIL("WriteFileWithSeek()");
            goto Error;
        }
        
    }

    LOG(_T("Reading data from %s using ReadFileWithSeek() and verifying..."), szFile);
    // verify the data using RFWS
    for(i = 0; i < maxIterations; i++)
    {
        if(!ReadFileWithSeek(hFile, &dwData, sizeof(dwData), &dwWritten, NULL, 
            i*offset, 0))
        {
            ERRFAIL("ReadFileWithSeek()");
            goto Error;
        }
        LOG(_T("File %s: read 0x%08X at offset %u"), szFile, dwData, i*offset);
        if(dwData != i)
        {
            FAIL("data mismatch");
            goto Error;
        }
    }

    LOG(_T("Reading data from %s using SetFilePointer() and ReadFile() and verifying..."), szFile);
    // verify the data using SetFilePointer and ReadFile
    for(i = 0; i < maxIterations; i++)
    {
        if(0xFFFFFFFF == SetFilePointer(hFile, i*offset, NULL, FILE_BEGIN))
        {
            ERRFAIL("SetFilePointer()");
            goto Error;
        }
        if(!ReadFile(hFile, &dwData, sizeof(dwData), &dwWritten, NULL))
        {
            ERRFAIL("ReadFile()");
            goto Error;
        }
        LOG(_T("File %s: read 0x%08X at offset %u"), szFile, dwData, i*offset);
        if(dwData != i)
        {
            FAIL("data mismatch");
            goto Error;
        }
    }
        
    // success
    retVal = TPR_PASS;
    
Error:

    if(VALID_HANDLE(hFile))
    {
        if(!CloseHandle(hFile))
        {
            ERRFAIL("CloseHandle()");
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
