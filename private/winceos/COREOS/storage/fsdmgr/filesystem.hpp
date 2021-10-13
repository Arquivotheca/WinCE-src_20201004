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
#ifndef __FILESYSTEM_HPP__
#define __FILESYSTEM_HPP__

#include <errorrep.h>
#include <notify.h>

class MountableDisk_t;

typedef BOOL    (*PFNAPI)();
typedef BOOL    (*PFNMOUNTDISK)(PDSK pDsk);
typedef BOOL    (*PFNUNMOUNTDISK)(PDSK pDsk);
typedef DWORD   (*PFNRWDISK)(PDSK pDsk, DWORD dwSector, DWORD cSectors, PBYTE pBuffer, DWORD cbBuffer);
typedef DWORD   (*PFNHOOKVOLUME)(PDSK hDsk, PFILTERHOOK pHook);
typedef BOOL    (*PFNUNHOOKVOLUME)(DWORD hVolume);
typedef DWORD   (*PFNFORMATVOLUME)(PDSK pDsk);
typedef BOOL    (*PFNGETVOLUMEINFO)(PDSK pDsk, FSD_VOLUME_INFO *pInfo);

// FileSystem_t
//
// Abstract class describing a file system and its entry points.
// 
class FileSystem_t {

public:

    // Enumerated type used to indicate the type of an object that
    // inherits from FileSystem_t. Any child class should set the
    // m_ThisFileSystemType member of FileSystem_t to the appropriate
    // type in the costructor. This is helpful for debugging.
    enum FileSystemType_e {
        FileSystemDriver_Type,
        FileSystemFilterDriver_Type
    };   

    // Valid flags for the m_FSDFlags member.
    static const DWORD FSD_FLAGS_PAGEABLE   = 0x00000001;
    static const DWORD FSD_FLAGS_FSCTL_TRUST_REQUIRED = 0x00000002;
    static const DWORD FSD_FLAGS_LOCK_IO_BUFFERS = 0x00000004;
    static const DWORD FSD_FLAGS_POWER_NOTIFY = 0x00000008;
    
    FileSystem_t (MountableDisk_t* pDisk) :
        m_pDisk (pDisk),
        m_FSDFlags (0),
        m_dwFreeClusters (0),
        m_dwFreeSpaceThreshold (0)
    { 
        ::memset (&m_FilterHook, 0, sizeof (m_FilterHook));
        m_FilterHook.cbSize = sizeof (m_FilterHook);
    } 
    
    virtual ~FileSystem_t ()
    { ; }

    // Pure, virtual function responsible for loading the file system and
    // populating all function pointers in the FILTERHOOK structure.
    virtual LRESULT Load () = 0;
    
    // Pure, virtual function responsible for unloading the file system.
    virtual LRESULT Unload () = 0;

    // Return a pointer to the filterhook function table for this file system.
    // This is required when chaining filters.
    inline FILTERHOOK* GetFilterHook ()
    {
        return &m_FilterHook;
    }    

    // Retrieve the FSD flags associated with this file system.
    inline DWORD GetFSDFlags ()
    {
        return m_FSDFlags;
    }
    
    // Call the file system's CloseVolume function.
    inline BOOL CloseVolume ()
    {
        BOOL fResult = FALSE;
        __try {
            fResult = m_FilterHook.pCloseVolume (m_FilterHook.hVolume);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        return fResult;
    }

    // Call the file system's CreateDirectory function.
    inline BOOL CreateDirectoryW (const WCHAR* pDirectoryName, 
            SECURITY_ATTRIBUTES* pSecurityAttributes)
    {
        BOOL fResult = FALSE;
        __try {
            fResult = m_FilterHook.pCreateDirectoryW (m_FilterHook.hVolume, 
                    pDirectoryName, pSecurityAttributes);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        return fResult;
    }

    // Call the file system's RemoveDirectory function.
    inline BOOL RemoveDirectoryW (const WCHAR* pDirectoryName)
    {
        BOOL fResult = FALSE;
        __try {
            fResult = m_FilterHook.pRemoveDirectoryW (m_FilterHook.hVolume, pDirectoryName);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        return fResult;
    }

    // Call the file system's GetFileAttributes function.
    inline DWORD GetFileAttributesW (const WCHAR* pPathName)
    {
        DWORD Result = INVALID_FILE_ATTRIBUTES;
        __try {
            Result = m_FilterHook.pGetFileAttributesW (m_FilterHook.hVolume, pPathName);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        return Result;
    }
    
    // Call the file system's SetFileAttributes function.
    inline BOOL SetFileAttributesW (const WCHAR* pPathName, DWORD Attributes)
    {
        BOOL fResult = FALSE;
        __try {
            fResult = m_FilterHook.pSetFileAttributesW (m_FilterHook.hVolume, pPathName,
                Attributes);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        return fResult;
    }

    // Call the file system's DeleteFileW function.
    inline BOOL DeleteFileW (const WCHAR* pFileName)
    {
        BOOL fResult = FALSE;
        __try {
            fResult = m_FilterHook.pDeleteFileW (m_FilterHook.hVolume, pFileName);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        return fResult;
    }
    
    // Call the file system's MoveFileW function.
    inline BOOL MoveFileW (const WCHAR* pOldFileName, const WCHAR* pNewFileName)
    {
        BOOL fResult = FALSE;
        __try {
            fResult = m_FilterHook.pMoveFileW (m_FilterHook.hVolume, pOldFileName, 
                pNewFileName);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        return fResult;
    }
    
    // Call the file system's DeleteAndRenameFileW function.
    inline BOOL DeleteAndRenameFileW (const WCHAR* pDestFileName, const WCHAR* pSourceFileName)
    {
        BOOL fResult = FALSE;
        __try {
            fResult = m_FilterHook.pDeleteAndRenameFileW (m_FilterHook.hVolume, pDestFileName, 
                pSourceFileName);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        return fResult;
    }

    // Call the file system's GetDiskFreeSpaceW function.
    inline BOOL GetDiskFreeSpaceW (const WCHAR* pPathName, DWORD* pSectorsPerCluster, 
        DWORD* pBytesPerSector, DWORD* pFreeClusters, DWORD* pClusters)
    {
        BOOL fResult = FALSE;
        __try {
            fResult = m_FilterHook.pGetDiskFreeSpaceW (m_FilterHook.hVolume, pPathName, 
                pSectorsPerCluster, pBytesPerSector, pFreeClusters, pClusters);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        return fResult;
    }

    // Call the file system's Notify function.
    inline void Notify (DWORD Flags)
    {
        __try {
            // Only send power notifications if the FSD requires them.
            if (FSD_FLAGS_POWER_NOTIFY & m_FSDFlags) {
                m_FilterHook.pNotify (m_FilterHook.hVolume, Flags);
            }
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
    }

    // Call the file system's RegisterFileSystemFunction function.
    inline BOOL RegisterFileSystemFunction (SHELLFILECHANGEFUNC_t pFn) 
    {
        BOOL fResult = FALSE;
        __try {
            fResult = m_FilterHook.pRegisterFileSystemFunction (m_FilterHook.hVolume, pFn);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        return fResult;
    }

    // Call the file system's FindFirstFileW function.
    inline HANDLE FindFirstFileW (HANDLE hProc, const WCHAR* pFileSpec, WIN32_FIND_DATA* pFindData) 
    {
        HANDLE hResult = INVALID_HANDLE_VALUE;
        __try {
            hResult = m_FilterHook.pFindFirstFileW (m_FilterHook.hVolume, hProc, pFileSpec, pFindData);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        return hResult; 
    }

    // Call the file system's FindNextFileW function.
    inline BOOL FindNextFileW (DWORD hSearch, WIN32_FIND_DATA* pFindData) 
    {
        BOOL fResult = FALSE;
        __try {
            fResult = m_FilterHook.pFindNextFileW (hSearch, pFindData);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        return fResult;
    }

    // Call the file system's FindClose function.
    inline BOOL FindClose (DWORD hSearch) 
    {
        BOOL fResult = FALSE;
        __try {
            fResult = m_FilterHook.pFindClose (hSearch);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        return fResult;
    }

    // Call the file system's CreateFile function.
    inline HANDLE CreateFileW (HANDLE hProc, const WCHAR* FileName, DWORD AccessMode, 
            DWORD ShareMode, SECURITY_ATTRIBUTES* pSecurityAttributes, 
            DWORD CreationDisposition, DWORD FlagsAndAttributes, 
            HANDLE hTemplateFile)
    {
        HANDLE hResult = INVALID_HANDLE_VALUE;
        __try {
            hResult = m_FilterHook.pCreateFileW (m_FilterHook.hVolume, hProc, 
                    (WCHAR*)FileName, AccessMode, ShareMode, pSecurityAttributes, 
                    CreationDisposition, FlagsAndAttributes, hTemplateFile);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        return hResult;
    }
    
    // Call the file system's ReadFile function.
    inline BOOL ReadFile (DWORD hFile, void* pBuffer, DWORD BufferSize, DWORD* BytesRead,
        OVERLAPPED* pOverlapped)
    {       
        BOOL fResult = FALSE;
        __try {
            fResult = m_FilterHook.pReadFile (hFile, pBuffer, BufferSize, BytesRead, pOverlapped);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }        
        return fResult;
    }
    
    // Call the file system's ReadFileWithSeek function.
    inline BOOL ReadFileWithSeek (DWORD hFile, void* pBuffer, DWORD BufferSize, DWORD* BytesRead,
        OVERLAPPED* pOverlapped, DWORD LowOffset, DWORD HighOffset)
    {
        if (!pBuffer && !BufferSize && !(FSD_FLAGS_PAGEABLE & m_FSDFlags)) {
            // This is a special query performed by the kernel to test whether or
            // not the file system driver supports paging. Explicitly fail the
            // test if we don't support paging. If we don't know, then let the FSD
            // succeed or fail the test.
            ::SetLastError (ERROR_NOT_SUPPORTED);
            return FALSE;
        }
                
        BOOL fResult = FALSE;        
        __try {
            fResult = m_FilterHook.pReadFileWithSeek (hFile, pBuffer, BufferSize, BytesRead, 
                pOverlapped, LowOffset, HighOffset);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        
        return fResult;
    }
    
    // Call the file system's WriteFile function.
    inline BOOL WriteFile (DWORD hFile, const void* pBuffer, DWORD BufferSize, 
        DWORD* BytesWritten, OVERLAPPED* pOverlapped)
    {       
        BOOL fResult = FALSE;
        __try {
            fResult = m_FilterHook.pWriteFile (hFile, pBuffer, BufferSize, BytesWritten, pOverlapped);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }        
        return fResult;
    }
    
    // Call the file system's WriteFileWithSeek function.
    inline BOOL WriteFileWithSeek (DWORD hFile, const void* pBuffer, DWORD BufferSize, 
        DWORD* BytesWritten, OVERLAPPED* pOverlapped, DWORD LowOffset, DWORD HighOffset)
    {
        BOOL fResult = FALSE;
        __try {
            fResult = m_FilterHook.pWriteFileWithSeek (hFile, pBuffer, BufferSize, BytesWritten, 
                pOverlapped, LowOffset, HighOffset);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }       
        return fResult;
    }

    // Call the file system's SetFilePointer function.
    inline DWORD SetFilePointer (DWORD hFile, LONG DistanceToMoveLow, LONG* pDistanceToMoveHigh,
        DWORD MoveMethod)
    {
        DWORD Result = INVALID_SET_FILE_POINTER;
        __try {
            Result = m_FilterHook.pSetFilePointer (hFile, DistanceToMoveLow, pDistanceToMoveHigh,
                MoveMethod);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        return Result;
    }

    // Call the file system's GetFileSize function.
    inline DWORD GetFileSize (DWORD hFile, DWORD* pFileSizeHigh)
    {
        DWORD Result = INVALID_FILE_SIZE;
        __try {
            Result = m_FilterHook.pGetFileSize (hFile, pFileSizeHigh);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        return Result;
    }
    
    // Call the file system's GetFileInformationByHandle function.
    inline BOOL GetFileInformationByHandle (DWORD hFile, BY_HANDLE_FILE_INFORMATION* pFileInfo)
    {
        BOOL fResult = FALSE;
        __try {
            fResult = m_FilterHook.pGetFileInformationByHandle (hFile, pFileInfo);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        return fResult;
    }

    // Call the file system's FlushFileBuffers function.
    inline BOOL FlushFileBuffers (DWORD hFile)
    {
        BOOL fResult = FALSE;
        __try {
            fResult = m_FilterHook.pFlushFileBuffers (hFile);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        return fResult;
    }
    
    // Call the file system's GetFileTime function.
    inline BOOL GetFileTime (DWORD hFile, FILETIME* pCreation, FILETIME* pLastAccess, 
        FILETIME* pLastWrite)
    {
        BOOL fResult = FALSE;
        __try {
            fResult = m_FilterHook.pGetFileTime (hFile, pCreation, pLastAccess, pLastWrite);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        return fResult;
    }

    // Call the file system's SetFileTime function.
    inline BOOL SetFileTime (DWORD hFile, const FILETIME* pCreation, 
        const FILETIME* pLastAccess, const FILETIME* pLastWrite)
    {
        BOOL fResult = FALSE;
        __try {
            fResult = m_FilterHook.pSetFileTime (hFile, pCreation, pLastAccess, pLastWrite);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        return fResult;
    }
    
    // Call the file system's SetEndOfFile function.
    inline BOOL SetEndOfFile (DWORD hFile)
    {
        BOOL fResult = FALSE;
        __try {
            fResult = m_FilterHook.pSetEndOfFile (hFile);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        return fResult;
    }

    // Call the file system's SetEndOfFile function.
    inline BOOL DeviceIoControl (DWORD hFile, DWORD Ioctl, void* pInBuf, DWORD InBufSize, 
        void* pOutBuf, DWORD OutBufSize, DWORD* pBytesReturned, OVERLAPPED* pOverlapped)
    {
        BOOL fResult = FALSE;
        __try {
            fResult = m_FilterHook.pDeviceIoControl (hFile, Ioctl, pInBuf, InBufSize, pOutBuf,
                OutBufSize, pBytesReturned, pOverlapped);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        return fResult;
    }
    
    // Call the file system's CloseFile function.
    inline BOOL CloseFile (DWORD hFile)
    {
        BOOL fResult = FALSE;
        __try {
            fResult = m_FilterHook.pCloseFile (hFile);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        return fResult;
    }

    // Call the file system's RefreshVolume function.
    inline BOOL RefreshVolume ()
    {
        BOOL fResult = FALSE;
        __try {
            fResult = m_FilterHook.pRefreshVolume (m_FilterHook.hVolume);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        return fResult;
    }

    // Call the file system's FsIoControl function
    inline BOOL FsIoControl (HANDLE /* hProcess */, DWORD Ioctl, void* pInBuf, DWORD InBufSize, 
        void* pOutBuf, DWORD OutBufSize, DWORD* pBytesReturned, OVERLAPPED* pOverlapped)
    {        
        BOOL fResult = FALSE;
        __try {
            fResult = m_FilterHook.pFsIoControl (m_FilterHook.hVolume, Ioctl, pInBuf, InBufSize,
                pOutBuf, OutBufSize, pBytesReturned, pOverlapped);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        return fResult;
    }
    
    // Call the file system's LockFileEx function
    inline BOOL LockFileEx (DWORD hFile, DWORD Flags, DWORD Reserved, DWORD BytesToLockLow, 
        DWORD BytesToLockHigh, OVERLAPPED* pOverlapped)
    {
        BOOL fResult = FALSE;
        __try {
            fResult = m_FilterHook.pLockFileEx (hFile, Flags, Reserved, BytesToLockLow, 
                BytesToLockHigh, pOverlapped);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        return fResult;
    }

    
    // Call the file system's UnlockFileEx function
    inline BOOL UnlockFileEx (DWORD hFile, DWORD Reserved, DWORD BytesToLockLow, 
        DWORD BytesToLockHigh, OVERLAPPED* pOverlapped)
    {
        BOOL fResult = FALSE;
        __try {
            fResult = m_FilterHook.pUnlockFileEx (hFile, Reserved, BytesToLockLow, 
                BytesToLockHigh, pOverlapped);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        return fResult;
    }

    // Call the file system's GetVolumeInfo function
    inline BOOL GetVolumeInfo (FSD_VOLUME_INFO *pInfo)
    {
        BOOL fResult = FALSE;
        __try {
            fResult = m_FilterHook.pGetVolumeInfo (m_FilterHook.hVolume, pInfo);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        return fResult;
    }    

    // Call the file system's ReadFileScatter funcion
    inline BOOL ReadFileScatter (DWORD hFile, HANDLE hProc, FILE_SEGMENT_ELEMENT SegmentArray[], 
        DWORD BytesToRead, DWORD* pReserved, OVERLAPPED* pOverlapped)
    {
        BOOL fResult = FALSE;
        FILE_SEGMENT_ELEMENT* pSegmentArrayLocal = NULL;
        
        LRESULT lResult = OpenSGArray (&pSegmentArrayLocal, SegmentArray, 
            BytesToRead, FALSE, hProc);
        if (ERROR_SUCCESS != lResult) {
            ::SetLastError (lResult);
            return FALSE;
        }

        __try {
            fResult = m_FilterHook.pReadFileScatter (hFile, pSegmentArrayLocal, BytesToRead,
                pReserved, pOverlapped);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }

        CloseSGArray (pSegmentArrayLocal, SegmentArray, BytesToRead, FALSE);

        return fResult;
    }

    // Call the file system's WriteFileGather funcion
    inline BOOL WriteFileGather (DWORD hFile, HANDLE hProc, FILE_SEGMENT_ELEMENT SegmentArray[], 
        DWORD BytesToWrite, DWORD* pReserved, OVERLAPPED* pOverlapped)
    {
        BOOL fResult = FALSE;
        FILE_SEGMENT_ELEMENT* pSegmentArrayLocal = NULL;

        LRESULT lResult = OpenSGArray (&pSegmentArrayLocal, SegmentArray,
                    BytesToWrite, TRUE, hProc);
        if (ERROR_SUCCESS != lResult) {
            ::SetLastError (lResult);
            return FALSE;
        }
        
        __try {
            fResult = m_FilterHook.pWriteFileGather (hFile, pSegmentArrayLocal, BytesToWrite,
                pReserved, pOverlapped);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }

        CloseSGArray (pSegmentArrayLocal, SegmentArray, BytesToWrite, TRUE);

        return fResult;
    }

    DWORD GetFreeSpaceThreshold()
    {
        return m_dwFreeSpaceThreshold;
    }

    void GetMountName(__out_ecount(elem) WCHAR *pMountName, DWORD elem);

    // Notifies when disk free space is low
    void NotifyChangeinFreeSpace()
    {
#ifdef UNDER_CE

        if (m_dwFreeSpaceThreshold == 0) return;

        DWORD dwSectorsPerCluster = 0;
        DWORD dwBytesPerSector = 0;
        DWORD dwFreeClusters = 0;
        DWORD dwClusters = 0;

        this->GetDiskFreeSpaceW(
            L"\\",
            &dwSectorsPerCluster,
            &dwBytesPerSector,
            &dwFreeClusters,
            &dwClusters);

        ULONGLONG ullClusters = dwClusters;

        if ((100ULL*dwFreeClusters < ullClusters*m_dwFreeSpaceThreshold)
            && !(100ULL*m_dwFreeClusters < ullClusters*m_dwFreeSpaceThreshold))
        {
            WCHAR pszMountName[MAX_PATH] ;
            GetMountName(pszMountName,_countof(pszMountName));
            CeEventHasOccurred(NOTIFICATION_EVENT_LOW_STORAGE, pszMountName);
        }

        m_dwFreeClusters = dwFreeClusters;

#endif //UNDER_CE
    }

    // File system meta-operation to quickly determine whether or not a file
    // exists on the volume managed by this file system
    inline BOOL FileExists (const WCHAR* FileName)
    {
        DWORD Attributes = GetFileAttributesW (FileName);
        return (INVALID_FILE_ATTRIBUTES != Attributes);
    }

    static void FSStub_Void (void)
    {
        ::SetLastError (ERROR_NOT_SUPPORTED);
    }

    static BOOL FSStub_Bool (void)
    {
        ::SetLastError (ERROR_NOT_SUPPORTED);
        return FALSE;
    }
    
    static HANDLE FSStub_Handle (void)
    {
        ::SetLastError (ERROR_NOT_SUPPORTED);
        return INVALID_HANDLE_VALUE;
    }
            
    static DWORD FSStub_FileSize (void)
    {
        ::SetLastError (ERROR_NOT_SUPPORTED);
        return INVALID_FILE_SIZE;        
    }

    static DWORD FSStub_FilePointer (void)
    {
        ::SetLastError (ERROR_NOT_SUPPORTED);
        return INVALID_SET_FILE_POINTER;        
    }
    
    static DWORD FSStub_FileAttributes (void)
    {
        ::SetLastError (ERROR_NOT_SUPPORTED);
        return INVALID_FILE_ATTRIBUTES;        
    }

    static LRESULT FSStub_LResult (void)
    {
        return ERROR_NOT_SUPPORTED;
    }
    
protected:

    // Validate parameters passed to WriteFileGather or ReadFileScatter.
    LRESULT OpenSGArray (FILE_SEGMENT_ELEMENT** ppSegmentArrayLocal,
            FILE_SEGMENT_ELEMENT* pSegmentArray, DWORD BytesToTransfer, 
            BOOL fWrite, HANDLE hProc);

    // Free any memory associated with WriteFileGather or ReadFileScatter
    // returned by OpenSGArray. Must be called after OpenSGArray.
    void CloseSGArray (FILE_SEGMENT_ELEMENT* pSegmentArrayLocal,
            FILE_SEGMENT_ELEMENT* pSegmentArray, DWORD BytesToTransfer, 
            BOOL fWrite);
        
    // Get the FSD flags for this FSD instance.
    virtual void LoadSettings ();

    // Contains FSD_FLAGS_XXX settings for this FSD instance.
    DWORD m_FSDFlags;
    
    // Encapsulates all basic file system function pointers and tracks 
    // the volume context to be passed to the file system (hVolume).
    FILTERHOOK m_FilterHook;

    // A reference to the MountableDisk_t object associated with this file 
    // system instance. This is required for invoking the MountDisk/UnmountDisk
    // exports of an FSD and the HookVolume/UnhookVolume exports of a filter.
    MountableDisk_t* m_pDisk;

    // A count of the number of free clusters in a filesystem
    // Used for Low Storage notifications

    DWORD m_dwFreeClusters;

    // Maximum free space percentage below which Low Storage notifications
    // will fire
    DWORD m_dwFreeSpaceThreshold;

    FileSystemType_e m_ThisFileSystemType;
};

// FileSystemDriver_t
//
// Class describing the interface to an FSD.
// 
class FileSystemDriver_t : public FileSystem_t {

public:
    FileSystemDriver_t (MountableDisk_t* pDisk, const WCHAR* pFSDName);
    virtual ~FileSystemDriver_t ();

    // Load will LoadLibrary the FSD dll and extract all FSD exports. Where
    // the FSD does not export a funciton, the FSDMGRStub_ version of the 
    // function will be used in its place. This funciton is analagous to the
    // AllocFSD function in the Macallan storage manager.
    virtual LRESULT Load ();
    
    // Unload will call the FS
    virtual LRESULT Unload ();

    // Mount and associate a volume with the MountableDisk_t object. Association
    // is performed in the FSDMGR_RegisterVolume function, which must be called
    // from the FSD's FSD_MountDisk function.
    inline BOOL Mount ()
    {
        BOOL fResult = FALSE;
        __try {            
            // Pass our member mountable disk as a PDSK to the FSD_UnmountDisk
            // export of the file system driver.
            PDSK pDsk = reinterpret_cast<PDSK> (m_pDisk);
            fResult = m_pfnMountDisk (pDsk);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        return fResult;
    }
    
    // Once a volume is mounted, this function must be called to associate the
    // MountedVolume_t object with the FSD object. This should be called during 
    // the FSDMGR_RegisterVolume function.
    inline BOOL RegisterContext (DWORD hVolume) 
    {
        DEBUGCHK (!m_FilterHook.hVolume); // Must not be currently registered.
        m_FilterHook.hVolume = hVolume;
        return TRUE;
    }
        
    // Call the FSD's FSD_UnmountDisk export.
    inline BOOL Unmount ()
    {
        BOOL fResult = FALSE;
        __try {
            // Pass our member mountable disk as a PDSK to the FSD_UnmountDisk
            // export of the file system driver.
            PDSK pDsk = reinterpret_cast<PDSK> (m_pDisk);
            fResult = m_pfnUnmountDisk (pDsk);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        return fResult;
    }

    // Call the FSD's FSD_FormatVolume export.
    inline LRESULT Format ()
    {
        LRESULT lResult = ERROR_GEN_FAILURE;
        __try {
            // Pass our member mountable disk as a PDSK to the FSD_FormatVolume
            // export of the file system driver.
            PDSK pDsk = reinterpret_cast<PDSK> (m_pDisk);
            lResult = m_pfnFormatVolume (pDsk);
        } __except (::ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            lResult = ERROR_EXCEPTION_IN_SERVICE;
        }
        return lResult;
    }

    static LRESULT GetExportPrefix (HMODULE hDll, __out_ecount(PrefixChars) WCHAR* pPrefix, size_t PrefixChars);

    // Function used by FileSystemDriver_t and FileSystemFilterDriver_t objects to
    // populate the FILTERHOOK structure with function pointers.
    static LRESULT PopulateFilterHook (HMODULE hDll, const WCHAR* pPrefix, 
        FILTERHOOK* pFilterHook);
    

protected:

    struct ExportNameStubPointerPair {
        WCHAR* pName;
        PFNAPI pStubFunction;
    };
    
    static const ExportNameStubPointerPair FilterHookFunctions[];

    // Name of the FSD dll.
    WCHAR* m_pFSDName;

    // Module handle to this FSD Dll returned by LoadLibrary.
    HMODULE m_hFSDDll;

    // Additional FSD-specific file system functions.
    PFNMOUNTDISK m_pfnMountDisk;
    PFNUNMOUNTDISK m_pfnUnmountDisk;
    PFNFORMATVOLUME m_pfnFormatVolume;    
};

// FileSystemFilterDriver_t
//
// Class describing the interface to an FSD Filter.
// 
class FileSystemFilterDriver_t : public FileSystem_t {

public:
    FileSystemFilterDriver_t (MountableDisk_t* pDisk, const WCHAR* pFilterName);
    virtual ~FileSystemFilterDriver_t ();

    // Load will LoadLibrary the Filter dll and extract all Filter exports.
    // Where the Filter does not export a funciton, the FSDMGRStub_ version 
    // of the function will be used in its place. This funciton is analagous 
    // to the AllocFSD function in the Macallan storage manager.
    virtual LRESULT Load ();

    // Unload this filter and all subsequent file systems under it in the chain.
    virtual LRESULT Unload ();

    // Call the Filter's FILTER_UnhookVolume export.
    inline BOOL Unhook ()
    {
        BOOL fResult = FALSE;
        __try {
            fResult = m_pfnUnhookVolume (m_FilterHook.hVolume);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            ::SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
        return fResult;
    }

    // Call the FSD's FILTER_HookVolume export.
    FileSystem_t* Hook (FileSystem_t* pNextFileSystem);

protected:

    // Override the base class' LoadSettings function.
    virtual void LoadSettings ();

    // Name of the filter dll.
    WCHAR* m_pFilterName;

    // Module handle to this Filter Dll returned by LoadLibrary
    HMODULE m_hFilterDll;

    // Additional filter-specific file system functions.
    PFNHOOKVOLUME m_pfnHookVolume;
    PFNUNHOOKVOLUME m_pfnUnhookVolume;

    // Pointer to the next file system filter on the filter chain.
    FileSystem_t* m_pNextFileSystem;
};

#endif // __FILESYSTEM_HPP__
