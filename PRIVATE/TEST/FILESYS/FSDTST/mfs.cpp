//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "mfs.h"

// static member init
DWORD CMtdPart::uniqueId = 0;

// helper function for displaying disk info
LPTSTR FormatValue(DWORD dwValue, LPTSTR szValue) {

   // Loop on each thousand's group.
   DWORD dw = 0, dwGroup[4] = { 0, 0, 0, 0 };
   while (dwValue) {
      dwGroup[dw++] = dwValue % 1000;
      dwValue /= 1000;
   }

   // Format the output with commas.
   switch (dw) {
      case 2:  wsprintf(szValue, TEXT("%u,%03u"), dwGroup[1], dwGroup[0]); break;
      case 3:  wsprintf(szValue, TEXT("%u,%03u,%03u"), dwGroup[2], dwGroup[1], dwGroup[0]); break;
      case 4:  wsprintf(szValue, TEXT("%u,%03u,%03u,%03u"), dwGroup[3], dwGroup[2], dwGroup[1], dwGroup[0]); break;
      default: wsprintf(szValue, TEXT("%u"), dwGroup[0]);
   }

   return szValue;
}

CMtdPart *CMtdPart::Allocate(PPARTINFO pPartInfo, PSTOREINFO pStoreInfo)
{
    // add logic to determine the type of partition
    if(0 == _tcsicmp(pPartInfo->szFileSys, _T("FATFSD.DLL")))
    {
        return (new CFATMtdPart(pStoreInfo, pPartInfo));
    }
    // snag the root dir number and cluster size from the user test options for
    // a non-FAT file system
    return (new CMtdPart(pStoreInfo, pPartInfo, g_testOptions.nMaxRootDirs, 
        g_testOptions.nClusterSize));
}

CMtdPart::CMtdPart(PSTOREINFO pStoreInfo, PPARTINFO pPartInfo, UINT nRootDirs, UINT nClusterSize) :
    m_hPart(INVALID_HANDLE_VALUE),
    m_hStore(INVALID_HANDLE_VALUE),
    m_uniqueId(uniqueId++),
    m_nRootDirs(nRootDirs),
    m_nClusterSize(nClusterSize)
{
    DWORD dwBytes;
    
    ::memcpy(&m_partInfo, pPartInfo, sizeof(m_partInfo));
    ::memcpy(&m_storeInfo, pStoreInfo, sizeof(m_storeInfo));

    m_hStore = ::OpenStore(m_storeInfo.szDeviceName);
    if(INVALID_HANDLE(m_hStore))
    {
        ERR("OpenStore()");
    }
    else
    {
        m_hPart = ::OpenPartition(m_hStore, m_partInfo.szPartitionName);
        if(INVALID_HANDLE(m_hPart))
        {
            ERR("OpenPartition()");
        }
        else
        {
            if(!::DeviceIoControl(m_hPart, DISK_IOCTL_GETINFO, &m_diskInfo,
                sizeof(m_diskInfo), &m_diskInfo, sizeof(m_diskInfo), &dwBytes, NULL))
            {
                ERR("DeviceIoControl(DISK_IOCTL_GETINFO)");
            }
        }
    }
}

CMtdPart::~CMtdPart(VOID)
{
    ::CloseHandle(m_hPart);
    ::CloseHandle(m_hStore);
}

SECTORNUM CMtdPart::GetNumSectors(VOID)
{
    return m_partInfo.snNumSectors;
}

LPTSTR CMtdPart::GetPartitionName(LPTSTR szPartitionName)
{
    ::_tcscpy(szPartitionName, m_partInfo.szPartitionName);
    return szPartitionName;
}

LPTSTR CMtdPart::GetVolumeName(LPTSTR szVolumeName)
{
    LPTSTR pszTmp = szVolumeName;

    // make sure the first character is a '\'
    if(_T('\\') != m_partInfo.szVolumeName[0])
    {
        pszTmp[0] = _T('\\');
        pszTmp++;
    }
    ::_tcscpy(pszTmp, m_partInfo.szVolumeName);
    
    return szVolumeName;
}

LPTSTR CMtdPart::GetFileSysName(LPTSTR szFileSysName)
{
    ::_tcscpy(szFileSysName, m_partInfo.szFileSys);
    return szFileSysName;
}

DWORD CMtdPart::GetPartitionAttributes(VOID)
{
    return m_partInfo.dwAttributes;
}

BYTE CMtdPart::GetPartitionType(VOID)
{
    return m_partInfo.bPartType;
}

LPTSTR CMtdPart::GetDeviceName(LPTSTR szDeviceName)
{
    ::_tcscpy(szDeviceName, m_storeInfo.szDeviceName);
    return szDeviceName;
}

LPTSTR CMtdPart::GetStoreName(LPTSTR szStoreName)
{
    ::_tcscpy(szStoreName, m_storeInfo.szStoreName);
    return szStoreName;
}

LPTSTR CMtdPart::GetProfileName(LPTSTR szProfileName)
{
    ::_tcscpy(szProfileName, m_storeInfo.sdi.szProfile);
    return szProfileName;
}

DWORD CMtdPart::GetBytesPerSector(VOID)
{
    return m_diskInfo.di_bytes_per_sect;
}

DWORD CMtdPart::GetCallerDiskFreeSpace(VOID)
{
    TCHAR szVolName[VOLUMENAMESIZE];
    DWORD dwFreeSpace = 0;
    ULARGE_INTEGER uliFreeToCaller, uliTotal, uliTotalFree;

    if(!::GetDiskFreeSpaceEx(GetTestFilePath(szVolName),
        &uliFreeToCaller, &uliTotal, &uliTotalFree))
    {
        ERR("GetDiskFreeSpaceEx()");
    }
    else
    {
        dwFreeSpace = (DWORD)uliFreeToCaller.QuadPart;
    }
    
    return dwFreeSpace;
}

BOOL CMtdPart::FormatVolume(VOID)
{
    return TRUE;
}

BOOL CMtdPart::UnFillDisk(VOID)
{
    TCHAR szFileName[MAX_PATH];
    TCHAR szVolName[VOLUMENAMESIZE];
    HANDLE hFind;
    WIN32_FIND_DATA w32fd;

    // find any fill file because there could be multiple on large disks
    ::_stprintf(szFileName, _T("%s\\%s*"), GetVolumeName(szVolName), EAT_FILE_NAME);
    hFind = ::FindFirstFile(szFileName, &w32fd);
    if(VALID_HANDLE(hFind))
    {
        do
        {
            ::_stprintf(szFileName, _T("%s\\%s"), GetVolumeName(szVolName),
                w32fd.cFileName);
            LOG(_T("Removing fill file %s..."), szFileName);
            ::DeleteFile(szFileName);
            
        } while(::FindNextFile(hFind, &w32fd));
        ::FindClose(hFind);
    }
    
    return TRUE;
}

BOOL CMtdPart::FillDiskMinusXBytesWithWriteFile(DWORD freeSpace)
{
    BOOL fRet = FALSE;
    TCHAR szFileName[MAX_PATH];
    HANDLE hFile = INVALID_HANDLE_VALUE;
    ULARGE_INTEGER fileSize;
    ULARGE_INTEGER fb, tb, tfb;
    BYTE buffer[WRITE_BUFFER_SIZE];
    DWORD bytesToWrite, bytesWritten;
    TCHAR szVolName[VOLUMENAMESIZE];
    TCHAR szDevName[DEVICENAMESIZE];

    LOG(_T("Filling partition [%s]%s so that %u bytes remain (long version)"), 
        GetDeviceName(szDevName), GetVolumeName(szVolName), freeSpace);

    // the fill file might already exist, so delete it before we query disk space
    UnFillDisk();  // this will fail when the file doesn't exist

    // build the fill file name
    ::_stprintf(szFileName, _T("%s\\%s"), GetVolumeName(szVolName), EAT_FILE_NAME);
    
    if(!::GetDiskFreeSpaceEx(GetVolumeName(szVolName), &fb, &tb, &tfb))
    {
        ERR("GetDiskFreeSpaceEx()");
        goto EXIT;
    }

    LOG(_T("Partition [%s]%s has %I64u free bytes"),  
        GetDeviceName(szDevName), GetVolumeName(szVolName), fb.QuadPart);

    if(fb.QuadPart < freeSpace)
    {
        LOG(_T("Disk has only %I64u free bytes"), fb.QuadPart);
        DETAIL("Not enough disk space for fill file");
        goto EXIT;
    }

    // size of the file to create
    fileSize.QuadPart = fb.QuadPart - freeSpace;
    
    LOG(_T("Creating file %s of %I64u bytes to fill disk"), szFileName, fileSize.QuadPart);

    hFile = ::CreateFile(szFileName, GENERIC_READ | GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(INVALID_HANDLE(hFile))
    {
        ERR("CreateFile()");
        goto EXIT;
    }

    // write buffer to the file until disk is appropriately full
    while(fileSize.QuadPart > 0)
    {
        if(fileSize.QuadPart > WRITE_BUFFER_SIZE)
            bytesToWrite = WRITE_BUFFER_SIZE;
        else
            bytesToWrite = fileSize.LowPart;
        
        if(!::WriteFile(hFile, buffer, bytesToWrite, &bytesWritten, NULL))
        {
            ERR("WriteFile()");
            goto EXIT;
        }
        fileSize.QuadPart -= bytesWritten;
    }
    
    fRet = TRUE;
    
EXIT:
    if(VALID_HANDLE(hFile))
        ::CloseHandle(hFile);

    return fRet;
    
}

/* fill the partition so that only freeSpace, in bytes, remains */
BOOL CMtdPart::FillDiskMinusXBytesWithSetEndOfFile(DWORD freeSpace)
{
    BOOL fRet = FALSE;
    TCHAR szFileName[MAX_PATH];
    HANDLE hFile = INVALID_HANDLE_VALUE;
    ULARGE_INTEGER fileSize;
    ULARGE_INTEGER fb, tb, tfb;
    TCHAR szVolName[VOLUMENAMESIZE];
    TCHAR szDevName[DEVICENAMESIZE];
    LONG distToMove;
    UINT fileCount;    

    LOG(_T("Filling partition [%s]%s so that %u bytes remain (quick version)"), 
        GetDeviceName(szDevName), GetVolumeName(szVolName), freeSpace);

    // the fill file might already exist, so delete it before we query disk space
    UnFillDisk();  // this will fail when the file doesn't exist    
    
    if(!::GetDiskFreeSpaceEx(GetVolumeName(szVolName), &fb, &tb, &tfb))
    {
        ERR("GetDiskFreeSpaceEx()");
        goto EXIT;
    }

    LOG(_T("Partition [%s]%s has %I64u free bytes"),  
        GetDeviceName(szDevName), GetVolumeName(szVolName), fb.QuadPart);

    // the free space requested was more than is currently free on the disk
    if(fb.QuadPart < freeSpace)
    {
        LOG(_T("Disk has only %I64u free bytes"), fb.QuadPart);
        DETAIL("Not enough disk space for fill file");
        goto EXIT;
    }

    // size of the file to create is current free space minus requested free space
    fileSize.QuadPart = fb.QuadPart - freeSpace;

    // some things can occur when we're trying to fill a disk:
    //
    //  the disk could be larger than 2 GB, the FATFS file size limit -- this 
    //  require multiple fill files
    //
    //  the disk could advertise more free space than is actually available; 
    //  TFAT seems to have this issue
    //
    fileCount = 0;
    while(fileSize.QuadPart > 0)
    {
#define MIN(x, y)   (x < y ? x : y)

        // create several fill files, if required, because
        // SetFilePointer won't go past 32-bit address of a file
        distToMove = (LONG)MIN(MAXDWORD>>1, fileSize.QuadPart);

        // build the fill file name
        ::_stprintf(szFileName, _T("%s\\%s%u"), GetVolumeName(szVolName), EAT_FILE_NAME,
            ++fileCount);
        
        LOG(_T("Creating file %s of %u bytes to fill disk"), szFileName, distToMove);
        hFile = ::CreateFile(szFileName, GENERIC_READ | GENERIC_WRITE, 0, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if(INVALID_HANDLE(hFile))
        {
            ERR("CreateFile()");
            goto EXIT;
        }

        do
        {
            do
            {
                // set file pointer
                if(0xFFFFFFFF == ::SetFilePointer(hFile, distToMove, NULL, FILE_BEGIN) &&
                   NO_ERROR != ::GetLastError())
                {
                    ERR("SetFilePointer()");
                    goto EXIT;
                }            
                
                // it might not actually allow us to write a file which is this big

            } while(!::SetEndOfFile(hFile) && (distToMove -= GetClusterSize()));

            if(!::FlushFileBuffers(hFile)) 
            {
                ERR("FlushFileBuffers()");
                goto EXIT;
            }            

            // query the current free space
            if(!::GetDiskFreeSpaceEx(GetVolumeName(szVolName), &fb, &tb, &tfb))
            {
                ERR("GetDiskFreeSpaceEx()");
                goto EXIT;
            }

            if(freeSpace > fb.QuadPart) 
            {
                distToMove -= (DWORD)(freeSpace - fb.QuadPart);
            }

        } while(freeSpace > fb.QuadPart);               

        if(fb.QuadPart > freeSpace)
        {
            fileSize.QuadPart = fb.QuadPart - freeSpace;
        }
        else
        {
            fileSize.QuadPart = 0;
        }

        

        LOG(_T("Partition [%s]%s now has %I64u free bytes"),  
            GetDeviceName(szDevName), GetVolumeName(szVolName), fb.QuadPart);

        ::CloseHandle(hFile);
        hFile = INVALID_HANDLE_VALUE;
    }

    fRet = TRUE;
    
EXIT:
    if(VALID_HANDLE(hFile))
        ::CloseHandle(hFile);

    return fRet;
}

BOOL CMtdPart::FillDiskMinusXBytes(DWORD freeSpace, BOOL fQuick)
{
    // must round to the nearest cluster since this is the smallest amount
    // of disk space which can actually be allocated
    freeSpace -= (freeSpace % GetClusterSize());
    
    if(fQuick)
        return FillDiskMinusXBytesWithSetEndOfFile(freeSpace);
    else
        return FillDiskMinusXBytesWithWriteFile(freeSpace);
}

/* create a file named szName of size fileSize, in bytes, on the mounted filesystem */
BOOL CMtdPart::CreateTestFile(LPCTSTR szName, DWORD fileSize)
{
    BOOL fRet = FALSE;
    TCHAR szFileName[MAX_PATH] = _T("");
    HANDLE hFile;
    BYTE buffer[WRITE_BUFFER_SIZE];
    DWORD cBytesToWrite, cBytesWritten;
    TCHAR szVolName[VOLUMENAMESIZE];

    ::_stprintf(szFileName, _T("%s\\%s"), GetVolumeName(szVolName), szName);

    LOG(_T("Creating file %s of size %u bytes"), szFileName, fileSize);

    hFile = ::CreateFile(szFileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if(INVALID_HANDLE(hFile))
    {
        ERR("CreateFile()");
        goto EXIT;
    }

    // write data to the file
    while(fileSize > 0)
    {
        if(fileSize > WRITE_BUFFER_SIZE)
            cBytesToWrite = WRITE_BUFFER_SIZE;
        else
            cBytesToWrite = fileSize;

        if(!::WriteFile(hFile, buffer, cBytesToWrite, &cBytesWritten, NULL))
        {
            ERR("WriteFile()");
            goto EXIT;
        }

        fileSize -= cBytesWritten;
    }

    // commit stream data to file
    if(!::FlushFileBuffers(hFile))
    {
        ERR("FlushFileBuffers()");
        goto EXIT;
    }

    fRet = TRUE;
EXIT:

    if(VALID_HANDLE(hFile))
        ::CloseHandle(hFile);
    
    return fRet;
}

BOOL CMtdPart::DeleteTestFile(LPCTSTR szName)
{
    TCHAR szFileName[MAX_PATH] = _T("");
    TCHAR szVolName[VOLUMENAMESIZE];

    ::_stprintf(szFileName, _T("%s\\%s"), GetVolumeName(szVolName), szName);
    LOG(_T("Deleting file %s"), szFileName);

    return ::DeleteFile(szFileName);
}

BOOL CMtdPart::CreateTestDirectory(LPCTSTR szName)
{
    TCHAR szDirName[MAX_PATH] = _T("");
    TCHAR szVolName[VOLUMENAMESIZE];

    ::_stprintf(szDirName, _T("%s\\%s"), GetVolumeName(szVolName), szName);
    LOG(_T("Creating directory %s"), szDirName);

    return ::CreateDirectory(szDirName, NULL);
}

BOOL CMtdPart::RemoveTestDirectory(LPCTSTR szName)
{
    TCHAR szDirName[MAX_PATH] = _T("");
    TCHAR szVolName[VOLUMENAMESIZE];

    ::_stprintf(szDirName, _T("%s\\%s"), GetVolumeName(szVolName), szName);
    LOG(_T("Removing directory %s"), szDirName);

    return ::RemoveDirectory(szDirName);
}

DWORD CMtdPart::GetClusterSize(VOID)
{
    return m_nClusterSize;
}

DWORD CMtdPart::GetMaxRootDirEntries(VOID)
{
    return m_nRootDirs;
}

DWORD CMtdPart::GetUniqueId(VOID)
{
    return m_uniqueId;
}

// --------------------------------------------------------------------
TCHAR *CMtdPart::GetTestFilePath(LPTSTR pszPathBuf)
// --------------------------------------------------------------------
{
    LPTSTR pszTmpPathBuf = GetVolumeName(pszPathBuf);
    _tcscat(pszTmpPathBuf, _TEST_FILE_PATH_TEXT);
    return pszTmpPathBuf;
}

// --------------------------------------------------------------------
TCHAR *CMtdPart::GetCopyFilePath(LPTSTR pszPathBuf)
// --------------------------------------------------------------------
{
    LPTSTR pszTmpPathBuf = GetTestFilePath(pszPathBuf);
    _tcscat(pszTmpPathBuf, _COPY_FILE_PATH_TEXT);
    return pszTmpPathBuf; 
}

// --------------------------------------------------------------------
TCHAR *CMtdPart::GetMoveFilePath(LPTSTR pszPathBuf)
// --------------------------------------------------------------------
{
    LPTSTR pszTmpPathBuf = GetTestFilePath(pszPathBuf);
    _tcscat(pszTmpPathBuf, _MOVE_FILE_PATH_TEXT);
    return pszTmpPathBuf;
}

// --------------------------------------------------------------------
TCHAR *CMtdPart::GetOsTestFilePath(LPTSTR pszPathBuf)
// --------------------------------------------------------------------
{
    wsprintf(pszPathBuf, TEXT("%s%u"), _TEST_FILE_PATH_TEXT, GetUniqueId());
    return pszPathBuf;
}

// --------------------------------------------------------------------
TCHAR *CMtdPart::GetOsCopyFilePath(LPTSTR pszPathBuf)
// --------------------------------------------------------------------
{
    wsprintf(pszPathBuf, TEXT("%s%u%s"), _TEST_FILE_PATH_TEXT, GetUniqueId(), _COPY_FILE_PATH_TEXT);
    return pszPathBuf;
}

// --------------------------------------------------------------------
TCHAR *CMtdPart::GetOsMoveFilePath(LPTSTR pszPathBuf)
// --------------------------------------------------------------------
{
    wsprintf(pszPathBuf, TEXT("%s%u%s"), _TEST_FILE_PATH_TEXT, GetUniqueId(), _MOVE_FILE_PATH_TEXT);
    return pszPathBuf;
}

// --------------------------------------------------------------------
DWORD CMtdPart::GetNumberOfRootDirEntries(VOID)
// --------------------------------------------------------------------
{
    TCHAR szFileSpec[MAX_PATH], szPathBuf[MAX_PATH];
    WIN32_FIND_DATA ff;
    HANDLE hFind;
    DWORD dwRootEntries = 0;

    _stprintf(szFileSpec, _T("%s\\*.*"), GetVolumeName(szPathBuf));
    hFind = FindFirstFile(szFileSpec, &ff);
    if(VALID_HANDLE(hFind))
    {
        dwRootEntries++;
        while(FindNextFile(hFind, &ff))
        {
            dwRootEntries++;
        }
        FindClose(hFind);
    }

    return dwRootEntries;
}

// --------------------------------------------------------------------
void CMtdPart::LogWin32StorageDeviceInfo(PSTORAGEDEVICEINFO pInfo)
// --------------------------------------------------------------------
{
    if(pInfo)
    {
        LOG(L"STORAGEDEVICEINFO");
        LOG(L"  szProfile:                  %s",    pInfo->szProfile);
        LOG(L"  dwDeviceClass:              0x%08X",pInfo->dwDeviceClass);
        LOG(L"  dwDeviceType:               0x%08X",pInfo->dwDeviceType);
        LOG(L"  dwDeviceFlags:              0x%08X",pInfo->dwDeviceFlags);
    }
}

// --------------------------------------------------------------------
BOOL CMtdPart::WaitForMount(DWORD cTimeout)
// --------------------------------------------------------------------
{
    DWORD cStartTime = ::GetTickCount();
    while(0xFFFFFFFF == ::GetFileAttributes(m_partInfo.szVolumeName))
    {
        if(INFINITE != cTimeout && 
           cTimeout <= ::GetTickCount() - cStartTime)
        {
            return FALSE;
        }
        ::Sleep(0);
    }
    return TRUE;
}

// --------------------------------------------------------------------
void CMtdPart::LogWin32StoreInfo(PSTOREINFO pInfo)
// --------------------------------------------------------------------
{
    SYSTEMTIME sysTime = {0};
    
    if(pInfo)
    {
        LOG(L"STOREINFO");
        LOG(L"  szDeviceName:               %s",    pInfo->szDeviceName);
        LOG(L"  szStoreName:                %s",    pInfo->szStoreName);
        LOG(L"  dwDeviceClass:              %u",    pInfo->dwDeviceClass);
        LOG(L"  dwDeviceType:               %u",    pInfo->dwDeviceType);
        LOG(L"  dwDeviceFlags:              %u",    pInfo->dwDeviceFlags);        
        LOG(L"  snNumSectors:               %I64u", pInfo->snNumSectors);
        LOG(L"  dwBytesPerSector:           %u",    pInfo->dwBytesPerSector);
        LOG(L"  snFreeSectors:              %I64u", pInfo->snFreeSectors);
        LOG(L"  snBiggestPartCreatable:     %I64u", pInfo->snBiggestPartCreatable);
    
        FileTimeToSystemTime(&pInfo->ftCreated, &sysTime);
        LOG(L"  ftCreated:                  %u/%u/%u, %u:%u:%u:%u",
            sysTime.wMonth, sysTime.wDay, sysTime.wYear, sysTime.wHour, 
            sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds);
        
        FileTimeToSystemTime(&pInfo->ftLastModified, &sysTime);
        LOG(L"  ftLastModified:             %u/%u/%u, %u:%u:%u:%u",
            sysTime.wMonth, sysTime.wDay, sysTime.wYear, sysTime.wHour, 
            sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds);

        LOG(L"  dwPartitionCount:           %u",    pInfo->dwPartitionCount);
        LOG(L"  dwMountCount:               %u",    pInfo->dwMountCount);
    
        LOG(L"  dwAttributes:               0x%08x", pInfo->dwAttributes);
        LOGFLAG(pInfo->dwAttributes, STORE_ATTRIBUTE_AUTOFORMAT);
        LOGFLAG(pInfo->dwAttributes, STORE_ATTRIBUTE_AUTOMOUNT);
        LOGFLAG(pInfo->dwAttributes, STORE_ATTRIBUTE_AUTOPART);
        LOGFLAG(pInfo->dwAttributes, STORE_ATTRIBUTE_READONLY);
        LOGFLAG(pInfo->dwAttributes, STORE_ATTRIBUTE_REMOVABLE);
        LOGFLAG(pInfo->dwAttributes, STORE_ATTRIBUTE_UNFORMATTED);

        LogWin32StorageDeviceInfo(&pInfo->sdi);
        
    }
}

// --------------------------------------------------------------------
void CMtdPart::LogWin32PartInfo(PPARTINFO pInfo)
// --------------------------------------------------------------------
{
    SYSTEMTIME sysTime = {0};
    
    if(pInfo)
    {
        LOG(L"PARTINFO");
        LOG(L"  szPartitionName             %s",    pInfo->szPartitionName);
        LOG(L"  szFileSys                   %s",    pInfo->szFileSys);
        LOG(L"  szVolumeNmae                %s",    pInfo->szVolumeName);
        LOG(L"  snNumSectors:               %I64u", pInfo->snNumSectors);
    
        FileTimeToSystemTime(&pInfo->ftCreated, &sysTime);        
        LOG(L"  ftCreated:                  %u/%u/%u, %u:%u:%u:%u",
            sysTime.wMonth, sysTime.wDay, sysTime.wYear, sysTime.wHour, 
            sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds);
        
        FileTimeToSystemTime(&pInfo->ftLastModified, &sysTime);        
        LOG(L"  ftLastModified:             %u/%u/%u, %u:%u:%u:%u",
            sysTime.wMonth, sysTime.wDay, sysTime.wYear, sysTime.wHour, 
            sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds);
        
        LOG(L"  dwAttributes:               0x%08x", pInfo->dwAttributes);
        LOGFLAG(pInfo->dwAttributes, PARTITION_ATTRIBUTE_ACTIVE);
        LOGFLAG(pInfo->dwAttributes, PARTITION_ATTRIBUTE_AUTOFORMAT);
        LOGFLAG(pInfo->dwAttributes, PARTITION_ATTRIBUTE_BOOT);
        LOGFLAG(pInfo->dwAttributes, PARTITION_ATTRIBUTE_EXPENDABLE);
        LOGFLAG(pInfo->dwAttributes, PARTITION_ATTRIBUTE_MOUNTED);
        LOGFLAG(pInfo->dwAttributes, PARTITION_ATTRIBUTE_READONLY);

        LOG(L"  bPartType:                  0x%02x", pInfo->bPartType);
        
        LOGPARTTYPE(pInfo->bPartType, PART_DOS2_FAT);
        LOGPARTTYPE(pInfo->bPartType, PART_DOS3_FAT);
        LOGPARTTYPE(pInfo->bPartType, PART_DOS4_FAT);
        LOGPARTTYPE(pInfo->bPartType, PART_DOS32);
        LOGPARTTYPE(pInfo->bPartType, PART_DOS32X13);
        LOGPARTTYPE(pInfo->bPartType, PART_DOSX13);
        LOGPARTTYPE(pInfo->bPartType, PART_DOSX13X);      
    }
}

// --------------------------------------------------------

CFATMtdPart::CFATMtdPart(PSTOREINFO pStoreInfo, PPARTINFO pPartInfo) :
    CMtdPart(pStoreInfo, pPartInfo, 0, 0) // we'll override max root-dirs and cluster size ourselves
{
    GetFATInfo(&m_fatInfo);
    DisplayFATInfo(&m_fatInfo);
}

CFATMtdPart::~CFATMtdPart(VOID)
{
    
}

VOID CFATMtdPart::DisplayFATInfo(PFATINFO pFatInfo)
{
    TCHAR szValue[32];
    DWORD dwClusterSize = pFatInfo->dwBytesPerSector * pFatInfo->dwSectorsPerCluster;

    LOG(TEXT(""));
    LOG(TEXT("FAT Format Information:"));
    LOG(TEXT("  Total Bytes:     %12s"), FormatValue(pFatInfo->dwTotalSectors * pFatInfo->dwBytesPerSector, szValue));
    LOG(TEXT("  Usable Bytes:    %12s"), FormatValue(pFatInfo->dwUsableClusters * dwClusterSize, szValue));
    LOG(TEXT("  Used Bytes:      %12s"), FormatValue((pFatInfo->dwUsableClusters - pFatInfo->dwFreeClusters) * dwClusterSize, szValue));
    LOG(TEXT("  Free Bytes:      %12s"), FormatValue(pFatInfo->dwFreeClusters * dwClusterSize, szValue));
    LOG(TEXT("  Media Descriptor:        0x%02X"), pFatInfo->dwMediaDescriptor);
    LOG(TEXT("  Sector Size:     %12s"), FormatValue(pFatInfo->dwBytesPerSector, szValue));
    LOG(TEXT("  Sectors/Cluster: %12s"), FormatValue(pFatInfo->dwSectorsPerCluster, szValue));
    LOG(TEXT("  Cluster Size:    %12s"), FormatValue(dwClusterSize, szValue));
    LOG(TEXT("  Total Sectors:   %12s"), FormatValue(pFatInfo->dwTotalSectors, szValue));
    LOG(TEXT("  Reserved Sectors:%12s"), FormatValue(pFatInfo->dwReservedSecters, szValue));
    LOG(TEXT("  Hidden Sectors:  %12s"), FormatValue(pFatInfo->dwHiddenSectors, szValue));
    LOG(TEXT("  Sectors/Track:   %12s"), FormatValue(pFatInfo->dwSectorsPerTrack, szValue));
    LOG(TEXT("  Number of Heads: %12s"), FormatValue(pFatInfo->dwHeads, szValue));
    LOG(TEXT("  Sectors/FAT:     %12s"), FormatValue(pFatInfo->dwSectorsPerFAT, szValue));
    LOG(TEXT("  Number of FATs:  %12s"), FormatValue(pFatInfo->dwNumberOfFATs, szValue));
    LOG(TEXT("  Root Entries:    %12s"), FormatValue(pFatInfo->dwRootEntries, szValue));
    LOG(TEXT("  Usable Clusters: %12s"), FormatValue(pFatInfo->dwUsableClusters, szValue));
    LOG(TEXT("  Used Clusters:   %12s"), FormatValue(pFatInfo->dwUsableClusters - pFatInfo->dwFreeClusters, szValue));
    LOG(TEXT("  Free Clusters:   %12s"), FormatValue(pFatInfo->dwFreeClusters, szValue));
    LOG(TEXT(""));
}

BOOL CFATMtdPart::GetFATInfo(PFATINFO pFatInfo)
{
    BOOL fRet = FALSE;
    TCHAR szVol[MAX_PATH];
    HANDLE hVol;
    DEVPB volpb = {0};
    FREEREQ fr = {0};
    DWORD cb = 0;

    _stprintf(szVol, _T("%s\\VOL:"), m_partInfo.szVolumeName);
    hVol = ::CreateFile(szVol, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if(INVALID_HANDLE(hVol))
    {
        ERRFAIL("CreateFile()");
        goto EXIT;
    }
    else
    {
        if(!::DeviceIoControl(hVol, IOCTL_DISK_GET_VOLUME_PARAMETERS, NULL, 0, &volpb, 
            sizeof(volpb), &cb, NULL) || cb != sizeof(volpb))
        {
            ERRFAIL("DeviceIoControl(IOCTL_DISK_GET_VOLUME_PARAMETERS)");
            goto EXIT;
        }
        else
        {
            m_fatInfo.dwMediaDescriptor   = (DWORD)volpb.DevPrm.ExtDevPB.BPB.oldBPB.BPB_MediaDescriptor;
            m_fatInfo.dwBytesPerSector    = (DWORD)volpb.DevPrm.ExtDevPB.BPB.oldBPB.BPB_BytesPerSector;
            m_fatInfo.dwSectorsPerCluster = (DWORD)volpb.DevPrm.ExtDevPB.BPB.oldBPB.BPB_SectorsPerCluster;
            m_fatInfo.dwTotalSectors      = (DWORD)volpb.DevPrm.ExtDevPB.BPB.oldBPB.BPB_TotalSectors ?
                (DWORD)volpb.DevPrm.ExtDevPB.BPB.oldBPB.BPB_TotalSectors :
                (DWORD)volpb.DevPrm.ExtDevPB.BPB.oldBPB.BPB_BigTotalSectors;
            m_fatInfo.dwReservedSecters   = (DWORD)volpb.DevPrm.ExtDevPB.BPB.oldBPB.BPB_ReservedSectors;
            m_fatInfo.dwHiddenSectors     = (DWORD)volpb.DevPrm.ExtDevPB.BPB.oldBPB.BPB_HiddenSectors;
            m_fatInfo.dwSectorsPerTrack   = (DWORD)volpb.DevPrm.ExtDevPB.BPB.oldBPB.BPB_SectorsPerTrack;
            m_fatInfo.dwHeads             = (DWORD)volpb.DevPrm.ExtDevPB.BPB.oldBPB.BPB_Heads;
            m_fatInfo.dwSectorsPerFAT     = (DWORD)volpb.DevPrm.ExtDevPB.BPB.oldBPB.BPB_SectorsPerFAT;
            m_fatInfo.dwNumberOfFATs      = (DWORD)volpb.DevPrm.ExtDevPB.BPB.oldBPB.BPB_NumberOfFATs;
            m_fatInfo.dwRootEntries       = (DWORD)volpb.DevPrm.ExtDevPB.BPB.oldBPB.BPB_RootEntries;

            if(!::DeviceIoControl(hVol, IOCTL_DISK_GET_FREE_SPACE, NULL, 0, &fr, sizeof(fr), 
                &(cb = 0), NULL) || cb != sizeof(fr))
            {
                ERRFAIL("DeviceIoControl(IOCTL_DISK_GET_FREE_SPACE");
                goto EXIT;
            }
            else
            {
                m_fatInfo.dwUsableClusters = fr.fr_Clusters;
                m_fatInfo.dwFreeClusters   = fr.fr_FreeClusters;
            }
        }
        ::CloseHandle(hVol);
    }

    fRet = TRUE;
    
EXIT:
    return fRet;
}

DWORD CFATMtdPart::GetClusterSize(VOID)
{
    return (m_fatInfo.dwSectorsPerCluster * m_fatInfo.dwBytesPerSector);
}

DWORD CFATMtdPart::GetMaxRootDirEntries(VOID)
{
    return m_fatInfo.dwRootEntries;
}

BOOL CFATMtdPart::FormatVolume(VOID)
{
    // load utility library
    BOOL fRet = FALSE;
    HINSTANCE hUtilLib = NULL;
    PFN_FORMATVOLUME pfn = NULL;
    FORMAT_OPTIONS fopts;

    // get the current fat information
    if(!GetFATInfo(&m_fatInfo))
    {
        fRet = FALSE;
        FAIL("GetFATInfo()");
        goto EXIT;
    }

    // load the utility library
    hUtilLib = ::LoadLibrary(L"FATUTIL.DLL");
    if(NULL == hUtilLib)
    {
        ERRFAIL("LoadLibrary(FATUTIL.DLL)");
        goto EXIT;
    }

    // extract the fn pointer for the format volume function
    pfn  = (PFN_FORMATVOLUME)::GetProcAddress(hUtilLib, SZ_FN_FORMATVOLUME);
    if(!pfn)
    {
        ERRFAIL("GetProcAddres()");
        goto EXIT;
    }        

    if(!::DismountPartition(m_hPart))
    {
        ERRFAIL("DismountPartition()");
        goto EXIT;
    }

    LOG(_T("Formatting %s\\%s..."), m_storeInfo.szDeviceName, m_partInfo.szPartitionName);

    // format the partition based on the current format information
    fopts.dwClusSize = m_fatInfo.dwBytesPerSector * m_fatInfo.dwSectorsPerCluster;
    fopts.dwFatVersion = 32;
    fopts.dwRootEntries = m_fatInfo.dwRootEntries;
    fopts.dwNumFats = m_fatInfo.dwNumberOfFATs;
    fopts.dwFlags = 0;    

    // don't have a good way of determining if it is TFAT already, so we always 
    // use TFAT if there are 2 FATs on the volume. If the FAT implementation does
    // not include TFAT support, this will basically be ignored
    if(2 == fopts.dwNumFats) {
        fopts.dwFlags |= FATUTIL_FORMAT_TFAT;
    }

    // call the format function
    fRet = (ERROR_SUCCESS == pfn(m_hPart, &m_diskInfo, &fopts, NULL, NULL));

    // remount the partition
    if(!::MountPartition(m_hPart))
    {
        fRet = FALSE;
        ERRFAIL("MountPartition()");
        goto EXIT;
    }

    if(!WaitForMount(1000))
    {
        fRet = FALSE;
        FAIL("WaitForMount()");
        goto EXIT;
    }
    
    if(!GetFATInfo(&m_fatInfo))
    {
        fRet = FALSE;
        FAIL("GetFATInfo()");
        goto EXIT;
    }

    LOG(_T("Format on %s\\%s %s."), m_storeInfo.szDeviceName,
        m_partInfo.szPartitionName, fRet ? _T("succeeded") : _T("failed"));

    DisplayFATInfo(&m_fatInfo);

EXIT:
    return fRet;
}
