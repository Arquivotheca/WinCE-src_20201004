//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+-------------------------------------------------------------------------
//
//
//  File:       udfs.h
//
//  Contents:
//
//  Classes:
//
//  Functions:
//
//--------------------------------------------------------------------------

#define WINCEOEM 1      
#include <windows.h>
#include <extfile.h>
#include <fsdmgr.h> 
#include <cdioctl.h>
#include <dvdioctl.h>
#include <udfsdbg.h>
#include <udfshelper.h>
#include <dbt.h>
#include <storemgr.h>

#include <iso9660.h>
#pragma warning( disable : 4200 )
#include <iso13346.h>
#pragma warning( default : 4200 )

#define FS_MOUNT_POINT_NAME L"CD-ROM"
#define FS_MOUNT_POINT_NAME_LENGTH 6

#define MAX_SECTORS_TO_PROBE    10

const BYTE DIR_HEADER_SIGNATURE[8] = {
    0x08,0x11,0x71,0x40,0xB4,0x43,0x52,0x10
};

#define CD_SECTOR_SIZE 2048

#define MAX_CD_READ_SIZE 65536

#define StateClean       0
#define StateDirty       1
#define StateCleaning    2
#define StateInUse       3
#define StateWindUp      4

#define FIND_HANDLE_LIST_START_SIZE 5

#define IS_ANSI_FILENAME 8000

#define FILE_SYSTEM_TYPE_CDFS 1
#define FILE_SYSTEM_TYPE_UDFS 2
#define FILE_SYSTEM_TYPE_CDDA 3

extern CRITICAL_SECTION g_csMain;
extern const SIZE_T g_uHeapInitSize;
extern SIZE_T g_uHeapMaxSize;
extern BOOL g_bDestroyOnUnmount;

typedef struct tagDirectoryEntry
{
    WORD                cbSize; // Length of the whole structure
    WORD                fFlags;
    DWORD               dwDiskLocation;
    DWORD               dwDiskSize; //  Per spec no file > 1 gig
    DWORD               dwByteOffset;
    FILETIME            Time;

    struct tagDirectoryHeader*  pHeader;
    struct tagDirectoryEntry*  pCacheLocation;

    union {
        WCHAR               szNameW[1];
        CHAR                szNameA[1];
    };

} DIRECTORY_ENTRY, *PDIRECTORY_ENTRY, **PPDIRECTORY_ENTRY;

typedef struct tagDirectoryHeader
{
    DWORD               cbSize;
    BYTE                Signature[8];
    PDIRECTORY_ENTRY    pParent;
    LONG                dwLockCount;

    WCHAR               szFullPath[1];

} DIRECTORY_HEADER, *PDIRECTORY_HEADER;


typedef class CReadOnlyFileSystemDriver* PUDFSDRIVER;

typedef struct tagFHI
{
    DWORD               cbSize;
    LONG                State;
    PUDFSDRIVER         pVol;
    DWORD               dwDiskLocation;
    DWORD               dwByteOffset;
    DWORD               dwDiskSize;
    DWORD               dwFilePointer;
    FILETIME            Time;
    DWORD               fFlags;
    BOOL                fOverlapped;
    struct tagFHI*      pNext;
    struct tagFHI*      pPrevious;

} FILE_HANDLE_INFO, *PFILE_HANDLE_INFO, **PPFILE_HANDLE_INFO;


typedef struct tagFindInfo
{
    PUDFSDRIVER         pVol;
    PDIRECTORY_ENTRY    pDirectoryEntry;
    PDIRECTORY_HEADER   pHeader;
    LONG                State;
    WCHAR               szFileMask[1];

} FIND_HANDLE_INFO, *PFIND_HANDLE_INFO, **PPFIND_HANDLE_INFO;


//
//  We need to make sure that all directory structures are 4 byte aligned
//

#define ALIGNMENT 4

#define ALIGN(x) (((x) + (ALIGNMENT-1)) & ~(ALIGNMENT - 1))

inline PDIRECTORY_ENTRY NextDirectoryEntry(PDIRECTORY_ENTRY pDirEntry)
{
    DEBUGCHK(pDirEntry != NULL);

    return (PDIRECTORY_ENTRY)
             ((DWORD)pDirEntry + pDirEntry->cbSize);

}

inline BOOL IsLastDirectoryEntry(PDIRECTORY_ENTRY pDirEntry)
{
    return (pDirEntry->cbSize == 0);
}

class CReadOnlyFileSystemDriver;

//
// CFileSystem - serach Media for the File System type.
//             - create and init detected File System (CDFS/UDFS/CDDA)
//
//  Notes:  Probably the best solution would to derive CFileSystem Class
//          From CReadOnlyFileSystemDriver. In this case we have only one 
//          for each CD File System. At the moment we have to mentain two
//          objects connected with each other via pointers.
//  
//
//  TBD:    class CFileSystem. public CReadOnlyFileSystemDriver;
//
class CFileSystem
{
protected:
    //
    // Pointer to the corresponding Driver/Volume object.
    //
    PUDFSDRIVER m_pDriver; 
    
public:

    static BYTE DetectCreateAndInit( PUDFSDRIVER pDrv, PDIRECTORY_ENTRY pRootDirectoryPointer, CFileSystem** ppNewFS);
    //
    // This fucntion is defined in the derived class 
    //      - CCDFSFileSystem
    //      - CUDFSFileSystem
    //      - CCDDAFileSystem

    virtual BOOL ReadDirectory( LPWSTR pszFullPath, PDIRECTORY_ENTRY pDirectoryEntry) = 0;
};

class CCDFSFileSystem : public CFileSystem
{
private:
    BOOL    m_fUnicodeFileSystem;
    
public:
    CCDFSFileSystem( PUDFSDRIVER pDrv, BOOL fUnicodeFileSystem)
    {
        m_pDriver = pDrv;
        m_fUnicodeFileSystem = fUnicodeFileSystem;
    }

    static BYTE DetectCreateAndInit( PUDFSDRIVER pDrv, PDIRECTORY_ENTRY pRootDirectoryPointer, CFileSystem** ppNewFS);

    static BOOL DetectCreateAndInitAtLocation( DWORD dwInitialSector, PUDFSDRIVER pDrv, PDIRECTORY_ENTRY pRootDirectoryPointer, CFileSystem** ppNewFS);


    virtual BOOL  ReadDirectory( LPWSTR pszFullPath, PDIRECTORY_ENTRY pDirectoryEntry);

};

class CUDFSFileSystem : public CFileSystem
{
private:
    DWORD       m_dwPartitionStart;
    
public:

    CUDFSFileSystem (PUDFSDRIVER pDrv)
    {
        m_pDriver = pDrv;
    }

    static BYTE DetectCreateAndInit( PUDFSDRIVER pDrv, PDIRECTORY_ENTRY pRootDirectoryPointer, CFileSystem** ppNewFS);

    virtual BOOL ReadDirectory( LPWSTR pszFullPath, PDIRECTORY_ENTRY pDirectoryEntry);

    BOOL CUDFSFileSystem::ResolveFilePosition( DWORD dwDiskLocation, PDIRECTORY_ENTRY pCurrentCacheEntry, PDWORD pdwDiskSize);

};

class CCDDAFileSystem : public CFileSystem
{
private:
    CDROM_TOC m_cdtoc;
public:
    CCDDAFileSystem(PUDFSDRIVER pDrv, CDROM_TOC cdtoc)
    {
        m_pDriver = pDrv;
        m_cdtoc = cdtoc;
    }
    static BYTE DetectCreateAndInit( PUDFSDRIVER pDrv, PDIRECTORY_ENTRY pRootDirectoryPointer, CFileSystem** ppNewFS);

    virtual BOOL  ReadDirectory( LPWSTR pszFullPath, PDIRECTORY_ENTRY pDirectoryEntry);
};



class CReadOnlyFileSystemDriver
{
private:

    //
    // Root/Volume Name 
    //
    TCHAR               m_RootName[33];
    //
    //
    HVOL                m_hVolume;
    HDSK                m_hDsk;


    // Following is for disk removal detection
    HANDLE              m_hTestUnitThread;
    HANDLE              m_hWakeUpEvent;
    BOOL                m_fUnitReady;

    //
    //  Cached list of directory entries
    //

    DIRECTORY_ENTRY     m_RootDirectoryPointer;

    //
    //  Find handle list
    //

    PPFIND_HANDLE_INFO  m_ppFindHandles;

    DWORD               m_cFindHandleListSize;

    //
    //  File handle list
    //

    FILE_HANDLE_INFO*   m_pFileHandleList;

    CRITICAL_SECTION    m_csListAccess;

    LONG                m_State;


    LONG                m_fRegisterLabel;

    //
    //  Pointer to File System Object (CDFS/UDFS/CDDA)
    //
    BYTE                m_bFileSystemType;
    CFileSystem*        m_pFileSystem;

    CReadOnlyFileSystemDriver *m_pNextDriver;

    TCHAR               m_szAFSName[MAX_PATH];

    // To Handle Mount Notifications
    TCHAR               m_szMountName[MAX_PATH];
    GUID                m_MountGuid;
    BOOL                m_bMounted;
    HANDLE              m_hMounting;

public:

    // Handle to our custom heap
    HANDLE              m_hHeap;

    CReadOnlyFileSystemDriver() :
        m_State(StateClean),
        m_pFileHandleList(NULL),
        m_cFindHandleListSize(0),
        m_ppFindHandles(NULL),
        m_fRegisterLabel(FALSE),
        m_pFileSystem(NULL),
        m_pNextDriver(NULL),
        m_hTestUnitThread(NULL),
        m_hWakeUpEvent(NULL),
        m_bFileSystemType(0),
        m_bMounted(FALSE),
        m_hHeap(NULL)
    {
        memset( &m_RootDirectoryPointer, 0, sizeof(DIRECTORY_ENTRY));
        memset( &m_MountGuid, 0, sizeof(GUID));
        wcscpy( m_szMountName, L"");
        wcscpy( m_szAFSName, L"");
        InitializeCriticalSection(&m_csListAccess);
        m_hMounting = CreateMutex(NULL, FALSE, NULL);
    }

    virtual ~CReadOnlyFileSystemDriver()
    {
        if (m_pFileSystem) 
            delete m_pFileSystem;
            
        if (m_hWakeUpEvent)
            CloseHandle(m_hWakeUpEvent);
            
        if (m_hTestUnitThread)
            CloseHandle(m_hTestUnitThread);

        if(g_bDestroyOnUnmount) {
            if(m_hHeap) {
                HeapDestroy( m_hHeap);
            }
        }
        else {
            FILE_HANDLE_INFO *pTempFileHandle;
        
            while (m_pFileHandleList) {
                pTempFileHandle = m_pFileHandleList->pNext;
                UDFSFree(m_hHeap, m_pFileHandleList);
                m_pFileHandleList = pTempFileHandle;
            }    
        
            if (m_ppFindHandles) {
                while( m_cFindHandleListSize) {
                    UDFSFree(m_hHeap, m_ppFindHandles[--m_cFindHandleListSize]);
                }
                UDFSFree(m_hHeap, m_ppFindHandles);
            }
        }
            
        DeleteCriticalSection(&m_csListAccess);

        if(m_hMounting) {
            CloseHandle(m_hMounting);
            m_hMounting = NULL;
        }
    }
    

    BOOL Initialize(HDSK hDsk);

    BOOL RegisterVolume(CReadOnlyFileSystemDriver *pDrv, BOOL bMountLabel = TRUE);

    void DeregisterVolume(CReadOnlyFileSystemDriver *pDrv);
    
    BOOL Mount();

    BOOL Unmount();
    
    void MediaCheckInside(CReadOnlyFileSystemDriver* pDrv);

    void SaveRootName( PCHAR pName,BOOL UniCode );

    static void  MediaChanged(CReadOnlyFileSystemDriver* pDrv, BOOL bUnitReady);

    inline BOOL IsEqualDisk(HDSK hDsk)
    {
        return(m_hDsk == hDsk);
    }

    inline CReadOnlyFileSystemDriver *GetNextDriver()
    {
        return m_pNextDriver;
    }    

public:    

    void Clean();

    void CleanRecurse(PDIRECTORY_ENTRY pDir);

    BOOL IsHandleDirty(PFILE_HANDLE_INFO pHandle);

    BOOL ReadDisk( ULONG Sector, PBYTE pBuffer, DWORD cbBuffer);

    BOOL Read( DWORD dwSector, DWORD dwStartSectorOffset, ULONG nBytesToRead, PBYTE pBuffer, DWORD * pNumBytesRead, LPOVERLAPPED lpOverlapped);

    BOOL UDFSDeviceIoControl(DWORD dwIoControlCode,LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer,
                                  DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped);

  
    BOOL FindFile(LPCWSTR pFileName, PPDIRECTORY_ENTRY   ppDirEntry);

    BOOL FindFileRecurse(LPWSTR  pFileName, LPWSTR  pFullPath, PDIRECTORY_ENTRY pIntialDirectoryEntry, PPDIRECTORY_ENTRY ppFileInfo);

    void CopyDirEntryToWin32FindStruct(PWIN32_FIND_DATAW pfd, PDIRECTORY_ENTRY pDirEntry);


public:    

    HANDLE ROFS_CreateFile(HANDLE hProc,PCWSTR pwsFileName, DWORD dwAccess, DWORD dwShareMode,
                                LPSECURITY_ATTRIBUTES pSecurityAttributes, DWORD dwCreate, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);

    BOOL ROFS_CloseFileHandle( PFILE_HANDLE_INFO pfh);

    HANDLE ROFS_FindFirstFile( HANDLE hProc, PCWSTR pwsFileSpec, PWIN32_FIND_DATAW pfd);

    BOOL ROFS_FindNextFile( PFIND_HANDLE_INFO psfr, PWIN32_FIND_DATAW pfd);

    BOOL ROFS_FindClose( PFIND_HANDLE_INFO psfr);

    DWORD ROFS_GetFileAttributes( PCWSTR pwsFileName);

    BOOL ROFS_ReadFile( PFILE_HANDLE_INFO pfh, LPVOID buffer, DWORD nBytesToRead, DWORD * pNumBytesRead, OVERLAPPED * pOverlapped);

    BOOL ROFS_ReadFileWithSeek( PFILE_HANDLE_INFO pfh, LPVOID buffer, DWORD nBytesToRead, LPDWORD lpNumBytesRead, 
                                       LPOVERLAPPED lpOverlapped, DWORD dwLowOffset, DWORD dwHighOffset);

    DWORD ROFS_SetFilePointer( PFILE_HANDLE_INFO pfh, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod);

    DWORD ROFS_GetFileSize( PFILE_HANDLE_INFO pfh, PDWORD pFileSizeHigh);
    
    BOOL ROFS_GetFileTime( PFILE_HANDLE_INFO pfh, LPFILETIME pCreation, LPFILETIME pLastAccess, LPFILETIME pLastWrite);

    BOOL ROFS_GetFileInformationByHandle( PFILE_HANDLE_INFO pfh, LPBY_HANDLE_FILE_INFORMATION pFileInfo);
    
    BOOL ROFS_RegisterFileSystemNotification(HWND hWnd) { return FALSE; }

    BOOL ROFS_DeviceIoControl(PFILE_HANDLE_INFO pfh, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped);

    BOOL    ROFS_GetDiskFreeSpace( PDWORD pSectorsPerCluster, PDWORD pBytesPerSector, PDWORD pFreeClusters, PDWORD pClusters);
};

typedef struct tagUDFSDRIVERLIST
{
    PUDFSDRIVER pUDFS;
    HDSK        hDsk;
    struct tagUDFSDRIVERLIST* pUDFSNext;
} UDFSDRIVERLIST, *PUDFSDRIVERLIST;    


#ifdef __cplusplus
extern "C" {
#endif

//
//  API Methods
//

BOOL    ROFS_CreateDirectory( PUDFSDRIVER pVol, PCWSTR pwsPathName, LPSECURITY_ATTRIBUTES pSecurityAttributes);
BOOL    ROFS_RemoveDirectory( PUDFSDRIVER pVol, PCWSTR pwsPathName);
BOOL    ROFS_SetFileAttributes( PUDFSDRIVER pVol, PCWSTR pwsFileName, DWORD dwAttributes);
DWORD   ROFS_GetFileAttributes( PUDFSDRIVER pDrv, PCWSTR pwsFileName);
HANDLE  ROFS_CreateFile( 
                PUDFSDRIVER pVol, 
                HANDLE hProc,
                PCWSTR pwsFileName,
                DWORD dwAccess,
                DWORD dwShareMode,
                LPSECURITY_ATTRIBUTES pSecurityAttributes,
                DWORD dwCreate,
                DWORD dwFlagsAndAttributes,
                HANDLE hTemplateFile);
BOOL    ROFS_DeleteFile( PUDFSDRIVER pVol, PCWSTR pwsFileName);
BOOL    ROFS_MoveFile( PUDFSDRIVER pVol, PCWSTR pwsOldFileName, PCWSTR pwsNewFileName);
BOOL    ROFS_RegisterFileSystemNotification( PUDFSDRIVER pVol, HWND hWnd);
HANDLE  ROFS_FindFirstFile( PUDFSDRIVER pvol, HANDLE hProc, PCWSTR pwsFileSpec, PWIN32_FIND_DATAW pfd);
BOOL    ROFS_RegisterFileSystemNotification( PUDFSDRIVER pVol, HWND hWnd);
BOOL    ROFS_RegisterFileSystemFunction( PUDFSDRIVER pVol, SHELLFILECHANGEFUNC_t pShellFunc);
BOOL    ROFS_OidGetInfo( PUDFSDRIVER pvol, CEOID oid, CEOIDINFO *poi);
BOOL    ROFS_DeleteAndRenameFile( PUDFSDRIVER pVol, PCWSTR pwsOldFile, PCWSTR pwsNewFile);
BOOL    ROFS_CloseAllFileHandles( PUDFSDRIVER pVol, HANDLE hProc);
BOOL    ROFS_GetDiskFreeSpace( PUDFSDRIVER pVol, PCWSTR pwsPathName, PDWORD pSectorsPerCluster, PDWORD pBytesPerSector, PDWORD pFreeClusters, PDWORD pClusters);
void    ROFS_Notify( PUDFSDRIVER pVol, DWORD dwFlags);



//
//  File API Methods
//

BOOL ROFS_CloseFileHandle( PFILE_HANDLE_INFO pfh);
BOOL ROFS_ReadFile( PFILE_HANDLE_INFO pfh, LPVOID buffer, DWORD nBytesToRead, DWORD * pNumBytesRead, OVERLAPPED * pOverlapped);
BOOL ROFS_WriteFile( PFILE_HANDLE_INFO pfh, LPCVOID buffer, DWORD nBytesToWrite, LPDWORD pNumBytesWritten, LPOVERLAPPED pOverlapped);
DWORD ROFS_GetFileSize( PFILE_HANDLE_INFO pfh, PDWORD pFileSizeHigh);
DWORD ROFS_SetFilePointer( PFILE_HANDLE_INFO pfh, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod);
BOOL ROFS_GetFileInformationByHandle( PFILE_HANDLE_INFO pfh, LPBY_HANDLE_FILE_INFORMATION pFileInfo);
BOOL ROFS_FlushFileBuffers( PFILE_HANDLE_INFO pfh);
BOOL ROFS_GetFileTime( PFILE_HANDLE_INFO pfh, LPFILETIME pCreation, LPFILETIME pLastAccess, LPFILETIME pLastWrite);
BOOL ROFS_SetFileTime( PFILE_HANDLE_INFO pfh, CONST FILETIME *pCreation, CONST FILETIME *pLastAccess, CONST FILETIME *pLastWrite);
BOOL ROFS_SetEndOfFile( PFILE_HANDLE_INFO pfh);
// DeviceIOControl


BOOL ROFS_ReadFileWithSeek( 
                PFILE_HANDLE_INFO pfh, 
                LPVOID buffer, 
                DWORD nBytesToRead, 
                LPDWORD lpNumBytesRead, 
                LPOVERLAPPED lpOverlapped, 
                DWORD dwLowOffset, 
                DWORD dwHighOffset);

BOOL ROFS_WriteFileWithSeek( 
                PFILE_HANDLE_INFO pfh, 
                LPCVOID buffer, 
                DWORD nBytesToWrite, 
                LPDWORD lpNumBytesWritten, 
                LPOVERLAPPED lpOverlapped, 
                DWORD dwLowOffset, 
                DWORD dwHighOffset);

//
//  Find API Methods
//

BOOL ROFS_FindClose(PFIND_HANDLE_INFO   psfr);
BOOL ROFS_FindNextFile(PFIND_HANDLE_INFO   psfr, PWIN32_FIND_DATAW   pfd);

BOOL ROFS_DeviceIoControl( PFILE_HANDLE_INFO pfh, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, 
                                 DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped);


BOOL ROFS_GetVolumeInfo(PUDFSDRIVER pVol, CE_VOLUME_INFO_LEVEL InfoLevel, FSD_VOLUME_INFO *pInfo);
BOOL FSD_MountDisk(HDSK hDsk);
BOOL FSD_UnmountDisk(HDSK hdsk);

#ifdef __cplusplus
}
#endif


