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
#ifndef __STOREHLP_H__
#define __STOREHLP_H__

#include <windows.h>
#include <storemgr.h>
#include <diskio.h>
#include <fatutil.h>
#include <pnp.h>
#include <devload.h>
#include <msgqueue.h>

// These are defied in fathlp.h but we can't really ship them so copy
// the definition here.... (otherwise it won't build the test tree)
#define FMTVOL_12BIT_FAT        0x00000040      // 12-bit FAT desired
#define FMTVOL_16BIT_FAT        0x00000080      // 16-bit FAT desired
#define FMTVOL_32BIT_FAT        0x00000100      // 32-bit FAT desired

// Read/write size for the test buffer
#define TEST_WRITE_BUFFER_SIZE  1024*1024 // 1MB buffer
#define TEST_READ_BUFFER_SIZE   1024*1024 // 1MB buffer

//
// C++ Interface Definition
//

// an interface to all the information about a mounted partition
class CFSInfo {
public:
    CFSInfo(LPCWSTR pszRootDirectory = NULL);
    ~CFSInfo();

    const LPWSTR RootDirectory(
        LPWSTR pszRootDirectory = NULL,
        UINT cLen = 0);

    const LPCE_VOLUME_INFO CFSInfo::GetVolumeInfo(LPCE_VOLUME_INFO pVolumeInfo = NULL);
    const PPARTINFO PartInfo(PPARTINFO pPartInfo = NULL);
    const PSTOREINFO StoreInfo(PSTOREINFO pStoreInfo = NULL);

    const BOOL GetDiskFreeSpaceEx(PULARGE_INTEGER pFreeBytesAvailable,
        PULARGE_INTEGER pTotalNumberOfBytes,
        PULARGE_INTEGER pTotalNumberOfFreeBytes);

    const BOOL Format(PFORMAT_OPTIONS pFormatOptions);
    const BOOL FormatEx(PFORMAT_PARAMS pFormatParams);

    const BOOL Scan(PSCAN_OPTIONS pScanOptions);
    const BOOL ScanEx(PSCAN_PARAMS pScanParams);

    const BOOL Defrag(PDEFRAG_OPTIONS pDefragOptions);
    const BOOL DefragEx(PDEFRAG_PARAMS pDefragParams);

    const BOOL Mount(void);
    const BOOL Dismount(void);

    static const LPWSTR FindStorageWithProfile(
        LPCWSTR pszProfile,
        __out_ecount(cLen) LPWSTR pszRootDirectory,
        UINT cLen,
        BOOL bReadOnlyOk = TRUE);

    static const BOOL WaitForFATFSMount(
        IN LPCWSTR pszExpectedFolderName,
        __out_ecount(cLen) LPWSTR pszMountedFolderName,
        UINT cLen,
        BOOL fExactFolderNameMatch,
        DWORD dwWaitTimeout);

    const BOOL EnableVolumeAccess();
    const BOOL DisableVolumeAccess();

    static void DoOutputStoreInfo();
    static const PSECTOR_LIST_ENTRY GetFileSectorInfo(IN HANDLE hFileHandle, IN OUT LPDWORD pdwBytesReturned);
    static const BOOL OutputFileSectorInfo(IN LPCTSTR pszFileName);
private:
    static const WCHAR DEF_ROOT_DIRECTORY[];
    CE_VOLUME_INFO m_volInfo;
    PARTINFO m_partInfo;
    STOREINFO m_storeInfo;
    HANDLE m_hStore;
    HANDLE m_hPart;
}; //Class: CFSInfo

namespace StoreHlp
{

    typedef enum{
        OneFileWithSetFilePointer = 0,
        OneFileWithWriteFile,
        ManyFilesWithSetFilePointer,
        ManyFilesWithWriteFile,
        OneFileWithWriteFileWithSeek,
        OneFileWriteFileGather
    } FillSpaceMethod;

    //Convert the #defines from fathlp.h into a usable type
    typedef enum
    {
        FORMAT_FAT12 = FMTVOL_12BIT_FAT,    /*0x00000040*/
        FORMAT_FAT16 = FMTVOL_16BIT_FAT,    /*0x00000080*/
        FORMAT_FAT32 = FMTVOL_32BIT_FAT,    /*0x00000100*/
        FORMAT_UNKNOWN,
        NUM_FORMAT_TYPES = 4
    } DISK_FORMAT_TYPE;

    DISK_FORMAT_TYPE GetPartitionFatFormat(  PARTINFO &sPartInfo  );
    void OutputStoreInfo();
    BOOL PrintFreeDiskSpace(LPCWSTR szRootPath);
    BOOL IsFileExists(LPCWSTR szFile);
    BOOL IsPathWriteable( const TCHAR* szRootPath );
    BOOL GenerateRandomPattern(__out_ecount(cbPattern) BYTE* pbPattern, size_t cbPattern);
    BOOL IsExFAT(LPCTSTR szRootPath);
    BOOL IsObjStore(LPCTSTR szRootPath);
    HRESULT FillDiskSpace(LPCWSTR szRootPath, ULARGE_INTEGER ulBytes, FillSpaceMethod fillSpaceMethod);
    HRESULT FillDiskSpaceMinusXBytes(LPCWSTR szRootPath, ULARGE_INTEGER ulBytesLeft, FillSpaceMethod fillSpaceMethod);
    HRESULT FillDiskSpaceMinusXPercent(LPCWSTR szRootPath, DWORD dwPercentLeft, FillSpaceMethod fillSpaceMethod);
    BOOL DeleteTree(LPCTSTR szPath, BOOL fDeleteRoot);
    BOOL GetStoragePathByProfile(LPWSTR pszProfileName, LPWSTR pszPath, DWORD cchPath);
    BOOL GetFileChecksum(LPCTSTR pszPath, LPDWORD lpdwFileChkSum);
}

//
// C Interface Definition
//

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __PARTDATA {
    HANDLE hStore;
    HANDLE hPart;
    STOREINFO storeInfo;
    PARTINFO partInfo;

// don't want to touch these
    HANDLE hFindStore;
    HANDLE hFindPart;
    WCHAR szProfileMatch[PROFILENAMESIZE];

} PARTDATA, *PPARTDATA;

void DumpVolumeInfo(CE_VOLUME_INFO& sVolumeInfo);
void DumpStoreInfo(STOREINFO& sStoreInfo);
void DumpPartInfo(PARTINFO& sPartInfo);
void Translate(DWORD dwValue, void (*printCallback)(DWORD) );
void PrettyPrintPartType(DWORD dwMask);
void PrettyPrintPartAttrib(DWORD dwMask);
void PrettyPrintVolumeAttrib(DWORD dwMask);
void PrettyPrintVolumeFlags(DWORD dwMask);
void PrettyPrintDevType(DWORD dwMask);
void PrettyPrintDevClass(DWORD dwMask);
void PrettyPrintDevFlags(DWORD dwMask);
void PrettyPrintStoreAttrib(DWORD dwMask);


BOOL Stg_RefreshStore (
    IN LPCWSTR pszStore
    );

BOOL Stg_RefreshAllStores (
    );

BOOL Stg_GetStoreInfoByPath (
    IN LPCWSTR pszPath,
    OUT PSTOREINFO pStoreInfo,
    OUT PPARTINFO pPartInfo
    );

PPARTDATA Stg_FindFirstPart (
    IN LPCWSTR pszProfileMatch,
    IN BOOL bReadOnlyOk = TRUE
    );

BOOL Stg_FindNextPart (
    IN PPARTDATA pInfo
    );

BOOL Stg_FindClosePart (
    IN PPARTDATA pInfo
    );

BOOL Stg_QueryProfileRegValue(
    IN PPARTDATA pPartData,
    IN LPCWSTR pszValueName,
    OUT PDWORD pType,
    OUT LPBYTE pData,
    IN OUT PDWORD pcbData
    );


BOOL Stg_FormatPart (
    IN HANDLE hPart,
    IN PFORMAT_OPTIONS pFormatOpts
    );

BOOL Stg_FormatPartEx (
    IN HANDLE hPart,
    IN PFORMAT_PARAMS pFormatParams
    );

BOOL Stg_ScanPart (
    IN HANDLE hPart,
    IN PSCAN_OPTIONS pScanOpts
    );

BOOL Stg_ScanPartEx (
    IN HANDLE hPart,
    IN PSCAN_PARAMS pScanParams
    );

BOOL Stg_DefragPart (
    IN HANDLE hPart,
    IN PDEFRAG_OPTIONS pDefragOpts
    );

BOOL Stg_DefragPartEx (
    IN HANDLE hPart,
    IN PDEFRAG_PARAMS pDefragOpts
    );


BOOL Stg_MountPart (
    IN HANDLE hPart
    );

BOOL Stg_DismountPart (
    IN HANDLE hPart
    );

#ifdef __cplusplus
};
#endif


//
// Useful node/class to populate with test files and keep track of content's checksums
//
typedef struct _TEST_FILE_INFO {
    TCHAR szFullPath[MAX_PATH];
    BOOL fDirectory;
    DWORD dwSize;
    DWORD dwAttribute;
    DWORD dwCheckSum;
} TEST_FILE_INFO, *LTEST_FILE_INFO;

// Test file node
typedef struct _TEST_FILE_NODE {
    TEST_FILE_INFO tfi;
    struct _TEST_FILE_NODE *pNext;

    // Initialization
    _TEST_FILE_NODE::_TEST_FILE_NODE() {
        pNext = NULL;
        };
} TEST_FILE_NODE, *LPTEST_FILE_NODE;


//
// CFileList: A list to store information about test files
//
class CTestFileList
{
public:

    // Constructor / Destructor
    CTestFileList();
    ~CTestFileList();

    // Public Methods
    void Init();
    void Deinit();
    BOOL Create(BOOL fDirectory, LPCTSTR szFullPath, DWORD dwAttrib, DWORD dwSize);
    BOOL Resize(LPCTSTR szFullPath, DWORD dwSize);
    BOOL Append(LPCTSTR szFullPath, DWORD dwAppendSize);
    BOOL Delete(LPCTSTR szFullPath);
    BOOL DeleteAll();
    BOOL ReadFirst(TEST_FILE_INFO* lpFileInfo);
    BOOL ReadNext(TEST_FILE_INFO* lpFileInfo);
    void ReadClose();
    BOOL VerifyTFI(TEST_FILE_INFO& tfi);
    BOOL Verify();
    void PrintList();
    DWORD Count();

private:

    // Private Methods
    BOOL Destroy();

    // Private Declarations
    TEST_FILE_NODE* m_pHead; // Head of the list
    TEST_FILE_NODE* m_pTail;  // Last node on the list
    TEST_FILE_NODE* m_pCurrent;   // Current node while reading

    CRITICAL_SECTION m_csTestFileList; // Critical section for test file list
};


#endif //__STOREHLP_H__
