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
#include "TestProc.H"
#include "DiskHelp.H"
#include "utility.h"

#define SZ_PART_NAME        L"TestPart%u"

SECTORNUM g_snPartSize = 0;
WCHAR g_szDskName[MAX_PATH] = L"";
WCHAR g_szPartName[MAX_PATH] = L"";
BYTE g_bPartType = 0;

extern SPS_SHELL_INFO *g_pShellInfo;


// --------------------------------------------------------------------
TESTPROCAPI 
Tst_Template(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;
    
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }

    // success
    retVal = TPR_PASS;
    
Error:
    return retVal;
}

// --------------------------------------------------------------------
TESTPROCAPI 
Tst_StoreBVT(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
//
//  Storage Manager BVT performs the following steps:
//
//      Mounts block driver
//      Opens store on disk
//      Verifies store is unformatted
//      Retrieves store information
//      Tries to enumerate partitions on unformatted store
//      Formats store
//      Retrieves and saves store information
//      Closes store
//      Reopens store
//      Checks that store is still formatted
//      Retrieves store information and compares to previous value
// --------------------------------------------------------------------
{
    DWORD retVal = TPR_FAIL;
    HANDLE hStore = INVALID_HANDLE_VALUE;
    STOREINFO storeInfo1 = {0};
    STOREINFO storeInfo2 = {0};
    DWORD dwDiskCount = 0;
    
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        // If we use multi-thread to test each disk, then test will only pass only if all threads have "pass" results, 
        // which is impossible because most likely there are some disks which is not formatable and skips the test 
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 1;
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }
    if(uMsg == TPM_EXECUTE)
        if(!Zorch(g_pShellInfo->szDllCmdLine))
        {
            Zorchage(g_pKato); 
            return TPR_SKIP; 
        }

    dwDiskCount = QueryDiskCount();
    if (0 == dwDiskCount) 
    {
        LOG(TEXT("No disk found and skipping test."));
        retVal = TPR_SKIP;
        goto Error;
    }

    LOG(TEXT("Found %d disks"), dwDiskCount);

    for (DWORD dwDiskNumber = 0; dwDiskNumber < dwDiskCount; dwDiskNumber ++)
    { 
        memset(&storeInfo1, 0, sizeof(STOREINFO));
        memset(&storeInfo2, 0, sizeof(STOREINFO));
        storeInfo1.cbSize = sizeof(STOREINFO);
        storeInfo2.cbSize = sizeof(STOREINFO);

        LOG(TEXT("Testing disk %u"), dwDiskNumber);

        if(!IsAValidStore(dwDiskNumber))
        {
            LOG(TEXT("Disk %u is not a valid volume for partitioning, skipping this disk."), dwDiskNumber);
            retVal = TPR_SKIP;
            continue;
        }

        HRESULT hrFormat = S_OK;
        hStore = OpenAndFormatStore(dwDiskNumber, &storeInfo1, &hrFormat);
        if (INVALID_HANDLE(hStore))
        {
            if (FAILED(hrFormat) ) 
            {
                ERRFAIL("OpenAndFormatStore()");
                retVal = TPR_FAIL;
            }
            else if (hrFormat == S_FALSE) 
            {
                // Format can not happen on this disk, passing this BVT
                // by skipping this disk
                LOG(TEXT("Disk can not be formatted, skipping disk %u by design"), dwDiskNumber);
                retVal = TPR_SKIP;
            }
            goto Error;
        }

        // get store info on formatted store
        DETAIL("Calling GetStoreInfo() on formatted disk volume");
        if(!GetStoreInfo(hStore, &storeInfo1))
        {
            ERRFAIL("GetStoreInfo()");
            retVal = TPR_FAIL;
            goto Error;
        }
        LogStoreInfo(&storeInfo1);    

        // close the formatted store
        DETAIL("Calling CloseStore() on formatted disk volume");   
        CloseHandle(hStore);
        hStore = INVALID_HANDLE_VALUE;

        // again, open an HSTORE handle to the volume
        DETAIL("Calling OpenStore() on disk volume...");
        hStore = OpenStoreByIndex(dwDiskNumber);
        if(INVALID_HANDLE(hStore))
        {
            FAIL("OpenStoreByIndex()");
            retVal = TPR_FAIL;
            goto Error;
        }
        LOG(TEXT("Store opened on disk volume; store handle = 0x%08x"), hStore);

        // get store info and compare to previous store info
        DETAIL("Calling GetStoreInfo() on disk volume and comparing to previous info");
        if(!GetStoreInfo(hStore, &storeInfo2))
        {
            ERRFAIL("T_GetStoreInfo()");
            retVal = TPR_FAIL;
            goto Error;
        }
        if(memcmp(&storeInfo1, &storeInfo2, sizeof(STOREINFO)))
        {
            FAIL("T_GetStoreInfo() returned unexpected info");
            DETAIL("Original Info:");        
            LogStoreInfo(&storeInfo1);  
            DETAIL("New Info:");
            LogStoreInfo(&storeInfo2);
            retVal = TPR_FAIL;
            goto Error;
        }

        if(VALID_HANDLE(hStore))
            CloseHandle(hStore);

        // success
        LOG(TEXT("All tests passed on disk %u"), dwDiskNumber);
        retVal = TPR_PASS;
        break;
    }    
    
Error:
    if(VALID_HANDLE(hStore))
        CloseHandle(hStore);
    
    return retVal;
}

// --------------------------------------------------------------------
TESTPROCAPI 
Tst_PartBVT(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
//
//  Partition Manager BVT performs the following steps:
//
//      Mounts block driver
//      Opens and formats store on disk
//      Attempts to enumerate non-existing partition
//      Creates a partition 1/4 of disk
//      Checks storage info (free sectors) was updated
//      Creates a partition size of snBiggestPartCreatable
//      Enumerates and verifies new partitions
//      Renames the second partition
//      Opens handle to reneamed partition
//      Closes partition handle
//      Deletes the first partition
//      Verifies only the second partition enumerates
//      Create another partition size of new snBiggestPartCreatable
//      Enumerates and verifies partitions
//      Gets partition info for both partitions
//      Closes store handle
//      Opens store handle
//      Enumerates partitions
//      Verifies partition data
//      Closes store handle
//
// --------------------------------------------------------------------
{
    DWORD retVal = TPR_FAIL;
    HANDLE hStore = INVALID_HANDLE_VALUE;
    WCHAR szPart1[PARTITIONNAMESIZE] = L"";
    WCHAR szPart2[PARTITIONNAMESIZE] = L"";
    WCHAR szPart3[PARTITIONNAMESIZE] = L"";

    HANDLE hPart = INVALID_HANDLE_VALUE;
    HANDLE hSearch = INVALID_HANDLE_VALUE;
    STOREINFO storeInfo = {0};
    PARTINFO partInfo1 = {0};
    PARTINFO partInfo2 = {0};
    PARTINFO partInfoTmp = {0};
    SECTORNUM cSectors = 0;
    SECTORNUM cOldFreeSectors = 0;

    DWORD dwDiskNumber = 0;
    DWORD dwDiskCount = 0;
    
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 1;
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }
    if(uMsg == TPM_EXECUTE)
    {
        if(!Zorch(g_pShellInfo->szDllCmdLine))
        {
            Zorchage(g_pKato); 
            return TPR_SKIP; 
        }
    }

    dwDiskCount = QueryDiskCount();   

    if (0 == dwDiskCount) 
    {
        retVal = TPR_SKIP;
        LOG(TEXT("No disk found. Skipping test"));
        goto Error;
    }

    for (DWORD dwDiskNumber = 0; dwDiskNumber < dwDiskCount; ++dwDiskNumber)
    {                
        memset(&storeInfo, 0, sizeof(STOREINFO));
        memset(&partInfo1, 0, sizeof(PARTINFO));
        memset(&partInfo2, 0, sizeof(PARTINFO));
        memset(&partInfoTmp, 0, sizeof(PARTINFO));

        storeInfo.cbSize = sizeof(STOREINFO);
        partInfo1.cbSize = sizeof(PARTINFO);
        partInfo2.cbSize = sizeof(PARTINFO);
        partInfoTmp.cbSize = sizeof(PARTINFO);

        LOG(TEXT("Testing disk %u"), dwDiskNumber);
        if(!IsAValidStore(dwDiskNumber))
        {
            LOG(TEXT("Disk %u is not a valid volume for partitioning, skipping test."), dwDiskNumber);
            retVal = TPR_SKIP;
            continue;
        }

        // open and format store
        HRESULT hrFormat = S_OK;
        hStore = OpenAndFormatStore(dwDiskNumber, &storeInfo, &hrFormat);
        if (INVALID_HANDLE(hStore))
        {
            if (FAILED(hrFormat) ) {
                retVal = TPR_FAIL;
                ERRFAIL("OpenAndFormatStore()");
                goto Error;
            }
            else if (hrFormat == S_FALSE) {
                // Format can not happen on this disk, passing this BVT
                // by skipping this disk
                LOG(TEXT("Disk can not be formatted, skipping disk %u by design"), dwDiskNumber);
                retVal = TPR_SKIP;
                continue;
            }
        }
        DETAIL("Store opened and formatted successfully");

        // try to enumerate non-existing partitions
        DETAIL("Attempting to enumerate non-existing partitions");
        hSearch = FindFirstPartition(hStore, &partInfoTmp);
        if(!INVALID_HANDLE(hSearch))
        {
            FAIL("FindFirstPartition() succeeded when it should have failed");
            retVal = TPR_FAIL;
            goto Error;
        }    
        if(ERROR_NO_MORE_ITEMS != GetLastError())
        {
            FAIL("FindFirstPartition() returned incorrect error code on failure");
            retVal = TPR_FAIL;
            goto Error;
        }
        FindClosePartition(hSearch);
        hSearch= INVALID_HANDLE_VALUE;
        DETAIL("Successfully enumerated zero partitions as expected");    

        // create first partition on disk
        DETAIL("Creating first partition on volume");
        cSectors = storeInfo.snFreeSectors / 4; // use a quarter of the disk
        cOldFreeSectors = storeInfo.snFreeSectors;
        swprintf_s(szPart1,_countof(szPart1), SZ_PART_NAME, 1);
        if(!CreateAndVerifyPartition(hStore, szPart1, NULL, cSectors, TRUE, &partInfo1))
        {
            FAIL("CreateAndVerifyPartition()");
            retVal = TPR_FAIL;
            goto Error;
        }
        LOG(TEXT("Successfully created new auto-type partition \"%s\" of size %u"), szPart1, cSectors);
        LogPartInfo(&partInfo1);
        // make sure store info was updated accordingly
        DETAIL("Verifying store info was updated");
        if(!GetStoreInfo(hStore, &storeInfo))
        {
            FAIL("GetStoreInfo()");
            retVal = TPR_FAIL;
            goto Error;
        }
        if(partInfo1.snNumSectors > (storeInfo.snFreeSectors - cOldFreeSectors))
        {
            LOG(TEXT("requested sectors %u is greater than actual disk sectors used %u"), 
                cSectors, storeInfo.snFreeSectors - cOldFreeSectors);
            FAIL("Store info not updated correctly after partition creation");
            retVal = TPR_FAIL;
            goto Error;
        }
        DETAIL("Store info updated correctly after partition creation");    

        // open a second partition taking up the rest of the disk
        DETAIL("Creating a second partition");
        swprintf_s(szPart2,_countof(szPart2), SZ_PART_NAME, 2);
        cSectors = storeInfo.snBiggestPartCreatable;
        if(!CreateAndVerifyPartition(hStore, szPart2, PART_DOS32, cSectors, FALSE, &partInfo2))
        {
            FAIL("CreateAndVerifyPartition()");
            retVal = TPR_FAIL;
            goto Error;
        }
        LOG(TEXT("Successfully created new FAT32 partition \"%s\" of size %u"), szPart2, cSectors);

        // make sure the new partitions enumerates
        DETAIL("Enumerating new partitions");
        hSearch = FindFirstPartition(hStore, &partInfoTmp);
        if(INVALID_HANDLE(hSearch))
        {
            FAIL("FindFirstPartition()");
            retVal = TPR_FAIL;
            goto Error;
        }    

        // the partitions can be enumerated in any order
        if(memcmp(&partInfo1, &partInfoTmp, sizeof(PARTINFO)))
        {       
            // if it does not match partition 1, try partition 2
            if(memcmp(&partInfo2, &partInfoTmp, sizeof(PARTINFO)))
            {
                // it doesn't  match either partition
                FAIL("FindFirstPartition() info differs from GetPartitionInfo() info");
                LogPartInfo(&partInfoTmp);
                LogPartInfo(&partInfo1);
                LogPartInfo(&partInfo2);
                retVal = TPR_FAIL;
                goto Error;
            }
            LOG(TEXT("Found first partition \"%s\""), partInfoTmp.szPartitionName);

            // enumerate the next partition, it should match parition 1
            if(!FindNextPartition(hSearch, &partInfoTmp))
            {
                FAIL("FindNextPartition()");
                retVal = TPR_FAIL;
                goto Error;
            }
            if(memcmp(&partInfo1, &partInfoTmp, sizeof(PARTINFO)))
            {
                FAIL("FindNextPartition() info differs from GetPartitionInfo() info");
                LogPartInfo(&partInfoTmp);
                LogPartInfo(&partInfo1);
                retVal = TPR_FAIL;
                goto Error;
            }
            LOG(TEXT("Found second partition \"%s\""), partInfoTmp.szPartitionName);
            DETAIL("NOTE: Partitions were enumerated in opposite order of disk location");
        }    
        else
        {
            // partition matched partition 1        
            LOG(TEXT("Found first partition \"%s\""), partInfoTmp.szPartitionName);

            // enumerate the second partition, it should match partition 2
            if(!FindNextPartition(hSearch, &partInfoTmp))
            {
                FAIL("FindNextPartition()");
                retVal = TPR_FAIL;
                goto Error;
            }
            if(memcmp(&partInfo2, &partInfoTmp, sizeof(PARTINFO)))
            {
                FAIL("FindNextPartition() info differs from GetPartitionInfo() info");
                LogPartInfo(&partInfoTmp);
                LogPartInfo(&partInfo2);
                retVal = TPR_FAIL;
                goto Error;
            }
            LOG(TEXT("Found second partition \"%s\""), partInfoTmp.szPartitionName);
        }

        // should not enumerate any more partitions    
        if(FindNextPartition(hSearch, &partInfoTmp))
        {
            FAIL("Enumerated a third partition when there should not be any");
            retVal = TPR_FAIL;
            goto Error;
        }
        if(ERROR_NO_MORE_ITEMS != GetLastError())
        {
            FAIL("FindNextPartition() returned incorrect error code on failure");
            retVal = TPR_FAIL;
            goto Error;
        }
        FindClosePartition(hSearch); // no return value
        hSearch = INVALID_HANDLE_VALUE;
        DETAIL("Successfully enumerated and verified partitions");

        // rename the partition
        hPart = OpenPartition(hStore, szPart2);
        if(INVALID_HANDLE(hPart)) {
            FAIL("OpenPartition() failed");
            retVal = TPR_FAIL;
            goto Error;
        }
        swprintf_s(szPart3,_countof(szPart3), SZ_PART_NAME, 3);
        LOG(TEXT("renaming partition \"%s\" to \"%s\""), szPart2, szPart3);

        // Some BSPs do not support renaming the partition.  In that case, skip rest of the test
        if(!RenamePartition(hPart, szPart3))
        {
            if (ERROR_NOT_SUPPORTED == GetLastError())
            {
                LOG(TEXT("The file system driver does not support partition renaming.  Skipping this test.") );
                retVal = TPR_SKIP;
                continue;
            }
            else
            {
                FAIL("RenamePartition()");
                retVal = TPR_FAIL;
                goto Error;
            }
        }
        CloseHandle(hPart);
        hPart = INVALID_HANDLE_VALUE;

        // delete first partition    
        if(!DismountStore(hStore))
        {
            FAIL("DismountStore()");
            retVal = TPR_FAIL;
            goto Error;
        }
        DETAIL("Deleting first partition");
        if(!DeletePartition(hStore, szPart1))
        {
            FAIL("DeletePartition()");
            retVal = TPR_FAIL;
            goto Error;
        }
        LOG(TEXT("Partition \"%s\" deleted"), szPart1);

        // make sure second partition still enumerates
        DETAIL("Enumerating partitions");
        hSearch = FindFirstPartition(hStore, &partInfoTmp);
        if(INVALID_HANDLE(hSearch))
        {
            FAIL("FindFirstPartition()");
            retVal = TPR_FAIL;
            goto Error;
        }    
        // verify only the name because the partition driver could have moved the partition
        if(wcscmp(szPart3, partInfoTmp.szPartitionName))
        {
            LOG(TEXT("Name of found partition \"%s\" does not match only known exisiting partition \"%s\""),
                partInfoTmp.szPartitionName, szPart3);
            FAIL("Found a partition that should not exist");
            retVal = TPR_FAIL;
            goto Error;
        }
        LOG(TEXT("Found partition \"%s\""), partInfoTmp.szPartitionName);   
        // should not enumerate any more partitions
        if(FindNextPartition(hSearch, &partInfoTmp))
        {
            FAIL("Enumerated a second partition when there should not be any");
            retVal = TPR_FAIL;
            goto Error;
        }
        if(ERROR_NO_MORE_ITEMS != GetLastError())
        {
            FAIL("T_FindPartitionNext() returned incorrect error code on failure");
            retVal = TPR_FAIL;
            goto Error;
        }
        FindClosePartition(hSearch); // no return value
        hSearch = INVALID_HANDLE_VALUE;
        DETAIL("Successfully enumerated and verified remaining partition");

        // attempt to create another partition
        DETAIL("Creating another partition");
        swprintf_s(szPart1, _countof(szPart1), SZ_PART_NAME, 4);
        if(!GetStoreInfo(hStore, &storeInfo))
        {
            FAIL("GetStoreInfo()");
            retVal = TPR_FAIL;
            goto Error;
        }
        if(0 == storeInfo.snFreeSectors)
        {
            FAIL("No free sectors on store");
            retVal = TPR_FAIL;
            goto Error;
        }
        if(0 == storeInfo.snBiggestPartCreatable)
        {
            FAIL("Biggest creatable partition is set to zero");
            retVal = TPR_FAIL;
            goto Error;
        }
        if(!CreateAndVerifyPartition(hStore, szPart1, NULL, 
            storeInfo.snBiggestPartCreatable, TRUE, &partInfoTmp))
        {
            FAIL("CreateAndVerifyPartition()");
            retVal = TPR_FAIL;
            goto Error;
        }

        if(partInfoTmp.snNumSectors != partInfo1.snNumSectors)
        {
            LOG(TEXT("new partition has %u sectors, old partition had %u sectors") );
            FAIL("Sectors were either gained or lost during deletion/creation of partitions");
            retVal = TPR_FAIL;
            goto Error;
        }

        LOG(TEXT("Correctly created new partition \"%s\""), szPart1);

        hPart = OpenPartition(hStore, szPart1);
        if(INVALID_HANDLE(hPart))
        {
            FAIL("OpenPartition() failed");
            retVal = TPR_FAIL;
            goto Error;
        }
        // get info before closing
        if(!GetPartitionInfo(hPart, &partInfo1))
        {
            FAIL("T_GetPartitionInfo()");
            retVal = TPR_FAIL;
            goto Error;
        }
        CloseHandle(hPart);
        hPart = INVALID_HANDLE_VALUE;

        hPart = OpenPartition(hStore, szPart3);
        if(INVALID_HANDLE(hPart))
        {
            FAIL("OpenPartition() failed");
            retVal = TPR_FAIL;
            goto Error;
        }    
        if(!GetPartitionInfo(hPart, &partInfo2))
        {
            FAIL("T_GetPartitionInfo()");
            retVal = TPR_FAIL;
            goto Error;
        }
        CloseHandle(hPart);
        hPart = INVALID_HANDLE_VALUE;

        CloseHandle(hStore);
        hStore = INVALID_HANDLE_VALUE;
        DETAIL("Closed store");  

        // reopen store and verify changes were permanent
        DETAIL("Re-opening store");
        hStore = OpenStoreByIndex(dwDiskNumber);
        if(INVALID_HANDLE(hStore))
        {
            FAIL("T_OpenStore()");
            retVal = TPR_FAIL;
            goto Error;
        }
        // enumerate the partition
        DETAIL("Enumerating partitions");
        hSearch = FindFirstPartition(hStore, &partInfoTmp);
        if(INVALID_HANDLE(hSearch))
        {
            FAIL("FindFirstPartition()");
            retVal = TPR_FAIL;
            goto Error;
        }
        LOG(TEXT("Found first partition \"%s\""), partInfoTmp.szPartitionName);

        // verify that the partition info matches
        if(partInfoTmp.snNumSectors == partInfo1.snNumSectors &&
            partInfoTmp.dwAttributes == partInfo1.dwAttributes &&
            partInfoTmp.bPartType == partInfo1.bPartType)
        {
            LOG(TEXT("First enumerated partition, %s, matches created partition named %s"), 
                partInfoTmp.szPartitionName, partInfo1.szPartitionName);
            if(wcscmp(partInfoTmp.szPartitionName, partInfo1.szPartitionName))
                DETAIL("Warning: partition name was not preserved");
            // zero out the partition info so the next parition can't possibly
            // match it (in case the same partition gets enum'd twice)
            memset(&partInfo1, 0, sizeof(PARTINFO));
        }
        else if(partInfoTmp.snNumSectors == partInfo2.snNumSectors &&
            partInfoTmp.dwAttributes == partInfo2.dwAttributes &&
            partInfoTmp.bPartType == partInfo2.bPartType)
        {
            LOG(TEXT("First enumerated partition, %s, matches created partition named %s"), 
                partInfoTmp.szPartitionName, partInfo2.szPartitionName);
            if(wcscmp(partInfoTmp.szPartitionName, partInfo2.szPartitionName))
                DETAIL("Warning: partition name was not preserved");
            // zero out the partition info so the next parition can't possibly
            // match it (in case the same partition gets enum'd twice)
            memset(&partInfo2, 0, sizeof(PARTINFO));
        }

        // enumerate the second partition
        if(!FindNextPartition(hSearch, &partInfoTmp))
        {
            FAIL("FindNextPartition()");
            retVal = TPR_FAIL;
            goto Error;
        }
        LOG(TEXT("Found second partition \"%s\""), partInfoTmp.szPartitionName);

        // verify that the partition info matches
        if(partInfoTmp.snNumSectors == partInfo2.snNumSectors &&
            partInfoTmp.dwAttributes == partInfo2.dwAttributes &&
            partInfoTmp.bPartType == partInfo2.bPartType)
        {
            LOG(TEXT("First enumerated partition, %s, matches partition created as %s"), 
                partInfoTmp.szPartitionName, partInfo2.szPartitionName);
            if(wcscmp(partInfoTmp.szPartitionName, partInfo2.szPartitionName))
                DETAIL("Warning: partition name was not preserved");
            // zero out the partition info so the next parition can't possibly
            // match it (in case the same partition gets enum'd twice)
            memset(&partInfo2, 0, sizeof(PARTINFO));
        }
        else if(partInfoTmp.snNumSectors == partInfo1.snNumSectors &&
            partInfoTmp.dwAttributes == partInfo1.dwAttributes &&
            partInfoTmp.bPartType == partInfo1.bPartType)
        {
            LOG(TEXT("First enumerated partition, %s, matches partition created as %s"), 
                partInfoTmp.szPartitionName, partInfo1.szPartitionName);
            if(wcscmp(partInfoTmp.szPartitionName, partInfo1.szPartitionName))
                DETAIL("Warning: partition name was not preserved");
            // zero out the partition info so the next parition can't possibly
            // match it (in case the same partition gets enum'd twice)
            memset(&partInfo1, 0, sizeof(PARTINFO));
        }

        // should not enumerate any more partitions    
        if(FindNextPartition(hSearch, &partInfoTmp))
        {
            FAIL("Enumerated a third partition when there should not be any");
            retVal = TPR_FAIL;
            goto Error;
        }
        if(ERROR_NO_MORE_ITEMS != GetLastError())
        {
            FAIL("T_FindPartitionNext() returned incorrect error code on failure");
            retVal = TPR_FAIL;
            goto Error;
        }
        FindClosePartition(hSearch); // no return value
        hSearch = INVALID_HANDLE_VALUE;
        DETAIL("Successfully enumerated partitions");

        CloseHandle(hStore);
        hStore = INVALID_HANDLE_VALUE;
        
        // close search handle
        if(!INVALID_HANDLE(hSearch))
        {
            FindClosePartition(hSearch);
            hSearch = INVALID_HANDLE_VALUE;
        }

        // close partition handle
        if(!INVALID_HANDLE(hPart))
        {
            CloseHandle(hPart);
            hPart = INVALID_HANDLE_VALUE;
        }

        // close the store handle if necessary
        if(!INVALID_HANDLE(hStore))
        {   
            CloseHandle(hStore);
            hStore = INVALID_HANDLE_VALUE;
        }

        // success
        LOG(TEXT("All tests passed on disk %u"), dwDiskNumber);
        retVal = TPR_PASS;
        break;
    }
    
Error:
    // close search handle
    if(!INVALID_HANDLE(hSearch))
    {
        FindClosePartition(hSearch);
        hSearch = INVALID_HANDLE_VALUE;
    }

    // close partition handle
    if(!INVALID_HANDLE(hPart))
    {
        CloseHandle(hPart);
        hPart = INVALID_HANDLE_VALUE;
    }
    
    // close the store handle if necessary
    if(!INVALID_HANDLE(hStore))
    {   
        CloseHandle(hStore);
        hStore = INVALID_HANDLE_VALUE;
    }
    
    return retVal;
}

// --------------------------------------------------------------------
TESTPROCAPI 
Tst_MultiPart(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
//
//  This test does the following:
//      
//      1) Installs driver
//      2) Open and format store on disk volume
//      3) Create partitions of increasing size until the disk is full
//         and the last CreatePartition() call fails
//      4) enumerate all partitions, verify all are enumerated
//      5) delete every other partition
//      6) repeat 4-6 until no more partitions remain
//      7) verify store info is the same as it was after first format
//
// --------------------------------------------------------------------
{
   
    UINT cPartition = 0;
    UINT cPartEnum = 0;
    UINT cPartDeleted = 0;
    DWORD retVal = TPR_FAIL;
    BOOL *pfPartDeleted = NULL;
    HANDLE hStore = INVALID_HANDLE_VALUE;
    WCHAR szPart[PARTITIONNAMESIZE] = L"";

    HANDLE hSearch = INVALID_HANDLE_VALUE;
    SECTORNUM cSectors = 0;
    PARTINFO partInfo = {0};
    STOREINFO storeInfo = {0};

    DWORD dwDiskNumber = 0;
    DWORD dwDiskCount = 0;

    partInfo.cbSize = sizeof(PARTINFO);
    storeInfo.cbSize = sizeof(STOREINFO);
    
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 1;
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }
    if (uMsg == TPM_EXECUTE)
    {
        if (!Zorch(g_pShellInfo->szDllCmdLine))
        {
            Zorchage(g_pKato); 
            return TPR_SKIP; 
        }
    }

    dwDiskCount = QueryDiskCount();
    if (0 == dwDiskCount) 
    {
        LOG(TEXT("No disks found. Skipping test"));
        retVal = TPR_SKIP;
        goto Error;
    }

    for (DWORD dwDiskNumber = 0; dwDiskNumber < dwDiskCount; ++dwDiskNumber)
    {    
        LOG(TEXT("Testing disk %u"), dwDiskNumber);
        if(!IsAValidStore(dwDiskNumber) )
        {
            LOG(TEXT("Disk %u is not a valid volume for partitioning, skipping test."), dwDiskNumber);
            retVal = TPR_SKIP;
            continue;
        }

        // open and format store
        HRESULT hrFormat = S_OK;
        hStore = OpenAndFormatStore(dwDiskNumber, &storeInfo, &hrFormat);
        if(INVALID_HANDLE(hStore))
        {
            if (FAILED(hrFormat) ) 
            {
                ERRFAIL("OpenAndFormatStore()");
                retVal = TPR_FAIL;
                goto Error;
            }
            else if (hrFormat == S_FALSE) 
            {
                // Format can not happen on this disk, passing this BVT
                // by skipping this disk
                LOG(TEXT("Disk can not be formatted, skipping disk %u by design"), dwDiskNumber);
                retVal = TPR_SKIP;
                continue;
            }
        }
        DETAIL("Store opened and formatted successfully");

        // create partitions of increasing size until failure
        // partitions won't necessarily be created with the requested size

        // the max should never be reached
        cPartition = 0;
        for (cSectors = 1; cSectors < storeInfo.snNumSectors; cSectors++)
        {
            swprintf_s(szPart, _countof(szPart), SZ_PART_NAME, cSectors);
            if (!CreateAndVerifyPartition(hStore, szPart, 0, cSectors, TRUE, NULL))
            {
                LOG(TEXT("Failed to create a partition of size %u sectors"), cSectors);
                break;
            }
            else
            {
                LOG(TEXT("Created partition \"%s\" of size %u sectors"), szPart, cSectors);
                cPartition++;
            }
        }

        // no partitions were created
        if (0 == cPartition)
        {
            FAIL("Unable to create partitions on the store");
            retVal = TPR_FAIL;
            goto Error;
        }

        if (!DismountStore(hStore))
        {
            ERRFAIL("DismountStore()");
            retVal = TPR_FAIL;
            goto Error;
        }

        // allocate an array to keep track of which partitions have been deleted
        pfPartDeleted = (BOOL*)VirtualAlloc(NULL, sizeof(BOOL)*cPartition, MEM_COMMIT, PAGE_READWRITE);
        if (NULL == pfPartDeleted)
        {
            ERRFAIL("VirtualAlloc()");
            retVal = TPR_FAIL;
            goto Error;
        }
        memset(pfPartDeleted, FALSE, sizeof(BOOL)*cPartition);

        // perform test while there are still partition on the disk
        while (cPartition)
        {   
            cPartEnum = 0;
            cPartDeleted = 0;
            hSearch = FindFirstPartition(hStore, &partInfo);
            if (INVALID_HANDLE(hSearch)) {
                FAIL("FindFirstPartition failed");
                retVal = TPR_FAIL;
                goto Error;
            }
            do
            {
                // search for partitions
                LOG(TEXT("Found partition \"%s\" of size %u sectors"), partInfo.szPartitionName, partInfo.snNumSectors);
                cPartEnum++;

                // try to parse out the size from the name
                cSectors = 0;
                if (0 == swscanf_s(partInfo.szPartitionName, SZ_PART_NAME, &cSectors) )
                {
                    // a defunct name
                    LOG(TEXT("Unable to find the size embedded in the partition name \"%s\""), partInfo.szPartitionName);
                    FAIL("Enumerated a partition that was not created");
                    retVal = TPR_FAIL;
                    goto Error;
                }

                // make sure this partition is not flagged as deleted
                if (pfPartDeleted[cSectors])
                {
                    LOG(TEXT("Partition \"%s\" is supposed to be deleted"), partInfo.szPartitionName);
                    FAIL("Enumerated a deleted partition");
                    retVal = TPR_FAIL;
                    goto Error;
                }

                // make sure the sector size matches the number in the file name
                if (cSectors > partInfo.snNumSectors)
                {
                    LOG(TEXT("Partition \"%s\" is size %I64u sectors when it should be %I64u sectors"),
                        partInfo.szPartitionName, partInfo.snNumSectors, cSectors);
                    FAIL("Partition size mismatch");
                    retVal = TPR_FAIL;
                    goto Error;
                }

                // delete every other partition starting with the first one enumerated
                if(cPartEnum % 2)
                {
                    if(!DeletePartition(hStore, partInfo.szPartitionName))
                    {
                        LOG(TEXT("Unable to delete partition \"%s\""), partInfo.szPartitionName);
                        ERRFAIL("DeletePartition()");
                        retVal = TPR_FAIL;
                        goto Error;
                    }
                    // update delete info
                    cPartDeleted++;
                    pfPartDeleted[cSectors] = TRUE;
                }
            } while(FindNextPartition(hSearch, &partInfo));
            FindClosePartition(hSearch);
            hSearch = INVALID_HANDLE_VALUE;

            LOG(TEXT("Found %u of %u partitions"), cPartEnum, cPartition);
            LOG(TEXT("Deleted %u of %u partitions"), cPartDeleted, cPartition);

            // make sure all partitions were enumerated
            if(cPartEnum != cPartition)
            {
                FAIL("Not all partitions were enumerated");
                retVal = TPR_FAIL;
                goto Error;
            }

            // should never delete more partitions than exist
            if(cPartDeleted > cPartition)
            {
                FAIL("Deleted more partitions than exist");
                retVal = TPR_FAIL;
                goto Error;
            }   

            // update the remaining partition count
            cPartition -= cPartDeleted;
        }    

        if(VALID_HANDLE(hSearch))
        {
            FindClosePartition(hSearch);
        }

        if(pfPartDeleted)
        {
            VirtualFree(pfPartDeleted, 0, MEM_RELEASE);
        }

        if(VALID_HANDLE(hStore)) 
        {
            CloseHandle(hStore);
        }

        // success
        LOG(TEXT("All tests passed on disk %u"), dwDiskNumber);
        retVal = TPR_PASS;
        break;
    }
    
Error:

    if(VALID_HANDLE(hSearch))
    {
        FindClosePartition(hSearch);
    }
    
    if(pfPartDeleted)
    {
        VirtualFree(pfPartDeleted, 0, MEM_RELEASE);
    }
    
    if(VALID_HANDLE(hStore)) 
    {
        CloseHandle(hStore);
    }
    
    return retVal;
}

// --------------------------------------------------------------------
TESTPROCAPI 
Tst_PartRename(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
//
//  This test does the following:
//  
//      Installs driver
//      Opens/formats a store on the disk volume
//      Creates a partition
//      Attempts to create another partition of the same name
//      Creates a second partition with a different name
//      Attempts to rename partition 1 the same as partition 2
//      Attempts to rename partition 2 the same as partition 1
//      Renames partition 1 to a temporary name
//      Renames partition 2 to partition 1's original name
//      Renames partition 1 to partition 2's original name
// --------------------------------------------------------------------
{
    DWORD retVal = TPR_FAIL;
    HANDLE hStore = INVALID_HANDLE_VALUE;
    WCHAR szPart1[PARTITIONNAMESIZE] = L"";
    WCHAR szPart2[PARTITIONNAMESIZE] = L"";

    HANDLE hPart = INVALID_HANDLE_VALUE;
    SECTORNUM cPartitionSize = 0;
    PARTINFO partInfo1 = {0};
    PARTINFO partInfo2 = {0};
    PARTINFO partInfoTmp = {0};
    STOREINFO storeInfo = {0};

    DWORD dwDiskNumber = 0;
    DWORD dwDiskCount = 0;

    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 1;
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }
    if(!Zorch(g_pShellInfo->szDllCmdLine))
    {
        Zorchage(g_pKato); 
        return TPR_SKIP; 
    }
    
    dwDiskCount = QueryDiskCount();
    if (0 == dwDiskCount) 
    {
        retVal = TPR_SKIP;
        LOG(TEXT("No disk found. Skipping test"));
        goto Error;
    }

    for (DWORD dwDiskNumber = 0; dwDiskNumber < dwDiskCount; ++dwDiskNumber)
    {
        memset(&partInfo1, 0, sizeof(PARTINFO));
        memset(&partInfo2, 0, sizeof(PARTINFO));
        memset(&partInfoTmp, 0, sizeof(PARTINFO));
        memset(&storeInfo, 0, sizeof(PARTINFO));

        partInfo1.cbSize = sizeof(PARTINFO);
        partInfo2.cbSize = sizeof(PARTINFO);
        partInfoTmp.cbSize = sizeof(PARTINFO);
        storeInfo.cbSize = sizeof(STOREINFO);

        LOG(TEXT("Testing disk %u"), dwDiskNumber);
        if(!IsAValidStore(dwDiskNumber))
        {
            LOG(TEXT("Disk %u is not a valid volume for partitioning, skipping test."), dwDiskNumber);
            retVal = TPR_SKIP;
            continue;
        }

        // open and format store
        HRESULT hrFormat = S_OK;
        hStore = OpenAndFormatStore(dwDiskNumber, &storeInfo, &hrFormat);
        if(INVALID_HANDLE(hStore))
        {
            if (FAILED(hrFormat) ) {
                ERRFAIL("OpenAndFormatStore()");
                retVal = TPR_FAIL;
                break;
            }
            else if (hrFormat == S_FALSE) {
                // Format can not happen on this disk, passing this BVT
                // by skipping this disk
                LOG(TEXT("Disk can not be formatted, skipping disk %u by design"), dwDiskNumber);
                retVal = TPR_SKIP;
                continue;
            }
        }
        DETAIL("Store opened and formatted successfully");

        cPartitionSize = storeInfo.snBiggestPartCreatable / 3;

        // create partition 1
        swprintf_s(szPart1, _countof(szPart1), SZ_PART_NAME, 1);
        if(!CreatePartition(hStore, szPart1, cPartitionSize))
        {
            FAIL("CreatePartition()");
            retVal = TPR_FAIL;
            goto Error;
        }
        LOG(TEXT("Created partition \"%s\" of size %u sectors"), szPart1, cPartitionSize);
        hPart = OpenPartition(hStore, szPart1);
        if(INVALID_HANDLE(hPart))
        {
            FAIL("OpenPartition()");
            retVal = TPR_FAIL;
            goto Error;
        }
        // get info about the partition
        if(!GetPartitionInfo(hPart, &partInfo1))
        {
            FAIL("GetPartitionInfo()");
            retVal = TPR_FAIL;
            goto Error;
        }
        LogPartInfo(&partInfo1);
        CloseHandle(hPart);
        hPart = INVALID_HANDLE_VALUE;

        // attempt to create another partition with the same name
        if(CreatePartition(hStore, szPart1, cPartitionSize))
        {
            FAIL("CreatePartition() succeeded when partition name was already in use");
            retVal = TPR_FAIL;
            goto Error;
        }
        if(ERROR_ALREADY_EXISTS != GetLastError())
        {
            FAIL("CreatePartition() returned incorrect error code on failure");
            retVal = TPR_FAIL;
            goto Error;
        }
        DETAIL("Failed to create a partition as expected with a name already in use");

        // create the second partition with a different name
        swprintf_s(szPart2, _countof(szPart2), SZ_PART_NAME, 2);
        if(!CreatePartition(hStore, szPart2, cPartitionSize))
        {
            FAIL("CreatePartition()");
            retVal = TPR_FAIL;
            goto Error;

        }
        LOG(TEXT("Created partition \"%s\" of size %u sectors"), szPart2, cPartitionSize);
        hPart = OpenPartition(hStore, szPart2);
        if(INVALID_HANDLE(hPart))
        {
            FAIL("OpenPartition()");
            retVal = TPR_FAIL;
            goto Error;
        }
        // get info about the partition
        if(!GetPartitionInfo(hPart, &partInfo2))
        {
            FAIL("GetPartitionInfo()");
            retVal = TPR_FAIL;
            goto Error;
        }
        LogPartInfo(&partInfo2);

        // attempt to rename partition 2 with the same name as partition 1
        if(RenamePartition(hPart, szPart1))
        {
            FAIL("RenamePartition() succeeded with an already existing name");
            retVal = TPR_FAIL;
            goto Error;
        }

        // Error is expected
        DWORD errStatus = GetLastError();

        // Possible that FS driver does not support partition renaming
        if (ERROR_NOT_SUPPORTED == errStatus)
        {
            LOG(TEXT("The file system driver does not support partition renaming.  Skipping this test.") );
            retVal = TPR_SKIP;
            continue;
        }

        if(ERROR_ALREADY_EXISTS != errStatus)
        {
            FAIL("RenamePartition() returned incorrect error code on failure");
            retVal = TPR_FAIL;
            goto Error;
        }
        LOG(TEXT("Unable to rename partition \"%s\" to \"%s\" as expected"), szPart2, szPart1);
        CloseHandle(hPart);
        hPart = INVALID_HANDLE_VALUE;

        hPart = OpenPartition(hStore, szPart1);
        if(INVALID_HANDLE(hPart))
        {
            FAIL("OpenPartition()");
            retVal = TPR_FAIL;
            goto Error;
        }

        // attempt to rename partition 1 with the same name as partition 2
        if(RenamePartition(hPart, szPart2))
        {
            FAIL("RenamePartition() succeeded with an already existing name");
            retVal = TPR_FAIL;
            goto Error;
        }
        if(ERROR_ALREADY_EXISTS != GetLastError())
        {
            FAIL("RenamePartition() returned incorrect error code on failure");
            retVal = TPR_FAIL;
            goto Error;
        }
        LOG(TEXT("Unable to rename partition \"%s\" to \"%s\" as expected"), szPart1, szPart2);

        // rename partition 1 to a temporary name
        if(!RenamePartition(hPart, L"TEMP"))
        {
            LOG(TEXT("Unable to rename partition \"%s\" to \"TEMP\""), szPart1);
            FAIL("RenamePartition()");
            retVal = TPR_FAIL;
            goto Error;
        }
        LOG(TEXT("Renamed partition \"%s\" to \"TEMP\""), szPart1);

        CloseHandle(hPart);
        hPart = INVALID_HANDLE_VALUE;

        hPart = OpenPartition(hStore, szPart2);
        if(INVALID_HANDLE(hPart))
        {
            FAIL("OpenPartition()");
            retVal = TPR_FAIL;
            goto Error;
        }    

        // rename partition 2 to partition 1
        if(!RenamePartition(hPart, szPart1))
        {
            LOG(TEXT("Unable to rename partition \"%s\" to \"%s\""), szPart2, szPart1);
            FAIL("RenamePartition()");
            retVal = TPR_FAIL;
            goto Error;
        }
        LOG(TEXT("Renamed partition \"%s\" to \"%s\""), szPart2, szPart1);

        CloseHandle(hPart);
        hPart = INVALID_HANDLE_VALUE;

        hPart = OpenPartition(hStore, L"TEMP");
        if(INVALID_HANDLE(hPart))
        {
            FAIL("OpenPartition()");
            retVal = TPR_FAIL;
            goto Error;
        }        

        // rename temp to partition 2
        if(!RenamePartition(hPart, szPart2))
        {
            LOG(TEXT("Unable to rename partition \"TEMP\" to \"%s\""), szPart2);
            FAIL("RenamePartition()");
            retVal = TPR_FAIL;
            goto Error;
        }
        LOG(TEXT("Renamed partition \"TEMP\" to \"%s\""), szPart2);

        CloseHandle(hPart);
        hPart = INVALID_HANDLE_VALUE;

        hPart = OpenPartition(hStore, szPart1);
        if(INVALID_HANDLE(hPart))
        {
            FAIL("OpenPartition()");
            retVal = TPR_FAIL;
            goto Error;
        }        

        // verify that the partitions have swapped names only
        if(!GetPartitionInfo(hPart, &partInfoTmp))
        {
            ERRFAIL("GetPartitionInfo()");
            retVal = TPR_FAIL;
            goto Error;
        }
        // partition 1's name should have partition 2's info    
        if(wcscmp(partInfoTmp.szPartitionName, partInfo1.szPartitionName) ||
            memcmp(&partInfoTmp.snNumSectors, &partInfo2.snNumSectors, sizeof(partInfoTmp.snNumSectors)) ||
            memcmp(&partInfoTmp.ftCreated, &partInfo2.ftCreated, sizeof(partInfoTmp.ftCreated)) ||
            memcmp(&partInfoTmp.ftLastModified, &partInfo2.ftLastModified, sizeof(partInfoTmp.ftLastModified)) ||
            memcmp(&partInfoTmp.dwAttributes, &partInfo2.dwAttributes, sizeof(partInfoTmp.dwAttributes)) ||
            memcmp(&partInfoTmp.bPartType, &partInfo2.bPartType, sizeof(partInfoTmp.bPartType)) )
        {
            FAIL("partition info comparision failure");
            retVal = TPR_FAIL;
            goto Error;
        }
        LOG(TEXT("Partition \"%s\" has the correct information"), szPart1);

        CloseHandle(hPart);
        hPart = INVALID_HANDLE_VALUE;

        hPart = OpenPartition(hStore, szPart2);
        if(INVALID_HANDLE(hPart))
        {
            FAIL("OpenPartition()");
            retVal = TPR_FAIL;
            goto Error;
        }        

        // check partition 2
        if(!GetPartitionInfo(hPart, &partInfoTmp))
        {
            ERRFAIL("GetPartitionInfo()");
            retVal = TPR_FAIL;
            goto Error;
        }
        // partition 2's name should have partition 1's info    
        if(wcscmp(partInfoTmp.szPartitionName, partInfo2.szPartitionName) ||
            memcmp(&partInfoTmp.snNumSectors, &partInfo1.snNumSectors, sizeof(partInfoTmp.snNumSectors)) ||
            memcmp(&partInfoTmp.ftCreated, &partInfo1.ftCreated, sizeof(partInfoTmp.ftCreated)) ||
            memcmp(&partInfoTmp.ftLastModified, &partInfo1.ftLastModified, sizeof(partInfoTmp.ftLastModified)) ||
            memcmp(&partInfoTmp.dwAttributes, &partInfo1.dwAttributes, sizeof(partInfoTmp.dwAttributes)) ||
            memcmp(&partInfoTmp.bPartType, &partInfo1.bPartType, sizeof(partInfoTmp.bPartType)) )
        {
            FAIL("partition info comparision failure");
            retVal = TPR_FAIL;
            goto Error;
        }
        LOG(TEXT("Partition \"%s\" has the correct information"), szPart2);
        
        if (VALID_HANDLE(hPart))
        {
            CloseHandle(hPart);
        }

        if(VALID_HANDLE(hStore))
        {
            CloseHandle(hStore);
        }

        // success
        LOG(TEXT("All tests passed on disk %u"), ((TPS_EXECUTE *)tpParam)->dwThreadNumber);
        retVal = TPR_PASS;
        break;
    }
    
Error:
    if (VALID_HANDLE(hPart))
    {
        CloseHandle(hPart);
    }
    
    if(VALID_HANDLE(hStore))
    {
       CloseHandle(hStore);
    }
    
    return retVal;
}

// --------------------------------------------------------------------
TESTPROCAPI 
Tst_ReadWrite(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
//
//
//  This test does the following:
//
//      Partitions the disk into three sections
//      Verifies IO on these partitions
//      
// --------------------------------------------------------------------
{
    DWORD retVal = TPR_FAIL;
    UINT cPartition = 0;
    HANDLE hStore = INVALID_HANDLE_VALUE;
    WCHAR szPart[PARTITIONNAMESIZE] = L"";

    HANDLE hPart = INVALID_HANDLE_VALUE;    
    STOREINFO storeInfo = {0};
    PARTINFO partInfo = {0};
    SECTORNUM cPartitionSize = 0;

    DWORD dwDiskNumber = 0;
    DWORD dwDiskCount = 0;
    
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 1;
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }
    if(uMsg == TPM_EXECUTE)
    {
        if(!Zorch(g_pShellInfo->szDllCmdLine))
        {
            Zorchage(g_pKato); 
            return TPR_SKIP; 
        }
    }

    dwDiskCount = QueryDiskCount();
   
    if (0 == QueryDiskCount) 
    {
        retVal = TPR_SKIP;
        LOG(TEXT("No disk found. Skipping test"));
        goto Error;
    }

    for (DWORD dwDiskNumber = 0; dwDiskNumber < dwDiskCount; ++dwDiskNumber)
    {
        memset(&storeInfo, 0, sizeof(STOREINFO));
        memset(&storeInfo, 0, sizeof(PARTINFO));

        storeInfo.cbSize = sizeof(STOREINFO);
        partInfo.cbSize = sizeof(PARTINFO);

        LOG(TEXT("Testing disk %u"), dwDiskNumber);
        if(!IsAValidStore(dwDiskNumber))
        {
            LOG(TEXT("Disk %u is not a valid volume for partitioning, skipping test."), dwDiskNumber);
            retVal = TPR_SKIP;
            continue;
        }

        // open and format store
        HRESULT hrFormat = S_OK;
        hStore = OpenAndFormatStore(dwDiskNumber, &storeInfo, &hrFormat);
        if(INVALID_HANDLE(hStore))
        {
            if (FAILED(hrFormat) ) {
                ERRFAIL("OpenAndFormatStore()");
                retVal = TPR_FAIL;
                goto Error;
            }
            else if (hrFormat == S_FALSE) {
                // Format can not happen on this disk, passing this BVT
                // by skipping this disk
                LOG(TEXT("Disk can not be formatted, skipping disk %u by design"), dwDiskNumber);
                retVal = TPR_SKIP;
                continue;
            }
        }
        DETAIL("Store opened and formatted successfully");

        // create partitions
        for(cPartition = 1; cPartition <= PART_COUNT; cPartition++)
        {
            if(!GetStoreInfo(hStore, &storeInfo))
            {
                FAIL("GetStoreInfo()");
                retVal = TPR_FAIL;
                goto Error;
            }

            // leave enough room for remaining partitions -- 
            // they will all be roughly the same size
            cPartitionSize = storeInfo.snBiggestPartCreatable / (PART_COUNT - cPartition + 1);
            swprintf_s(szPart, _countof(szPart), SZ_PART_NAME, cPartition);
            LOG(TEXT("Requesting partition \"%s\" of size %I64u sectors"), szPart, cPartitionSize);
            if(!CreatePartition(hStore, szPart, cPartitionSize))
            {
                FAIL("CreatePartition()");
                retVal = TPR_FAIL;
                goto Error;
            }
            hPart = OpenPartition(hStore, szPart);
            if(!GetPartitionInfo(hPart, &partInfo))
            {
                FAIL("GetPartitionInfo()");
                retVal = TPR_FAIL;
                goto Error;
            }
            LOG(TEXT("Created partition \"%s\" of size %I64u sectors"), partInfo.szPartitionName, partInfo.snNumSectors);
            CloseHandle(hPart);
            hPart = INVALID_HANDLE_VALUE;
        }

        // write partitions
        for(cPartition = 1; cPartition <= PART_COUNT; cPartition++)
        {
            swprintf_s(szPart, _countof(szPart), SZ_PART_NAME, cPartition);
            if(!WritePartitionData(hStore, szPart))
            {
                FAIL("WritePartitionData()");
                retVal = TPR_FAIL;
                goto Error;
            }
        }

        // write and verify partitions
        for(cPartition = 1; cPartition <= PART_COUNT; cPartition++)
        {
            swprintf_s(szPart, _countof(szPart), SZ_PART_NAME, cPartition);
            if(!WriteAndVerifyPartition(hStore, szPart))
            {
                FAIL("WriteAndVerifyPartition()");
                retVal = TPR_FAIL;
                goto Error;
            }
        }

        // verify partitions
        for(cPartition = 1; cPartition <= PART_COUNT; cPartition++)
        {
            swprintf_s(szPart, _countof(szPart), SZ_PART_NAME, cPartition);
            if(!VerifyPartitionData(hStore, szPart))
            {
                FAIL("VerifyPartitionData()");
                retVal = TPR_FAIL;
                goto Error;
            }
        }
        
        if(VALID_HANDLE(hPart))
        {
            CloseHandle(hPart);
        }

        if(VALID_HANDLE(hStore))
        {
            CloseHandle(hStore);    
        }

        // success
        LOG(TEXT("All tests passed on disk %u"), ((TPS_EXECUTE *)tpParam)->dwThreadNumber);
        retVal = TPR_PASS;
        break;
    }
    
Error:
    if(VALID_HANDLE(hPart))
    {
        CloseHandle(hPart);
    }

    if(VALID_HANDLE(hStore))
    {
        CloseHandle(hStore);    
    }
    return retVal;
}

// --------------------------------------------------------------------
TESTPROCAPI 
Tst_DiskInfo(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    DWORD retVal = TPR_FAIL;
    HANDLE hStore = INVALID_HANDLE_VALUE;
    DISK_INFO diskInfo1 = {0};
    DISK_INFO diskInfo2 = {0};
    DWORD dwBytes = 0;
    
    STOREINFO storeInfo = {0};
    PARTINFO partInfo = {0};
    HANDLE hPart = INVALID_HANDLE_VALUE;

    DWORD dwDiskNumber = 0;
    DWORD dwDiskCount = 0;
    
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 1;
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }
    if(!Zorch(g_pShellInfo->szDllCmdLine))
    {
        Zorchage(g_pKato); 
        return TPR_SKIP; 
    }
    
    dwDiskCount = QueryDiskCount();
    if (0 == dwDiskCount) 
    {
        retVal = TPR_SKIP;
        LOG(TEXT("No disk found. Skipping test"));
        goto Error;
    }

    for (DWORD dwDiskNumber = 0; dwDiskNumber < dwDiskCount; ++ dwDiskNumber)
    {
        memset(&storeInfo, 0, sizeof(STOREINFO));
        memset(&partInfo, 0, sizeof(PARTINFO));

        storeInfo.cbSize = sizeof(STOREINFO);
        partInfo.cbSize = sizeof(PARTINFO);

        LOG(TEXT("Testing disk %u"), dwDiskNumber);
        if(!IsAValidStore(dwDiskNumber))
        {
            LOG(TEXT("Disk %u is not a valid volume for partitioning, skipping test."), dwDiskNumber);
            retVal = TPR_SKIP;
            continue;
        }

        // open and format store
        HRESULT hrFormat = S_OK;
        hStore = OpenAndFormatStore(dwDiskNumber, &storeInfo, &hrFormat);
        if(INVALID_HANDLE(hStore))
        {
            if (FAILED(hrFormat) ) {
                ERRFAIL("OpenAndFormatStore()");
                retVal = TPR_FAIL;
                goto Error;
            }
            else if (hrFormat == S_FALSE) {
                // Format can not happen on this disk, passing this BVT
                // by skipping this disk
                LOG(TEXT("Disk can not be formatted, skipping disk %u by design"), dwDiskNumber);
                retVal = TPR_SKIP;
                continue;
            }
        }

        DETAIL("Store opened and formatted successfully");

        // get raw disk info
        if(!GetRawDiskInfo(hStore, &diskInfo1))
        {
            FAIL("GetRawDiskInfo()");
            retVal = TPR_FAIL;
            goto Error;
        }

        DETAIL("Raw disk information:");
        LogDiskInfo(&diskInfo1);

        DETAIL("Store information:");
        LogStoreInfo(&storeInfo);

        // verify store and raw disk values are the same
        if(storeInfo.snNumSectors != diskInfo1.di_total_sectors)
        {
            FAIL("STOREINFO sector count does not match DISK_INFO sector count");
            retVal = TPR_FAIL;
            goto Error;
        }
        if(storeInfo.dwBytesPerSector != diskInfo1.di_bytes_per_sect)
        {
            FAIL("STOREINFO bytes-per-sector does not match DISK_INFO bytes-per-sector");
            retVal = TPR_FAIL;
            goto Error;
        }

        // create a partition that takes up the entire disk
        if(!CreateAndVerifyPartition(hStore, L"PART00", 0, storeInfo.snBiggestPartCreatable, TRUE, &partInfo))
        {
            FAIL("CreateAndVerifyPartition()");
            retVal = TPR_FAIL;
            goto Error;   
        }

        // verify raw disk values and partition info values
        hPart = OpenPartition(hStore, partInfo.szPartitionName);
        if(INVALID_HANDLE(hPart))
        {
            FAIL("OpenPartition()");
            retVal = TPR_FAIL;
            goto Error;
        }

        if(DeviceIoControl(hPart, IOCTL_DISK_GETINFO, NULL, 0, (PBYTE)&diskInfo2, sizeof(DISK_INFO), &dwBytes, NULL))
            LogDiskInfo(&diskInfo2);
        else if(DeviceIoControl(hPart, DISK_IOCTL_GETINFO, (PBYTE)&diskInfo2, sizeof(DISK_INFO), NULL, 0, &dwBytes, NULL))
            LogDiskInfo(&diskInfo2);
        else
        {
            DETAIL("Partition DeviceIoControl() failed when raw disk DeviceIoControl() succeeded");
            FAIL("DeviceIoControl(IOCTL_DISK_GETINFO)");
            retVal = TPR_FAIL;
            goto Error;
        }
        CloseHandle(hPart);
        hPart = INVALID_HANDLE_VALUE;

        if(diskInfo1.di_bytes_per_sect != diskInfo2.di_bytes_per_sect ||
            diskInfo1.di_total_sectors < diskInfo2.di_total_sectors)
        {
            FAIL("Partition  info does not fit in constraints of actual disk info");
            retVal = TPR_FAIL;
            goto Error;
        }

        if(VALID_HANDLE(hPart))
        {
            CloseHandle(hPart);
        }

        if(VALID_HANDLE(hStore))
        {
            CloseHandle(hStore);
        }

        // success
        LOG(TEXT("All tests passed on disk %u"), ((TPS_EXECUTE *)tpParam)->dwThreadNumber);
        retVal = TPR_PASS;
        break;
    }
    
Error:
    if(VALID_HANDLE(hPart))
    {
        CloseHandle(hPart);
    }

    if(VALID_HANDLE(hStore))
    {
        CloseHandle(hStore);
    }
    
    return retVal;
}

// --------------------------------------------------------------------
TESTPROCAPI 
Tst_InfoUpdate(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;
    HANDLE hStore = INVALID_HANDLE_VALUE;
    WCHAR szPartName[PARTITIONNAMESIZE];

    STOREINFO storeInfo1 = {0};
    STOREINFO storeInfo2 = {0};
    PARTINFO partInfo = {0};

    DWORD dwDiskNumber = 0;
    DWORD dwDiskCount = 0;
    
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 1;
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }
    if(uMsg == TPM_EXECUTE)
    {
        if(!Zorch(g_pShellInfo->szDllCmdLine))
        {
            Zorchage(g_pKato); 
            return TPR_SKIP; 
        }
    }
  
    dwDiskCount = QueryDiskCount();
    if (0 == dwDiskCount) 
    {
        retVal = TPR_SKIP;
        LOG(TEXT("No disk found. Skipping test"));
        goto Error;
    }

    for (DWORD dwDiskNumber = 0; dwDiskNumber < dwDiskCount; ++ dwDiskNumber)
    {
        memset(&storeInfo1, 0, sizeof(STOREINFO));
        memset(&storeInfo2, 0, sizeof(STOREINFO));
        memset(&partInfo, 0, sizeof(PARTINFO));

        storeInfo1.cbSize = sizeof(STOREINFO);
        storeInfo2.cbSize = sizeof(STOREINFO);
        partInfo.cbSize = sizeof(PARTINFO);

        LOG(TEXT("Testing disk %u"), dwDiskNumber);
        if(!IsAValidStore(dwDiskNumber))
        {
            LOG(TEXT("Disk %u is not a valid volume for partitioning, skipping test."), dwDiskNumber);
            retVal = TPR_SKIP;
            continue;
        }

        // open and format store
        HRESULT hrFormat = S_OK;
        hStore = OpenAndFormatStore(dwDiskNumber, &storeInfo1, &hrFormat);
        if(INVALID_HANDLE(hStore))
        {
            if (FAILED(hrFormat) ) {
                ERRFAIL("OpenAndFormatStore()");
                retVal = TPR_FAIL;
                goto Error;
            }
            else if (hrFormat == S_FALSE) {
                // Format can not happen on this disk, passing this BVT
                // by skipping this disk
                LOG(TEXT("Disk can not be formatted, skipping disk %u by design"), dwDiskNumber);
                retVal = TPR_SKIP;
                continue;
            }
        }
        DETAIL("Store opened and formatted successfully");
        DETAIL("Store information:");
        LogStoreInfo(&storeInfo1);

        swprintf_s(szPartName, _countof(szPartName), SZ_PART_NAME, 1);
        if(!CreateAndVerifyPartition(hStore, szPartName, 0, storeInfo1.snBiggestPartCreatable / 3, TRUE, &partInfo))
        {
            FAIL("CreateAndVerifyPartition()");
            retVal = TPR_FAIL;
            goto Error;
        }
        LogPartInfo(&partInfo);
        if(!GetStoreInfo(hStore, &storeInfo2))
        {
            FAIL("GetStoreInfo()");
            retVal = TPR_FAIL;
            goto Error;
        }
        LogStoreInfo(&storeInfo2);
        if(storeInfo2.snFreeSectors > (storeInfo1.snFreeSectors - partInfo.snNumSectors))
        {
            FAIL("STOREINFO struct not updated properly");
            retVal = TPR_FAIL;
            goto Error;
        }

        if(!DismountStore(hStore))
        {
            FAIL("DismountStore()");
            retVal = TPR_FAIL;
            goto Error;
        }

        if(!DeletePartition(hStore, szPartName))
        {
            FAIL("DeletePartition()");
            retVal = TPR_FAIL;
            goto Error;
        }

        if(!GetStoreInfo(hStore, &storeInfo2))
        {
            FAIL("GetStoreInfo()");
            retVal = TPR_FAIL;
            goto Error;
        }
        LogStoreInfo(&storeInfo2);

        if(storeInfo1.snFreeSectors != storeInfo2.snFreeSectors ||
            storeInfo1.snBiggestPartCreatable != storeInfo2.snBiggestPartCreatable)
        {
            FAIL("STOREINFO struct not updated properly");
            retVal = TPR_FAIL;
            goto Error;
        }

        // success
        LOG(TEXT("All tests passed on disk %u"), ((TPS_EXECUTE *)tpParam)->dwThreadNumber);
        retVal = TPR_PASS;
        break;
    }
    
Error:
    return retVal;
}

// --------------------------------------------------------------------
TESTPROCAPI 
Tst_PartitionBounds(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;
    HANDLE hStore = INVALID_HANDLE_VALUE;
    DWORD dwDiskNumber = 0;
    DWORD dwDiskCount = 0;
    SECTORNUM cSectors = 0;

    STOREINFO storeInfo = {0};
    PARTINFO partInfo1 = {0};
    PARTINFO partInfo2 = {0};
    PARTINFO partInfo3 = {0};
    PARTINFO partInfoTmp = {0};
    
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 1;
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }
    if(uMsg == TPM_EXECUTE)
    {
        if(!Zorch(g_pShellInfo->szDllCmdLine))
        {
            Zorchage(g_pKato); 
            return TPR_SKIP; 
        }   
    }
  
    dwDiskCount = QueryDiskCount();
    if (0 == dwDiskCount) 
    {
        retVal = TPR_SKIP;
        LOG(TEXT("No disk found. Skipping test"));
        goto Error;
    }

    for (DWORD dwDiskNumber = 0; dwDiskNumber < dwDiskCount; ++ dwDiskNumber)
    {
        memset(&storeInfo, 0, sizeof(STOREINFO));
        memset(&partInfo1, 0, sizeof(PARTINFO));
        memset(&partInfo2, 0, sizeof(PARTINFO));
        memset(&partInfo3, 0, sizeof(PARTINFO));
        memset(&partInfoTmp, 0, sizeof(PARTINFO));

        storeInfo.cbSize = sizeof(STOREINFO);
        partInfo1.cbSize = sizeof(PARTINFO);
        partInfo2.cbSize = sizeof(PARTINFO);
        partInfo3.cbSize = sizeof(PARTINFO);
        partInfoTmp.cbSize = sizeof(PARTINFO);

        LOG(TEXT("Testing disk %u"), dwDiskNumber);
        if(!IsAValidStore(dwDiskNumber))
        {
            LOG(TEXT("Disk %u is not a valid volume for partitioning, skipping test."), dwDiskNumber);
            retVal = TPR_SKIP;
            continue;
        }

        // open and format store
        HRESULT hrFormat = S_OK;
        hStore = OpenAndFormatStore(dwDiskNumber, &storeInfo, &hrFormat);
        if(INVALID_HANDLE(hStore))
        {
            if (FAILED(hrFormat) ) {
                ERRFAIL("OpenAndFormatStore()");
                retVal = TPR_FAIL;
                goto Error;
            }
            else if (hrFormat == S_FALSE) {
                // Format can not happen on this disk, passing this BVT
                // by skipping this disk
                LOG(TEXT("Disk can not be formatted, skipping disk %u by design"), dwDiskNumber);
                retVal = TPR_SKIP;
                continue;
            }
        }
        DETAIL("Store opened and formatted successfully");

        // create 1st partitoin
        cSectors = storeInfo.snBiggestPartCreatable / 3;
        if(!CreateAndVerifyPartition(hStore, _T("PART1"), 0, cSectors, TRUE, &partInfo1))
        {
            FAIL("CreateAndVerifyPartition()");
            retVal = TPR_FAIL;
            goto Error;
        }
        if(!GetStoreInfo(hStore, &storeInfo))
        {
            FAIL("GetStoreInfo()");
            retVal = TPR_FAIL;
            goto Error;
        }

        // create 2nd partition
        cSectors = storeInfo.snBiggestPartCreatable / 2;
        if(!CreateAndVerifyPartition(hStore, _T("PART2"), 0, cSectors, TRUE, &partInfo2))
        {
            FAIL("CreateAndVerifyPartition()");
            retVal = TPR_FAIL;
            goto Error;
        }
        if(!GetStoreInfo(hStore, &storeInfo))
        {
            FAIL("GetStoreInfo()");
            retVal = TPR_FAIL;
            goto Error;
        }

        // create 3rd partition
        cSectors = storeInfo.snBiggestPartCreatable;
        if(!CreateAndVerifyPartition(hStore, _T("PART3"), 0, cSectors, TRUE, &partInfo3))
        {
            FAIL("CreateAndVerifyPartition");
            retVal = TPR_FAIL;
            goto Error;
        }

        // stripe the partitions
        if(!WritePartitionData(hStore, _T("PART1")) ||
            !WritePartitionData(hStore, _T("PART2")) ||
            !WritePartitionData(hStore, _T("PART3")))
        {
            FAIL("WritePartitionData()");
            retVal = TPR_FAIL;
            goto Error;
        }

        // verify the partitions
        if(!VerifyPartitionData(hStore, _T("PART1")) ||
            !VerifyPartitionData(hStore, _T("PART2")) ||
            !VerifyPartitionData(hStore, _T("PART3")))
        {
            FAIL("VerifyPartitionData()");
            retVal = TPR_FAIL;
            goto Error;
        }

        // success
        retVal = TPR_PASS;
        break;
    }
    
Error:
    return retVal;
}

