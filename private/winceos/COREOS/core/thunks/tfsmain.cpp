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
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//

#define BUILDING_FS_STUBS
#include <windows.h>
#include <mloaderverifier.h>
#include <guiddef.h>
#include <diskio.h>
#include <acctid.h>
#include <samplent.h>

// Automatically initialized correctly
extern "C" CEGUID SystemGUID;
CEGUID SystemGUID;

extern "C"
BOOL
WINAPI
xxx_DeviceIoControl(
    HANDLE          hDevice,
    DWORD           dwIoControlCode,
    LPVOID          lpInBuf,
    DWORD           nInBufSize,
    LPVOID          lpOutBuf,
    DWORD           nOutBufSize,
    LPDWORD         lpBytesReturned,
    LPOVERLAPPED    lpOverlapped
    );

//
// SDK exports
//

extern "C"
BOOL WINAPI xxx_CreateDirectoryW(LPCWSTR lpPathName,
                        LPSECURITY_ATTRIBUTES lpsa) {
    return CreateDirectoryW_Trap(lpPathName,lpsa);
}

extern "C"
BOOL WINAPI xxx_RemoveDirectoryW(LPCWSTR lpPathName) {
    return RemoveDirectoryW_Trap(lpPathName);
}

extern "C"
BOOL WINAPI xxx_MoveFileW(LPCWSTR lpExistingFileName,
                     LPCWSTR lpNewFileName) {
    return MoveFileW_Trap(lpExistingFileName,lpNewFileName);
}

extern "C"
BOOL WINAPI xxx_DeleteFileW(LPCWSTR lpFileName) {
    return DeleteFileW_Trap(lpFileName);
}

extern "C"
BOOL xxx_GetDiskFreeSpaceExW(LPCTSTR lpDirectoryName, PULARGE_INTEGER lpFreeBytesAvailableToCaller,
    PULARGE_INTEGER lpTotalNumberOfBytes, PULARGE_INTEGER lpTotalNumberOfFreeBytes) {
    if (!lpDirectoryName) {
        // NULL path indicates root file system for backwards 
        // compatibility.
        lpDirectoryName = L"\\";
    }
    return GetDiskFreeSpaceExW_Trap(lpDirectoryName, lpFreeBytesAvailableToCaller,
        lpTotalNumberOfBytes, lpTotalNumberOfFreeBytes);
}

extern "C"
HANDLE WINAPI xxx_FindFirstFileExW(LPCWSTR lpFileName, FINDEX_INFO_LEVELS fInfoLevelId, LPVOID lpFindFileData,
    FINDEX_SEARCH_OPS fSearchOp, LPVOID lpSearchFilter, DWORD dwAdditionalFlags) {
    return FindFirstFileExW_Trap(lpFileName, fInfoLevelId, lpFindFileData, sizeof(WIN32_FIND_DATAW), fSearchOp, lpSearchFilter, dwAdditionalFlags);
}

extern "C"
BOOL WINAPI xxx_SetFileAttributesW(LPCWSTR lpszName,
                          DWORD dwAttributes) {
    return SetFileAttributesW_Trap(lpszName,dwAttributes);
}

extern "C"
DWORD WINAPI xxx_GetFileSize(HANDLE hFile,LPDWORD lpFileSizeHigh) {
    return GetFileSize_Trap(hFile,lpFileSizeHigh);
}

extern "C"
BOOL WINAPI xxx_GetFileInformationByHandle(HANDLE hFile,
    LPBY_HANDLE_FILE_INFORMATION lpFileInformation) {
    return GetFileInformationByHandle_Trap(hFile,lpFileInformation,sizeof(BY_HANDLE_FILE_INFORMATION));
}

extern "C"
BOOL WINAPI xxx_FlushFileBuffers(HANDLE hFile) {
    return FlushFileBuffers_Trap(hFile);
}

extern "C"
BOOL WINAPI xxx_GetFileTime(HANDLE hFile, LPFILETIME lpCreation,
                       LPFILETIME lpLastAccess,
                       LPFILETIME lpLastWrite) {
    return GetFileTime_Trap(hFile,lpCreation,lpLastAccess,lpLastWrite);
}

extern "C"
BOOL WINAPI xxx_SetFileTime(HANDLE hFile,
                       CONST FILETIME *lpCreation,
                       CONST FILETIME *lpLastAccess,
                       CONST FILETIME *lpLastWrite) {
    return SetFileTime_Trap(hFile,lpCreation,lpLastAccess,lpLastWrite);
}

extern "C"
BOOL WINAPI xxx_SetEndOfFile(HANDLE hFile) {
    return SetEndOfFile_Trap(hFile);
}

extern "C"
BOOL WINAPI xxx_FindClose(HANDLE hFindFile) {
    return CloseHandle (hFindFile);
}

extern "C"
BOOL WINAPI xxx_FindNextFileW(HANDLE hFindFile, LPWIN32_FIND_DATAW lpFindFileData) {
    return FindNextFileW_Trap(hFindFile,lpFindFileData,sizeof(WIN32_FIND_DATAW));
}

extern "C"
BOOL xxx_DeleteAndRenameFile (LPCWSTR lpszOldFile, LPCWSTR lpszNewFile) {
    return DeleteAndRenameFile_Trap(lpszOldFile,lpszNewFile);
}

extern "C"
void xxx_SignalStarted(DWORD dw) {
    SignalStarted_Trap(dw);
}

extern "C"
BOOL xxx_GetStoreInformation(LPSTORE_INFORMATION lpsi)
{
    return GetStoreInformation_Trap(lpsi);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// CeOidGetInfo

// Helper for converting between SORTORDERSPEC versions.
// FromEx could throw away some information
void CopySortsFromEx(SORTORDERSPEC* pDest, SORTORDERSPECEX* pSrc, WORD wNumSorts)
{
    WORD wSort;

    for (wSort = 0; (wSort < wNumSorts) && (wSort < CEDB_MAXSORTORDER); wSort++) {
        // If there are multiple props, only copy the first
        DEBUGMSG(pSrc->wNumProps > 1,
                 (TEXT("DB: Warning - returning incomplete information, use OidGetInfoEx2 instead\r\n")));
        pDest->propid = pSrc->rgPropID[0];
        pDest->dwFlags = pSrc->rgdwFlags[0];

        // Since there is only one prop in the Ex version, the unique flag
        // is kept there.  But in the Ex2 version, it's in the keyflags.
        if (pSrc->wKeyFlags & CEDB_SORT_UNIQUE) {
            pDest->dwFlags |= CEDB_SORT_UNIQUE;
        }

        pSrc++;
        pDest++;
    }
}


extern "C"
BOOL xxx_CeOidGetInfoEx(PCEGUID pguid, CEOID oid, CEOIDINFO *oidInfo)
{
    CEOIDINFOEX exInfo;

    exInfo.wVersion = CEOIDINFOEX_VERSION;

    if (!CeOidGetInfoEx2_Trap(pguid, oid, &exInfo)) {
        return FALSE;
    }

    // Shoehorn the Ex struct into the old-style oidInfo
    __try {
        
        oidInfo->wObjType = exInfo.wObjType;
        switch (exInfo.wObjType) {
        case OBJTYPE_FILE:
            memcpy((LPBYTE)&oidInfo->infFile, (LPBYTE)&(exInfo.infFile),
                   sizeof(CEFILEINFO));
            break;

        case OBJTYPE_DIRECTORY:
            memcpy((LPBYTE)&(oidInfo->infDirectory), (LPBYTE)&(exInfo.infDirectory),
                   sizeof(CEDIRINFO));
            break;

        case OBJTYPE_DATABASE:
            {
                // Can't copy exactly
                CEDBASEINFOEX* pdbSrc = &(exInfo.infDatabase);
                CEDBASEINFO* pdbDest = &(oidInfo->infDatabase);

                // If the number of records exceeds a WORD's worth, we can't convert
                if (pdbSrc->dwNumRecords & 0xFFFF0000) {
                    SetLastError(ERROR_NOT_SUPPORTED);
                    return FALSE;
                }

                pdbDest->wNumRecords = (WORD)pdbSrc->dwNumRecords;
                pdbDest->dwFlags = pdbSrc->dwFlags;
                VERIFY (SUCCEEDED (StringCchCopyW (pdbDest->szDbaseName, _countof(pdbSrc->szDbaseName), pdbSrc->szDbaseName)));
                pdbDest->dwDbaseType = pdbSrc->dwDbaseType;
                pdbDest->wNumSortOrder = pdbSrc->wNumSortOrder;
                pdbDest->dwSize = pdbSrc->dwSize;
                memcpy(&(pdbDest->ftLastModified), &(pdbSrc->ftLastModified),
                       sizeof(FILETIME));

                CopySortsFromEx(pdbDest->rgSortSpecs, pdbSrc->rgSortSpecs, pdbSrc->wNumSortOrder);
            }
            break;

        case OBJTYPE_RECORD:
            memcpy((LPBYTE)&(oidInfo->infRecord), (LPBYTE)&(exInfo.infRecord),
                   sizeof(CERECORDINFO));
            break;

        default:
            // No data to copy
            ;
        }

        return TRUE;
    
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        SetLastError(ERROR_INVALID_PARAMETER);
    }

    return FALSE;
}

extern "C"
BOOL xxx_CeOidGetInfo(CEOID oid, CEOIDINFO *oidInfo)
{
    return xxx_CeOidGetInfoEx(&SystemGUID, oid, oidInfo);
}


//
// OAK exports
//

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

extern "C"
BOOL xxx_CeRegisterFileSystemNotification(HWND hWnd)
{
    return CeRegisterFileSystemNotification_Trap(hWnd);
}

extern "C"
BOOL xxx_RegisterAFSEx(int index, HANDLE h, DWORD dw, DWORD version, DWORD flags)
{
    return RegisterAFSEx_Trap(index, h, dw, version, flags);
}

extern "C"
BOOL xxx_DeregisterAFS(int index)
{
    return DeregisterAFS_Trap(index);
}    

extern "C"
int xxx_RegisterAFSName(LPCWSTR pName)
{
    return RegisterAFSName_Trap(pName);
}

extern "C"
BOOL xxx_DeregisterAFSName(int index)
{
    return DeregisterAFSName_Trap(index);
}

extern "C"
BOOL xxx_GetSystemMemoryDivision(LPDWORD lpdwStorePages, LPDWORD lpdwRamPages, LPDWORD lpdwPageSize) 
{
    return GetSystemMemoryDivision_Trap(lpdwStorePages,lpdwRamPages,lpdwPageSize);
}

extern "C"
DWORD xxx_SetSystemMemoryDivision(DWORD dwStorePages) 
{
    return SetSystemMemoryDivision_Trap(dwStorePages);
}

extern "C"
void xxx_DumpFileSystemHeap(void)
{
    // THIS API HAS BEEN DEPRECATED
    return;
}

extern "C"
void xxx_FileSystemPowerFunction(DWORD flags) {
    FileSystemPowerFunction_Trap(flags);
}

extern "C"
void xxx_CloseAllFileHandles() {
    CloseAllFileHandles_Trap();
}

extern "C"
BOOL
xxx_ReadFileWithSeek(
    HANDLE          hFile,
    LPVOID          lpBuffer,
    DWORD           nNumberOfBytesToRead,
    LPDWORD         lpNumberOfBytesRead,
    LPOVERLAPPED    lpOverlapped,
    DWORD           dwLowOffset,
    DWORD           dwHighOffset
    )
{
    return ReadFileWithSeek_Trap(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped, dwLowOffset, dwHighOffset);
}

extern "C"
BOOL
xxx_WriteFileWithSeek(
    HANDLE          hFile,
    LPCVOID         lpBuffer,
    DWORD           nNumberOfBytesToWrite,
    LPDWORD         lpNumberOfBytesWritten,
    LPOVERLAPPED    lpOverlapped,
    DWORD           dwLowOffset,
    DWORD           dwHighOffset
    )
{
    return WriteFileWithSeek_Trap(hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten, lpOverlapped, dwLowOffset, dwHighOffset);
}


extern "C"
BOOL
xxx_ReadFileScatter(
    HANDLE                  hFile,
    FILE_SEGMENT_ELEMENT    aSegmentArray[],
    DWORD                   nNumberOfBytesToRead,
    LPDWORD                 lpReserved,
    LPOVERLAPPED            lpOverlapped
    )
{
    return xxx_DeviceIoControl(hFile, IOCTL_FILE_READ_SCATTER, aSegmentArray, nNumberOfBytesToRead, lpReserved, 
        0, NULL, NULL);
}

extern "C"
BOOL
xxx_WriteFileGather(
    HANDLE                    hFile,
    FILE_SEGMENT_ELEMENT     aSegmentArray[],
    DWORD                    nNumberOfBytesToWrite,
    LPDWORD                lpReserved,
    LPOVERLAPPED            lpOverlapped
    )
{
    return xxx_DeviceIoControl(hFile, IOCTL_FILE_WRITE_GATHER, aSegmentArray, nNumberOfBytesToWrite, lpReserved, 
        0, NULL, NULL);
}

extern "C"
BOOL
xxx_LockFileEx(
    HANDLE hFile,
    DWORD dwFlags,
    DWORD dwReserved,
    DWORD nNumberOfBytesToLockLow,
    DWORD nNumberOfBytesToLockHigh,
    LPOVERLAPPED lpOverlapped
    )
{
    return LockFileEx_Trap(hFile, dwFlags, dwReserved, nNumberOfBytesToLockLow, nNumberOfBytesToLockHigh, lpOverlapped);
}

extern "C"
BOOL
xxx_UnlockFileEx(
    HANDLE hFile,
    DWORD dwReserved,
    DWORD nNumberOfBytesToUnlockLow,
    DWORD nNumberOfBytesToUnlockHigh,
    LPOVERLAPPED lpOverlapped
    )
{
    return UnlockFileEx_Trap(hFile, dwReserved, nNumberOfBytesToUnlockLow, nNumberOfBytesToUnlockHigh, lpOverlapped);
}

extern "C"
DWORD xxx_GetTempPathW(DWORD nBufferLength, LPWSTR lpBuffer) {
    if (MAXDWORD / sizeof(WCHAR) < nBufferLength) {
        SetLastError(ERROR_ARITHMETIC_OVERFLOW);
        return 0;
    }
    return GetTempPathW_Trap(lpBuffer, nBufferLength * sizeof(WCHAR));
}

extern "C"
BOOL xxx_IsSystemFile(LPCWSTR lpszFileName) {
    return IsSystemFile_Trap(lpszFileName);
}

extern "C"
HANDLE xxx_RequestDeviceNotifications(LPCGUID devclass, HANDLE hMsgQ, BOOL fAll) {
    return RequestDeviceNotifications_Trap(devclass, hMsgQ, fAll);
}

extern "C"
BOOL xxx_StopDeviceNotifications(HANDLE h) {
    return StopDeviceNotifications_Trap(h);
}

extern "C"
BOOL xxx_AdvertiseInterface(const GUID *devclass, LPCWSTR name, BOOL fAdd) {
    return AdvertiseInterface_Trap(devclass, name, 0xFFFFFFFF, fAdd);
}

extern "C"
BOOL xxx_AdvertiseInterfaceEx(const GUID *devclass, LPCWSTR name, DWORD dwId, BOOL fAdd) {
    return AdvertiseInterface_Trap(devclass, name, dwId, fAdd);
}

extern "C"
HANDLE xxx_CeCreateTokenFromAccount (LPCWSTR pszAccountName)
{
    RETAILMSG(1, (TEXT("\r\n\r\n*** CeCreateTokenFromAccount() called - Not supported in CE7.\r\n\r\n")));
    ASSERT(0);

    SetLastError(ERROR_NOT_SUPPORTED);
    return INVALID_HANDLE_VALUE;
}


//
// Convert a String SD to SD
//
extern "C"
BOOL xxx_CeCvtStrToSD (
  LPCWSTR StringSecurityDescriptor,
  DWORD StringSDRevision,
  PSECURITY_DESCRIPTOR* SecurityDescriptor,
  PULONG SecurityDescriptorSize
) {
    return CvtStrToSD_Trap (StringSecurityDescriptor, StringSDRevision, SecurityDescriptor, SecurityDescriptorSize);
}

//
// convert a SD to string SD
//
extern "C"
BOOL xxx_CeCvtSDToStr (
  PSECURITY_DESCRIPTOR SecurityDescriptor,
  DWORD RequestedStringSDRevision,
  SECURITY_INFORMATION SecurityInformation,
  LPWSTR* StringSecurityDescriptor,
  PULONG StringSecurityDescriptorLen
) {
    return CvtSDToStr_Trap (SecurityDescriptor, RequestedStringSDRevision, SecurityInformation, StringSecurityDescriptor, StringSecurityDescriptorLen);
}


//
// secure loader interfaces
//

extern "C"
HRESULT xxx_LoaderVerifierInitialize(
    LPCWSTR pszLvModulePath
)
{
    return LoaderVerifierInitialize_Trap (pszLvModulePath);
}

extern "C"
HRESULT xxx_LoaderVerifierUninitialize()
{
    return LoaderVerifierUninitialize_Trap ();
}

extern "C"
HRESULT xxx_LoaderVerifierAuthenticateFile(
    const GUID*   guidAuthClass,
    HANDLE    hFile,
    LPCWSTR   szFilePath,
    LPCWSTR   szHashHint,
    HANDLE    hauthnCatalog,
    LPHANDLE  phslauthnFile
)
{
    return LoaderVerifierAuthenticateFile_Trap ((LPVOID)guidAuthClass, sizeof (GUID), hFile, szFilePath, szHashHint, hauthnCatalog, phslauthnFile);
}

extern "C"
HRESULT xxx_LoaderVerifierAuthorize(
    HANDLE            hslauthnFile,
    enum LV_AUTHORIZATION* pslauthz
)
{
    return LoaderVerifierAuthorize_Trap (hslauthnFile, pslauthz);
}

extern "C"
HRESULT xxx_LoaderVerifierGetHash(
    HANDLE   hslauthnFile,
    LPCWSTR  pszHashHint,
    LPBYTE   pbHash,
    DWORD    cbHash,
    LPDWORD  pcbHash,
    LPWSTR*  ppszHashAlgorithm
)
{
    return LoaderVerifierGetHash_Trap (hslauthnFile, pszHashHint, pbHash, cbHash, pcbHash, ppszHashAlgorithm);
}
    
extern "C"
HRESULT xxx_LoaderVerifierAddToBlockList(
    enum LV_BLOCKLIST   slBlockList, 
    BYTE*               pbItem, 
    DWORD               cbItem
)
{
    return LoaderVerifierAddToBlockList_Trap (slBlockList, pbItem, cbItem);
}

extern "C"
HRESULT xxx_LoaderVerifierRemoveFromBlockList(
    enum LV_BLOCKLIST   slBlockList, 
    BYTE*               pbItem, 
    DWORD               cbItem
)
{
    return LoaderVerifierRemoveFromBlockList_Trap (slBlockList, pbItem, cbItem);
}

extern "C"
HRESULT xxx_LoaderVerifierIsInBlockList(
    enum LV_BLOCKLIST   slBlockList, 
    BYTE*               pbItem, 
    DWORD               cbItem,
    BOOL*               pfInBlockList

)
{
    return LoaderVerifierIsInBlockList_Trap (slBlockList, pbItem, cbItem, pfInBlockList);
}

extern "C"
HRESULT xxx_LoaderVerifierEnumBlockList(
    enum LV_BLOCKLIST   slBlockList, 
    DWORD               iEntry,
    BYTE*               pbItem, 
    DWORD               cbItem,
    DWORD*              pcbItem
)
{
    return LoaderVerifierEnumBlockList_Trap (slBlockList, iEntry, pbItem, cbItem, pcbItem);
}



extern "C"
BOOL
WINAPI
xxx_SampleCounterForEntropyPool(ULONG ulSampleLocationIndex, 
                            __in_bcount(cbAdditionalData) const BYTE * rgAdditionalData,
                            ULONG cbAdditionalData)
{
#ifdef KCOREDLL
    return SampleCounterForEntropyPool_Trap(ulSampleLocationIndex, rgAdditionalData, cbAdditionalData);
#else
    return FALSE;
#endif // !KCOREDLL
}

