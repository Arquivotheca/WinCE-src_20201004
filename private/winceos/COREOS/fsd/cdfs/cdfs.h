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
#include <cdfsdbg.h>
#include <cdfshelper.h>
#include <dbt.h>
#include <storemgr.h>
#include <fsioctl.h>
#include <atapi2.h>

#include <iso9660.h>

#define FS_MOUNT_POINT_NAME L"CD-ROM"
#define FS_MOUNT_POINT_NAME_LENGTH 6

#define MAX_SECTORS_TO_PROBE    10

extern const __declspec(selectany) BYTE DIR_HEADER_SIGNATURE[8] = {
    0x08,0x11,0x71,0x40,0xB4,0x43,0x52,0x10
};

#define CD_SECTOR_SIZE 2048
#define CD_SHIFT_COUNT 11

#define MAX_CD_READ_SIZE 65536

#define StateClean       0
#define StateDirty       1
#define StateCleaning    2

#define FIND_HANDLE_LIST_START_SIZE 5

#define IS_ANSI_FILENAME 8000

#define FILE_SYSTEM_TYPE_CDFS 1
#define FILE_SYSTEM_TYPE_CDDA 2

extern CRITICAL_SECTION g_csMain;
extern const SIZE_T g_uHeapInitSize;
extern SIZE_T g_uHeapMaxSize;
extern BOOL g_bDestroyOnUnmount;

//
//  This macro copies an src longword to a dst longword,
//  performing an little/big endian swap.
//
#define SwapCopyUchar4(Dst,Src) {                                        \
    *((UCHAR *)(Dst)) = *((UCHAR *)(Src) + 3);     \
    *((UCHAR *)(Dst) + 1) = *((UCHAR *)(Src) + 2); \
    *((UCHAR *)(Dst) + 2) = *((UCHAR *)(Src) + 1); \
    *((UCHAR *)(Dst) + 3) = *((UCHAR *)(Src));     \
}

#define FlagOn(Flags,SingleFlag)        ((Flags) & (SingleFlag))

#define MF_CHECK_TRACK_MODE     0x0001
#define MF_PREPEND_HEADER       0x0002

typedef struct tagDirectoryEntry
{
    WORD                cbSize; // Length of the whole structure
    WORD                fFlags;
    DWORD               dwDiskLocation;
    DWORD               dwDiskSize; //  Per spec no file > 1 gig
    DWORD               dwByteOffset;
    FILETIME            Time;
    
    WORD                MediaFlags;   // Used for audio/video CDs
    WORD                HeaderSize;   // Used for audio/video CDs
    TRACK_MODE_TYPE     TrackMode;    // Used for audio/video CDs
    WORD                XAAttributes; // Used for video CDs
    BYTE                XAFileNumber; // Used for video CDs
    BYTE                Reserved;     // Allignment

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

//
//  These two sturctures taken from the desktop.  Code is part of DDK and ok.
//
//  RIFF header.  Prepended to the data of a file containing XA sectors.
//  This is a hard-coded structure except that we bias the 'ChunkSize' and
//  'RawSectors' fields with the file size.  We also copy the attributes flag
//  from the system use area in the dirent.  We always initialize this
//  structure by copying the XAFileHeader.
//
typedef struct _RIFF_HEADER {

    ULONG ChunkId;
    LONG ChunkSize;
    ULONG SignatureCDXA;
    ULONG SignatureFMT;
    ULONG XAChunkSize;
    ULONG OwnerId;
    USHORT Attributes;
    USHORT SignatureXA;
    UCHAR FileNumber;
    UCHAR Reserved[7];
    ULONG SignatureData;
    ULONG RawSectors;

} RIFF_HEADER;
typedef RIFF_HEADER *PRIFF_HEADER;

//
//  Audio play header for CDDA tracks.
//
typedef struct _AUDIO_PLAY_HEADER {

    ULONG Chunk;
    ULONG ChunkSize;
    ULONG SignatureCDDA;
    ULONG SignatureFMT;
    ULONG FMTChunkSize;
    USHORT FormatTag;
    USHORT TrackNumber;
    ULONG DiskID;
    ULONG StartingSector;
    ULONG SectorCount;
    UCHAR TrackAddress[4];
    UCHAR TrackLength[4];

} AUDIO_PLAY_HEADER;
typedef AUDIO_PLAY_HEADER *PAUDIO_PLAY_HEADER;

//
//  Hardcoded header for RIFF files.
//

extern LONG CdXAFileHeader[];
extern LONG CdXAAudioPhileHeader[];

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

    WORD                MediaFlags;   // Used for audio/video CDs
    WORD                HeaderSize;   // Used for audio/video CDs
    TRACK_MODE_TYPE     TrackMode;    // Used for audio/video CDs
    WORD                XAAttributes; // Used for video CDs
    BYTE                XAFileNumber; // Used for video CDs

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
//             - create and init detected File System (CDFS/CDDA)
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
public:
    PCD_PARTITION_INFO m_pPartInfo;
    
protected:
    //
    // Pointer to the corresponding Driver/Volume object.
    //
    PUDFSDRIVER m_pDriver; 
    
public:
    CFileSystem()
    {
        m_pDriver = NULL;
        m_pPartInfo = NULL;
    }
    
    virtual ~CFileSystem()
    {
        if( m_pPartInfo )
        {
            delete [] m_pPartInfo;
        }
    }

    static BYTE DetectCreateAndInit( PUDFSDRIVER pDrv, PDIRECTORY_ENTRY pRootDirectoryPointer, CFileSystem** ppNewFS);
    //
    // This fucntion is defined in the derived class 
    //      - CCDFSFileSystem
    //      - CCDDAFileSystem

    virtual BOOL ReadDirectory( const WCHAR* pszFullPath, PDIRECTORY_ENTRY pDirectoryEntry) = 0;

    virtual BOOL GetHeader( PRIFF_HEADER pHeader, PFILE_HANDLE_INFO pFileInfo ) = 0;

    virtual const WCHAR* GetName() const = 0;
};

class CCDFSFileSystem : public CFileSystem
{
public:
    BOOL    m_fUnicodeFileSystem;
    BOOL    m_fCDXA;
    
public:
    CCDFSFileSystem( PUDFSDRIVER pDrv, BOOL fUnicodeFileSystem, BOOL fCDXA, PCD_PARTITION_INFO pPartInfo )
    {
        m_fCDXA = fCDXA;
        m_pPartInfo = pPartInfo;
        m_pDriver = pDrv;
        m_fUnicodeFileSystem = fUnicodeFileSystem;
    }

    static BOOL Recognize( HDSK hDisk,
                           PDIRECTORY_ENTRY pRootDirectoryPointer, 
                           BOOL* fUnicode,
                           BOOL* fCDXA,
                           __out_ecount_opt(LabelSize) BYTE* VolumeLabel,
                           DWORD LabelSize );
    static BOOL DetectCreateAndInit( PUDFSDRIVER pDrv, PDIRECTORY_ENTRY pRootDirectoryPointer, CFileSystem** ppNewFS, PCD_PARTITION_INFO pPartInfo );

    virtual BOOL  ReadDirectory( const WCHAR* pszFullPath, PDIRECTORY_ENTRY pDirectoryEntry);

    BOOL GetHeader( PRIFF_HEADER pHeader, PFILE_HANDLE_INFO pFileInfo );

    const WCHAR* GetName() const { return L"CDFS"; }
};

class CCDDAFileSystem : public CFileSystem
{
public:
    CCDDAFileSystem( PUDFSDRIVER pDrv, PCD_PARTITION_INFO pPartInfo )
    {
        m_pPartInfo = pPartInfo;
        m_pDriver = pDrv;
    }

    static BOOL DetectCreateAndInit( PUDFSDRIVER pDrv, PDIRECTORY_ENTRY pRootDirectoryPointer, CFileSystem** ppNewFS, PCD_PARTITION_INFO pPartInfo );

    virtual BOOL  ReadDirectory( const WCHAR* pszFullPath, PDIRECTORY_ENTRY pDirectoryEntry);

    BOOL GetHeader( PRIFF_HEADER pHeader, PFILE_HANDLE_INFO pFileInfo );

    const WCHAR* GetName() const { return L"CDDA"; }
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
    //  Pointer to File System Object (CDFS/CDDA)
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
        m_bFileSystemType(0),
        m_bMounted(FALSE),
        m_hHeap(NULL)
    {
        ZeroMemory( &m_RootDirectoryPointer, sizeof(DIRECTORY_ENTRY) );
        ZeroMemory( &m_MountGuid, sizeof(GUID) );
        ZeroMemory( m_szMountName, sizeof(m_szMountName) );
        ZeroMemory( m_szAFSName, sizeof(m_szAFSName) );
        
        InitializeCriticalSection(&m_csListAccess);
        m_hMounting = CreateMutex(NULL, FALSE, NULL);
    }

    virtual ~CReadOnlyFileSystemDriver()
    {
        if (m_pFileSystem) 
            delete m_pFileSystem;
            
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
        }
    }
    

    BOOL Initialize(HDSK hDsk);

    BOOL RegisterVolume(CReadOnlyFileSystemDriver *pDrv, BOOL bMountLabel = TRUE);

    BOOL Mount();

    BOOL Unmount();
    
    void SaveRootName( const char* pName, BOOL UniCode );

    inline BOOL IsEqualDisk(HDSK hDsk)
    {
        return(m_hDsk == hDsk);
    }

    inline CReadOnlyFileSystemDriver *GetNextDriver()
    {
        return m_pNextDriver;
    }

    inline HDSK CReadOnlyFileSystemDriver::GetDisk() const
    {
        return m_hDsk;
    }

    inline const WCHAR* CReadOnlyFileSystemDriver::GetFSName() const
    {
        return m_pFileSystem->GetName();
    }

public:    

    void Clean();

    void CleanRecurse(PDIRECTORY_ENTRY pDir);

    BOOL IsHandleDirty(PFILE_HANDLE_INFO pHandle);

    static BOOL ReadDisk( HDSK hDisk, ULONG Sector, PBYTE pBuffer, DWORD cbBuffer);

    BOOL Read( DWORD dwSector, 
               DWORD dwStartSectorOffset, 
               ULONG nBytesToRead, 
               PBYTE pBuffer, 
               DWORD * pNumBytesRead, 
               LPOVERLAPPED lpOverlapped,
               BOOL fRaw = FALSE,
               TRACK_MODE_TYPE TrackMode = YellowMode2 );

    BOOL UDFSDeviceIoControl(DWORD dwIoControlCode,LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer,
                                  DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped);

  
    BOOL FindFile(LPCWSTR pFileName, PPDIRECTORY_ENTRY   ppDirEntry);

    BOOL FindFileRecurse( __inout PWSTR pFileName, 
                          __in PWSTR pFullPath, 
                          PDIRECTORY_ENTRY pIntialDirectoryEntry, 
                          PPDIRECTORY_ENTRY ppFileInfo );

    void CopyDirEntryToWin32FindStruct(PWIN32_FIND_DATAW pfd, PDIRECTORY_ENTRY pDirEntry);


public:    

    HANDLE ROFS_CreateFile(HANDLE hProc,PCWSTR pwsFileName, DWORD dwAccess, DWORD dwShareMode,
                                LPSECURITY_ATTRIBUTES pSecurityAttributes, DWORD dwCreate, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);

    BOOL ROFS_CloseFileHandle( PFILE_HANDLE_INFO pfh);

    HANDLE ROFS_FindFirstFile( HANDLE hProc, PCWSTR pwsFileSpec, PWIN32_FIND_DATAW pfd);

    BOOL ROFS_FindNextFile( PFIND_HANDLE_INFO psfr, PWIN32_FIND_DATAW pfd);

    BOOL ROFS_FindClose( PFIND_HANDLE_INFO psfr);

    DWORD ROFS_GetFileAttributes( PCWSTR pwsFileName);

    BOOL ROFS_ReadFile( PFILE_HANDLE_INFO pfh,
                        __in_ecount(nBytesToRead) LPVOID buffer,
                        DWORD nBytesToRead,
                        DWORD * pNumBytesRead,
                        OVERLAPPED * pOverlapped );

    BOOL ROFS_ReadFileWithSeek( PFILE_HANDLE_INFO pfh, LPVOID buffer, DWORD nBytesToRead, LPDWORD lpNumBytesRead, 
                                       LPOVERLAPPED lpOverlapped, DWORD dwLowOffset, DWORD dwHighOffset);

    DWORD ROFS_SetFilePointer( PFILE_HANDLE_INFO pfh, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod);

    DWORD ROFS_GetFileSize( PFILE_HANDLE_INFO pfh, PDWORD pFileSizeHigh);
    
    BOOL ROFS_GetFileTime( PFILE_HANDLE_INFO pfh, LPFILETIME pCreation, LPFILETIME pLastAccess, LPFILETIME pLastWrite);

    BOOL ROFS_GetFileInformationByHandle( PFILE_HANDLE_INFO pfh, LPBY_HANDLE_FILE_INFORMATION pFileInfo);
    
    BOOL ROFS_RegisterFileSystemNotification(HWND hWnd) { return FALSE; }

    BOOL ROFS_DeviceIoControl(PFILE_HANDLE_INFO pfh, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped);

    BOOL ROFS_GetDiskFreeSpace( PDWORD pSectorsPerCluster, PDWORD pBytesPerSector, PDWORD pFreeClusters, PDWORD pClusters);

private:
    BOOL RemountMedia();
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


BOOL ROFS_GetVolumeInfo(PUDFSDRIVER pVol, FSD_VOLUME_INFO *pInfo);
BOOL FSD_MountDisk(HDSK hDsk);
BOOL FSD_UnmountDisk(HDSK hdsk);
BOOL ROFS_RecognizeVolume( HANDLE hDisk, 
                           const GUID* pGuid, 
                           const BYTE* pBootSector, 
                           DWORD dwSectorSize );

#ifdef __cplusplus
}
#endif


