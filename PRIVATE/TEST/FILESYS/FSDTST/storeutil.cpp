//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "storeutil.h"

DWORD g_cPartitions = 0;
DWORD g_cMounted = 0;
DWORD g_cReadOnly = 0;
PMOUNTED_FILESYSTEM_LIST g_pmfsList = NULL;

extern void LOG(LPWSTR szFmt, ...);

CMtdPart *GetPartition(DWORD index)
{
    PMOUNTED_FILESYSTEM_LIST plist = g_pmfsList;

    for(DWORD i = 0; i < index; i++)
    {
        if(plist)
            plist = plist->pNextFS;
    }

    if(plist)
    {
        return plist->pMtdPart;
    }
    
    return NULL;
}

DWORD GetPartitionCount()
{
    return g_cPartitions;
}

VOID AddPartToList(PMOUNTED_FILESYSTEM_LIST *ppmfsList, PPARTINFO pPartInfo, PSTOREINFO pStoreInfo)
{
    PMOUNTED_FILESYSTEM_LIST pmfs;
    PMOUNTED_FILESYSTEM_LIST plist;

    // allocate the list structure
    pmfs = (PMOUNTED_FILESYSTEM_LIST)VirtualAlloc(NULL, sizeof(PMOUNTED_FILESYSTEM_LIST), MEM_COMMIT, PAGE_READWRITE);
    if(NULL == pmfs) // OOM?
        return;

    // fill in the list structure
    pmfs->pNextFS = NULL;
    pmfs->pMtdPart = CMtdPart::Allocate(pPartInfo, pStoreInfo);
    if(NULL == pmfs->pMtdPart) // OOM?
        return;

    if(*ppmfsList)
    {
        // find the end of the list
        plist = *ppmfsList;
        while(NULL != plist->pNextFS)
            plist = plist->pNextFS;

        // insert at the end of the list
        plist->pNextFS = pmfs;
    }
    else
    {
        // insert at head of the list
        *ppmfsList = pmfs;
    }

}

DWORD BuildFSListOnStore(HANDLE hStore, PSTOREINFO pStoreInfo)
{
    HANDLE hFindPart;
    PARTINFO partInfo;
    DWORD cTotalParts = 0;

    partInfo.cbSize = sizeof(PARTINFO);
    
    hFindPart = FindFirstPartition(hStore, &partInfo);
    if(VALID_HANDLE(hFindPart))
    {
        do
        {
            //
            // If the partition has the requested file system, add it to
            // the list of available partitions
            //
            if(0 != _tcsicmp(partInfo.szFileSys, g_testOptions.szFilesystem))
                continue;

            // 
            // since we're testing file systems, the partition must be
            // mounted. skip it otherwise
            //
            if(!(partInfo.dwAttributes & PARTITION_ATTRIBUTE_MOUNTED))
                continue;

            // 
            // add the partition to our list
            //
            CMtdPart::LogWin32PartInfo(&partInfo);
            AddPartToList(&g_pmfsList, &partInfo, pStoreInfo);
            cTotalParts++;
            
        } while(FindNextPartition(hFindPart, &partInfo));
        FindClosePartition(hFindPart);
    }
        
    return cTotalParts;
}

DWORD BuildFSList(VOID)
{
    HANDLE hStore, hFindStore;
    STOREINFO storeInfo;
    PARTINFO partInfo;

    storeInfo.cbSize = sizeof(STOREINFO);
    partInfo.cbSize = sizeof(PARTINFO);

    g_cPartitions = 0;

    //
    // was a path specified on the command line? 
    // use it first if so and skip volume enumeration
    //
    // this is really just a way so we can run the test
    // on FSDs that aren't enumerated with storage mgr,
    // eg, IPSM, RELFSD, and the Object Store filesystem
    //
    if(g_testOptions.szPath[0])
    {
        g_cPartitions = 1;
        memset(&storeInfo, 0, sizeof(STOREINFO));
        memset(&partInfo, 0, sizeof(PARTINFO));
        _tcsncpy(partInfo.szVolumeName, g_testOptions.szPath, 
            sizeof(partInfo.szVolumeName)/sizeof(partInfo.szVolumeName[0]));

        AddPartToList(&g_pmfsList, &partInfo, &storeInfo);
    }

    //
    // there was no path specified, so we will enumerate all the
    // partitions on a particular storage device that match the
    // specified storage profile and file system specified on 
    // the command line. the tests will run on all paths that match
    //
    else
    {

        // enumerate stores
        hFindStore = FindFirstStore(&storeInfo);
        if(VALID_HANDLE(hFindStore))
        {
            LOG(_T("Available storage profiles:"));
            do
            {
                LOG(_T("   %s"), storeInfo.sdi.szProfile);
                //
                // If the profile of this store matches the type we're looking
                // to test, add this store's partitions to the list
                //
                if(0 != _tcsicmp(storeInfo.sdi.szProfile, g_testOptions.szProfile))
                    continue;

                LOG(_T("Found store matching required storage profile [%s]"), 
                    g_testOptions.szProfile);
                CMtdPart::LogWin32StoreInfo(&storeInfo);            
                
                // open the store
                hStore = OpenStore(storeInfo.szDeviceName);
                if(VALID_HANDLE(hStore))
                {
                    g_cPartitions += BuildFSListOnStore(hStore, &storeInfo);
                    CloseHandle(hStore);
                }
                
            } while(FindNextStore(hFindStore, &storeInfo));
            FindCloseStore(hFindStore);
        }
    }
    return g_cPartitions;
}

VOID DestroyFSList(VOID)
{
    PMOUNTED_FILESYSTEM_LIST pmfs;

    while(g_pmfsList)
    {
        delete g_pmfsList->pMtdPart;
        pmfs = g_pmfsList->pNextFS;
        VirtualFree(g_pmfsList, 0, MEM_RELEASE);

        g_pmfsList = pmfs;
    }
    g_pmfsList = NULL;
}
