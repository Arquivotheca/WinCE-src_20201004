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

#pragma once

class CFile;
class CDirectory;
class CUDFPartition;
class CPhysicalPartition;
class CFilePartition;

// /////////////////////////////////////////////////////////////////////////////
// PARTITION_INFO
//
// This contains information from the partition descriptor that describes the
// essentials for a UDF partition.  Note that in most cases there will only be
// one partition.  In fact, that's all that I've ever seen.  However, it is
// possible that there are two partition descriptors.  In this case, one has to
// be read-only, and the other is some form of writable partition.
//
typedef struct
{
    Uint32  Start;      // The starting sector.
    Uint32  Length;     // The length of the partition.
    Uint32  AccessType; // Read-Only, Write-Once, Rewrite, Overwrite
    Uint16  Number;     // The partition number is referenced through the LVD.
    SHORTAD SBD;        // The location of the space bitmap descriptor.

    CPhysicalPartition* pPhysicalPartition;
    CFilePartition*     pVirtualPartition;
} PARTITION_INFO, *PPARTITION_INFO;

// /////////////////////////////////////////////////////////////////////////////
// CVolume
//
class CVolume
{
public:

    CVolume();
    virtual ~CVolume();

    BOOL Initialize( HDSK hDsk );
    BOOL Deregister();

public:

    LRESULT  CreateFile( HANDLE hProc,
                         const WCHAR* strFileName,
                         DWORD Access,
                         DWORD ShareMode,
                         LPSECURITY_ATTRIBUTES pSecurityAttributes,
                         DWORD Create,
                         DWORD FlagsAndAttributes,
                         PHANDLE pHandle );
    LRESULT  CloseFile ( PFILE_HANDLE pHandle, BOOL Flush );
    LRESULT  FsIoControl( DWORD IoControlCode,
                          LPVOID pInBuf,
                          DWORD InBufSize,
                          LPVOID pOutBuf,
                          DWORD OutBufSize,
                          LPDWORD pBytesReturned,
                          LPOVERLAPPED pOverlapped );
    LRESULT  DeviceIoControl( DWORD IoControlCode,
                              LPVOID pInBuf,
                              DWORD InBufSize,
                              LPVOID pOutBuf,
                              DWORD OutBufSize,
                              LPDWORD pBytesReturned,
                              LPOVERLAPPED pOverlapped );
    LRESULT  GetVolumeInfo( FSD_VOLUME_INFO *pInfo );
    LRESULT  FindFirstFile( HANDLE hProc,
                            PCWSTR FileSpec,
                            PWIN32_FIND_DATAW pFindData,
                            PHANDLE pHandle );
    LRESULT  CreateDirectory( PCWSTR PathName,
                              LPSECURITY_ATTRIBUTES pSecurityAttributes );
    LRESULT  RemoveDirectory( PCWSTR PathName );
    LRESULT  GetFileAttributes( PCWSTR strFileName,
                                PDWORD pAttributes );
    LRESULT  SetFileAttributes( PCWSTR FileName,
                                DWORD Attributes );
    LRESULT  DeleteFile( PCWSTR FileName );
    LRESULT  MoveFile( PCWSTR OldFileName,
                       PCWSTR NewFileName );
    LRESULT  DeleteAndRenameFile( PCWSTR OldFile,
                                  PCWSTR NewFile );
    LRESULT  GetDiskFreeSpace( PDWORD pSectorsPerCluster,
                               PDWORD pBytesPerSector,
                               PDWORD pFreeClusters,
                               PDWORD pClusters );

    Uint16 GetPhysicalReference( CPhysicalPartition* pPart ) const;
    BOOL ValidateUDFVolume(HDSK hDisk);
    BOOL ValidateAndInitMedia();

public:
    BOOL UDFSDeviceIoControl( DWORD dwIoControlCode,
                              LPVOID lpInBuffer,
                              DWORD nInBufferSize,
                              LPVOID lpOutBuffer,
                              DWORD nOutBufferSize,
                              LPDWORD lpBytesReturned,
                              LPOVERLAPPED lpOverlapped );

    Uint32 GetPartitionStart( Uint16 Reference ) const;

public:
    inline HANDLE GetHeap()
    {
        return m_hHeap;
    }

    inline DWORD GetSectorSize() const
    {
        return m_dwSectorSize;
    }

    inline DWORD GetShiftCount() const
    {
        return m_dwShiftCount;
    }

    inline DWORD GetMediaType() const
    {
        return m_dwMediaType;
    }

    inline CUDFMedia* GetMedia() const
    {
        return m_pMedia;
    }

    inline CUDFPartition* GetPartition() const
    {
        return m_pPartition;
    }

    inline Uint32 GetAllocExtentsPerChunk() const
    {
        return m_dwAllocSegmentsPerChunk;
    }

    inline void Lock()
    {
        EnterCriticalSection( &m_cs );
    }

    inline void Unlock()
    {
        LeaveCriticalSection( &m_cs );
    }

    inline CFileManager* GetFileManager()
    {
        return &m_FileManager;
    }

    inline CHandleManager* GetHandleManager()
    {
        return &m_HandleManager;
    }

    inline DWORD GetLastSector() const
    {
        return m_dwLastSector;
    }

    inline DWORD GetFirstSector() const
    {
        return m_dwFirstSector;
    }

    inline HDSK GetDisk() const
    {
        return m_hDsk;
    }

protected:
    //
    // These methods are used during initialization of a volume to
    // ensure that the volume is valid to mount.
    //
    BOOL ValidateUDFVolume();

    BOOL FeatureIsActive( FEATURE_NUMBER Feature );
    BOOL DetermineMediaType( DWORD* pdwMediaType );

    BOOL GetSessionBounds( DWORD* pdwFirstSector, DWORD* pdwLastSector );
    BOOL GetClosedMediaBounds( __out DWORD* pdwFirstSector,
                               __out DWORD* pdwLastSector );
    BOOL GetOpenMediaBounds( __out BOOL* pfFirstTrackIsEmpty,
                             const DISC_INFORMATION& DiscInfo,
                             __out DWORD* pdwFirstSector,
                             __out DWORD* pdwLastSector );
    BOOL ReadVRS( DWORD dwStartingSector, USHORT* pNSRVersion );
    BOOL ValidateVDS();
    BOOL ReadVDS( BYTE** ppLogicalVolume,
                  LONGAD* pFSDAddr,
                  PPARTITION_INFO pPartitionArray );
    BOOL ReadLVIDSequence( const EXTENTAD& Start );
    BOOL ReadPartitionMaps( PNSR_LVOL pLVol );
    BOOL AddPhysicalPartition( CPhysicalPartition* pPartition, Uint16 Number );
    BOOL AddVirtualPartition( CFilePartition* pPartition,
                              Uint16 Number,
                              Uint16 Reference );
    BOOL GetVDSLocations( PEXTENTAD pdwMainVDS, PEXTENTAD pdwReserveVDS );
    BOOL ReadFSD( LONGAD* pRootAddr );

    CFileLink* ParsePath( CDirectory* pDir,
                          CFileName* pFileName,
                          BOOL fReturnDir = FALSE );

    BOOL RemountMedia();


    CRITICAL_SECTION m_cs;

private:
    HDSK m_hDsk;
    HVOL m_hVol;
    WCHAR m_strDiskName[MAX_PATH];
    WCHAR m_strVolumeName[MAX_PATH];
    BOOL m_fUnAdvertise;
    HANDLE m_hHeap;

    DWORD m_dwSectorSize;
    DWORD m_dwShiftCount;
    DWORD m_dwMediaType;
    DWORD m_dwFirstSector;
    DWORD m_dwLastSector;

    DWORD m_dwFlags;

    PARTITION_INFO m_PartitionInfo[2];
    CUDFPartition* m_pPartition;
    LONGAD m_FsdStart;
    LONGAD m_RootDir;

    USHORT m_NSRVer;

    CUDFMedia* m_pMedia;

    CDirectory* m_pRoot;

    CFileManager m_FileManager;
    CHandleManager m_HandleManager;

    //
    // These are values picked up from the registry for tuning of the
    // file system.
    //
private:

    //
    // This value determines how many allocation segments are allocated in each
    // chunk of data.  An allocation segment describes a mapping from a file
    // offset to a location on disk.  If a file is badly fragmented, there will
    // be far more allocation descriptors needed per file and this value should
    // be increased for better performance.  For mastered media, like you will
    // find mostly in a consumer device, fewer allocation descriptors per chunk
    // will reduce memory requirements without reducing peformance as the disk
    // will not be fragmented.
    //
    DWORD m_dwAllocSegmentsPerChunk;

};

#define VOL_FLAG_READ_0NLY 0x00000001

typedef CVolume* PUDF_DRIVER;

typedef struct _UDF_DRIVER_LIST
{
    PUDF_DRIVER pVolume;
    HDSK        hDsk;
    struct _UDF_DRIVER_LIST* pUDFSNext;
} UDF_DRIVER_LIST, *PUDF_DRIVER_LIST;

