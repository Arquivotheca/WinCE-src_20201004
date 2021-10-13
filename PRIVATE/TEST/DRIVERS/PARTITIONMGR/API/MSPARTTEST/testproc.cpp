//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "TestProc.H"
#include "TPartDrv.H"
#include "DiskHelp.H"

#define SZ_PART_NAME        L"TestPart%u"

SECTORNUM g_snPartSize = 0;
WCHAR g_szDskName[MAX_PATH] = L"";
WCHAR g_szPartName[MAX_PATH] = L"";
BYTE g_bPartType = 0;

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
    HANDLE hDisk = INVALID_HANDLE_VALUE;
    STOREID storeId = INVALID_STOREID;
    SEARCHID searchId = INVALID_SEARCHID;
    PD_STOREINFO storeInfo1 = {0};
    PD_STOREINFO storeInfo2 = {0};
    DWORD dwDiskNumber = 0;
    
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = QueryDiskCount();
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }

    storeInfo1.cbSize = sizeof(PD_STOREINFO);
    storeInfo2.cbSize = sizeof(PD_STOREINFO);

    dwDiskNumber = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;
    
    LOG(L"Testing disk %u", dwDiskNumber);
    if(!IsAValidDisk(dwDiskNumber))
    {
        LOG(L"Disk %u is not a valid volume for partitioning. It may be a CD-ROM device", dwDiskNumber);
        retVal = TPR_PASS;
        goto Error;
    }
    hDisk = OpenDisk(dwDiskNumber);
    if(INVALID_HANDLE(hDisk))
    {
        FAIL("OpenDisk()");
        goto Error;
    }
    
    // open an HSTORE handle to the volume through the partition manager
    DETAIL("Calling T_OpenStore() on volume...");
    storeId = T_OpenStore(hDisk);
    if(INVALID_STOREID == storeId)
    {
        ERRFAIL("T_OpenStore()");
        goto Error;
    }
    LOG(L"Store opened on volume; store handle = 0x%08x", storeId);

    // get store info on the store
    DETAIL("Calling T_GetStoreInfo() on disk volume");
    if(!T_GetStoreInfo(storeId, &storeInfo1))
    {
        FAIL("T_GetStoreInfo()");
        goto Error;
    }
    LogStoreInfo(&storeInfo1);
    
    // format the store
    DETAIL("Calling T_FormatStore() on disk volume");
    if(!T_FormatStore(storeId))
    {
        ERRFAIL("T_FormatStore()");
        goto Error;
    }
    DETAIL("T_FormatStore succeeded");

    // get store info on formatted store
    DETAIL("Calling T_GetStoreInfo() on formatted disk volume");
    if(!T_GetStoreInfo(storeId, &storeInfo1))
    {
        ERRFAIL("T_GetStoreInfo()");
        goto Error;
    }
    LogStoreInfo(&storeInfo1);    

    // close the formatted store
    DETAIL("Calling T_CloseStore() on formatted disk volume");   
    T_CloseStore(storeId);
    storeId = INVALID_STOREID;

    // again, open an HSTORE handle to the volume
    DETAIL("Calling T_OpenStore() on disk volume...");
    storeId = T_OpenStore(hDisk);
    if(INVALID_STOREID == storeId)
    {
        FAIL("T_OpenStore()");
        goto Error;
    }
    LOG(L"Store opened on disk volume; store handle = 0x%08x", storeId);

    // verify that the store is formatted
    DETAIL("Calling T_IsStoreFormatted() on disk volume");
    if(!T_IsStoreFormatted(storeId))
    {
        FAIL("T_IsStoreFormatted()");
        goto Error;
    }
    DETAIL("Store is formatted");

    // get store info and compare to previous store info
    DETAIL("Calling T_GetStoreInfo() on disk volume and comparing to previous info");
    if(!T_GetStoreInfo(storeId, &storeInfo2))
    {
        ERRFAIL("T_GetStoreInfo()");
        goto Error;
    }
    if(memcmp(&storeInfo1, &storeInfo2, sizeof(PD_STOREINFO)))
    {
        FAIL("T_GetStoreInfo() returned unexpected info");
        DETAIL("Original Info:");        
        LogStoreInfo(&storeInfo1);  
        DETAIL("New Info:");
        LogStoreInfo(&storeInfo2);
    }
    
    // success
    LOG(L"All tests passed on disk %u", ((TPS_EXECUTE *)tpParam)->dwThreadNumber);
    retVal = TPR_PASS;
    
Error:
    // close the store handle if necessary
    if(INVALID_STOREID != storeId)
    {
        T_CloseStore(storeId);
        storeId = INVALID_STOREID;
    }

    if(INVALID_SEARCHID != searchId)
    {
        T_FindPartitionClose(searchId);
        searchId = INVALID_SEARCHID;
    }

    // de-init the disk driver
    if(VALID_HANDLE(hDisk))
        CloseHandle(hDisk);
    
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
    HANDLE hDisk = INVALID_HANDLE_VALUE;
    WCHAR szPart1[PARTITIONNAMESIZE] = L"";
    WCHAR szPart2[PARTITIONNAMESIZE] = L"";
    WCHAR szPart3[PARTITIONNAMESIZE] = L"";

    STOREID storeId = INVALID_STOREID;
    PARTID partId = INVALID_PARTID;
    SEARCHID searchId = INVALID_SEARCHID;
    PD_STOREINFO storeInfo = {0};
    PD_PARTINFO partInfo1 = {0};
    PD_PARTINFO partInfo2 = {0};
    PD_PARTINFO partInfoTmp = {0};
    SECTORNUM cSectors = 0;
    SECTORNUM cOldFreeSectors = 0;

    DWORD dwDiskNumber = 0;
    
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = QueryDiskCount();
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }

    storeInfo.cbSize = sizeof(PD_STOREINFO);
    partInfo1.cbSize = sizeof(PD_PARTINFO);
    partInfo2.cbSize = sizeof(PD_PARTINFO);
    partInfoTmp.cbSize = sizeof(PD_PARTINFO);

    dwDiskNumber = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;
    
    LOG(L"Testing disk %u", dwDiskNumber);
    if(!IsAValidDisk(dwDiskNumber))
    {
        LOG(L"Disk %u is not a valid volume for partitioning. It may be a CD-ROM device", dwDiskNumber);
        retVal = TPR_PASS;
        goto Error;
    }
    hDisk = OpenDisk(dwDiskNumber);
    if(INVALID_HANDLE(hDisk))
    {
        FAIL("OpenDisk()");
        goto Error;
    }
    
    // open and format store
    storeId = OpenAndFormatStore(hDisk, &storeInfo);
    if(INVALID_STOREID == storeId)
    {
        FAIL("OpenAndFormatStore()");
        goto Error;
    }
    DETAIL("Store opened and formatted successfully");
    
    // try to enumerate non-existing partitions
    DETAIL("Attempting to enumerate non-existing partitions");
    searchId = T_FindPartitionStart(storeId);
    if(INVALID_SEARCHID == searchId)
    {
        FAIL("T_FindPartitionStart()");
        goto Error;
    }
    if(T_FindPartitionNext(searchId, &partInfo1))
    {
        FAIL("T_FindPartitionNext() succeeded when it should have failed");
        goto Error;
    }
    if(ERROR_NO_MORE_ITEMS != GetLastError())
    {
        FAIL("T_FindPartitionNext() returned incorrect error code on failure");
        goto Error;
    }
    T_FindPartitionClose(searchId);
    searchId = INVALID_SEARCHID;
    DETAIL("Successfully enumerated zero partitions as expected");    
    
    // create first partition on disk
    DETAIL("Creating first partition on volume");
    cSectors = storeInfo.snFreeSectors / 4; // use a quarter of the disk
    cOldFreeSectors = storeInfo.snFreeSectors;
    swprintf(szPart1, SZ_PART_NAME, 1);
    if(!CreateAndVerifyPartition(storeId, szPart1, NULL, cSectors, TRUE, &partInfo1))
    {
        FAIL("CreateAndVerifyPartition()");
        goto Error;
    }
    LOG(L"Successfully created new auto-type partition \"%s\" of size %u", szPart1, cSectors);
    LogPartInfo(&partInfo1);
    // make sure store info was updated accordingly
    DETAIL("Verifying store info was updated");
    if(!T_GetStoreInfo(storeId, &storeInfo))
    {
        FAIL("T_GetStoreInfo()");
        goto Error;
    }
    if(partInfo1.snNumSectors > (storeInfo.snFreeSectors - cOldFreeSectors))
    {
        LOG(L"requested sectors %u is greater than actual disk sectors used %u", 
            cSectors, storeInfo.snFreeSectors - cOldFreeSectors);
        FAIL("Store info not updated correctly after partition creation");
        goto Error;
    }
    DETAIL("Store info updated correctly after partition creation");    

    // open a second partition taking up the rest of the disk
    DETAIL("Creating a second partition");
    swprintf(szPart2, SZ_PART_NAME, 2);
    cSectors = storeInfo.snBiggestPartCreatable;
    if(!CreateAndVerifyPartition(storeId, szPart2, PART_DOS32, cSectors, FALSE, &partInfo2))
    {
        FAIL("CreateAndVerifyPartition()");
        goto Error;
    }
    LOG(L"Successfully created new FAT32 partition \"%s\" of size %u", szPart2, cSectors);

    // make sure the new partitions enumerates
    DETAIL("Enumerating new partitions");
    searchId = T_FindPartitionStart(storeId);
    if(INVALID_SEARCHID == searchId)
    {
        FAIL("T_FindPartitionStart()");
        goto Error;
    }    
    // should be enumerated in order
    if(!T_FindPartitionNext(searchId, &partInfoTmp))
    {
        FAIL("T_FindPartitionNext()");
        goto Error;
    }

    // the partitions can be enumerated in any order
    if(memcmp(&partInfo1, &partInfoTmp, sizeof(PD_PARTINFO)))
    {       
        // if it does not match partition 1, try partition 2
        if(memcmp(&partInfo2, &partInfoTmp, sizeof(PD_PARTINFO)))
        {
            // it doesn't  match either partition
            FAIL("T_FindPartitionNext() info differs from T_GetPartitionInfo() info");
            LogPartInfo(&partInfoTmp);
            LogPartInfo(&partInfo1);
            LogPartInfo(&partInfo2);
            goto Error;
        }
        LOG(L"Found first partition \"%s\"", partInfoTmp.szPartitionName);
        
        // enumerate the next partition, it should match parition 1
        if(!T_FindPartitionNext(searchId, &partInfoTmp))
        {
            FAIL("T_FindPartitionNext()");
            goto Error;
        }
        if(memcmp(&partInfo1, &partInfoTmp, sizeof(PD_PARTINFO)))
        {
            FAIL("T_FindPartitionNext() info differs from T_GetPartitionInfo() info");
            LogPartInfo(&partInfoTmp);
            LogPartInfo(&partInfo1);
            goto Error;
        }
        LOG(L"Found second partition \"%s\"", partInfoTmp.szPartitionName);
        DETAIL("NOTE: Partitions were enumerated in opposite order of disk location");
    }    
    else
    {
        // partition matched partition 1        
        LOG(L"Found first partition \"%s\"", partInfoTmp.szPartitionName);

        // enumerate the second partition, it should match partition 2
        if(!T_FindPartitionNext(searchId, &partInfoTmp))
        {
            FAIL("T_FindPartitionNext()");
            goto Error;
        }
        if(memcmp(&partInfo2, &partInfoTmp, sizeof(PD_PARTINFO)))
        {
            FAIL("T_FindPartitionNext() info differs from T_GetPartitionInfo() info");
            LogPartInfo(&partInfoTmp);
            LogPartInfo(&partInfo2);
            goto Error;
        }
        LOG(L"Found second partition \"%s\"", partInfoTmp.szPartitionName);
    }
    
    // should not enumerate any more partitions    
    if(T_FindPartitionNext(searchId, &partInfoTmp))
    {
        FAIL("Enumerated a third partition when there should not be any");
        goto Error;
    }
    if(ERROR_NO_MORE_ITEMS != GetLastError())
    {
        FAIL("T_FindPartitionNext() returned incorrect error code on failure");
        goto Error;
    }
    T_FindPartitionClose(searchId); // no return value
    searchId = INVALID_SEARCHID;
    DETAIL("Successfully enumerated and verified partitions");
    
    // rename the partition
    swprintf(szPart3, SZ_PART_NAME, 3);
    LOG(L"renaming paritition \"%s\" to \"%s\"", szPart2, szPart3);
    if(!T_RenamePartition(storeId, szPart2, szPart3))
    {
        FAIL("T_RenamePartition()");
        goto Error;
    }

    // delete first partition    
    DETAIL("Deleting first partition");
    if(!T_DeletePartition(storeId, szPart1))
    {
        FAIL("T_DeletePartition()");
        goto Error;
    }
    LOG(L"Partition \"%s\" deleted", szPart1);

    // make sure second partition still enumerates
    DETAIL("Enumerating partitions");
    searchId = T_FindPartitionStart(storeId);
    if(INVALID_SEARCHID == searchId)
    {
        FAIL("T_FindPartitionStart()");
        goto Error;
    }    
    // should only find one partition
    if(!T_FindPartitionNext(searchId, &partInfoTmp))
    {
        ERRFAIL("T_FindPartitionNext()");
        goto Error;
    }
    // verify only the name because the partition driver could have moved the partition
    if(wcscmp(szPart3, partInfoTmp.szPartitionName))
    {
        LOG(L"Name of found partition \"%s\" does not match only know exisiting partition \"%s\"",
            partInfoTmp.szPartitionName, szPart3);
        FAIL("Found a partition that should not exist");
        goto Error;
    }
    LOG(L"Found partition \"%s\"", partInfoTmp.szPartitionName);   
    // should not enumerate any more partitions
    if(T_FindPartitionNext(searchId, &partInfoTmp))
    {
        FAIL("Enumerated a second partition when there should not be any");
        goto Error;
    }
    if(ERROR_NO_MORE_ITEMS != GetLastError())
    {
        FAIL("T_FindPartitionNext() returned incorrect error code on failure");
        goto Error;
    }
    T_FindPartitionClose(searchId); // no return value
    searchId = INVALID_SEARCHID;
    DETAIL("Successfully enumerated and verified remaining partition");

    // attempt to create another partition
    DETAIL("Creating another partition");
    swprintf(szPart1, SZ_PART_NAME, 4);
    if(!T_GetStoreInfo(storeId, &storeInfo))
    {
        FAIL("GetStoreInfo()");
        goto Error;
    }
    if(0 == storeInfo.snFreeSectors)
    {
        FAIL("No free sectors on store");
        goto Error;
    }
    if(0 == storeInfo.snBiggestPartCreatable)
    {
        FAIL("Biggest creatable partition is set to zero");
        goto Error;
    }
    if(!CreateAndVerifyPartition(storeId, szPart1, NULL, 
        storeInfo.snBiggestPartCreatable, TRUE, &partInfoTmp))
    {
        FAIL("CreateAndVerifyPartition()");
        goto Error;
    }

    if(partInfoTmp.snNumSectors != partInfo1.snNumSectors)
    {
        LOG(L"new partition has %u sectors, old partition had %u sectors");
        FAIL("Sectors were either gained or lost during deletion/creation of partitions");
    }
    
    LOG(L"Correctly created new partition \"%s\"", szPart1);

    // get info before closing
    if(!T_GetPartitionInfo(storeId, szPart1, &partInfo1))
    {
        FAIL("T_GetPartitionInfo()");
        goto Error;
    }
    if(!T_GetPartitionInfo(storeId, szPart3, &partInfo2))
    {
        FAIL("T_GetPartitionInfo()");
        goto Error;
    }

    T_CloseStore(storeId);
    storeId = INVALID_STOREID;
    DETAIL("Closed store");  

    // reopen store and verify changes were permanent
    DETAIL("Re-opening store");
    storeId = T_OpenStore(hDisk);
    if(INVALID_STOREID == storeId)
    {
        FAIL("T_OpenStore()");
        goto Error;
    }
    // enumerate the partition
    DETAIL("Enumerating partitions");
    searchId = T_FindPartitionStart(storeId);
    if(INVALID_SEARCHID == searchId)
    {
        FAIL("T_FindPartitionStart()");
        goto Error;
    }
    if(!T_FindPartitionNext(searchId, &partInfoTmp))
    {
        ERRFAIL("T_FindPartitionNext()");
        goto Error;
    }
    LOG(L"Found first partition \"%s\"", partInfoTmp.szPartitionName);

    // verify that the partition info matches
    if(partInfoTmp.snNumSectors == partInfo1.snNumSectors &&
       partInfoTmp.dwAttributes == partInfo1.dwAttributes &&
       partInfoTmp.bPartType == partInfo1.bPartType)
    {
        LOG(L"First enumerated partition, %s, matches created partition named %s", 
            partInfoTmp.szPartitionName, partInfo1.szPartitionName);
        if(wcscmp(partInfoTmp.szPartitionName, partInfo1.szPartitionName))
            DETAIL("Warning: partition name was not preserved");
        // zero out the partition info so the next parition can't possibly
        // match it (in case the same partition gets enum'd twice)
        memset(&partInfo1, 0, sizeof(PD_PARTINFO));
    }
    else if(partInfoTmp.snNumSectors == partInfo2.snNumSectors &&
       partInfoTmp.dwAttributes == partInfo2.dwAttributes &&
       partInfoTmp.bPartType == partInfo2.bPartType)
    {
        LOG(L"First enumerated partition, %s, matches created partition named %s", 
            partInfoTmp.szPartitionName, partInfo2.szPartitionName);
        if(wcscmp(partInfoTmp.szPartitionName, partInfo2.szPartitionName))
            DETAIL("Warning: partition name was not preserved");
        // zero out the partition info so the next parition can't possibly
        // match it (in case the same partition gets enum'd twice)
        memset(&partInfo2, 0, sizeof(PD_PARTINFO));
    }

    // enumerate the second partition
    if(!T_FindPartitionNext(searchId, &partInfoTmp))
    {
        FAIL("T_FindPartitionNext()");
        goto Error;
    }
    LOG(L"Found second partition \"%s\"", partInfoTmp.szPartitionName);

    // verify that the partition info matches
    if(partInfoTmp.snNumSectors == partInfo2.snNumSectors &&
       partInfoTmp.dwAttributes == partInfo2.dwAttributes &&
       partInfoTmp.bPartType == partInfo2.bPartType)
    {
        LOG(L"First enumerated partition, %s, matches partition created as %s", 
            partInfoTmp.szPartitionName, partInfo2.szPartitionName);
        if(wcscmp(partInfoTmp.szPartitionName, partInfo2.szPartitionName))
            DETAIL("Warning: partition name was not preserved");
        // zero out the partition info so the next parition can't possibly
        // match it (in case the same partition gets enum'd twice)
        memset(&partInfo2, 0, sizeof(PD_PARTINFO));
    }
    else if(partInfoTmp.snNumSectors == partInfo1.snNumSectors &&
       partInfoTmp.dwAttributes == partInfo1.dwAttributes &&
       partInfoTmp.bPartType == partInfo1.bPartType)
    {
        LOG(L"First enumerated partition, %s, matches partition created as %s", 
            partInfoTmp.szPartitionName, partInfo1.szPartitionName);
        if(wcscmp(partInfoTmp.szPartitionName, partInfo1.szPartitionName))
            DETAIL("Warning: partition name was not preserved");
        // zero out the partition info so the next parition can't possibly
        // match it (in case the same partition gets enum'd twice)
        memset(&partInfo1, 0, sizeof(PD_PARTINFO));
    }
    
    // should not enumerate any more partitions    
    if(T_FindPartitionNext(searchId, &partInfoTmp))
    {
        FAIL("Enumerated a third partition when there should not be any");
        goto Error;
    }
    if(ERROR_NO_MORE_ITEMS != GetLastError())
    {
        FAIL("T_FindPartitionNext() returned incorrect error code on failure");
        goto Error;
    }
    T_FindPartitionClose(searchId); // no return value
    searchId = INVALID_SEARCHID;
    DETAIL("Successfully enumerated partitions");

    T_CloseStore(storeId);
    storeId = INVALID_STOREID;
    
    // success
    LOG(L"All tests passed on disk %u", ((TPS_EXECUTE *)tpParam)->dwThreadNumber);
    retVal = TPR_PASS;
    
Error:
    // close search handle
    if(INVALID_SEARCHID != searchId)
    {
        T_FindPartitionClose(searchId);
        searchId = INVALID_SEARCHID;
    }

    // close partition handle
    if(INVALID_PARTID != partId)
    {
        T_ClosePartition(partId);
        partId = INVALID_PARTID;
    }
    
    // close the store handle if necessary
    if(INVALID_STOREID != storeId)
    {   
        T_CloseStore(storeId);
        storeId = INVALID_STOREID;
    }    

    // de-init the disk driver
    if(VALID_HANDLE(hDisk))
        CloseHandle(hDisk);
    
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
    HANDLE hDisk = INVALID_HANDLE_VALUE;
    WCHAR szPart[PARTITIONNAMESIZE] = L"";

    STOREID storeId = INVALID_STOREID;
    SEARCHID searchId = INVALID_SEARCHID;
    SECTORNUM cSectors = 0;
    PD_PARTINFO partInfo = {0};
    PD_STOREINFO storeInfo = {0};

    DWORD dwDiskNumber = 0;

    partInfo.cbSize = sizeof(PD_PARTINFO);
    storeInfo.cbSize = sizeof(PD_STOREINFO);
    
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = QueryDiskCount();
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }

    dwDiskNumber = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;
    
    LOG(L"Testing disk %u", dwDiskNumber);
    if(!IsAValidDisk(dwDiskNumber))
    {
        LOG(L"Disk %u is not a valid volume for partitioning. It may be a CD-ROM device", dwDiskNumber);
        retVal = TPR_PASS;
        goto Error;
    }
    hDisk = OpenDisk(dwDiskNumber);
    if(INVALID_HANDLE(hDisk))
    {
        FAIL("OpenDisk()");
        goto Error;
    }

    // open and format store
    storeId = OpenAndFormatStore(hDisk, &storeInfo);
    if(INVALID_STOREID == storeId)
    {
        FAIL("OpenAndFormatStore()");
        goto Error;
    }
    DETAIL("Store opened and formatted successfully");

    // create partitions of increasing size until failure
    // partitions won't necessarily be created with the requested size

    // the max should never be reached
    cPartition = 0;
    for(cSectors = 1; cSectors < storeInfo.snNumSectors; cSectors++)
    {
        swprintf(szPart, SZ_PART_NAME, cSectors);
        if(!CreateAndVerifyPartition(storeId, szPart, 0, cSectors, TRUE, NULL))
        {
            LOG(L"Failed to create a partition of size %u sectors", cSectors);
            break;
        }
        else
        {
            LOG(L"Created partition \"%s\" of size %u sectors", szPart, cSectors);
            cPartition++;
        }
    }
    
    // no partitions were created
    if(0 == cPartition)
    {
        FAIL("Unable to create partitions on the store");
        goto Error;
    }

    // allocate an array to keep track of which partitions have been deleted
    pfPartDeleted = (BOOL*)VirtualAlloc(NULL, sizeof(BOOL)*cPartition, MEM_COMMIT, PAGE_READWRITE);
    if(NULL == pfPartDeleted)
    {
        ERRFAIL("VirtualAlloc()");
        goto Error;
    }
    memset(pfPartDeleted, FALSE, sizeof(BOOL)*cPartition);

    // perform test while there are still partition on the disk
    while(cPartition)
    {   
        cPartEnum = 0;
        cPartDeleted = 0;
        searchId = T_FindPartitionStart(storeId);
        if(INVALID_SEARCHID == searchId)
        {
            FAIL("T_FindPartitionStart()");
            goto Error;
        }

        // search for partitions
        while(T_FindPartitionNext(searchId, &partInfo))
        {           
            LOG(L"Found partition \"%s\" of size %u sectors",
                partInfo.szPartitionName, partInfo.snNumSectors);
            cPartEnum++;
            
            // try to parse out the size from the name
            cSectors = 0;
            if(0 == swscanf(partInfo.szPartitionName, SZ_PART_NAME, &cSectors))
            {
                // a defunct name
                LOG(L"Unable to find the size embedded in the partition name \"%s\"",
                    partInfo.szPartitionName);
                FAIL("Enumerated a partition that was not created");
                goto Error;
            }

            // make sure this partition is not flagged as deleted
            if(pfPartDeleted[cSectors-1])
            {
                LOG(L"Partition \"%s\" is supposed to be deleted", partInfo.szPartitionName);
                FAIL("Enumerated a deleted partition");
                goto Error;
            }

            // make sure the sector size matches the number in the file name
            if(cSectors > partInfo.snNumSectors)
            {
                LOG(L"Partition \"%s\" is size %I64u sectors when it should be %I64u sectors",
                    partInfo.szPartitionName, partInfo.snNumSectors, cSectors);
                FAIL("Partition size mismatch");
                goto Error;
            }

            // delete every other partition starting with the first one enumerated
            if(cPartEnum % 2)
            {
                if(!T_DeletePartition(storeId, partInfo.szPartitionName))
                {
                    LOG(L"Unable to delete partition \"%s\"", partInfo.szPartitionName);
                    ERRFAIL("T_DeletePartition()");
                    goto Error;
                }
                // update delete info
                cPartDeleted++;
                pfPartDeleted[cSectors - 1] = TRUE;
            }
        }
        T_FindPartitionClose(searchId);
        searchId = INVALID_SEARCHID;

        LOG(L"Found %u of %u partitions", cPartEnum, cPartition);
        LOG(L"Deleted %u of %u partitions", cPartDeleted, cPartition);

        // make sure all partitions were enumerated
        if(cPartEnum != cPartition)
        {
            FAIL("Not all partitions were enumerated");
            goto Error;
        }

        // should never delete more partitions than exist
        if(cPartDeleted > cPartition)
        {
            FAIL("Deleted more partitions than exist");
            goto Error;
        }   

        // update the remaining partition count
        cPartition -= cPartDeleted;
    }    
            
    // success
    LOG(L"All tests passed on disk %u", ((TPS_EXECUTE *)tpParam)->dwThreadNumber);
    retVal = TPR_PASS;
    
Error:

    if(INVALID_SEARCHID != searchId)
    {
        T_FindPartitionClose(searchId);
    }
    
    // close the store handle if necessary
    if(INVALID_STOREID != storeId)
    {   
        T_CloseStore(storeId);
        storeId = NULL;
    }

    if(pfPartDeleted)
    {
        VirtualFree(pfPartDeleted, 0, MEM_RELEASE);
    }

    // de-init the disk driver
    if(VALID_HANDLE(hDisk))
        CloseHandle(hDisk);
    
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
    HANDLE hDisk = INVALID_HANDLE_VALUE;
    WCHAR szPart1[PARTITIONNAMESIZE] = L"";
    WCHAR szPart2[PARTITIONNAMESIZE] = L"";

    STOREID storeId = INVALID_STOREID;
    //SEARCHID searchId = INVALID_SEARCHID;
    SECTORNUM cPartitionSize = 0;
    PD_PARTINFO partInfo1 = {0};
    PD_PARTINFO partInfo2 = {0};
    PD_PARTINFO partInfoTmp = {0};
    PD_STOREINFO storeInfo = {0};

    DWORD dwDiskNumber = 0;

    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = QueryDiskCount();
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }

    partInfo1.cbSize = sizeof(PD_PARTINFO);
    partInfo2.cbSize = sizeof(PD_PARTINFO);
    partInfoTmp.cbSize = sizeof(PD_PARTINFO);
    storeInfo.cbSize = sizeof(PD_STOREINFO);

    dwDiskNumber = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;
    
    LOG(L"Testing disk %u", dwDiskNumber);
    if(!IsAValidDisk(dwDiskNumber))
    {
        LOG(L"Disk %u is not a valid volume for partitioning. It may be a CD-ROM device", dwDiskNumber);
        retVal = TPR_PASS;
        goto Error;
    }
    hDisk = OpenDisk(dwDiskNumber);
    if(INVALID_HANDLE(hDisk))
    {
        FAIL("OpenDisk()");
        goto Error;
    }

    // open and format store
    storeId = OpenAndFormatStore(hDisk, &storeInfo);
    if(NULL == storeId)
    {
        FAIL("OpenAndFormatStore()");
        goto Error;
    }
    DETAIL("Store opened and formatted successfully");

    cPartitionSize = storeInfo.snBiggestPartCreatable / 3;

    // create partition 1
    swprintf(szPart1, SZ_PART_NAME, 1);
    if(!T_CreatePartition(storeId, szPart1, NULL, cPartitionSize, TRUE))
    {
        FAIL("T_CreatePartition()");
        goto Error;
    }
    LOG(L"Created partition \"%s\" of size %u sectors", szPart1, cPartitionSize);
    // get info about the partition
    if(!T_GetPartitionInfo(storeId, szPart1, &partInfo1))
    {
        FAIL("T_GetPartitionInfo()");
        goto Error;
    }
    LogPartInfo(&partInfo1);

    // attempt to create another partition with the same name
    if(T_CreatePartition(storeId, szPart1, NULL, cPartitionSize, TRUE))
    {
        FAIL("T_CreatePartition() succeeded when partition name was already in use");
        goto Error;
    }
    if(ERROR_ALREADY_EXISTS != GetLastError())
    {
        FAIL("T_CreatePartition() returned incorrect error code on failure");
        goto Error;
    }
    DETAIL("Failed to create a partition as expected with a name already in use");

    // create the second partition with a different name
    swprintf(szPart2, SZ_PART_NAME, 2);
    if(!T_CreatePartition(storeId, szPart2, NULL, cPartitionSize, TRUE))
    {
        FAIL("T_CreatePartition()");
        goto Error;
        
    }
    LOG(L"Created partition \"%s\" of size %u sectors", szPart2, cPartitionSize);
    // get info about the partition
    if(!T_GetPartitionInfo(storeId, szPart2, &partInfo2))
    {
        FAIL("T_GetPartitionInfo()");
        goto Error;
    }
    LogPartInfo(&partInfo2);
    

    // attempt to rename partition 2 with the same name as partition 1
    if(T_RenamePartition(storeId, szPart2, szPart1))
    {
        FAIL("T_RenamePartition() succeeded with an already existing name");
        goto Error;
    }   
    if(ERROR_ALREADY_EXISTS != GetLastError())
    {
        FAIL("T_RenamePartition() returned incorrect error code on failure");
        goto Error;
    }
    LOG(L"Unable to rename partition \"%s\" to \"%s\" as expected", szPart2, szPart1);

    // attempt to rename partition 1 with the same name as partition 2
    if(T_RenamePartition(storeId, szPart1, szPart2))
    {
        FAIL("T_RenamePartition() succeeded with an already existing name");
        goto Error;
    }
    if(ERROR_ALREADY_EXISTS != GetLastError())
    {
        FAIL("T_RenamePartition() returned incorrect error code on failure");
        goto Error;
    }
    LOG(L"Unable to rename partition \"%s\" to \"%s\" as expected", szPart1, szPart2);

    // rename partition 1 to a temporary name
    if(!T_RenamePartition(storeId, szPart1, L"TEMP"))
    {
        LOG(L"Unable to rename partition \"%s\" to \"TEMP\"", szPart1);
        FAIL("T_RenamePartition()");
        goto Error;
    }
    LOG(L"Renamed partition \"%s\" to \"TEMP\"", szPart1);
    
    // rename partition 2 to partition 1
    if(!T_RenamePartition(storeId, szPart2, szPart1))
    {
        LOG(L"Unable to rename partition \"%s\" to \"%s\"", szPart2, szPart1);
        FAIL("T_RenamePartition()");
        goto Error;
    }
    LOG(L"Renamed partition \"%s\" to \"%s\"", szPart2, szPart1);

    // rename temp to partition 2
    if(!T_RenamePartition(storeId, L"TEMP", szPart2))
    {
        LOG(L"Unable to rename partition \"TEMP\" to \"%s\"", szPart2);
        FAIL("T_RenamePartition()");
        goto Error;
    }
    LOG(L"Renamed partition \"TEMP\" to \"%s\"", szPart2);

    // verify that the partitions have swapped names only
    if(!T_GetPartitionInfo(storeId, szPart1, &partInfoTmp))
    {
        ERRFAIL("T_GetPartitionInfo()");
        goto Error;
    }
    // partition 1's name should have partition 2's info    
    if(wcscmp(partInfoTmp.szPartitionName, partInfo1.szPartitionName) ||
//       memcmp(&partInfoTmp.snFirstSector, &partInfo2.snFirstSector, sizeof(partInfoTmp.snFirstSector)) ||
       memcmp(&partInfoTmp.snNumSectors, &partInfo2.snNumSectors, sizeof(partInfoTmp.snNumSectors)) ||
       memcmp(&partInfoTmp.ftCreated, &partInfo2.ftCreated, sizeof(partInfoTmp.ftCreated)) ||
       memcmp(&partInfoTmp.ftLastModified, &partInfo2.ftLastModified, sizeof(partInfoTmp.ftLastModified)) ||
       memcmp(&partInfoTmp.dwAttributes, &partInfo2.dwAttributes, sizeof(partInfoTmp.dwAttributes)) ||
       memcmp(&partInfoTmp.bPartType, &partInfo2.bPartType, sizeof(partInfoTmp.bPartType)) )
    {
        FAIL("partition info comparision failure");
        goto Error;
    }
    LOG(L"Partition \"%s\" has the correct information", szPart1);

    // check partition 2
    if(!T_GetPartitionInfo(storeId, szPart2, &partInfoTmp))
    {
        ERRFAIL("T_GetPartitionInfo()");
        goto Error;
    }
    // partition 2's name should have partition 1's info    
    if(wcscmp(partInfoTmp.szPartitionName, partInfo2.szPartitionName) ||
//       memcmp(&partInfoTmp.snFirstSector, &partInfo1.snFirstSector, sizeof(partInfoTmp.snFirstSector)) ||
       memcmp(&partInfoTmp.snNumSectors, &partInfo1.snNumSectors, sizeof(partInfoTmp.snNumSectors)) ||
       memcmp(&partInfoTmp.ftCreated, &partInfo1.ftCreated, sizeof(partInfoTmp.ftCreated)) ||
       memcmp(&partInfoTmp.ftLastModified, &partInfo1.ftLastModified, sizeof(partInfoTmp.ftLastModified)) ||
       memcmp(&partInfoTmp.dwAttributes, &partInfo1.dwAttributes, sizeof(partInfoTmp.dwAttributes)) ||
       memcmp(&partInfoTmp.bPartType, &partInfo1.bPartType, sizeof(partInfoTmp.bPartType)) )
    {
        FAIL("partition info comparision failure");
        goto Error;
    }
    LOG(L"Partition \"%s\" has the correct information", szPart2);

    T_CloseStore(storeId);
    storeId = INVALID_STOREID;
    
    // success
    LOG(L"All tests passed on disk %u", ((TPS_EXECUTE *)tpParam)->dwThreadNumber);
    retVal = TPR_PASS;
    
Error:
    // close the store handle if necessary
    if(INVALID_STOREID != storeId)
    {   
        T_CloseStore(storeId);
        storeId = INVALID_STOREID;
    }
    
    // de-init the disk driver
    if(VALID_HANDLE(hDisk))
       CloseHandle(hDisk);
    
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
    HANDLE hDisk = INVALID_HANDLE_VALUE;
    WCHAR szPart[PARTITIONNAMESIZE] = L"";
    
    STOREID storeId = INVALID_STOREID;
    PD_STOREINFO storeInfo = {0};
    PD_PARTINFO partInfo = {0};
    SECTORNUM cPartitionSize = 0;

    DWORD dwDiskNumber = 0;
    
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = QueryDiskCount();
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }

    storeInfo.cbSize = sizeof(PD_STOREINFO);
    partInfo.cbSize = sizeof(PD_PARTINFO);

    dwDiskNumber = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;
    
    LOG(L"Testing disk %u", dwDiskNumber);
    if(!IsAValidDisk(dwDiskNumber))
    {
        LOG(L"Disk %u is not a valid volume for partitioning. It may be a CD-ROM device", dwDiskNumber);
        retVal = TPR_PASS;
        goto Error;
    }
    hDisk = OpenDisk(dwDiskNumber);
    if(INVALID_HANDLE(hDisk))
    {
        FAIL("OpenDisk()");
        goto Error;
    }
    
    // open and format store
    storeId = OpenAndFormatStore(hDisk, &storeInfo);
    if(INVALID_STOREID == storeId)
    {
        FAIL("OpenAndFormatStore()");
        goto Error;
    }
    DETAIL("Store opened and formatted successfully");

    // create partitions
    for(cPartition = 1; cPartition <= PART_COUNT; cPartition++)
    {
        if(!T_GetStoreInfo(storeId, &storeInfo))
        {
            FAIL("T_GetStoreInfo()");
            goto Error;
        }

        // leave enough room for remaining partitions -- 
        // they will all be roughly the same size
        cPartitionSize = storeInfo.snBiggestPartCreatable / (PART_COUNT - cPartition + 1);
        swprintf(szPart, SZ_PART_NAME, cPartition);
        LOG(L"Requesting partition \"%s\" of size %I64u sectors", szPart, cPartitionSize);
        if(!T_CreatePartition(storeId, szPart, NULL, cPartitionSize, TRUE))
        {
            FAIL("T_CreatePartition()");
            goto Error;
        }
        if(!T_GetPartitionInfo(storeId, szPart, &partInfo))
        {
            FAIL("T_GetPartitionInfo()");
            goto Error;
        }
        LOG(L"Created partition \"%s\" of size %I64u sectors", partInfo.szPartitionName, partInfo.snNumSectors);
    }

    // write partitions
    for(cPartition = 1; cPartition <= PART_COUNT; cPartition++)
    {
        swprintf(szPart, SZ_PART_NAME, cPartition);
        if(!WritePartitionData(storeId, szPart))
        {
            FAIL("WritePartitionData()");
            goto Error;
        }
    }

    // write and verify partitions
    for(cPartition = 1; cPartition <= PART_COUNT; cPartition++)
    {
        swprintf(szPart, SZ_PART_NAME, cPartition);
        if(!WriteAndVerifyPartition(hDisk, storeId, szPart))
        {
            FAIL("WriteAndVerifyPartition()");
            goto Error;
        }
    }

    // verify partitions
    for(cPartition = 1; cPartition <= PART_COUNT; cPartition++)
    {
        swprintf(szPart, SZ_PART_NAME, cPartition);
        if(!VerifyPartitionData(storeId, szPart))
        {
            FAIL("VerifyPartitionData()");
            goto Error;
        }
    }
    
    // success
    LOG(L"All tests passed on disk %u", ((TPS_EXECUTE *)tpParam)->dwThreadNumber);
    retVal = TPR_PASS;
    
Error:
    // close the store handle if necessary
    if(INVALID_STOREID != storeId)
    {   
        T_CloseStore(storeId);
        storeId = INVALID_STOREID;
    }
    
    // de-init the disk driver
    if(VALID_HANDLE(hDisk))
        CloseHandle(hDisk);    
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
    HANDLE hDisk = INVALID_HANDLE_VALUE;
    DISK_INFO diskInfo1 = {0};
    DISK_INFO diskInfo2 = {0};
    DWORD dwBytes = 0;
    
    PD_STOREINFO storeInfo = {0};
    PD_PARTINFO partInfo = {0};
    STOREID storeId = INVALID_STOREID;
    PARTID partId = INVALID_PARTID;

    DWORD dwDiskNumber = 0;
    
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = QueryDiskCount();
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }

    storeInfo.cbSize = sizeof(PD_STOREINFO);
    partInfo.cbSize = sizeof(PD_PARTINFO);

    dwDiskNumber = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;
    
    LOG(L"Testing disk %u", dwDiskNumber);
    if(!IsAValidDisk(dwDiskNumber))
    {
        LOG(L"Disk %u is not a valid volume for partitioning. It may be a CD-ROM device", dwDiskNumber);
        retVal = TPR_PASS;
        goto Error;
    }
    hDisk = OpenDisk(dwDiskNumber);
    if(INVALID_HANDLE(hDisk))
    {
        FAIL("OpenDisk()");
        goto Error;
    }

    // get raw disk info
    if(!GetRawDiskInfo(hDisk, &diskInfo1))
    {
        FAIL("GetRawDiskInfo()");
        goto Error;
    }

    DETAIL("Raw disk information:");
    LogDiskInfo(&diskInfo1);
    
    // open and format store
    storeId = OpenAndFormatStore(hDisk, &storeInfo);
    if(INVALID_STOREID == storeId)
    {
        FAIL("OpenAndFormatStore()");
        goto Error;
    }
    DETAIL("Store opened and formatted successfully");
    DETAIL("Store information:");
    LogStoreInfo(&storeInfo);

    // verify store and raw disk values are the same
    if(storeInfo.snNumSectors != diskInfo1.di_total_sectors)
    {
        FAIL("PD_STOREINFO sector count does not match DISK_INFO sector count");
        goto Error;
    }
    if(storeInfo.dwBytesPerSector != diskInfo1.di_bytes_per_sect)
    {
        FAIL("PD_STOREINFO bytes-per-sector does not match DISK_INFO bytes-per-sector");
        goto Error;
    }

    // create a partition that takes up the entire disk
    if(!CreateAndVerifyPartition(storeId, L"PART00", 0, storeInfo.snBiggestPartCreatable, TRUE, &partInfo))
    {
        FAIL("CreateAndVerifyPartition()");
        goto Error;   
    }

    // verify raw disk values and partition info values
    partId = T_OpenPartition(storeId, partInfo.szPartitionName);
    if(INVALID_PARTID == partId)
    {
        FAIL("T_OpenPartition()");
        goto Error;
    }
    //if(T_DeviceIoControl(partId, IOCTL_DISK_GETINFO, (PBYTE)&diskInfo2, sizeof(DISK_INFO), NULL, 0, &dwBytes))
    //    LogDiskInfo(&diskInfo2);
    if(T_DeviceIoControl(partId, IOCTL_DISK_GETINFO, NULL, 0, (PBYTE)&diskInfo2, sizeof(DISK_INFO), &dwBytes))
        LogDiskInfo(&diskInfo2);
    else if(T_DeviceIoControl(partId, DISK_IOCTL_GETINFO, (PBYTE)&diskInfo2, sizeof(DISK_INFO), NULL, 0, &dwBytes))
        LogDiskInfo(&diskInfo2);
    //else if(T_DeviceIoControl(partId, DISK_IOCTL_GETINFO, NULL, 0, (PBYTE)&diskInfo2, sizeof(DISK_INFO), &dwBytes))
    //    LogDiskInfo(&diskInfo2);
    else
    {
        DETAIL("Partition DeviceIoControl() failed when raw disk DeviceIoControl() succeeded");
        FAIL("T_DeviceIoControl(IOCTL_DISK_GETINFO)");
        goto Error;
    }
    T_ClosePartition(partId);
    partId = INVALID_PARTID;

    if(diskInfo1.di_bytes_per_sect != diskInfo2.di_bytes_per_sect ||
       diskInfo1.di_total_sectors < diskInfo2.di_total_sectors)
    {
        FAIL("Partition  info does not fit in constraints of actual disk info");
        goto Error;
    }
        
    // success
    LOG(L"All tests passed on disk %u", ((TPS_EXECUTE *)tpParam)->dwThreadNumber);
    retVal = TPR_PASS;
    
Error:
    // close the store handle if necessary
    if(INVALID_STOREID != storeId)
    {   
        T_CloseStore(storeId);
        storeId = INVALID_STOREID;
    }

    // de-init the disk driver
    if(VALID_HANDLE(hDisk))
        CloseHandle(hDisk);
    
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
    HANDLE hDisk = INVALID_HANDLE_VALUE;
    WCHAR szPartName[PARTITIONNAMESIZE];

    STOREID storeId = INVALID_STOREID;
    PD_STOREINFO storeInfo1 = {0};
    PD_STOREINFO storeInfo2 = {0};
    PD_PARTINFO partInfo = {0};

    DWORD dwDiskNumber = 0;
    
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = QueryDiskCount();
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }

    storeInfo1.cbSize = sizeof(PD_STOREINFO);
    storeInfo2.cbSize = sizeof(PD_STOREINFO);
    partInfo.cbSize = sizeof(PD_PARTINFO);

    dwDiskNumber = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;
    
    LOG(L"Testing disk %u", dwDiskNumber);
    if(!IsAValidDisk(dwDiskNumber))
    {
        LOG(L"Disk %u is not a valid volume for partitioning. It may be a CD-ROM device", dwDiskNumber);
        retVal = TPR_PASS;
        goto Error;
    }
    hDisk = OpenDisk(dwDiskNumber);
    if(INVALID_HANDLE(hDisk))
    {
        FAIL("OpenDisk()");
        goto Error;
    }

    // open and format store
    storeId = OpenAndFormatStore(hDisk, &storeInfo1);
    if(INVALID_STOREID == storeId)
    {
        FAIL("OpenAndFormatStore()");
        goto Error;
    }
    DETAIL("Store opened and formatted successfully");
    DETAIL("Store information:");
    LogStoreInfo(&storeInfo1);

    swprintf(szPartName, SZ_PART_NAME, 1);
    if(!CreateAndVerifyPartition(storeId, szPartName, 0, storeInfo1.snBiggestPartCreatable / 3, TRUE, &partInfo))
    {
        FAIL("CreateAndVerifyPartition()");
        goto Error;
    }
    LogPartInfo(&partInfo);
    if(!T_GetStoreInfo(storeId, &storeInfo2))
    {
        FAIL("T_GetStoreInfo()");
        goto Error;
    }
    LogStoreInfo(&storeInfo2);
    if(storeInfo2.snFreeSectors > (storeInfo1.snFreeSectors - partInfo.snNumSectors))
    {
        FAIL("PD_STOREINFO struct not updated properly");
        goto Error;
    }

    if(!T_DeletePartition(storeId, szPartName))
    {
        FAIL("T_DeletePartition()");
        goto Error;
    }

    if(!T_GetStoreInfo(storeId, &storeInfo2))
    {
        FAIL("T_GetStoreInfo()");
        goto Error;
    }
    LogStoreInfo(&storeInfo2);

    if(storeInfo1.snFreeSectors != storeInfo2.snFreeSectors ||
       storeInfo1.snBiggestPartCreatable != storeInfo2.snBiggestPartCreatable)
    {
        FAIL("PD_STOREINFO struct not updated properly");
        goto Error;
    }
    
    // success
    LOG(L"All tests passed on disk %u", ((TPS_EXECUTE *)tpParam)->dwThreadNumber);
    retVal = TPR_PASS;
    
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
    HANDLE hDisk = INVALID_HANDLE_VALUE;
    DWORD dwDiskNumber = 0;
    SECTORNUM cSectors = 0;

    STOREID storeId = INVALID_STOREID;
    PD_STOREINFO storeInfo = {0};
    PD_PARTINFO partInfo1 = {0};
    PD_PARTINFO partInfo2 = {0};
    PD_PARTINFO partInfo3 = {0};
    PD_PARTINFO partInfoTmp = {0};
    
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = QueryDiskCount();
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }

    storeInfo.cbSize = sizeof(PD_STOREINFO);
    partInfo1.cbSize = sizeof(PD_PARTINFO);
    partInfo2.cbSize = sizeof(PD_PARTINFO);
    partInfo3.cbSize = sizeof(PD_PARTINFO);
    partInfoTmp.cbSize = sizeof(PD_PARTINFO);
    
    dwDiskNumber = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;
    
    LOG(L"Testing disk %u", dwDiskNumber);
    if(!IsAValidDisk(dwDiskNumber))
    {
        LOG(L"Disk %u is not a valid volume for partitioning. It may be a CD-ROM device", dwDiskNumber);
        retVal = TPR_PASS;
        goto Error;
    }
    hDisk = OpenDisk(dwDiskNumber);
    if(INVALID_HANDLE(hDisk))
    {
        FAIL("OpenDisk()");
        goto Error;
    }

    // open and format store
    storeId = OpenAndFormatStore(hDisk, &storeInfo);
    if(INVALID_STOREID == storeId)
    {
        FAIL("OpenAndFormatStore()");
        goto Error;
    }
    DETAIL("Store opened and formatted successfully");

    // create 1st partitoin
    cSectors = storeInfo.snBiggestPartCreatable / 3;
    if(!CreateAndVerifyPartition(storeId, _T("PART1"), 0, cSectors, TRUE, &partInfo1))
    {
        FAIL("CreateAndVerifyPartition()");
        goto Error;
    }
    if(!T_GetStoreInfo(storeId, &storeInfo))
    {
        FAIL("T_GetStoreInfo()");
        goto Error;
    }

    // create 2nd partition
    cSectors = storeInfo.snBiggestPartCreatable / 2;
    if(!CreateAndVerifyPartition(storeId, _T("PART2"), 0, cSectors, TRUE, &partInfo2))
    {
        FAIL("CreateAndVerifyPartition()");
        goto Error;
    }
    if(!T_GetStoreInfo(storeId, &storeInfo))
    {
        FAIL("T_GetStoreInfo()");
        goto Error;
    }

    // create 3rd partition
    cSectors = storeInfo.snBiggestPartCreatable;
    if(!CreateAndVerifyPartition(storeId, _T("PART3"), 0, cSectors, TRUE, &partInfo3))
    {
        FAIL("CreateAndVerifyPartition");
        goto Error;
    }

    // stripe the partitions
    if(!WritePartitionData(storeId, _T("PART1"), 1) ||
       !WritePartitionData(storeId, _T("PART2"), 1) ||
       !WritePartitionData(storeId, _T("PART3"), 1))
    {
        FAIL("WritePartitionData()");
        goto Error;
    }

    // verify the partitions
    if(!VerifyPartitionData(storeId, _T("PART1"), 1) ||
       !VerifyPartitionData(storeId, _T("PART2"), 1) ||
       !VerifyPartitionData(storeId, _T("PART3"), 1))
    {
        FAIL("VerifyPartitionData()");
        goto Error;
    }
    
    // success
    retVal = TPR_PASS;
    
Error:
    return retVal;
}
